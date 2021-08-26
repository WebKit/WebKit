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

#if ENABLE(WEBGL) && USE(ANGLE)
#include "GraphicsContextGL.h"

#include "ANGLEHeaders.h"
#include "ExtensionsGLANGLE.h"
#include "GraphicsContextGLANGLEUtilities.h"
#include "GraphicsContextGLOpenGL.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PixelBuffer.h"
#include "TemporaryANGLESetting.h"
#include <JavaScriptCore/RegularExpression.h>
#include <algorithm>
#include <cstring>
#include <wtf/HexNumber.h>
#include <wtf/Seconds.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>


#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#include "GraphicsContextGLCVANGLE.h"
#endif

// This one definition short-circuits the need for gl2ext.h, which
// would need more work to be included from WebCore.
#define GL_MAX_SAMPLES_EXT 0x8D57

namespace WebCore {

static const char* packedDepthStencilExtensionName = "GL_OES_packed_depth_stencil";

static Seconds maxFrameDuration = 5_s;

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
static void wipeAlphaChannelFromPixels(int width, int height, unsigned char* pixels)
{
    // We can assume this doesn't overflow because the calling functions
    // use checked arithmetic.
    int totalBytes = width * height * 4;
    for (int i = 0; i < totalBytes; i += 4)
        pixels[i + 3] = 255;
}
#endif

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readPixelsForPaintResults()
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = PixelBuffer::tryCreate(format, getInternalFramebufferSize());
    if (!pixelBuffer)
        return std::nullopt;
    ScopedPixelStorageMode packAlignment(GL_PACK_ALIGNMENT);
    if (packAlignment > 4)
        packAlignment.pixelStore(4);
    ScopedPixelStorageMode packRowLength(GL_PACK_ROW_LENGTH, 0, m_isForWebGL2);
    ScopedPixelStorageMode packSkipRows(GL_PACK_SKIP_ROWS, 0, m_isForWebGL2);
    ScopedPixelStorageMode packSkipPixels(GL_PACK_SKIP_PIXELS, 0, m_isForWebGL2);
    ScopedBufferBinding scopedPixelPackBufferReset(GL_PIXEL_PACK_BUFFER, 0, m_isForWebGL2);

    gl::ReadnPixelsRobustANGLE(0, 0, pixelBuffer->size().width(), pixelBuffer->size().height(), GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer->data().byteLength(), nullptr, nullptr, nullptr, pixelBuffer->data().data());
    // FIXME: Rendering to GL_RGB textures with a IOSurface bound to the texture image leaves
    // the alpha in the IOSurface in incorrect state. Also ANGLE gl::ReadPixels will in some
    // cases expose the non-255 values.
    // https://bugs.webkit.org/show_bug.cgi?id=215804
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    if (!contextAttributes().alpha)
        wipeAlphaChannelFromPixels(pixelBuffer->size().width(), pixelBuffer->size().height(), pixelBuffer->data().data());
#endif
    return pixelBuffer;
}

void GraphicsContextGLOpenGL::validateAttributes()
{
    m_internalColorFormat = contextAttributes().alpha ? GL_RGBA8 : GL_RGB8;

    validateDepthStencil(packedDepthStencilExtensionName);
}

bool GraphicsContextGLOpenGL::reshapeFBOs(const IntSize& size)
{
    auto attrs = contextAttributes();
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = attrs.alpha ? GL_RGBA : GL_RGB;

    // Resize multisample FBO.
    if (attrs.antialias) {
        GLint maxSampleCount;
        gl::GetIntegerv(GL_MAX_SAMPLES_ANGLE, &maxSampleCount);
        // Using more than 4 samples is slow on some hardware and is unlikely to
        // produce a significantly better result.
        GLint sampleCount = std::min(4, maxSampleCount);
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        gl::BindRenderbuffer(GL_RENDERBUFFER, m_multisampleColorBuffer);
        gl::RenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, m_internalColorFormat, width, height);
        gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_multisampleColorBuffer);
        if (attrs.stencil || attrs.depth) {
            gl::BindRenderbuffer(GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            gl::RenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, m_internalDepthStencilFormat, width, height);
            // WebGL 1.0's rules state that combined depth/stencil renderbuffers
            // have to be attached to the synthetic DEPTH_STENCIL_ATTACHMENT point.
            if (!isGLES2Compliant() && m_internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES)
                gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            else {
                if (attrs.stencil)
                    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
                if (attrs.depth)
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

#if PLATFORM(COCOA)
    if (!reshapeDisplayBufferBacking()) {
        RELEASE_LOG(WebGL, "Fatal: Unable to allocate backing store of size %d x %d", width, height);
        forceContextLost();
        return true;
    }
    if (m_preserveDrawingBufferTexture) {
        // The context requires the use of an intermediate texture in order to implement
        // preserveDrawingBuffer:true without antialiasing.
        GLint texture2DBinding = 0;
        gl::GetIntegerv(GL_TEXTURE_BINDING_2D, &texture2DBinding);
        gl::BindTexture(GL_TEXTURE_2D, m_preserveDrawingBufferTexture);
        // Note that any pixel unpack buffer was unbound earlier, in reshape().
        gl::TexImage2D(GL_TEXTURE_2D, 0, colorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        // m_fbo is bound at this point.
        gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_preserveDrawingBufferTexture, 0);
        gl::BindTexture(GL_TEXTURE_2D, texture2DBinding);
        // Attach m_texture to m_preserveDrawingBufferFBO for later blitting.
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_preserveDrawingBufferFBO);
        gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    } else
        gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);
#else
    GLenum textureTarget = drawingBufferTextureTarget();
    GLuint internalColorFormat = textureTarget == GL_TEXTURE_2D ? colorFormat : m_internalColorFormat;
    gl::BindTexture(textureTarget, m_texture);
    gl::TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget, m_texture, 0);
#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture) {
        gl::BindTexture(textureTarget, m_compositorTexture);
        gl::TexImage2D(textureTarget, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        gl::BindTexture(textureTarget, 0);
        gl::BindTexture(textureTarget, m_intermediateTexture);
        gl::TexImage2D(textureTarget, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        gl::BindTexture(textureTarget, 0);
    }
#endif
#endif // PLATFORM(COCOA)

    attachDepthAndStencilBufferIfNeeded(m_internalDepthStencilFormat, width, height);

    bool mustRestoreFBO = true;
    if (attrs.antialias) {
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        if (m_state.boundDrawFBO == m_multisampleFBO && m_state.boundReadFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    } else {
        if (m_state.boundDrawFBO == m_fbo && m_state.boundReadFBO == m_fbo)
            mustRestoreFBO = false;
    }

    return mustRestoreFBO;
}

void GraphicsContextGLOpenGL::attachDepthAndStencilBufferIfNeeded(GLuint internalDepthStencilFormat, int width, int height)
{
    auto attrs = contextAttributes();

    if (!attrs.antialias && (attrs.stencil || attrs.depth)) {
        gl::BindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
        gl::RenderbufferStorage(GL_RENDERBUFFER, internalDepthStencilFormat, width, height);
        // WebGL 1.0's rules state that combined depth/stencil renderbuffers
        // have to be attached to the synthetic DEPTH_STENCIL_ATTACHMENT point.
        if (!isGLES2Compliant() && internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES)
            gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        else {
            if (attrs.stencil)
                gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
            if (attrs.depth)
                gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        }
        gl::BindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    if (gl::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // FIXME: cleanup
        notImplemented();
    }
}

void GraphicsContextGLOpenGL::resolveMultisamplingIfNecessary(const IntRect& rect)
{
    TemporaryANGLESetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryANGLESetting scopedDither(GL_DITHER, GL_FALSE);

    GLint boundFrameBuffer = 0;
    GLint boundReadFrameBuffer = 0;
    if (m_isForWebGL2) {
        gl::GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundFrameBuffer);
        gl::GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &boundReadFrameBuffer);
    } else
        gl::GetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFrameBuffer);
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, m_multisampleFBO);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, m_fbo);

    // FIXME: figure out more efficient solution for iOS.
    if (m_isForWebGL2) {
        // ES 3.0 has BlitFramebuffer.
        IntRect resolveRect = rect.isEmpty() ? IntRect { 0, 0, m_currentWidth, m_currentHeight } : rect;
        gl::BlitFramebuffer(resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
        // ES 2.0 has BlitFramebufferANGLE only.
        gl::BlitFramebufferANGLE(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    if (m_isForWebGL2) {
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, boundFrameBuffer);
        gl::BindFramebuffer(GL_READ_FRAMEBUFFER, boundReadFrameBuffer);
    } else
        gl::BindFramebuffer(GL_FRAMEBUFFER, boundFrameBuffer);
}

void GraphicsContextGLOpenGL::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    gl::RenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContextGLOpenGL::getIntegerv(GCGLenum pname, GCGLSpan<GCGLint> value)
{
    if (!makeContextCurrent())
        return;
    gl::GetIntegervRobustANGLE(pname, value.bufSize, nullptr, value.data);
}

void GraphicsContextGLOpenGL::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLSpan<GCGLint, 2> range, GCGLint* precision)
{
    if (!makeContextCurrent())
        return;

    gl::GetShaderPrecisionFormat(shaderType, precisionType, range.data, precision);
}

