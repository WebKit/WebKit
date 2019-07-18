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
#import "ImageBufferUtilitiesCG.h"
#import "NotImplemented.h"
#import "PlatformCALayer.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/FastMalloc.h>
#import <wtf/RetainPtr.h>

#if USE(OPENGL)
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#endif

#if USE(ANGLE)
#define EGL_EGL_PROTOTYPES 0
#include <ANGLE/egl.h>
#include <ANGLE/eglext.h>
#include <ANGLE/eglext_angle.h>
#include <ANGLE/entry_points_egl.h>
#include <ANGLE/entry_points_gles_2_0_autogen.h>
// Skip the inclusion of ANGLE's explicit context entry points for now.
#define GL_ANGLE_explicit_context
#include <ANGLE/gl2ext.h>
#include <ANGLE/gl2ext_angle.h>
#endif

@implementation WebGLLayer

@synthesize context=_context;

-(id)initWithGraphicsContext3D:(WebCore::GraphicsContext3D*)context
{
    _context = context;
    self = [super init];
    _devicePixelRatio = context->getContextAttributes().devicePixelRatio;
#if USE(OPENGL) || USE(ANGLE) && PLATFORM(MAC)
    self.contentsOpaque = !context->getContextAttributes().alpha;
    self.transform = CATransform3DIdentity;
    self.contentsScale = _devicePixelRatio;
#else
    self.opaque = !context->getContextAttributes().alpha;
#endif
    return self;
}

#if USE(OPENGL) || (USE(ANGLE) && PLATFORM(MAC))
// When using an IOSurface as layer contents, we need to flip the
// layer to take into account that the IOSurface provides content
// in Y-up. This means that any incoming transform (unlikely, since
// this is a contents layer) and anchor point must add a Y scale of
// -1 and make sure the transform happens from the top.

- (void)setTransform:(CATransform3D)t
{
    [super setTransform:CATransform3DScale(t, 1, -1, 1)];
}

- (void)setAnchorPoint:(CGPoint)p
{
    [super setAnchorPoint:CGPointMake(p.x, 1.0 - p.y)];
}
#endif // USE(OPENGL) || (USE(ANGLE) && PLATFORM(MAC))

#if USE(OPENGL)
static void freeData(void *, const void *data, size_t /* size */)
{
    fastFree(const_cast<void *>(data));
}
#endif // USE(OPENGL)

