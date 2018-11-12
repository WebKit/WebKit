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

#pragma once

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <WebCore/GraphicsLayer.h>

@protocol WKNativelyInteractible <NSObject>
@end

@interface WKCompositingView : UIView
@end

@interface WKTransformView : WKCompositingView
@end

@interface WKSimpleBackdropView : WKCompositingView
@end

@interface WKShapeView : WKCompositingView
@end

@interface WKRemoteView : WKCompositingView

- (instancetype)initWithFrame:(CGRect)frame contextID:(uint32_t)contextID;

@end

@interface WKUIRemoteView : _UIRemoteView
@end

@interface WKBackdropView : _UIBackdropView
@end

@interface WKChildScrollView : UIScrollView
@end

@interface WKEmbeddedView : UIView

@property (nonatomic, assign) WebCore::GraphicsLayer::EmbeddedViewID embeddedViewID;
@property (nonatomic, assign) WebCore::GraphicsLayer::PlatformLayerID layerID;

@end

#endif // PLATFORM(IOS_FAMILY)
