/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2012 Igalia S.L.
 Copyright (C) 2012 Adobe Systems Incorporated

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "BitmapTexture.h"

#if USE(TEXTURE_MAPPER)

#include "GLContext.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "ImageBuffer.h"
#include "LengthFunctions.h"
#include "NativeImage.h"
#include "TextureMapper.h"
#include "TextureMapperFlags.h"
#include "TextureMapperShaderProgram.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if USE(CAIRO)
#include "CairoUtilities.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <wtf/text/CString.h>
#endif

#if OS(DARWIN)
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
static const GLenum s_pixelDataType = GL_UNSIGNED_INT_8_8_8_8_REV;
#else
static const GLenum s_pixelDataType = GL_UNSIGNED_BYTE;
#endif

// On GLES3, the format we want for packed depth stencil is GL_DEPTH24_STENCIL8, but when added through
// the extension this format is called GL_DEPTH24_STENCIL8_OES. In any case they hold the same value 0x88F0
// so we can just use the first one.
// These definitions may not exist if this is a GLES1/2 context without the GL_OES_packed_depth_stencil
// extension. We need to define the one we want to use in order to build on every case.
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

namespace WebCore {

GLenum depthBufferFormat()
{
    auto* glContext = GLContext::current();
    if (glContext->version() >= 300 || glContext->glExtensions().OES_packed_depth_stencil)
        return GL_DEPTH24_STENCIL8;

    return GL_DEPTH_COMPONENT16;
}

BitmapTexture::BitmapTexture(const IntSize& size, OptionSet<Flags> flags, GLint internalFormat)
    : m_flags(flags)
    , m_size(size)
    , m_internalFormat(internalFormat == GL_DONT_CARE ? GL_RGBA : internalFormat)
    , m_format(GL_RGBA)
{
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_size.width(), m_size.height(), 0, m_format, s_pixelDataType, nullptr);
}

void BitmapTexture::reset(const IntSize& size, OptionSet<Flags> flags)
{
    m_flags = flags;
    m_shouldClear = true;
    m_colorConvertFlags = { };
    m_filterOperation = nullptr;
    if (m_size != size) {
        m_size = size;
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_size.width(), m_size.height(), 0, m_format, s_pixelDataType, nullptr);
    }
}

void BitmapTexture::updateContents(const void* srcData, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine)
{
    // We are updating a texture with format RGBA with content from a buffer that has BGRA format. Instead of turning BGRA
    // into RGBA and then uploading it, we upload it as is. This causes the texture format to be RGBA but the content to be BGRA,
    // so we mark the texture to convert the colors when painting the texture.
#if CPU(LITTLE_ENDIAN)
    m_colorConvertFlags = TextureMapperFlags::ShouldConvertTextureBGRAToRGBA;
#else
    m_colorConvertFlags = TextureMapperFlags::ShouldConvertTextureARGBToRGBA;
#endif

    glBindTexture(GL_TEXTURE_2D, m_id);

    const unsigned bytesPerPixel = 4;
    auto data = static_cast<const uint8_t*>(srcData);
    Vector<uint8_t> temporaryData;
    IntPoint adjustedSourceOffset = sourceOffset;

    // Texture upload requires subimage buffer if driver doesn't support subimage and we don't have full image upload.
    bool supportsUnpackSubimage = GLContext::current()->glExtensions().EXT_unpack_subimage;
    bool requireSubImageBuffer = !supportsUnpackSubimage
        && !(bytesPerLine == static_cast<int>(targetRect.width() * bytesPerPixel) && adjustedSourceOffset == IntPoint::zero());

    // prepare temporaryData if necessary
    if (requireSubImageBuffer) {
        temporaryData.resize(targetRect.width() * targetRect.height() * bytesPerPixel);
        auto dst = temporaryData.data();
        data = dst;
        auto bits = static_cast<const uint8_t*>(srcData);
        auto src = bits + sourceOffset.y() * bytesPerLine + sourceOffset.x() * bytesPerPixel;
        const int targetBytesPerLine = targetRect.width() * bytesPerPixel;
        for (int y = 0; y < targetRect.height(); ++y) {
            memcpy(dst, src, targetBytesPerLine);
            src += bytesPerLine;
            dst += targetBytesPerLine;
        }

        bytesPerLine = targetBytesPerLine;
        adjustedSourceOffset = IntPoint(0, 0);
    }

    glBindTexture(GL_TEXTURE_2D, m_id);

    if (supportsUnpackSubimage) {
        // Use the OpenGL sub-image extension, now that we know it's available.
        glPixelStorei(GL_UNPACK_ROW_LENGTH, bytesPerLine / bytesPerPixel);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, adjustedSourceOffset.y());
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, adjustedSourceOffset.x());
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height(), m_format, s_pixelDataType, data);

    if (supportsUnpackSubimage) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }
}

