/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(PERIODIC_MEMORY_MONITOR)

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreDelegate.h>
#import <wtf/HexNumber.h>
#import <wtf/Vector.h>

static constexpr auto memoryFootprintBytes = R"HTML(
<script>
function touchBytes(len)
{
    const array = new Uint8Array(len);
    for (let i = 0; i < len; i += 4096)
        array[i] = Math.random() * 256;
    return array.reduce((accumulator, element) => accumulator + element, 0);
}
</script>
)HTML"_s;

@interface MemoryFootprintDelegate : NSObject<_WKWebsiteDataStoreDelegate> {
@package
    bool _done;
    NSInteger _expectedCallbackCount;
    Vector<size_t> _footprints;
}
@end

@implementation MemoryFootprintDelegate

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore domain:(NSString *)registrableDomain didExceedMemoryFootprintThreshold:(size_t)footprint withPageCount:(NSUInteger)pageCount processLifetime:(NSTimeInterval)processLifetime inForeground:(BOOL)inForeground wasPrivateRelayed:(BOOL)wasPrivateRelayed canSuspend:(BOOL)canSuspend
{
    _footprints.append(footprint);

    if (_expectedCallbackCount-- == 1)
        _done = YES;
}

@end
// FIXME when rdar://problem/154211896 is resolved.
#if !defined(NDEBUG)
TEST(MemoryFootprintThreshold, DISABLED_TestDelegateMethod)
#else
TEST(MemoryFootprintThreshold, TestDelegateMethod)
#endif
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { memoryFootprintBytes } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    RetainPtr delegate = adoptNS([MemoryFootprintDelegate new]);
    configuration.get().websiteDataStore._delegate = delegate.get();

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setMemoryFootprintPollIntervalForTesting:0.25];
    [processPoolConfiguration setMemoryFootprintNotificationThresholds:@[@(25 << 20), @(50 << 20), @(100 << 20)]];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() processPoolConfiguration:processPoolConfiguration.get()]);
    [webView synchronouslyLoadRequest:server.request("/"_s)];

    delegate->_expectedCallbackCount = 3;
    [webView evaluateJavaScript:@"touchBytes(100 << 20)" completionHandler:nil];
    TestWebKitAPI::Util::run(&delegate->_done);

    EXPECT_EQ(delegate->_footprints.size(), 3u);
    EXPECT_EQ(delegate->_footprints[0], 25u << 20);
    EXPECT_EQ(delegate->_footprints[1], 50u << 20);
    EXPECT_EQ(delegate->_footprints[2], 100u << 20);
}

#endif // ENABLE(PERIODIC_MEMORY_MONITOR)
