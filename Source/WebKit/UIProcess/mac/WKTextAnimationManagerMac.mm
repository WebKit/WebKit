/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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


#if ENABLE(WRITING_TOOLS) && PLATFORM(MAC)

#import "config.h"
#import "WKTextAnimationManagerMac.h"

#import "ImageOptions.h"
#import "WKWebView.h"
#import "WebPageProxy.h"
#import "WebViewImpl.h"
#import <WebCore/NativeImage.h>
#import <WebCore/TextAnimationTypes.h>
#import <pal/cocoa/WritingToolsUISoftLink.h>

@interface WKTextAnimationTypeEffectData : NSObject
@property (nonatomic, strong, readonly) NSUUID *effectID;
@property (nonatomic, assign, readonly) WebCore::TextAnimationType type;
@end

@implementation WKTextAnimationTypeEffectData {
    RetainPtr<NSUUID> _effectID;
}

- (instancetype)initWithEffectID:(NSUUID *)effectID type:(WebCore::TextAnimationType)type
{
    if (!(self = [super init]))
        return nil;

    _effectID = effectID;
    _type = type;

    return self;
}

- (NSUUID *)effectID
{
    return _effectID.get();
}

@end

@interface WKTextAnimationManager () <_WTTextPreviewAsyncSource>
@end

@implementation WKTextAnimationManager {
    WeakPtr<WebKit::WebViewImpl> _webView;
    RetainPtr<NSMutableDictionary<NSUUID *, WKTextAnimationTypeEffectData *>> _chunkToEffect;
    RetainPtr<_WTTextEffectView> _effectView;
}

- (instancetype)initWithWebViewImpl:(WebKit::WebViewImpl&)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _chunkToEffect = adoptNS([[NSMutableDictionary alloc] init]);

    _effectView = adoptNS([PAL::alloc_WTTextEffectViewInstance() initWithAsyncSource:self]);
    [_effectView setClipsToBounds:YES];
    [_effectView setFrame:webView.view().bounds];

    return self;
}

- (void)addTextAnimationForAnimationID:(NSUUID *)uuid withData:(const WebCore::TextAnimationData&)data
{
    if (data.style == WebCore::TextAnimationType::Initial && _webView->page().writingToolsTextReplacementsFinished()) {
        // When the session has finished sending all of the partial replacements, the `TextAnimationController` may still
        // request an initial text animation for the remaining "unreplaced" range of the context range (which may or may
        // not actually end up getting replaced for a given partial replacement). However, this "unreplaced" range will
        // never end up ever getting replaced if the replacement is finished.
        return;
    }

    RetainPtr<id<_WTTextEffect>> effect;
    RetainPtr chunk = adoptNS([PAL::alloc_WTTextChunkInstance() initChunkWithIdentifier:uuid.UUIDString]);

    switch (data.style) {
    case WebCore::TextAnimationType::Initial:
        effect = adoptNS([PAL::alloc_WTSweepTextEffectInstance() initWithChunk:chunk.get() effectView:_effectView.get()]);
        break;

    case WebCore::TextAnimationType::Source:
        effect = adoptNS([PAL::alloc_WTReplaceSourceTextEffectInstance() initWithChunk:chunk.get() effectView:_effectView.get()]);

        effect.get().preCompletion = makeBlockPtr([weakWebView = WeakPtr<WebKit::WebViewImpl>(_webView), uuid = RetainPtr(uuid), runMode = data.runMode] {
            CheckedPtr webView = weakWebView.get();
            auto animationID = WTF::UUID::fromNSUUID(uuid.get());
            if (webView && animationID)
                webView->page().callCompletionHandlerForAnimationID(*animationID, runMode);
        }).get();

        effect.get().completion = makeBlockPtr([weakSelf = WeakObjCPtr<WKTextAnimationManager>(self), uuid = RetainPtr(uuid)] {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            [strongSelf removeTextAnimationForAnimationID:uuid.get()];

            if (![strongSelf hasActiveTextAnimationType])
                [strongSelf hideTextAnimationView];
        }).get();

        break;

    case WebCore::TextAnimationType::Final:
        effect = adoptNS([PAL::alloc_WTReplaceDestinationTextEffectInstance() initWithChunk:chunk.get() effectView:_effectView.get()]);

        effect.get().preCompletion = makeBlockPtr([weakWebView = WeakPtr<WebKit::WebViewImpl>(_webView), remainingID = *data.unanimatedRangeUUID] {
            if (CheckedPtr webView = weakWebView.get())
                webView->page().updateUnderlyingTextVisibilityForTextAnimationID(remainingID, false);
        }).get();

        effect.get().completion = makeBlockPtr([weakSelf = WeakObjCPtr<WKTextAnimationManager>(self), weakWebView = WeakPtr<WebKit::WebViewImpl>(_webView), remainingID = *data.unanimatedRangeUUID, uuid = RetainPtr(uuid), runMode = data.runMode, effect = WeakObjCPtr<id<_WTTextEffect>>(effect.get())] {
            if (auto strongEffect = effect.get())
                [strongEffect setCompletion:nil];

            CheckedPtr webView = weakWebView.get();
            auto animationID = WTF::UUID::fromNSUUID(uuid.get());

            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            [strongSelf removeTextAnimationForAnimationID:uuid.get()];

            if (![strongSelf hasActiveTextAnimationType])
                [strongSelf hideTextAnimationView];

            if (!webView || !animationID)
                return;

            webView->page().didEndPartialIntelligenceTextAnimationImpl();

            webView->page().updateUnderlyingTextVisibilityForTextAnimationID(remainingID, true);
            webView->page().callCompletionHandlerForAnimationID(*animationID, runMode);
        }).get();

        break;
    }

    ASSERT(effect);

    if (data.style == WebCore::TextAnimationType::Initial)
        [_effectView setFrame:_webView->view().bounds];

    if (![_effectView superview])
        [_webView->view() addSubview:_effectView.get()];

    RetainPtr effectID = [_effectView addEffect:effect.get()];
    RetainPtr effectData = adoptNS([[WKTextAnimationTypeEffectData alloc] initWithEffectID:effectID.get() type:data.style]);
    [_chunkToEffect setObject:effectData.get() forKey:uuid];
}

