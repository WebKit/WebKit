/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Google Inc. All rights reserved.
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

#if ENABLE(GRAPHICS_CONTEXT_3D) && USE(ANGLE)
#include "GraphicsContext3D.h"

#if PLATFORM(IOS_FAMILY)
#include "GraphicsContext3DIOS.h"
#endif
#include "Extensions3DANGLE.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "TemporaryANGLESetting.h"
#include <JavaScriptCore/RegularExpression.h>
#include <algorithm>
#include <cstring>
#include <wtf/HexNumber.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

// Skip the inclusion of ANGLE's explicit context entry points for now.
#define GL_ANGLE_explicit_context
#define GL_ANGLE_explicit_context_gles1
typedef void* GLeglContext;
#include <ANGLE/entry_points_gles_2_0_autogen.h>
#include <ANGLE/entry_points_gles_3_0_autogen.h>
#include <ANGLE/entry_points_gles_ext_autogen.h>
#include <ANGLE/gl2ext.h>
#include <ANGLE/gl2ext_angle.h>

// Note: this file can't be compiled in the same unified source file
// as others which include the system's OpenGL headers.

// This one definition short-circuits the need for gl2ext.h, which
// would need more work to be included from WebCore.
#define GL_MAX_SAMPLES_EXT 0x8D57

namespace WebCore {

static const char* packedDepthStencilExtensionName = "GL_OES_packed_depth_stencil";

void GraphicsContext3D::releaseShaderCompiler()
{
    makeContextCurrent();
    notImplemented();
}

#if PLATFORM(MAC)
static void wipeAlphaChannelFromPixels(int width, int height, unsigned char* pixels)
{
    // We can assume this doesn't overflow because the calling functions
    // use checked arithmetic.
    int totalBytes = width * height * 4;
    for (int i = 0; i < totalBytes; i += 4)
        pixels[i + 3] = 255;
}
#endif

void GraphicsContext3D::readPixelsAndConvertToBGRAIfNecessary(int x, int y, int width, int height, unsigned char* pixels)
{
    // NVIDIA drivers have a bug where calling readPixels in BGRA can return the wrong values for the alpha channel when the alpha is off for the context.
    gl::ReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#if USE(ACCELERATE)
    vImage_Buffer src;
    src.height = height;
    src.width = width;
    src.rowBytes = width * 4;
    src.data = pixels;

    vImage_Buffer dest;
    dest.height = height;
    dest.width = width;
    dest.rowBytes = width * 4;
    dest.data = pixels;

    // Swap pixel channels from RGBA to BGRA.
    const uint8_t map[4] = { 2, 1, 0, 3 };
    vImagePermuteChannels_ARGB8888(&src, &dest, map, kvImageNoFlags);
#else
    int totalBytes = width * height * 4;
    for (int i = 0; i < totalBytes; i += 4)
        std::swap(pixels[i], pixels[i + 2]);
#endif

#if PLATFORM(MAC)
    if (!m_attrs.alpha)
        wipeAlphaChannelFromPixels(width, height, pixels);
#endif
}

void GraphicsContext3D::validateAttributes()
{
    validateDepthStencil(packedDepthStencilExtensionName);
}

bool GraphicsContext3D::reshapeFBOs(const IntSize& size)
{
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat, internalDepthStencilFormat = 0;
    if (m_attrs.alpha) {
        m_internalColorFormat = GL_RGBA8;
        colorFormat = GL_RGBA;
    } else {
        m_internalColorFormat = GL_RGB8;
        colorFormat = GL_RGB;
    }
    if (m_attrs.stencil || m_attrs.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContext3D::validateAttributes.

        Extensions3D& extensions = getExtensions();
        // Use a 24 bit depth buffer where we know we have it.
        if (extensions.supports("GL_OES_packed_depth_stencil"))
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_OES;
        else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT16;
    }

    // Resize multisample FBO.
    if (m_attrs.antialias) {
        GLint maxSampleCount;
        gl::GetIntegerv(GL_MAX_SAMPLES_ANGLE, &maxSampleCount);
        // Using more than 4 samples is slow on some hardware and is unlikely to
        // produce a significantly better result.
        GLint sampleCount = std::min(4, maxSampleCount);
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        gl::BindRenderbuffer(GL_RENDERBUFFER, m_multisampleColorBuffer);
        getExtensions().renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_RGBA8, width, height);
        gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth) {
            gl::BindRenderbuffer(GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            getExtensions().renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, internalDepthStencilFormat, width, height);
            // WebGL 1.0's rules state that combined depth/stencil renderbuffers
            // have to be attached to the synthetic DEPTH_STENCIL_ATTACHMENT point.
            if (!isGLES2Compliant() && internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES)
                gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            else {
                if (m_attrs.stencil)
                    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
                if (m_attrs.depth)
                    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            }
        }
        gl::BindRenderbuffer(GL_RENDERBUFFER, 0);
        if (gl::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            // FIXME: cleanup.
            notImplemented();
        }
    }

    // resize regular FBO
    gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    ASSERT(m_texture);
#if PLATFORM(COCOA)

#if PLATFORM(MAC)
    allocateIOSurfaceBackingStore(IntSize(width, height));
    updateFramebufferTextureBackingStoreFromLayer();
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ANGLE, m_texture, 0);
#elif PLATFORM(IOS_FAMILY)
    // FIXME (kbr): implement iOS path, ideally using glFramebufferTexture2DMultisample.
    // gl::BindRenderbuffer(GL_RENDERBUFFER, m_texture);
    // gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_texture);
    // setRenderbufferStorageFromDrawable(m_currentWidth, m_currentHeight);
