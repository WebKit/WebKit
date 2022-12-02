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

#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
#import <AssetViewer/ARQuickLookWebKitItem.h>
#endif

#if PLATFORM(IOS)
#import <AssetViewer/ASVThumbnailView.h>
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#import <AssetViewer/ASVInlinePreview.h>
#endif

#if HAVE(UIKIT_WEBKIT_INTERNALS)
#import <AssetViewer/ASVLaunchPreview.h>
#endif

#else

#import <UIKit/UIKit.h>

#if PLATFORM(IOS)
#import <pal/spi/ios/QuickLookSPI.h>

@class ASVThumbnailView;
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

NS_ASSUME_NONNULL_END

#endif

#if HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)
#import <ARKit/ARKit.h>

NS_ASSUME_NONNULL_BEGIN

@protocol ARQuickLookWebKitItemDelegate
@end

@class ARQuickLookWebKitItem;

@interface ARQuickLookWebKitItem : QLItem
- (instancetype)initWithPreviewItemProvider:(NSItemProvider *)itemProvider contentType:(NSString *)contentType previewTitle:(NSString *)previewTitle fileSize:(NSNumber *)fileSize previewItem:(ARQuickLookPreviewItem *)previewItem;
- (void)setDelegate:(id <ARQuickLookWebKitItemDelegate>)delegate;
@end

NS_ASSUME_NONNULL_END

#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)

#import <simd/simd.h>

NS_ASSUME_NONNULL_BEGIN

@class ASVInlinePreview;
@class CAFenceHandle;
@interface ASVInlinePreview : NSObject
@property (nonatomic, readonly) CALayer *layer;

- (instancetype)initWithFrame:(CGRect)frame;
- (void)setupRemoteConnectionWithCompletionHandler:(void (^)(NSError * _Nullable error))handler;
- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable error))handler;
- (void)updateFrame:(CGRect)newFrame completionHandler:(void (^)(CAFenceHandle * _Nullable fenceHandle, NSError * _Nullable error))handler;
- (void)setFrameWithinFencedTransaction:(CGRect)frame;
- (void)createFullscreenInstanceWithInitialFrame:(CGRect)initialFrame previewOptions:(NSDictionary *)previewOptions completionHandler:(void (^)(UIViewController *remoteViewController, CAFenceHandle * _Nullable fenceHandle, NSError * _Nullable error))handler;
- (void)observeDismissFullscreenWithCompletionHandler:(void (^)(CAFenceHandle * _Nullable fenceHandle, NSDictionary * _Nonnull payload, NSError * _Nullable error))handler;
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event;
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event;
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(nullable UIEvent *)event;

typedef void (^ASVCameraTransformReplyBlock) (simd_float3 cameraTransform, NSError * _Nullable error);
- (void)getCameraTransform:(ASVCameraTransformReplyBlock)reply;
- (void)setCameraTransform:(simd_float3)transform;

@property (nonatomic, readwrite) NSTimeInterval currentTime;
@property (nonatomic, readonly) NSTimeInterval duration;
@property (nonatomic, readwrite) BOOL isLooping;
@property (nonatomic, readonly) BOOL isPlaying;
typedef void (^ASVSetIsPlayingReplyBlock) (BOOL isPlaying, NSError * _Nullable error);
- (void)setIsPlaying:(BOOL)isPlaying reply:(ASVSetIsPlayingReplyBlock)reply;

@property (nonatomic, readonly) BOOL hasAudio;
@property (nonatomic, readwrite) BOOL isMuted;

@property (nonatomic, retain, nullable) NSURL *canonicalWebPageURL;
@property (nonatomic, retain, nullable) NSString *urlFragment;

@end

@interface ASVLaunchPreview : NSObject
+ (void)beginPreviewApplicationWithURLs:(nonnull NSArray *)urls is3DContent:(BOOL)is3DContent completion:(nonnull void (^)(NSError * _Nullable))handler;
+ (void)launchPreviewApplicationWithURLs:(nonnull NSArray *)urls completion:(nonnull void (^)(NSError * _Nullable))handler;
@end

NS_ASSUME_NONNULL_END

#endif

#endif
