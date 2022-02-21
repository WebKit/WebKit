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
#include "GraphicsContextGLANGLE.h"

#include "ANGLEHeaders.h"
#include "ANGLEUtilities.h"
#include "GraphicsContextGLOpenGLManager.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PixelBuffer.h"
#include <algorithm>
#include <cstring>
#include <wtf/Seconds.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#include "GraphicsContextGLCVCocoa.h"
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


bool GraphicsContextGLANGLE::initialize()
{
    if (!platformInitializeContext())
        return false;
    String extensionsString = String(reinterpret_cast<const char*>(GL_GetString(GL_EXTENSIONS)));
    for (auto& extension : extensionsString.split(' '))
        m_availableExtensions.add(extension);
    extensionsString = String(reinterpret_cast<const char*>(GL_GetString(GL_REQUESTABLE_EXTENSIONS_ANGLE)));
    for (auto& extension : extensionsString.split(' '))
        m_requestableExtensions.add(extension);
    return platformInitialize();
}

bool GraphicsContextGLANGLE::platformInitializeContext()
{
    return true;
}

bool GraphicsContextGLANGLE::platformInitialize()
{
    return true;
}

GCGLenum GraphicsContextGLANGLE::drawingBufferTextureTarget()
{
#if !PLATFORM(COCOA)
    m_drawingBufferTextureTarget = EGL_TEXTURE_2D;
#else
    if (m_drawingBufferTextureTarget == -1)
        EGL_GetConfigAttrib(platformDisplay(), platformConfig(), EGL_BIND_TO_TEXTURE_TARGET_ANGLE, &m_drawingBufferTextureTarget);
#endif

    switch (m_drawingBufferTextureTarget) {
    case EGL_TEXTURE_2D:
        return TEXTURE_2D;
    case EGL_TEXTURE_RECTANGLE_ANGLE:
        return TEXTURE_RECTANGLE_ARB;

    }
    ASSERT_WITH_MESSAGE(false, "Invalid enum returned from EGL_GetConfigAttrib");
    return 0;
}

GCGLenum GraphicsContextGLANGLE::drawingBufferTextureTargetQueryForDrawingTarget(GCGLenum drawingTarget)
{
    switch (drawingTarget) {
    case TEXTURE_2D:
        return TEXTURE_BINDING_2D;
    case TEXTURE_RECTANGLE_ARB:
        return TEXTURE_BINDING_RECTANGLE_ARB;
    }
    ASSERT_WITH_MESSAGE(false, "Invalid drawing target");
    return -1;
}

GCGLint GraphicsContextGLANGLE::EGLDrawingBufferTextureTargetForDrawingTarget(GCGLenum drawingTarget)
{
    switch (drawingTarget) {
    case TEXTURE_2D:
        return EGL_TEXTURE_2D;
    case TEXTURE_RECTANGLE_ARB:
        return EGL_TEXTURE_RECTANGLE_ANGLE;
    }
    ASSERT_WITH_MESSAGE(false, "Invalid drawing target");
    return 0;
}

bool GraphicsContextGLANGLE::releaseThreadResources(ReleaseThreadResourceBehavior releaseBehavior)
{
    platformReleaseThreadResources();

    if (!platformIsANGLEAvailable())
        return false;

    // Unset the EGL current context, since the next access might be from another thread, and the
    // context cannot be current on multiple threads.
    if (releaseBehavior == ReleaseThreadResourceBehavior::ReleaseCurrentContext) {
        EGLDisplay display = EGL_GetCurrentDisplay();
        if (display == EGL_NO_DISPLAY)
            return true;
        // At the time of writing, ANGLE does not flush on MakeCurrent. Since we are
        // potentially switching threads, we should flush.
        // Note: Here we assume also that ANGLE has only one platform context -- otherwise
        // we would need to flush each EGL context that has been used.
        GL_Flush();
        return EGL_MakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (releaseBehavior == ReleaseThreadResourceBehavior::TerminateAndReleaseThreadResources) {
        EGLDisplay currentDisplay = EGL_GetCurrentDisplay();
        if (currentDisplay != EGL_NO_DISPLAY) {
            ASSERT_NOT_REACHED(); // All resources must have been destroyed.
            EGL_MakeCurrent(currentDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
        const EGLNativeDisplayType nativeDisplays[] = {
            reinterpret_cast<EGLNativeDisplayType>(defaultDisplay),
#if PLATFORM(COCOA)
            reinterpret_cast<EGLNativeDisplayType>(defaultOpenGLDisplay),
#endif
        };
        for (auto nativeDisplay : nativeDisplays) {
            EGLDisplay display = EGL_GetDisplay(nativeDisplay);
            if (display != EGL_NO_DISPLAY)
                EGL_Terminate(display);
        }
    }
    // Called when we do not know if we will ever see another call from this thread again.
    // Unset the EGL current context by releasing whole EGL thread state.
    return EGL_ReleaseThread();
}

std::optional<PixelBuffer> GraphicsContextGLANGLE::readPixelsForPaintResults()
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

    GL_ReadnPixelsRobustANGLE(0, 0, pixelBuffer->size().width(), pixelBuffer->size().height(), GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer->data().byteLength(), nullptr, nullptr, nullptr, pixelBuffer->data().data());
    // FIXME: Rendering to GL_RGB textures with a IOSurface bound to the texture image leaves
    // the alpha in the IOSurface in incorrect state. Also ANGLE GL_ReadPixels will in some
    // cases expose the non-255 values.
    // https://bugs.webkit.org/show_bug.cgi?id=215804
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    if (!contextAttributes().alpha)
        wipeAlphaChannelFromPixels(pixelBuffer->size().width(), pixelBuffer->size().height(), pixelBuffer->data().data());
#endif
    return pixelBuffer;
}

void GraphicsContextGLANGLE::validateAttributes()
{
    m_internalColorFormat = contextAttributes().alpha ? GL_RGBA8 : GL_RGB8;

    validateDepthStencil(packedDepthStencilExtensionName);
}

bool GraphicsContextGLANGLE::reshapeFBOs(const IntSize& size)
{
    auto attrs = contextAttributes();
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = attrs.alpha ? GL_RGBA : GL_RGB;

    // Resize multisample FBO.
    if (attrs.antialias) {
        GLint maxSampleCount;
        GL_GetIntegerv(GL_MAX_SAMPLES_ANGLE, &maxSampleCount);
        // Using more than 4 samples is slow on some hardware and is unlikely to
        // produce a significantly better result.
        GLint sampleCount = std::min(4, maxSampleCount);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        GL_BindRenderbuffer(GL_RENDERBUFFER, m_multisampleColorBuffer);
        GL_RenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, m_internalColorFormat, width, height);
        GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_multisampleColorBuffer);
        if (attrs.stencil || attrs.depth) {
            GL_BindRenderbuffer(GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            GL_RenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, m_internalDepthStencilFormat, width, height);
            // WebGL 1.0's rules state that combined depth/stencil renderbuffers
            // have to be attached to the synthetic DEPTH_STENCIL_ATTACHMENT point.
            if (!isGLES2Compliant() && m_internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES)
                GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            else {
                if (attrs.stencil)
                    GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
                if (attrs.depth)
                    GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            }
        }
        GL_BindRenderbuffer(GL_RENDERBUFFER, 0);
        if (GL_CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            // FIXME: cleanup.
            notImplemented();
        }
    }

    // resize regular FBO
    GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    if (!reshapeDisplayBufferBacking()) {
        RELEASE_LOG(WebGL, "Fatal: Unable to allocate backing store of size %d x %d", width, height);
        forceContextLost();
        return true;
    }
    if (m_preserveDrawingBufferTexture) {
        // The context requires the use of an intermediate texture in order to implement
        // preserveDrawingBuffer:true without antialiasing.
        GLint texture2DBinding = 0;
        GL_GetIntegerv(GL_TEXTURE_BINDING_2D, &texture2DBinding);
        GL_BindTexture(GL_TEXTURE_2D, m_preserveDrawingBufferTexture);
        // Note that any pixel unpack buffer was unbound earlier, in reshape().
        GL_TexImage2D(GL_TEXTURE_2D, 0, colorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        // m_fbo is bound at this point.
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_preserveDrawingBufferTexture, 0);
        GL_BindTexture(GL_TEXTURE_2D, texture2DBinding);
        // Attach m_texture to m_preserveDrawingBufferFBO for later blitting.
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_preserveDrawingBufferFBO);
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    } else
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);

    attachDepthAndStencilBufferIfNeeded(m_internalDepthStencilFormat, width, height);

    bool mustRestoreFBO = true;
    if (attrs.antialias) {
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        if (m_state.boundDrawFBO == m_multisampleFBO && m_state.boundReadFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    } else {
        if (m_state.boundDrawFBO == m_fbo && m_state.boundReadFBO == m_fbo)
            mustRestoreFBO = false;
    }

    return mustRestoreFBO;
}