#else
#error Unknown Cocoa platform
#endif
#else

#error Must port to non-Cocoa platforms

#endif // PLATFORM(COCOA)

    attachDepthAndStencilBufferIfNeeded(internalDepthStencilFormat, width, height);

    bool mustRestoreFBO = true;
    if (m_attrs.antialias) {
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        if (m_state.boundFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    } else {
        if (m_state.boundFBO == m_fbo)
            mustRestoreFBO = false;
    }

    return mustRestoreFBO;
}

void GraphicsContext3D::attachDepthAndStencilBufferIfNeeded(GLuint internalDepthStencilFormat, int width, int height)
{
    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth)) {
        gl::BindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
        gl::RenderbufferStorage(GL_RENDERBUFFER, internalDepthStencilFormat, width, height);
        // WebGL 1.0's rules state that combined depth/stencil renderbuffers
        // have to be attached to the synthetic DEPTH_STENCIL_ATTACHMENT point.
        if (!isGLES2Compliant() && internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES)
            gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        else {
            if (m_attrs.stencil)
                gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
            if (m_attrs.depth)
                gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        }
        gl::BindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    if (gl::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // FIXME: cleanup
        notImplemented();
    }
}

void GraphicsContext3D::resolveMultisamplingIfNecessary(const IntRect& rect)
{
    TemporaryANGLESetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryANGLESetting scopedDither(GL_DITHER, GL_FALSE);

    GLint boundFrameBuffer;
    gl::GetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFrameBuffer);
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, m_multisampleFBO);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, m_fbo);

    // FIXME: figure out more efficient solution for iOS.
    IntRect resolveRect = rect;
    // When using an ES 2.0 context, the full framebuffer must always be
    // resolved; partial blits are not allowed.
    if (!isGLES2Compliant() || rect.isEmpty())
        resolveRect = IntRect(0, 0, m_currentWidth, m_currentHeight);

    gl::BlitFramebufferANGLE(resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    gl::BindFramebuffer(GL_FRAMEBUFFER, boundFrameBuffer);
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    gl::RenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    switch (pname) {
    case MAX_TEXTURE_SIZE:
        gl::GetIntegerv(MAX_TEXTURE_SIZE, value);
        if (getExtensions().requiresRestrictedMaximumTextureSize())
            *value = std::min(4096, *value);
        break;
    case MAX_CUBE_MAP_TEXTURE_SIZE:
        gl::GetIntegerv(MAX_CUBE_MAP_TEXTURE_SIZE, value);
        if (getExtensions().requiresRestrictedMaximumTextureSize())
            *value = std::min(1024, *value);
        break;
#if PLATFORM(MAC)
    // Some older hardware advertises a larger maximum than they
    // can actually handle. Rather than detecting such devices, simply
    // clamp the maximum to 8192, which is big enough for a 5K display.
    case MAX_RENDERBUFFER_SIZE:
        gl::GetIntegerv(MAX_RENDERBUFFER_SIZE, value);
        *value = std::min(8192, *value);
        break;
    case MAX_VIEWPORT_DIMS:
        gl::GetIntegerv(MAX_VIEWPORT_DIMS, value);
        value[0] = std::min(8192, value[0]);
        value[1] = std::min(8192, value[1]);
        break;
#endif
    default:
        gl::GetIntegerv(pname, value);
    }
}

void GraphicsContext3D::getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType, GC3Dint* range, GC3Dint* precision)
{
    UNUSED_PARAM(shaderType);
    ASSERT(range);
    ASSERT(precision);

    makeContextCurrent();

    switch (precisionType) {
    case GraphicsContext3D::LOW_INT:
    case GraphicsContext3D::MEDIUM_INT:
    case GraphicsContext3D::HIGH_INT:
        // These values are for a 32-bit twos-complement integer format.
        range[0] = 31;
        range[1] = 30;
        precision[0] = 0;
        break;
    case GraphicsContext3D::LOW_FLOAT:
    case GraphicsContext3D::MEDIUM_FLOAT:
    case GraphicsContext3D::HIGH_FLOAT:
        // These values are for an IEEE single-precision floating-point format.
        range[0] = 127;
        range[1] = 127;
        precision[0] = 23;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    texImage2DDirect(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    makeContextCurrent();
    gl::DepthRangef(static_cast<float>(zNear), static_cast<float>(zFar));
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    makeContextCurrent();
    gl::ClearDepthf(static_cast<float>(depth));
}

Extensions3D& GraphicsContext3D::getExtensions()
{
    if (!m_extensions)
        m_extensions = makeUnique<Extensions3DANGLE>(this, isGLES2Compliant());
    return *m_extensions;
}

void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    makeContextCurrent();
    gl::Flush();
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
        gl::Flush();
    }
    gl::ReadPixels(x, y, width, height, format, type, data);
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO)
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);

#if PLATFORM(MAC)
    if (!m_attrs.alpha && (format == GraphicsContext3D::RGBA || format == GraphicsContext3D::BGRA) && (m_state.boundFBO == m_fbo || (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO)))
        wipeAlphaChannelFromPixels(width, height, static_cast<unsigned char*>(data));
#endif
}


// The contents of GraphicsContext3DOpenGLCommon follow, ported to use ANGLE.

