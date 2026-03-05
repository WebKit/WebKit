//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// hash_utils.h: Hashing based helper functions.

#ifndef COMMON_HASHUTILS_H_
#define COMMON_HASHUTILS_H_

#include "common/debug.h"
#include "common/span.h"
#include "xxhash.h"

namespace angle
{
// Computes a hash of "key". Any data passed to this function must be multiples of
// 4 bytes, since the PMurHash32 method can only operate increments of 4-byte words.
inline size_t ComputeGenericHash(angle::Span<const uint8_t> key)
{
    constexpr unsigned int kSeed = 0xABCDEF98;

    // We can't support "odd" alignments.  ComputeGenericHash requires aligned types
    ASSERT(key.size() % 4 == 0);
#if defined(ANGLE_IS_64_BIT_CPU)
    return XXH64(key.data(), key.size(), kSeed);
#else
    return XXH32(key.data(), key.size(), kSeed);
#endif  // defined(ANGLE_IS_64_BIT_CPU)
}

inline void HashCombine(size_t &seed) {}

template <typename T, typename... Rest>
inline void HashCombine(std::size_t &seed, const T &hashableObject, Rest... rest)
{
    std::hash<T> hasher;
    seed ^= hasher(hashableObject) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    HashCombine(seed, rest...);
}

template <typename T, typename... Rest>
inline size_t HashMultiple(const T &hashableObject, Rest... rest)
{
    size_t seed = 0;
    HashCombine(seed, hashableObject, rest...);
    return seed;
}

}  // namespace angle

#endif  // COMMON_HASHUTILS_H_
