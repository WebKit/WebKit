/*
 * Copyright (C) 2022 Igalia S.L.
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
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/Function.h>
#import <wtf/text/WTFString.h>

class TimeZoneOverrideTest : public testing::Test {
public:
    void SetUp() override
    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        [processPoolConfig setTimeZoneOverride:@"Europe/Berlin"];

        _webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);
    }

    void runScriptAndExecuteCallback(const String& script, Function<void(id)>&& callback)
    {
        bool complete = false;
        [_webView evaluateJavaScript:script completionHandler:[&] (id result, NSError *error) {
            EXPECT_NULL(error);
            callback(result);
            complete = true;
        }];
        TestWebKitAPI::Util::run(&complete);
    }

    void callAsyncFunctionBody(const String& functionBody, Function<void(id)>&& callback)
    {
        bool complete = false;
        [_webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
            EXPECT_NULL(error);
            callback(result);
            complete = true;
        }];
        TestWebKitAPI::Util::run(&complete);
    }


private:
    RetainPtr<TestWKWebView> _webView;
};

TEST_F(TimeZoneOverrideTest, TimeZoneOverride)
{
    runScriptAndExecuteCallback("let now = new Date(1651511226050); now.getTimezoneOffset()"_s, [](id result) {
        EXPECT_WK_STREQ([result stringValue], "-120");
    });
}

TEST_F(TimeZoneOverrideTest, TimeZoneOverrideInWorkers)
{
    String functionBody =
        "return new Promise(fulfill => {"
        "  const results = [Intl.DateTimeFormat().resolvedOptions().timeZone];"
        "  for (let i = 0; i < 3; i++) {"
        "    const worker = new Worker('data:text/javascript,self.postMessage(Intl.DateTimeFormat().resolvedOptions().timeZone)');"
        "    worker.onmessage = message => {"
        "      results.push(message.data);"
        "      if (results.length === 4)"
        "        fulfill(results.join(', '));"
        "    };"
        "  }"
        "});"_s;
    callAsyncFunctionBody(functionBody, [](id result) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(result, @"Europe/Berlin, Europe/Berlin, Europe/Berlin, Europe/Berlin");
    });
}

#endif