void GraphicsContext3D::validateDepthStencil(const char* packedDepthStencilExtension)
{
    // Note there are no Extensions3D::ensureEnabled calls here. The ANGLE
    // backend currently assumes at a fairly deep level that
    // EGL_EXTENSIONS_ENABLED_ANGLE is set to true during context creation: for
    // the allocation of rectangular textures, etc.
    Extensions3D& extensions = getExtensions();
    if (m_attrs.stencil) {
        if (extensions.supports(packedDepthStencilExtension)) {
            // Force depth if stencil is true.
            m_attrs.depth = true;
        } else
            m_attrs.stencil = false;

    }
    if (m_attrs.antialias) {
        // FIXME: must adjust this when upgrading to WebGL 2.0 / OpenGL ES 3.0 support.
        if (!extensions.supports("GL_ANGLE_framebuffer_multisample") || !extensions.supports("GL_ANGLE_framebuffer_blit") || !extensions.supports("GL_OES_rgb8_rgba8") || isGLES2Compliant())
            m_attrs.antialias = false;
    }
}

void GraphicsContext3D::paintRenderingResultsToCanvas(ImageBuffer* imageBuffer)
{
    Checked<int, RecordOverflow> rowBytes = Checked<int, RecordOverflow>(m_currentWidth) * 4;
    if (rowBytes.hasOverflowed())
        return;

    Checked<int, RecordOverflow> totalBytesChecked = rowBytes * m_currentHeight;
    if (totalBytesChecked.hasOverflowed())
        return;
    int totalBytes = totalBytesChecked.unsafeGet();

    auto pixels = makeUniqueArray<unsigned char>(totalBytes);
    if (!pixels)
        return;

    readRenderingResults(pixels.get(), totalBytes);

    if (!m_attrs.premultipliedAlpha) {
        for (int i = 0; i < totalBytes; i += 4) {
            // Premultiply alpha.
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    }

    paintToCanvas(pixels.get(), IntSize(m_currentWidth, m_currentHeight), imageBuffer->internalSize(), imageBuffer->context());

#if PLATFORM(COCOA) && USE(OPENGL_ES)
    // FIXME: work on iOS integration.
    presentRenderbuffer();
#endif
}

bool GraphicsContext3D::paintCompositedResultsToCanvas(ImageBuffer*)
{
    // Not needed at the moment, so return that nothing was done.
    return false;
}

RefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData()
{
    // Reading premultiplied alpha would involve unpremultiplying, which is
    // lossy.
    if (m_attrs.premultipliedAlpha)
        return nullptr;

    auto imageData = ImageData::create(IntSize(m_currentWidth, m_currentHeight));
    unsigned char* pixels = imageData->data()->data();
    Checked<int, RecordOverflow> totalBytesChecked = 4 * Checked<int, RecordOverflow>(m_currentWidth) * Checked<int, RecordOverflow>(m_currentHeight);
    if (totalBytesChecked.hasOverflowed())
        return imageData;
    int totalBytes = totalBytesChecked.unsafeGet();

    readRenderingResults(pixels, totalBytes);

    // Convert to RGBA.
    for (int i = 0; i < totalBytes; i += 4)
        std::swap(pixels[i], pixels[i + 2]);

    return imageData;
}

void GraphicsContext3D::prepareTexture()
{
    if (m_layerComposited)
        return;

    makeContextCurrent();

    if (m_attrs.antialias)
        resolveMultisamplingIfNecessary();

    gl::Flush();
}

void GraphicsContext3D::readRenderingResults(unsigned char *pixels, int pixelsSize)
{
    if (pixelsSize < m_currentWidth * m_currentHeight * 4)
        return;

    makeContextCurrent();

    bool mustRestoreFBO = false;
    if (m_attrs.antialias) {
        resolveMultisamplingIfNecessary();
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
        mustRestoreFBO = true;
    } else {
        if (m_state.boundFBO != m_fbo) {
            mustRestoreFBO = true;
            gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
        }
    }

    GLint packAlignment = 4;
    bool mustRestorePackAlignment = false;
    gl::GetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    if (packAlignment > 4) {
        gl::PixelStorei(GL_PACK_ALIGNMENT, 4);
        mustRestorePackAlignment = true;
    }

    readPixelsAndConvertToBGRAIfNecessary(0, 0, m_currentWidth, m_currentHeight, pixels);

    if (mustRestorePackAlignment)
        gl::PixelStorei(GL_PACK_ALIGNMENT, packAlignment);

    if (mustRestoreFBO)
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_state.boundFBO);
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (!platformGraphicsContext3D())
        return;

    if (width == m_currentWidth && height == m_currentHeight)
        return;

    ASSERT(width >= 0 && height >= 0);
    if (width < 0 || height < 0)
        return;

    markContextChanged();

    m_currentWidth = width;
    m_currentHeight = height;

    makeContextCurrent();
    validateAttributes();

    TemporaryANGLESetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryANGLESetting scopedDither(GL_DITHER, GL_FALSE);

    bool mustRestoreFBO = reshapeFBOs(IntSize(width, height));

    // Initialize renderbuffers to 0.
    GLfloat clearColor[] = {0, 0, 0, 0}, clearDepth = 0;
    GLint clearStencil = 0;
    GLboolean colorMask[] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, depthMask = GL_TRUE;
    GLuint stencilMask = 0xffffffff, stencilMaskBack = 0xffffffff;
    GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
    gl::GetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    gl::ClearColor(0, 0, 0, 0);
    gl::GetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    gl::ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (m_attrs.depth) {
        gl::GetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        GraphicsContext3D::clearDepth(1);
        gl::GetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        gl::DepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (m_attrs.stencil) {
        gl::GetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
        gl::ClearStencil(0);
        gl::GetIntegerv(GL_STENCIL_WRITEMASK, reinterpret_cast<GLint*>(&stencilMask));
        gl::GetIntegerv(GL_STENCIL_BACK_WRITEMASK, reinterpret_cast<GLint*>(&stencilMaskBack));
        gl::StencilMaskSeparate(GL_FRONT, 0xffffffff);
        gl::StencilMaskSeparate(GL_BACK, 0xffffffff);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }

    gl::Clear(clearMask);

    gl::ClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    gl::ColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (m_attrs.depth) {
        GraphicsContext3D::clearDepth(clearDepth);
        gl::DepthMask(depthMask);
    }
    if (m_attrs.stencil) {
        gl::ClearStencil(clearStencil);
        gl::StencilMaskSeparate(GL_FRONT, stencilMask);
        gl::StencilMaskSeparate(GL_BACK, stencilMaskBack);
    }

    if (mustRestoreFBO)
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_state.boundFBO);

    gl::Flush();
}

