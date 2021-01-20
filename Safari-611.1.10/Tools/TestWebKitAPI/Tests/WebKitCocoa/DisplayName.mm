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

#import "config.h"

#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <WebKit/WebKit.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

#if PLATFORM(MAC)
static void checkUntilDisplayNameIs(WKWebView *webView, NSString *expectedName, bool* done, size_t iterations = 20)
{
    [webView _getProcessDisplayNameWithCompletionHandler:^(NSString *name) {
        if ([name isEqualToString:expectedName])
            *done = true;
        else if (!iterations) {
            EXPECT_FALSE(true);
            *done = true;
        } else
            checkUntilDisplayNameIs(webView, expectedName, done, iterations - 1);
    }];
}

TEST(WebKit, CustomDisplayName)
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration new] autorelease];
    NSString *displayNameToSet = @"test display name";
    configuration._processDisplayName = displayNameToSet;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView synchronouslyLoadHTMLString:@"start web process"];

    bool done = false;
    checkUntilDisplayNameIs(webView.get(), displayNameToSet, &done);
    Util::run(&done);
}

TEST(WebKit, DefaultDisplayName)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"start web process"];

    __block bool done = false;
    checkUntilDisplayNameIs(webView.get(), @"TestWebKitAPI Web Content", &done);
    Util::run(&done);
}
#endif // PLATFORM(MAC)

}
