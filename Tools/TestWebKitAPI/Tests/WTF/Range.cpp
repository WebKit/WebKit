/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <wtf/Range.h>

#include <wtf/RangeSet.h>

namespace TestWebKitAPI {

using IntRange = WTF::Range<int>;

TEST(WTF_Range, Subsumes)
{
    IntRange range(10, 20);

    EXPECT_TRUE(range.subsumes(range));
    EXPECT_TRUE(range.subsumes(IntRange(10, 15)));
    EXPECT_TRUE(range.subsumes(IntRange(15, 20)));
    EXPECT_TRUE(range.subsumes(IntRange(12)));

    EXPECT_FALSE(range.subsumes(IntRange(9, 20)));
    EXPECT_FALSE(range.subsumes(IntRange(10, 21)));
    EXPECT_FALSE(range.subsumes(IntRange(5, 25)));
    EXPECT_FALSE(range.subsumes(IntRange(20, 30)));
    EXPECT_FALSE(range.subsumes(IntRange(20)));

    EXPECT_FALSE(IntRange(12, 15).subsumes(range));
    EXPECT_TRUE(IntRange::top().subsumes(range));
}

TEST(WTF_Range, SubsumesEmpty)
{
    IntRange empty;

    EXPECT_TRUE(IntRange(10, 20).subsumes(empty));
    EXPECT_TRUE(IntRange(10, 20).subsumes(IntRange(15, 15)));
    EXPECT_TRUE(empty.subsumes(empty));
    EXPECT_FALSE(empty.subsumes(IntRange(0, 1)));
}

static RangeSet<IntRange> makeRangeSet(std::initializer_list<IntRange> ranges)
{
    RangeSet<IntRange> set;
    for (const IntRange& range : ranges)
        set.add(range);
    return set;
}

TEST(WTF_RangeSet, Overlaps)
{
    RangeSet<IntRange> empty;
    EXPECT_FALSE(empty.overlaps(IntRange(0, 10)));
    EXPECT_FALSE(empty.overlaps(IntRange::top()));

    auto set = makeRangeSet({ IntRange(20, 30), IntRange(0, 10), IntRange(25, 35), IntRange(2, 4) });
    EXPECT_TRUE(set.overlaps(IntRange(3, 4)));
    EXPECT_TRUE(set.overlaps(IntRange(21, 22)));
    EXPECT_FALSE(set.overlaps(IntRange(12, 18)));
    EXPECT_FALSE(set.overlaps(IntRange(10, 20)));
    EXPECT_TRUE(set.overlaps(IntRange(8, 22)));
    EXPECT_TRUE(set.overlaps(IntRange::top()));
    EXPECT_FALSE(set.overlaps(IntRange()));
    EXPECT_FALSE(set.overlaps(IntRange(40, 50)));
    EXPECT_FALSE(set.overlaps(IntRange(-10, 0)));
    EXPECT_TRUE(set.overlaps(IntRange(34, 35)));
    EXPECT_FALSE(set.overlaps(IntRange(35, 36)));

    auto everything = makeRangeSet({ IntRange::top() });
    EXPECT_TRUE(everything.overlaps(IntRange(100, 101)));
}

TEST(WTF_RangeSet, Contains)
{
    RangeSet<IntRange> empty;
    EXPECT_FALSE(empty.contains(IntRange(0, 10)));
    EXPECT_FALSE(empty.contains(IntRange()));

    auto set = makeRangeSet({ IntRange(0, 10), IntRange(2, 4), IntRange(10, 15), IntRange(20, 30), IntRange(25, 35) });
    EXPECT_TRUE(set.contains(IntRange(2, 4)));
    EXPECT_TRUE(set.contains(IntRange(0, 10)));
    // Adjacent ranges contain a range that spans them.
    EXPECT_TRUE(set.contains(IntRange(5, 15)));
    EXPECT_FALSE(set.contains(IntRange(5, 16)));
    EXPECT_FALSE(set.contains(IntRange(14, 21)));
    EXPECT_TRUE(set.contains(IntRange(21, 35)));
    EXPECT_FALSE(set.contains(IntRange(21, 36)));
    EXPECT_FALSE(set.contains(IntRange(40, 50)));
    EXPECT_FALSE(set.contains(IntRange::top()));
    EXPECT_FALSE(set.contains(IntRange()));

    auto everything = makeRangeSet({ IntRange::top() });
    EXPECT_TRUE(everything.contains(IntRange(100, 101)));
    EXPECT_TRUE(everything.contains(IntRange::top()));
}

TEST(WTF_RangeSet, ManyRanges)
{
    constexpr int count = 1000;
    RangeSet<IntRange> set;
    for (int i = count; i--;)
        set.add(IntRange(i * 10, i * 10 + 5));

    for (int i = 0; i < count; ++i) {
        EXPECT_TRUE(set.contains(IntRange(i * 10, i * 10 + 5)));
        EXPECT_TRUE(set.overlaps(IntRange(i * 10 + 4, i * 10 + 6)));
        EXPECT_FALSE(set.overlaps(IntRange(i * 10 + 5, i * 10 + 10)));
        EXPECT_FALSE(set.contains(IntRange(i * 10 + 4, i * 10 + 6)));
    }
    EXPECT_FALSE(set.overlaps(IntRange(-5, 0)));
    EXPECT_FALSE(set.overlaps(IntRange(count * 10, count * 10 + 5)));
}

} // namespace TestWebKitAPI