void GraphicsContextGLANGLE::attachDepthAndStencilBufferIfNeeded(GLuint internalDepthStencilFormat, int width, int height)
{
    auto attrs = contextAttributes();

    if (!attrs.antialias && (attrs.stencil || attrs.depth)) {
        GL_BindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
        GL_RenderbufferStorage(GL_RENDERBUFFER, internalDepthStencilFormat, width, height);
        // WebGL 1.0's rules state that combined depth/stencil renderbuffers
        // have to be attached to the synthetic DEPTH_STENCIL_ATTACHMENT point.
        if (!isGLES2Compliant() && internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES)
            GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        else {
            if (attrs.stencil)
                GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
            if (attrs.depth)
                GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        }
        GL_BindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    if (GL_CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // FIXME: cleanup
        notImplemented();
    }
}

void GraphicsContextGLANGLE::resolveMultisamplingIfNecessary(const IntRect& rect)
{
    ScopedGLCapability scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    ScopedGLCapability scopedDither(GL_DITHER, GL_FALSE);

    GLint boundFrameBuffer = 0;
    GLint boundReadFrameBuffer = 0;
    if (m_isForWebGL2) {
        GL_GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundFrameBuffer);
        GL_GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &boundReadFrameBuffer);
    } else
        GL_GetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFrameBuffer);
    GL_BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, m_multisampleFBO);
    GL_BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, m_fbo);

    // FIXME: figure out more efficient solution for iOS.
    if (m_isForWebGL2) {
        // ES 3.0 has BlitFramebuffer.
        IntRect resolveRect = rect.isEmpty() ? IntRect { 0, 0, m_currentWidth, m_currentHeight } : rect;
        GL_BlitFramebuffer(resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
        // ES 2.0 has BlitFramebufferANGLE only.
        GL_BlitFramebufferANGLE(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    if (m_isForWebGL2) {
        GL_BindFramebuffer(GL_DRAW_FRAMEBUFFER, boundFrameBuffer);
        GL_BindFramebuffer(GL_READ_FRAMEBUFFER, boundReadFrameBuffer);
    } else
        GL_BindFramebuffer(GL_FRAMEBUFFER, boundFrameBuffer);
}

void GraphicsContextGLANGLE::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_RenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContextGLANGLE::getIntegerv(GCGLenum pname, GCGLSpan<GCGLint> value)
{
    if (!makeContextCurrent())
        return;
    GL_GetIntegervRobustANGLE(pname, value.bufSize, nullptr, value.data);
}

void GraphicsContextGLANGLE::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLSpan<GCGLint, 2> range, GCGLint* precision)
{
    if (!makeContextCurrent())
        return;

    GL_GetShaderPrecisionFormat(shaderType, precisionType, range.data, precision);
}

void GraphicsContextGLANGLE::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!m_isForWebGL2)
        internalformat = adjustWebGL1TextureInternalFormat(internalformat, format, type);
    if (!makeContextCurrent())
        return;
    GL_TexImage2DRobustANGLE(target, level, internalformat, width, height, border, format, type, pixels.bufSize, pixels.data);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    texImage2D(target, level, internalformat, width, height, border, format, type, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLANGLE::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoff, GCGLint yoff, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!makeContextCurrent())
        return;

    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size.
    GL_TexSubImage2DRobustANGLE(target, level, xoff, yoff, width, height, format, type, pixels.bufSize, pixels.data);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoff, GCGLint yoff, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    texSubImage2D(target, level, xoff, yoff, width, height, format, type, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLANGLE::compressedTexImage2D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, int border, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    GL_CompressedTexImage2DRobustANGLE(target, level, internalformat, width, height, border, imageSize, data.bufSize, data.data);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::compressedTexImage2D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, int border, GCGLsizei imageSize, GCGLintptr offset)
{
    compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLANGLE::compressedTexSubImage2D(GCGLenum target, int level, int xoffset, int yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    GL_CompressedTexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width, height, format, imageSize, data.bufSize, data.data);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::compressedTexSubImage2D(GCGLenum target, int level, int xoffset, int yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLANGLE::depthRange(GCGLclampf zNear, GCGLclampf zFar)
{
    if (!makeContextCurrent())
        return;

    GL_DepthRangef(static_cast<float>(zNear), static_cast<float>(zFar));
}

void GraphicsContextGLANGLE::clearDepth(GCGLclampf depth)
{
    if (!makeContextCurrent())
        return;

    GL_ClearDepthf(static_cast<float>(depth));
}

void GraphicsContextGLANGLE::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<GCGLvoid> data)
{
    readnPixelsImpl(x, y, width, height, format, type, data.bufSize, nullptr, nullptr, nullptr, data.data, false);
}

void GraphicsContextGLANGLE::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    readnPixelsImpl(x, y, width, height, format, type, 0, nullptr, nullptr, nullptr, reinterpret_cast<GCGLvoid*>(offset), true);
}

void GraphicsContextGLANGLE::readnPixelsImpl(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, GCGLsizei* length, GCGLsizei* columns, GCGLsizei* rows, GCGLvoid* data, bool readingToPixelBufferObject)
{
    if (!makeContextCurrent())
        return;

    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    GL_Flush();
    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        GL_BindFramebuffer(framebufferTarget, m_fbo);
        GL_Flush();
    }
    moveErrorsToSyntheticErrorList();
    GL_ReadnPixelsRobustANGLE(x, y, width, height, format, type, bufSize, length, columns, rows, data);
    GLenum error = GL_GetError();
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        GL_BindFramebuffer(framebufferTarget, m_multisampleFBO);

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

// The contents of GraphicsContextGLANGLECommon follow, ported to use ANGLE.

void GraphicsContextGLANGLE::validateDepthStencil(const char* packedDepthStencilExtension)
{
    // FIXME: Since the constructors of various platforms are not shared, we initialize this here.
    // Upon constructing the context, always initialize the extensions that the WebGLRenderingContext* will
    // use to turn on feature flags.
    if (supportsExtension(packedDepthStencilExtension)) {
        ensureExtensionEnabled(packedDepthStencilExtension);
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
        if (!supportsExtension("GL_ANGLE_framebuffer_multisample") || !supportsExtension("GL_ANGLE_framebuffer_blit") || !supportsExtension("GL_OES_rgb8_rgba8")) {
            attrs.antialias = false;
            setContextAttributes(attrs);
        } else {
            ensureExtensionEnabled("GL_ANGLE_framebuffer_multisample");
            ensureExtensionEnabled("GL_ANGLE_framebuffer_blit");
            ensureExtensionEnabled("GL_OES_rgb8_rgba8");
        }
    } else if (attrs.preserveDrawingBuffer) {
        // Needed for preserveDrawingBuffer:true support without antialiasing.
        ensureExtensionEnabled("GL_ANGLE_framebuffer_blit");
    }
}

void GraphicsContextGLANGLE::prepareTexture()
{
    if (m_layerComposited)
        return;

    if (!makeContextCurrent())
        return;

    prepareTextureImpl();
}

void GraphicsContextGLANGLE::prepareTextureImpl()
{
    ASSERT(!m_layerComposited);

    if (contextAttributes().antialias)
        resolveMultisamplingIfNecessary();

#if USE(COORDINATED_GRAPHICS)
    std::swap(m_texture, m_compositorTexture);
    std::swap(m_texture, m_intermediateTexture);
    std::swap(m_textureBacking, m_compositorTextureBacking);
    std::swap(m_textureBacking, m_intermediateTextureBacking);

    GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);
    GL_Flush();

    if (m_state.boundDrawFBO != m_fbo)
        GL_BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
#else
    if (m_preserveDrawingBufferTexture) {
        // Blit m_preserveDrawingBufferTexture into m_texture.
        ScopedGLCapability scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
        ScopedGLCapability scopedDither(GL_DITHER, GL_FALSE);
        GL_BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, m_preserveDrawingBufferFBO);
        GL_BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, m_fbo);
        GL_BlitFramebufferANGLE(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Note: it's been observed that BlitFramebuffer may destroy the alpha channel of the
        // destination texture if it's an RGB texture bound to an IOSurface. This wasn't observable
        // through the WebGL conformance tests, but it may be necessary to save and restore the
        // color mask and clear color, and use the color mask to clear the alpha channel of the
        // destination texture to 1.0.

        // Restore user's framebuffer bindings.
        if (m_isForWebGL2) {
            GL_BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_state.boundDrawFBO);
            GL_BindFramebuffer(GL_READ_FRAMEBUFFER, m_state.boundReadFBO);
        } else
            GL_BindFramebuffer(GL_FRAMEBUFFER, m_state.boundDrawFBO);
    }
#endif
}

std::optional<PixelBuffer> GraphicsContextGLANGLE::readRenderingResults()
{
    ScopedRestoreReadFramebufferBinding fboBinding(m_isForWebGL2, m_state.boundReadFBO);
    if (contextAttributes().antialias) {
        resolveMultisamplingIfNecessary();
        fboBinding.markBindingChanged();
    }
    fboBinding.bindFramebuffer(m_fbo);
    return readPixelsForPaintResults();
}

