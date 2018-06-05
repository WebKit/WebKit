/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if JSC_OBJC_API_ENABLED

#import "PlatformUtilities.h"
#import "WebCoreTestSupport.h"
#import <Carbon/Carbon.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <JavaScriptCore/JSContext.h>
#import <WebCore/Settings.h>
#import <WebKit/WebKitLegacy.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

static bool didFinishLoad = false;
static bool didBeginPlaying = false;
static bool didStopPlaying = false;
static bool didBeginRemotePlayback = false;
static bool didEndRemotePlayback = false;

@interface MediaPlaybackSleepAssertionLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation MediaPlaybackSleepAssertionLoadDelegate
- (void)webView:(WebView *)webView didCreateJavaScriptContext:(JSContext *)context forFrame:(WebFrame *)frame
{
    WebCoreTestSupport::injectInternalsObject(context.JSGlobalContextRef);
}
@end

@interface MediaPlaybackSleepAssertionPolicyDelegate : NSObject<WebPolicyDelegate>
@end

@implementation MediaPlaybackSleepAssertionPolicyDelegate
- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    NSURL* URL = request.URL;
    if ([URL.scheme isEqualToString:@"callback"]) {
        if ([URL.resourceSpecifier isEqualToString:@"loaded"])
            didFinishLoad = true;
        else if ([URL.resourceSpecifier isEqualToString:@"playing"]) {
            didBeginPlaying = true;
            didStopPlaying = false;
        } else if ([URL.resourceSpecifier isEqualToString:@"paused"]) {
            didBeginPlaying = false;
            didStopPlaying = true;
        } else if ([URL.resourceSpecifier isEqualToString:@"remote-start"]) {
            didBeginRemotePlayback = true;
            didEndRemotePlayback = false;
        } else if ([URL.resourceSpecifier isEqualToString:@"remote-end"]) {
            didBeginRemotePlayback = false;
            didEndRemotePlayback = true;
        }

        [listener ignore];
        return;
    }

    [listener use];
}
@end

namespace TestWebKitAPI {

static void simulateKeyDown(NSWindow *window)
{
    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown
        location:NSMakePoint(5, 5)
        modifierFlags:0
        timestamp:GetCurrentEventTime()
        windowNumber:window.windowNumber
        context:[NSGraphicsContext currentContext]
        characters:@" "
        charactersIgnoringModifiers:@" "
        isARepeat:NO
        keyCode:0x31];

    [window sendEvent:event];

    event = [NSEvent keyEventWithType:NSEventTypeKeyUp
        location:NSMakePoint(5, 5)
        modifierFlags:0
        timestamp:GetCurrentEventTime()
        windowNumber:window.windowNumber
        context:[NSGraphicsContext currentContext]
        characters:@" "
        charactersIgnoringModifiers:@" "
        isARepeat:NO
        keyCode:0x31];

    [window sendEvent:event];
}

static bool hasAssertionType(CFStringRef type)
{
    int32_t pid = getpid();
    auto cfPid = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pid));
    CFDictionaryRef bareAssertionsByPID = nullptr;

    IOPMCopyAssertionsByProcess(&bareAssertionsByPID);
    if (!bareAssertionsByPID)
        return false;

    auto assertionsByPID = adoptCF(bareAssertionsByPID);

    CFArrayRef assertions = (CFArrayRef)CFDictionaryGetValue(assertionsByPID.get(), cfPid.get());
    if (!assertions)
        return false;

    for (CFIndex i = 0, count = CFArrayGetCount(assertions); i < count; ++i) {
        CFDictionaryRef assertion = (CFDictionaryRef)CFArrayGetValueAtIndex(assertions, i);
        CFStringRef assertionType = (CFStringRef)CFDictionaryGetValue(assertion, kIOPMAssertionTypeKey);
        if (CFEqual(type, assertionType))
            return true;
    }
    return false;
}

TEST(WebKitLegacy, MediaPlaybackSleepAssertion)
{
    didFinishLoad = false;
    didBeginPlaying = false;
    didStopPlaying = false;
    didBeginRemotePlayback = false;
    didEndRemotePlayback = false;

    @autoreleasepool {
        auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        auto frameLoadDelegate = adoptNS([[MediaPlaybackSleepAssertionLoadDelegate alloc] init]);
        auto policyDelegate = adoptNS([[MediaPlaybackSleepAssertionPolicyDelegate alloc] init]);

        auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
        [[window contentView] addSubview:webView.get()];
        [window makeKeyAndOrderFront:nil];

        webView.get().policyDelegate = policyDelegate.get();
        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        WebFrame *mainFrame = webView.get().mainFrame;

        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"MediaPlaybackSleepAssertion" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [mainFrame loadRequest:request];

        Util::run(&didFinishLoad);
        didFinishLoad = false;

        EXPECT_FALSE(hasAssertionType(CFSTR("PreventUserIdleDisplaySleep")));
        EXPECT_FALSE(hasAssertionType(CFSTR("PreventUserIdleSystemSleep")));

        simulateKeyDown(window.get());
        Util::run(&didBeginPlaying);

        EXPECT_TRUE(hasAssertionType(CFSTR("PreventUserIdleDisplaySleep")));
        EXPECT_FALSE(hasAssertionType(CFSTR("PreventUserIdleSystemSleep")));

        simulateKeyDown(window.get());
        Util::run(&didBeginRemotePlayback);

        EXPECT_FALSE(hasAssertionType(CFSTR("PreventUserIdleDisplaySleep")));
        EXPECT_TRUE(hasAssertionType(CFSTR("PreventUserIdleSystemSleep")));

        simulateKeyDown(window.get());
        Util::run(&didStopPlaying);

        EXPECT_FALSE(hasAssertionType(CFSTR("PreventUserIdleDisplaySleep")));
        EXPECT_FALSE(hasAssertionType(CFSTR("PreventUserIdleSystemSleep")));
    }
}

}

#endif // JSC_OBJC_API_ENABLED
