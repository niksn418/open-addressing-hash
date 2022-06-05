#pragma once

#include <cstddef>
#include <limits>

struct LinearProbing
{
    static constexpr std::size_t next(const std::size_t start, const std::size_t step, const std::size_t size) noexcept
    {
        return (start + step) & (size - 1);
    }
};

struct QuadraticProbing
{
    static constexpr std::size_t next(std::size_t start, std::size_t step, std::size_t size) noexcept
    {
        if ((size & (size - 1)) == 0) { // Is size is power of 2, then we can choose coefficients that will avoid ending up in cycle
            return (start + ((step * step + step) >> 1)) & (size - 1);
        }
        return (start + step * step) % size;
    }
};

struct MaskRangeHashing
{
    static constexpr std::size_t hash(const std::size_t index, const std::size_t size) noexcept
    {
        return index & (size - 1);
    }
};

struct Power2RehashPolicy
{
    static constexpr float max_load_factor() noexcept
    {
        return 0.5;
    }

    static constexpr bool need_rehash(const std::size_t size, const std::size_t bucket_count) noexcept
    {
        return size > (bucket_count >> 1);
    }

    static constexpr std::size_t buckets_number(const std::size_t desired_size) noexcept
    {
        return desired_size << 1;
    }

    static constexpr std::size_t new_size(const std::size_t desired_size, std::size_t current_size = 64) noexcept
    {
        while (current_size < desired_size) {
            current_size <<= 1;
        }
        return current_size;
    }
};