void GraphicsContextGLANGLE::reshape(int width, int height)
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

    ScopedGLCapability scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    ScopedGLCapability scopedDither(GL_DITHER, GL_FALSE);
    ScopedBufferBinding scopedPixelUnpackBufferReset(GL_PIXEL_UNPACK_BUFFER, 0, m_isForWebGL2);

    bool mustRestoreFBO = reshapeFBOs(IntSize(width, height));
    auto attrs = contextAttributes();

    // Initialize renderbuffers to 0.
    GLfloat clearColor[] = {0, 0, 0, 0}, clearDepth = 0;
    GLint clearStencil = 0;
    GLboolean colorMask[] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, depthMask = GL_TRUE;
    GLuint stencilMask = 0xffffffff, stencilMaskBack = 0xffffffff;
    GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
    GL_GetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    GL_ClearColor(0, 0, 0, 0);
    GL_GetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    GL_ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (attrs.depth) {
        GL_GetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        GL_ClearDepthf(1.0f);
        GL_GetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        GL_DepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }

    if (attrs.stencil) {
        GL_GetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
        GL_ClearStencil(0);
        GL_GetIntegerv(GL_STENCIL_WRITEMASK, reinterpret_cast<GLint*>(&stencilMask));
        GL_GetIntegerv(GL_STENCIL_BACK_WRITEMASK, reinterpret_cast<GLint*>(&stencilMaskBack));
        GL_StencilMaskSeparate(GL_FRONT, 0xffffffff);
        GL_StencilMaskSeparate(GL_BACK, 0xffffffff);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }

    GL_Clear(clearMask);

    GL_ClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    GL_ColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (attrs.depth) {
        GL_ClearDepthf(clearDepth);
        GL_DepthMask(depthMask);
    }
    if (attrs.stencil) {
        GL_ClearStencil(clearStencil);
        GL_StencilMaskSeparate(GL_FRONT, stencilMask);
        GL_StencilMaskSeparate(GL_BACK, stencilMaskBack);
    }

    if (mustRestoreFBO) {
        GL_BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
        if (m_isForWebGL2 && m_state.boundDrawFBO != m_state.boundReadFBO)
            GL_BindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, m_state.boundReadFBO);
    }

    auto error = GL_GetError();
    if (error != GL_NO_ERROR) {
        RELEASE_LOG(WebGL, "Fatal: OpenGL error during GraphicsContextGL buffer initialization (%d).", error);
        forceContextLost();
        return;
    }

    GL_Flush();
}

void GraphicsContextGLANGLE::activeTexture(GCGLenum texture)
{
    if (!makeContextCurrent())
        return;

    m_state.activeTextureUnit = texture;
    GL_ActiveTexture(texture);
}

void GraphicsContextGLANGLE::attachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    GL_AttachShader(program, shader);
}

void GraphicsContextGLANGLE::bindAttribLocation(PlatformGLObject program, GCGLuint index, const String& name)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    GL_BindAttribLocation(program, index, name.utf8().data());
}

void GraphicsContextGLANGLE::bindBuffer(GCGLenum target, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GL_BindBuffer(target, buffer);
}

void GraphicsContextGLANGLE::bindFramebuffer(GCGLenum target, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GLuint fbo;
    if (buffer)
        fbo = buffer;
    else
        fbo = (contextAttributes().antialias ? m_multisampleFBO : m_fbo);

    GL_BindFramebuffer(target, fbo);
    if (target == GL_FRAMEBUFFER) {
        m_state.boundReadFBO = m_state.boundDrawFBO = fbo;
    } else if (target == GL_READ_FRAMEBUFFER) {
        m_state.boundReadFBO = fbo;
    } else if (target == GL_DRAW_FRAMEBUFFER) {
        m_state.boundDrawFBO = fbo;
    }
}

void GraphicsContextGLANGLE::bindRenderbuffer(GCGLenum target, PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;

    GL_BindRenderbuffer(target, renderbuffer);
}


void GraphicsContextGLANGLE::bindTexture(GCGLenum target, PlatformGLObject texture)
{
    if (!makeContextCurrent())
        return;

    m_state.setBoundTexture(m_state.activeTextureUnit, texture, target);
    GL_BindTexture(target, texture);
}

void GraphicsContextGLANGLE::blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha)
{
    if (!makeContextCurrent())
        return;

    GL_BlendColor(red, green, blue, alpha);
}

void GraphicsContextGLANGLE::blendEquation(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_BlendEquation(mode);
}

void GraphicsContextGLANGLE::blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (!makeContextCurrent())
        return;

    GL_BlendEquationSeparate(modeRGB, modeAlpha);
}


void GraphicsContextGLANGLE::blendFunc(GCGLenum sfactor, GCGLenum dfactor)
{
    if (!makeContextCurrent())
        return;

    GL_BlendFunc(sfactor, dfactor);
}

void GraphicsContextGLANGLE::blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    if (!makeContextCurrent())
        return;

    GL_BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContextGLANGLE::bufferData(GCGLenum target, GCGLsizeiptr size, GCGLenum usage)
{
    if (!makeContextCurrent())
        return;

    GL_BufferData(target, size, 0, usage);
}

void GraphicsContextGLANGLE::bufferData(GCGLenum target, GCGLSpan<const GCGLvoid> data, GCGLenum usage)
{
    if (!makeContextCurrent())
        return;

    GL_BufferData(target, data.bufSize, data.data, usage);
}

void GraphicsContextGLANGLE::bufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    GL_BufferSubData(target, offset, data.bufSize, data.data);
}

void GraphicsContextGLANGLE::getBufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;
    GCGLvoid* ptr = GL_MapBufferRange(target, offset, data.bufSize, GraphicsContextGL::MAP_READ_BIT);
    if (!ptr)
        return;
    memcpy(data.data, ptr, data.bufSize);
    if (!GL_UnmapBuffer(target))
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
}

void GraphicsContextGLANGLE::copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr size)
{
    if (!makeContextCurrent())
        return;

    GL_CopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void GraphicsContextGLANGLE::getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, GCGLSpan<GCGLint> data)
{
    if (!makeContextCurrent())
        return;
    GL_GetInternalformativRobustANGLE(target, internalformat, pname, data.bufSize, nullptr, data.data);
}

void GraphicsContextGLANGLE::renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void GraphicsContextGLANGLE::texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_TexStorage2D(target, levels, internalformat, width, height);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth)
{
    if (!makeContextCurrent())
        return;

    GL_TexStorage3D(target, levels, internalformat, width, height, depth);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texImage3D(GCGLenum target, int level, int internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!makeContextCurrent())
        return;

    GL_TexImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, format, type, pixels.bufSize, pixels.data);
}

void GraphicsContextGLANGLE::texImage3D(GCGLenum target, int level, int internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    texImage3D(target, level, internalformat, width, height, depth, border, format, type, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLANGLE::texSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!makeContextCurrent())
        return;

    GL_TexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels.bufSize, pixels.data);
}

void GraphicsContextGLANGLE::texSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLANGLE::compressedTexImage3D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    GL_CompressedTexImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, imageSize, data.bufSize, data.data);
}

void GraphicsContextGLANGLE::compressedTexImage3D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLsizei imageSize, GCGLintptr offset)
{
    compressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

void GraphicsContextGLANGLE::compressedTexSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    GL_CompressedTexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data.bufSize, data.data);
}

void GraphicsContextGLANGLE::compressedTexSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, makeGCGLSpan(reinterpret_cast<const GCGLvoid*>(offset), 0));
}

Vector<GCGLint> GraphicsContextGLANGLE::getActiveUniforms(PlatformGLObject program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname)
{
    Vector<GCGLint> result(uniformIndices.size(), 0);
    ASSERT(program);
    if (!makeContextCurrent())
        return result;

    GL_GetActiveUniformsiv(program, uniformIndices.size(), uniformIndices.data(), pname, result.data());
    return result;
}

GCGLenum GraphicsContextGLANGLE::checkFramebufferStatus(GCGLenum target)
{
    if (!makeContextCurrent())
        return GL_INVALID_OPERATION;

    return GL_CheckFramebufferStatus(target);
}

void GraphicsContextGLANGLE::clearColor(GCGLclampf r, GCGLclampf g, GCGLclampf b, GCGLclampf a)
{
    if (!makeContextCurrent())
        return;

    GL_ClearColor(r, g, b, a);
}

void GraphicsContextGLANGLE::clear(GCGLbitfield mask)
{
    if (!makeContextCurrent())
        return;

    GL_Clear(mask);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::clearStencil(GCGLint s)
{
    if (!makeContextCurrent())
        return;

    GL_ClearStencil(s);
}

void GraphicsContextGLANGLE::colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (!makeContextCurrent())
        return;

    GL_ColorMask(red, green, blue, alpha);
}

