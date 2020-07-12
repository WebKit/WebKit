/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <array>
#include <limits>
#include <wtf/Hasher.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

// Test against actual hash values.
// We don't really depend on specific values, but it makes testing simpler.
// Could instead construct tests that check hashes against each other.
// No big deal to update all of these if we change the hash algorithm.

const uint32_t emptyHash = 82610334U;
const uint32_t zero32BitHash = 242183504U;
const uint32_t zero64BitHash = 579236260U;
const uint32_t one32BitHash = 690138192U;
const uint32_t one64BitHash = 1621454911U;

TEST(WTF, Hasher_integer)
{
    EXPECT_EQ(zero32BitHash, computeHash(0U));
    EXPECT_EQ(zero32BitHash, computeHash(false));
    EXPECT_EQ(zero32BitHash, computeHash(static_cast<char>(0)));
    EXPECT_EQ(zero32BitHash, computeHash(static_cast<int8_t>(0)));
    EXPECT_EQ(zero32BitHash, computeHash(static_cast<uint8_t>(0)));
    EXPECT_EQ(zero32BitHash, computeHash(static_cast<int16_t>(0)));
    EXPECT_EQ(zero32BitHash, computeHash(static_cast<uint16_t>(0)));
    EXPECT_EQ(zero32BitHash, computeHash(0));

    EXPECT_EQ(zero64BitHash, computeHash(0ULL));
    EXPECT_EQ(sizeof(long) == sizeof(int32_t) ? zero32BitHash : zero64BitHash, computeHash(0L));
    EXPECT_EQ(sizeof(unsigned long) == sizeof(uint32_t) ? zero32BitHash : zero64BitHash, computeHash(0UL));
    EXPECT_EQ(zero64BitHash, computeHash(0LL));

    EXPECT_EQ(one32BitHash, computeHash(1U));
    EXPECT_EQ(one32BitHash, computeHash(true));
    EXPECT_EQ(one32BitHash, computeHash(static_cast<char>(1)));
    EXPECT_EQ(one32BitHash, computeHash(static_cast<int8_t>(1)));
    EXPECT_EQ(one32BitHash, computeHash(static_cast<uint8_t>(1)));
    EXPECT_EQ(one32BitHash, computeHash(static_cast<int16_t>(1)));
    EXPECT_EQ(one32BitHash, computeHash(static_cast<uint16_t>(1)));
    EXPECT_EQ(one32BitHash, computeHash(1));

    EXPECT_EQ(one64BitHash, computeHash(1ULL));
    EXPECT_EQ(sizeof(long) == sizeof(int32_t) ? one32BitHash : one64BitHash, computeHash(1L));
    EXPECT_EQ(sizeof(unsigned long) == sizeof(uint32_t) ? one32BitHash : one64BitHash, computeHash(1UL));
    EXPECT_EQ(one64BitHash, computeHash(1LL));

    EXPECT_EQ(1772381661U, computeHash(0x7FU));
    EXPECT_EQ(1772381661U, computeHash(std::numeric_limits<int8_t>::max()));

    EXPECT_EQ(3730877340U, computeHash(0x80U));
    EXPECT_EQ(3730877340U, computeHash(std::numeric_limits<int8_t>::min()));

    EXPECT_EQ(376510634U, computeHash(0xFFU));
    EXPECT_EQ(376510634U, computeHash(std::numeric_limits<uint8_t>::max()));
    EXPECT_EQ(376510634U, computeHash(static_cast<char>(-1)));
    EXPECT_EQ(376510634U, computeHash(static_cast<int8_t>(-1)));

    EXPECT_EQ(262632278U, computeHash(0x7FFFU));
    EXPECT_EQ(262632278U, computeHash(std::numeric_limits<int16_t>::max()));

    EXPECT_EQ(2981978661U, computeHash(0x8000U));
    EXPECT_EQ(2981978661U, computeHash(std::numeric_limits<int16_t>::min()));

    EXPECT_EQ(894984179U, computeHash(0xFFFFU));
    EXPECT_EQ(894984179U, computeHash(std::numeric_limits<uint16_t>::max()));
    EXPECT_EQ(894984179U, computeHash(static_cast<int16_t>(-1)));

    EXPECT_EQ(3430670328U, computeHash(0x7FFFFFFFU));
    EXPECT_EQ(3430670328U, computeHash(std::numeric_limits<int32_t>::max()));

    EXPECT_EQ(2425683428U, computeHash(0x80000000U));
    EXPECT_EQ(2425683428U, computeHash(std::numeric_limits<int32_t>::min()));

    EXPECT_EQ(1955511435U, computeHash(0xFFFFFFFFU));
    EXPECT_EQ(1955511435U, computeHash(std::numeric_limits<uint32_t>::max()));
    EXPECT_EQ(1955511435U, computeHash(-1));

    EXPECT_EQ(1264532604U, computeHash(0x8000000000000000ULL));
    EXPECT_EQ(1264532604U, computeHash(std::numeric_limits<int64_t>::min()));
    EXPECT_EQ(sizeof(long) == sizeof(int32_t) ? 2425683428U : 1264532604U, computeHash(std::numeric_limits<long>::min()));

    EXPECT_EQ(2961049834U, computeHash(0x7FFFFFFFFFFFFFFFULL));
    EXPECT_EQ(2961049834U, computeHash(std::numeric_limits<int64_t>::max()));
    EXPECT_EQ(sizeof(long) == sizeof(int32_t) ? 3430670328U : 2961049834U, computeHash(std::numeric_limits<long>::max()));

    EXPECT_EQ(1106332091U, computeHash(0xFFFFFFFFFFFFFFFFULL));
    EXPECT_EQ(1106332091U, computeHash(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(sizeof(long) == sizeof(int32_t) ? 1955511435U : 1106332091U, computeHash(std::numeric_limits<unsigned long>::max()));
    EXPECT_EQ(sizeof(long) == sizeof(int32_t) ? 1955511435U : 1106332091U, computeHash(-1L));
    EXPECT_EQ(1106332091U, computeHash(-1LL));
}

TEST(WTF, Hasher_floatingPoint)
{
    EXPECT_EQ(zero64BitHash, computeHash(0.0));
    EXPECT_EQ(1264532604U, computeHash(-0.0)); // Note, not same as hash of 0.0.
    EXPECT_EQ(one64BitHash, computeHash(std::numeric_limits<double>::denorm_min()));

    EXPECT_EQ(2278399980U, computeHash(1.0));
    EXPECT_EQ(3870689297U, computeHash(-1.0));

    EXPECT_EQ(3016344414U, computeHash(std::numeric_limits<double>::min()));
    EXPECT_EQ(1597662982U, computeHash(std::numeric_limits<double>::max()));

    EXPECT_EQ(2501702556U, computeHash(std::numeric_limits<double>::lowest()));
    EXPECT_EQ(2214439802U, computeHash(std::numeric_limits<double>::epsilon()));

    EXPECT_EQ(2678086759U, computeHash(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_EQ(2304445393U, computeHash(std::numeric_limits<double>::infinity()));
    EXPECT_EQ(2232593311U, computeHash(-std::numeric_limits<double>::infinity()));

    EXPECT_EQ(zero32BitHash, computeHash(0.0f));
    EXPECT_EQ(2425683428U, computeHash(-0.0f)); // Note, not same as hash of 0.0f.
    EXPECT_EQ(one32BitHash, computeHash(std::numeric_limits<float>::denorm_min()));

    EXPECT_EQ(1081575966U, computeHash(1.0f));
    EXPECT_EQ(3262093188U, computeHash(-1.0f));

    EXPECT_EQ(3170189524U, computeHash(std::numeric_limits<float>::min()));
    EXPECT_EQ(11021299U, computeHash(std::numeric_limits<float>::max()));

    EXPECT_EQ(3212069506U, computeHash(std::numeric_limits<float>::lowest()));
    EXPECT_EQ(1308784506U, computeHash(std::numeric_limits<float>::epsilon()));

    EXPECT_EQ(2751288511U, computeHash(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(3457049256U, computeHash(std::numeric_limits<float>::infinity()));
    EXPECT_EQ(4208873971U, computeHash(-std::numeric_limits<float>::infinity()));
}

TEST(WTF, Hasher_multiple)
{
    EXPECT_EQ(emptyHash, computeHash());
    EXPECT_EQ(emptyHash, computeHash(std::make_tuple()));
    EXPECT_EQ(emptyHash, computeHash(std::array<int, 0> { }));
    EXPECT_EQ(emptyHash, computeHash(Vector<int> { }));
    EXPECT_EQ(emptyHash, computeHash(Vector<int, 1> { }));

    EXPECT_EQ(zero32BitHash, computeHash(std::array<int, 1> { { 0 } }));
    EXPECT_EQ(zero32BitHash, computeHash(Vector<int> { 0 }));
    EXPECT_EQ(zero32BitHash, computeHash(Vector<int, 1> { 0 }));
    EXPECT_EQ(zero32BitHash, computeHash(Optional<int> { WTF::nullopt }));
    EXPECT_EQ(zero32BitHash, computeHash(std::make_tuple(0)));

    EXPECT_EQ(one64BitHash, computeHash(1, 0));
    EXPECT_EQ(one64BitHash, computeHash(std::make_tuple(1, 0)));
    EXPECT_EQ(one64BitHash, computeHash(std::make_pair(1, 0)));
    EXPECT_EQ(one64BitHash, computeHash(std::array<int, 2> { { 1, 0 } }));
    EXPECT_EQ(one64BitHash, computeHash({ 1, 0 }));
    EXPECT_EQ(one64BitHash, computeHash(Optional<int> { 0 }));
    EXPECT_EQ(one64BitHash, computeHash(Vector<int> { { 1, 0 } }));
    EXPECT_EQ(one64BitHash, computeHash(Vector<int, 1> { { 1, 0 } }));

    EXPECT_EQ(one64BitHash, computeHash(std::make_tuple(1), std::array<int, 1> { { 0 } }));
    EXPECT_EQ(one64BitHash, computeHash(std::make_tuple(std::make_tuple(1), std::array<int, 1> { { 0 } })));

    EXPECT_EQ(1652352321U, computeHash(1, 2, 3, 4));
    EXPECT_EQ(1652352321U, computeHash(std::make_tuple(1, 2, 3, 4)));
    EXPECT_EQ(1652352321U, computeHash(std::make_pair(std::make_pair(1, 2), std::make_pair(3, 4))));
}

struct HasherAddCustom1 { };

void add(Hasher& hasher, const HasherAddCustom1&)
{
    add(hasher, 1, 2, 3, 4);
}

struct HasherAddCustom2 { };

void add(Hasher& hasher, const HasherAddCustom2&)
{
    add(hasher, { 1, 2, 3, 4 });
}

TEST(WTF, Hasher_custom)
{
    EXPECT_EQ(1652352321U, computeHash(HasherAddCustom1 { }));
    EXPECT_EQ(1652352321U, computeHash(HasherAddCustom2 { }));
}

#if 0 // FIXME: Add support for tuple-like classes.

struct HasherAddTupleLikeClass1 {
    std::array<int, 4> array { { 1, 2, 3, 4 } };
    template<size_t i> int get() const { return std::get<i>(array); }
};

struct HasherAddTupleLikeClass2 {
    std::array<int, 4> array { { 1, 2, 3, 4 } };
};

}

namespace std {

// FIXME: Documentation at cppreference.cpp says std::tuple_size is a struct, but it's a class in current macOS tools.
// FIXME: It's inelegant to inject this into the std namespace. Is that really how a tuple-like class needs to be defined?
// FIXME: This is so inconvenient that I am not sure it's something we want to do for lots of classes in WebKit.
template<> class std::tuple_size<TestWebKitAPI::HasherAddTupleLikeClass1> : public std::integral_constant<size_t, std::tuple_size<decltype(TestWebKitAPI::HasherAddTupleLikeClass1::array)>::value> { };

template<> class std::tuple_size<TestWebKitAPI::HasherAddTupleLikeClass2> : public std::integral_constant<size_t, std::tuple_size<decltype(TestWebKitAPI::HasherAddTupleLikeClass2::array)>::value> { };

}

namespace TestWebKitAPI {

// FIXME: Is it OK for the get to be in the class's namespace and rely on argument-dependent lookup?
// Or does this function template need to be moved into the std namespace like the tuple_size specialization?
template<size_t i> int get(const HasherAddTupleLikeClass2& object)
{
    return get<i>(object.array);
}

TEST(WTF, Hasher_tupleLike)
{
    EXPECT_EQ(1652352321U, computeHash(HasherAddTupleLikeClass1 { }));
    EXPECT_EQ(1652352321U, computeHash(HasherAddTupleLikeClass2 { }));
}

#endif

} // namespace TestWebKitAPI
