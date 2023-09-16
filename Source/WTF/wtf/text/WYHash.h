/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * License header from wyhash https://github.com/wangyi-fudan/wyhash/blob/master/LICENSE
 * 
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <wtf/Int128.h>
#include <wtf/UnalignedAccess.h>
#include <wtf/text/StringHasher.h>

namespace WTF {

// https://github.com/wangyi-fudan/wyhash
class WYHash {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr bool forceConvertInRead = false;
    static constexpr unsigned flagCount = StringHasher::flagCount;
    static constexpr unsigned maskHash = StringHasher::maskHash;
    static constexpr uint64_t secret[4] = { 0xa0761d6478bd642full, 0xe7037ed1a0b428dbull, 0x8ebc6af09c88c6e3ull, 0x589965cc75374cc3ull };
    using DefaultConverter = StringHasher::DefaultConverter;

    WYHash() = default;

    template<typename T, typename Converter = DefaultConverter>
    ALWAYS_INLINE static constexpr unsigned computeHashAndMaskTop8Bits(const T* data, unsigned characterCount)
    {
        return StringHasher::avoidZero(computeHashImpl<T, Converter>(data, characterCount) & StringHasher::maskHash);
    }

private:
    friend class StringHasher;

    ALWAYS_INLINE static constexpr uint64_t wyrot(uint64_t x)
    {
        return (x >> 32) | (x << 32);
    }

    ALWAYS_INLINE static constexpr void wymum(uint64_t* A, uint64_t* B)
    {
        UInt128 r = static_cast<UInt128>(*A) * *B;
        *A = static_cast<uint64_t>(r);
        *B = static_cast<uint64_t>(r >> 64);
    }

    ALWAYS_INLINE static constexpr uint64_t wymix(uint64_t A, uint64_t B)
    {
        wymum(&A, &B);
        return A ^ B;
    }

    ALWAYS_INLINE static constexpr uint64_t bigEndianToLittleEndian8(uint64_t v)
    {
        return (((v >> 56) & 0xff) | ((v >> 40) & 0xff00) | ((v >> 24) & 0xff0000) | ((v >> 8) & 0xff000000)
            | ((v << 8) & 0xff00000000) | ((v << 24) & 0xff0000000000) | ((v << 40) & 0xff000000000000) | ((v << 56) & 0xff00000000000000));
    }