void GraphicsContextGLANGLE::compileShader(PlatformGLObject shader)
{
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

#if !PLATFORM(WIN)
    // We need the ANGLE_texture_rectangle extension to support IOSurface
    // backbuffers, but we don't want it exposed to WebGL user shaders.
    // Temporarily disable it during shader compilation.
    GL_Disable(GL_TEXTURE_RECTANGLE_ANGLE);
#endif
    GL_CompileShader(shader);
#if !PLATFORM(WIN)
    GL_Enable(GL_TEXTURE_RECTANGLE_ANGLE);
#endif
}

void GraphicsContextGLANGLE::compileShaderDirect(PlatformGLObject shader)
{
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    GL_CompileShader(shader);
}

void GraphicsContextGLANGLE::texImage2DDirect(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels)
{
    if (!makeContextCurrent())
        return;
    GL_TexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        GL_BindFramebuffer(framebufferTarget, m_fbo);
    }
    GL_CopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        GL_BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

void GraphicsContextGLANGLE::copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        GL_BindFramebuffer(framebufferTarget, m_fbo);
    }
    GL_CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        GL_BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

void GraphicsContextGLANGLE::cullFace(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_CullFace(mode);
}

void GraphicsContextGLANGLE::depthFunc(GCGLenum func)
{
    if (!makeContextCurrent())
        return;

    GL_DepthFunc(func);
}

void GraphicsContextGLANGLE::depthMask(GCGLboolean flag)
{
    if (!makeContextCurrent())
        return;

    GL_DepthMask(flag);
}

void GraphicsContextGLANGLE::detachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    GL_DetachShader(program, shader);
}

void GraphicsContextGLANGLE::disable(GCGLenum cap)
{
    if (!makeContextCurrent())
        return;

    GL_Disable(cap);
}

void GraphicsContextGLANGLE::disableVertexAttribArray(GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    GL_DisableVertexAttribArray(index);
}

void GraphicsContextGLANGLE::drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count)
{
    if (!makeContextCurrent())
        return;

    GL_DrawArrays(mode, first, count);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    GL_DrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
    checkGPUStatus();
}

void GraphicsContextGLANGLE::enable(GCGLenum cap)
{
    if (!makeContextCurrent())
        return;

    GL_Enable(cap);
}

void GraphicsContextGLANGLE::enableVertexAttribArray(GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    GL_EnableVertexAttribArray(index);
}

void GraphicsContextGLANGLE::finish()
{
    if (!makeContextCurrent())
        return;

    GL_Finish();
}

void GraphicsContextGLANGLE::flush()
{
    if (!makeContextCurrent())
        return;

    GL_Flush();
}

void GraphicsContextGLANGLE::framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GL_FramebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
}

void GraphicsContextGLANGLE::framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject texture, GCGLint level)
{
    if (!makeContextCurrent())
        return;

    GL_FramebufferTexture2D(target, attachment, textarget, texture, level);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::frontFace(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_FrontFace(mode);
}

void GraphicsContextGLANGLE::generateMipmap(GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    GL_GenerateMipmap(target);
}

bool GraphicsContextGLANGLE::getActiveAttribImpl(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    if (!makeContextCurrent())
        return false;

    GLint maxAttributeSize = 0;
    GL_GetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeSize);
    Vector<GLchar> name(maxAttributeSize); // GL_ACTIVE_ATTRIBUTE_MAX_LENGTH includes null termination.
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    GL_GetActiveAttrib(program, index, maxAttributeSize, &nameLength, &size, &type, name.data());
    if (!nameLength)
        return false;

    info.name = String(name.data(), nameLength);
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContextGLANGLE::getActiveAttrib(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    return getActiveAttribImpl(program, index, info);
}

bool GraphicsContextGLANGLE::getActiveUniformImpl(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    if (!makeContextCurrent())
        return false;

    GLint maxUniformSize = 0;
    GL_GetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);
    Vector<GLchar> name(maxUniformSize); // GL_ACTIVE_UNIFORM_MAX_LENGTH includes null termination.
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    GL_GetActiveUniform(program, index, maxUniformSize, &nameLength, &size, &type, name.data());
    if (!nameLength)
        return false;

    info.name = String(name.data(), nameLength);
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContextGLANGLE::getActiveUniform(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    return getActiveUniformImpl(program, index, info);
}

void GraphicsContextGLANGLE::getAttachedShaders(PlatformGLObject program, GCGLsizei maxCount, GCGLsizei* count, PlatformGLObject* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    if (!makeContextCurrent())
        return;

    GL_GetAttachedShaders(program, maxCount, count, shaders);
}

int GraphicsContextGLANGLE::getAttribLocation(PlatformGLObject program, const String& name)
{
    if (!program)
        return -1;

    if (!makeContextCurrent())
        return -1;

    return GL_GetAttribLocation(program, name.utf8().data());
}

int GraphicsContextGLANGLE::getAttribLocationDirect(PlatformGLObject program, const String& name)
{
    return getAttribLocation(program, name);
}

bool GraphicsContextGLANGLE::moveErrorsToSyntheticErrorList()
{
    if (!makeContextCurrent())
        return false;

    bool movedAnError = false;

    // Set an arbitrary limit of 100 here to avoid creating a hang if
    // a problem driver has a bug that causes it to never clear the error.
    // Otherwise, we would just loop until we got NO_ERROR.
    for (unsigned i = 0; i < 100; ++i) {
        GCGLenum error = GL_GetError();
        if (error == NO_ERROR)
            break;
        m_syntheticErrors.add(error);
        movedAnError = true;
    }

    return movedAnError;
}

GCGLenum GraphicsContextGLANGLE::getError()
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

    return GL_GetError();
}

String GraphicsContextGLANGLE::getString(GCGLenum name)
{
    if (!makeContextCurrent())
        return String();

    return String(reinterpret_cast<const char*>(GL_GetString(name)));
}

void GraphicsContextGLANGLE::hint(GCGLenum target, GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_Hint(target, mode);
}

GCGLboolean GraphicsContextGLANGLE::isBuffer(PlatformGLObject buffer)
{
    if (!buffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsBuffer(buffer);
}

GCGLboolean GraphicsContextGLANGLE::isEnabled(GCGLenum cap)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsEnabled(cap);
}

GCGLboolean GraphicsContextGLANGLE::isFramebuffer(PlatformGLObject framebuffer)
{
    if (!framebuffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsFramebuffer(framebuffer);
}

GCGLboolean GraphicsContextGLANGLE::isProgram(PlatformGLObject program)
{
    if (!program)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsProgram(program);
}

GCGLboolean GraphicsContextGLANGLE::isRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!renderbuffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsRenderbuffer(renderbuffer);
}

GCGLboolean GraphicsContextGLANGLE::isShader(PlatformGLObject shader)
{
    if (!shader)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsShader(shader);
}

GCGLboolean GraphicsContextGLANGLE::isTexture(PlatformGLObject texture)
{
    if (!texture)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsTexture(texture);
}

void GraphicsContextGLANGLE::lineWidth(GCGLfloat width)
{
    if (!makeContextCurrent())
        return;

    GL_LineWidth(width);
}

void GraphicsContextGLANGLE::linkProgram(PlatformGLObject program)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    GL_LinkProgram(program);
}

void GraphicsContextGLANGLE::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (!makeContextCurrent())
        return;

    GL_PixelStorei(pname, param);
}

void GraphicsContextGLANGLE::polygonOffset(GCGLfloat factor, GCGLfloat units)
{
    if (!makeContextCurrent())
        return;

    GL_PolygonOffset(factor, units);
}

void GraphicsContextGLANGLE::sampleCoverage(GCGLclampf value, GCGLboolean invert)
{
    if (!makeContextCurrent())
        return;

    GL_SampleCoverage(value, invert);
}

void GraphicsContextGLANGLE::scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_Scissor(x, y, width, height);
}

void GraphicsContextGLANGLE::shaderSource(PlatformGLObject shader, const String& string)
{
    ASSERT(shader);

    if (!makeContextCurrent())
        return;

    const CString& shaderSourceCString = string.utf8();
    const char* shaderSourcePtr = shaderSourceCString.data();
    int shaderSourceLength = shaderSourceCString.length();
    GL_ShaderSource(shader, 1, &shaderSourcePtr, &shaderSourceLength);
}

void GraphicsContextGLANGLE::stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    GL_StencilFunc(func, ref, mask);
}

void GraphicsContextGLANGLE::stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    GL_StencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContextGLANGLE::stencilMask(GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    GL_StencilMask(mask);
}

void GraphicsContextGLANGLE::stencilMaskSeparate(GCGLenum face, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    GL_StencilMaskSeparate(face, mask);
}

void GraphicsContextGLANGLE::stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (!makeContextCurrent())
        return;

    GL_StencilOp(fail, zfail, zpass);
}

