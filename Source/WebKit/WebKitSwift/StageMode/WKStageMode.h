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

#if defined(TARGET_OS_VISION) && TARGET_OS_VISION
#import "RealityKitBridging.h"
#import <Foundation/Foundation.h>
#import <simd/simd.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, WKStageModeOperation) {
    WKStageModeOperationNone = 0,
    WKStageModeOperationOrbit,
};

@protocol WKStageModeInteractionAware <NSObject>
- (void)stageModeInteractionDidUpdateModel;
@end

@interface WKStageModeInteractionDriver : NSObject
@property (nonatomic, readonly) REEntityRef interactionContainerRef;
@property (nonatomic, readonly) bool stageModeInteractionInProgress;

- (instancetype)initWithModel:(WKSRKEntity *)model container:(REEntityRef)container delegate:(id<WKStageModeInteractionAware> _Nullable)delegate NS_SWIFT_NAME(init(with:container:delegate:));
- (void)setContainerTransformInPortal NS_SWIFT_NAME(setContainerTransformInPortal());
- (void)interactionDidBegin:(simd_float4x4)transform NS_SWIFT_NAME(interactionDidBegin(_:));
- (void)interactionDidUpdate:(simd_float4x4)transform NS_SWIFT_NAME(interactionDidUpdate(_:));
- (void)interactionDidEnd NS_SWIFT_NAME(interactionDidEnd());
- (void)operationDidUpdate:(WKStageModeOperation)operation NS_SWIFT_NAME(operationDidUpdate(_:));
- (void)removeInteractionContainerFromSceneOrParent;
@end

NS_ASSUME_NONNULL_END

#endif