- (void)removeTextAnimationForAnimationID:(NSUUID *)uuid
{
    RetainPtr effectData = [_chunkToEffect objectForKey:uuid];
    if (effectData) {
        [_effectView removeEffect:[effectData effectID]];
        [_chunkToEffect removeObjectForKey:uuid];
    }
}

- (void)hideTextAnimationView
{
    [_effectView removeFromSuperview];
}

- (BOOL)hasActiveTextAnimationType
{
    return [_chunkToEffect count];
}

- (void)suppressTextAnimationType
{
    for (NSUUID *chunkID in [_chunkToEffect allKeys]) {
        RetainPtr effectData = [_chunkToEffect objectForKey:chunkID];
        [_effectView removeEffect:[effectData effectID]];

        if ([effectData type] != WebCore::TextAnimationType::Initial)
            [_chunkToEffect removeObjectForKey:chunkID];
    }
}

- (void)restoreTextAnimationType
{
    for (NSUUID *chunkID in [_chunkToEffect allKeys]) {
        RetainPtr effectData = [_chunkToEffect objectForKey:chunkID];
        if ([effectData type] == WebCore::TextAnimationType::Initial)
            [self addTextAnimationForAnimationID:chunkID withData:{ WebCore::TextAnimationType::Initial, WebCore::TextAnimationRunMode::RunAnimation }];
    }
}

#pragma mark _WTTextPreviewAsyncSource

- (void)textPreviewsForChunk:(_WTTextChunk *)chunk completion:(void (^)(NSArray <_WTTextPreview *> *previews))completionHandler
{
    RetainPtr nsUUID = adoptNS([[NSUUID alloc] initWithUUIDString:chunk.identifier]);
    auto uuid = WTF::UUID::fromNSUUID(nsUUID.get());
    if (!uuid || !uuid->isValid()) {
        completionHandler(nil);
        return;
    }

    _webView->page().getTextIndicatorForID(*uuid, [protectedSelf = retainPtr(self), completionHandler = makeBlockPtr(completionHandler)] (RefPtr<WebCore::TextIndicator> textIndicator) {
        if (!textIndicator) {
            completionHandler(nil);
            return;
        }

        RefPtr snapshot = textIndicator->contentImage();
        if (!snapshot) {
            completionHandler(nil);
            return;
        }

        RefPtr snapshotImage = snapshot->nativeImage();
        if (!snapshotImage) {
            completionHandler(nil);
            return;
        }

        RetainPtr textPreviews = adoptNS([[NSMutableArray alloc] initWithCapacity:textIndicator->textRectsInBoundingRectCoordinates().size()]);
        RetainPtr snapshotPlatformImage = snapshotImage->platformImage();
        CGRect snapshotRectInBoundingRectCoordinates = textIndicator->textBoundingRectInRootViewCoordinates();

        for (auto textRectInSnapshotCoordinates : textIndicator->textRectsInBoundingRectCoordinates()) {
            CGRect textLineFrameInBoundingRectCoordinates = CGRectOffset(textRectInSnapshotCoordinates, snapshotRectInBoundingRectCoordinates.origin.x, snapshotRectInBoundingRectCoordinates.origin.y);
            textRectInSnapshotCoordinates.scale(textIndicator->contentImageScaleFactor());
            [textPreviews addObject:adoptNS([PAL::alloc_WTTextPreviewInstance() initWithSnapshotImage:adoptCF(CGImageCreateWithImageInRect(snapshotPlatformImage.get(), textRectInSnapshotCoordinates)).get() presentationFrame:textLineFrameInBoundingRectCoordinates]).get()];
        }

        completionHandler(textPreviews.get());
    });
}

- (void)textPreviewForRect:(CGRect)rect completion:(void (^)(_WTTextPreview *preview))completionHandler
{
    CGFloat deviceScale = _webView->page().deviceScaleFactor();
    WebCore::IntSize bitmapSize(rect.size.width, rect.size.height);
    bitmapSize.scale(deviceScale, deviceScale);

    _webView->page().takeSnapshot(WebCore::IntRect(rect), bitmapSize, WebKit::SnapshotOption::Shareable, [rect, completionHandler = makeBlockPtr(completionHandler)](CGImageRef image) {
        if (!image) {
            completionHandler(nil);
            return;
        }

        RetainPtr textPreview = adoptNS([PAL::alloc_WTTextPreviewInstance() initWithSnapshotImage:image presentationFrame:rect]);
        completionHandler(textPreview.get());
    });
}

- (void)updateIsTextVisible:(BOOL)isTextVisible forChunk:(_WTTextChunk *)chunk completion:(void (^)(void))completionHandler
{
    RetainPtr nsUUID = adoptNS([[NSUUID alloc] initWithUUIDString:chunk.identifier]);
    auto uuid = WTF::UUID::fromNSUUID(nsUUID.get());
    if (!uuid || !uuid->isValid()) {
        if (completionHandler)
            completionHandler();
        return;
    }

    _webView->page().updateUnderlyingTextVisibilityForTextAnimationID(*uuid, isTextVisible, [completionHandler = makeBlockPtr(completionHandler)] () {
        if (completionHandler)
            completionHandler();
    });
}

@end

#endif // ENABLE(WRITING_TOOLS) && PLATFORM(MAC)