void GraphicsContextGLANGLE::stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (!makeContextCurrent())
        return;

    GL_StencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContextGLANGLE::texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat value)
{
    if (!makeContextCurrent())
        return;

    GL_TexParameterf(target, pname, value);
}

void GraphicsContextGLANGLE::texParameteri(GCGLenum target, GCGLenum pname, GCGLint value)
{
    if (!makeContextCurrent())
        return;

    GL_TexParameteri(target, pname, value);
}

void GraphicsContextGLANGLE::uniform1f(GCGLint location, GCGLfloat v0)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1f(location, v0);
}

void GraphicsContextGLANGLE::uniform1fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1fv(location, array.bufSize, array.data);
}

void GraphicsContextGLANGLE::uniform2f(GCGLint location, GCGLfloat v0, GCGLfloat v1)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform2f(location, v0, v1);
}

void GraphicsContextGLANGLE::uniform2fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 2));
    if (!makeContextCurrent())
        return;

    GL_Uniform2fv(location, array.bufSize / 2, array.data);
}

void GraphicsContextGLANGLE::uniform3f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform3f(location, v0, v1, v2);
}

void GraphicsContextGLANGLE::uniform3fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 3));
    if (!makeContextCurrent())
        return;

    GL_Uniform3fv(location, array.bufSize / 3, array.data);
}

void GraphicsContextGLANGLE::uniform4f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform4f(location, v0, v1, v2, v3);
}

void GraphicsContextGLANGLE::uniform4fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 4));
    if (!makeContextCurrent())
        return;

    GL_Uniform4fv(location, array.bufSize / 4, array.data);
}

void GraphicsContextGLANGLE::uniform1i(GCGLint location, GCGLint v0)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1i(location, v0);
}

void GraphicsContextGLANGLE::uniform1iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1iv(location, array.bufSize, array.data);
}

void GraphicsContextGLANGLE::uniform2i(GCGLint location, GCGLint v0, GCGLint v1)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform2i(location, v0, v1);
}

void GraphicsContextGLANGLE::uniform2iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 2));
    if (!makeContextCurrent())
        return;

    GL_Uniform2iv(location, array.bufSize / 2, array.data);
}

void GraphicsContextGLANGLE::uniform3i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform3i(location, v0, v1, v2);
}

void GraphicsContextGLANGLE::uniform3iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 3));
    if (!makeContextCurrent())
        return;

    GL_Uniform3iv(location, array.bufSize / 3, array.data);
}

void GraphicsContextGLANGLE::uniform4i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2, GCGLint v3)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform4i(location, v0, v1, v2, v3);
}

void GraphicsContextGLANGLE::uniform4iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 4));
    if (!makeContextCurrent())
        return;

    GL_Uniform4iv(location, array.bufSize / 4, array.data);
}

void GraphicsContextGLANGLE::uniformMatrix2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix2fv(location, array.bufSize / 4, transpose, array.data);
}

void GraphicsContextGLANGLE::uniformMatrix3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 9));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix3fv(location, array.bufSize / 9, transpose, array.data);
}

void GraphicsContextGLANGLE::uniformMatrix4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 16));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix4fv(location, array.bufSize / 16, transpose, array.data);
}

void GraphicsContextGLANGLE::useProgram(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return;

    GL_UseProgram(program);
}

void GraphicsContextGLANGLE::validateProgram(PlatformGLObject program)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return;

    GL_ValidateProgram(program);
}

void GraphicsContextGLANGLE::vertexAttrib1f(GCGLuint index, GCGLfloat v0)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib1f(index, v0);
}

void GraphicsContextGLANGLE::vertexAttrib1fv(GCGLuint index, GCGLSpan<const GCGLfloat, 1> array)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib1fv(index, array.data);
}

void GraphicsContextGLANGLE::vertexAttrib2f(GCGLuint index, GCGLfloat v0, GCGLfloat v1)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib2f(index, v0, v1);
}

void GraphicsContextGLANGLE::vertexAttrib2fv(GCGLuint index, GCGLSpan<const GCGLfloat, 2> array)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib2fv(index, array.data);
}

void GraphicsContextGLANGLE::vertexAttrib3f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib3f(index, v0, v1, v2);
}

void GraphicsContextGLANGLE::vertexAttrib3fv(GCGLuint index, GCGLSpan<const GCGLfloat, 3> array)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib3fv(index, array.data);
}

void GraphicsContextGLANGLE::vertexAttrib4f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib4f(index, v0, v1, v2, v3);
}

void GraphicsContextGLANGLE::vertexAttrib4fv(GCGLuint index, GCGLSpan<const GCGLfloat, 4> array)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib4fv(index, array.data);
}

void GraphicsContextGLANGLE::vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContextGLANGLE::vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribIPointer(index, size, type, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContextGLANGLE::viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_Viewport(x, y, width, height);
}

PlatformGLObject GraphicsContextGLANGLE::createVertexArray()
{
    if (!makeContextCurrent())
        return 0;

    GLuint array = 0;
    if (m_isForWebGL2)
        GL_GenVertexArrays(1, &array);
    else
        GL_GenVertexArraysOES(1, &array);
    return array;
}

void GraphicsContextGLANGLE::deleteVertexArray(PlatformGLObject array)
{
    if (!array)
        return;
    if (!makeContextCurrent())
        return;
    if (m_isForWebGL2)
        GL_DeleteVertexArrays(1, &array);
    else
        GL_DeleteVertexArraysOES(1, &array);
}

GCGLboolean GraphicsContextGLANGLE::isVertexArray(PlatformGLObject array)
{
    if (!array)
        return GL_FALSE;
    if (!makeContextCurrent())
        return GL_FALSE;

    if (m_isForWebGL2)
        return GL_IsVertexArray(array);
    return GL_IsVertexArrayOES(array);
}

void GraphicsContextGLANGLE::bindVertexArray(PlatformGLObject array)
{
    if (!makeContextCurrent())
        return;
    if (m_isForWebGL2)
        GL_BindVertexArray(array);
    else
        GL_BindVertexArrayOES(array);
}

void GraphicsContextGLANGLE::getBooleanv(GCGLenum pname, GCGLSpan<GCGLboolean> value)
{
    if (!makeContextCurrent())
        return;

    GL_GetBooleanvRobustANGLE(pname, value.bufSize, nullptr, value.data);
}

GCGLint GraphicsContextGLANGLE::getBufferParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetBufferParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLANGLE::getFloatv(GCGLenum pname, GCGLSpan<GCGLfloat> value)
{
    if (!makeContextCurrent())
        return;

    GL_GetFloatvRobustANGLE(pname, value.bufSize, nullptr, value.data);
}
    
GCGLint64 GraphicsContextGLANGLE::getInteger64(GCGLenum pname)
{
    GCGLint64 value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetInteger64vRobustANGLE(pname, 1, nullptr, &value);
    return value;
}

GCGLint64 GraphicsContextGLANGLE::getInteger64i(GCGLenum pname, GCGLuint index)
{
    GCGLint64 value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetInteger64i_vRobustANGLE(pname, index, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getFramebufferAttachmentParameteri(GCGLenum target, GCGLenum attachment, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    GL_GetFramebufferAttachmentParameterivRobustANGLE(target, attachment, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getProgrami(PlatformGLObject program, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetProgramivRobustANGLE(program, pname, 1, nullptr, &value);
    return value;
}

String GraphicsContextGLANGLE::getUnmangledInfoLog(PlatformGLObject shaders[2], GCGLsizei count, const String& log)
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

String GraphicsContextGLANGLE::getProgramInfoLog(PlatformGLObject program)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return String();

    GLint length = 0;
    GL_GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return String(); 

    GLsizei size = 0;
    Vector<GLchar> info(length);
    GL_GetProgramInfoLog(program, length, &size, info.data());

    GCGLsizei count;
    PlatformGLObject shaders[2];
    getAttachedShaders(program, 2, &count, shaders);

    return getUnmangledInfoLog(shaders, count, String(info.data(), size));
}

GCGLint GraphicsContextGLANGLE::getRenderbufferParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetRenderbufferParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getShaderi(PlatformGLObject shader, GCGLenum pname)
{
    ASSERT(shader);
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetShaderivRobustANGLE(shader, pname, 1, nullptr, &value);
    return value;
}

String GraphicsContextGLANGLE::getShaderInfoLog(PlatformGLObject shader)
{
    ASSERT(shader);

    if (!makeContextCurrent())
        return String();

    GLint length = 0;
    GL_GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return String(); 

    GLsizei size = 0;
    Vector<GLchar> info(length);
    GL_GetShaderInfoLog(shader, length, &size, info.data());

    PlatformGLObject shaders[2] = { shader, 0 };
    return getUnmangledInfoLog(shaders, 1, String(info.data(), size));
}

String GraphicsContextGLANGLE::getShaderSource(PlatformGLObject)
{
    return emptyString();
}

GCGLfloat GraphicsContextGLANGLE::getTexParameterf(GCGLenum target, GCGLenum pname)
{
    GCGLfloat value = 0.f;
    if (!makeContextCurrent())
        return value;
    GL_GetTexParameterfvRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getTexParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetTexParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLANGLE::getUniformfv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLfloat> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.bufSize * sizeof(*value);
    GL_GetUniformfvRobustANGLE(program, location, bufSize, nullptr, value.data);
}

void GraphicsContextGLANGLE::getUniformiv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLint> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.bufSize * sizeof(*value);
    GL_GetUniformivRobustANGLE(program, location, bufSize, nullptr, value.data);
}

void GraphicsContextGLANGLE::getUniformuiv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLuint> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.bufSize * sizeof(*value);
    GL_GetUniformuivRobustANGLE(program, location, bufSize, nullptr, value.data);
}

GCGLint GraphicsContextGLANGLE::getUniformLocation(PlatformGLObject program, const String& name)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return -1;

    return GL_GetUniformLocation(program, name.utf8().data());
}