void GraphicsContextGLOpenGL::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!m_isForWebGL2)
        internalformat = static_cast<ExtensionsGLANGLE&>(getExtensions()).adjustWebGL1TextureInternalFormat(internalformat, format, type);
    if (!makeContextCurrent())
        return;
    gl::TexImage2DRobustANGLE(target, level, internalformat, width, height, border, format, type, pixels.bufSize, pixels.data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
    }

void GraphicsContextGLOpenGL::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    texImage2D(target, level, internalformat, width, height, border, format, type, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLOpenGL::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoff, GCGLint yoff, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!makeContextCurrent())
        return;

    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size.
    gl::TexSubImage2DRobustANGLE(target, level, xoff, yoff, width, height, format, type, pixels.bufSize, pixels.data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoff, GCGLint yoff, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    texSubImage2D(target, level, xoff, yoff, width, height, format, type, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLOpenGL::compressedTexImage2D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, int border, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    gl::CompressedTexImage2DRobustANGLE(target, level, internalformat, width, height, border, imageSize, data.bufSize, data.data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::compressedTexImage2D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, int border, GCGLsizei imageSize, GCGLintptr offset)
{
    compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLOpenGL::compressedTexSubImage2D(GCGLenum target, int level, int xoffset, int yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    gl::CompressedTexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width, height, format, imageSize, data.bufSize, data.data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::compressedTexSubImage2D(GCGLenum target, int level, int xoffset, int yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLOpenGL::depthRange(GCGLclampf zNear, GCGLclampf zFar)
{
    if (!makeContextCurrent())
        return;

    gl::DepthRangef(static_cast<float>(zNear), static_cast<float>(zFar));
}

void GraphicsContextGLOpenGL::clearDepth(GCGLclampf depth)
{
    if (!makeContextCurrent())
        return;

    gl::ClearDepthf(static_cast<float>(depth));
}

ExtensionsGL& GraphicsContextGLOpenGL::getExtensions()
{
    if (!m_extensions)
        m_extensions = makeUnique<ExtensionsGLANGLE>(this);
    return *m_extensions;
}

void GraphicsContextGLOpenGL::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<GCGLvoid> data)
{
    readnPixelsImpl(x, y, width, height, format, type, data.bufSize, nullptr, nullptr, nullptr, data.data, false);
}

void GraphicsContextGLOpenGL::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    readnPixelsImpl(x, y, width, height, format, type, 0, nullptr, nullptr, nullptr, reinterpret_cast<GCGLvoid*>(offset), true);
}

void GraphicsContextGLOpenGL::readnPixelsImpl(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, GCGLsizei* length, GCGLsizei* columns, GCGLsizei* rows, GCGLvoid* data, bool readingToPixelBufferObject)
{
    if (!makeContextCurrent())
        return;

    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    gl::Flush();
    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        gl::BindFramebuffer(framebufferTarget, m_fbo);
        gl::Flush();
    }
    moveErrorsToSyntheticErrorList();
    gl::ReadnPixelsRobustANGLE(x, y, width, height, format, type, bufSize, length, columns, rows, data);
    GLenum error = gl::GetError();
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        gl::BindFramebuffer(framebufferTarget, m_multisampleFBO);

    if (error) {
        // ANGLE detected a failure during the ReadnPixelsRobustANGLE operation. Surface this in the
        // synthetic error list, and skip the alpha channel fixup below.
        synthesizeGLError(error);
        return;
    }


#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    if (!readingToPixelBufferObject && !attrs.alpha && (format == GraphicsContextGL::RGBA || format == GraphicsContextGL::BGRA) && (type == GraphicsContextGL::UNSIGNED_BYTE) && (m_state.boundReadFBO == m_fbo || (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)))
        wipeAlphaChannelFromPixels(width, height, static_cast<unsigned char*>(data));
#else
    UNUSED_PARAM(readingToPixelBufferObject);
#endif
}

// The contents of GraphicsContextGLOpenGLCommon follow, ported to use ANGLE.

void GraphicsContextGLOpenGL::validateDepthStencil(const char* packedDepthStencilExtension)
{
    ExtensionsGL& extensions = getExtensions();
    // FIXME: Since the constructors of various platforms are not shared, we initialize this here.
    // Upon constructing the context, always initialize the extensions that the WebGLRenderingContext* will
    // use to turn on feature flags.
    if (extensions.supports(packedDepthStencilExtension)) {
        extensions.ensureEnabled(packedDepthStencilExtension);
        m_internalDepthStencilFormat = GL_DEPTH24_STENCIL8_OES;
    } else
        m_internalDepthStencilFormat = GL_DEPTH_COMPONENT16;

    auto attrs = contextAttributes();
    if (attrs.stencil) {
        if (m_internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES) {
            // Force depth if stencil is true.
            attrs.depth = true;
        } else
            attrs.stencil = false;
        setContextAttributes(attrs);
    }

    if (attrs.antialias) {
        // FIXME: must adjust this when upgrading to WebGL 2.0 / OpenGL ES 3.0 support.
        if (!extensions.supports("GL_ANGLE_framebuffer_multisample") || !extensions.supports("GL_ANGLE_framebuffer_blit") || !extensions.supports("GL_OES_rgb8_rgba8")) {
            attrs.antialias = false;
            setContextAttributes(attrs);
        } else {
            extensions.ensureEnabled("GL_ANGLE_framebuffer_multisample");
            extensions.ensureEnabled("GL_ANGLE_framebuffer_blit");
            extensions.ensureEnabled("GL_OES_rgb8_rgba8");
        }
    } else if (attrs.preserveDrawingBuffer) {
        // Needed for preserveDrawingBuffer:true support without antialiasing.
        extensions.ensureEnabled("GL_ANGLE_framebuffer_blit");
    }
}

void GraphicsContextGLOpenGL::prepareTexture()
{
    if (m_layerComposited)
        return;

    if (!makeContextCurrent())
        return;

    prepareTextureImpl();
}

void GraphicsContextGLOpenGL::prepareTextureImpl()
{
    ASSERT(!m_layerComposited);

    if (contextAttributes().antialias)
        resolveMultisamplingIfNecessary();

#if USE(COORDINATED_GRAPHICS)
    std::swap(m_texture, m_compositorTexture);
    std::swap(m_texture, m_intermediateTexture);
    gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ANGLE, m_texture, 0);
    gl::Flush();

    if (m_state.boundDrawFBO != m_fbo)
        gl::BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
    else
        gl::BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_fbo);
#else
    if (m_preserveDrawingBufferTexture) {
        // Blit m_preserveDrawingBufferTexture into m_texture.
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, m_preserveDrawingBufferFBO);
        gl::BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, m_fbo);
        gl::BlitFramebufferANGLE(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Note: it's been observed that BlitFramebuffer may destroy the alpha channel of the
        // destination texture if it's an RGB texture bound to an IOSurface. This wasn't observable
        // through the WebGL conformance tests, but it may be necessary to save and restore the
        // color mask and clear color, and use the color mask to clear the alpha channel of the
        // destination texture to 1.0.

        // Restore user's framebuffer bindings.
        if (m_isForWebGL2) {
            gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_state.boundDrawFBO);
            gl::BindFramebuffer(GL_READ_FRAMEBUFFER, m_state.boundReadFBO);
        } else
            gl::BindFramebuffer(GL_FRAMEBUFFER, m_state.boundDrawFBO);
    }
