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

#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import <WebCore/DataListSuggestionInformation.h>

namespace TestWebKitAPI {

TEST(DataListTests, CoalesceSuggestionUpdatesWhenAddingManyOptions)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@""
        "<input list='suggestions'>"
        "<datalist id='suggestions'></datalist>"
        "<script>"
        "function addOptions(count) {"
        "    const datalist = document.getElementById('suggestions');"
        "    for (let i = 0; i < count; ++i) {"
        "        const option = document.createElement('option');"
        "        option.value = `Option ${datalist.children.length}`;"
        "        datalist.appendChild(option);"
        "    }"
        "    return datalist.children.length;"
        "}"
        "</script>"];

    // Adding the first option to the focused input's empty <datalist> presents the suggestions dropdown.
    [webView objectByEvaluatingJavaScript:@"document.querySelector('input').focus()"];
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"addOptions(1)"] intValue], 1);

    EXPECT_TRUE(Util::waitFor(^{
        return [[webView window] childWindows].count == 1;
    }));

    __block unsigned updateCount = 0;
    __block size_t lastSuggestionCount = 0;
    InstanceMethodSwizzler updateSwizzler {
        NSClassFromString(@"WKDataListSuggestionsController"),
        NSSelectorFromString(@"updateWithInformation:"),
        imp_implementationWithBlock(^(id, WebCore::DataListSuggestionInformation& information) {
            ++updateCount;
            lastSuggestionCount = information.suggestions.size();
        })
    };

    // Each insertion notifies the input that its <datalist> may have changed; all of these
    // notifications should be coalesced into a single update of the suggestions dropdown.
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"addOptions(100)"] intValue], 101);

    EXPECT_TRUE(Util::waitFor(^{
        return lastSuggestionCount == 101;
    }));
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(updateCount, 1u);
    EXPECT_EQ(lastSuggestionCount, 101u);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
