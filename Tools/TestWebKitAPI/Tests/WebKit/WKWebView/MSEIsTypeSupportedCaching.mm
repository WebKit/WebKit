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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebCore/Logging.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStore.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

using namespace WebCore;

namespace TestWebKitAPI {

// All web views share a single process pool so that m_mediaSourceTypesSupported
// accumulates across processes. Each web view gets its own non-persistent data
// store, which prevents process reuse (processForSite matches on both site and
// data store), guaranteeing each web view lands in a fresh web process.
static WKProcessPool *sharedProcessPool()
{
    static NeverDestroyed<RetainPtr<WKProcessPool>> pool;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        pool.get() = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    });
    return pool.get().get();
}

static RetainPtr<TestWKWebView> createWebViewWithSeparateProcess()
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:sharedProcessPool()];
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    [[configuration preferences] _setManagedMediaSourceEnabled:YES];
    [[configuration preferences] _setMediaSourceEnabled:YES];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"is-type-supported-perf"];
    return webView;
}

static std::optional<std::pair<double, double>> runMeasurement(TestWKWebView *webView)
{
    // Bail out if ManagedMediaSource is not available.
    if (![[webView objectByEvaluatingJavaScript:@"typeof ManagedMediaSource !== 'undefined'"] boolValue])
        return std::nullopt;

    NSString *resultJSON = [webView objectByEvaluatingJavaScript:@"measurePerformance()"];
    NSData *data = [resultJSON dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *result = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];

    if (result[@"error"])
        return std::nullopt;

    return std::make_pair([result[@"coldElapsedMs"] doubleValue], [result[@"warmElapsedMs"] doubleValue]);
}

// This test measures the performance of ManagedMediaSource.isTypeSupported with MP4 codecs
// across multiple web content processes. The isTypeSupported result cache is delivered via
// WebProcessCreationParameters.mediaSourceTypesSupported (populated from the UIProcess
// WebProcessPool::m_mediaSourceTypesSupported), so only processes created *after* the
// UIProcess cache has been populated benefit. Processes that were already running when
// the first CacheMediaSourceTypeSupported IPC arrived will not receive the cache.
TEST(MediaSource, IsTypeSupportedCachingAcrossProcesses)
{
    constexpr size_t preCacheCount = 3;
    constexpr size_t postCacheCount = 3;

    // --- Create pre-cache web views upfront, all before any isTypeSupported call ---
    // All processes are launched before the UIProcess cache is populated,
    // so none of them receive mediaSourceTypesSupported via WebProcessCreationParameters.
    Vector<RetainPtr<TestWKWebView>> preCacheViews;
    for (size_t i = 0; i < preCacheCount; i++)
        preCacheViews.append(createWebViewWithSeparateProcess());

    for (size_t i = 0; i < preCacheCount; i++)
        EXPECT_NE([preCacheViews[i] _webProcessIdentifier], 0);

    // Measure all pre-cache views.
    Vector<std::pair<double, double>> preCacheResults;
    for (size_t i = 0; i < preCacheCount; i++) {
        auto result = runMeasurement(preCacheViews[i].get());
        if (!result) {
            LOG(Media, "Skipping IsTypeSupportedCachingAcrossProcesses: ManagedMediaSource not available");
            return;
        }
        LOG(Media, "Pre-cache process %zu (PID %d): cold=%.2f ms, warm=%.2f ms", i + 1, [preCacheViews[i] _webProcessIdentifier], result->first, result->second);
        preCacheResults.append(*result);
    }

    // Allow time for the CacheMediaSourceTypeSupported IPC messages to propagate back
    // to the UIProcess and populate WebProcessPool::m_mediaSourceTypesSupported.
    Util::runFor(0.5_s);

    // --- Create post-cache web views AFTER the UIProcess cache is populated ---
    // These processes receive mediaSourceTypesSupported via WebProcessCreationParameters,
    // so their MediaSourceTypeSupportedCache is pre-populated and avoids querying AVFoundation.
    Vector<std::pair<double, double>> postCacheResults;
    for (size_t i = 0; i < postCacheCount; i++) {
        auto webView = createWebViewWithSeparateProcess();
        auto result = runMeasurement(webView.get());
        ASSERT_TRUE(result.has_value());
        LOG(Media, "Post-cache process %zu (PID %d): cold=%.2f ms, warm=%.2f ms", i + 1, [webView _webProcessIdentifier], result->first, result->second);
        postCacheResults.append(*result);
    }

    // Compute averages.
    double preColdAvg = 0, postColdAvg = 0;
    for (auto& r : preCacheResults)
        preColdAvg += r.first;
    preColdAvg /= preCacheCount;

    for (auto& r : postCacheResults)
        postColdAvg += r.first;
    postColdAvg /= postCacheCount;

    LOG(Media, "=== IsTypeSupported MP4 Performance Summary ===");
    LOG(Media, "  Pre-cache avg cold:  %.2f ms (%zu processes)", preColdAvg, preCacheCount);
    LOG(Media, "  Post-cache avg cold: %.2f ms (%zu processes)", postColdAvg, postCacheCount);
    if (preColdAvg > 0 && postColdAvg > 0)
        LOG(Media, "  Cold call speedup (post-cache vs pre-cache): %.1fx", preColdAvg / postColdAvg);
}