#endif
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readRenderingResults()
{
    ScopedRestoreReadFramebufferBinding fboBinding(m_isForWebGL2, m_state.boundReadFBO);
    if (contextAttributes().antialias) {
        resolveMultisamplingIfNecessary();
        fboBinding.markBindingChanged();
    }
    fboBinding.bindFramebuffer(m_fbo);
    return readPixelsForPaintResults();
}

void GraphicsContextGLOpenGL::reshape(int width, int height)
{
    if (width == m_currentWidth && height == m_currentHeight)
        return;

    ASSERT(width >= 0 && height >= 0);
    if (width < 0 || height < 0)
        return;

    if (!makeContextCurrent())
        return;

    // FIXME: these may call makeContextCurrent again, we need to do this before changing the size.
    moveErrorsToSyntheticErrorList();
    validateAttributes();

    markContextChanged();

    m_currentWidth = width;
    m_currentHeight = height;

    TemporaryANGLESetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryANGLESetting scopedDither(GL_DITHER, GL_FALSE);
    ScopedBufferBinding scopedPixelUnpackBufferReset(GL_PIXEL_UNPACK_BUFFER, 0, m_isForWebGL2);

    bool mustRestoreFBO = reshapeFBOs(IntSize(width, height));
    auto attrs = contextAttributes();

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
    if (attrs.depth) {
        gl::GetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        gl::ClearDepthf(1.0f);
        gl::GetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        gl::DepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }

    if (attrs.stencil) {
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
    if (attrs.depth) {
        gl::ClearDepthf(clearDepth);
        gl::DepthMask(depthMask);
    }
    if (attrs.stencil) {
        gl::ClearStencil(clearStencil);
        gl::StencilMaskSeparate(GL_FRONT, stencilMask);
        gl::StencilMaskSeparate(GL_BACK, stencilMaskBack);
    }

    if (mustRestoreFBO) {
        gl::BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
        if (m_isForWebGL2 && m_state.boundDrawFBO != m_state.boundReadFBO)
            gl::BindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, m_state.boundReadFBO);
    }

    auto error = gl::GetError();
    if (error != GL_NO_ERROR) {
        RELEASE_LOG(WebGL, "Fatal: OpenGL error during GraphicsContextGL buffer initialization (%d).", error);
        forceContextLost();
        return;
    }

    gl::Flush();
}

void GraphicsContextGLOpenGL::activeTexture(GCGLenum texture)
{
    if (!makeContextCurrent())
        return;

    m_state.activeTextureUnit = texture;
    gl::ActiveTexture(texture);
}

void GraphicsContextGLOpenGL::attachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    gl::AttachShader(program, shader);
}

void GraphicsContextGLOpenGL::bindAttribLocation(PlatformGLObject program, GCGLuint index, const String& name)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    gl::BindAttribLocation(program, index, name.utf8().data());
}

void GraphicsContextGLOpenGL::bindBuffer(GCGLenum target, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    gl::BindBuffer(target, buffer);
}

void GraphicsContextGLOpenGL::bindFramebuffer(GCGLenum target, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GLuint fbo;
    if (buffer)
        fbo = buffer;
    else
        fbo = (contextAttributes().antialias ? m_multisampleFBO : m_fbo);

    gl::BindFramebuffer(target, fbo);
    if (target == GL_FRAMEBUFFER) {
        m_state.boundReadFBO = m_state.boundDrawFBO = fbo;
    } else if (target == GL_READ_FRAMEBUFFER) {
        m_state.boundReadFBO = fbo;
    } else if (target == GL_DRAW_FRAMEBUFFER) {
        m_state.boundDrawFBO = fbo;
    }
}

void GraphicsContextGLOpenGL::bindRenderbuffer(GCGLenum target, PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;

    gl::BindRenderbuffer(target, renderbuffer);
}


void GraphicsContextGLOpenGL::bindTexture(GCGLenum target, PlatformGLObject texture)
{
    if (!makeContextCurrent())
        return;

    m_state.setBoundTexture(m_state.activeTextureUnit, texture, target);
    gl::BindTexture(target, texture);
}

void GraphicsContextGLOpenGL::blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha)
{
    if (!makeContextCurrent())
        return;

    gl::BlendColor(red, green, blue, alpha);
}

void GraphicsContextGLOpenGL::blendEquation(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    gl::BlendEquation(mode);
}

void GraphicsContextGLOpenGL::blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (!makeContextCurrent())
        return;

    gl::BlendEquationSeparate(modeRGB, modeAlpha);
}


void GraphicsContextGLOpenGL::blendFunc(GCGLenum sfactor, GCGLenum dfactor)
{
    if (!makeContextCurrent())
        return;

    gl::BlendFunc(sfactor, dfactor);
}

void GraphicsContextGLOpenGL::blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    if (!makeContextCurrent())
        return;

    gl::BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContextGLOpenGL::bufferData(GCGLenum target, GCGLsizeiptr size, GCGLenum usage)
{
    if (!makeContextCurrent())
        return;

    gl::BufferData(target, size, 0, usage);
}

void GraphicsContextGLOpenGL::bufferData(GCGLenum target, GCGLSpan<const GCGLvoid> data, GCGLenum usage)
{
    if (!makeContextCurrent())
        return;

    gl::BufferData(target, data.bufSize, data.data, usage);
}

void GraphicsContextGLOpenGL::bufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    gl::BufferSubData(target, offset, data.bufSize, data.data);
}

void GraphicsContextGLOpenGL::getBufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;
    GCGLvoid* ptr = gl::MapBufferRange(target, offset, data.bufSize, GraphicsContextGL::MAP_READ_BIT);
    if (!ptr)
        return;
    memcpy(data.data, ptr, data.bufSize);
    if (!gl::UnmapBuffer(target))
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
}

void GraphicsContextGLOpenGL::copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr size)
{
    if (!makeContextCurrent())
        return;

    gl::CopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void GraphicsContextGLOpenGL::getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, GCGLSpan<GCGLint> data)
{
    if (!makeContextCurrent())
        return;
    gl::GetInternalformativRobustANGLE(target, internalformat, pname, data.bufSize, nullptr, data.data);
}

void GraphicsContextGLOpenGL::renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    gl::RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void GraphicsContextGLOpenGL::texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    gl::TexStorage2D(target, levels, internalformat, width, height);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth)
{
    if (!makeContextCurrent())
        return;

    gl::TexStorage3D(target, levels, internalformat, width, height, depth);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::texImage3D(GCGLenum target, int level, int internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!makeContextCurrent())
        return;

    gl::TexImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, format, type, pixels.bufSize, pixels.data);
}

void GraphicsContextGLOpenGL::texImage3D(GCGLenum target, int level, int internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    texImage3D(target, level, internalformat, width, height, depth, border, format, type, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLOpenGL::texSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!makeContextCurrent())
        return;

    gl::TexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels.bufSize, pixels.data);
}

void GraphicsContextGLOpenGL::texSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLOpenGL::compressedTexImage3D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    gl::CompressedTexImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, imageSize, data.bufSize, data.data);
}

void GraphicsContextGLOpenGL::compressedTexImage3D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLsizei imageSize, GCGLintptr offset)
{
    compressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLOpenGL::compressedTexSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    gl::CompressedTexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data.bufSize, data.data);
}

void GraphicsContextGLOpenGL::compressedTexSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

Vector<GCGLint> GraphicsContextGLOpenGL::getActiveUniforms(PlatformGLObject program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname)
{
    Vector<GCGLint> result(uniformIndices.size(), 0);
    ASSERT(program);
    if (!makeContextCurrent())
        return result;

    gl::GetActiveUniformsiv(program, uniformIndices.size(), uniformIndices.data(), pname, result.data());
    return result;
}

GCGLenum GraphicsContextGLOpenGL::checkFramebufferStatus(GCGLenum target)
{
    if (!makeContextCurrent())
        return GL_INVALID_OPERATION;

    return gl::CheckFramebufferStatus(target);
}

void GraphicsContextGLOpenGL::clearColor(GCGLclampf r, GCGLclampf g, GCGLclampf b, GCGLclampf a)
{
    if (!makeContextCurrent())
        return;

    gl::ClearColor(r, g, b, a);
}

