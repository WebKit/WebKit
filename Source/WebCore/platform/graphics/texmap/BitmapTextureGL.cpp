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
#include "GraphicsContext.h"
#include "Image.h"
#include "LengthFunctions.h"
#include "NotImplemented.h"
#include "TextureMapperShaderProgram.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/TemporaryChange.h>

#if USE(CAIRO)
#include "CairoUtilities.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <wtf/text/CString.h>
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

BitmapTextureGL::BitmapTextureGL(PassRefPtr<GraphicsContext3D> context3D)
    : m_id(0)
    , m_fbo(0)
    , m_rbo(0)
    , m_depthBufferObject(0)
    , m_shouldClear(true)
    , m_context3D(context3D)
#if OS(DARWIN)
    , m_type(GL_UNSIGNED_INT_8_8_8_8_REV)
#else
    , m_type(GraphicsContext3D::UNSIGNED_BYTE)
#endif
{
    // If GL_EXT_texture_format_BGRA8888 is supported in the OpenGLES
    // internal and external formats need to be BGRA
    m_internalFormat = GraphicsContext3D::RGBA;
    m_format = GraphicsContext3D::BGRA;
    if (m_context3D->isGLES2Compliant()) {
        if (m_context3D->getExtensions()->supports("GL_EXT_texture_format_BGRA8888"))
            m_internalFormat = GraphicsContext3D::BGRA;
        else
            m_format = GraphicsContext3D::RGBA;
    }
}

static void swizzleBGRAToRGBA(uint32_t* data, const IntRect& rect, int stride = 0)
{
    stride = stride ? stride : rect.width();
    for (int y = rect.y(); y < rect.maxY(); ++y) {
        uint32_t* p = data + y * stride;
        for (int x = rect.x(); x < rect.maxX(); ++x)
            p[x] = ((p[x] << 16) & 0xff0000) | ((p[x] >> 16) & 0xff) | (p[x] & 0xff00ff00);
    }
}

static bool driverSupportsSubImage(GraphicsContext3D* context)
{
    if (context->isGLES2Compliant()) {
        static bool supportsSubImage = context->getExtensions()->supports("GL_EXT_unpack_subimage");
        return supportsSubImage;
    }

    return true;
}

void BitmapTextureGL::didReset()
{
    if (!m_id)
        m_id = m_context3D->createTexture();

    m_shouldClear = true;
    if (m_textureSize == contentSize())
        return;

    m_textureSize = contentSize();
    m_context3D->bindTexture(GraphicsContext3D::TEXTURE_2D, m_id);
    m_context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);

    m_context3D->texImage2DDirect(GraphicsContext3D::TEXTURE_2D, 0, m_internalFormat, m_textureSize.width(), m_textureSize.height(), 0, m_format, m_type, 0);
}

void BitmapTextureGL::updateContentsNoSwizzle(const void* srcData, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine, unsigned bytesPerPixel, Platform3DObject glFormat)
{
    m_context3D->bindTexture(GraphicsContext3D::TEXTURE_2D, m_id);
    // For ES drivers that don't support sub-images.
    if (driverSupportsSubImage(m_context3D.get())) {
        // Use the OpenGL sub-image extension, now that we know it's available.
        m_context3D->pixelStorei(GraphicsContext3D::UNPACK_ROW_LENGTH, bytesPerLine / bytesPerPixel);
        m_context3D->pixelStorei(GraphicsContext3D::UNPACK_SKIP_ROWS, sourceOffset.y());
        m_context3D->pixelStorei(GraphicsContext3D::UNPACK_SKIP_PIXELS, sourceOffset.x());
    }

    m_context3D->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height(), glFormat, m_type, srcData);

    // For ES drivers that don't support sub-images.
    if (driverSupportsSubImage(m_context3D.get())) {
        m_context3D->pixelStorei(GraphicsContext3D::UNPACK_ROW_LENGTH, 0);
        m_context3D->pixelStorei(GraphicsContext3D::UNPACK_SKIP_ROWS, 0);
        m_context3D->pixelStorei(GraphicsContext3D::UNPACK_SKIP_PIXELS, 0);
    }
}

