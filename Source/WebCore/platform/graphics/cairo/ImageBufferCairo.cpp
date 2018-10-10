/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "ImageBuffer.h"

#if USE(CAIRO)

#include "BitmapImage.h"
#include "CairoUtilities.h"
#include "Color.h"
#include "GraphicsContext.h"
#include "GraphicsContextImplCairo.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "Pattern.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <cairo.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>
#include <wtf/text/WTFString.h>

#if ENABLE(ACCELERATED_2D_CANVAS)
#include "GLContext.h"
#include "TextureMapperGL.h"

#if USE(EGL)
#if USE(LIBEPOXY)
#include "EpoxyEGL.h"
#else
#include <EGL/egl.h>
#endif
#endif
#include <cairo-gl.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif USE(OPENGL_ES)
#include <GLES2/gl2.h>
#else
#include "OpenGLShims.h"
#endif

#if USE(COORDINATED_GRAPHICS_THREADED)
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"
#endif
#endif


namespace WebCore {
using namespace std;

ImageBufferData::ImageBufferData(const IntSize& size, RenderingMode renderingMode)
    : m_platformContext(0)
    , m_size(size)
    , m_renderingMode(renderingMode)
#if ENABLE(ACCELERATED_2D_CANVAS)
#if USE(COORDINATED_GRAPHICS_THREADED)
    , m_compositorTexture(0)
#endif
    , m_texture(0)
#endif
{
#if ENABLE(ACCELERATED_2D_CANVAS) && USE(COORDINATED_GRAPHICS_THREADED)
    if (m_renderingMode == RenderingMode::Accelerated) {
#if USE(NICOSIA)
        m_nicosiaLayer = Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this));
#else
        m_platformLayerProxy = adoptRef(new TextureMapperPlatformLayerProxy);
#endif
    }
#endif
}

ImageBufferData::~ImageBufferData()
{
    if (m_renderingMode != Accelerated)
        return;

#if ENABLE(ACCELERATED_2D_CANVAS)
#if USE(COORDINATED_GRAPHICS_THREADED) && USE(NICOSIA)
    downcast<Nicosia::ContentLayerTextureMapperImpl>(m_nicosiaLayer->impl()).invalidateClient();
#endif

    GLContext* previousActiveContext = GLContext::current();
    PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->makeContextCurrent();

    if (m_texture)
        glDeleteTextures(1, &m_texture);

#if USE(COORDINATED_GRAPHICS_THREADED)
    if (m_compositorTexture)
        glDeleteTextures(1, &m_compositorTexture);
#endif

    if (previousActiveContext)
        previousActiveContext->makeContextCurrent();
#endif
}

#if ENABLE(ACCELERATED_2D_CANVAS)
#if USE(COORDINATED_GRAPHICS_THREADED)
void ImageBufferData::createCompositorBuffer()
{
    auto* context = PlatformDisplay::sharedDisplayForCompositing().sharingGLContext();
    context->makeContextCurrent();

    glGenTextures(1, &m_compositorTexture);
    glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0 , GL_RGBA, m_size.width(), m_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    m_compositorSurface = adoptRef(cairo_gl_surface_create_for_texture(context->cairoDevice(), CAIRO_CONTENT_COLOR_ALPHA, m_compositorTexture, m_size.width(), m_size.height()));
    m_compositorCr = adoptRef(cairo_create(m_compositorSurface.get()));
    cairo_set_antialias(m_compositorCr.get(), CAIRO_ANTIALIAS_NONE);
}

#if !USE(NICOSIA)
RefPtr<TextureMapperPlatformLayerProxy> ImageBufferData::proxy() const
{
    return m_platformLayerProxy.copyRef();
}
#endif