- (CGImageRef)copyImageSnapshotWithColorSpace:(CGColorSpaceRef)colorSpace
{
    if (!_context)
        return nullptr;

#if USE(OPENGL)
    CGLSetCurrentContext(_context->platformGraphicsContext3D());

    RetainPtr<CGColorSpaceRef> imageColorSpace = colorSpace;
    if (!imageColorSpace)
        imageColorSpace = WebCore::sRGBColorSpaceRef();

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

    WebCore::verifyImageBufferIsBigEnough((uint8_t*)data, dataSize);
    CGDataProviderRef provider = CGDataProviderCreateWithData(0, data, dataSize, freeData);
    CGImageRef image = CGImageCreate(width, height, 8, 32, rowBytes, imageColorSpace.get(),
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host, provider, 0, true, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    return image;
#else
    // FIXME: implement.
    UNUSED_PARAM(colorSpace);
    return nullptr;
#endif
}

- (void)display
{
    if (!_context)
        return;

#if USE(OPENGL)
    _context->prepareTexture();
    if (_drawingBuffer) {
        std::swap(_contentsBuffer, _drawingBuffer);
        self.contents = _contentsBuffer->asLayerContents();
        [self reloadValueForKeyPath:@"contents"];
        [self bindFramebufferToNextAvailableSurface];
    }
#elif USE(OPENGL_ES)
    _context->presentRenderbuffer();
#elif USE(ANGLE)
    _context->prepareTexture();
    if (_drawingBuffer) {
        if (_latchedPbuffer) {
            GC3Denum texture = _context->platformTexture();
            gl::BindTexture(GL_TEXTURE_RECTANGLE_ANGLE, texture);
            if (!EGL_ReleaseTexImage(_eglDisplay, _latchedPbuffer, EGL_BACK_BUFFER)) {
                // FIXME: report error.
                notImplemented();
            }
            _latchedPbuffer = nullptr;
        }
        std::swap(_contentsBuffer, _drawingBuffer);
        std::swap(_contentsPbuffer, _drawingPbuffer);
        self.contents = _contentsBuffer->asLayerContents();
        [self reloadValueForKeyPath:@"contents"];
        [self bindFramebufferToNextAvailableSurface];
    }
#endif

    _context->markLayerComposited();
    WebCore::PlatformCALayer* layer = WebCore::PlatformCALayer::platformCALayer((__bridge void*)self);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayerDidDisplay(layer);
}

#if (USE(ANGLE) && PLATFORM(MAC))
- (void)setEGLDisplay:(void*)display andConfig:(void*)config
{
    _eglDisplay = display;
    _eglConfig = config;
}

- (void)dealloc
{
    EGL_DestroySurface(_eglDisplay, _contentsPbuffer);
    EGL_DestroySurface(_eglDisplay, _drawingPbuffer);
    EGL_DestroySurface(_eglDisplay, _sparePbuffer);
    [super dealloc];
}
#endif

#if USE(OPENGL) || (USE(ANGLE) && PLATFORM(MAC))
- (void)allocateIOSurfaceBackingStoreWithSize:(WebCore::IntSize)size usingAlpha:(BOOL)usingAlpha
{
    _bufferSize = size;
    _usingAlpha = usingAlpha;
    _contentsBuffer = WebCore::IOSurface::create(size, WebCore::sRGBColorSpaceRef());
    _drawingBuffer = WebCore::IOSurface::create(size, WebCore::sRGBColorSpaceRef());
    _spareBuffer = WebCore::IOSurface::create(size, WebCore::sRGBColorSpaceRef());

    ASSERT(_contentsBuffer);
    ASSERT(_drawingBuffer);
    ASSERT(_spareBuffer);

    _contentsBuffer->migrateColorSpaceToProperties();
    _drawingBuffer->migrateColorSpaceToProperties();
    _spareBuffer->migrateColorSpaceToProperties();

#if USE(ANGLE) && PLATFORM(MAC)
    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_IOSURFACE_PLANE_ANGLE, 0,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_RECTANGLE_ANGLE,
        EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_BGRA_EXT,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TYPE_ANGLE, GL_UNSIGNED_BYTE,
        EGL_NONE, EGL_NONE
    };

    _contentsPbuffer = EGL_CreatePbufferFromClientBuffer(_eglDisplay, EGL_IOSURFACE_ANGLE, _contentsBuffer->surface(), _eglConfig, surfaceAttributes);
    _drawingPbuffer = EGL_CreatePbufferFromClientBuffer(_eglDisplay, EGL_IOSURFACE_ANGLE, _drawingBuffer->surface(), _eglConfig, surfaceAttributes);
    _sparePbuffer = EGL_CreatePbufferFromClientBuffer(_eglDisplay, EGL_IOSURFACE_ANGLE, _spareBuffer->surface(), _eglConfig, surfaceAttributes);
#endif
}

- (void)bindFramebufferToNextAvailableSurface
{
#if USE(OPENGL)
    GC3Denum texture = _context->platformTexture();
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);

    if (_drawingBuffer && _drawingBuffer->isInUse())
        std::swap(_drawingBuffer, _spareBuffer);

    IOSurfaceRef ioSurface = _drawingBuffer->surface();
    GC3Denum internalFormat = _usingAlpha ? GL_RGBA : GL_RGB;

    // Link the IOSurface to the texture.
    CGLError error = CGLTexImageIOSurface2D(_context->platformGraphicsContext3D(), GL_TEXTURE_RECTANGLE_ARB, internalFormat, _bufferSize.width(), _bufferSize.height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, ioSurface, 0);
    ASSERT_UNUSED(error, error == kCGLNoError);
#elif USE(ANGLE)
    GC3Denum texture = _context->platformTexture();
    gl::BindTexture(GL_TEXTURE_RECTANGLE_ANGLE, texture);

    if (_latchedPbuffer) {
        if (!EGL_ReleaseTexImage(_eglDisplay, _latchedPbuffer, EGL_BACK_BUFFER)) {
            // FIXME: report error.
            notImplemented();
        }
        _latchedPbuffer = nullptr;
    }

    if (_drawingBuffer && _drawingBuffer->isInUse()) {
        std::swap(_drawingBuffer, _spareBuffer);
        std::swap(_drawingPbuffer, _sparePbuffer);
    }

    // Link the IOSurface to the texture via the previously-created pbuffer.
    if (!EGL_BindTexImage(_eglDisplay, _drawingPbuffer, EGL_BACK_BUFFER)) {
        // FIXME: report error.
        notImplemented();
    }
    _latchedPbuffer = _drawingPbuffer;
#endif
}
#endif // USE(OPENGL) || (USE(ANGLE) && PLATFORM(MAC))

@end

#endif // ENABLE(WEBGL)
