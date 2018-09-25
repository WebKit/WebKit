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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "OnscreenRenderer.h"

#import <simd/simd.h>

@interface OnscreenRenderer ()

@property MTKView *view;
@property CGSize drawableSize;

@end

@implementation OnscreenRenderer

- (instancetype)initWithView:(MTKView *)view device:(nonnull id<MTLDevice>)device
{
    self = [super initWithDevice:device];
    if (self) {
        self.view = view;
        self.view.delegate = self;
    }
    return self;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
    [self setTargetWidth:size.width setTargetHeight:size.height];
}

- (void)drawInMTKView:(MTKView *)view
{
    [self draw];
}

- (MTLRenderPassDescriptor*)renderPassDescriptor
{
    return self.view.currentRenderPassDescriptor;
}

- (void)configureCommandBufferBeforeCommit:(id<MTLCommandBuffer>)commandBuffer
{
    [commandBuffer presentDrawable:self.view.currentDrawable];
}

- (void)setShaderSource:(NSString *)source vertexShaderName:(NSString *)vertexShaderName fragmentShaderName:(NSString *)fragmentShaderName
{
    MTLCompileOptions *compileOptions = [[MTLCompileOptions alloc] init];
    NSError *error = nil;
    id<MTLLibrary> library = [self.device newLibraryWithSource:source options:compileOptions error:&error];
    if (!library && error) {
        NSLog(@"Compile error: %@", error.localizedDescription);
        return;
    }

    [self setLibrary:library vertexShaderName:vertexShaderName fragmentShaderName:fragmentShaderName pixelFormat:self.view.colorPixelFormat];
}

@end
