/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "PlatformWebView.h"

#import <WebKit/DOM.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface AdditionalSupportedImageTypesTest : NSObject <WebFrameLoadDelegate> {
}
@end

static bool didFinishLoad;

@implementation AdditionalSupportedImageTypesTest

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}
@end

namespace TestWebKitAPI {

static void runTest(NSArray *additionalSupportedImageTypes, Boolean expectedToLoad)
{
    RetainPtr<WebPreferences> preferences = adoptNS([[WebPreferences alloc] initWithIdentifier:nil]);
    [preferences setAdditionalSupportedImageTypes:additionalSupportedImageTypes];

    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    [webView setPreferences:preferences.get()];

    RetainPtr<AdditionalSupportedImageTypesTest> testController = adoptNS([AdditionalSupportedImageTypesTest new]);
    webView.get().frameLoadDelegate = testController.get();

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"AdditionalSupportedImageTypes" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:testURL.get()]];

    Util::run(&didFinishLoad);
    didFinishLoad = false;

    DOMDocument *document = webView.get().mainFrameDocument;
    DOMElement *documentElement = [document documentElement];

    DOMHTMLImageElement *image = (DOMHTMLImageElement *)[documentElement querySelector:@"img"];
    EXPECT_EQ(image != nullptr, expectedToLoad);
    if (image)
        EXPECT_EQ([image width], 100);
}

TEST(WebKitLegacy, AdditionalSupportedStringImageType)
{
    runTest(@[@"com.truevision.tga-image"], true);
}

TEST(WebKitLegacy, AdditionalBogusStringImageType)
{
    runTest(@[@"public.bogus"], false);
}

TEST(WebKitLegacy, AdditionalEmptyArrayImageType)
{
    runTest(@[], false);
}

TEST(WebKitLegacy, AdditionalArryOfNullImageType)
{
    runTest(@[[NSNull null]], false);
}

TEST(WebKitLegacy, AdditionalArrayOfArrayImageType)
{
    runTest(@[@[@"com.truevision.tga-image"]], false);
}

} // namespace TestWebKitAPI
