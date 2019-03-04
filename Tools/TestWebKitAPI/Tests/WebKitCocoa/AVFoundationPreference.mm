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

#if PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>

static bool receivedScriptMessage = false;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface PreferenceTestMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation PreferenceTestMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    lastScriptMessage = message;
    receivedScriptMessage = true;
}
@end

namespace TestWebKitAPI {

class AVFoundationPref : public testing::Test {
public:
    virtual void SetUp()
    {
        m_configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        auto handler = adoptNS([[PreferenceTestMessageHandler alloc] init]);
        [[m_configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    }

    enum class ElementType { Audio, Video };
    void testWithPreference(ElementType elementType, bool enabled)
    {
        receivedScriptMessage = false;

        auto preferences = [m_configuration preferences];
        preferences._avFoundationEnabled = enabled;
        m_configuration.get()._mediaDataLoadsAutomatically = YES;
#if TARGET_OS_IPHONE
        m_configuration.get().allowsInlineMediaPlayback = YES;
        m_configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif

        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:m_configuration.get()]);
        [webView synchronouslyLoadTestPageNamed:@"video"];

        [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"test('%s')", elementType == ElementType::Audio ? "audio" : "video"]];

        TestWebKitAPI::Util::run(&receivedScriptMessage);
        auto type = [webView stringByEvaluatingJavaScript:@"elementType()"];

        if (elementType == ElementType::Audio)
            EXPECT_STREQ(type.UTF8String, "[object HTMLAudioElement]");
        else
            EXPECT_STREQ(type.UTF8String, "[object HTMLVideoElement]");

        if (enabled)
            EXPECT_STREQ(((NSString *)[lastScriptMessage body]).UTF8String, "canplay");
        else
            EXPECT_STREQ(((NSString *)[lastScriptMessage body]).UTF8String, "error");
    }

private:
    RetainPtr<WKWebViewConfiguration> m_configuration;
};

TEST_F(AVFoundationPref, PrefTest)
{
    testWithPreference(ElementType::Audio, true);
    testWithPreference(ElementType::Audio, false);
    testWithPreference(ElementType::Video, true);
    testWithPreference(ElementType::Video, false);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
