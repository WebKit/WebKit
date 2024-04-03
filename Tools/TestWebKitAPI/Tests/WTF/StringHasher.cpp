/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/DataLog.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHasherInlines.h>


namespace TestWebKitAPI {

static bool dumpHashingSpeedComparison = false;

TEST(WTF, StringHasher)
{
    auto generateLCharArray = [&](size_t size) {
        auto array = std::unique_ptr<LChar[]>(new LChar[size]);
        for (size_t i = 0; i < size; i++)
            array[i] = i;
        return array;
    };

    auto generateUCharArray = [&](size_t size) {
        auto array = std::unique_ptr<UChar[]>(new UChar[size]);
        for (size_t i = 0; i < size; i++)
            array[i] = i;
        return array;
    };

    StringHasher hash;
    unsigned max8Bit = std::numeric_limits<uint8_t>::max();
    for (size_t size = 0; size <= max8Bit; size++) {
        std::unique_ptr<const LChar[]> arr1 = generateLCharArray(size);
        std::unique_ptr<const UChar[]> arr2 = generateUCharArray(size);
        unsigned left = StringHasher::computeHashAndMaskTop8Bits(std::span { arr1.get(), size });
        unsigned right = StringHasher::computeHashAndMaskTop8Bits(std::span { arr2.get(), size });
        ASSERT_EQ(left, right);

        for (size_t i = 0; i < size; i++)
            hash.addCharacter(arr2.get()[i]);
        unsigned result1 = hash.hashWithTop8BitsMasked();
        ASSERT_EQ(right, result1);
    }
}

TEST(WTF, StringHasher_SuperFastHash_VS_WYHash)
{
    if (!dumpHashingSpeedComparison)
        return;

    size_t sum = 0;
    dataLogLn("UChar");
    for (size_t size = 2; size < (1 << 16); size *= 2) {
        Vector<UChar> vector;
        vector.resize(size);
        for (unsigned i = 0; i < size; ++i)
            vector[i] = i & 0x7f;
        sum += SuperFastHash::computeHashAndMaskTop8Bits(std::span<const UChar> { vector.data(), size });
        sum += WYHash::computeHashAndMaskTop8Bits(std::span<const UChar> { vector.data(), size });
        sum += StringHasher::computeHashAndMaskTop8Bits(std::span<const UChar> { vector.data(), size });
        auto start = MonotonicTime::now();
        for (unsigned i = 0; i < 1e5; ++i)
            sum += SuperFastHash::computeHashAndMaskTop8Bits(std::span<const UChar> { vector.data(), size });
        dataLogLn("SFH ", size, " -> ", MonotonicTime::now() - start);
        start = MonotonicTime::now();
        for (unsigned i = 0; i < 1e5; ++i)
            sum += WYHash::computeHashAndMaskTop8Bits(std::span<const UChar> { vector.data(), size });
        dataLogLn("WYH ", size, " -> ", MonotonicTime::now() - start);
        start = MonotonicTime::now();
        for (unsigned i = 0; i < 1e5; ++i)
            sum += StringHasher::computeHashAndMaskTop8Bits(std::span<const UChar> { vector.data(), size });
        dataLogLn("STH ", size, " -> ", MonotonicTime::now() - start);
    }

    dataLogLn("LChar");
    for (size_t size = 2; size < (1 << 16); size *= 2) {
        Vector<LChar> vector;
        vector.resize(size);
        for (unsigned i = 0; i < size; ++i)
            vector[i] = i & 0x7f;
        sum += SuperFastHash::computeHashAndMaskTop8Bits(std::span<const LChar> { vector.data(), size });
        sum += WYHash::computeHashAndMaskTop8Bits(std::span<const LChar> { vector.data(), size });
        sum += StringHasher::computeHashAndMaskTop8Bits(std::span<const LChar> { vector.data(), size });
        auto start = MonotonicTime::now();
        for (unsigned i = 0; i < 1e5; ++i)
            sum += SuperFastHash::computeHashAndMaskTop8Bits(std::span<const LChar> { vector.data(), size });
        dataLogLn("SFH ", size, " -> ", MonotonicTime::now() - start);
        start = MonotonicTime::now();
        for (unsigned i = 0; i < 1e5; ++i)
            sum += WYHash::computeHashAndMaskTop8Bits(std::span<const LChar> { vector.data(), size });
        dataLogLn("WYH ", size, " -> ", MonotonicTime::now() - start);
        start = MonotonicTime::now();
        for (unsigned i = 0; i < 1e5; ++i)
            sum += StringHasher::computeHashAndMaskTop8Bits(std::span<const LChar> { vector.data(), size });
        dataLogLn("STH ", size, " -> ", MonotonicTime::now() - start);
    }
    dataLogLn(sum);
}

} // namespace TestWebKitAPI