GCGLsizeiptr GraphicsContextGLANGLE::getVertexAttribOffset(GCGLuint index, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLvoid* pointer = 0;
    GL_GetVertexAttribPointervRobustANGLE(index, pname, 1, nullptr, &pointer);
    return static_cast<GCGLsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

PlatformGLObject GraphicsContextGLANGLE::createBuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    GL_GenBuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLANGLE::createFramebuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    GL_GenFramebuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLANGLE::createProgram()
{
    if (!makeContextCurrent())
        return 0;

    return GL_CreateProgram();
}

PlatformGLObject GraphicsContextGLANGLE::createRenderbuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    GL_GenRenderbuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLANGLE::createShader(GCGLenum type)
{
    if (!makeContextCurrent())
        return 0;

    return GL_CreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

PlatformGLObject GraphicsContextGLANGLE::createTexture()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    GL_GenTextures(1, &o);
    invalidateKnownTextureContent(o);
    return o;
}

void GraphicsContextGLANGLE::deleteBuffer(PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteBuffers(1, &buffer);
}

void GraphicsContextGLANGLE::deleteFramebuffer(PlatformGLObject framebuffer)
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
    GL_DeleteFramebuffers(1, &framebuffer);
}

void GraphicsContextGLANGLE::deleteProgram(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteProgram(program);
}

void GraphicsContextGLANGLE::deleteRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContextGLANGLE::deleteShader(PlatformGLObject shader)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteShader(shader);
}

void GraphicsContextGLANGLE::deleteTexture(PlatformGLObject texture)
{
    if (!makeContextCurrent())
        return;

    m_state.boundTextureMap.removeIf([texture] (auto& keyValue) {
        return keyValue.value.first == texture;
    });
    GL_DeleteTextures(1, &texture);
    invalidateKnownTextureContent(texture);
}

void GraphicsContextGLANGLE::synthesizeGLError(GCGLenum error)
{
    // Need to move the current errors to the synthetic error list to
    // preserve the order of errors, so a caller to getError will get
    // any errors from GL_Error before the error we are synthesizing.
    moveErrorsToSyntheticErrorList();
    m_syntheticErrors.add(error);
}

void GraphicsContextGLANGLE::forceContextLost()
{
    for (auto* client : copyToVector(m_clients))
        client->forceContextLost();
}

void GraphicsContextGLANGLE::recycleContext()
{
    for (auto* client : copyToVector(m_clients))
        client->recycleContext();
}

void GraphicsContextGLANGLE::dispatchContextChangedNotification()
{
    for (auto* client : copyToVector(m_clients))
        client->dispatchContextChangedNotification();
}

void GraphicsContextGLANGLE::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    if (!makeContextCurrent())
        return;

    if (m_isForWebGL2)
        GL_DrawArraysInstanced(mode, first, count, primcount);
    else
        GL_DrawArraysInstancedANGLE(mode, first, count, primcount);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount)
{
    if (!makeContextCurrent())
        return;

    if (m_isForWebGL2)
        GL_DrawElementsInstanced(mode, count, type, reinterpret_cast<void*>(offset), primcount);
    else
        GL_DrawElementsInstancedANGLE(mode, count, type, reinterpret_cast<void*>(offset), primcount);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    if (!makeContextCurrent())
        return;

    if (m_isForWebGL2)
        GL_VertexAttribDivisor(index, divisor);
    else
        GL_VertexAttribDivisorANGLE(index, divisor);
}

GCGLuint GraphicsContextGLANGLE::getUniformBlockIndex(PlatformGLObject program, const String& uniformBlockName)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return GL_INVALID_INDEX;

    return GL_GetUniformBlockIndex(program, uniformBlockName.utf8().data());
}

String GraphicsContextGLANGLE::getActiveUniformBlockName(PlatformGLObject program, GCGLuint uniformBlockIndex)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return String();

    GLint maxLength = 0;
    GL_GetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxLength);
    if (maxLength <= 0) {
        synthesizeGLError(INVALID_VALUE);
        return String();
    }
    Vector<GLchar> buffer(maxLength);
    GLsizei length = 0;
    GL_GetActiveUniformBlockName(program, uniformBlockIndex, buffer.size(), &length, buffer.data());
    if (!length)
        return String();
    return String(buffer.data(), length);
}

void GraphicsContextGLANGLE::uniformBlockBinding(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    GL_UniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

// Query Functions

PlatformGLObject GraphicsContextGLANGLE::createQuery()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    GL_GenQueries(1, &name);
    return name;
}

void GraphicsContextGLANGLE::beginQuery(GCGLenum target, PlatformGLObject query)
{
    if (!makeContextCurrent())
        return;

    GL_BeginQuery(target, query);
}

void GraphicsContextGLANGLE::endQuery(GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    GL_EndQuery(target);
}

GCGLuint GraphicsContextGLANGLE::getQueryObjectui(GCGLuint id, GCGLenum pname)
{
    GCGLuint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetQueryObjectuivRobustANGLE(id, pname, 1, nullptr, &value);
    return value;
}

// Transform Feedback Functions

PlatformGLObject GraphicsContextGLANGLE::createTransformFeedback()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    GL_GenTransformFeedbacks(1, &name);
    return name;
}

void GraphicsContextGLANGLE::deleteTransformFeedback(PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteTransformFeedbacks(1, &transformFeedback);
}

GCGLboolean GraphicsContextGLANGLE::isTransformFeedback(PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsTransformFeedback(transformFeedback);
}

void GraphicsContextGLANGLE::bindTransformFeedback(GCGLenum target, PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return;

    GL_BindTransformFeedback(target, transformFeedback);
}

void GraphicsContextGLANGLE::beginTransformFeedback(GCGLenum primitiveMode)
{
    if (!makeContextCurrent())
        return;

    GL_BeginTransformFeedback(primitiveMode);
}

void GraphicsContextGLANGLE::endTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    GL_EndTransformFeedback();
}

void GraphicsContextGLANGLE::transformFeedbackVaryings(PlatformGLObject program, const Vector<String>& varyings, GCGLenum bufferMode)
{
    if (!makeContextCurrent())
        return;

    Vector<CString> convertedVaryings = varyings.map([](const String& varying) {
        return varying.utf8();
    });
    Vector<const char*> pointersToVaryings = convertedVaryings.map([](const CString& varying) {
        return varying.data();
    });

    GL_TransformFeedbackVaryings(program, pointersToVaryings.size(), pointersToVaryings.data(), bufferMode);
}

void GraphicsContextGLANGLE::getTransformFeedbackVarying(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!makeContextCurrent())
        return;

    GCGLsizei bufSize = 0;
    GL_GetProgramiv(program, GraphicsContextGLANGLE::TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &bufSize);
    if (!bufSize)
        return;

    GCGLsizei length = 0;
    GCGLsizei size = 0;
    GCGLenum type = 0;
    Vector<GCGLchar> name(bufSize);

    GL_GetTransformFeedbackVarying(program, index, bufSize, &length, &size, &type, name.data());

    info.name = String(name.data(), length);
    info.size = size;
    info.type = type;
}

void GraphicsContextGLANGLE::bindBufferBase(GCGLenum target, GCGLuint index, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GL_BindBufferBase(target, index, buffer);
}

void GraphicsContextGLANGLE::blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter)
{
    if (!makeContextCurrent())
        return;

    GL_BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void GraphicsContextGLANGLE::framebufferTextureLayer(GCGLenum target, GCGLenum attachment, PlatformGLObject texture, GCGLint level, GCGLint layer)
{
    if (!makeContextCurrent())
        return;

    GL_FramebufferTextureLayer(target, attachment, texture, level, layer);
}

void GraphicsContextGLANGLE::invalidateFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments)
{
    if (!makeContextCurrent())
        return;

    GL_InvalidateFramebuffer(target, attachments.bufSize, attachments.data);
}

