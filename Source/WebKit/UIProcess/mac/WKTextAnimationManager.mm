/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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


#if ENABLE(WRITING_TOOLS_UI) && PLATFORM(MAC)

#import "config.h"
#import "WKTextAnimationManager.h"

#import "ImageOptions.h"
#import "TextAnimationType.h"
#import "WKTextAnimationType.h"
#import "WebViewImpl.h"
#import <pal/cocoa/WritingToolsUISoftLink.h>

@interface WKTextAnimationTypeEffectData : NSObject
@property (nonatomic, strong, readonly) NSUUID *effectID;
@property (nonatomic, assign, readonly) WebKit::TextAnimationType type;
@end

@implementation WKTextAnimationTypeEffectData {
    RetainPtr<NSUUID> _effectID;
}

- (instancetype)initWithEffectID:(NSUUID *)effectID type:(WebKit::TextAnimationType)type
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
    [_effectView setFrame:webView.view().frame];
    [_webView->view() addSubview:_effectView.get()];
    return self;
}

- (void)addTextAnimationTypeForID:(NSUUID *)uuid withData:(const WebKit::TextAnimationData&)data
{
    RetainPtr<id<_WTTextEffect>> effect;
    RetainPtr chunk = adoptNS([PAL::alloc_WTTextChunkInstance() initChunkWithIdentifier:uuid.UUIDString]);
    switch (data.style) {
    case WebKit::TextAnimationType::Initial:
        effect = adoptNS([PAL::alloc_WTSweepTextEffectInstance() initWithChunk:chunk.get() effectView:_effectView.get()]);
        break;
    case WebKit::TextAnimationType::Source:
        effect = adoptNS([PAL::alloc_WTReplaceSourceTextEffectInstance() initWithChunk:chunk.get() effectView:_effectView.get()]);
        break;
    case WebKit::TextAnimationType::Final:
        effect = adoptNS([PAL::alloc_WTReplaceDestinationTextEffectInstance() initWithChunk:chunk.get() effectView:_effectView.get()]);
        if ([effect respondsToSelector:@selector(setPreCompletion:)] && [effect respondsToSelector:@selector(setCompletion:)]) {
            static_cast<_WTReplaceDestinationTextEffect *>(effect.get()).preCompletion = makeBlockPtr([weakWebView = WeakPtr<WebKit::WebViewImpl>(_webView), remainingID = data.remainingRangeUUID] {
                auto strongWebView = weakWebView.get();
                if (strongWebView)
                    strongWebView->page().updateUnderlyingTextVisibilityForTextAnimationID(remainingID, false);
            }).get();
            effect.get().completion = makeBlockPtr([weakWebView = WeakPtr<WebKit::WebViewImpl>(_webView), remainingID = data.remainingRangeUUID] {
                auto strongWebView = weakWebView.get();
                if (strongWebView)
                    strongWebView->page().updateUnderlyingTextVisibilityForTextAnimationID(remainingID, true);
            }).get();
        }
        break;
    }

    RetainPtr effectID = [_effectView addEffect:effect.get()];
    RetainPtr effectData = adoptNS([[WKTextAnimationTypeEffectData alloc] initWithEffectID:effectID.get() type:data.style]);
    [_chunkToEffect setObject:effectData.get() forKey:uuid];
}

- (void)removeTextAnimationForID:(NSUUID *)uuid
{
    RetainPtr effectData = [_chunkToEffect objectForKey:uuid];
    if (effectData) {
        [_effectView removeEffect:[effectData effectID]];
        [_chunkToEffect removeObjectForKey:uuid];
    }
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

        if ([effectData type] != WebKit::TextAnimationType::Initial)
            [_chunkToEffect removeObjectForKey:chunkID];
    }
}

- (void)restoreTextAnimationType
{
    for (NSUUID *chunkID in [_chunkToEffect allKeys]) {
        RetainPtr effectData = [_chunkToEffect objectForKey:chunkID];
        if ([effectData type] == WebKit::TextAnimationType::Initial)
            [self addTextAnimationTypeForID:chunkID withData: { WebKit::TextAnimationType::Initial, WTF::UUID(WTF::UUID::emptyValue) }];
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

    _webView->page().getTextIndicatorForID(*uuid, [protectedSelf = retainPtr(self), completionHandler = makeBlockPtr(completionHandler)] (std::optional<WebCore::TextIndicatorData> indicatorData) {

        if (!indicatorData) {
            completionHandler(nil);
            return;
        }

        auto snapshot = indicatorData->contentImage;
        if (!snapshot) {
            completionHandler(nil);
            return;
        }

        auto snapshotImage = snapshot->nativeImage();
        if (!snapshotImage) {
            completionHandler(nil);
            return;
        }

        RetainPtr textPreviews = adoptNS([[NSMutableArray alloc] initWithCapacity:indicatorData->textRectsInBoundingRectCoordinates.size()]);
        CGImageRef snapshotPlatformImage = snapshotImage->platformImage().get();
        CGRect snapshotRectInBoundingRectCoordinates = indicatorData->textBoundingRectInRootViewCoordinates;
        for (auto textRectInSnapshotCoordinates : indicatorData->textRectsInBoundingRectCoordinates) {
            CGRect textLineFrameInBoundingRectCoordinates = CGRectOffset(textRectInSnapshotCoordinates, snapshotRectInBoundingRectCoordinates.origin.x, snapshotRectInBoundingRectCoordinates.origin.y);
            textRectInSnapshotCoordinates.scale(indicatorData->contentImageScaleFactor);
            [textPreviews addObject:adoptNS([PAL::alloc_WTTextPreviewInstance() initWithSnapshotImage:adoptCF(CGImageCreateWithImageInRect(snapshotPlatformImage, textRectInSnapshotCoordinates)).get() presentationFrame:textLineFrameInBoundingRectCoordinates]).get()];
        }

        completionHandler(textPreviews.get());
    });

}

- (void)textPreviewForRect:(CGRect)rect completion:(void (^)(_WTTextPreview *preview))completionHandler
{
    CGFloat deviceScale = _webView->page().deviceScaleFactor();
    WebCore::IntSize bitmapSize(rect.size.width, rect.size.height);
    bitmapSize.scale(deviceScale, deviceScale);

    _webView->page().takeSnapshot(WebCore::IntRect(rect), bitmapSize, WebKit::SnapshotOptionsShareable, [rect, completionHandler = makeBlockPtr(completionHandler)](std::optional<WebCore::ShareableBitmap::Handle>&& imageHandle) {
        if (!imageHandle) {
            completionHandler(nil);
            return;
        }
        auto bitmap = WebCore::ShareableBitmap::create(WTFMove(*imageHandle), WebCore::SharedMemory::Protection::ReadOnly);
        RetainPtr<CGImageRef> cgImage = bitmap ? bitmap->makeCGImage() : nullptr;
        RetainPtr textPreview = adoptNS([PAL::alloc_WTTextPreviewInstance() initWithSnapshotImage:cgImage.get() presentationFrame:rect]);
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

#endif // ENABLE(WRITING_TOOLS_UI) && PLATFORM(MAC)