void BitmapTexture::updateContents(NativeImage* frameImage, const IntRect& targetRect, const IntPoint& offset)
{
    if (!frameImage)
        return;

    int bytesPerLine;
    const uint8_t* imageData;

#if USE(CAIRO)
    cairo_surface_t* surface = frameImage->platformImage().get();
    imageData = cairo_image_surface_get_data(surface);
    bytesPerLine = cairo_image_surface_get_stride(surface);
#endif

    updateContents(imageData, targetRect, offset, bytesPerLine);
}

void BitmapTexture::updateContents(GraphicsLayer* sourceLayer, const IntRect& targetRect, const IntPoint& offset, float scale)
{
    // Making an unconditionally unaccelerated buffer here is OK because this code
    // isn't used by any platforms that respect the accelerated bit.
    auto imageBuffer = ImageBuffer::create(targetRect.size(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return;

    GraphicsContext& context = imageBuffer->context();
    context.setTextDrawingMode(TextDrawingMode::Fill);

    IntRect sourceRect(targetRect);
    sourceRect.setLocation(offset);
    sourceRect.scale(1 / scale);
    context.applyDeviceScaleFactor(scale);
    context.translate(-sourceRect.x(), -sourceRect.y());

    sourceLayer->paintGraphicsLayerContents(context, sourceRect);

    auto image = ImageBuffer::sinkIntoNativeImage(WTFMove(imageBuffer));
    if (!image)
        return;

    updateContents(image.get(), targetRect, IntPoint());
}

void BitmapTexture::initializeStencil()
{
    if (m_flags.contains(Flags::DepthBuffer)) {
        // We have a depth buffer and we're asked to have a stencil buffer as well. This is only
        // possible if packed depth stencil is available. If that's the case, just bind the depth
        // buffer as the stencil one if haven't done so. If packed depth stencil is not available
        // don't do anything, which will cause stencil clips on this surface to fail.
        if (depthBufferFormat() == GL_DEPTH24_STENCIL8 && !m_stencilBound) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthBufferObject);
            m_stencilBound = true;
        }
        return;
    }

    // We don't have a depth buffer. Use a stencil only buffer.
    if (m_stencilBufferObject)
        return;

    glGenRenderbuffers(1, &m_stencilBufferObject);
    glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, m_size.width(), m_size.height());
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBufferObject);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
}

void BitmapTexture::initializeDepthBuffer()
{
    if (m_depthBufferObject)
        return;

    glGenRenderbuffers(1, &m_depthBufferObject);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, depthBufferFormat(), m_size.width(), m_size.height());
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBufferObject);
}

void BitmapTexture::clearIfNeeded()
{
    if (!m_shouldClear)
        return;

    m_clipStack.reset(IntRect(IntPoint::zero(), m_size), ClipStack::YAxisMode::Default);
    m_clipStack.applyIfNeeded();
    glClearColor(0, 0, 0, 0);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    m_shouldClear = false;
}

void BitmapTexture::createFboIfNeeded()
{
    if (m_fbo)
        return;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id(), 0);
    if (m_flags.contains(Flags::DepthBuffer))
        initializeDepthBuffer();
    m_shouldClear = true;
}

void BitmapTexture::bindAsSurface()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    createFboIfNeeded();
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_size.width(), m_size.height());
    if (m_flags.contains(Flags::DepthBuffer))
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    clearIfNeeded();
    m_clipStack.apply();
}

BitmapTexture::~BitmapTexture()
{
    glDeleteTextures(1, &m_id);

    if (m_fbo)
        glDeleteFramebuffers(1, &m_fbo);

    if (m_depthBufferObject)
        glDeleteRenderbuffers(1, &m_depthBufferObject);

    if (m_stencilBufferObject)
        glDeleteRenderbuffers(1, &m_stencilBufferObject);
}

void BitmapTexture::copyFromExternalTexture(GLuint sourceTextureID)
{
    GLint boundTexture = 0;
    GLint boundFramebuffer = 0;
    GLint boundActiveTexture = 0;

    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFramebuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &boundActiveTexture);

    glBindTexture(GL_TEXTURE_2D, sourceTextureID);

    GLuint copyFbo = 0;
    glGenFramebuffers(1, &copyFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, copyFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sourceTextureID, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id());
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_size.width(), m_size.height());

    glBindTexture(GL_TEXTURE_2D, boundTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, boundFramebuffer);
    glBindTexture(GL_TEXTURE_2D, boundTexture);
    glActiveTexture(boundActiveTexture);
    glDeleteFramebuffers(1, &copyFbo);
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
