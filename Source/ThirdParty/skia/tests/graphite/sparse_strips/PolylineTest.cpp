/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/sparse_strips/Polyline.h"
#include "tests/Test.h"

#include <cmath>
#include <iterator>

namespace skgpu::graphite {

DEF_TEST(SparseStrips_Polyline, reporter) {
    // Append Deduplications
    {
        Polyline polyline;
        REPORTER_ASSERT(reporter, polyline.empty());
        REPORTER_ASSERT(reporter, polyline.count() == 0);

        polyline.appendPoint({0.0f, 0.0f});
        REPORTER_ASSERT(reporter, polyline.count() == 1);

        polyline.appendPoint({0.0f, 0.0f});
        REPORTER_ASSERT(reporter, polyline.count() == 1);

        polyline.appendPoint({1.0f, 1.0f});
        REPORTER_ASSERT(reporter, polyline.count() == 2);

        polyline.appendSentinel();
        REPORTER_ASSERT(reporter, polyline.count() == 3);
        REPORTER_ASSERT(reporter, std::isnan(polyline.points().back().fX));
        REPORTER_ASSERT(reporter, std::isnan(polyline.points().back().fY));

        polyline.appendSentinel();
        REPORTER_ASSERT(reporter, polyline.count() == 3);

        polyline.reset();
        REPORTER_ASSERT(reporter, polyline.empty());
        polyline.appendSentinel();
        REPORTER_ASSERT(reporter, polyline.empty());
    }

    // Iterator
    {
        Polyline polyline;

        polyline.appendPoint({0.0f, 0.0f});
        polyline.appendPoint({1.0f, 0.0f});
        polyline.appendPoint({1.0f, 1.0f});
        polyline.appendSentinel();

        polyline.appendPoint({5.0f, 5.0f});
        polyline.appendPoint({6.0f, 6.0f});
        polyline.appendSentinel();

        polyline.appendPoint({10.0f, 10.0f});
        polyline.appendSentinel();

        int expectedIndices[] = {0, 1, 4};
        int count = 0;

        for (auto it = polyline.begin(); it != polyline.end(); ++it) {
            int idx = (*it).second;
            REPORTER_ASSERT(reporter, count < 3);
            REPORTER_ASSERT(reporter, idx == expectedIndices[count]);
            count++;
        }

        REPORTER_ASSERT(reporter, count == 3);
    }

    // Malformed inputs
    {
        static constexpr float kNaN = SK_ScalarNaN;
        static constexpr SkPoint kPathologicalPts[] = {
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {0.0f, 0.0f},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {1.0f, 1.0f},
            {2.0f, 2.0f},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {3.0f, 3.0f},
            {kNaN, kNaN},
            {4.0f, 4.0f},
            {5.0f, 5.0f},
            {6.0f, 6.0f},
            {kNaN, kNaN},
            {7.0f, 7.0f},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN}
        };

        int count = std::size(kPathologicalPts);
        Polyline::LineIterator it(kPathologicalPts, 0, count);
        Polyline::LineIterator end(kPathologicalPts, count, count);

        REPORTER_ASSERT(reporter, it != end);
        REPORTER_ASSERT(reporter, (*it).second == 8);
        ++it;

        REPORTER_ASSERT(reporter, it != end);
        REPORTER_ASSERT(reporter, (*it).second == 18);
        ++it;

        REPORTER_ASSERT(reporter, it != end);
        REPORTER_ASSERT(reporter, (*it).second == 19);
        ++it;

        REPORTER_ASSERT(reporter, !(it != end));
    }

    // Empty/Null
    {
        Polyline polyline;

        auto it = polyline.begin();
        auto end = polyline.end();

        REPORTER_ASSERT(reporter, !(it != end));

        const SkPoint test[] = {{0.0f, 0.0f}};
        Polyline::LineIterator rawIt(test, 0, 0);
        Polyline::LineIterator rawEnd(test, 0, 0);

        REPORTER_ASSERT(reporter, !(rawIt != rawEnd));
    }

    // Single Point
    {
        const SkPoint pts[] = {{1.0f, 1.0f}};
        Polyline::LineIterator it(pts, 0, 1);
        Polyline::LineIterator end(pts, 1, 1);

        REPORTER_ASSERT(reporter, !(it != end));
    }
}

}  // namespace skgpu::graphite