void GraphicsContextGLOpenGL::clear(GCGLbitfield mask)
{
    if (!makeContextCurrent())
        return;

    gl::Clear(mask);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::clearStencil(GCGLint s)
{
    if (!makeContextCurrent())
        return;

    gl::ClearStencil(s);
}

void GraphicsContextGLOpenGL::colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (!makeContextCurrent())
        return;

    gl::ColorMask(red, green, blue, alpha);
}

void GraphicsContextGLOpenGL::compileShader(PlatformGLObject shader)
{
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

#if !PLATFORM(WIN)
    // We need the ANGLE_texture_rectangle extension to support IOSurface
    // backbuffers, but we don't want it exposed to WebGL user shaders.
    // Temporarily disable it during shader compilation.
    gl::Disable(GL_TEXTURE_RECTANGLE_ANGLE);
#endif
    gl::CompileShader(shader);
#if !PLATFORM(WIN)
    gl::Enable(GL_TEXTURE_RECTANGLE_ANGLE);
#endif
}

void GraphicsContextGLOpenGL::compileShaderDirect(PlatformGLObject shader)
{
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    gl::CompileShader(shader);
}

void GraphicsContextGLOpenGL::texImage2DDirect(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels)
{
    if (!makeContextCurrent())
        return;
    gl::TexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        gl::BindFramebuffer(framebufferTarget, m_fbo);
    }
    gl::CopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        gl::BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

void GraphicsContextGLOpenGL::copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        gl::BindFramebuffer(framebufferTarget, m_fbo);
    }
    gl::CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        gl::BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

void GraphicsContextGLOpenGL::cullFace(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    gl::CullFace(mode);
}

void GraphicsContextGLOpenGL::depthFunc(GCGLenum func)
{
    if (!makeContextCurrent())
        return;

    gl::DepthFunc(func);
}

void GraphicsContextGLOpenGL::depthMask(GCGLboolean flag)
{
    if (!makeContextCurrent())
        return;

    gl::DepthMask(flag);
}

void GraphicsContextGLOpenGL::detachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    gl::DetachShader(program, shader);
}

void GraphicsContextGLOpenGL::disable(GCGLenum cap)
{
    if (!makeContextCurrent())
        return;

    gl::Disable(cap);
}

void GraphicsContextGLOpenGL::disableVertexAttribArray(GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    gl::DisableVertexAttribArray(index);
}

void GraphicsContextGLOpenGL::drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count)
{
    if (!makeContextCurrent())
        return;

    gl::DrawArrays(mode, first, count);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    gl::DrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::enable(GCGLenum cap)
{
    if (!makeContextCurrent())
        return;

    gl::Enable(cap);
}

void GraphicsContextGLOpenGL::enableVertexAttribArray(GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    gl::EnableVertexAttribArray(index);
}

void GraphicsContextGLOpenGL::finish()
{
    if (!makeContextCurrent())
        return;

    gl::Finish();
}

void GraphicsContextGLOpenGL::flush()
{
    if (!makeContextCurrent())
        return;

    gl::Flush();
}

void GraphicsContextGLOpenGL::framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    gl::FramebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
}

void GraphicsContextGLOpenGL::framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject texture, GCGLint level)
{
    if (!makeContextCurrent())
        return;

    gl::FramebufferTexture2D(target, attachment, textarget, texture, level);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::frontFace(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    gl::FrontFace(mode);
}

void GraphicsContextGLOpenGL::generateMipmap(GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    gl::GenerateMipmap(target);
}

bool GraphicsContextGLOpenGL::getActiveAttribImpl(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    if (!makeContextCurrent())
        return false;

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

bool GraphicsContextGLOpenGL::getActiveAttrib(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    return getActiveAttribImpl(program, index, info);
}

bool GraphicsContextGLOpenGL::getActiveUniformImpl(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    if (!makeContextCurrent())
        return false;

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

bool GraphicsContextGLOpenGL::getActiveUniform(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    return getActiveUniformImpl(program, index, info);
}

void GraphicsContextGLOpenGL::getAttachedShaders(PlatformGLObject program, GCGLsizei maxCount, GCGLsizei* count, PlatformGLObject* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    if (!makeContextCurrent())
        return;

    gl::GetAttachedShaders(program, maxCount, count, shaders);
}

int GraphicsContextGLOpenGL::getAttribLocation(PlatformGLObject program, const String& name)
{
    if (!program)
        return -1;

    if (!makeContextCurrent())
        return -1;


    return gl::GetAttribLocation(program, name.utf8().data());
}

int GraphicsContextGLOpenGL::getAttribLocationDirect(PlatformGLObject program, const String& name)
{
    return getAttribLocation(program, name);
}

bool GraphicsContextGLOpenGL::moveErrorsToSyntheticErrorList()
{
    if (!makeContextCurrent())
        return false;

    bool movedAnError = false;

    // Set an arbitrary limit of 100 here to avoid creating a hang if
    // a problem driver has a bug that causes it to never clear the error.
    // Otherwise, we would just loop until we got NO_ERROR.
    for (unsigned i = 0; i < 100; ++i) {
        GCGLenum error = gl::GetError();
        if (error == NO_ERROR)
            break;
        m_syntheticErrors.add(error);
        movedAnError = true;
    }

    return movedAnError;
}

GCGLenum GraphicsContextGLOpenGL::getError()
{
    if (!m_syntheticErrors.isEmpty()) {
        // Need to move the current errors to the synthetic error list in case
        // that error is already there, since the expected behavior of both
        // glGetError and getError is to only report each error code once.
        moveErrorsToSyntheticErrorList();
        return m_syntheticErrors.takeFirst();
    }

    if (!makeContextCurrent())
        return GL_INVALID_OPERATION;

    return gl::GetError();
}

String GraphicsContextGLOpenGL::getString(GCGLenum name)
{
    if (!makeContextCurrent())
        return String();

    return String(reinterpret_cast<const char*>(gl::GetString(name)));
}

void GraphicsContextGLOpenGL::hint(GCGLenum target, GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    gl::Hint(target, mode);
}

GCGLboolean GraphicsContextGLOpenGL::isBuffer(PlatformGLObject buffer)
{
    if (!buffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsBuffer(buffer);
}

GCGLboolean GraphicsContextGLOpenGL::isEnabled(GCGLenum cap)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsEnabled(cap);
}

GCGLboolean GraphicsContextGLOpenGL::isFramebuffer(PlatformGLObject framebuffer)
{
    if (!framebuffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsFramebuffer(framebuffer);
}

GCGLboolean GraphicsContextGLOpenGL::isProgram(PlatformGLObject program)
{
    if (!program)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsProgram(program);
}

GCGLboolean GraphicsContextGLOpenGL::isRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!renderbuffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsRenderbuffer(renderbuffer);
}

GCGLboolean GraphicsContextGLOpenGL::isShader(PlatformGLObject shader)
{
    if (!shader)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsShader(shader);
}

GCGLboolean GraphicsContextGLOpenGL::isTexture(PlatformGLObject texture)
{
    if (!texture)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsTexture(texture);
}

void GraphicsContextGLOpenGL::lineWidth(GCGLfloat width)
{
    if (!makeContextCurrent())
        return;

    gl::LineWidth(width);
}

void GraphicsContextGLOpenGL::linkProgram(PlatformGLObject program)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    gl::LinkProgram(program);
}

void GraphicsContextGLOpenGL::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (!makeContextCurrent())
        return;

    gl::PixelStorei(pname, param);
}

void GraphicsContextGLOpenGL::polygonOffset(GCGLfloat factor, GCGLfloat units)
{
    if (!makeContextCurrent())
        return;

    gl::PolygonOffset(factor, units);
}

void GraphicsContextGLOpenGL::sampleCoverage(GCGLclampf value, GCGLboolean invert)
{
    if (!makeContextCurrent())
        return;

    gl::SampleCoverage(value, invert);
}

void GraphicsContextGLOpenGL::scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    gl::Scissor(x, y, width, height);
}

void GraphicsContextGLOpenGL::shaderSource(PlatformGLObject shader, const String& string)
{
    ASSERT(shader);

    if (!makeContextCurrent())
        return;

    const CString& shaderSourceCString = string.utf8();
    const char* shaderSourcePtr = shaderSourceCString.data();
    int shaderSourceLength = shaderSourceCString.length();
    gl::ShaderSource(shader, 1, &shaderSourcePtr, &shaderSourceLength);
}

