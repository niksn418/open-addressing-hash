#pragma once

#include "policy.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <variant>
#include <vector>

namespace hashmap_details {
template <class T>
constexpr bool IsConst = false;
template <template <bool> class T>
constexpr bool IsConst<T<true>> = true;
} // namespace hashmap_details

template <class Key,
          class Value,
          class CollisionPolicy = LinearProbing,
          class Hash = std::hash<Key>,
          class Equal = std::equal_to<Key>,
          class RangeHash = MaskRangeHashing,
          class RehashPolicy = Power2RehashPolicy>
class HashMap : private Hash
    , private Equal
{
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
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

        Node(value_type && value)
            : Node(const_cast<key_type &&>(std::move(value.first)), std::move(value.second))
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
        using value_type = std::conditional_t<is_const, const HashMap::value_type, HashMap::value_type>;
        using difference_type = HashMap::difference_type;
        using pointer = value_type *;
        using reference = value_type &;

        Iterator() = default;

        template <class T = Iterator, std::enable_if_t<hashmap_details::IsConst<T>, int> = 0>
        Iterator(const Iterator<false> & other)
            : m_pos(other.m_pos)
            , m_data(other.m_data)
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
            return lhs.m_pos == rhs.m_pos && lhs.m_data == rhs.m_data;
        }

        friend bool operator!=(const Iterator & lhs, const Iterator & rhs)
        {
            return !(lhs == rhs);
        }

    private:
        friend class HashMap;

        using element_ptr_type = std::conditional_t<is_const, const HashMap::Element *, HashMap::Element *>;

        size_type m_pos;
        element_ptr_type m_data;

        constexpr Iterator(const size_type node_index, const element_ptr_type data) noexcept
            : m_pos(node_index)
            , m_data(data)
        {
        }

        constexpr auto & get_node() const noexcept
        {
            return m_data[m_pos].get();
        }
    };

    std::vector<Element> m_data;

    size_type m_size;

    size_type m_begin;
    static constexpr size_type m_end = std::numeric_limits<size_type>::max();

