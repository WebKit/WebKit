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

// Only runs on Mac because RunningBoard management is on by default on all other platforms. Only
// runs with internal SDK since that enables WK_USE_RESTRICTED_ENTITLEMENTS, which provides
// WebContent with the necessary entitlements to interface with RunningBoard.
#if PLATFORM(MAC) && USE(APPLE_INTERNAL_SDK)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <sys/resource.h>
#import <sys/resource_private.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(RunningBoardManagement, NonVisibleWebContentGoesIntoBackground)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300) configuration:adoptNS([[WKWebViewConfiguration alloc] init]).get() addToWindow:NO]);
    EXPECT_FALSE([[webView window] isVisible]);

    [webView synchronouslyLoadHTMLString:@"<body>hello world</body>"];

    pid_t pid = [webView _webProcessIdentifier];
    ASSERT_NE(pid, 0);

    bool success = Util::waitFor([pid] {
        int role = getpriority(PRIO_DARWIN_ROLE, pid);
        return role == PRIO_DARWIN_ROLE_DARWIN_BG;
    });
    EXPECT_TRUE(success) << "WebContent pid " << pid << " never got the PRIO_DARWIN_BG role even though it's non-visible. Check to make sure WebContent is properly managed by RunningBoard.";
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && USE(APPLE_INTERNAL_SDK)
