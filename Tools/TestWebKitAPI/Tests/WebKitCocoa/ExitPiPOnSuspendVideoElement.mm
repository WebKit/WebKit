/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

// We can enable the test for old iOS versions after <rdar://problem/63572534> is fixed.
#if ENABLE(VIDEO_PRESENTATION_MODE) && (PLATFORM(MAC) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000))

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebCore/PictureInPictureSupport.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>

static bool didEnterPiP;
static bool didExitPiP;

@interface ExitPiPOnSuspendVideoElementUIDelegate : NSObject <WKUIDelegate>
@end

@implementation ExitPiPOnSuspendVideoElementUIDelegate

- (void)_webView:(WKWebView *)webView hasVideoInPictureInPictureDidChange:(BOOL)value
{
    if (value)
        didEnterPiP = true;
    else
        didExitPiP = true;
}

@end

namespace TestWebKitAPI {

TEST(PictureInPicture, ExitPiPOnSuspendVideoElement)
{
    if (!WebCore::supportsPictureInPicture())
        return;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    [configuration preferences]._allowsPictureInPictureMediaPlayback = YES;
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    RetainPtr<ExitPiPOnSuspendVideoElementUIDelegate> handler = adoptNS([[ExitPiPOnSuspendVideoElementUIDelegate alloc] init]);
    [webView setUIDelegate:handler.get()];

    [webView synchronouslyLoadTestPageNamed:@"ExitFullscreenOnEnterPiP"];

    didEnterPiP = false;
    [webView evaluateJavaScript:@"document.getElementById('enter-pip').click()" completionHandler: nil];
    TestWebKitAPI::Util::run(&didEnterPiP);

    sleep(1_s);

    didExitPiP = false;
    [webView synchronouslyLoadHTMLString:@"<body>Hello world</body>"];
    TestWebKitAPI::Util::run(&didExitPiP);
}

} // namespace TestWebKitAPI

#endif