void ImageBufferData::swapBuffersIfNeeded()
{
    GLContext* previousActiveContext = GLContext::current();

    if (!m_compositorTexture) {
        createCompositorBuffer();

        auto proxyOperation =
            [this](TextureMapperPlatformLayerProxy& proxy)
            {
                LockHolder holder(proxy.lock());
                proxy.pushNextBuffer(std::make_unique<TextureMapperPlatformLayerBuffer>(m_compositorTexture, m_size, TextureMapperGL::ShouldBlend, GL_RGBA));
            };
#if USE(NICOSIA)
        proxyOperation(downcast<Nicosia::ContentLayerTextureMapperImpl>(m_nicosiaLayer->impl()).proxy());
#else
        proxyOperation(*m_platformLayerProxy);
#endif
    }

    // It would be great if we could just swap the buffers here as we do with webgl, but that breaks the cases
    // where one frame uses the content already rendered in the previous frame. So we just copy the content
    // into the compositor buffer.
    cairo_set_source_surface(m_compositorCr.get(), m_surface.get(), 0, 0);
    cairo_set_operator(m_compositorCr.get(), CAIRO_OPERATOR_SOURCE);
    cairo_paint(m_compositorCr.get());

    if (previousActiveContext)
        previousActiveContext->makeContextCurrent();
}
#endif

void clearSurface(cairo_surface_t* surface)
{
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
        return;

    RefPtr<cairo_t> cr = adoptRef(cairo_create(surface));
    cairo_set_operator(cr.get(), CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr.get());
}

void ImageBufferData::createCairoGLSurface()
{
    auto* context = PlatformDisplay::sharedDisplayForCompositing().sharingGLContext();
    context->makeContextCurrent();

    // We must generate the texture ourselves, because there is no Cairo API for extracting it
    // from a pre-existing surface.
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0 /* level */, GL_RGBA, m_size.width(), m_size.height(), 0 /* border */, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    cairo_device_t* device = context->cairoDevice();

    // Thread-awareness is a huge performance hit on non-Intel drivers.
    cairo_gl_device_set_thread_aware(device, FALSE);

    m_surface = adoptRef(cairo_gl_surface_create_for_texture(device, CAIRO_CONTENT_COLOR_ALPHA, m_texture, m_size.width(), m_size.height()));
    clearSurface(m_surface.get());
}
#endif

static RefPtr<cairo_surface_t>
cairoSurfaceCoerceToImage(cairo_surface_t* surface)
{
    if (cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE
        && cairo_surface_get_content(surface) == CAIRO_CONTENT_COLOR_ALPHA)
        return surface;

    auto copy = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
        cairo_image_surface_get_width(surface),
        cairo_image_surface_get_height(surface)));

    auto cr = adoptRef(cairo_create(copy.get()));
    cairo_set_operator(cr.get(), CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr.get(), surface, 0, 0);
    cairo_paint(cr.get());

    return copy;
}

Vector<uint8_t> ImageBuffer::toBGRAData() const
{
    auto surface = cairoSurfaceCoerceToImage(m_data.m_surface.get());
    cairo_surface_flush(surface.get());

    Vector<uint8_t> imageData;
    if (cairo_surface_status(surface.get()))
        return imageData;

    auto pixels = cairo_image_surface_get_data(surface.get());
    imageData.append(pixels, cairo_image_surface_get_stride(surface.get()) *
        cairo_image_surface_get_height(surface.get()));

    return imageData;
}

ImageBuffer::ImageBuffer(const FloatSize& size, float resolutionScale, ColorSpace, RenderingMode renderingMode, const HostWindow*, bool& success)
    : m_data(IntSize(size), renderingMode)
    , m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    success = false;  // Make early return mean error.

    float scaledWidth = ceilf(m_resolutionScale * size.width());
    float scaledHeight = ceilf(m_resolutionScale * size.height());

    // FIXME: Should we automatically use a lower resolution?
    if (!FloatSize(scaledWidth, scaledHeight).isExpressibleAsIntSize())
        return;

    m_size = IntSize(scaledWidth, scaledHeight);
    m_data.m_size = m_size;

    if (m_size.isEmpty())
        return;