    ALWAYS_INLINE static constexpr uint32_t bigEndianToLittleEndian4(uint32_t v)
    {
        return (((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000));
    }

    ALWAYS_INLINE static constexpr uint32_t bigEndianToLittleEndian2(uint16_t v)
    {
        return ((v >> 8) & 0xff) | ((v << 8) & 0xff00);
    }

    template<typename T, typename Converter = DefaultConverter>
    struct Reader16Bit {
        ALWAYS_INLINE static constexpr bool hasDefaultConverter()
        {
            return std::is_same<Converter, DefaultConverter>::value;
        }

        ALWAYS_INLINE static constexpr uint32_t convert(const T p)
        {
            return Converter::convert(static_cast<uint16_t>(p));
        }

        ALWAYS_INLINE static constexpr uint64_t wyr3(const T* p)
        {
            static_assert(sizeof(T) == 2);
            uint16_t v = convert(p[0]);
            uint64_t firstByte = static_cast<uint8_t>(v);
            uint64_t secondByte = static_cast<uint8_t>(v >> 8);
            // Since it's only used for one character (16 bits) case,
            // this aligns with the original algorithm which copy in
            // big endian no matter what current CPU(BIG_ENDIAN) is.
            return (firstByte << 16) | (secondByte << 8) | secondByte;
        }

        ALWAYS_INLINE static constexpr uint64_t wyr4WithConvert(const T* p)
        {
            return static_cast<uint64_t>(convert(p[0]))
                | (static_cast<uint64_t>(convert(p[1])) << 16);
        }

        ALWAYS_INLINE static constexpr uint64_t wyr4(const T* p)
        {
            static_assert(sizeof(T) == 2);
            if (std::is_constant_evaluated() || forceConvertInRead)
                return wyr4WithConvert(p);
            uint32_t value = unalignedLoad<uint32_t>(p);
#if CPU(BIG_ENDIAN)
            value = bigEndianToLittleEndian4(value);
#endif
            return value;
        }

        ALWAYS_INLINE static constexpr uint64_t wyr8WithConvert(const T* p)
        {
            return static_cast<uint64_t>(convert(p[0]))
                | (static_cast<uint64_t>(convert(p[1])) << 16)
                | (static_cast<uint64_t>(convert(p[2])) << 32)
                | (static_cast<uint64_t>(convert(p[3])) << 48);
        }

        ALWAYS_INLINE static constexpr uint64_t wyr8(const T* p)
        {
            static_assert(sizeof(T) == 2);
            if (std::is_constant_evaluated() || forceConvertInRead)
                return wyr8WithConvert(p);
            uint64_t value = unalignedLoad<uint64_t>(p);
#if CPU(BIG_ENDIAN)
            value = bigEndianToLittleEndian8(value);
#endif
            return value;
        }
    };

    // LChar data is interpreted as Latin-1-encoded (zero-extended to 16 bits).
    // To match the hash value of UChar with same content, extend 16 bits (0xff)
    // to 32 bits (0x00ff).
    template<typename T, typename Converter = DefaultConverter>
    struct Reader8Bit {
        ALWAYS_INLINE static constexpr bool hasDefaultConverter()
        {
            return std::is_same<Converter, DefaultConverter>::value;
        }

        ALWAYS_INLINE static constexpr uint32_t convert(const T p)
        {
            return Converter::convert(static_cast<uint8_t>(p));
        }

        ALWAYS_INLINE static constexpr uint64_t wyr3(const T* p)
        {
            static_assert(sizeof(T) == 1);
            return static_cast<uint64_t>(convert(p[0])) << 16;
        }

        ALWAYS_INLINE static constexpr uint64_t wyr4WithConvert(const T* p)
        {
            return static_cast<uint64_t>(convert(p[0]))
                | (static_cast<uint64_t>(convert(p[1])) << 16);
        }

        ALWAYS_INLINE static constexpr uint64_t wyr4(const T* p)
        {
            static_assert(sizeof(T) == 1);
            if (std::is_constant_evaluated() || forceConvertInRead)
                return wyr4WithConvert(p);
            // Copy 16 bits and extends to 32 bits.
            uint16_t v16 = unalignedLoad<uint16_t>(p);
#if CPU(BIG_ENDIAN)
            v16 = bigEndianToLittleEndian2(v16);
#endif
            uint32_t v32 = static_cast<uint32_t>(v16);
            return (v32 | (v32 << 8)) & 0x00ff00ffUL;
        }

        ALWAYS_INLINE static constexpr uint64_t wyr8WithConvert(const T* p)
        {
            return static_cast<uint64_t>(convert(p[0]))
                | (static_cast<uint64_t>(convert(p[1])) << 16)
                | (static_cast<uint64_t>(convert(p[2])) << 32)
                | (static_cast<uint64_t>(convert(p[3])) << 48);
        }

        ALWAYS_INLINE static constexpr uint64_t wyr8(const T* p)
        {
            static_assert(sizeof(T) == 1);
            if (std::is_constant_evaluated() || forceConvertInRead)
                return wyr8WithConvert(p);
            // Copy 32 bits and Extends to 64 bits.
            uint32_t v32 = unalignedLoad<uint32_t>(p);
#if CPU(BIG_ENDIAN)
            v32 = bigEndianToLittleEndian4(v32);
#endif
            uint64_t v64 = static_cast<uint64_t>(v32);
            v64 = (v64 | (v64 << 16)) & 0x0000ffff0000ffffULL;
            return (v64 | (v64 << 8)) & 0x00ff00ff00ff00ffULL;
        }
    };

    ALWAYS_INLINE static constexpr uint64_t initSeed()
    {
        return wymix(secret[0], secret[1]);
    }

    template<typename T, typename Read8Functor>
    ALWAYS_INLINE static constexpr void consume24Characters(const T* p, const Read8Functor& wyr8, uint64_t& seed, uint64_t& see1, uint64_t& see2)
    {
        seed = wymix(wyr8(p) ^ secret[1], wyr8(p + 4) ^ seed);
        see1 = wymix(wyr8(p + 8) ^ secret[2], wyr8(p + 12) ^ see1);
        see2 = wymix(wyr8(p + 16) ^ secret[3], wyr8(p + 20) ^ see2);
    }

    ALWAYS_INLINE static constexpr unsigned handleEndCase(uint64_t a, uint64_t b, const uint64_t seed, const uint64_t byteCount)
    {
        a ^= secret[1];
        b ^= seed;

        wymum(&a, &b);
        return wymix(a ^ secret[0] ^ byteCount, b ^ secret[1]);
    }

    template<typename T, typename Read8Functor>
    ALWAYS_INLINE static constexpr void handleGreaterThan8CharactersCase(T*& p, unsigned& i, const Read8Functor& wyr8, uint64_t& seed, uint64_t see1, uint64_t see2)
    {
        if (UNLIKELY(i > 24)) {
            do {
                consume24Characters(p, wyr8, seed, see1, see2);
                p += 24;
                i -= 24;
            } while (LIKELY(i > 24));
            seed ^= see1 ^ see2;
        }
        while (UNLIKELY(i > 8)) {
            seed = wymix(wyr8(p) ^ secret[1], wyr8(p + 4) ^ seed);
            i -= 8;
            p += 8;
        }
    }

    template<typename T, typename Reader>
    ALWAYS_INLINE static constexpr unsigned hash(const T* p, const unsigned characterCount)
    {
        auto wyr3 = Reader::wyr3;
        auto wyr4 = Reader::wyr4;
        auto wyr8 = Reader::wyr8;

        if constexpr (!Reader::hasDefaultConverter()) {
            wyr4 = Reader::wyr4WithConvert;
            wyr8 = Reader::wyr8WithConvert;
        }

        uint64_t a = 0;
        uint64_t b = 0;
        const uint64_t byteCount = static_cast<uint64_t>(characterCount) << 1;
        uint64_t seed = initSeed();

        if (LIKELY(characterCount <= 8)) {
            if (LIKELY(characterCount >= 2)) {
                const uint64_t offset = ((byteCount >> 3) << 2) >> 1;
                a = (wyr4(p) << 32) | wyr4(p + offset);
                p += characterCount - 2;
                b = (wyr4(p) << 32) | wyr4(p - offset);
            } else if (LIKELY(characterCount > 0)) {
                a = wyr3(p);
                b = 0;
            } else {
                a = 0;
                b = 0;
            }
        } else {
            unsigned i = characterCount;
            handleGreaterThan8CharactersCase(p, i, wyr8, seed, seed, seed);
            a = wyr8(p + i - 8);
            b = wyr8(p + i - 4);
        }
        return handleEndCase(a, b, seed, byteCount);
    }

    template<typename T, typename Converter = DefaultConverter>
    static constexpr unsigned computeHashImpl(const T* characters, unsigned characterCount)
    {
        if constexpr (sizeof(T) == 2)
            return hash<T, Reader16Bit<T, Converter>>(characters, characterCount);
        else
            return hash<T, Reader8Bit<T, Converter>>(characters, characterCount);
    }
};

} // namespace WTF

using WTF::WYHash;