IntSize GraphicsContext3D::getInternalFramebufferSize() const
{
    return IntSize(m_currentWidth, m_currentHeight);
}

void GraphicsContext3D::activeTexture(GC3Denum texture)
{
    makeContextCurrent();
    m_state.activeTextureUnit = texture;
    gl::ActiveTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    gl::AttachShader(program, shader);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    ASSERT(program);
    makeContextCurrent();
    gl::BindAttribLocation(program, index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    gl::BindBuffer(target, buffer);
}

void GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    GLuint fbo;
    if (buffer)
        fbo = buffer;
    else
        fbo = (m_attrs.antialias ? m_multisampleFBO : m_fbo);
    if (fbo != m_state.boundFBO) {
        gl::BindFramebuffer(target, fbo);
        m_state.boundFBO = fbo;
    }
}

void GraphicsContext3D::bindRenderbuffer(GC3Denum target, Platform3DObject renderbuffer)
{
    makeContextCurrent();
    gl::BindRenderbuffer(target, renderbuffer);
}


void GraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject texture)
{
    makeContextCurrent();
    m_state.setBoundTexture(m_state.activeTextureUnit, texture, target);
    gl::BindTexture(target, texture);
}

void GraphicsContext3D::blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    makeContextCurrent();
    gl::BlendColor(red, green, blue, alpha);
}

void GraphicsContext3D::blendEquation(GC3Denum mode)
{
    makeContextCurrent();
    gl::BlendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    makeContextCurrent();
    gl::BlendEquationSeparate(modeRGB, modeAlpha);
}


void GraphicsContext3D::blendFunc(GC3Denum sfactor, GC3Denum dfactor)
{
    makeContextCurrent();
    gl::BlendFunc(sfactor, dfactor);
}

void GraphicsContext3D::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    makeContextCurrent();
    gl::BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    makeContextCurrent();
    gl::BufferData(target, size, 0, usage);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    makeContextCurrent();
    gl::BufferData(target, size, data, usage);
}

void GraphicsContext3D::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    makeContextCurrent();
    gl::BufferSubData(target, offset, size, data);
}

void* GraphicsContext3D::mapBufferRange(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr length, GC3Dbitfield access)
{
    makeContextCurrent();
    return gl::MapBufferRange(target, offset, length, access);
}

GC3Dboolean GraphicsContext3D::unmapBuffer(GC3Denum target)
{
    makeContextCurrent();
    return gl::UnmapBuffer(target);
}

void GraphicsContext3D::copyBufferSubData(GC3Denum readTarget, GC3Denum writeTarget, GC3Dintptr readOffset, GC3Dintptr writeOffset, GC3Dsizeiptr size)
{
    makeContextCurrent();
    gl::CopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void GraphicsContext3D::getInternalformativ(GC3Denum target, GC3Denum internalformat, GC3Denum pname, GC3Dsizei bufSize, GC3Dint* params)
{
    makeContextCurrent();
    gl::GetInternalformativ(target, internalformat, pname, bufSize, params);
}

void GraphicsContext3D::renderbufferStorageMultisample(GC3Denum target, GC3Dsizei samples, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    gl::RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void GraphicsContext3D::texStorage2D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    gl::TexStorage2D(target, levels, internalformat, width, height);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::texStorage3D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth)
{
    makeContextCurrent();
    gl::TexStorage3D(target, levels, internalformat, width, height, depth);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::getActiveUniforms(Platform3DObject program, const Vector<GC3Duint>& uniformIndices, GC3Denum pname, Vector<GC3Dint>& params)
{
    ASSERT(program);
    makeContextCurrent();

    gl::GetActiveUniformsiv(program, uniformIndices.size(), uniformIndices.data(), pname, params.data());
}

GC3Denum GraphicsContext3D::checkFramebufferStatus(GC3Denum target)
{
    makeContextCurrent();
    return gl::CheckFramebufferStatus(target);
}

void GraphicsContext3D::clearColor(GC3Dclampf r, GC3Dclampf g, GC3Dclampf b, GC3Dclampf a)
{
    makeContextCurrent();
    gl::ClearColor(r, g, b, a);
}

void GraphicsContext3D::clear(GC3Dbitfield mask)
{
    makeContextCurrent();
    gl::Clear(mask);
    checkGPUStatus();
}

void GraphicsContext3D::clearStencil(GC3Dint s)
{
    makeContextCurrent();
    gl::ClearStencil(s);
}

void GraphicsContext3D::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    makeContextCurrent();
    gl::ColorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    ASSERT(shader);
    makeContextCurrent();
    gl::CompileShader(shader);
}

void GraphicsContext3D::compileShaderDirect(Platform3DObject shader)
{
    compileShader(shader);
}

void GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    makeContextCurrent();
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    }
    gl::CopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO)
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
}

void GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    }
    gl::CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO)
        gl::BindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
}

void GraphicsContext3D::cullFace(GC3Denum mode)
{
    makeContextCurrent();
    gl::CullFace(mode);
}

void GraphicsContext3D::depthFunc(GC3Denum func)
{
    makeContextCurrent();
    gl::DepthFunc(func);
}

void GraphicsContext3D::depthMask(GC3Dboolean flag)
{
    makeContextCurrent();
    gl::DepthMask(flag);
}

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    gl::DetachShader(program, shader);
}

void GraphicsContext3D::disable(GC3Denum cap)
{
    makeContextCurrent();
    gl::Disable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    gl::DisableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    makeContextCurrent();
    gl::DrawArrays(mode, first, count);
    checkGPUStatus();
}

void GraphicsContext3D::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset)
{
    makeContextCurrent();
    gl::DrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
    checkGPUStatus();
}

void GraphicsContext3D::enable(GC3Denum cap)
{
    makeContextCurrent();
    gl::Enable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    gl::EnableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    makeContextCurrent();
    gl::Finish();
}

void GraphicsContext3D::flush()
{
    makeContextCurrent();
    gl::Flush();
}

void GraphicsContext3D::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, Platform3DObject buffer)
{
    makeContextCurrent();
    gl::FramebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
}

void GraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, Platform3DObject texture, GC3Dint level)
{
    makeContextCurrent();
    gl::FramebufferTexture2D(target, attachment, textarget, texture, level);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::frontFace(GC3Denum mode)
{
    makeContextCurrent();
    gl::FrontFace(mode);
}

void GraphicsContext3D::generateMipmap(GC3Denum target)
{
    makeContextCurrent();
    gl::GenerateMipmap(target);
}

bool GraphicsContext3D::getActiveAttribImpl(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    GLint maxAttributeSize = 0;
    gl::GetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeSize);
    Vector<GLchar> name(maxAttributeSize); // GL_ACTIVE_ATTRIBUTE_MAX_LENGTH includes null termination.
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    gl::GetActiveAttrib(program, index, maxAttributeSize, &nameLength, &size, &type, name.data());
    if (!nameLength)
        return false;

    info.name = String(name.data(), nameLength);
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    return getActiveAttribImpl(program, index, info);
}

bool GraphicsContext3D::getActiveUniformImpl(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    makeContextCurrent();
    GLint maxUniformSize = 0;
    gl::GetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);

    Vector<GLchar> name(maxUniformSize); // GL_ACTIVE_UNIFORM_MAX_LENGTH includes null termination.
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    gl::GetActiveUniform(program, index, maxUniformSize, &nameLength, &size, &type, name.data());
    if (!nameLength)
        return false;

    info.name = String(name.data(), nameLength);
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    return getActiveUniformImpl(program, index, info);
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    makeContextCurrent();
    gl::GetAttachedShaders(program, maxCount, count, shaders);
}

int GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    if (!program)
        return -1;

    makeContextCurrent();

    return gl::GetAttribLocation(program, name.utf8().data());
}

int GraphicsContext3D::getAttribLocationDirect(Platform3DObject program, const String& name)
{
    return getAttribLocation(program, name);
}

GraphicsContext3DAttributes GraphicsContext3D::getContextAttributes()
{
    return m_attrs;
}

bool GraphicsContext3D::moveErrorsToSyntheticErrorList()
{
    makeContextCurrent();
    bool movedAnError = false;

    // Set an arbitrary limit of 100 here to avoid creating a hang if
    // a problem driver has a bug that causes it to never clear the error.
    // Otherwise, we would just loop until we got NO_ERROR.
    for (unsigned i = 0; i < 100; ++i) {
        GC3Denum error = gl::GetError();
        if (error == NO_ERROR)
            break;
        m_syntheticErrors.add(error);
        movedAnError = true;
    }

    return movedAnError;
}

GC3Denum GraphicsContext3D::getError()
{
    if (!m_syntheticErrors.isEmpty()) {
        // Need to move the current errors to the synthetic error list in case
        // that error is already there, since the expected behavior of both
        // glGetError and getError is to only report each error code once.
        moveErrorsToSyntheticErrorList();
        return m_syntheticErrors.takeFirst();
    }

    makeContextCurrent();
    return gl::GetError();
}

String GraphicsContext3D::getString(GC3Denum name)
{
    makeContextCurrent();
    return String(reinterpret_cast<const char*>(gl::GetString(name)));
}

void GraphicsContext3D::hint(GC3Denum target, GC3Denum mode)
{
    makeContextCurrent();
    gl::Hint(target, mode);
}

GC3Dboolean GraphicsContext3D::isBuffer(Platform3DObject buffer)
{
    if (!buffer)
        return GL_FALSE;

    makeContextCurrent();
    return gl::IsBuffer(buffer);
}

GC3Dboolean GraphicsContext3D::isEnabled(GC3Denum cap)
{
    makeContextCurrent();
    return gl::IsEnabled(cap);
}