#if ENABLE(ACCELERATED_2D_CANVAS)
    if (m_data.m_renderingMode == Accelerated) {
        m_data.createCairoGLSurface();
        if (!m_data.m_surface || cairo_surface_status(m_data.m_surface.get()) != CAIRO_STATUS_SUCCESS)
            m_data.m_renderingMode = Unaccelerated; // If allocation fails, fall back to non-accelerated path.
    }
    if (m_data.m_renderingMode == Unaccelerated)
#else
    ASSERT(m_data.m_renderingMode != Accelerated);
#endif
    {
        static cairo_user_data_key_t s_surfaceDataKey;

        int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, m_size.width());
        void* surfaceData;
        if (!tryFastZeroedMalloc(m_size.height() * stride).getValue(surfaceData))
            return;

        m_data.m_surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(surfaceData), CAIRO_FORMAT_ARGB32, m_size.width(), m_size.height(), stride));
        cairo_surface_set_user_data(m_data.m_surface.get(), &s_surfaceDataKey, surfaceData, [](void* data) { fastFree(data); });
    }

    if (cairo_surface_status(m_data.m_surface.get()) != CAIRO_STATUS_SUCCESS)
        return;  // create will notice we didn't set m_initialized and fail.

    cairoSurfaceSetDeviceScale(m_data.m_surface.get(), m_resolutionScale, m_resolutionScale);

    RefPtr<cairo_t> cr = adoptRef(cairo_create(m_data.m_surface.get()));
    m_data.m_platformContext.setCr(cr.get());
    m_data.m_context = std::make_unique<GraphicsContext>(GraphicsContextImplCairo::createFactory(m_data.m_platformContext));
    success = true;
}

ImageBuffer::~ImageBuffer() = default;

std::unique_ptr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const FloatSize& size, const GraphicsContext& context)
{
    return createCompatibleBuffer(size, ColorSpaceSRGB, context);
}

GraphicsContext& ImageBuffer::context() const
{
    return *m_data.m_context;
}

RefPtr<Image> ImageBuffer::sinkIntoImage(std::unique_ptr<ImageBuffer> imageBuffer, PreserveResolution preserveResolution)
{
    return imageBuffer->copyImage(DontCopyBackingStore, preserveResolution);
}

RefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, PreserveResolution) const
{
    // copyCairoImageSurface inherits surface's device scale factor.
    if (copyBehavior == CopyBackingStore)
        return BitmapImage::create(copyCairoImageSurface(m_data.m_surface.get()));

    // BitmapImage will release the passed in surface on destruction
    return BitmapImage::create(RefPtr<cairo_surface_t>(m_data.m_surface));
}

BackingStoreCopy ImageBuffer::fastCopyImageMode()
{
    return DontCopyBackingStore;
}

void ImageBuffer::drawConsuming(std::unique_ptr<ImageBuffer> imageBuffer, GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode)
{
    imageBuffer->draw(destContext, destRect, srcRect, op, blendMode);
}

void ImageBuffer::draw(GraphicsContext& destinationContext, const FloatRect& destRect, const FloatRect& srcRect,
    CompositeOperator op, BlendMode blendMode)
{
    BackingStoreCopy copyMode = &destinationContext == &context() ? CopyBackingStore : DontCopyBackingStore;
    RefPtr<Image> image = copyImage(copyMode);
    destinationContext.drawImage(*image, destRect, srcRect, ImagePaintingOptions(op, blendMode, DecodingMode::Synchronous, ImageOrientationDescription()));
}

void ImageBuffer::drawPattern(GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform,
    const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, BlendMode)
{
    if (RefPtr<Image> image = copyImage(DontCopyBackingStore))
        image->drawPattern(context, destRect, srcRect, patternTransform, phase, spacing, op);
}