public:
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    explicit HashMap(size_type expected_max_size = 0,
                     const hasher & hash = hasher(),
                     const key_equal & equal = key_equal())
        : hasher(hash)
        , key_equal(equal)
        , m_data(RehashPolicy::new_size(RehashPolicy::buckets_number(expected_max_size)))
    {
        reset();
    }

    template <class InputIt>
    HashMap(InputIt first,
            InputIt last,
            size_type expected_max_size = 0,
            const hasher & hash = hasher(),
            const key_equal & equal = key_equal())
        : HashMap(expected_max_size, hash, equal)
    {
        insert(first, last);
    }

    HashMap(const HashMap & other)
        : hasher(other)
        , key_equal(other)
        , m_data(other.m_data)
        , m_size(other.m_size)
        , m_begin(other.m_begin)
    {
    }

    HashMap(HashMap && other) = default;

    HashMap(std::initializer_list<value_type> init,
            size_type expected_max_size = 0,
            const hasher & hash = hasher(),
            const key_equal & equal = key_equal())
        : HashMap(init.begin(), init.end(), std::max(expected_max_size, init.size()), hash, equal)
    {
    }

    HashMap & operator=(const HashMap & other)
    {
        return *this = HashMap{other};
    }

    HashMap & operator=(HashMap && other) noexcept = default;

    HashMap & operator=(std::initializer_list<value_type> init)
    {
        return *this = HashMap{init};
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
        return generic_try_emplace(value.first, value.second);
    }

    std::pair<iterator, bool> insert(value_type && value)
    {
        return generic_try_emplace(std::move(value.first), std::move(value.second));
    }

    template <class P>
    std::pair<iterator, bool> insert(P && value)
    {
        return emplace(std::forward<P>(value));
    }

    iterator insert(const_iterator hint, const value_type & value)
    {
        return generic_try_emplace(hint, value.first, value.second);
    }

    iterator insert(const_iterator hint, value_type && value)
    {
        return generic_try_emplace(hint, std::move(value.first), std::move(value.second));
    }

    template <class P>
    iterator insert(const_iterator hint, P && value)
    {
        return emplace_hint(hint, std::forward<P>(value));
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

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const key_type & key, M && value)
    {
        return generic_insert_or_assign(key, std::forward<M>(value));
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(key_type && key, M && value)
    {
        return generic_insert_or_assign(std::move(key), std::forward<M>(value));
    }

    template <class M>
    iterator insert_or_assign(const_iterator hint, const key_type & key, M && value)
    {
        return generic_insert_or_assign(hint, key, std::forward<M>(value));
    }

    template <class M>
    iterator insert_or_assign(const_iterator hint, key_type && key, M && value)
    {
        return generic_insert_or_assign(hint, std::move(key), std::forward<M>(value));
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args &&... args)
    {
        key_type key = create_key(std::forward<Args>(args)...);
        return common_emplace(std::move(key), std::forward<Args>(args)...);
    }

    template <class... Args>
    iterator emplace_hint(const_iterator hint, Args &&... args)
    {
        key_type key = create_key(std::forward<Args>(args)...);
        if (check_hint(hint, key)) {
            return create_iterator(hint.m_pos);
        }
        return common_emplace(std::move(key), std::forward<Args>(args)...).first;
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const key_type & key, Args &&... args)
    {
        return generic_try_emplace(key, std::forward<Args>(args)...);
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(key_type && key, Args &&... args)
    {
        return generic_try_emplace(std::move(key), std::forward<Args>(args)...);
    }

    template <class... Args>
    iterator try_emplace(const_iterator hint, const key_type & key, Args &&... args)
    {
        return generic_try_emplace(hint, key, std::forward<Args>(args)...);
    }

    template <class... Args>
    iterator try_emplace(const_iterator hint, key_type && key, Args &&... args)
    {
        return generic_try_emplace(hint, std::move(key), std::forward<Args>(args)...);
    }

    iterator erase(const_iterator pos)
    {
        const size_type next = pos.get_node().next;
        remove_node(pos.m_pos);
        return create_iterator(next);
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        link_nodes(first.get_node().prev, last.m_pos);
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

    void swap(HashMap & other) noexcept
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

    mapped_type & at(const key_type & key)
    {
        if (const size_type pos = search(key); m_data[pos].is_used()) {
            return m_data[pos].get().value.second;
        }
        throw std::out_of_range("HashMap::at");
    }

    const mapped_type & at(const key_type & key) const
    {
        if (const size_type pos = search(key); m_data[pos].is_used()) {
            return m_data[pos].get().value.second;
        }
        throw std::out_of_range("HashMap::at");
    }

    mapped_type & operator[](const key_type & key)
    {
        return generic_subscript_operator(key);
    }

    mapped_type & operator[](key_type && key)
    {
        return generic_subscript_operator(std::move(key));
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
        HashMap old{std::move(*this)};
        m_data = std::vector<Element>(RehashPolicy::new_size(count, old.m_data.size()));
        reset();
        for (auto & value : old) {
            const size_type start = index(value.first);
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

    friend bool operator==(const HashMap & lhs, const HashMap & rhs)
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (const auto & value : lhs) {
            const const_iterator it = rhs.find(value.first);
            if (it == rhs.cend() || !(it->second == value.second)) {
                return false;
            }
        }
        return true;
    }

    friend bool operator!=(const HashMap & lhs, const HashMap & rhs)
    {
        return !(lhs == rhs);
    }

private:
    constexpr bool equal_keys(const key_type & a, const key_type & b) const noexcept
    {
        return key_equal::operator()(a, b);
    }

    constexpr size_type index(const key_type & key) const noexcept
    {
        return RangeHash::hash(hasher::operator()(key), m_data.size());
    }

    constexpr iterator create_iterator(const size_type pos) noexcept
    {
        return {pos, m_data.data()};
    }

    constexpr const_iterator create_const_iterator(const size_type pos) const noexcept
    {
        return {pos, m_data.data()};
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

    constexpr size_type find_pos(const key_type & key, const bool seek_erased) const noexcept
    {
        const size_type start = index(key);
        size_type first_erased = m_data.size();
        for (size_type step = 0, i = start;; i = CollisionPolicy::next(start, ++step, m_data.size())) {
            if (m_data[i].is_empty()) {
                return first_erased == m_data.size() ? i : first_erased;
            }
            if (m_data[i].is_used()) {
                if (equal_keys(m_data[i].get().value.first, key)) {
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

    template <class T, class M>
    std::pair<iterator, bool> generic_insert_or_assign(T && key, M && value)
    {
        std::pair<iterator, bool> result = try_emplace(std::forward<T>(key), std::forward<M>(value));
        if (!result.second) {
            result.first->second = std::forward<M>(value);
        }
        return result;
    }

    template <class T, class M>
    iterator generic_insert_or_assign(const_iterator hint, T && key, M && value)
    {
        if (check_hint(hint, key)) {
            m_data[hint.m_pos].get().value.second = std::forward<M>(value);
            return create_iterator(hint.m_pos);
        }
        return generic_insert_or_assign(std::forward<T>(key), std::forward<M>(value)).first;
    }

    template <class... Args>
    std::pair<iterator, bool> common_emplace(key_type && key, Args &&... args)
    {
        if (RehashPolicy::need_rehash(size() + 1, m_data.size())) {
            reserve(size() + 1);
        }
        const size_type pos = find_insertion_pos(key);
        const bool used = m_data[pos].is_used();
        if (!used) {
            insert_at(pos,
                      std::piecewise_construct,
                      std::forward_as_tuple(std::move(key)),
                      value_args(std::forward<Args>(args)...));
        }
        return {create_iterator(pos), !used};
    }

    template <class T, class... Args>
    std::pair<iterator, bool> generic_try_emplace(T && key, Args &&... args)
    {
        const size_type old_size = size();
        return {create_iterator(try_emplace_impl(std::forward<T>(key), std::forward<Args>(args)...)), old_size != size()};
    }

    template <class T, class... Args>
    iterator generic_try_emplace(const_iterator hint, T && key, Args &&... args)
    {
        if (check_hint(hint, key)) {
            return create_iterator(hint.m_pos);
        }
        return create_iterator(try_emplace_impl(std::forward<T>(key), std::forward<Args>(args)...));
    }

    template <class T, class... Args>
    size_type try_emplace_impl(T && key, Args &&... args)
    {
        if (RehashPolicy::need_rehash(size() + 1, m_data.size())) {
            reserve(size() + 1);
        }
        const size_type pos = find_insertion_pos(key);
        if (!m_data[pos].is_used()) {
            insert_at(pos,
                      std::piecewise_construct,
                      std::forward_as_tuple(std::forward<T>(key)),
                      std::forward_as_tuple(std::forward<Args>(args)...));
        }
        return pos;
    }

    template <class T>
    mapped_type & generic_subscript_operator(T && key)
    {
        return m_data[try_emplace_impl(std::forward<T>(key))].get().value.second;
    }

    template <class... Args>
    void insert_at(const size_type pos, Args &&... args)
    {
        m_data[pos].set(std::forward<Args>(args)...);
        m_data[pos].get().next = m_begin;
        if (m_begin != m_end) {
            m_data[m_begin].get().prev = pos;
        }
        m_begin = pos;
        ++m_size;
    }

    bool check_hint(const_iterator hint, const key_type & key)
    {
        return hint != cend() && equal_keys(hint->first, key);
    }

    template <class T>
    static constexpr key_type create_key(T && x)
    {
        return {std::forward<T>(x).first};
    }

    template <class... KeyArgs, class... ValueArgs>
    static constexpr key_type create_key(std::piecewise_construct_t,
                                         std::tuple<KeyArgs...> key_args,
                                         std::tuple<ValueArgs...>)
    {
        return std::make_from_tuple<key_type>(key_args);
    }

    template <class KeyArg, class ValueArg>
    static constexpr key_type create_key(KeyArg && key_arg, ValueArg &&)
    {
        return {std::forward<KeyArg>(key_arg)};
    }

    template <class T>
    static constexpr auto value_args(T && x)
    {
        return std::forward_as_tuple(std::forward<T>(x).second);
    }

    template <class... KeyArgs, class... ValueArgs>
    static constexpr auto value_args(std::piecewise_construct_t,
                                     std::tuple<KeyArgs...>,
                                     std::tuple<ValueArgs...> value_args)
    {
        return value_args;
    }

    template <class KeyArg, class ValueArg>
    static constexpr auto value_args(KeyArg &&, ValueArg && value_arg)
    {
        return std::forward_as_tuple(std::forward<ValueArg>(value_arg));
    }
};