void GraphicsContextGLOpenGL::stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    gl::StencilFunc(func, ref, mask);
}

void GraphicsContextGLOpenGL::stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    gl::StencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContextGLOpenGL::stencilMask(GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    gl::StencilMask(mask);
}

void GraphicsContextGLOpenGL::stencilMaskSeparate(GCGLenum face, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    gl::StencilMaskSeparate(face, mask);
}

void GraphicsContextGLOpenGL::stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (!makeContextCurrent())
        return;

    gl::StencilOp(fail, zfail, zpass);
}

void GraphicsContextGLOpenGL::stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (!makeContextCurrent())
        return;

    gl::StencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContextGLOpenGL::texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat value)
{
    if (!makeContextCurrent())
        return;

    gl::TexParameterf(target, pname, value);
}

void GraphicsContextGLOpenGL::texParameteri(GCGLenum target, GCGLenum pname, GCGLint value)
{
    if (!makeContextCurrent())
        return;

    gl::TexParameteri(target, pname, value);
}

void GraphicsContextGLOpenGL::uniform1f(GCGLint location, GCGLfloat v0)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform1f(location, v0);
}

void GraphicsContextGLOpenGL::uniform1fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform1fv(location, array.bufSize, array.data);
}

void GraphicsContextGLOpenGL::uniform2f(GCGLint location, GCGLfloat v0, GCGLfloat v1)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform2f(location, v0, v1);
}

void GraphicsContextGLOpenGL::uniform2fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 2));
    if (!makeContextCurrent())
        return;

    gl::Uniform2fv(location, array.bufSize / 2, array.data);
}

void GraphicsContextGLOpenGL::uniform3f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform3f(location, v0, v1, v2);
}

void GraphicsContextGLOpenGL::uniform3fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 3));
    if (!makeContextCurrent())
        return;

    gl::Uniform3fv(location, array.bufSize / 3, array.data);
}

void GraphicsContextGLOpenGL::uniform4f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform4f(location, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::uniform4fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 4));
    if (!makeContextCurrent())
        return;

    gl::Uniform4fv(location, array.bufSize / 4, array.data);
}

void GraphicsContextGLOpenGL::uniform1i(GCGLint location, GCGLint v0)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform1i(location, v0);
}

void GraphicsContextGLOpenGL::uniform1iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform1iv(location, array.bufSize, array.data);
}

void GraphicsContextGLOpenGL::uniform2i(GCGLint location, GCGLint v0, GCGLint v1)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform2i(location, v0, v1);
}

void GraphicsContextGLOpenGL::uniform2iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 2));
    if (!makeContextCurrent())
        return;

    gl::Uniform2iv(location, array.bufSize / 2, array.data);
}

void GraphicsContextGLOpenGL::uniform3i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform3i(location, v0, v1, v2);
}

void GraphicsContextGLOpenGL::uniform3iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 3));
    if (!makeContextCurrent())
        return;

    gl::Uniform3iv(location, array.bufSize / 3, array.data);
}

void GraphicsContextGLOpenGL::uniform4i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2, GCGLint v3)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform4i(location, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::uniform4iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 4));
    if (!makeContextCurrent())
        return;

    gl::Uniform4iv(location, array.bufSize / 4, array.data);
}

void GraphicsContextGLOpenGL::uniformMatrix2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix2fv(location, array.bufSize / 4, transpose, array.data);
}

void GraphicsContextGLOpenGL::uniformMatrix3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 9));
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix3fv(location, array.bufSize / 9, transpose, array.data);
}

void GraphicsContextGLOpenGL::uniformMatrix4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 16));
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix4fv(location, array.bufSize / 16, transpose, array.data);
}

void GraphicsContextGLOpenGL::useProgram(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return;

    gl::UseProgram(program);
}

void GraphicsContextGLOpenGL::validateProgram(PlatformGLObject program)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return;

    gl::ValidateProgram(program);
}

void GraphicsContextGLOpenGL::vertexAttrib1f(GCGLuint index, GCGLfloat v0)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttrib1f(index, v0);
}

void GraphicsContextGLOpenGL::vertexAttrib1fv(GCGLuint index, GCGLSpan<const GCGLfloat, 1> array)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttrib1fv(index, array.data);
}

void GraphicsContextGLOpenGL::vertexAttrib2f(GCGLuint index, GCGLfloat v0, GCGLfloat v1)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttrib2f(index, v0, v1);
}

void GraphicsContextGLOpenGL::vertexAttrib2fv(GCGLuint index, GCGLSpan<const GCGLfloat, 2> array)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttrib2fv(index, array.data);
}

void GraphicsContextGLOpenGL::vertexAttrib3f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttrib3f(index, v0, v1, v2);
}

void GraphicsContextGLOpenGL::vertexAttrib3fv(GCGLuint index, GCGLSpan<const GCGLfloat, 3> array)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttrib3fv(index, array.data);
}

void GraphicsContextGLOpenGL::vertexAttrib4f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttrib4f(index, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::vertexAttrib4fv(GCGLuint index, GCGLSpan<const GCGLfloat, 4> array)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttrib4fv(index, array.data);
}

void GraphicsContextGLOpenGL::vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContextGLOpenGL::vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttribIPointer(index, size, type, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContextGLOpenGL::viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    gl::Viewport(x, y, width, height);
}

PlatformGLObject GraphicsContextGLOpenGL::createVertexArray()
{
    if (!makeContextCurrent())
        return 0;

    GLuint array = 0;
    if (m_isForWebGL2)
        gl::GenVertexArrays(1, &array);
    else
        gl::GenVertexArraysOES(1, &array);
    return array;
}

void GraphicsContextGLOpenGL::deleteVertexArray(PlatformGLObject array)
{
    if (!array)
        return;
    if (!makeContextCurrent())
        return;
    if (m_isForWebGL2)
        gl::DeleteVertexArrays(1, &array);
    else
        gl::DeleteVertexArraysOES(1, &array);
}

GCGLboolean GraphicsContextGLOpenGL::isVertexArray(PlatformGLObject array)
{
    if (!array)
        return GL_FALSE;
    if (!makeContextCurrent())
        return GL_FALSE;

    if (m_isForWebGL2)
        return gl::IsVertexArray(array);
    return gl::IsVertexArrayOES(array);
}

void GraphicsContextGLOpenGL::bindVertexArray(PlatformGLObject array)
{
    if (!makeContextCurrent())
        return;
    if (m_isForWebGL2)
        gl::BindVertexArray(array);
    else
        gl::BindVertexArrayOES(array);
}

void GraphicsContextGLOpenGL::getBooleanv(GCGLenum pname, GCGLSpan<GCGLboolean> value)
{
    if (!makeContextCurrent())
        return;

    gl::GetBooleanvRobustANGLE(pname, value.bufSize, nullptr, value.data);
}

GCGLint GraphicsContextGLOpenGL::getBufferParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    gl::GetBufferParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLOpenGL::getFloatv(GCGLenum pname, GCGLSpan<GCGLfloat> value)
{
    if (!makeContextCurrent())
        return;

    gl::GetFloatvRobustANGLE(pname, value.bufSize, nullptr, value.data);
}
    
GCGLint64 GraphicsContextGLOpenGL::getInteger64(GCGLenum pname)
{
    GCGLint64 value = 0;
    if (!makeContextCurrent())
        return value;
    gl::GetInteger64vRobustANGLE(pname, 1, nullptr, &value);
    return value;
}

