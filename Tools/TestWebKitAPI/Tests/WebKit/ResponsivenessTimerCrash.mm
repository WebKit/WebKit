/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if WK_HAVE_C_SPI

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPagePrivateMac.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/HashSet.h>
#import <wtf/RetainPtr.h>

static bool didBecomeUnresponsive;
static RetainPtr<TestWKWebView> webView;
static HashSet<RetainPtr<id>> observableStates;
static bool webViewSeen;

@interface ResponsivenessTimerObserver : NSObject
@end

@implementation ResponsivenessTimerObserver

-(void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (object == webView.get())
        webViewSeen = true;
    else {
        [object removeObserver:self forKeyPath:@"_webProcessIsResponsive"];
        observableStates.remove(object);
    }

    if (!observableStates.isEmpty()) {
        [observableStates.begin()->get() removeObserver:self forKeyPath:@"_webProcessIsResponsive"];
        observableStates.remove(observableStates.begin());
    }

    if (webViewSeen && observableStates.isEmpty())
        didBecomeUnresponsive = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKit, ResponsivenessTimerCrash)
{
    RetainPtr<ResponsivenessTimerObserver> observer = adoptNS([[ResponsivenessTimerObserver alloc] init]);
    @autoreleasepool {
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
        
        [webView addObserver:observer.get() forKeyPath:@"_webProcessIsResponsive" options:0 context:nullptr];

        auto pageRef = [webView _pageRefForTransitionToWKWebView];
        
        for (size_t i = 0; i < 50; ++i) {
            RetainPtr<id> observableState = adoptNS(WKPageCreateObservableState(pageRef));
            [observableState.get() addObserver:observer.get() forKeyPath:@"_webProcessIsResponsive" options:0 context:nullptr];
            observableStates.add(WTFMove(observableState));
        }

        [webView synchronouslyLoadHTMLString:@"<script>document.addEventListener('keydown', function(){while(1){}});</script>"];
        [webView typeCharacter:'a'];
    }
    
    Util::run(&didBecomeUnresponsive);

    [webView removeObserver:observer.get() forKeyPath:@"_webProcessIsResponsive"];
}

} // namespace TestWebKitAPI

#endif // WK_HAVE_C_SPI
