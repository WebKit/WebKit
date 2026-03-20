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


#if ENABLE(WRITING_TOOLS_TEXT_EFFECTS)

#import "config.h"
#import "WKTextEffectManager.h"

#import "ImageOptions.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/NativeImage.h>
#import <WebCore/TextAnimationTypes.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/WritingDirection.h>
#import <wtf/WeakObjCPtr.h>

#import <pal/spi/cocoa/WritingToolsUISPI.h>

#import <pal/cocoa/WritingToolsUISoftLink.h>


static WTTextEffectManagerWritingDirection EditorWritingDirectionToTextEffectWritingDirection(WebCore::WritingDirection editorWritingDirection)
{
    switch (editorWritingDirection) {
    case WebCore::WritingDirection::Natural:
        return WTTextEffectManagerWritingDirectionLeftToRight;
    case WebCore::WritingDirection::LeftToRight:
        return WTTextEffectManagerWritingDirectionLeftToRight;
    case WebCore::WritingDirection::RightToLeft:
        return WTTextEffectManagerWritingDirectionRightToLeft;
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
    _textEffectManager = adoptNS([PAL::alloc_WTTextEffectManagerInstance() initWithDelegate:self]);

    return self;
}

- (void)addTextEffectForID:(NSUUID *)uuid withData:(const WebCore::TextEffectData&)data
{
    [_textEffectManager startAnimationForSuggestionWithUUID:uuid writingDirection:EditorWritingDirectionToTextEffectWritingDirection(data.writingDirection) effectType:WTTextEffectManagerEffectTypeDefault completion:^(NSUUID *uuid) { }];
}

- (void)removeTextEffectForID:(NSUUID *)uuid
{
    [_textEffectManager cancelAnimationForSuggestionWithUUID:uuid];
}

- (void)removeAllTextEffects
{
    [_textEffectManager cancelAllAnimations];
}

#pragma mark _WTTextEffectManagerDelegate
- (void)hideTextForSuggestionWithUUID:(NSUUID *)uuid completion:(void(^)(void))completionHandler
{
    [_webView _page]->updateUnderlyingTextVisibilityForTextEffectID(WTF::UUID::fromNSUUID(uuid).value(), false, [completionHandler = makeBlockPtr(completionHandler)] () {
        if (completionHandler)
            completionHandler();
    });
}
- (void)showTextForSuggestionWithUUID:(NSUUID *)uuid completion:(void(^)(void))completionHandler
{
    [_webView _page]->updateUnderlyingTextVisibilityForTextEffectID(WTF::UUID::fromNSUUID(uuid).value(), true, [completionHandler = makeBlockPtr(completionHandler)] () {
        if (completionHandler)
            completionHandler();
    });
}

- (void)containerViewForSuggestionWithUUID:(NSUUID *)uuid completion:(void(^)(CocoaView *containerView))completionHandler
{
    completionHandler(_webView.get());
}

static RetainPtr<NSArray<_WTTextPreview *>> textPreviewsFromIndicator(const RefPtr<WebCore::TextIndicator>& textIndicator)
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
    CGRect snapshotRectInBoundingRectCoordinates = textIndicator->textBoundingRectInRootViewCoordinates();

    for (auto textRectInSnapshotCoordinates : textIndicator->textRectsInBoundingRectCoordinates()) {
        CGRect textLineFrameInBoundingRectCoordinates = CGRectOffset(textRectInSnapshotCoordinates, snapshotRectInBoundingRectCoordinates.origin.x, snapshotRectInBoundingRectCoordinates.origin.y);
        textRectInSnapshotCoordinates.scale(textIndicator->contentImageScaleFactor());
        [previews addObject:adoptNS([PAL::alloc_WTTextPreviewInstance() initWithSnapshotImage:adoptCF(CGImageCreateWithImageInRect(snapshotPlatformImage.get(), textRectInSnapshotCoordinates)).get() presentationFrame:textLineFrameInBoundingRectCoordinates]).get()];
    }

    return previews;
}

- (void)previewsForSuggestionWithUUID:(NSUUID *)uuid completion:(void (^)(NSArray<_WTTextPreview *> * _Nullable textPreviews, NSArray<_WTTextPreview *> * _Nullable underlinePreviews))completionHandler
{
    auto textEffectID = WTF::UUID::fromNSUUID(uuid).value();

    [_webView _page]->getTextIndicatorForTextEffectID(textEffectID, [protectedSelf = retainPtr(self), textEffectID, completionHandler = makeBlockPtr(completionHandler)] (RefPtr<WebCore::TextIndicator> textIndicator) {
        auto textPreviews = textPreviewsFromIndicator(textIndicator);
        if (!textPreviews) {
            completionHandler(nil, nil);
            return;
        }

        [protectedSelf->_webView _page]->getDecorationIndicatorForTextEffectID(textEffectID, [textPreviews = WTF::move(textPreviews), completionHandler] (RefPtr<WebCore::TextIndicator> decorationIndicator) {
            auto underlinePreviews = textPreviewsFromIndicator(decorationIndicator);
            completionHandler(textPreviews.get(), underlinePreviews.get());
        });
    });
}

@end

#endif // ENABLE(WRITING_TOOLS_TEXT_EFFECT)