void GraphicsContextGLANGLE::invalidateSubFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_InvalidateSubFramebuffer(target, attachments.bufSize, attachments.data, x, y, width, height);
}

void GraphicsContextGLANGLE::readBuffer(GCGLenum src)
{
    if (!makeContextCurrent())
        return;

    GL_ReadBuffer(src);
}

void GraphicsContextGLANGLE::copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        GL_BindFramebuffer(framebufferTarget, m_fbo);
    }
    GL_CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        GL_BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

GCGLint GraphicsContextGLANGLE::getFragDataLocation(PlatformGLObject program, const String& name)
{
    if (!makeContextCurrent())
        return -1;

    return GL_GetFragDataLocation(program, name.utf8().data());
}

void GraphicsContextGLANGLE::uniform1ui(GCGLint location, GCGLuint v0)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1ui(location, v0);
}

void GraphicsContextGLANGLE::uniform2ui(GCGLint location, GCGLuint v0, GCGLuint v1)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform2ui(location, v0, v1);
}

void GraphicsContextGLANGLE::uniform3ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform3ui(location, v0, v1, v2);
}

void GraphicsContextGLANGLE::uniform4ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform4ui(location, v0, v1, v2, v3);
}

void GraphicsContextGLANGLE::uniform1uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1uiv(location, data.bufSize, data.data);
}

void GraphicsContextGLANGLE::uniform2uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    ASSERT(!(data.bufSize % 2));
    if (!makeContextCurrent())
        return;

    GL_Uniform2uiv(location, data.bufSize / 2, data.data);
}

void GraphicsContextGLANGLE::uniform3uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    ASSERT(!(data.bufSize % 3));
    if (!makeContextCurrent())
        return;

    GL_Uniform3uiv(location, data.bufSize / 3, data.data);
}

void GraphicsContextGLANGLE::uniform4uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    ASSERT(!(data.bufSize % 4));
    if (!makeContextCurrent())
        return;

    GL_Uniform4uiv(location, data.bufSize / 4, data.data);
}

void GraphicsContextGLANGLE::uniformMatrix2x3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 6));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix2x3fv(location, data.bufSize / 6, transpose, data.data);
}

void GraphicsContextGLANGLE::uniformMatrix3x2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 6));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix3x2fv(location, data.bufSize / 6, transpose, data.data);
}

void GraphicsContextGLANGLE::uniformMatrix2x4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 8));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix2x4fv(location, data.bufSize / 8, transpose, data.data);
}

void GraphicsContextGLANGLE::uniformMatrix4x2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 8));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix4x2fv(location, data.bufSize / 8, transpose, data.data);
}

void GraphicsContextGLANGLE::uniformMatrix3x4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 12));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix3x4fv(location, data.bufSize / 12, transpose, data.data);
}

void GraphicsContextGLANGLE::uniformMatrix4x3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    ASSERT(!(data.bufSize % 12));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix4x3fv(location, data.bufSize / 12, transpose, data.data);
}

void GraphicsContextGLANGLE::vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribI4i(index, x, y, z, w);
}

void GraphicsContextGLANGLE::vertexAttribI4iv(GCGLuint index, GCGLSpan<const GCGLint, 4> values)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribI4iv(index, values.data);
}

void GraphicsContextGLANGLE::vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribI4ui(index, x, y, z, w);
}

void GraphicsContextGLANGLE::vertexAttribI4uiv(GCGLuint index, GCGLSpan<const GCGLuint, 4> values)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribI4uiv(index, values.data);
}

void GraphicsContextGLANGLE::drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    GL_DrawRangeElements(mode, start, end, count, type, reinterpret_cast<void*>(offset));
}

void GraphicsContextGLANGLE::drawBuffers(GCGLSpan<const GCGLenum> bufs)
{
    if (!makeContextCurrent())
        return;

    GL_DrawBuffers(bufs.bufSize, bufs.data);
}

void GraphicsContextGLANGLE::clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLint> values)
{
    if (!makeContextCurrent())
        return;

    GL_ClearBufferiv(buffer, drawbuffer, values.data);
}

void GraphicsContextGLANGLE::clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLuint> values)
{
    if (!makeContextCurrent())
        return;

    GL_ClearBufferuiv(buffer, drawbuffer, values.data);
}

void GraphicsContextGLANGLE::clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLfloat> values)
{
    if (!makeContextCurrent())
        return;

    GL_ClearBufferfv(buffer, drawbuffer, values.data);
}

void GraphicsContextGLANGLE::clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil)
{
    if (!makeContextCurrent())
        return;

    GL_ClearBufferfi(buffer, drawbuffer, depth, stencil);
}

void GraphicsContextGLANGLE::deleteQuery(PlatformGLObject query)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteQueries(1, &query);
}

GCGLboolean GraphicsContextGLANGLE::isQuery(PlatformGLObject query)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsQuery(query);
}

PlatformGLObject GraphicsContextGLANGLE::getQuery(GCGLenum target, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLint value;
    GL_GetQueryiv(target, pname, &value);
    return static_cast<PlatformGLObject>(value);
}

PlatformGLObject GraphicsContextGLANGLE::createSampler()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    GL_GenSamplers(1, &name);
    return name;
}

void GraphicsContextGLANGLE::deleteSampler(PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteSamplers(1, &sampler);
}

GCGLboolean GraphicsContextGLANGLE::isSampler(PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsSampler(sampler);
}

void GraphicsContextGLANGLE::bindSampler(GCGLuint unit, PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return;

    GL_BindSampler(unit, sampler);
}

void GraphicsContextGLANGLE::samplerParameteri(PlatformGLObject sampler, GCGLenum pname, GCGLint param)
{
    if (!makeContextCurrent())
        return;

    GL_SamplerParameteri(sampler, pname, param);
}

void GraphicsContextGLANGLE::samplerParameterf(PlatformGLObject sampler, GCGLenum pname, GCGLfloat param)
{
    if (!makeContextCurrent())
        return;

    GL_SamplerParameterf(sampler, pname, param);
}

GCGLfloat GraphicsContextGLANGLE::getSamplerParameterf(PlatformGLObject sampler, GCGLenum pname)
{
    GCGLfloat value = 0.f;
    if (!makeContextCurrent())
        return value;

    GL_GetSamplerParameterfvRobustANGLE(sampler, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getSamplerParameteri(PlatformGLObject sampler, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;

    GL_GetSamplerParameterivRobustANGLE(sampler, pname, 1, nullptr, &value);
    return value;
}

GCGLsync GraphicsContextGLANGLE::fenceSync(GCGLenum condition, GCGLbitfield flags)
{
    if (!makeContextCurrent())
        return 0;

    return GL_FenceSync(condition, flags);
}

GCGLboolean GraphicsContextGLANGLE::isSync(GCGLsync sync)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsSync(static_cast<GLsync>(sync));
}

void GraphicsContextGLANGLE::deleteSync(GCGLsync sync)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteSync(static_cast<GLsync>(sync));
}

GCGLenum GraphicsContextGLANGLE::clientWaitSync(GCGLsync sync, GCGLbitfield flags, GCGLuint64 timeout)
{
    if (!makeContextCurrent())
        return GL_WAIT_FAILED;

    return GL_ClientWaitSync(static_cast<GLsync>(sync), flags, timeout);
}

void GraphicsContextGLANGLE::waitSync(GCGLsync sync, GCGLbitfield flags, GCGLint64 timeout)
{
    if (!makeContextCurrent())
        return;

    GL_WaitSync(static_cast<GLsync>(sync), flags, timeout);
}

GCGLint GraphicsContextGLANGLE::getSynci(GCGLsync sync, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;

    GL_GetSynciv(static_cast<GLsync>(sync), pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLANGLE::pauseTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    GL_PauseTransformFeedback();
}

void GraphicsContextGLANGLE::resumeTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    GL_ResumeTransformFeedback();
}

void GraphicsContextGLANGLE::bindBufferRange(GCGLenum target, GCGLuint index, PlatformGLObject buffer, GCGLintptr offset, GCGLsizeiptr size)
{
    if (!makeContextCurrent())
        return;

    GL_BindBufferRange(target, index, buffer, offset, size);
}

Vector<GCGLuint> GraphicsContextGLANGLE::getUniformIndices(PlatformGLObject program, const Vector<String>& uniformNames)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return { };

    Vector<CString> utf8 = uniformNames.map([](auto& x) { return x.utf8(); });
    Vector<const char*> cstr = utf8.map([](auto& x) { return x.data(); });
    Vector<GCGLuint> result(cstr.size(), 0);
    GL_GetUniformIndices(program, cstr.size(), cstr.data(), result.data());
    return result;
}