void ImageBuffer::platformTransformColorSpace(const std::array<uint8_t, 256>& lookUpTable)
{
    // FIXME: Enable color space conversions on accelerated canvases.
    if (cairo_surface_get_type(m_data.m_surface.get()) != CAIRO_SURFACE_TYPE_IMAGE)
        return;

    unsigned char* dataSrc = cairo_image_surface_get_data(m_data.m_surface.get());
    int stride = cairo_image_surface_get_stride(m_data.m_surface.get());
    for (int y = 0; y < m_size.height(); ++y) {
        unsigned* row = reinterpret_cast_ptr<unsigned*>(dataSrc + stride * y);
        for (int x = 0; x < m_size.width(); x++) {
            unsigned* pixel = row + x;
            Color pixelColor = colorFromPremultipliedARGB(*pixel);
            pixelColor = Color(lookUpTable[pixelColor.red()],
                               lookUpTable[pixelColor.green()],
                               lookUpTable[pixelColor.blue()],
                               pixelColor.alpha());
            *pixel = premultipliedARGBFromColor(pixelColor);
        }
    }
    cairo_surface_mark_dirty_rectangle(m_data.m_surface.get(), 0, 0, m_logicalSize.width(), m_logicalSize.height());
}

RefPtr<cairo_surface_t> copySurfaceToImageAndAdjustRect(cairo_surface_t* surface, IntRect& rect)
{
    cairo_surface_type_t surfaceType = cairo_surface_get_type(surface);

    // If we already have an image, we write directly to the underlying data;
    // otherwise we create a temporary surface image
    if (surfaceType == CAIRO_SURFACE_TYPE_IMAGE)
        return surface;
    
    rect.setX(0);
    rect.setY(0);
    return adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, rect.width(), rect.height()));
}

template <AlphaPremultiplication premultiplied>
RefPtr<Uint8ClampedArray> getImageData(const IntRect& rect, const IntRect& logicalRect, const ImageBufferData& data, const IntSize& size, const IntSize& logicalSize, float resolutionScale)
{
    // The area can overflow if the rect is too big.
    Checked<unsigned, RecordOverflow> area = 4;
    area *= rect.width();
    area *= rect.height();
    if (area.hasOverflowed())
        return nullptr;

    auto result = Uint8ClampedArray::tryCreateUninitialized(area.unsafeGet());
    if (!result)
        return nullptr;

    // Can overflow, as we are adding 2 ints.
    int endx = 0;
    if (!WTF::safeAdd(rect.x(), rect.width(), endx))
        return nullptr;

    // Can overflow, as we are adding 2 ints.
    int endy = 0;
    if (!WTF::safeAdd(rect.y(), rect.height(), endy))
        return nullptr;

    if (rect.x() < 0 || rect.y() < 0 || endx > size.width() || endy > size.height())
        result->zeroFill();

    int originx = rect.x();
    int destx = 0;
    if (originx < 0) {
        destx = -originx;
        originx = 0;
    }

    if (endx > size.width())
        endx = size.width();
    int numColumns = endx - originx;

    int originy = rect.y();
    int desty = 0;
    if (originy < 0) {
        desty = -originy;
        originy = 0;
    }

    if (endy > size.height())
        endy = size.height();
    int numRows = endy - originy;

    // Nothing will be copied, so just return the result.
    if (numColumns <= 0 || numRows <= 0)
        return result;

    // The size of the derived surface is in BackingStoreCoordinateSystem.
    // We need to set the device scale for the derived surface from this ImageBuffer.
    IntRect imageRect(originx, originy, numColumns, numRows);
    RefPtr<cairo_surface_t> imageSurface = copySurfaceToImageAndAdjustRect(data.m_surface.get(), imageRect);
    cairoSurfaceSetDeviceScale(imageSurface.get(), resolutionScale, resolutionScale);
    originx = imageRect.x();
    originy = imageRect.y();
    if (imageSurface != data.m_surface.get()) {
        // This cairo surface operation is done in LogicalCoordinateSystem.
        IntRect logicalArea = intersection(logicalRect, IntRect(0, 0, logicalSize.width(), logicalSize.height()));
        copyRectFromOneSurfaceToAnother(data.m_surface.get(), imageSurface.get(), IntSize(-logicalArea.x(), -logicalArea.y()), IntRect(IntPoint(), logicalArea.size()), IntSize(), CAIRO_OPERATOR_SOURCE);
    }

    unsigned char* dataSrc = cairo_image_surface_get_data(imageSurface.get());
    unsigned char* dataDst = result->data();
    int stride = cairo_image_surface_get_stride(imageSurface.get());
    unsigned destBytesPerRow = 4 * rect.width();

    unsigned char* destRows = dataDst + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        unsigned* row = reinterpret_cast_ptr<unsigned*>(dataSrc + stride * (y + originy));
        for (int x = 0; x < numColumns; x++) {
            int basex = x * 4;
            unsigned* pixel = row + x + originx;

            // Avoid calling Color::colorFromPremultipliedARGB() because one
            // function call per pixel is too expensive.
            unsigned alpha = (*pixel & 0xFF000000) >> 24;
            unsigned red = (*pixel & 0x00FF0000) >> 16;
            unsigned green = (*pixel & 0x0000FF00) >> 8;
            unsigned blue = (*pixel & 0x000000FF);

            if (premultiplied == AlphaPremultiplication::Unpremultiplied) {
                if (alpha && alpha != 255) {
                    red = red * 255 / alpha;
                    green = green * 255 / alpha;
                    blue = blue * 255 / alpha;
                }
            }

            destRows[basex]     = red;
            destRows[basex + 1] = green;
            destRows[basex + 2] = blue;
            destRows[basex + 3] = alpha;
        }
        destRows += destBytesPerRow;
    }

    return result;
}

