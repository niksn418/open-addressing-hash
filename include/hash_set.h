#pragma once

#include "policy.h"

#include <algorithm>
#include <tuple>
#include <variant>
#include <vector>

namespace hashset_details {
template <class T>
constexpr bool IsConst = false;
template <template <bool> class T>
constexpr bool IsConst<T<true>> = true;
} // namespace hashset_details

template <class Key,
          class CollisionPolicy = LinearProbing,
          class Hash = std::hash<Key>,
          class Equal = std::equal_to<Key>,
          class RangeHash = MaskRangeHashing,
          class RehashPolicy = Power2RehashPolicy>
class HashSet
{
public:
    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = Equal;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;

private:
    struct Empty
    {
    };

    struct Erased
    {
    };

    struct Node
    {
        value_type value;
        size_type next = m_end;
        size_type prev = m_end;

        template <class... Args>
        Node(Args &&... args)
            : value(std::forward<Args>(args)...)
        {
        }

        Node(const value_type && value)
            : value(const_cast<value_type &&>(std::move(value)))
        {
        }
    };

    struct Element
    {
        constexpr Element() noexcept
            : value(std::in_place_type<Empty>)
        {
        }

        constexpr bool is_used() const noexcept
        {
            return holds<Node>();
        }

        constexpr bool is_empty() const noexcept
        {
            return holds<Empty>();
        }

        constexpr void erase() noexcept
        {
            value = Erased();
        }

        constexpr void clear() noexcept
        {
            value = Empty();
        }

        template <class... Args>
        void set(Args &&... args)
        {
            value.template emplace<Node>(std::forward<Args>(args)...);
        }

        constexpr Node & get() noexcept
        {
            return std::get<Node>(value);
        }

        constexpr const Node & get() const noexcept
        {
            return std::get<Node>(value);
        }

    private:
        std::variant<Empty, Erased, Node> value;

        template <class T>
        constexpr bool holds() const noexcept
        {
            return std::holds_alternative<T>(value);
        }
    };

    template <bool is_const>
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::conditional_t<is_const, const HashSet::value_type, HashSet::value_type>;
        using difference_type = HashSet::difference_type;
        using pointer = const value_type *;
        using reference = const value_type &;

        Iterator() = default;

        template <class T = Iterator, std::enable_if_t<hashset_details::IsConst<T>, int> = 0>
        Iterator(const Iterator<false> & other)
            : m_pos(other.m_pos)
            , m_container(other.m_container)
        {
        }

        reference operator*() const
        {
            return get_node().value;
        }

        pointer operator->() const
        {
            return &get_node().value;
        }

        Iterator & operator++()
        {
            m_pos = get_node().next;
            return *this;
        }

        Iterator operator++(int)
        {
            auto tmp = *this;
            operator++();
            return tmp;
        }

        friend bool operator==(const Iterator & lhs, const Iterator & rhs)
        {
            return lhs.m_pos == rhs.m_pos;
        }

        friend bool operator!=(const Iterator & lhs, const Iterator & rhs)
        {
            return !(lhs == rhs);
        }

    private:
        friend class HashSet;

        using container_type = std::conditional_t<is_const, const HashSet, HashSet>;

        size_type m_pos;
        container_type * m_container;
        // we can store m_data.data(), so get_node will look like (m_data.data() + m_pos)->get() (less memory lookups)

        constexpr Iterator(const size_type node_index, container_type * container) noexcept
            : m_pos(node_index)
            , m_container(container)
        {
        }

        constexpr auto & get_node() const noexcept
        {
            return m_container->m_data[m_pos].get();
        }
    };

    const hasher & m_hash;
    const key_equal & m_key_equal;

    std::vector<Element> m_data;

    size_type m_size;

    size_type m_begin;
    static constexpr size_type m_end = std::numeric_limits<size_type>::max();

