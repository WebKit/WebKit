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
#import "WKTextEffectManager.h"

#if ENABLE(WRITING_TOOLS_TEXT_EFFECTS)

#import "ImageOptions.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/NativeImage.h>
#import <WebCore/TextAnimationTypes.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/WritingDirection.h>
#import <pal/spi/cocoa/WritingToolsUISPI.h>
#import <wtf/WeakObjCPtr.h>

#import <pal/cocoa/WritingToolsUISoftLink.h>


static constexpr WTTextEffectManagerWritingDirection toTextEffectWritingDirection(WebCore::WritingDirection editorWritingDirection)
{
    switch (editorWritingDirection) {
    case WebCore::WritingDirection::Natural:
        return WTTextEffectManagerWritingDirectionLeftToRight;
    case WebCore::WritingDirection::LeftToRight:
        return WTTextEffectManagerWritingDirectionLeftToRight;
    case WebCore::WritingDirection::RightToLeft:
        return WTTextEffectManagerWritingDirectionRightToLeft;
    default:
        ASSERT_NOT_REACHED();
        return WTTextEffectManagerWritingDirectionLeftToRight;
    }
}

@interface WKTextEffectManager () <_WTTextEffectManagerDelegate>
@end

@implementation WKTextEffectManager {
    WeakObjCPtr<WKWebView> _webView;
    RetainPtr<_WTTextEffectManager> _textEffectManager;
}

- (instancetype)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    if (WKTextEffectManager.canUse)
        _textEffectManager = adoptNS([PAL::alloc_WTTextEffectManagerInstance() initWithDelegate:self]);

    return self;
}

- (void)addTextEffectForID:(NSUUID *)uuid withData:(const WebCore::TextEffectData&)data
{
    if (!WKTextEffectManager.canUse)
        return;

    [_textEffectManager startAnimationForSuggestionWithUUID:uuid writingDirection:toTextEffectWritingDirection(data.writingDirection) effectType:WTTextEffectManagerEffectTypeDefault completion:^(NSUUID *uuid) { }];
}

- (void)removeTextEffectForID:(NSUUID *)uuid
{
    if (!WKTextEffectManager.canUse)
        return;

    [_textEffectManager cancelAnimationForSuggestionWithUUID:uuid];
}

- (void)removeAllTextEffects
{
    if (!WKTextEffectManager.canUse)
        return;

    [_textEffectManager cancelAllAnimations];
}

#pragma mark _WTTextEffectManagerDelegate
- (void)hideTextForSuggestionWithUUID:(NSUUID *)uuid completion:(void(^)(void))completionHandler
{
    RetainPtr webView = _webView.get();
    if (!webView)
        return completionHandler();

    auto textEffectUUID = WTF::UUID::fromNSUUID(uuid);
    if (!textEffectUUID)
        return completionHandler();

    [webView _page]->updateUnderlyingTextVisibilityForTextEffectID(*textEffectUUID, false, [completionHandler = makeBlockPtr(completionHandler)] {
        if (completionHandler)
            completionHandler();
    });
}
- (void)showTextForSuggestionWithUUID:(NSUUID *)uuid completion:(void(^)(void))completionHandler
{
    RetainPtr webView = _webView.get();
    if (!webView)
        return completionHandler();

    auto textEffectUUID = WTF::UUID::fromNSUUID(uuid);
    if (!textEffectUUID)
        return completionHandler();

    [webView _page]->updateUnderlyingTextVisibilityForTextEffectID(*textEffectUUID, true, [completionHandler = makeBlockPtr(completionHandler)] {
        if (completionHandler)
            completionHandler();
    });
}

- (void)containerViewForSuggestionWithUUID:(NSUUID *)uuid completion:(void(^)(CocoaView *containerView))completionHandler
{
    completionHandler(_webView.get());
}

static RetainPtr<NSArray<_WTTextPreview *>> textPreviewsFromIndicator(const RefPtr<WebCore::TextIndicator>& textIndicator, CocoaView *rootView, CocoaView *containerView)
{
    if (!textIndicator)
        return nil;

    RefPtr snapshot = textIndicator->contentImage();
    if (!snapshot)
        return nil;

    RefPtr snapshotImage = snapshot->nativeImage();
    if (!snapshotImage)
        return nil;

    RetainPtr previews = adoptNS([[NSMutableArray alloc] initWithCapacity:textIndicator->textRectsInBoundingRectCoordinates().size()]);
    RetainPtr snapshotPlatformImage = snapshotImage->platformImage();

    if (!snapshotPlatformImage)
        return nil;

    CGRect boundingRectInRootViewCoordinates = textIndicator->textBoundingRectInRootViewCoordinates();

    for (auto textRectInSnapshotCoordinates : textIndicator->textRectsInBoundingRectCoordinates()) {
        CGRect frameInRootViewCoordinates = CGRectOffset(textRectInSnapshotCoordinates, boundingRectInRootViewCoordinates.origin.x, boundingRectInRootViewCoordinates.origin.y);
        CGRect presentationFrame = [rootView convertRect:frameInRootViewCoordinates toView:containerView];
        textRectInSnapshotCoordinates.scale(textIndicator->contentImageScaleFactor());
        [previews addObject:adoptNS([PAL::alloc_WTTextPreviewInstance() initWithSnapshotImage:adoptCF(CGImageCreateWithImageInRect(snapshotPlatformImage.get(), textRectInSnapshotCoordinates)).get() presentationFrame:presentationFrame]).get()];
    }

    return previews;
}

- (void)previewsForSuggestionWithUUID:(NSUUID *)uuid completion:(void (^)(NSArray<_WTTextPreview *> * _Nullable textPreviews, NSArray<_WTTextPreview *> * _Nullable underlinePreviews))completionHandler
{
    RetainPtr webView = _webView.get();
    if (!webView)
        return completionHandler(nil, nil);

    auto textEffectUUID = WTF::UUID::fromNSUUID(uuid);
    if (!textEffectUUID)
        return completionHandler(nil, nil);

    [webView _page]->textIndicatorForTextEffectID(*textEffectUUID, [protectedSelf = retainPtr(self), textEffectUUID = *textEffectUUID, completionHandler = makeBlockPtr(completionHandler)](RefPtr<WebCore::TextIndicator> textIndicator) {
        RetainPtr webView = protectedSelf->_webView.get();
        if (!webView) {
            completionHandler(nil, nil);
            return;
        }

#if PLATFORM(IOS_FAMILY)
        RetainPtr rootView = webView->_contentView.get();
#else
        RetainPtr rootView = webView.get();
#endif
        RetainPtr textPreviews = textPreviewsFromIndicator(textIndicator, rootView, webView);
        if (!textPreviews) {
            completionHandler(nil, nil);
            return;
        }

        [webView _page]->decorationIndicatorForTextEffectID(textEffectUUID, [textPreviews = WTF::move(textPreviews), rootView, webView, completionHandler](RefPtr<WebCore::TextIndicator> decorationIndicator) {
            auto underlinePreviews = textPreviewsFromIndicator(decorationIndicator, rootView.get(), webView.get());
            completionHandler(textPreviews.get(), underlinePreviews.get());
        });
    });
}

@end

#endif // ENABLE(WRITING_TOOLS_TEXT_EFFECT)

