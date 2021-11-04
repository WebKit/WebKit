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

#if USE(APPLE_INTERNAL_SDK)

#if ENABLE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
#import <AssetViewer/ARQuickLookWebKitItem.h>
#endif

#if PLATFORM(IOS)
#import <AssetViewer/ASVThumbnailView.h>
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#import <AssetViewer/ASVInlinePreview.h>
#endif

#else

#import <UIKit/UIKit.h>

#if PLATFORM(IOS)
@class ASVThumbnailView;
@class QLItem;
@class QLPreviewController;

NS_ASSUME_NONNULL_BEGIN

@protocol ASVThumbnailViewDelegate <NSObject>
- (void)thumbnailView:(ASVThumbnailView *)thumbnailView wantsToPresentPreviewController:(QLPreviewController *)previewController forItem:(QLItem *)item;
@end

@interface ASVThumbnailView : UIView
@property (nonatomic, weak) id<ASVThumbnailViewDelegate> delegate;
@property (nonatomic, assign) QLItem *thumbnailItem;
@property (nonatomic) CGSize maxThumbnailSize;
@end
#endif

#if ENABLE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
#import <ARKit/ARKit.h>

@protocol ARQuickLookWebKitItemDelegate
@end

@class ARQuickLookWebKitItem;

@interface ARQuickLookWebKitItem : QLItem
- (instancetype)initWithPreviewItemProvider:(NSItemProvider *)itemProvider contentType:(NSString *)contentType previewTitle:(NSString *)previewTitle fileSize:(NSNumber *)fileSize previewItem:(ARQuickLookPreviewItem *)previewItem;
- (void)setDelegate:(id <ARQuickLookWebKitItemDelegate>)delegate;
@end

#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)

@class ASVInlinePreview;
@class CAFenceHandle;
@interface ASVInlinePreview : NSObject
@property (nonatomic, readonly) CALayer *layer;

- (instancetype)initWithFrame:(CGRect)frame;
- (void)setupRemoteConnectionWithCompletionHandler:(void (^)(NSError * _Nullable error))handler;
- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable error))handler;
- (void)updateFrame:(CGRect)newFrame completionHandler:(void (^)(CAFenceHandle * _Nullable fenceHandle, NSError * _Nullable error))handler;
- (void)createFullscreenInstanceWithInitialFrame:(CGRect)initialFrame previewOptions:(NSDictionary *)previewOptions completionHandler:(void (^)(UIViewController *remoteViewController, CAFenceHandle * _Nullable fenceHandle, NSError * _Nullable error))handler;
- (void)observeDismissFullscreenWithCompletionHandler:(void (^)(CAFenceHandle * _Nullable fenceHandle, NSDictionary * _Nonnull payload, NSError * _Nullable error))handler;
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event;
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event;
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event;

@end

#endif

NS_ASSUME_NONNULL_END

#endif
