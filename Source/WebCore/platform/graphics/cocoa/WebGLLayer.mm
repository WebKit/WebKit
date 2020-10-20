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
    std::unique_ptr<WebCore::IOSurface> _contentsBuffer;
    std::unique_ptr<WebCore::IOSurface> _drawingBuffer;
    std::unique_ptr<WebCore::IOSurface> _spareBuffer;
    WebCore::IntSize _bufferSize;
    BOOL _usingAlpha;
    void* _eglDisplay;
    void* _eglConfig;
    void* _contentsPbuffer;
    void* _drawingPbuffer;
    void* _sparePbuffer;
    void* _latchedPbuffer;
    BOOL _preparedForDisplay;
}

- (id)initWithGraphicsContextGL:(NakedPtr<WebCore::GraphicsContextGLOpenGL>)context
{
    _context = context;
    self = [super init];
    auto attributes = context->contextAttributes();
    _devicePixelRatio = attributes.devicePixelRatio;
    self.contentsOpaque = !attributes.alpha;
    self.transform = CATransform3DIdentity;
    self.contentsScale = _devicePixelRatio;
    return self;
}

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

- (CGImageRef)copyImageSnapshotWithColorSpace:(CGColorSpaceRef)colorSpace
{
    if (!_context)
        return nullptr;
    // FIXME: implement. https://bugs.webkit.org/show_bug.cgi?id=217377
    UNUSED_PARAM(colorSpace);
    return nullptr;
}

- (void)prepareForDisplay
{
    if (!_context)
        return;

    // To avoid running any OpenGL code in `display`, this method should be called
    // at the end of the rendering task. We will flush all painting commands
    // leaving the buffers ready to composite.

    if (!_context->makeContextCurrent()) {
        // Context is likely being torn down.
        return;
    }
    _context->prepareTexture();
    if (_drawingBuffer) {
        if (_latchedPbuffer) {
            WTF::Optional<ScopedRestoreTextureBinding> restoreBinding;
            GCGLenum textureTarget = WebCore::GraphicsContextGLOpenGL::IOSurfaceTextureTarget();
            // We don't need to restore GL_TEXTURE_RECTANGLE because it's not accessible from user code.
            if (textureTarget != WebCore::GraphicsContextGL::TEXTURE_RECTANGLE_ARB)
                restoreBinding.emplace(WebCore::GraphicsContextGLOpenGL::IOSurfaceTextureTargetQuery(), textureTarget);
            GCGLenum texture = _context->platformTexture();
            gl::BindTexture(textureTarget, texture);
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

    if (_contentsBuffer && _preparedForDisplay) {
        self.contents = _contentsBuffer->asLayerContents();
        [self reloadValueForKeyPath:@"contents"];
    }

    _context->markLayerComposited();
    auto layer = WebCore::PlatformCALayer::platformCALayerForLayer((__bridge void*)self);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayerDidDisplay(layer.get());

    _preparedForDisplay = NO;
}

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
    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_IOSURFACE_PLANE_ANGLE, 0,
        EGL_TEXTURE_TARGET, WebCore::GraphicsContextGLOpenGL::EGLIOSurfaceTextureTarget(),
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

    return true;
}

- (void)bindFramebufferToNextAvailableSurface
{
    WTF::Optional<ScopedRestoreTextureBinding> restoreBinding;
    GCGLenum textureTarget = WebCore::GraphicsContextGLOpenGL::IOSurfaceTextureTarget();
    // We don't need to restore GL_TEXTURE_RECTANGLE because it's not accessible from user code.
    if (textureTarget != WebCore::GraphicsContextGL::TEXTURE_RECTANGLE_ARB)
        restoreBinding.emplace(WebCore::GraphicsContextGLOpenGL::IOSurfaceTextureTargetQuery(), textureTarget);

    GCGLenum texture = _context->platformTexture();
    gl::BindTexture(textureTarget, texture);

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
}

@end

#endif // ENABLE(WEBGL)