void BitmapTextureGL::updateContents(const void* srcData, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine, UpdateContentsFlag updateContentsFlag)
{
    m_context3D->bindTexture(GraphicsContext3D::TEXTURE_2D, m_id);

    const unsigned bytesPerPixel = 4;
    char* data = reinterpret_cast<char*>(const_cast<void*>(srcData));
    Vector<char> temporaryData;
    IntPoint adjustedSourceOffset = sourceOffset;

    // Texture upload requires subimage buffer if driver doesn't support subimage and we don't have full image upload.
    bool requireSubImageBuffer = !driverSupportsSubImage(m_context3D.get())
        && !(bytesPerLine == static_cast<int>(targetRect.width() * bytesPerPixel) && adjustedSourceOffset == IntPoint::zero());

    // prepare temporaryData if necessary
    if ((m_format == GraphicsContext3D::RGBA && updateContentsFlag == UpdateCannotModifyOriginalImageData) || requireSubImageBuffer) {
        temporaryData.resize(targetRect.width() * targetRect.height() * bytesPerPixel);
        data = temporaryData.data();
        const char* bits = static_cast<const char*>(srcData);
        const char* src = bits + sourceOffset.y() * bytesPerLine + sourceOffset.x() * bytesPerPixel;
        char* dst = data;
        const int targetBytesPerLine = targetRect.width() * bytesPerPixel;
        for (int y = 0; y < targetRect.height(); ++y) {
            memcpy(dst, src, targetBytesPerLine);
            src += bytesPerLine;
            dst += targetBytesPerLine;
        }

        bytesPerLine = targetBytesPerLine;
        adjustedSourceOffset = IntPoint(0, 0);
    }

    if (m_format == GraphicsContext3D::RGBA)
        swizzleBGRAToRGBA(reinterpret_cast_ptr<uint32_t*>(data), IntRect(adjustedSourceOffset, targetRect.size()), bytesPerLine / bytesPerPixel);

    updateContentsNoSwizzle(data, targetRect, adjustedSourceOffset, bytesPerLine, bytesPerPixel, m_format);
}

void BitmapTextureGL::updateContents(Image* image, const IntRect& targetRect, const IntPoint& offset, UpdateContentsFlag updateContentsFlag)
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
#endif

    updateContents(imageData, targetRect, offset, bytesPerLine, updateContentsFlag);
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

PassRefPtr<BitmapTexture> BitmapTextureGL::applyFilters(TextureMapper& textureMapper, const FilterOperations& filters)
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
                    intermediateSurface = texmapGL.acquireTextureFromPool(contentSize());
                texmapGL.bindSurface(intermediateSurface.get());
            }

            if (last) {
                toBitmapTextureGL(resultSurface.get())->m_filterInfo = BitmapTextureGL::FilterInfo(filter, j, spareSurface);
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

    m_rbo = m_context3D->createRenderbuffer();
    m_context3D->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_rbo);
    m_context3D->renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::STENCIL_INDEX8, m_textureSize.width(), m_textureSize.height());
    m_context3D->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
    m_context3D->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_rbo);
    m_context3D->clearStencil(0);
    m_context3D->clear(GraphicsContext3D::STENCIL_BUFFER_BIT);
}

void BitmapTextureGL::initializeDepthBuffer()
{
    if (m_depthBufferObject)
        return;

    m_depthBufferObject = m_context3D->createRenderbuffer();
    m_context3D->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_depthBufferObject);
    m_context3D->renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT16, m_textureSize.width(), m_textureSize.height());
    m_context3D->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
    m_context3D->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthBufferObject);
}

void BitmapTextureGL::clearIfNeeded()
{
    if (!m_shouldClear)
        return;

    m_clipStack.reset(IntRect(IntPoint::zero(), m_textureSize), TextureMapperGL::ClipStack::DefaultYAxis);
    m_clipStack.applyIfNeeded(m_context3D.get());
    m_context3D->clearColor(0, 0, 0, 0);
    m_context3D->clear(GraphicsContext3D::COLOR_BUFFER_BIT);
    m_shouldClear = false;
}

void BitmapTextureGL::createFboIfNeeded()
{
    if (m_fbo)
        return;

    m_fbo = m_context3D->createFramebuffer();
    m_context3D->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    m_context3D->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, id(), 0);
    m_shouldClear = true;
}

void BitmapTextureGL::bindAsSurface(GraphicsContext3D* context3D)
{
    context3D->bindTexture(GraphicsContext3D::TEXTURE_2D, 0);
    createFboIfNeeded();
    context3D->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    context3D->viewport(0, 0, m_textureSize.width(), m_textureSize.height());
    clearIfNeeded();
    m_clipStack.apply(m_context3D.get());
}

BitmapTextureGL::~BitmapTextureGL()
{
    if (m_id)
        m_context3D->deleteTexture(m_id);

    if (m_fbo)
        m_context3D->deleteFramebuffer(m_fbo);

    if (m_rbo)
        m_context3D->deleteRenderbuffer(m_rbo);

    if (m_depthBufferObject)
        m_context3D->deleteRenderbuffer(m_depthBufferObject);
}

bool BitmapTextureGL::isValid() const
{
    return m_id;
}

IntSize BitmapTextureGL::size() const
{
    return m_textureSize;
}

}; // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)