// This test verifies that isTypeSupported returns consistent results across
// multiple web content processes for a variety of MP4 codec strings, both
// for processes created before and after the UIProcess cache is populated.
TEST(MediaSource, IsTypeSupportedConsistencyAcrossProcesses)
{
    // Wrap each JS expression with String() to ensure we always get a string back
    // from objectByEvaluatingJavaScript: (booleans would return NSNumber otherwise).
    Vector<RetainPtr<NSString>> codecStrings = {
        // MP4 / H.264
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"avc1.42E01E\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"avc1.4D401E\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"avc1.64001E\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"avc1.640029\"'))",
        // HEVC
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"hvc1.1.6.L93.B0\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"hev1.1.6.L93.B0\"'))",
        // AAC
        @"String(ManagedMediaSource.isTypeSupported('audio/mp4; codecs=\"mp4a.40.2\"'))",
        // Combined
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"avc1.42E01E,mp4a.40.2\"'))",
        // VP9
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp09.00.41.08\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp09.02.10.10\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp09.02.10.10.01.09.16.09.01\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp9\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp9,opus\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp09.01.41.08\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp09.03.41.10\"'))",
        // Invalid codec strings
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"avc1.ZZZZZZ\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/mp4; codecs=\"notacodec\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp09.04.41.08\"'))",
        @"String(ManagedMediaSource.isTypeSupported('video/webm; codecs=\"vp09.00.70.08\"'))",
    };

    // Create two web views upfront (both before UIProcess cache is populated).
    auto webView1 = createWebViewWithSeparateProcess();
    auto webView2 = createWebViewWithSeparateProcess();

    if (![[webView1 objectByEvaluatingJavaScript:@"typeof ManagedMediaSource !== 'undefined'"] boolValue]) {
        LOG(Media, "Skipping IsTypeSupportedConsistencyAcrossProcesses: ManagedMediaSource not available");
        return;
    }

    EXPECT_NE([webView1 _webProcessIdentifier], [webView2 _webProcessIdentifier]);

    // Collect results from both pre-cache processes.
    Vector<RetainPtr<NSString>> results1;
    for (auto& js : codecStrings)
        results1.append([webView1 objectByEvaluatingJavaScript:js.get()]);

    Vector<RetainPtr<NSString>> results2;
    for (auto& js : codecStrings)
        results2.append([webView2 objectByEvaluatingJavaScript:js.get()]);

    // Verify pre-cache processes are consistent with each other.
    for (size_t i = 0; i < codecStrings.size(); i++) {
        EXPECT_WK_STREQ(results1[i].get(), results2[i].get());
        if (![results1[i] isEqualToString:results2[i].get()])
            LOG(Media, "MISMATCH (pre-cache) for %s: process1=%s, process2=%s", [codecStrings[i] UTF8String], [results1[i] UTF8String], [results2[i] UTF8String]);
    }

    // Allow cache propagation to UIProcess.
    Util::runFor(0.5_s);

    // Create a third web view after cache is populated.
    auto webView3 = createWebViewWithSeparateProcess();
    EXPECT_NE([webView1 _webProcessIdentifier], [webView3 _webProcessIdentifier]);

    Vector<RetainPtr<NSString>> results3;
    for (auto& js : codecStrings)
        results3.append([webView3 objectByEvaluatingJavaScript:js.get()]);

    // Verify post-cache process is consistent with pre-cache results.
    for (size_t i = 0; i < codecStrings.size(); i++) {
        EXPECT_WK_STREQ(results1[i].get(), results3[i].get());
        if (![results1[i] isEqualToString:results3[i].get()])
            LOG(Media, "MISMATCH (post-cache) for %s: process1=%s, process3=%s", [codecStrings[i] UTF8String], [results1[i] UTF8String], [results3[i] UTF8String]);
    }
}

} // namespace TestWebKitAPI
