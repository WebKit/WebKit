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
    // We can't support "odd" alignments.  ComputeGenericHash requires aligned types
    ASSERT(key.size() % 4 == 0);
#if defined(ANGLE_IS_64_BIT_CPU)
    return XXH3_64bits(key.data(), key.size());
#else
    constexpr unsigned int kSeed = 0xABCDEF98;
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

// A streaming hasher utility that can hash blobs without alignment requirements
// based on the streaming variant of the XXH3 128 bit hasher. It produces a
// non-cryptographic hash with a size of 16 bytes. Example usage -
//
//      StreamingHasher hasher;
//      hasher.Update(const uint8_t *data, size_t size);
//      hasher.Update(const unsigned char *data, size_t size);
//      UpdateHashWithValue(hasher, shortVal);
//      UpdateHashWithValue(hasher, uintVal);
//      // A 16-byte uint8_t array containing the hash
//      const uint8_t *hash = nullptr;
//      hash = hasher.Digest();
//      // Hasher can be reused without re-initialization
//      UpdateHashWithValue(hasher, sizeVal);
//      hash = hasher.Digest();
//      UpdateHashWithValue(hasher, floatVal);
//      hash = hasher.Digest();
class StreamingHasher final : angle::NonCopyable
{
  public:
    StreamingHasher() : mSeed(0x5EED) { init(); }
    StreamingHasher(size_t seed) : mSeed(seed) { init(); }

    ~StreamingHasher()
    {
        XXH3_freeState(mState);
        mState = nullptr;
    }

    ANGLE_INLINE void Update(const void *data, size_t size)
    {
        ASSERT(valid());
        ASSERT(size == 0 || data != nullptr);
        XXH3_128bits_update(mState, data, size);
    }

    ANGLE_INLINE const unsigned char *Digest()
    {
        ASSERT(valid());
        mHash = XXH3_128bits_digest(mState);
        return mDigest.data();
    }

    // Only used in perf tests
    ANGLE_INLINE void Init() {}
    ANGLE_INLINE void Final() {}

    // XXH3_128bits_digest(...) generates a 128-bit / 16-byte hash
    static constexpr size_t kHashSize = 16;

  private:
    ANGLE_INLINE void init()
    {
        mHash  = {};
        mState = nullptr;
        mState = XXH3_createState();
        ASSERT(valid());
        reset();
    }

    ANGLE_INLINE bool valid() { return mState != nullptr; }

    ANGLE_INLINE void reset()
    {
        ASSERT(valid());
        XXH3_128bits_reset_withSeed(mState, mSeed);
    }

    XXH64_hash_t mSeed;
    XXH3_state_t *mState;

    union
    {
        XXH128_hash_t mHash;
        std::array<uint8_t, kHashSize> mDigest;
    };
};

template <typename Hasher, typename T>
ANGLE_INLINE void UpdateHashWithValue(Hasher &hasher, T value)
{
    static_assert(std::is_fundamental<T>::value || std::is_enum<T>::value);
    hasher.Update(&value, sizeof(T));
}

}  // namespace angle

#endif  // COMMON_HASHUTILS_H_
