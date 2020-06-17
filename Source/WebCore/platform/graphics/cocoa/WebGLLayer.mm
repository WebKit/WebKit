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

#import "config.h"

#if ENABLE(WEBGL)
#import "WebGLLayer.h"

#import "GraphicsContextCG.h"
#import "GraphicsContextGLOpenGL.h"
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
#import <ANGLE/egl.h>
#import <ANGLE/eglext.h>
#import <ANGLE/eglext_angle.h>
#import <ANGLE/entry_points_egl.h>
#import <ANGLE/entry_points_gles_2_0_autogen.h>
// Skip the inclusion of ANGLE's explicit context entry points for now.
#define GL_ANGLE_explicit_context
#import <ANGLE/gl2ext.h>
#import <ANGLE/gl2ext_angle.h>
#endif

namespace {
    class ScopedRestoreTextureBinding {
        WTF_MAKE_NONCOPYABLE(ScopedRestoreTextureBinding);
    public:
        ScopedRestoreTextureBinding(GLenum bindingPointQuery, GLenum bindingPoint)
            : m_bindingPoint(bindingPoint)
        {
            gl::GetIntegerv(bindingPointQuery, &m_bindingValue);
        }

        ~ScopedRestoreTextureBinding()
        {
            gl::BindTexture(m_bindingPoint, m_bindingValue);
        }

    private:
        GLint m_bindingPoint { 0 };
        GLint m_bindingValue { 0 };
    };
}

@implementation WebGLLayer {
    float _devicePixelRatio;
#if USE(OPENGL) || USE(ANGLE)
    std::unique_ptr<WebCore::IOSurface> _contentsBuffer;
    std::unique_ptr<WebCore::IOSurface> _drawingBuffer;
    std::unique_ptr<WebCore::IOSurface> _spareBuffer;
    WebCore::IntSize _bufferSize;
    BOOL _usingAlpha;
#endif
#if USE(ANGLE)
    void* _eglDisplay;
    void* _eglConfig;
    void* _contentsPbuffer;
    void* _drawingPbuffer;
    void* _sparePbuffer;
    void* _latchedPbuffer;
#endif
    BOOL _preparedForDisplay;
}

- (id)initWithGraphicsContextGL:(NakedPtr<WebCore::GraphicsContextGLOpenGL>)context
{
    _context = context;
    self = [super init];
    auto attributes = context->contextAttributes();
    _devicePixelRatio = attributes.devicePixelRatio;
#if USE(OPENGL) || USE(ANGLE)
    self.contentsOpaque = !attributes.alpha;
    self.transform = CATransform3DIdentity;
    self.contentsScale = _devicePixelRatio;
#else
    self.opaque = !attributes.alpha;
#endif
    return self;
}

#if USE(OPENGL) || USE(ANGLE)
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
#endif // USE(OPENGL) || USE(ANGLE)

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
    CGLContextObj cglContext = static_cast<CGLContextObj>(_context->platformGraphicsContextGL());
    CGLSetCurrentContext(cglContext);

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

- (void)prepareForDisplay
{
    if (!_context)
        return;

    // To avoid running any OpenGL code in `display`, this method should be called
    // at the end of the rendering task. We will flush all painting commands
    // leaving the buffers ready to composite.

#if USE(OPENGL)
    _context->prepareTexture();
    if (_drawingBuffer) {
        std::swap(_contentsBuffer, _drawingBuffer);
        [self bindFramebufferToNextAvailableSurface];
    }
#elif USE(ANGLE)
    if (!_context->makeContextCurrent()) {
        // Context is likely being torn down.
        return;
    }
    _context->prepareTexture();
    if (_drawingBuffer) {
        if (_latchedPbuffer) {
            WTF::Optional<ScopedRestoreTextureBinding> restoreBinding;
            // We don't need to restore GL_TEXTURE_RECTANGLE because it's not accessible from user code.
            if (WebCore::GraphicsContextGL::IOSurfaceTextureTarget != WebCore::GraphicsContextGL::TEXTURE_RECTANGLE_ARB)
                restoreBinding.emplace(WebCore::GraphicsContextGL::IOSurfaceTextureTargetQuery, WebCore::GraphicsContextGL::IOSurfaceTextureTarget);
            GCGLenum texture = _context->platformTexture();
            gl::BindTexture(WebCore::GraphicsContextGL::IOSurfaceTextureTarget, texture);
            if (!EGL_ReleaseTexImage(_eglDisplay, _latchedPbuffer, EGL_BACK_BUFFER)) {
                // FIXME: report error.
                notImplemented();
            }
            _latchedPbuffer = nullptr;
        }

        std::swap(_contentsBuffer, _drawingBuffer);
        std::swap(_contentsPbuffer, _drawingPbuffer);
        [self bindFramebufferToNextAvailableSurface];
    }
#endif
    [self setNeedsDisplay];
    _preparedForDisplay = YES;
}