template<typename Unit>
inline Unit logicalUnit(const Unit& value, ImageBuffer::CoordinateSystem coordinateSystemOfValue, float resolutionScale)
{
    if (coordinateSystemOfValue == ImageBuffer::LogicalCoordinateSystem || resolutionScale == 1.0)
        return value;
    Unit result(value);
    result.scale(1.0 / resolutionScale);
    return result;
}

template<typename Unit>
inline Unit backingStoreUnit(const Unit& value, ImageBuffer::CoordinateSystem coordinateSystemOfValue, float resolutionScale)
{
    if (coordinateSystemOfValue == ImageBuffer::BackingStoreCoordinateSystem || resolutionScale == 1.0)
        return value;
    Unit result(value);
    result.scale(resolutionScale);
    return result;
}

RefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, IntSize* pixelArrayDimensions, CoordinateSystem coordinateSystem) const
{
    IntRect logicalRect = logicalUnit(rect, coordinateSystem, m_resolutionScale);
    IntRect backingStoreRect = backingStoreUnit(rect, coordinateSystem, m_resolutionScale);
    if (pixelArrayDimensions)
        *pixelArrayDimensions = backingStoreRect.size();
    return getImageData<AlphaPremultiplication::Unpremultiplied>(backingStoreRect, logicalRect, m_data, m_size, m_logicalSize, m_resolutionScale);
}

RefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, IntSize* pixelArrayDimensions, CoordinateSystem coordinateSystem) const
{
    IntRect logicalRect = logicalUnit(rect, coordinateSystem, m_resolutionScale);
    IntRect backingStoreRect = backingStoreUnit(rect, coordinateSystem, m_resolutionScale);
    if (pixelArrayDimensions)
        *pixelArrayDimensions = backingStoreRect.size();
    return getImageData<AlphaPremultiplication::Premultiplied>(backingStoreRect, logicalRect, m_data, m_size, m_logicalSize, m_resolutionScale);
}