void GraphicsContextGLANGLE::getActiveUniformBlockiv(GCGLuint program, GCGLuint uniformBlockIndex, GCGLenum pname, GCGLSpan<GCGLint> params)
{
    if (!makeContextCurrent())
        return;
    GL_GetActiveUniformBlockivRobustANGLE(program, uniformBlockIndex, pname, params.bufSize, nullptr, params.data);
}

void GraphicsContextGLANGLE::multiDrawArraysANGLE(GCGLenum mode, GCGLSpan<const GCGLint> firsts, GCGLSpan<const GCGLsizei> counts, GCGLsizei drawcount)
{
    if (!makeContextCurrent())
        return;

    GL_MultiDrawArraysANGLE(mode, firsts.data, counts.data, drawcount);
}

void GraphicsContextGLANGLE::multiDrawArraysInstancedANGLE(GCGLenum mode, GCGLSpan<const GCGLint> firsts, GCGLSpan<const GCGLsizei> counts, GCGLSpan<const GCGLsizei> instanceCounts, GCGLsizei drawcount)
{
    if (!makeContextCurrent())
        return;

    GL_MultiDrawArraysInstancedANGLE(mode, firsts.data, counts.data, instanceCounts.data, drawcount);
}

void GraphicsContextGLANGLE::multiDrawElementsANGLE(GCGLenum mode, GCGLSpan<const GCGLsizei> counts, GCGLenum type, GCGLSpan<const GCGLint> offsets, GCGLsizei drawcount)
{
    if (!makeContextCurrent())
        return;

    // Must perform conversion from integer offsets to void* pointers before passing down to ANGLE.
    Vector<void*> pointers;
    for (size_t i = 0; i < offsets.bufSize; ++i)
        pointers.append(reinterpret_cast<void*>(offsets[i]));

    GL_MultiDrawElementsANGLE(mode, counts.data, type, pointers.data(), drawcount);
}

void GraphicsContextGLANGLE::multiDrawElementsInstancedANGLE(GCGLenum mode, GCGLSpan<const GCGLsizei> counts, GCGLenum type, GCGLSpan<const GCGLint> offsets, GCGLSpan<const GCGLsizei> instanceCounts, GCGLsizei drawcount)
{
    if (!makeContextCurrent())
        return;

    // Must perform conversion from integer offsets to void* pointers before passing down to ANGLE.
    Vector<void*> pointers;
    for (size_t i = 0; i < offsets.bufSize; ++i)
        pointers.append(reinterpret_cast<void*>(offsets[i]));

    GL_MultiDrawElementsInstancedANGLE(mode, counts.data, type, pointers.data(), instanceCounts.data, drawcount);
}

bool GraphicsContextGLANGLE::supportsExtension(const String& name)
{
    return m_availableExtensions.contains(name) || m_requestableExtensions.contains(name);
}

void GraphicsContextGLANGLE::ensureExtensionEnabled(const String& name)
{
    // Enable support in ANGLE (if not enabled already).
    if (m_requestableExtensions.contains(name) && !m_enabledExtensions.contains(name)) {
        if (!makeContextCurrent())
            return;
        GL_RequestExtensionANGLE(name.ascii().data());
        m_enabledExtensions.add(name);

        if (name == "GL_CHROMIUM_color_buffer_float_rgba"_s)
            m_webglColorBufferFloatRGBA = true;
        else if (name == "GL_CHROMIUM_color_buffer_float_rgb"_s)
            m_webglColorBufferFloatRGB = true;
    }
}

bool GraphicsContextGLANGLE::isExtensionEnabled(const String& name)
{
    return m_availableExtensions.contains(name) || m_enabledExtensions.contains(name);
}

GLint GraphicsContextGLANGLE::getGraphicsResetStatusARB()
{
    return GraphicsContextGL::NO_ERROR;
}

String GraphicsContextGLANGLE::getTranslatedShaderSourceANGLE(PlatformGLObject shader)
{
    if (!makeContextCurrent())
        return String();

    int sourceLength = getShaderi(shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE);

    if (!sourceLength)
        return emptyString();
    Vector<GLchar> name(sourceLength); // GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE includes null termination.
    GCGLint returnedLength = 0;
    GL_GetTranslatedShaderSourceANGLE(shader, sourceLength, &returnedLength, name.data());
    if (!returnedLength)
        return emptyString();
    // returnedLength does not include the null terminator.
    ASSERT(returnedLength == sourceLength - 1);
    return String(name.data(), returnedLength);
}

void GraphicsContextGLANGLE::drawBuffersEXT(GCGLSpan<const GCGLenum> bufs)
{
    if (!makeContextCurrent())
        return;

    GL_DrawBuffersEXT(bufs.bufSize, bufs.data);
}

bool GraphicsContextGLANGLE::waitAndUpdateOldestFrame()
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
        GLenum result = GL_ClientWaitSync(static_cast<GLsync>(fence.get()), flags, maxFrameDuration.nanosecondsAs<GLuint64>());
        ASSERT(result != GL_WAIT_FAILED);
        success = result != GL_WAIT_FAILED && result != GL_TIMEOUT_EXPIRED;
    }
    m_frameCompletionFences[oldestFrameCompletionFence].fenceSync();
    return success;
}

void GraphicsContextGLANGLE::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (event == SimulatedEventForTesting::ContextChange) {
        dispatchContextChangedNotification();
        return;
    }
    if (event == SimulatedEventForTesting::GPUStatusFailure) {
        m_failNextStatusCheck = true;
        return;
    }
}

bool GraphicsContextGLANGLE::isGLES2Compliant() const
{
    return m_isForWebGL2;
}

void GraphicsContextGLANGLE::paintRenderingResultsToCanvas(ImageBuffer& imageBuffer)
{
    if (!makeContextCurrent())
        return;
    if (getInternalFramebufferSize().isEmpty())
        return;
    auto pixelBuffer = readRenderingResults();
    if (!pixelBuffer)
        return;
    paintToCanvas(contextAttributes(), WTFMove(*pixelBuffer), imageBuffer.backendSize(), imageBuffer.context());
}

void GraphicsContextGLANGLE::paintCompositedResultsToCanvas(ImageBuffer& imageBuffer)
{
    if (!makeContextCurrent())
        return;
    if (getInternalFramebufferSize().isEmpty())
        return;
    auto pixelBuffer = readCompositedResults();
    if (!pixelBuffer)
        return;
    paintToCanvas(contextAttributes(), WTFMove(*pixelBuffer), imageBuffer.backendSize(), imageBuffer.context());
}

std::optional<PixelBuffer> GraphicsContextGLANGLE::paintRenderingResultsToPixelBuffer()
{
    // Reading premultiplied alpha would involve unpremultiplying, which is lossy.
    if (contextAttributes().premultipliedAlpha)
        return std::nullopt;
    auto results = readRenderingResultsForPainting();
    if (results && !results->size().isEmpty()) {
        ASSERT(results->format().pixelFormat == PixelFormat::RGBA8 || results->format().pixelFormat == PixelFormat::BGRA8);
        // FIXME: Make PixelBufferConversions support negative rowBytes and in-place conversions.
        const auto size = results->size();
        const size_t rowStride = size.width() * 4;
        uint8_t* top = results->data().data();
        uint8_t* bottom = top + (size.height() - 1) * rowStride;
        std::unique_ptr<uint8_t[]> temp(new uint8_t[rowStride]);
        for (; top < bottom; top += rowStride, bottom -= rowStride) {
            memcpy(temp.get(), bottom, rowStride);
            memcpy(bottom, top, rowStride);
            memcpy(top, temp.get(), rowStride);
        }
    }
    return results;
}

std::optional<PixelBuffer> GraphicsContextGLANGLE::readRenderingResultsForPainting()
{
    if (!makeContextCurrent())
        return std::nullopt;
    if (getInternalFramebufferSize().isEmpty())
        return std::nullopt;
    return readRenderingResults();
}

std::optional<PixelBuffer> GraphicsContextGLANGLE::readCompositedResultsForPainting()
{
    if (!makeContextCurrent())
        return std::nullopt;
    if (getInternalFramebufferSize().isEmpty())
        return std::nullopt;
    return readCompositedResults();
}

void GraphicsContextGLANGLE::invalidateKnownTextureContent(GCGLuint)
{
}

GCGLenum GraphicsContextGLANGLE::adjustWebGL1TextureInternalFormat(GCGLenum internalformat, GCGLenum format, GCGLenum type)
{
    // The implementation of WEBGL_color_buffer_float for WebGL 1.0 / ES 2.0 requires a sized
    // internal format. Adjust it if necessary at this lowest level.
    if (type == GL_FLOAT) {
        if (m_webglColorBufferFloatRGBA && format == GL_RGBA && internalformat == GL_RGBA)
            return GL_RGBA32F;
        if (m_webglColorBufferFloatRGB && format == GL_RGB && internalformat == GL_RGB)
            return GL_RGB32F;
    }
    return internalformat;
}

}

#endif // ENABLE(WEBGL) && USE(ANGLE)
