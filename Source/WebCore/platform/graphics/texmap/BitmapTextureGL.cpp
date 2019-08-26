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
#include "BitmapTextureGL.h"

#if USE(TEXTURE_MAPPER_GL)

#include "Extensions3D.h"
#include "FilterOperations.h"
#include "Image.h"
#include "LengthFunctions.h"
#include "NativeImage.h"
#include "NotImplemented.h"
#include "TextureMapperShaderProgram.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if USE(CAIRO)
#include "CairoUtilities.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <wtf/text/CString.h>
#endif

#if USE(DIRECT2D)
#include <d2d1.h>
#include <wincodec.h>
#endif

#if OS(DARWIN)
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif

namespace WebCore {

BitmapTextureGL* toBitmapTextureGL(BitmapTexture* texture)
{
    if (!texture || !texture->isBackedByOpenGL())
        return 0;

    return static_cast<BitmapTextureGL*>(texture);
}

BitmapTextureGL::BitmapTextureGL(const TextureMapperContextAttributes& contextAttributes, const Flags, GLint internalFormat)
    : m_contextAttributes(contextAttributes)
{
    if (internalFormat != GL_DONT_CARE) {
        m_internalFormat = m_format = internalFormat;
        return;
    }

    m_internalFormat = m_format = GL_RGBA;
}

void BitmapTextureGL::didReset()
{
    if (!m_id)
        glGenTextures(1, &m_id);

    m_shouldClear = true;
    m_colorConvertFlags = TextureMapperGL::NoFlag;
    if (m_textureSize == contentSize())
        return;

    m_textureSize = contentSize();
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_textureSize.width(), m_textureSize.height(), 0, m_format, m_type, 0);
}

void BitmapTextureGL::updateContents(const void* srcData, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine)
{
    // We are updating a texture with format RGBA with content from a buffer that has BGRA format. Instead of turning BGRA
    // into RGBA and then uploading it, we upload it as is. This causes the texture format to be RGBA but the content to be BGRA,
    // so we mark the texture to convert the colors when painting the texture.
    m_colorConvertFlags = TextureMapperGL::ShouldConvertTextureBGRAToRGBA;

    glBindTexture(GL_TEXTURE_2D, m_id);

    const unsigned bytesPerPixel = 4;
    const char* data = static_cast<const char*>(srcData);
    Vector<char> temporaryData;
    IntPoint adjustedSourceOffset = sourceOffset;

    // Texture upload requires subimage buffer if driver doesn't support subimage and we don't have full image upload.
    bool requireSubImageBuffer = !m_contextAttributes.supportsUnpackSubimage
        && !(bytesPerLine == static_cast<int>(targetRect.width() * bytesPerPixel) && adjustedSourceOffset == IntPoint::zero());

    // prepare temporaryData if necessary
    if (requireSubImageBuffer) {
        temporaryData.resize(targetRect.width() * targetRect.height() * bytesPerPixel);
        char* dst = temporaryData.data();
        data = dst;
        const char* bits = static_cast<const char*>(srcData);
        const char* src = bits + sourceOffset.y() * bytesPerLine + sourceOffset.x() * bytesPerPixel;
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

    if (m_contextAttributes.supportsUnpackSubimage) {
        // Use the OpenGL sub-image extension, now that we know it's available.
        glPixelStorei(GL_UNPACK_ROW_LENGTH, bytesPerLine / bytesPerPixel);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, adjustedSourceOffset.y());
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, adjustedSourceOffset.x());
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height(), m_format, m_type, data);

    if (m_contextAttributes.supportsUnpackSubimage) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }
}

void BitmapTextureGL::updateContents(Image* image, const IntRect& targetRect, const IntPoint& offset)
{
    if (!image)
        return;
    NativeImagePtr frameImage = image->nativeImageForCurrentFrame();
    if (!frameImage)
        return;

    int bytesPerLine;
    const char* imageData;

#if USE(CAIRO)
    cairo_surface_t* surface = frameImage.get();
    imageData = reinterpret_cast<const char*>(cairo_image_surface_get_data(surface));
    bytesPerLine = cairo_image_surface_get_stride(surface);
#elif USE(DIRECT2D)
    notImplemented();
#endif

    updateContents(imageData, targetRect, offset, bytesPerLine);
}