public:
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    explicit HashSet(size_type expected_max_size = 0,
                     const hasher & hash = hasher(),
                     const key_equal & equal = key_equal())
        : m_hash(hash)
        , m_key_equal(equal)
        , m_data(RehashPolicy::new_size(RehashPolicy::buckets_number(expected_max_size)))
    {
        reset();
    }

    template <class InputIt>
    HashSet(InputIt first, InputIt last, size_type expected_max_size = 0, const hasher & hash = hasher(), const key_equal & equal = key_equal())
        : HashSet(expected_max_size, hash, equal)
    {
        insert(first, last);
    }

    HashSet(const HashSet & other) = default;

    HashSet(HashSet && other) = default;

    HashSet(std::initializer_list<value_type> init,
            size_type expected_max_size = 0,
            const hasher & hash = hasher(),
            const key_equal & equal = key_equal())
        : HashSet(init.begin(), init.end(), std::max(expected_max_size, init.size()), hash, equal)
    {
    }

    HashSet & operator=(const HashSet & other)
    {
        std::tie(m_data, m_size, m_begin) = std::tie(other.m_data, other.m_size, other.m_begin);
        return *this;
    }

    HashSet & operator=(HashSet && other) noexcept
    {
        m_data = std::move(other.m_data);
        m_size = std::move(other.m_size);
        m_begin = std::move(other.m_begin);
        return *this;
    }

    HashSet & operator=(std::initializer_list<value_type> init)
    {
        clear();
        insert(init);
        return *this;
    }

    iterator begin() noexcept
    {
        return create_iterator(m_begin);
    }

    const_iterator begin() const noexcept
    {
        return cbegin();
    }

    const_iterator cbegin() const noexcept
    {
        return create_const_iterator(m_begin);
    }

    iterator end() noexcept
    {
        return create_iterator(m_end);
    }

    const_iterator end() const noexcept
    {
        return cend();
    }

    const_iterator cend() const noexcept
    {
        return create_const_iterator(m_end);
    }

    bool empty() const
    {
        return size() == 0;
    }

    size_type size() const
    {
        return m_size;
    }

    size_type max_size() const
    {
        return m_data.max_size();
    }

    void clear()
    {
        for (auto it = begin(), stop = end(); it != stop;) {
            const size_type cur = it.m_pos;
            ++it;
            m_data[cur].clear();
        }
        reset();
    }

    std::pair<iterator, bool> insert(const value_type & value)
    {
        return generic_insert(value);
    }

    std::pair<iterator, bool> insert(value_type && value)
    {
        return generic_insert(std::move(value));
    }

    iterator insert(const_iterator hint, const value_type & value)
    {
        return generic_insert(hint, value);
    }

    iterator insert(const_iterator hint, value_type && value)
    {
        return generic_insert(hint, std::move(value));
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last)
    {
        for (auto it = first; it != last; ++it) {
            insert(*it);
        }
    }

    void insert(std::initializer_list<value_type> init)
    {
        if (RehashPolicy::need_rehash(size() + init.size(), m_data.size())) {
            reserve(size() + init.size());
        }
        insert(init.begin(), init.end());
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args &&... args)
    {
        return generic_insert(key_type{std::forward<Args>(args)...});
    }

    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args &&... args)
    {
        return generic_insert(hint, key_type{std::forward<Args>(args)...});
    }

    iterator erase(const_iterator pos)
    {
        const size_type next = m_data[pos.m_pos].get().next;
        // const size_type next = pos.get_node().next;
        remove_node(pos.m_pos);
        return create_iterator(next);
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        link_nodes(m_data[first.m_pos].get().prev, last.m_pos);
        for (auto it = first; it != last;) {
            const size_type cur = it.m_pos;
            ++it;
            m_data[cur].erase();
            --m_size;
        }
        return create_iterator(last.m_pos);
    }

    size_type erase(const key_type & key)
    {
        if (const size_type pos = search(key); m_data[pos].is_used()) {
            remove_node(pos);
            return 1;
        }
        return 0;
    }

    void swap(HashSet & other) noexcept
    {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_begin, other.m_begin);
    }

    size_type count(const key_type & key) const
    {
        return contains(key);
    }

    iterator find(const key_type & key)
    {
        const size_type pos = search(key);
        return create_iterator(m_data[pos].is_used() ? pos : m_end);
    }

    const_iterator find(const key_type & key) const
    {
        const size_type pos = search(key);
        return create_const_iterator(m_data[pos].is_used() ? pos : m_end);
    }

    bool contains(const key_type & key) const
    {
        return m_data[search(key)].is_used();
    }

    std::pair<iterator, iterator> equal_range(const key_type & key)
    {
        const iterator first = find(key);
        return {first, first != end() ? std::next(first) : first};
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type & key) const
    {
        const const_iterator first = find(key);
        return {first, first != cend() ? std::next(first) : first};
    }

    size_type bucket_count() const
    {
        return m_data.size();
    }

    size_type max_bucket_count() const
    {
        return max_size();
    }

    size_type bucket_size(const size_type pos) const
    {
        return m_data[pos].is_used();
    }

    size_type bucket(const key_type & key) const
    {
        return find_insertion_pos(key);
    }

    float load_factor() const
    {
        return 1.0f * size() / bucket_count();
    }

    float max_load_factor() const
    {
        return RehashPolicy::max_load_factor();
    }

    void rehash(const size_type count)
    {
        HashSet old{std::move(*this)};
        m_data = std::vector<Element>(RehashPolicy::new_size(count, old.m_data.size()));
        reset();
        for (auto & value : old) {
            const size_type start = index(value);
            size_type pos = start;
            for (size_type step = 0; m_data[pos].is_used(); pos = CollisionPolicy::next(start, ++step, m_data.size())) {
            }
            insert_at(pos, std::move(value));
        }
    }

    void reserve(size_type count)
    {
        rehash(RehashPolicy::buckets_number(count));
    }

    friend bool operator==(const HashSet & lhs, const HashSet & rhs)
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (const auto & value : lhs) {
            if (!rhs.contains(value)) {
                return false;
            }
        }
        return true;
    }

    friend bool operator!=(const HashSet & lhs, const HashSet & rhs)
    {
        return !(lhs == rhs);
    }