GCGLint64 GraphicsContextGLOpenGL::getInteger64i(GCGLenum pname, GCGLuint index)
{
    GCGLint64 value = 0;
    if (!makeContextCurrent())
        return value;
    gl::GetInteger64i_vRobustANGLE(pname, index, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getFramebufferAttachmentParameteri(GCGLenum target, GCGLenum attachment, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    gl::GetFramebufferAttachmentParameterivRobustANGLE(target, attachment, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getProgrami(PlatformGLObject program, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    gl::GetProgramivRobustANGLE(program, pname, 1, nullptr, &value);
    return value;
}

String GraphicsContextGLOpenGL::getUnmangledInfoLog(PlatformGLObject shaders[2], GCGLsizei count, const String& log)
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

String GraphicsContextGLOpenGL::getProgramInfoLog(PlatformGLObject program)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return String();

    GLint length = 0;
    gl::GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return String(); 

    GLsizei size = 0;
    Vector<GLchar> info(length);
    gl::GetProgramInfoLog(program, length, &size, info.data());

    GCGLsizei count;
    PlatformGLObject shaders[2];
    getAttachedShaders(program, 2, &count, shaders);

    return getUnmangledInfoLog(shaders, count, String(info.data(), size));
}

GCGLint GraphicsContextGLOpenGL::getRenderbufferParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    gl::GetRenderbufferParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getShaderi(PlatformGLObject shader, GCGLenum pname)
{
    ASSERT(shader);
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    gl::GetShaderivRobustANGLE(shader, pname, 1, nullptr, &value);
    return value;
}

String GraphicsContextGLOpenGL::getShaderInfoLog(PlatformGLObject shader)
{
    ASSERT(shader);

    if (!makeContextCurrent())
        return String();

    GLint length = 0;
    gl::GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return String(); 

    GLsizei size = 0;
    Vector<GLchar> info(length);
    gl::GetShaderInfoLog(shader, length, &size, info.data());

    PlatformGLObject shaders[2] = { shader, 0 };
    return getUnmangledInfoLog(shaders, 1, String(info.data(), size));
}

String GraphicsContextGLOpenGL::getShaderSource(PlatformGLObject)
{
    return emptyString();
}

GCGLfloat GraphicsContextGLOpenGL::getTexParameterf(GCGLenum target, GCGLenum pname)
{
    GCGLfloat value = 0.f;
    if (!makeContextCurrent())
        return value;
    gl::GetTexParameterfvRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getTexParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    gl::GetTexParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLOpenGL::getUniformfv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLfloat> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.bufSize * sizeof(*value);
    gl::GetUniformfvRobustANGLE(program, location, bufSize, nullptr, value.data);
}

void GraphicsContextGLOpenGL::getUniformiv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLint> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.bufSize * sizeof(*value);
    gl::GetUniformivRobustANGLE(program, location, bufSize, nullptr, value.data);
}

void GraphicsContextGLOpenGL::getUniformuiv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLuint> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.bufSize * sizeof(*value);
    gl::GetUniformuivRobustANGLE(program, location, bufSize, nullptr, value.data);
}

GCGLint GraphicsContextGLOpenGL::getUniformLocation(PlatformGLObject program, const String& name)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return -1;

    return gl::GetUniformLocation(program, name.utf8().data());
}

GCGLsizeiptr GraphicsContextGLOpenGL::getVertexAttribOffset(GCGLuint index, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLvoid* pointer = 0;
    gl::GetVertexAttribPointervRobustANGLE(index, pname, 1, nullptr, &pointer);
    return static_cast<GCGLsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

PlatformGLObject GraphicsContextGLOpenGL::createBuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    gl::GenBuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createFramebuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    gl::GenFramebuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createProgram()
{
    if (!makeContextCurrent())
        return 0;

    return gl::CreateProgram();
}

PlatformGLObject GraphicsContextGLOpenGL::createRenderbuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    gl::GenRenderbuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createShader(GCGLenum type)
{
    if (!makeContextCurrent())
        return 0;

    return gl::CreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

PlatformGLObject GraphicsContextGLOpenGL::createTexture()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    gl::GenTextures(1, &o);
    m_state.textureSeedCount.add(o);
    return o;
}

void GraphicsContextGLOpenGL::deleteBuffer(PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    gl::DeleteBuffers(1, &buffer);
}

void GraphicsContextGLOpenGL::deleteFramebuffer(PlatformGLObject framebuffer)
{
    if (!makeContextCurrent())
        return;

    // Make sure the framebuffer is not going to be used for drawing
    // operations after it gets deleted.
    if (m_isForWebGL2) {
        if (framebuffer == m_state.boundDrawFBO)
            bindFramebuffer(DRAW_FRAMEBUFFER, 0);
        if (framebuffer == m_state.boundReadFBO)
            bindFramebuffer(READ_FRAMEBUFFER, 0);
    } else if (framebuffer == m_state.boundDrawFBO)
        bindFramebuffer(FRAMEBUFFER, 0);
    gl::DeleteFramebuffers(1, &framebuffer);
}

void GraphicsContextGLOpenGL::deleteProgram(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return;

    gl::DeleteProgram(program);
}

void GraphicsContextGLOpenGL::deleteRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;

    gl::DeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContextGLOpenGL::deleteShader(PlatformGLObject shader)
{
    if (!makeContextCurrent())
        return;

    gl::DeleteShader(shader);
}

void GraphicsContextGLOpenGL::deleteTexture(PlatformGLObject texture)
{
    if (!makeContextCurrent())
        return;

    m_state.boundTextureMap.removeIf([texture] (auto& keyValue) {
        return keyValue.value.first == texture;
    });
    gl::DeleteTextures(1, &texture);
    m_state.textureSeedCount.removeAll(texture);
}

void GraphicsContextGLOpenGL::synthesizeGLError(GCGLenum error)
{
    // Need to move the current errors to the synthetic error list to
    // preserve the order of errors, so a caller to getError will get
    // any errors from gl::Error before the error we are synthesizing.
    moveErrorsToSyntheticErrorList();
    m_syntheticErrors.add(error);
}

void GraphicsContextGLOpenGL::markContextChanged()
{
    m_layerComposited = false;
}

void GraphicsContextGLOpenGL::markLayerComposited()
{
    m_layerComposited = true;
    resetBuffersToAutoClear();

    for (auto* client : copyToVector(m_clients))
        client->didComposite();
}

bool GraphicsContextGLOpenGL::layerComposited() const
{
    return m_layerComposited;
}

void GraphicsContextGLOpenGL::forceContextLost()
{
    for (auto* client : copyToVector(m_clients))
        client->forceContextLost();
}

void GraphicsContextGLOpenGL::recycleContext()
{
    for (auto* client : copyToVector(m_clients))
        client->recycleContext();
}

void GraphicsContextGLOpenGL::dispatchContextChangedNotification()
{
    for (auto* client : copyToVector(m_clients))
        client->dispatchContextChangedNotification();
}

void GraphicsContextGLOpenGL::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    if (!makeContextCurrent())
        return;

    if (m_isForWebGL2)
        gl::DrawArraysInstanced(mode, first, count, primcount);
    else
        gl::DrawArraysInstancedANGLE(mode, first, count, primcount);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount)
{
    if (!makeContextCurrent())
        return;

    if (m_isForWebGL2)
        gl::DrawElementsInstanced(mode, count, type, reinterpret_cast<void*>(offset), primcount);
    else
        gl::DrawElementsInstancedANGLE(mode, count, type, reinterpret_cast<void*>(offset), primcount);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    if (!makeContextCurrent())
        return;

    if (m_isForWebGL2)
        gl::VertexAttribDivisor(index, divisor);
    else
        gl::VertexAttribDivisorANGLE(index, divisor);
}

GCGLuint GraphicsContextGLOpenGL::getUniformBlockIndex(PlatformGLObject program, const String& uniformBlockName)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return GL_INVALID_INDEX;

    return gl::GetUniformBlockIndex(program, uniformBlockName.utf8().data());
}

String GraphicsContextGLOpenGL::getActiveUniformBlockName(PlatformGLObject program, GCGLuint uniformBlockIndex)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return String();

    GLint maxLength = 0;
    gl::GetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxLength);
    if (maxLength <= 0) {
        synthesizeGLError(INVALID_VALUE);
        return String();
    }
    Vector<GLchar> buffer(maxLength);
    GLsizei length = 0;
    gl::GetActiveUniformBlockName(program, uniformBlockIndex, buffer.size(), &length, buffer.data());
    if (!length)
        return String();
    return String(buffer.data(), length);
}

void GraphicsContextGLOpenGL::uniformBlockBinding(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    gl::UniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

// Query Functions

PlatformGLObject GraphicsContextGLOpenGL::createQuery()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    gl::GenQueries(1, &name);
    return name;
}

