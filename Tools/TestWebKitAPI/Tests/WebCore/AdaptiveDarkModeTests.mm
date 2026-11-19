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

#if ENABLE(AX_CUSTOM_COLOR_MODE)

#include <WebCore/Color.h>
#include <WebCore/DocumentView.h>
#include <WebCore/FrameSnapshotting.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#include <WebCore/SimpleRange.h>

namespace TestWebKitAPI {

using namespace WebCore;

static TestPageHarness createPageWithAdaptiveDarkMode()
{
    return TestPageHarness::create({ .configureSettings = [](auto& settings) {
        settings.setAxCustomColorModeEnabled(true);
    } });
}

static constexpr auto lightCardHTML = "<!DOCTYPE html>"
    "<body style='background-color: white; margin: 0'>"
    "<div id='card' style='background-color: white; padding: 20px'><a id='link' href='#'>A link on a light card</a></div>"
    "</body>"_s;

TEST(AdaptiveDarkMode, EstimatedBackgroundColorAppliesColorFilterToAncestors)
{
    auto page = createPageWithAdaptiveDarkMode();
    page.loadHTML(lightCardHTML);

    RefPtr link = page.getElementById("link"_s);
    ASSERT_TRUE(link);

    auto documentBackgroundColor = page.frame().view()->documentBackgroundColor();

    // The test is only meaningful if Adaptive Dark Mode actually changed the white document background.
    ASSERT_NE(documentBackgroundColor, Color::white);

    auto estimatedColor = estimatedBackgroundColorForRange(makeRangeSelectingNodeContents(*link), page.frame());

    // The card is the same authored color as the body, so the adapted estimate must match the adapted document background.
    EXPECT_NE(estimatedColor, Color::white);
    EXPECT_EQ(estimatedColor, documentBackgroundColor);
}

TEST(AdaptiveDarkMode, EstimatedBackgroundColorUnchangedWhenAdaptiveDarkModeIsOff)
{
    auto page = TestPageHarness::create();
    page.loadHTML(lightCardHTML);

    RefPtr link = page.getElementById("link"_s);
    ASSERT_TRUE(link);

    // Without Adaptive Dark Mode the document background is left as authored.
    ASSERT_EQ(page.frame().view()->documentBackgroundColor(), Color::white);

    auto estimatedColor = estimatedBackgroundColorForRange(makeRangeSelectingNodeContents(*link), page.frame());
    EXPECT_EQ(estimatedColor, Color::white);
}

} // namespace TestWebKitAPI

#endif // ENABLE(AX_CUSTOM_COLOR_MODE)