private:
    constexpr size_type index(const key_type & key) const noexcept
    {
        return RangeHash::hash(m_hash(key), m_data.size());
    }

    constexpr iterator create_iterator(const size_type pos) noexcept
    {
        return {pos, this};
    }

    constexpr const_iterator create_const_iterator(const size_type pos) const noexcept
    {
        return {pos, this};
    }

    constexpr void remove_node(const size_type pos) noexcept
    {
        link_nodes(m_data[pos].get().prev, m_data[pos].get().next);
        m_data[pos].erase();
        --m_size;
    }

    constexpr void link_nodes(const size_type left, const size_type right) noexcept
    {
        if (left != m_end) {
            m_data[left].get().next = right;
        }
        else {
            m_begin = right;
        }
        if (right != m_end) {
            m_data[right].get().prev = left;
        }
    }

    constexpr size_type find_pos(const key_type & key, bool seek_erased) const noexcept
    {
        const size_type start = index(key);
        size_type first_erased = m_data.size();
        for (size_type step = 0, i = start;; i = CollisionPolicy::next(start, ++step, m_data.size())) {
            if (m_data[i].is_empty()) {
                return first_erased == m_data.size() ? i : first_erased;
            }
            else if (m_data[i].is_used()) {
                if (m_key_equal(m_data[i].get().value, key)) {
                    return i;
                }
            }
            else if (seek_erased && first_erased == m_data.size()) {
                first_erased = i;
            }
        }
    }

    constexpr size_type search(const key_type & key) const noexcept
    {
        return find_pos(key, false);
    }

    constexpr size_type find_insertion_pos(const key_type & key) const noexcept
    {
        return find_pos(key, true);
    }

    constexpr void reset() noexcept
    {
        m_begin = m_end;
        m_size = 0;
    }

    template <class T>
    std::pair<iterator, bool> generic_insert(T && value)
    {
        return insert_impl(std::forward<T>(value));
    }

    template <class T>
    iterator generic_insert(const_iterator hint, T && value)
    {
        if (hint != cend() && m_key_equal(*hint, value)) {
            return create_iterator(hint.m_pos);
        }
        return insert_impl(std::forward<T>(value)).first;
    }

    template <class T>
    std::pair<iterator, bool> insert_impl(T && value)
    {
        if (RehashPolicy::need_rehash(size() + 1, m_data.size())) {
            reserve(size() + 1);
        }
        const size_type pos = find_insertion_pos(value);
        const bool used = m_data[pos].is_used();
        if (!used) {
            insert_at(pos, std::forward<T>(value));
        }
        return {create_iterator(pos), !used};
    }

    template <class T>
    void insert_at(const size_type pos, T && value)
    {
        m_data[pos].set(std::forward<T>(value));
        m_data[pos].get().next = m_begin;
        if (m_begin != m_end) {
            m_data[m_begin].get().prev = pos;
        }
        m_begin = pos;
        ++m_size;
    }
};