void GraphicsContextGLOpenGL::beginQuery(GCGLenum target, PlatformGLObject query)
{
    if (!makeContextCurrent())
        return;

    gl::BeginQuery(target, query);
}

void GraphicsContextGLOpenGL::endQuery(GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    gl::EndQuery(target);
}

GCGLuint GraphicsContextGLOpenGL::getQueryObjectui(GCGLuint id, GCGLenum pname)
{
    GCGLuint value = 0;
    if (!makeContextCurrent())
        return value;
    gl::GetQueryObjectuivRobustANGLE(id, pname, 1, nullptr, &value);
    return value;
}

// Transform Feedback Functions

PlatformGLObject GraphicsContextGLOpenGL::createTransformFeedback()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    gl::GenTransformFeedbacks(1, &name);
    return name;
}

void GraphicsContextGLOpenGL::deleteTransformFeedback(PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return;

    gl::DeleteTransformFeedbacks(1, &transformFeedback);
}

GCGLboolean GraphicsContextGLOpenGL::isTransformFeedback(PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsTransformFeedback(transformFeedback);
}

void GraphicsContextGLOpenGL::bindTransformFeedback(GCGLenum target, PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return;

    gl::BindTransformFeedback(target, transformFeedback);
}

void GraphicsContextGLOpenGL::beginTransformFeedback(GCGLenum primitiveMode)
{
    if (!makeContextCurrent())
        return;

    gl::BeginTransformFeedback(primitiveMode);
}

void GraphicsContextGLOpenGL::endTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    gl::EndTransformFeedback();
}

void GraphicsContextGLOpenGL::transformFeedbackVaryings(PlatformGLObject program, const Vector<String>& varyings, GCGLenum bufferMode)
{
    if (!makeContextCurrent())
        return;

    Vector<CString> convertedVaryings = varyings.map([](const String& varying) {
        return varying.utf8();
    });
    Vector<const char*> pointersToVaryings = convertedVaryings.map([](const CString& varying) {
        return varying.data();
    });

    gl::TransformFeedbackVaryings(program, pointersToVaryings.size(), pointersToVaryings.data(), bufferMode);
}

void GraphicsContextGLOpenGL::getTransformFeedbackVarying(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!makeContextCurrent())
        return;

    GCGLsizei bufSize = 0;
    gl::GetProgramiv(program, GraphicsContextGLOpenGL::TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &bufSize);
    if (!bufSize)
        return;

    GCGLsizei length = 0;
    GCGLsizei size = 0;
    GCGLenum type = 0;
    Vector<GCGLchar> name(bufSize);

    gl::GetTransformFeedbackVarying(program, index, bufSize, &length, &size, &type, name.data());

    info.name = String(name.data(), length);
    info.size = size;
    info.type = type;
}

void GraphicsContextGLOpenGL::bindBufferBase(GCGLenum target, GCGLuint index, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    gl::BindBufferBase(target, index, buffer);
}

void GraphicsContextGLOpenGL::blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter)
{
    if (!makeContextCurrent())
        return;

    gl::BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void GraphicsContextGLOpenGL::framebufferTextureLayer(GCGLenum target, GCGLenum attachment, PlatformGLObject texture, GCGLint level, GCGLint layer)
{
    if (!makeContextCurrent())
        return;

    gl::FramebufferTextureLayer(target, attachment, texture, level, layer);
}

void GraphicsContextGLOpenGL::invalidateFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments)
{
    if (!makeContextCurrent())
        return;

    gl::InvalidateFramebuffer(target, attachments.bufSize, attachments.data);
}

void GraphicsContextGLOpenGL::invalidateSubFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    gl::InvalidateSubFramebuffer(target, attachments.bufSize, attachments.data, x, y, width, height);
}

void GraphicsContextGLOpenGL::readBuffer(GCGLenum src)
{
    if (!makeContextCurrent())
        return;

    gl::ReadBuffer(src);
}

void GraphicsContextGLOpenGL::copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        gl::BindFramebuffer(framebufferTarget, m_fbo);
    }
    gl::CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        gl::BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

GCGLint GraphicsContextGLOpenGL::getFragDataLocation(PlatformGLObject program, const String& name)
{
    if (!makeContextCurrent())
        return -1;

    return gl::GetFragDataLocation(program, name.utf8().data());
}

void GraphicsContextGLOpenGL::uniform1ui(GCGLint location, GCGLuint v0)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform1ui(location, v0);
}

void GraphicsContextGLOpenGL::uniform2ui(GCGLint location, GCGLuint v0, GCGLuint v1)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform2ui(location, v0, v1);
}

void GraphicsContextGLOpenGL::uniform3ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform3ui(location, v0, v1, v2);
}

void GraphicsContextGLOpenGL::uniform4ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform4ui(location, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::uniform1uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    if (!makeContextCurrent())
        return;

    gl::Uniform1uiv(location, data.bufSize, data.data);
}

void GraphicsContextGLOpenGL::uniform2uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    ASSERT(!(data.bufSize % 2));
    if (!makeContextCurrent())
        return;

    gl::Uniform2uiv(location, data.bufSize / 2, data.data);
}

void GraphicsContextGLOpenGL::uniform3uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    ASSERT(!(data.bufSize % 3));
    if (!makeContextCurrent())
        return;

    gl::Uniform3uiv(location, data.bufSize / 3, data.data);
}

void GraphicsContextGLOpenGL::uniform4uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    ASSERT(!(data.bufSize % 4));
    if (!makeContextCurrent())
        return;

    gl::Uniform4uiv(location, data.bufSize / 4, data.data);
}

void GraphicsContextGLOpenGL::uniformMatrix2x3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 6));
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix2x3fv(location, data.bufSize / 6, transpose, data.data);
}

void GraphicsContextGLOpenGL::uniformMatrix3x2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 6));
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix3x2fv(location, data.bufSize / 6, transpose, data.data);
}

void GraphicsContextGLOpenGL::uniformMatrix2x4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 8));
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix2x4fv(location, data.bufSize / 8, transpose, data.data);
}

void GraphicsContextGLOpenGL::uniformMatrix4x2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 8));
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix4x2fv(location, data.bufSize / 8, transpose, data.data);
}

void GraphicsContextGLOpenGL::uniformMatrix3x4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 12));
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix3x4fv(location, data.bufSize / 12, transpose, data.data);
}

void GraphicsContextGLOpenGL::uniformMatrix4x3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 12));
    if (!makeContextCurrent())
        return;

    gl::UniformMatrix4x3fv(location, data.bufSize / 12, transpose, data.data);
}

void GraphicsContextGLOpenGL::vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttribI4i(index, x, y, z, w);
}

void GraphicsContextGLOpenGL::vertexAttribI4iv(GCGLuint index, GCGLSpan<const GCGLint, 4> values)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttribI4iv(index, values.data);
}

void GraphicsContextGLOpenGL::vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttribI4ui(index, x, y, z, w);
}

void GraphicsContextGLOpenGL::vertexAttribI4uiv(GCGLuint index, GCGLSpan<const GCGLuint, 4> values)
{
    if (!makeContextCurrent())
        return;

    gl::VertexAttribI4uiv(index, values.data);
}

void GraphicsContextGLOpenGL::drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    gl::DrawRangeElements(mode, start, end, count, type, reinterpret_cast<void*>(offset));
}

void GraphicsContextGLOpenGL::drawBuffers(GCGLSpan<const GCGLenum> bufs)
{
    if (!makeContextCurrent())
        return;

    gl::DrawBuffers(bufs.bufSize, bufs.data);
}

void GraphicsContextGLOpenGL::clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLint> values)
{
    if (!makeContextCurrent())
        return;

    gl::ClearBufferiv(buffer, drawbuffer, values.data);
}

void GraphicsContextGLOpenGL::clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLuint> values)
{
    if (!makeContextCurrent())
        return;

    gl::ClearBufferuiv(buffer, drawbuffer, values.data);
}

void GraphicsContextGLOpenGL::clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLfloat> values)
{
    if (!makeContextCurrent())
        return;

    gl::ClearBufferfv(buffer, drawbuffer, values.data);
}

void GraphicsContextGLOpenGL::clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil)
{
    if (!makeContextCurrent())
        return;

    gl::ClearBufferfi(buffer, drawbuffer, depth, stencil);
}