GC3Dboolean GraphicsContext3D::isFramebuffer(Platform3DObject framebuffer)
{
    if (!framebuffer)
        return GL_FALSE;

    makeContextCurrent();
    return gl::IsFramebuffer(framebuffer);
}

GC3Dboolean GraphicsContext3D::isProgram(Platform3DObject program)
{
    if (!program)
        return GL_FALSE;

    makeContextCurrent();
    return gl::IsProgram(program);
}

GC3Dboolean GraphicsContext3D::isRenderbuffer(Platform3DObject renderbuffer)
{
    if (!renderbuffer)
        return GL_FALSE;

    makeContextCurrent();
    return gl::IsRenderbuffer(renderbuffer);
}

GC3Dboolean GraphicsContext3D::isShader(Platform3DObject shader)
{
    if (!shader)
        return GL_FALSE;

    makeContextCurrent();
    return gl::IsShader(shader);
}

GC3Dboolean GraphicsContext3D::isTexture(Platform3DObject texture)
{
    if (!texture)
        return GL_FALSE;

    makeContextCurrent();
    return gl::IsTexture(texture);
}

void GraphicsContext3D::lineWidth(GC3Dfloat width)
{
    makeContextCurrent();
    gl::LineWidth(width);
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    ASSERT(program);
    makeContextCurrent();
    gl::LinkProgram(program);
}

void GraphicsContext3D::pixelStorei(GC3Denum pname, GC3Dint param)
{
    makeContextCurrent();
    gl::PixelStorei(pname, param);
}

void GraphicsContext3D::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    makeContextCurrent();
    gl::PolygonOffset(factor, units);
}

void GraphicsContext3D::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    makeContextCurrent();
    gl::SampleCoverage(value, invert);
}

void GraphicsContext3D::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    gl::Scissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& string)
{
    ASSERT(shader);

    makeContextCurrent();

    const CString& shaderSourceCString = string.utf8();
    const char* shaderSourcePtr = shaderSourceCString.data();
    int shaderSourceLength = shaderSourceCString.length();
    gl::ShaderSource(shader, 1, &shaderSourcePtr, &shaderSourceLength);
}

void GraphicsContext3D::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    gl::StencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    gl::StencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(GC3Duint mask)
{
    makeContextCurrent();
    gl::StencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    makeContextCurrent();
    gl::StencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    makeContextCurrent();
    gl::StencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    makeContextCurrent();
    gl::StencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContext3D::texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat value)
{
    makeContextCurrent();
    gl::TexParameterf(target, pname, value);
}

void GraphicsContext3D::texParameteri(GC3Denum target, GC3Denum pname, GC3Dint value)
{
    makeContextCurrent();
    gl::TexParameteri(target, pname, value);
}

void GraphicsContext3D::uniform1f(GC3Dint location, GC3Dfloat v0)
{
    makeContextCurrent();
    gl::Uniform1f(location, v0);
}

void GraphicsContext3D::uniform1fv(GC3Dint location, GC3Dsizei size, const GC3Dfloat* array)
{
    makeContextCurrent();
    gl::Uniform1fv(location, size, array);
}

void GraphicsContext3D::uniform2f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1)
{
    makeContextCurrent();
    gl::Uniform2f(location, v0, v1);
}

void GraphicsContext3D::uniform2fv(GC3Dint location, GC3Dsizei size, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 2.
    makeContextCurrent();
    gl::Uniform2fv(location, size, array);
}

