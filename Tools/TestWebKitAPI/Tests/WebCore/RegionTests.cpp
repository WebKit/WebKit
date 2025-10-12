/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * modification, are permitted provided that the following conditions
 * Redistribution and use in source and binary forms, with or without
 * 1. Redistributions of source code must retain the above copyright
 * are met:
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

#include "WebCoreTestUtilities.h"
#include <WebCore/Region.h>
#include <random>
#include <wtf/text/StringBuilder.h>

template<typename T>
static String convertToString(const T& value)
{
    TextStream stream(TextStream::LineMode::SingleLine);
    stream << value;
    return stream.release();
}

template<typename T>
static String convertToTrimmedString(const T& value)
{
    return makeStringByReplacingAll(convertToString(value), '\n', ' ').trim(isUnicodeWhitespace);
}

namespace WebCore {

inline std::ostream& operator<<(std::ostream& os, const WebCore::Region& value)
{
    return os << convertToString(value);
}

inline std::ostream& operator<<(std::ostream& os, const WebCore::Region::Shape& value)
{
    return os << convertToString(value);
}

}

namespace TestWebKitAPI {
using namespace WebCore;
using Shape = Region::Shape;

TEST(RegionTests, ShapeEmptyIsRepresentable)
{
    EXPECT_TRUE(Shape::isValidShape({ }, { }));
    Shape s1 = Shape::createForTesting({ }, { });
    EXPECT_TRUE(s1.isEmpty());
    EXPECT_EQ(Shape { }, s1);
    EXPECT_EQ(Shape { IntRect { } }, s1);
    Region r1 = Region::createForTesting(Shape { s1 });
    EXPECT_TRUE(r1.isEmpty());
    EXPECT_EQ(s1, r1.dataForTesting());
}

TEST(RegionTests, ShapeEmptyIsEmpty)
{
    EXPECT_TRUE(Shape().isEmpty());
    EXPECT_TRUE(Shape(IntRect { }).isEmpty());
}

TEST(RegionTests, IsValidShapeFalse)
{
    EXPECT_FALSE(Region::Shape::isValidShape({ }, Vector<Region::Span> { { 0, 0 } }));
    EXPECT_FALSE(Region::Shape::isValidShape(Vector<int> { 1 }, Vector<Region::Span> { { 0, 1 } }));
    EXPECT_FALSE(Region::Shape::isValidShape(Vector<int> { 0, 1 }, { }));
    EXPECT_FALSE(Region::Shape::isValidShape(Vector<int> { 0, 1 }, Vector<Region::Span> { { 0, 2 } }));
    EXPECT_FALSE(Region::Shape::isValidShape(Vector<int> { 0, 28, 8, 10, 31, 20, 5, 5607747, 11, 639, 23, 25, 20, 9 },
        Vector<Region::Span> { { 703, 12 }, { 2463700, 2 } }));
}

TEST(RegionTests, UniteTests1)
{
    Region r1;
    r1.unite(IntRect { 50, 40, 50, 40 });
    r1.unite(IntRect { 5, 5, 45, 35 });
    EXPECT_EQ("(rect (5,5) width=45 height=35) (rect (50,40) width=50 height=40)"_s, convertToTrimmedString(r1));

    Region r2;
    r2.unite(IntRect { 5, 40, 45, 40 });
    r2.unite(IntRect { 50, 5, 50, 35 });
    EXPECT_EQ("(rect (50,5) width=50 height=35) (rect (5,40) width=45 height=40)"_s, convertToTrimmedString(r2));

    Region r3;
    r3.unite(r1);
    r3.unite(r2);
    Region r4 { IntRect { 5, 5, 95, 75 } };
    EXPECT_TRUE(r4.contains(r3));
    EXPECT_TRUE(r3.contains(r4));
    EXPECT_EQ(r4, r3);
    EXPECT_TRUE(r4.isRect());
    EXPECT_TRUE(r3.isRect());
}

// Describes how the algorithm stores an individual rectangle.
TEST(RegionTests, ShapeFormatIndividual)
{
    Region r1 { IntRect { 0, 0, 10, 10 } };
    EXPECT_EQ("y: 0 spans: (0, 10) y: 10 spans: () spans: (y: 0 si: 0, y: 10 si: 2) segments: (0, 10)"_s, convertToTrimmedString(r1.dataForTesting()));
}

// Describes how the algorithm stores disjoint rectangles.
// The rect is marked as ended with the span that has y: as the end corner and segmentIndex the same as the previous span.
// I.e the "y: 50 si: 2" part.
TEST(RegionTests, ShapeFormatDisjoint)
{
    Region r1 { IntRect { 0, 0, 10, 10 } };
    r1.unite(IntRect { 50, 50, 60, 60 });
    EXPECT_EQ("y: 0 spans: (0, 10) y: 10 spans: () y: 50 spans: (50, 110) y: 110 spans: () spans: (y: 0 si: 0, y: 10 si: 2, y: 50 si: 2, y: 110 si: 4) segments: (0, 10, 50, 110)"_s, convertToTrimmedString(r1.dataForTesting()));
}

// Describes how the algorithm stores x-joint mergeable rectangles.
// It merges them and produces single rect.
TEST(RegionTests, ShapeFormatTestJointXMergeable)
{
    Region r1 { IntRect { 0, 0, 10, 10 } };
    r1.unite(IntRect { 10, 0, 10, 10 });
    EXPECT_EQ("y: 0 spans: (0, 20) y: 10 spans: () spans: (y: 0 si: 0, y: 10 si: 2) segments: (0, 20)"_s, convertToTrimmedString(r1.dataForTesting()));
}

// Describes how the algorithm stores mergeable y-joint rectangles.
// It merges them and produces a single rect.
TEST(RegionTests, ShapeFormatTestJointYMergeable)
{
    Region r1 { IntRect { 0, 0, 10, 10 } };
    r1.unite(IntRect { 0, 10, 10, 10 });
    EXPECT_EQ("y: 0 spans: (0, 10) y: 20 spans: () spans: (y: 0 si: 0, y: 20 si: 2) segments: (0, 10)"_s, convertToTrimmedString(r1.dataForTesting()));
}

// Describes how the algorithm stores x-joint rectangles.
// It merges the horizontal parts and produces new rect for the leftover vertical part.
TEST(RegionTests, ShapeFormatTestJointX)
{
    Region r1 { IntRect { 0, 0, 10, 10 } };
    r1.unite(IntRect { 10, 0, 10, 11 });
    EXPECT_EQ("y: 0 spans: (0, 20) y: 10 spans: (10, 20) y: 11 spans: () spans: (y: 0 si: 0, y: 10 si: 2, y: 11 si: 4) segments: (0, 20, 10, 20)"_s, convertToTrimmedString(r1.dataForTesting()));
}

// Describes how the algorithm stores y-joint rectangles.
// It does not merge anything.
TEST(RegionTests, ShapeFormatTestJointY)
{
    Region r1 { IntRect { 0, 0, 10, 10 } };
    r1.unite(IntRect { 0, 10, 11, 10 });
    EXPECT_EQ("y: 0 spans: (0, 10) y: 10 spans: (0, 11) y: 20 spans: () spans: (y: 0 si: 0, y: 10 si: 2, y: 20 si: 4) segments: (0, 10, 0, 11)"_s, convertToTrimmedString(r1.dataForTesting()));
}

// Describes how the algorithm always produces even number of segments.
// Other algorithm could share the segments, but not this one.
TEST(RegionTests, ShapeFormatTestEvenSegments)
{
    Region r1 { IntRect { 0, 0, 10, 10 } };
    r1.unite(IntRect { 10, 10, 10, 10 });
    EXPECT_EQ("(rect (0,0) width=10 height=10) (rect (10,10) width=10 height=10)"_s, convertToTrimmedString(r1));
    EXPECT_EQ("y: 0 spans: (0, 10) y: 10 spans: (10, 20) y: 20 spans: () spans: (y: 0 si: 0, y: 10 si: 2, y: 20 si: 4) segments: (0, 10, 10, 20)"_s, convertToTrimmedString(r1.dataForTesting()));
}

// Describes how the algorithm always produces sorted segment array per Span.
// The whole segment array is not sorted, only per Span regions of it.
TEST(RegionTests, ShapeFormatTestSortedSpan)
{
    Region r1 { IntRect { 10, 0, 10, 10 } };
    r1.unite(IntRect { 1, 1, 5, 5 });
    EXPECT_EQ("(rect (10,0) width=10 height=1) (rect (1,1) width=5 height=5) (rect (10,1) width=10 height=5) (rect (10,6) width=10 height=4)"_s, convertToTrimmedString(r1));
    EXPECT_EQ("y: 0 spans: (10, 20) y: 1 spans: (1, 6, 10, 20) y: 6 spans: (10, 20) y: 10 spans: () spans: (y: 0 si: 0, y: 1 si: 2, y: 6 si: 6, y: 10 si: 8) segments: (10, 20, 1, 6, 10, 20, 10, 20)"_s, convertToTrimmedString(r1.dataForTesting()));
}

static IntRect randomRect(std::minstd_rand& rand)
{
    std::uniform_int_distribution<> coord(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    int x0 = coord(rand);
    int y0 = coord(rand);
    int x1 = coord(rand);
    int y1 = coord(rand);
    if (x0 > x1)
        std::swap(x0, x1);
    if (y0 > y1)
        std::swap(y0, y1);
    return { x0, y0, x1 - x0, y1 - y0 };
}

// Tests that Region operations never produce segment-span lists that fail isValidShape.
TEST(RegionTests, FuzzOperationsIsValidShape)
{
    constexpr int iterations = 5000;
    constexpr bool printPassed = false;
    std::random_device rd;
    std::minstd_rand rand(rd());
    std::uniform_int_distribution<> operationCount(0, 15);
    std::uniform_int_distribution<> operation(0, 2);

    for (int i = 0; i < iterations; ++i) {
        Region r;
        StringBuilder commands;
        commands.append("Region r;\n"_s);
        const int operations = operationCount(rand);
        for (int i = 0; i < operations; ++i) {
            auto rect = randomRect(rand);
            ASCIILiteral command;
            switch (operation(rand)) {
            case 0:
                r.unite(rect);
                command = "unite"_s;
                break;
            case 1:
                r.intersect(rect);
                command = "intersect"_s;
                break;
            case 2:
                r.subtract(rect);
                command = "subtract"_s;
                break;
            }
            commands.append("r."_s, command, "(IntRect { "_s, rect.x(), ", "_s, rect.y(), ", "_s, rect.width(), ", "_s, rect.height(), " });\n"_s);
            (void) r.rects(); // No crash.
            auto [segments, spans] = r.dataForTesting().dataForTesting();
            if (!Shape::isValidShape(segments.span(), spans.span()))
                ASSERT_TRUE(Shape::isValidShape(segments.span(), spans.span())) << commands.toString() << r.dataForTesting();
        }
        if (printPassed) {
            WTFLogAlways("%s", commands.toString().utf8().data());
            WTFLogAlways("Shape: %s", convertToString(r.dataForTesting()).utf8().data());
        }
    }
}

TEST(RegionTests, IsValidShape1)
{
    Region r;
    r.unite(IntRect { 620280709, 86198313, 951242283, 733368814 });
    r.subtract(IntRect { 416621960, 440858151, 1275303923, 1424992047 });
    r.subtract(IntRect { 1329326038, 360395968, 435226361, 1588476209 });
    auto [segments, spans] = r.dataForTesting().dataForTesting();
    ASSERT_TRUE(Shape::isValidShape(segments.span(), spans.span())) << r.dataForTesting();
}

TEST(RegionTests, IsValidShape2)
{
    Region r;
    r.subtract(IntRect { 384928833, 12330228, 959619016, 578249146 });
    r.intersect(IntRect { 828730499, 921697549, 1295834393, 903531184 });
    r.intersect(IntRect { 290731592, 195208138, 497331448, 106244923 });
    r.unite(IntRect { 587928172, 216906104, 1428192965, 1718162329 });
    r.subtract(IntRect { 1745551117, 534538086, 297055811, 1154752629 });
    auto [segments, spans] = r.dataForTesting().dataForTesting();
    ASSERT_TRUE(Shape::isValidShape(segments.span(), spans.span())) << r.dataForTesting();
}

}
