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

#import "config.h"
#import "WKSeparatedImageView.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)

#import "RemoteLayerTreeViews.h"

@interface WKSeparatedImageView (WKContentControlled) <WKContentControlled>
@end

@implementation WKSeparatedImageView (WKContentControlled)

+ (Class)layerClass {
    return [WKObservingLayer class];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    [self layoutCustomSubtree];
}

@end

@implementation WKObservingLayer

- (void)setSeparated:(BOOL)isSeparated
{
    [super setSeparated:isSeparated];
    [self.layerDelegate layerSeparatedDidChange:self];
}

- (void)setContents:(id)contents
{
    [super setContents:contents];
    if (!contents)
        [self.layerDelegate layerWasCleared:self];

}

@end

#if USE(APPLE_INTERNAL_SDK)
// Swift implementation.
#else
@implementation WKSeparatedImageView

- (instancetype)init
{
    self = [super initWithFrame:CGRectZero];
    return self;
}

- (instancetype)initWithFrame:(CGRect)frame {
    return [self init];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    ASSERT_NOT_REACHED();
    return [self initWithCoder:coder];
}

- (void)setSurface:(nullable IOSurfaceRef)surface
{
}

- (void)layoutCustomSubtree
{
}

- (void)layerSeparatedDidChange:(CALayer *)layer
{
}

- (void)layerWasCleared:(CALayer *)layer
{
}

@end
#endif // USE(APPLE_INTERNAL_SDK)

#endif


