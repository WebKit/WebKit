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

#include <WebCore/PODIntervalTree.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using namespace WebCore;

using IntTree = PODIntervalTree<int, unsigned>;
using IntInterval = IntTree::IntervalType;

// nextIntervalAfter() must return the interval with the smallest low endpoint
// that is strictly greater than the query point. Because the tree is a BST
// keyed by the low endpoint, when the query point is below a node's low, a
// still-smaller candidate can only live in that node's *left* subtree. A
// regression that recursed into the right subtree instead would overshoot and
// return a farther-following interval (or none).

TEST(PODIntervalTree, NextIntervalAfterReturnsNearestFollowing)
{
    IntTree tree;
    // Inserting 10, 20, 30 in order yields a balanced tree rooted at 20 with
    // 10 on the left and 30 on the right, so the query below is forced to
    // descend past a node whose left subtree holds the correct answer.
    tree.add(IntInterval(10, 15, 1));
    tree.add(IntInterval(20, 25, 2));
    tree.add(IntInterval(30, 35, 3));

    // Point below every interval: nearest following starts at 10, not 30.
    auto result = tree.nextIntervalAfter(5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(10, result->low());
    EXPECT_EQ(1u, result->data());

    // Point sitting on an existing low: next strictly-greater low is 20.
    result = tree.nextIntervalAfter(10);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(20, result->low());

    // Point inside the first interval: still 20.
    result = tree.nextIntervalAfter(12);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(20, result->low());

    // Point between intervals: 30.
    result = tree.nextIntervalAfter(26);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(30, result->low());

    // Point at or past the last low: nothing follows.
    EXPECT_FALSE(tree.nextIntervalAfter(30).has_value());
    EXPECT_FALSE(tree.nextIntervalAfter(100).has_value());
}

// Exhaustively cross-check nextIntervalAfter() against a brute-force scan over
// a larger, less-balanced set so the assertion holds independent of the tree's
// internal shape.
TEST(PODIntervalTree, NextIntervalAfterMatchesBruteForce)
{
    Vector<int> lows { 50, 25, 75, 12, 37, 62, 87, 6, 18, 31, 43, 55, 70, 80, 95 };

    IntTree tree;
    for (int low : lows)
        tree.add(IntInterval(low, low + 5, 0));

    for (int point = -5; point <= 100; ++point) {
        std::optional<int> expected;
        for (int low : lows) {
            if (low > point && (!expected || low < *expected))
                expected = low;
        }

        auto actual = tree.nextIntervalAfter(point);
        if (expected) {
            ASSERT_TRUE(actual.has_value()) << "point=" << point;
            EXPECT_EQ(*expected, actual->low()) << "point=" << point;
        } else
            EXPECT_FALSE(actual.has_value()) << "point=" << point;
    }
}

} // namespace TestWebKitAPI