void GraphicsContext3D::uniform3f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2)
{
    makeContextCurrent();
    gl::Uniform3f(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3fv(GC3Dint location, GC3Dsizei size, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 3.
    makeContextCurrent();
    gl::Uniform3fv(location, size, array);
}

void GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2, GC3Dfloat v3)
{
    makeContextCurrent();
    gl::Uniform4f(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4fv(GC3Dint location, GC3Dsizei size, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    gl::Uniform4fv(location, size, array);
}

void GraphicsContext3D::uniform1i(GC3Dint location, GC3Dint v0)
{
    makeContextCurrent();
    gl::Uniform1i(location, v0);
}

void GraphicsContext3D::uniform1iv(GC3Dint location, GC3Dsizei size, const GC3Dint* array)
{
    makeContextCurrent();
    gl::Uniform1iv(location, size, array);
}

void GraphicsContext3D::uniform2i(GC3Dint location, GC3Dint v0, GC3Dint v1)
{
    makeContextCurrent();
    gl::Uniform2i(location, v0, v1);
}

void GraphicsContext3D::uniform2iv(GC3Dint location, GC3Dsizei size, const GC3Dint* array)
{
    // FIXME: length needs to be a multiple of 2.
    makeContextCurrent();
    gl::Uniform2iv(location, size, array);
}

void GraphicsContext3D::uniform3i(GC3Dint location, GC3Dint v0, GC3Dint v1, GC3Dint v2)
{
    makeContextCurrent();
    gl::Uniform3i(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3iv(GC3Dint location, GC3Dsizei size, const GC3Dint* array)
{
    // FIXME: length needs to be a multiple of 3.
    makeContextCurrent();
    gl::Uniform3iv(location, size, array);
}

void GraphicsContext3D::uniform4i(GC3Dint location, GC3Dint v0, GC3Dint v1, GC3Dint v2, GC3Dint v3)
{
    makeContextCurrent();
    gl::Uniform4i(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4iv(GC3Dint location, GC3Dsizei size, const GC3Dint* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    gl::Uniform4iv(location, size, array);
}

void GraphicsContext3D::uniformMatrix2fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    gl::UniformMatrix2fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix3fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 9.
    makeContextCurrent();
    gl::UniformMatrix3fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix4fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 16.
    makeContextCurrent();
    gl::UniformMatrix4fv(location, size, transpose, array);
}

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    makeContextCurrent();
    gl::UseProgram(program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    ASSERT(program);

    makeContextCurrent();
    gl::ValidateProgram(program);
}

void GraphicsContext3D::vertexAttrib1f(GC3Duint index, GC3Dfloat v0)
{
    makeContextCurrent();
    gl::VertexAttrib1f(index, v0);
}

void GraphicsContext3D::vertexAttrib1fv(GC3Duint index, const GC3Dfloat* array)
{
    makeContextCurrent();
    gl::VertexAttrib1fv(index, array);
}

void GraphicsContext3D::vertexAttrib2f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1)
{
    makeContextCurrent();
    gl::VertexAttrib2f(index, v0, v1);
}

void GraphicsContext3D::vertexAttrib2fv(GC3Duint index, const GC3Dfloat* array)
{
    makeContextCurrent();
    gl::VertexAttrib2fv(index, array);
}

void GraphicsContext3D::vertexAttrib3f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2)
{
    makeContextCurrent();
    gl::VertexAttrib3f(index, v0, v1, v2);
}

void GraphicsContext3D::vertexAttrib3fv(GC3Duint index, const GC3Dfloat* array)
{
    makeContextCurrent();
    gl::VertexAttrib3fv(index, array);
}

void GraphicsContext3D::vertexAttrib4f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2, GC3Dfloat v3)
{
    makeContextCurrent();
    gl::VertexAttrib4f(index, v0, v1, v2, v3);
}

void GraphicsContext3D::vertexAttrib4fv(GC3Duint index, const GC3Dfloat* array)
{
    makeContextCurrent();
    gl::VertexAttrib4fv(index, array);
}

void GraphicsContext3D::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized, GC3Dsizei stride, GC3Dintptr offset)
{
    makeContextCurrent();
    gl::VertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    gl::Viewport(x, y, width, height);
}

Platform3DObject GraphicsContext3D::createVertexArray()
{
    makeContextCurrent();
    GLuint array = 0;
    gl::GenVertexArrays(1, &array);
    return array;
}

void GraphicsContext3D::deleteVertexArray(Platform3DObject array)
{
    if (!array)
        return;
    makeContextCurrent();
    gl::DeleteVertexArrays(1, &array);
}

GC3Dboolean GraphicsContext3D::isVertexArray(Platform3DObject array)
{
    if (!array)
        return GL_FALSE;
    makeContextCurrent();
    return gl::IsVertexArray(array);
}

void GraphicsContext3D::bindVertexArray(Platform3DObject array)
{
    makeContextCurrent();
    gl::BindVertexArray(array);
}

void GraphicsContext3D::getBooleanv(GC3Denum pname, GC3Dboolean* value)
{
    makeContextCurrent();
    gl::GetBooleanv(pname, value);
}

void GraphicsContext3D::getBufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    gl::GetBufferParameteriv(target, pname, value);
}

void GraphicsContext3D::getFloatv(GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    gl::GetFloatv(pname, value);
}
    
void GraphicsContext3D::getInteger64v(GC3Denum pname, GC3Dint64* value)
{
    UNUSED_PARAM(pname);
    makeContextCurrent();
    *value = 0;
    // FIXME 141178: Before enabling this we must first switch over to using gl3.h and creating and initialing the WebGL2 context using OpenGL ES 3.0.
    // gl::GetInteger64v(pname, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    gl::GetFramebufferAttachmentParameteriv(target, attachment, pname, value);
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    gl::GetProgramiv(program, pname, value);
}

String GraphicsContext3D::getUnmangledInfoLog(Platform3DObject shaders[2], GC3Dsizei count, const String& log)
{
    UNUSED_PARAM(shaders);
    UNUSED_PARAM(count);
    LOG(WebGL, "Original ShaderInfoLog:\n%s", log.utf8().data());

    StringBuilder processedLog;

    // ANGLE inserts a "#extension" line into the shader source that
    // causes a warning in some compilers. There is no point showing
    // this warning to the user since they didn't write the code that
    // is causing it.
    static const NeverDestroyed<String> angleWarning { "WARNING: 0:1: extension 'GL_ARB_gpu_shader5' is not supported\n"_s };
    int startFrom = log.startsWith(angleWarning) ? angleWarning.get().length() : 0;
    processedLog.append(log.substring(startFrom, log.length() - startFrom));

    LOG(WebGL, "Unmangled ShaderInfoLog:\n%s", processedLog.toString().utf8().data());
    return processedLog.toString();
}

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    ASSERT(program);

    makeContextCurrent();
    GLint length = 0;
    gl::GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return String(); 

    GLsizei size = 0;
    Vector<GLchar> info(length);
    gl::GetProgramInfoLog(program, length, &size, info.data());

    GC3Dsizei count;
    Platform3DObject shaders[2];
    getAttachedShaders(program, 2, &count, shaders);

    return getUnmangledInfoLog(shaders, count, String(info.data(), size));
}

