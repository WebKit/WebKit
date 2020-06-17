/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#import "IOSurface.h"
#import "IntSize.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/NakedPtr.h>

namespace WebCore {
class GraphicsLayer;
class GraphicsContextGLOpenGL;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

#if USE(OPENGL)
@interface WebGLLayer : CALayer
#elif USE(OPENGL_ES)
@interface WebGLLayer : CAEAGLLayer
#elif USE(ANGLE)
@interface WebGLLayer : CALayer
#else
#error Unsupported platform
#endif

@property (nonatomic) NakedPtr<WebCore::GraphicsContextGLOpenGL> context;

- (id)initWithGraphicsContextGL:(NakedPtr<WebCore::GraphicsContextGLOpenGL>)context;

- (CGImageRef)copyImageSnapshotWithColorSpace:(CGColorSpaceRef)colorSpace;

- (void)prepareForDisplay;

#if USE(OPENGL) || USE(ANGLE)
- (bool)allocateIOSurfaceBackingStoreWithSize:(WebCore::IntSize)size usingAlpha:(BOOL)usingAlpha;
- (void)bindFramebufferToNextAvailableSurface;
#endif

#if USE(ANGLE)
- (void)setEGLDisplay:(void*)eglDisplay config:(void*)eglConfig;
- (void)releaseGLResources;
#endif

@end

ALLOW_DEPRECATED_DECLARATIONS_END
