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

#import "config.h"

#if PLATFORM(COCOA)

#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import <WebCore/PlatformScreen.h>
#import <WebCore/ScreenProperties.h>
#import <wtf/MainThread.h>

namespace TestWebKitAPI {

TEST(PlatformScreen, CollectScreenPropertiesAsync)
{
    auto expectedProperties = WebCore::collectScreenProperties();

    bool done = false;
    std::optional<WebCore::ScreenProperties> actualProperties;
    WebCore::collectScreenPropertiesAsync([&](WebCore::ScreenProperties&& properties) {
        EXPECT_TRUE(isMainThread());
        actualProperties = WTF::move(properties);
        done = true;
    });

    EXPECT_FALSE(done);

    Util::run(&done);

    ASSERT_TRUE(actualProperties.has_value());
    EXPECT_EQ(actualProperties->primaryDisplayID, expectedProperties.primaryDisplayID);
    EXPECT_EQ(actualProperties->screenDataMap.size(), expectedProperties.screenDataMap.size());

    // Only the dynamic range state is collected differently by the two functions, so limit the
    // comparison to that. The other properties might differ (e.g. currentEDRHeadroom is affected by
    // display brightness).
    for (auto& [displayID, expectedScreenData] : expectedProperties.screenDataMap) {
        auto iterator = actualProperties->screenDataMap.find(displayID);
        ASSERT_TRUE(iterator != actualProperties->screenDataMap.end());
        EXPECT_EQ(iterator->value.screenSupportsHighDynamicRange, expectedScreenData.screenSupportsHighDynamicRange);
#if PLATFORM(MAC)
        EXPECT_EQ(iterator->value.preferredDynamicRangeMode, expectedScreenData.preferredDynamicRangeMode);
#endif
    }
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