void GraphicsContext3D::getRenderbufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    gl::GetRenderbufferParameteriv(target, pname, value);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, GC3Denum pname, GC3Dint* value)
{
    ASSERT(shader);

    makeContextCurrent();
    gl::GetShaderiv(shader, pname, value);
}

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    ASSERT(shader);

    makeContextCurrent();
    GLint length = 0;
    gl::GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return String(); 

    GLsizei size = 0;
    Vector<GLchar> info(length);
    gl::GetShaderInfoLog(shader, length, &size, info.data());

    Platform3DObject shaders[2] = { shader, 0 };
    return getUnmangledInfoLog(shaders, 1, String(info.data(), size));
}

void GraphicsContext3D::getTexParameterfv(GC3Denum target, GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    gl::GetTexParameterfv(target, pname, value);
}

void GraphicsContext3D::getTexParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    gl::GetTexParameteriv(target, pname, value);
}

void GraphicsContext3D::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    makeContextCurrent();
    gl::GetUniformfv(program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    makeContextCurrent();
    gl::GetUniformiv(program, location, value);
}

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    ASSERT(program);

    makeContextCurrent();
    return gl::GetUniformLocation(program, name.utf8().data());
}

void GraphicsContext3D::getVertexAttribfv(GC3Duint index, GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    gl::GetVertexAttribfv(index, pname, value);
}

void GraphicsContext3D::getVertexAttribiv(GC3Duint index, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    gl::GetVertexAttribiv(index, pname, value);
}

GC3Dsizeiptr GraphicsContext3D::getVertexAttribOffset(GC3Duint index, GC3Denum pname)
{
    makeContextCurrent();

    GLvoid* pointer = 0;
    gl::GetVertexAttribPointerv(index, pname, &pointer);
    return static_cast<GC3Dsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

void GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoff, GC3Dint yoff, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();

    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size.
    gl::TexSubImage2D(target, level, xoff, yoff, width, height, format, type, pixels);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, const void* data)
{
    makeContextCurrent();
    gl::CompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Dsizei imageSize, const void* data)
{
    makeContextCurrent();
    gl::CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

Platform3DObject GraphicsContext3D::createBuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    gl::GenBuffers(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createFramebuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    gl::GenFramebuffers(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createProgram()
{
    makeContextCurrent();
    return gl::CreateProgram();
}

Platform3DObject GraphicsContext3D::createRenderbuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    gl::GenRenderbuffers(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createShader(GC3Denum type)
{
    makeContextCurrent();
    return gl::CreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

Platform3DObject GraphicsContext3D::createTexture()
{
    makeContextCurrent();
    GLuint o = 0;
    gl::GenTextures(1, &o);
    m_state.textureSeedCount.add(o);
    return o;
}

void GraphicsContext3D::deleteBuffer(Platform3DObject buffer)
{
    makeContextCurrent();
    gl::DeleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(Platform3DObject framebuffer)
{
    makeContextCurrent();
    if (framebuffer == m_state.boundFBO) {
        // Make sure the framebuffer is not going to be used for drawing
        // operations after it gets deleted.
        bindFramebuffer(FRAMEBUFFER, 0);
    }
    gl::DeleteFramebuffers(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(Platform3DObject program)
{
    makeContextCurrent();
    gl::DeleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(Platform3DObject renderbuffer)
{
    makeContextCurrent();
    gl::DeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(Platform3DObject shader)
{
    makeContextCurrent();
    gl::DeleteShader(shader);
}

void GraphicsContext3D::deleteTexture(Platform3DObject texture)
{
    makeContextCurrent();
    m_state.boundTextureMap.removeIf([texture] (auto& keyValue) {
        return keyValue.value.first == texture;
    });
    gl::DeleteTextures(1, &texture);
    m_state.textureSeedCount.removeAll(texture);
}

void GraphicsContext3D::synthesizeGLError(GC3Denum error)
{
    // Need to move the current errors to the synthetic error list to
    // preserve the order of errors, so a caller to getError will get
    // any errors from gl::Error before the error we are synthesizing.
    moveErrorsToSyntheticErrorList();
    m_syntheticErrors.add(error);
}

void GraphicsContext3D::markContextChanged()
{
    m_layerComposited = false;
}

void GraphicsContext3D::markLayerComposited()
{
    m_layerComposited = true;

    for (auto* client : copyToVector(m_clients))
        client->didComposite();
}

bool GraphicsContext3D::layerComposited() const
{
    return m_layerComposited;
}

void GraphicsContext3D::forceContextLost()
{
    for (auto* client : copyToVector(m_clients))
        client->forceContextLost();
}

void GraphicsContext3D::recycleContext()
{
    for (auto* client : copyToVector(m_clients))
        client->recycleContext();
}

void GraphicsContext3D::dispatchContextChangedNotification()
{
    for (auto* client : copyToVector(m_clients))
        client->dispatchContextChangedNotification();
}

void GraphicsContext3D::texImage2DDirect(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();
    gl::TexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount)
{
    getExtensions().drawArraysInstanced(mode, first, count, primcount);
    checkGPUStatus();
}

void GraphicsContext3D::drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset, GC3Dsizei primcount)
{
    getExtensions().drawElementsInstanced(mode, count, type, offset, primcount);
    checkGPUStatus();
}

void GraphicsContext3D::vertexAttribDivisor(GC3Duint index, GC3Duint divisor)
{
    getExtensions().vertexAttribDivisor(index, divisor);
}

}

#endif // ENABLE(GRAPHICS_CONTEXT_3D) && USE(ANGLE)
