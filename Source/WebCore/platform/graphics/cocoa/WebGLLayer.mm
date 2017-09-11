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

#include "config.h"

#if ENABLE(WEBGL)
#import "WebGLLayer.h"

#import "GraphicsContext3D.h"
#import "GraphicsContextCG.h"
#import "GraphicsLayer.h"
#import "GraphicsLayerCA.h"
#import "PlatformCALayer.h"
#import <wtf/FastMalloc.h>
#import <wtf/RetainPtr.h>

#if !PLATFORM(IOS)
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#endif

using namespace WebCore;

@implementation WebGLLayer

@synthesize context=_context;

-(id)initWithGraphicsContext3D:(GraphicsContext3D*)context
{
    _context = context;
    self = [super init];
    _devicePixelRatio = context->getContextAttributes().devicePixelRatio;
#if PLATFORM(MAC)
    self.contentsScale = _devicePixelRatio;
    self.colorspace = sRGBColorSpaceRef();
#endif
    return self;
}

#if !PLATFORM(IOS)
-(CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
    // We're basically copying the pixel format object from the existing
    // WebGL context, so we don't need to use the display mask.
    UNUSED_PARAM(mask);

    CGLPixelFormatObj webglPixelFormat = CGLGetPixelFormat(_context->platformGraphicsContext3D());

    Vector<CGLPixelFormatAttribute> attribs;
    GLint value;

    CGLDescribePixelFormat(webglPixelFormat, 0, kCGLPFAColorSize, &value);
    attribs.append(kCGLPFAColorSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(value));

    // We don't need to specify a depth size since we're only
    // using this context as a 2d blit destination for the WebGL FBO.
    attribs.append(kCGLPFADepthSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(0));

    CGLDescribePixelFormat(webglPixelFormat, 0, kCGLPFAAllowOfflineRenderers, &value);
    if (value)
        attribs.append(kCGLPFAAllowOfflineRenderers);

    CGLDescribePixelFormat(webglPixelFormat, 0, kCGLPFAAccelerated, &value);
    if (value)
        attribs.append(kCGLPFAAccelerated);

    CGLDescribePixelFormat(webglPixelFormat, 0, kCGLPFAOpenGLProfile, &value);
    if (value) {
        attribs.append(kCGLPFAOpenGLProfile);
        attribs.append(static_cast<CGLPixelFormatAttribute>(value));
    }

    attribs.append(static_cast<CGLPixelFormatAttribute>(0));

    CGLPixelFormatObj pixelFormat;
    GLint numPixelFormats = 0;
    CGLChoosePixelFormat(attribs.data(), &pixelFormat, &numPixelFormats);

    ASSERT(pixelFormat);
    ASSERT(numPixelFormats);

    return pixelFormat;
}

-(CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
    CGLContextObj contextObj;
    CGLCreateContext(pixelFormat, _context->platformGraphicsContext3D(), &contextObj);
    return contextObj;
}

-(void)drawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    if (!_context)
        return;

    _context->prepareTexture();

    CGLSetCurrentContext(glContext);

    CGRect frame = [self frame];
    frame.size.width *= _devicePixelRatio;
    frame.size.height *= _devicePixelRatio;

    // draw the FBO into the layer
    glViewport(0, 0, frame.size.width, frame.size.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _context->platformTexture());

    glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0, 0);
        glVertex2f(-1, -1);
        glTexCoord2f(1, 0);
        glVertex2f(1, -1);
        glTexCoord2f(1, 1);
        glVertex2f(1, 1);
        glTexCoord2f(0, 1);
        glVertex2f(-1, 1);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // Call super to finalize the drawing. By default all it does is call glFlush().
    [super drawInCGLContext:glContext pixelFormat:pixelFormat forLayerTime:timeInterval displayTime:timeStamp];
}

static void freeData(void *, const void *data, size_t /* size */)
{
    fastFree(const_cast<void *>(data));
}
#endif

-(CGImageRef)copyImageSnapshotWithColorSpace:(CGColorSpaceRef)colorSpace
{
    if (!_context)
        return nullptr;

#if PLATFORM(IOS)
    UNUSED_PARAM(colorSpace);
    return nullptr;
#else
    CGLSetCurrentContext(_context->platformGraphicsContext3D());

    RetainPtr<CGColorSpaceRef> imageColorSpace = colorSpace;
    if (!imageColorSpace)
        imageColorSpace = sRGBColorSpaceRef();

    CGRect layerBounds = CGRectIntegral([self bounds]);

    size_t width = layerBounds.size.width * _devicePixelRatio;
    size_t height = layerBounds.size.height * _devicePixelRatio;

    size_t rowBytes = (width * 4 + 15) & ~15;
    size_t dataSize = rowBytes * height;
    void* data = fastMalloc(dataSize);
    if (!data)
        return nullptr;

    glPixelStorei(GL_PACK_ROW_LENGTH, rowBytes / 4);
    glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);

    CGDataProviderRef provider = CGDataProviderCreateWithData(0, data, dataSize, freeData);
    CGImageRef image = CGImageCreate(width, height, 8, 32, rowBytes, imageColorSpace.get(),
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host, provider, 0, true, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    return image;
#endif
}

- (void)display
{
    if (!_context)
        return;

#if PLATFORM(IOS)
    _context->endPaint();
#else
    [super display];
#endif
    _context->markLayerComposited();
    PlatformCALayer* layer = PlatformCALayer::platformCALayer(self);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayerDidDisplay(layer);
}

@end

#endif // ENABLE(WEBGL)