static unsigned getPassesRequiredForFilter(FilterOperation::OperationType type)
{
    switch (type) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
        return 1;
    case FilterOperation::BLUR:
    case FilterOperation::DROP_SHADOW:
        // We use two-passes (vertical+horizontal) for blur and drop-shadow.
        return 2;
    default:
        return 0;
    }
}

RefPtr<BitmapTexture> BitmapTextureGL::applyFilters(TextureMapper& textureMapper, const FilterOperations& filters)
{
    if (filters.isEmpty())
        return this;

    TextureMapperGL& texmapGL = static_cast<TextureMapperGL&>(textureMapper);
    RefPtr<BitmapTexture> previousSurface = texmapGL.currentSurface();
    RefPtr<BitmapTexture> resultSurface = this;
    RefPtr<BitmapTexture> intermediateSurface;
    RefPtr<BitmapTexture> spareSurface;

    m_filterInfo = FilterInfo();

    for (size_t i = 0; i < filters.size(); ++i) {
        RefPtr<FilterOperation> filter = filters.operations()[i];
        ASSERT(filter);

        int numPasses = getPassesRequiredForFilter(filter->type());
        for (int j = 0; j < numPasses; ++j) {
            bool last = (i == filters.size() - 1) && (j == numPasses - 1);
            if (!last) {
                if (!intermediateSurface)
                    intermediateSurface = texmapGL.acquireTextureFromPool(contentSize(), BitmapTexture::SupportsAlpha);
                texmapGL.bindSurface(intermediateSurface.get());
            }

            if (last) {
                toBitmapTextureGL(resultSurface.get())->m_filterInfo = BitmapTextureGL::FilterInfo(filter.copyRef(), j, spareSurface.copyRef());
                break;
            }

            texmapGL.drawFiltered(*resultSurface.get(), spareSurface.get(), *filter, j);
            if (!j && filter->type() == FilterOperation::DROP_SHADOW) {
                spareSurface = resultSurface;
                resultSurface = nullptr;
            }
            std::swap(resultSurface, intermediateSurface);
        }
    }

    texmapGL.bindSurface(previousSurface.get());
    return resultSurface;
}

void BitmapTextureGL::initializeStencil()
{
    if (m_rbo)
        return;

    glGenRenderbuffers(1, &m_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, m_textureSize.width(), m_textureSize.height());
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
}

void BitmapTextureGL::initializeDepthBuffer()
{
    if (m_depthBufferObject)
        return;

    glGenRenderbuffers(1, &m_depthBufferObject);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthBufferObject);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_textureSize.width(), m_textureSize.height());
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBufferObject);
}

void BitmapTextureGL::clearIfNeeded()
{
    if (!m_shouldClear)
        return;

    m_clipStack.reset(IntRect(IntPoint::zero(), m_textureSize), ClipStack::YAxisMode::Default);
    m_clipStack.applyIfNeeded();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shouldClear = false;
}

void BitmapTextureGL::createFboIfNeeded()
{
    if (m_fbo)
        return;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id(), 0);
    m_shouldClear = true;
}

void BitmapTextureGL::bindAsSurface()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    createFboIfNeeded();
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_textureSize.width(), m_textureSize.height());
    clearIfNeeded();
    m_clipStack.apply();
}

BitmapTextureGL::~BitmapTextureGL()
{
    if (m_id)
        glDeleteTextures(1, &m_id);

    if (m_fbo)
        glDeleteFramebuffers(1, &m_fbo);

    if (m_rbo)
        glDeleteRenderbuffers(1, &m_rbo);

    if (m_depthBufferObject)
        glDeleteRenderbuffers(1, &m_depthBufferObject);
}

bool BitmapTextureGL::isValid() const
{
    return m_id;
}

IntSize BitmapTextureGL::size() const
{
    return m_textureSize;
}


void BitmapTextureGL::copyFromExternalTexture(GLuint sourceTextureID)
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
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_textureSize.width(), m_textureSize.height());

    glBindTexture(GL_TEXTURE_2D, boundTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, boundFramebuffer);
    glBindTexture(GL_TEXTURE_2D, boundTexture);
    glActiveTexture(boundActiveTexture);
    glDeleteFramebuffers(1, &copyFbo);
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)
