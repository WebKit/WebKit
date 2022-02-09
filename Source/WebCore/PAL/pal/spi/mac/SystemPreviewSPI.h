/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
#import <AssetViewer/ASVInlinePreview.h>
#endif

#else // USE(APPLE_INTERNAL_SDK)

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)

#import <simd/simd.h>

NS_ASSUME_NONNULL_BEGIN

@class ASVInlinePreview;

@interface ASVInlinePreview : NSObject
@property (nonatomic, readonly) NSUUID *uuid;
@property (nonatomic, readonly) CALayer *layer;
@property (nonatomic, readonly) uint32_t contextId;

- (instancetype)initWithFrame:(CGRect)frame;
- (instancetype)initWithFrame:(CGRect)frame UUID:(NSUUID *)uuid;
- (void)setupRemoteConnectionWithCompletionHandler:(void (^)(NSError * _Nullable error))handler;
- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable error))handler;
- (void)setRemoteContext:(uint32_t)contextId;

- (void)updateFrame:(CGRect)newFrame completionHandler:(void (^)(CAFenceHandle * _Nullable fenceHandle, NSError * _Nullable error))handler;
- (void)setFrameWithinFencedTransaction:(CGRect)frame;

- (void)mouseDownAtLocation:(CGPoint)location timestamp:(NSTimeInterval)timestamp;
- (void)mouseDraggedAtLocation:(CGPoint)location timestamp:(NSTimeInterval)timestamp;
- (void)mouseUpAtLocation:(CGPoint)location timestamp:(NSTimeInterval)timestamp;

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

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(ARKIT_INLINE_PREVIEW_MAC)

#endif // USE(APPLE_INTERNAL_SDK)
