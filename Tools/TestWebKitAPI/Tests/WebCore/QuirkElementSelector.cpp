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
#include "TestPageHarness.h"

#include <WebCore/Element.h>
#include <WebCore/NodeInlines.h>
#include <WebCore/QuirkTable.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(QuirkElementSelectorTest, MatchesFromTheNearestElementOfATextNode)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<!DOCTYPE html><div id='slider' role='slider'>label</div>"_s);

    RefPtr text = testPage.getElementById("slider"_s)->firstChild();
    ASSERT_TRUE(text && text->isTextNode());

    EXPECT_TRUE(quirkSelectorMatches("[role=slider], [role=slider] *"_s, text.get()));
}

TEST(QuirkElementSelectorTest, NullNodeNeverMatches)
{
    EXPECT_FALSE(quirkSelectorMatches("[role=slider]"_s, nullptr));
    EXPECT_FALSE(quirkSelectorMatches("[role=slider], [role=slider] *"_s, nullptr));
}

TEST(QuirkElementSelectorTest, EveryTableSelectorParses)
{
    auto testPage = TestPageHarness::create();

    for (auto& quirk : quirkTableForTesting()) {
        for (auto& condition : quirk.behaviors.conditions())
            EXPECT_TRUE(quirkSelectorParsesForTesting(condition.selector, testPage.document())) << condition.selector.characters();
    }
}

} // namespace TestWebKitAPI