void ImageBuffer::putByteArray(const Uint8ClampedArray& source, AlphaPremultiplication sourceFormat, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem coordinateSystem)
{
    IntRect scaledSourceRect = backingStoreUnit(sourceRect, coordinateSystem, m_resolutionScale);
    IntSize scaledSourceSize = backingStoreUnit(sourceSize, coordinateSystem, m_resolutionScale);
    IntPoint scaledDestPoint = backingStoreUnit(destPoint, coordinateSystem, m_resolutionScale);
    IntRect logicalSourceRect = logicalUnit(sourceRect, coordinateSystem, m_resolutionScale);
    IntPoint logicalDestPoint = logicalUnit(destPoint, coordinateSystem, m_resolutionScale);

    ASSERT(scaledSourceRect.width() > 0);
    ASSERT(scaledSourceRect.height() > 0);

    int originx = scaledSourceRect.x();
    int destx = scaledDestPoint.x() + scaledSourceRect.x();
    int logicalDestx = logicalDestPoint.x() + logicalSourceRect.x();
    ASSERT(destx >= 0);
    ASSERT(destx < m_size.width());
    ASSERT(originx >= 0);
    ASSERT(originx <= scaledSourceRect.maxX());

    int endx = scaledDestPoint.x() + scaledSourceRect.maxX();
    int logicalEndx = logicalDestPoint.x() + logicalSourceRect.maxX();
    ASSERT(endx <= m_size.width());

    int numColumns = endx - destx;
    int logicalNumColumns = logicalEndx - logicalDestx;

    int originy = scaledSourceRect.y();
    int desty = scaledDestPoint.y() + scaledSourceRect.y();
    int logicalDesty = logicalDestPoint.y() + logicalSourceRect.y();
    ASSERT(desty >= 0);
    ASSERT(desty < m_size.height());
    ASSERT(originy >= 0);
    ASSERT(originy <= scaledSourceRect.maxY());

    int endy = scaledDestPoint.y() + scaledSourceRect.maxY();
    int logicalEndy = logicalDestPoint.y() + logicalSourceRect.maxY();
    ASSERT(endy <= m_size.height());
    int numRows = endy - desty;
    int logicalNumRows = logicalEndy - logicalDesty;

    // The size of the derived surface is in BackingStoreCoordinateSystem.
    // We need to set the device scale for the derived surface from this ImageBuffer.
    IntRect imageRect(destx, desty, numColumns, numRows);
    RefPtr<cairo_surface_t> imageSurface = copySurfaceToImageAndAdjustRect(m_data.m_surface.get(), imageRect);
    cairoSurfaceSetDeviceScale(imageSurface.get(), m_resolutionScale, m_resolutionScale);
    destx = imageRect.x();
    desty = imageRect.y();

    uint8_t* pixelData = cairo_image_surface_get_data(imageSurface.get());

    unsigned srcBytesPerRow = 4 * scaledSourceSize.width();
    int stride = cairo_image_surface_get_stride(imageSurface.get());

    const uint8_t* srcRows = source.data() + originy * srcBytesPerRow + originx * 4;
    for (int y = 0; y < numRows; ++y) {
        unsigned* row = reinterpret_cast_ptr<unsigned*>(pixelData + stride * (y + desty));
        for (int x = 0; x < numColumns; x++) {
            int basex = x * 4;
            unsigned* pixel = row + x + destx;

            // Avoid calling Color::premultipliedARGBFromColor() because one
            // function call per pixel is too expensive.
            unsigned red = srcRows[basex];
            unsigned green = srcRows[basex + 1];
            unsigned blue = srcRows[basex + 2];
            unsigned alpha = srcRows[basex + 3];

            if (sourceFormat == AlphaPremultiplication::Unpremultiplied) {
                if (alpha != 255) {
                    red = (red * alpha + 254) / 255;
                    green = (green * alpha + 254) / 255;
                    blue = (blue * alpha + 254) / 255;
                }
            }

            *pixel = (alpha << 24) | red  << 16 | green  << 8 | blue;
        }
        srcRows += srcBytesPerRow;
    }

    // This cairo surface operation is done in LogicalCoordinateSystem.
    cairo_surface_mark_dirty_rectangle(imageSurface.get(), logicalDestx, logicalDesty, logicalNumColumns, logicalNumRows);

    if (imageSurface != m_data.m_surface.get()) {
        // This cairo surface operation is done in LogicalCoordinateSystem.
        copyRectFromOneSurfaceToAnother(imageSurface.get(), m_data.m_surface.get(), IntSize(), IntRect(0, 0, logicalNumColumns, logicalNumRows), IntSize(logicalDestPoint.x() + logicalSourceRect.x(), logicalDestPoint.y() + logicalSourceRect.y()), CAIRO_OPERATOR_SOURCE);
    }
}