- (void)display
{
    if (!_context)
        return;

    // At this point we've painted into the _drawingBuffer and swapped it with the old _contentsBuffer,
    // so all we need to do here is tickle the CALayer to let it know it has new contents.
    // This avoids running any OpenGL code in this method.

#if USE(OPENGL) || USE(ANGLE)
    if (_contentsBuffer && _preparedForDisplay) {
        self.contents = _contentsBuffer->asLayerContents();
        [self reloadValueForKeyPath:@"contents"];
    }
#elif USE(OPENGL_ES)
    _context->presentRenderbuffer();
#endif

    _context->markLayerComposited();
    auto layer = WebCore::PlatformCALayer::platformCALayerForLayer((__bridge void*)self);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayerDidDisplay(layer.get());

    _preparedForDisplay = NO;
}

#if USE(ANGLE)
- (void)setEGLDisplay:(void*)display config:(void*)config
{
    _eglDisplay = display;
    _eglConfig = config;
}

- (void)releaseGLResources
{
    if (!_context)
        return;

    if (_context->makeContextCurrent() && _latchedPbuffer) {
        EGL_ReleaseTexImage(_eglDisplay, _latchedPbuffer, EGL_BACK_BUFFER);
        _latchedPbuffer = nullptr;
    }

    EGL_DestroySurface(_eglDisplay, _contentsPbuffer);
    EGL_DestroySurface(_eglDisplay, _drawingPbuffer);
    EGL_DestroySurface(_eglDisplay, _sparePbuffer);
}
#endif

#if USE(OPENGL) || USE(ANGLE)
- (bool)allocateIOSurfaceBackingStoreWithSize:(WebCore::IntSize)size usingAlpha:(BOOL)usingAlpha
{
    _bufferSize = size;
    _usingAlpha = usingAlpha;
    _contentsBuffer = WebCore::IOSurface::create(size, WebCore::sRGBColorSpaceRef());
    _drawingBuffer = WebCore::IOSurface::create(size, WebCore::sRGBColorSpaceRef());
    _spareBuffer = WebCore::IOSurface::create(size, WebCore::sRGBColorSpaceRef());

    if (!_contentsBuffer || !_drawingBuffer || !_spareBuffer)
        return false;

    _contentsBuffer->migrateColorSpaceToProperties();
    _drawingBuffer->migrateColorSpaceToProperties();
    _spareBuffer->migrateColorSpaceToProperties();

#if USE(ANGLE)
    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_IOSURFACE_PLANE_ANGLE, 0,
        EGL_TEXTURE_TARGET, WebCore::GraphicsContextGL::EGLIOSurfaceTextureTarget,
        EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, usingAlpha ? GL_BGRA_EXT : GL_RGB,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TYPE_ANGLE, GL_UNSIGNED_BYTE,
        // Only has an effect on the iOS Simulator.
        EGL_IOSURFACE_USAGE_HINT_ANGLE, EGL_IOSURFACE_WRITE_HINT_ANGLE,
        EGL_NONE, EGL_NONE
    };

    _contentsPbuffer = EGL_CreatePbufferFromClientBuffer(_eglDisplay, EGL_IOSURFACE_ANGLE, _contentsBuffer->surface(), _eglConfig, surfaceAttributes);
    _drawingPbuffer = EGL_CreatePbufferFromClientBuffer(_eglDisplay, EGL_IOSURFACE_ANGLE, _drawingBuffer->surface(), _eglConfig, surfaceAttributes);
    _sparePbuffer = EGL_CreatePbufferFromClientBuffer(_eglDisplay, EGL_IOSURFACE_ANGLE, _spareBuffer->surface(), _eglConfig, surfaceAttributes);

    if (!_contentsPbuffer || !_drawingPbuffer || !_sparePbuffer)
        return false;
#endif

    return true;
}

- (void)bindFramebufferToNextAvailableSurface
{
#if USE(OPENGL)
    GCGLenum texture = _context->platformTexture();
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);

    if (_drawingBuffer && _drawingBuffer->isInUse())
        std::swap(_drawingBuffer, _spareBuffer);

    IOSurfaceRef ioSurface = _drawingBuffer->surface();
    GCGLenum internalFormat = _usingAlpha ? GL_RGBA : GL_RGB;

    // Link the IOSurface to the texture.
    CGLContextObj cglContext = static_cast<CGLContextObj>(_context->platformGraphicsContextGL());
    CGLError error = CGLTexImageIOSurface2D(cglContext, GL_TEXTURE_RECTANGLE_ARB, internalFormat, _bufferSize.width(), _bufferSize.height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, ioSurface, 0);
    ASSERT_UNUSED(error, error == kCGLNoError);
#elif USE(ANGLE)
    WTF::Optional<ScopedRestoreTextureBinding> restoreBinding;
    // We don't need to restore GL_TEXTURE_RECTANGLE because it's not accessible from user code.
    if (WebCore::GraphicsContextGL::IOSurfaceTextureTarget != WebCore::GraphicsContextGL::TEXTURE_RECTANGLE_ARB)
        restoreBinding.emplace(WebCore::GraphicsContextGL::IOSurfaceTextureTargetQuery, WebCore::GraphicsContextGL::IOSurfaceTextureTarget);

    GCGLenum texture = _context->platformTexture();

    gl::BindTexture(WebCore::GraphicsContextGL::IOSurfaceTextureTarget, texture);

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
#endif // USE(OPENGL) || USE(ANGLE)

@end

#endif // ENABLE(WEBGL)