void GraphicsContextGLOpenGL::deleteQuery(PlatformGLObject query)
{
    if (!makeContextCurrent())
        return;

    gl::DeleteQueries(1, &query);
}

GCGLboolean GraphicsContextGLOpenGL::isQuery(PlatformGLObject query)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsQuery(query);
}

PlatformGLObject GraphicsContextGLOpenGL::getQuery(GCGLenum target, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLint value;
    gl::GetQueryiv(target, pname, &value);
    return static_cast<PlatformGLObject>(value);
}

PlatformGLObject GraphicsContextGLOpenGL::createSampler()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    gl::GenSamplers(1, &name);
    return name;
}

void GraphicsContextGLOpenGL::deleteSampler(PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return;

    gl::DeleteSamplers(1, &sampler);
}

GCGLboolean GraphicsContextGLOpenGL::isSampler(PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsSampler(sampler);
}

void GraphicsContextGLOpenGL::bindSampler(GCGLuint unit, PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return;

    gl::BindSampler(unit, sampler);
}

void GraphicsContextGLOpenGL::samplerParameteri(PlatformGLObject sampler, GCGLenum pname, GCGLint param)
{
    if (!makeContextCurrent())
        return;

    gl::SamplerParameteri(sampler, pname, param);
}

void GraphicsContextGLOpenGL::samplerParameterf(PlatformGLObject sampler, GCGLenum pname, GCGLfloat param)
{
    if (!makeContextCurrent())
        return;

    gl::SamplerParameterf(sampler, pname, param);
}

GCGLfloat GraphicsContextGLOpenGL::getSamplerParameterf(PlatformGLObject sampler, GCGLenum pname)
{
    GCGLfloat value = 0.f;
    if (!makeContextCurrent())
        return value;

    gl::GetSamplerParameterfvRobustANGLE(sampler, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getSamplerParameteri(PlatformGLObject sampler, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;

    gl::GetSamplerParameterivRobustANGLE(sampler, pname, 1, nullptr, &value);
    return value;
}

GCGLsync GraphicsContextGLOpenGL::fenceSync(GCGLenum condition, GCGLbitfield flags)
{
    if (!makeContextCurrent())
        return 0;

    return gl::FenceSync(condition, flags);
}

GCGLboolean GraphicsContextGLOpenGL::isSync(GCGLsync sync)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return gl::IsSync(sync);
}

void GraphicsContextGLOpenGL::deleteSync(GCGLsync sync)
{
    if (!makeContextCurrent())
        return;

    gl::DeleteSync(sync);
}

GCGLenum GraphicsContextGLOpenGL::clientWaitSync(GCGLsync sync, GCGLbitfield flags, GCGLuint64 timeout)
{
    if (!makeContextCurrent())
        return GL_WAIT_FAILED;

    return gl::ClientWaitSync(sync, flags, timeout);
}

void GraphicsContextGLOpenGL::waitSync(GCGLsync sync, GCGLbitfield flags, GCGLint64 timeout)
{
    if (!makeContextCurrent())
        return;

    gl::WaitSync(sync, flags, timeout);
}

GCGLint GraphicsContextGLOpenGL::getSynci(GCGLsync sync, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;

    gl::GetSynciv(sync, pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLOpenGL::pauseTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    gl::PauseTransformFeedback();
}

void GraphicsContextGLOpenGL::resumeTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    gl::ResumeTransformFeedback();
}

void GraphicsContextGLOpenGL::bindBufferRange(GCGLenum target, GCGLuint index, PlatformGLObject buffer, GCGLintptr offset, GCGLsizeiptr size)
{
    if (!makeContextCurrent())
        return;

    gl::BindBufferRange(target, index, buffer, offset, size);
}

Vector<GCGLuint> GraphicsContextGLOpenGL::getUniformIndices(PlatformGLObject program, const Vector<String>& uniformNames)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return { };

    Vector<CString> utf8 = uniformNames.map([](auto& x) { return x.utf8(); });
    Vector<const char*> cstr = utf8.map([](auto& x) { return x.data(); });
    Vector<GCGLuint> result(cstr.size(), 0);
    gl::GetUniformIndices(program, cstr.size(), cstr.data(), result.data());
    return result;
}

void GraphicsContextGLOpenGL::getActiveUniformBlockiv(GCGLuint program, GCGLuint uniformBlockIndex, GCGLenum pname, GCGLSpan<GCGLint> params)
{
    if (!makeContextCurrent())
        return;
    gl::GetActiveUniformBlockivRobustANGLE(program, uniformBlockIndex, pname, params.bufSize, nullptr, params.data);
}

void GraphicsContextGLOpenGL::multiDrawArraysANGLE(GCGLenum mode, GCGLSpan<const GCGLint> firsts, GCGLSpan<const GCGLsizei> counts, GCGLsizei drawcount)
{
    if (!makeContextCurrent())
        return;

    gl::MultiDrawArraysANGLE(mode, firsts.data, counts.data, drawcount);
}

void GraphicsContextGLOpenGL::multiDrawArraysInstancedANGLE(GCGLenum mode, GCGLSpan<const GCGLint> firsts, GCGLSpan<const GCGLsizei> counts, GCGLSpan<const GCGLsizei> instanceCounts, GCGLsizei drawcount)
{
    if (!makeContextCurrent())
        return;

    gl::MultiDrawArraysInstancedANGLE(mode, firsts.data, counts.data, instanceCounts.data, drawcount);
}

void GraphicsContextGLOpenGL::multiDrawElementsANGLE(GCGLenum mode, GCGLSpan<const GCGLsizei> counts, GCGLenum type, GCGLSpan<const GCGLint> offsets, GCGLsizei drawcount)
{
    if (!makeContextCurrent())
        return;

    // Must perform conversion from integer offsets to void* pointers before passing down to ANGLE.
    Vector<void*> pointers;
    for (size_t i = 0; i < offsets.bufSize; ++i)
        pointers.append(reinterpret_cast<void*>(offsets[i]));

    gl::MultiDrawElementsANGLE(mode, counts.data, type, pointers.data(), drawcount);
}

void GraphicsContextGLOpenGL::multiDrawElementsInstancedANGLE(GCGLenum mode, GCGLSpan<const GCGLsizei> counts, GCGLenum type, GCGLSpan<const GCGLint> offsets, GCGLSpan<const GCGLsizei> instanceCounts, GCGLsizei drawcount)
{
    if (!makeContextCurrent())
        return;

    // Must perform conversion from integer offsets to void* pointers before passing down to ANGLE.
    Vector<void*> pointers;
    for (size_t i = 0; i < offsets.bufSize; ++i)
        pointers.append(reinterpret_cast<void*>(offsets[i]));

    gl::MultiDrawElementsInstancedANGLE(mode, counts.data, type, pointers.data(), instanceCounts.data, drawcount);
}

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
GraphicsContextGLCV* GraphicsContextGLOpenGL::asCV()
{
    if (!m_cv)
        m_cv = makeUnique<GraphicsContextGLCVANGLE>(*this);
    return m_cv.get();
}
#endif

bool GraphicsContextGLOpenGL::waitAndUpdateOldestFrame()
{
    size_t oldestFrameCompletionFence = m_oldestFrameCompletionFence++ % maxPendingFrames;
    bool success = true;
    if (ScopedGLFence fence = WTFMove(m_frameCompletionFences[oldestFrameCompletionFence])) {
        // Wait so that rendering doeØs not get more than maxPendingFrames frames ahead.
        GLbitfield flags = GL_SYNC_FLUSH_COMMANDS_BIT;
#if PLATFORM(COCOA)
        // Avoid using the GL_SYNC_FLUSH_COMMANDS_BIT because each each frame is ended with a flush
        // due to external IOSurface access. This particular fence is maxPendingFrames behind.
        // This means the creation of this fence has already been flushed.
        flags = 0;
#endif
        GLenum result = gl::ClientWaitSync(fence, flags, maxFrameDuration.nanosecondsAs<GLuint64>());
        ASSERT(result != GL_WAIT_FAILED);
        success = result != GL_WAIT_FAILED && result != GL_TIMEOUT_EXPIRED;
    }
    m_frameCompletionFences[oldestFrameCompletionFence].fenceSync();
    return success;
}

}

#endif // ENABLE(WEBGL) && USE(ANGLE)
