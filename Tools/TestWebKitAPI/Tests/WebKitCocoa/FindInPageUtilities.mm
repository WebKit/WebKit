/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "FindInPageUtilities.h"
#import "Utilities.h"
#import <WebKit/WKWebViewPrivate.h>

#import "InstanceMethodSwizzler.h"

#if PLATFORM(IOS_FAMILY)
#import "UIKITSPIForTesting.h"
#endif

#if HAVE(UIFINDINTERACTION)

static BOOL swizzledIsEmbeddedScreen(id, SEL, UIScreen *)
{
    return NO;
}

@implementation TestSearchAggregator {
    RetainPtr<NSMutableOrderedSet<UITextRange *>> _foundRanges;
    BlockPtr<void()> _completionHandler;
}

- (instancetype)initWithCompletionHandler:(dispatch_block_t)completionHandler
{
    if (!(self = [super init]))
        return nil;

    _foundRanges = adoptNS([[NSMutableOrderedSet alloc] init]);
    _completionHandler = makeBlockPtr(completionHandler);

    return self;
}

- (void)foundRange:(UITextRange *)range forSearchString:(NSString *)string inDocument:(UITextSearchDocumentIdentifier)document
{
    if (!string.length)
        return;

    [_foundRanges addObject:range];
}

- (void)finishedSearching
{
    if (_completionHandler)
        _completionHandler();
}

- (NSOrderedSet<UITextRange *> *)allFoundRanges
{
    return _foundRanges.get();
}

- (void)invalidateFoundRange:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document
{
    [_foundRanges removeObject:range];
}

- (void)invalidate
{
    [_foundRanges removeAllObjects];
}

- (NSUInteger)count
{
    return [_foundRanges count];
}

@end

@interface TestScrollViewDelegate ()
{
    std::unique_ptr<InstanceMethodSwizzler> _isEmbeddedScreenSwizzler;
}
@end

@implementation TestScrollViewDelegate

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _finishedScrolling = false;

    // Force UIKit to use a `CADisplayLink` rather than its own update cycle for `UIAnimation`s.
    // UIKit's own update cycle does not work in TestWebKitAPIApp, as it is started in
    // UIApplicationMain(), and TestWebKitAPIApp is not a real UIApplication. Without this,
    // scroll view animations would not be completed.
    _isEmbeddedScreenSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        UIScreen.class,
        @selector(_isEmbeddedScreen),
        reinterpret_cast<IMP>(swizzledIsEmbeddedScreen)
    );

    return self;
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
    _finishedScrolling = true;
}

@end

@implementation TestFindDelegate {
    BlockPtr<void()> _didAddLayerForFindOverlayHandler;
    BlockPtr<void()> _didRemoveLayerForFindOverlayHandler;
}

- (void)setDidAddLayerForFindOverlayHandler:(void (^)(void))didAddLayerForFindOverlayHandler
{
    _didAddLayerForFindOverlayHandler = makeBlockPtr(didAddLayerForFindOverlayHandler);
}

- (void (^)(void))didAddLayerForFindOverlayHandler
{
    return _didAddLayerForFindOverlayHandler.get();
}

- (void)setDidRemoveLayerForFindOverlayHandler:(void (^)(void))didRemoveLayerForFindOverlayHandler
{
    _didRemoveLayerForFindOverlayHandler = makeBlockPtr(didRemoveLayerForFindOverlayHandler);
}

- (void (^)(void))didRemoveLayerForFindOverlayHandler
{
    return _didRemoveLayerForFindOverlayHandler.get();
}

- (void)_webView:(WKWebView *)webView didAddLayerForFindOverlay:(CALayer *)layer
{
    if (_didAddLayerForFindOverlayHandler)
        _didAddLayerForFindOverlayHandler();
}

- (void)_webViewDidRemoveLayerForFindOverlay:(WKWebView *)webView
{
    if (_didRemoveLayerForFindOverlayHandler)
        _didRemoveLayerForFindOverlayHandler();
}

@end

@interface WKWebView () <UITextSearching>
@end

void testPerformTextSearchWithQueryStringInWebView(WKWebView *webView, NSString *query, UITextSearchOptions *searchOptions, NSUInteger expectedMatches)
{
    __block bool finishedSearching = false;
    RetainPtr aggregator = adoptNS([[TestSearchAggregator alloc] initWithCompletionHandler:^{
        finishedSearching = true;
    }]);

    [webView performTextSearchWithQueryString:query usingOptions:searchOptions resultAggregator:aggregator.get()];

    TestWebKitAPI::Util::run(&finishedSearching);

    EXPECT_EQ([aggregator count], expectedMatches);
}

RetainPtr<NSOrderedSet<UITextRange *>> textRangesForQueryString(WKWebView *webView, NSString *query)
{
    __block bool finishedSearching = false;
    auto aggregator = adoptNS([[TestSearchAggregator alloc] initWithCompletionHandler:^{
        finishedSearching = true;
    }]);

    RetainPtr options = adoptNS([[UITextSearchOptions alloc] init]);
    [webView performTextSearchWithQueryString:query usingOptions:options.get() resultAggregator:aggregator.get()];

    TestWebKitAPI::Util::run(&finishedSearching);

    return adoptNS([[aggregator allFoundRanges] copy]);
}

#endif