#if !PLATFORM(GTK)
static cairo_status_t writeFunction(void* output, const unsigned char* data, unsigned int length)
{
    if (!reinterpret_cast<Vector<uint8_t>*>(output)->tryAppend(data, length))
        return CAIRO_STATUS_WRITE_ERROR;
    return CAIRO_STATUS_SUCCESS;
}

static bool encodeImage(cairo_surface_t* image, const String& mimeType, Vector<uint8_t>* output)
{
    ASSERT_UNUSED(mimeType, mimeType == "image/png"); // Only PNG output is supported for now.

    return cairo_surface_write_to_png_stream(image, writeFunction, output) == CAIRO_STATUS_SUCCESS;
}

String ImageBuffer::toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution) const
{
    Vector<uint8_t> encodedImage = toData(mimeType, quality);
    if (encodedImage.isEmpty())
        return "data:,";

    Vector<char> base64Data;
    base64Encode(encodedImage.data(), encodedImage.size(), base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

Vector<uint8_t> ImageBuffer::toData(const String& mimeType, std::optional<double>) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    cairo_surface_t* image = cairo_get_target(context().platformContext()->cr());

    Vector<uint8_t> encodedImage;
    if (!image || !encodeImage(image, mimeType, &encodedImage))
        return { };

    return encodedImage;
}

#endif

#if ENABLE(ACCELERATED_2D_CANVAS) && !USE(COORDINATED_GRAPHICS_THREADED)
void ImageBufferData::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity)
{
    ASSERT(m_texture);

    // Cairo may change the active context, so we make sure to change it back after flushing.
    GLContext* previousActiveContext = GLContext::current();
    cairo_surface_flush(m_surface.get());
    previousActiveContext->makeContextCurrent();

    static_cast<TextureMapperGL&>(textureMapper).drawTexture(m_texture, TextureMapperGL::ShouldBlend, m_size, targetRect, matrix, opacity);
}
#endif

PlatformLayer* ImageBuffer::platformLayer() const
{
#if ENABLE(ACCELERATED_2D_CANVAS)
#if USE(NICOSIA)
    if (m_data.m_renderingMode == RenderingMode::Accelerated)
        return m_data.m_nicosiaLayer.get();
#else
    if (m_data.m_texture)
        return const_cast<ImageBufferData*>(&m_data);
#endif
#endif
    return 0;
}

bool ImageBuffer::copyToPlatformTexture(GraphicsContext3D&, GC3Denum target, Platform3DObject destinationTexture, GC3Denum internalformat, bool premultiplyAlpha, bool flipY)
{
#if ENABLE(ACCELERATED_2D_CANVAS)
    ASSERT_WITH_MESSAGE(m_resolutionScale == 1.0, "Since the HiDPI Canvas feature is removed, the resolution factor here is always 1.");
    if (premultiplyAlpha || flipY)
        return false;

    if (!m_data.m_texture)
        return false;

    GC3Denum bindTextureTarget;
    switch (target) {
    case GL_TEXTURE_2D:
        bindTextureTarget = GL_TEXTURE_2D;
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        bindTextureTarget = GL_TEXTURE_CUBE_MAP;
        break;
    default:
        return false;
    }

    cairo_surface_flush(m_data.m_surface.get());

    std::unique_ptr<GLContext> context = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing());
    context->makeContextCurrent();
    uint32_t fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_data.m_texture, 0);
    glBindTexture(bindTextureTarget, destinationTexture);
    glCopyTexImage2D(target, 0, internalformat, 0, 0, m_size.width(), m_size.height(), 0);
    glBindTexture(bindTextureTarget, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFlush();
    glDeleteFramebuffers(1, &fbo);
    return true;
#else
    UNUSED_PARAM(target);
    UNUSED_PARAM(destinationTexture);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(premultiplyAlpha);
    UNUSED_PARAM(flipY);
    return false;
#endif
}

} // namespace WebCore

#endif // USE(CAIRO)
