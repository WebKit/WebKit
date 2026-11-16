/*
 * Copyright (C) 2026 Microsoft Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Microsoft Corporation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(PopupMenuTests, MenuTrackingCancelledWhenPageCloses)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<select style='width: 200px; height: 200px;'><option>Alpha</option><option>Bravo</option></select>"];

    RetainPtr<NSMenu> trackingMenu;
    __block bool didEndTracking = false;
    RetainPtr beginTrackingObserver = [NSNotificationCenter.defaultCenter addObserverForName:NSMenuDidBeginTrackingNotification object:nil queue:nil usingBlock:[&trackingMenu](NSNotification *notification) {
        trackingMenu = notification.object;
    }];
    RetainPtr endTrackingObserver = [NSNotificationCenter.defaultCenter addObserverForName:NSMenuDidEndTrackingNotification object:nil queue:nil usingBlock:^(NSNotification *) {
        didEndTracking = true;
    }];

    bool didClosePage = false;
    bool menuStayedOpenAfterPageClose = false;
    RetainPtr<NSTimer> menuWatchdogTimer;
    RetainPtr closePageTimer = [NSTimer timerWithTimeInterval:0.25 repeats:YES block:[&didClosePage, &menuStayedOpenAfterPageClose, &menuWatchdogTimer, &trackingMenu, strongWebView = webView](NSTimer *timer) {
        // This timer only fires while AppKit is tracking the popup menu.
        if (!trackingMenu)
            return;

        [timer invalidate];
        [strongWebView _close];
        didClosePage = true;

        // This timer only fires if AppKit is still tracking the menu long after the page was closed.
        // Dismiss the menu ourselves so the test fails instead of hanging.
        menuWatchdogTimer = [NSTimer timerWithTimeInterval:2 repeats:NO block:[&menuStayedOpenAfterPageClose, menu = trackingMenu](NSTimer *) {
            menuStayedOpenAfterPageClose = true;
            [menu cancelTrackingWithoutAnimation];
        }];
        [NSRunLoop.mainRunLoop addTimer:menuWatchdogTimer.get() forMode:NSEventTrackingRunLoopMode];
    }];

    [NSRunLoop.mainRunLoop addTimer:closePageTimer.get() forMode:NSEventTrackingRunLoopMode];
    [[webView window] orderFrontRegardless];
    [webView sendClickAtPoint:NSMakePoint(100, 300)];
    Util::run(&didEndTracking);
    [closePageTimer invalidate];
    [menuWatchdogTimer invalidate];

    EXPECT_TRUE(didClosePage);
    EXPECT_FALSE(menuStayedOpenAfterPageClose);

    [NSNotificationCenter.defaultCenter removeObserver:beginTrackingObserver.get()];
    [NSNotificationCenter.defaultCenter removeObserver:endTrackingObserver.get()];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
