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
#import "WKApplicationStateTrackingView.h"

#if PLATFORM(IOS_FAMILY)

#import "ApplicationStateTracker.h"
#import "Logging.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

@implementation WKApplicationStateTrackingView {
    WeakObjCPtr<WKWebView> _webViewToTrack;
    std::unique_ptr<WebKit::ApplicationStateTracker> _applicationStateTracker;
}

- (instancetype)initWithFrame:(CGRect)frame webView:(WKWebView *)webView
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _webViewToTrack = webView;
    return self;
}

- (void)willMoveToWindow:(UIWindow *)newWindow
{
    if (!self._contentView.window || newWindow)
        return;

    auto page = [_webViewToTrack _page];
    RELEASE_LOG(ViewState, "%p - WKApplicationStateTrackingView: View with page [%p, pageProxyID: %" PRIu64 "] was removed from a window, _lastObservedStateWasBackground: %d", self, page.get(), page ? page->identifier().toUInt64() : 0, page ? page->lastObservedStateWasBackground() : false);
    ASSERT(_applicationStateTracker);
    _applicationStateTracker = nullptr;
}

- (void)didMoveToWindow
{
    if (!self._contentView.window || _applicationStateTracker)
        return;

    auto page = [_webViewToTrack _page];
    bool lastObservedStateWasBackground = page ? page->lastObservedStateWasBackground() : false;

    _applicationStateTracker = makeUnique<WebKit::ApplicationStateTracker>(self, @selector(_applicationDidEnterBackground), @selector(_applicationDidFinishSnapshottingAfterEnteringBackground), @selector(_applicationWillEnterForeground), @selector(_willBeginSnapshotSequence), @selector(_didCompleteSnapshotSequence));
    RELEASE_LOG(ViewState, "%p - WKApplicationStateTrackingView: View with page [%p, pageProxyID: %" PRIu64 "] was added to a window, _lastObservedStateWasBackground: %d, isNowBackground: %d", self, page.get(), page ? page->identifier().toUInt64() : 0, lastObservedStateWasBackground, [self isBackground]);

    if (lastObservedStateWasBackground && ![self isBackground])
        [self _applicationWillEnterForeground];
    else if (!lastObservedStateWasBackground && [self isBackground])
        [self _applicationDidEnterBackground];
}

- (void)_applicationDidEnterBackground
{
    auto page = [_webViewToTrack _page];
    if (!page)
        return;

    page->applicationDidEnterBackground();
    page->activityStateDidChange(WebCore::ActivityState::allFlags() - WebCore::ActivityState::IsInWindow);
}

- (void)_applicationDidFinishSnapshottingAfterEnteringBackground
{
    if (auto page = [_webViewToTrack _page])
        page->applicationDidFinishSnapshottingAfterEnteringBackground();
}

- (void)_applicationWillEnterForeground
{
    auto page = [_webViewToTrack _page];
    if (!page)
        return;

    page->applicationWillEnterForeground();
    page->activityStateDidChange(WebCore::ActivityState::allFlags() - WebCore::ActivityState::IsInWindow, WebKit::WebPageProxy::ActivityStateChangeDispatchMode::Immediate, WebKit::WebPageProxy::ActivityStateChangeReplyMode::Synchronous);
}

- (void)_willBeginSnapshotSequence
{
    auto page = [_webViewToTrack _page];
    if (!page)
        return;

    page->setIsTakingSnapshotsForApplicationSuspension(true);
}

- (void)_didCompleteSnapshotSequence
{
    auto page = [_webViewToTrack _page];
    if (!page)
        return;

    page->setIsTakingSnapshotsForApplicationSuspension(false);
}

- (BOOL)isBackground
{
    return _applicationStateTracker ? _applicationStateTracker->isInBackground() : YES;
}

- (UIView *)_contentView
{
    return self;
}

@end

#endif // PLATFORM(IOS_FAMILY)
