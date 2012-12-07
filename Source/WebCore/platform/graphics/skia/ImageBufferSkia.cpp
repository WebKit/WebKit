/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageBuffer.h"

#include "BitmapImage.h"
#include "BitmapImageSingleFrameSkia.h"
#include "Extensions3D.h"
#include "GrContext.h"
#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include "ImageData.h"
#include "JPEGImageEncoder.h"
#include "MIMETypeRegistry.h"
#include "MemoryInstrumentationSkia.h"
#include "PNGImageEncoder.h"
#include "PlatformContextSkia.h"
#include "SharedGraphicsContext3D.h"
#include "SkColorPriv.h"
#include "SkGpuDevice.h"
#include "SkiaUtils.h"
#include "WEBPImageEncoder.h"

#if USE(ACCELERATED_COMPOSITING)
#include "Canvas2DLayerBridge.h"
#endif

#include <wtf/text/Base64.h>
#include <wtf/text/WTFString.h>

using namespace std;

namespace WebCore {

// We pass a technically-uninitialized canvas to the platform context here since
// the canvas initialization completes in ImageBuffer::ImageBuffer. But
// PlatformContext doesn't actually need to use the object, and this makes all
// the ownership easier to manage.
ImageBufferData::ImageBufferData(const IntSize& size)
    : m_platformContext(0)  // Canvas is set in ImageBuffer constructor.
{
}

static SkCanvas* createAcceleratedCanvas(const IntSize& size, ImageBufferData* data, DeferralMode deferralMode)
{
    RefPtr<GraphicsContext3D> context3D = SharedGraphicsContext3D::get();
    if (!context3D)
        return 0;
    GrContext* gr = context3D->grContext();
    if (!gr)
        return 0;
    gr->resetContext();
    GrTextureDesc desc;
    desc.fFlags = kRenderTarget_GrTextureFlagBit;
    desc.fSampleCnt = 0;
    desc.fWidth = size.width();
    desc.fHeight = size.height();
    desc.fConfig = kSkia8888_GrPixelConfig;
    SkAutoTUnref<GrTexture> texture(gr->createUncachedTexture(desc, 0, 0));
    if (!texture.get())
        return 0;
    SkCanvas* canvas;
    SkAutoTUnref<SkDevice> device(new SkGpuDevice(gr, texture.get()));
#if USE(ACCELERATED_COMPOSITING)
    data->m_layerBridge = Canvas2DLayerBridge::create(context3D.release(), size, deferralMode, texture.get()->getTextureHandle());
    canvas = data->m_layerBridge->skCanvas(device.get());
#else
    canvas = new SkCanvas(device.get());
#endif
    data->m_platformContext.setAccelerated(true);
    return canvas;
}

static SkCanvas* createNonPlatformCanvas(const IntSize& size)
{
    SkAutoTUnref<SkDevice> device(new SkDevice(SkBitmap::kARGB_8888_Config, size.width(), size.height()));
    SkPixelRef* pixelRef = device->accessBitmap(false).pixelRef();
    return pixelRef ? new SkCanvas(device) : 0;
}

PassOwnPtr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const IntSize& size, float resolutionScale, ColorSpace colorSpace, const GraphicsContext* context, bool hasAlpha)
{
    bool success = false;
    OwnPtr<ImageBuffer> buf = adoptPtr(new ImageBuffer(size, resolutionScale, colorSpace, context, hasAlpha, success));
    if (!success)
        return nullptr;
    return buf.release();
}

ImageBuffer::ImageBuffer(const IntSize& size, float resolutionScale, ColorSpace, const GraphicsContext* compatibleContext, bool hasAlpha, bool& success)
    : m_data(size)
    , m_size(size)
    , m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    if (!compatibleContext) {
        success = false;
        return;
    }

    SkAutoTUnref<SkDevice> device(compatibleContext->platformContext()->createCompatibleDevice(size, hasAlpha));
    SkPixelRef* pixelRef = device->accessBitmap(false).pixelRef();
    if (!pixelRef) {
        success = false;
        return;
    }

    m_data.m_canvas = adoptPtr(new SkCanvas(device));
    m_data.m_platformContext.setCanvas(m_data.m_canvas.get());
    m_context = adoptPtr(new GraphicsContext(&m_data.m_platformContext));
    m_context->platformContext()->setDrawingToImageBuffer(true);
    m_context->scale(FloatSize(m_resolutionScale, m_resolutionScale));

    success = true;
}

ImageBuffer::ImageBuffer(const IntSize& size, float resolutionScale, ColorSpace, RenderingMode renderingMode, DeferralMode deferralMode, bool& success)
    : m_data(size)
    , m_size(size)
    , m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    OwnPtr<SkCanvas> canvas;

    if (renderingMode == Accelerated)
        canvas = adoptPtr(createAcceleratedCanvas(size, &m_data, deferralMode));
    else if (renderingMode == UnacceleratedNonPlatformBuffer)
        canvas = adoptPtr(createNonPlatformCanvas(size));

    if (!canvas)
        canvas = adoptPtr(skia::TryCreateBitmapCanvas(size.width(), size.height(), false));

    if (!canvas) {
        success = false;
        return;
    }

    m_data.m_canvas = canvas.release();
    m_data.m_platformContext.setCanvas(m_data.m_canvas.get());
    m_context = adoptPtr(new GraphicsContext(&m_data.m_platformContext));
    m_context->platformContext()->setDrawingToImageBuffer(true);
    m_context->scale(FloatSize(m_resolutionScale, m_resolutionScale));

    // Make the background transparent. It would be nice if this wasn't
    // required, but the canvas is currently filled with the magic transparency
    // color. Can we have another way to manage this?
    m_data.m_canvas->drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);

    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_data.m_layerBridge) {
        // We're using context acquisition as a signal that someone is about to render into our buffer and we need
        // to be ready. This isn't logically const-correct, hence the cast.
        const_cast<Canvas2DLayerBridge*>(m_data.m_layerBridge.get())->contextAcquired();
    }
#endif
    return m_context.get();
}

PassRefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, ScaleBehavior) const
{
    // FIXME: Start honoring ScaleBehavior to scale 2x buffers down to 1x.
    return BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), copyBehavior == CopyBackingStore, m_resolutionScale);
}

PlatformLayer* ImageBuffer::platformLayer() const
{
    return m_data.m_layerBridge ? m_data.m_layerBridge->layer() : 0;
}

bool ImageBuffer::copyToPlatformTexture(GraphicsContext3D& context, Platform3DObject texture, GC3Denum internalFormat, bool premultiplyAlpha, bool flipY)
{
    if (!m_data.m_layerBridge || !platformLayer())
        return false;

    Platform3DObject sourceTexture = m_data.m_layerBridge->backBufferTexture();

    if (!context.makeContextCurrent())
        return false;

    Extensions3D* extensions = context.getExtensions();
    if (!extensions->supports("GL_CHROMIUM_copy_texture") || !extensions->supports("GL_CHROMIUM_flipy"))
        return false;

    // The canvas is stored in a premultiplied format, so unpremultiply if necessary.
    context.pixelStorei(Extensions3D::UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM, !premultiplyAlpha);

    // The canvas is stored in an inverted position, so the flip semantics are reversed.
    context.pixelStorei(Extensions3D::UNPACK_FLIP_Y_CHROMIUM, !flipY);

    extensions->copyTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, sourceTexture, texture, 0, internalFormat);

    context.pixelStorei(Extensions3D::UNPACK_FLIP_Y_CHROMIUM, false);
    context.pixelStorei(Extensions3D::UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM, false);
    context.flush();
    return true;
}

void ImageBuffer::clip(GraphicsContext* context, const FloatRect& rect) const
{
    context->platformContext()->beginLayerClippedToImage(rect, this);
}

static bool drawNeedsCopy(GraphicsContext* src, GraphicsContext* dst)
{
    return dst->platformContext()->isDeferred() || src == dst;
}

void ImageBuffer::draw(GraphicsContext* context, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect,
    CompositeOperator op, BlendMode, bool useLowQualityScale)
{
    RefPtr<Image> image = BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), drawNeedsCopy(m_context.get(), context));
    context->drawImage(image.get(), styleColorSpace, destRect, srcRect, op, DoNotRespectImageOrientation, useLowQualityScale);
}

void ImageBuffer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const AffineTransform& patternTransform,
                              const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    RefPtr<Image> image = BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), drawNeedsCopy(m_context.get(), context));
    image->drawPattern(context, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookUpTable)
{
    // FIXME: Disable color space conversions on accelerated canvases (for now).
    if (m_data.m_platformContext.isAccelerated()) 
        return;

    const SkBitmap& bitmap = *context()->platformContext()->bitmap();
    if (bitmap.isNull())
        return;

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);
    for (int y = 0; y < m_size.height(); ++y) {
        uint32_t* srcRow = bitmap.getAddr32(0, y);
        for (int x = 0; x < m_size.width(); ++x) {
            SkColor color = SkPMColorToColor(srcRow[x]);
            srcRow[x] = SkPreMultiplyARGB(SkColorGetA(color),
                                          lookUpTable[SkColorGetR(color)],
                                          lookUpTable[SkColorGetG(color)],
                                          lookUpTable[SkColorGetB(color)]);
        }
    }
}

template <Multiply multiplied>
PassRefPtr<Uint8ClampedArray> getImageData(const IntRect& rect, PlatformContextSkia* context,
                                   const IntSize& size)
{
    float area = 4.0f * rect.width() * rect.height();
    if (area > static_cast<float>(std::numeric_limits<int>::max()))
        return 0;

    RefPtr<Uint8ClampedArray> result = Uint8ClampedArray::createUninitialized(rect.width() * rect.height() * 4);

    unsigned char* data = result->data();

    if (rect.x() < 0
        || rect.y() < 0
        || rect.maxX() > size.width()
        || rect.maxY() > size.height())
        result->zeroFill();

    unsigned destBytesPerRow = 4 * rect.width();
    SkBitmap destBitmap;
    destBitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height(), destBytesPerRow);
    destBitmap.setPixels(data);

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;

    context->readPixels(&destBitmap, rect.x(), rect.y(), config8888);
    return result.release();
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, CoordinateSystem) const
{
    return getImageData<Unmultiplied>(rect, context()->platformContext(), m_size);
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, CoordinateSystem) const
{
    return getImageData<Premultiplied>(rect, context()->platformContext(), m_size);
}

void ImageBuffer::putByteArray(Multiply multiplied, Uint8ClampedArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originX = sourceRect.x();
    int destX = destPoint.x() + sourceRect.x();
    ASSERT(destX >= 0);
    ASSERT(destX < m_size.width());
    ASSERT(originX >= 0);
    ASSERT(originX < sourceRect.maxX());

    int endX = destPoint.x() + sourceRect.maxX();
    ASSERT(endX <= m_size.width());

    int numColumns = endX - destX;

    int originY = sourceRect.y();
    int destY = destPoint.y() + sourceRect.y();
    ASSERT(destY >= 0);
    ASSERT(destY < m_size.height());
    ASSERT(originY >= 0);
    ASSERT(originY < sourceRect.maxY());

    int endY = destPoint.y() + sourceRect.maxY();
    ASSERT(endY <= m_size.height());
    int numRows = endY - destY;

    unsigned srcBytesPerRow = 4 * sourceSize.width();
    SkBitmap srcBitmap;
    srcBitmap.setConfig(SkBitmap::kARGB_8888_Config, numColumns, numRows, srcBytesPerRow);
    srcBitmap.setPixels(source->data() + originY * srcBytesPerRow + originX * 4);

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;

    context()->platformContext()->writePixels(srcBitmap, destX, destY, config8888);
}

template <typename T>
static bool encodeImage(T& source, const String& mimeType, const double* quality, Vector<char>* output)
{
    Vector<unsigned char>* encodedImage = reinterpret_cast<Vector<unsigned char>*>(output);

    if (mimeType == "image/jpeg") {
        int compressionQuality = JPEGImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!JPEGImageEncoder::encode(source, compressionQuality, encodedImage))
            return false;
#if USE(WEBP)
    } else if (mimeType == "image/webp") {
        int compressionQuality = WEBPImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!WEBPImageEncoder::encode(source, compressionQuality, encodedImage))
            return false;
#endif
    } else {
        if (!PNGImageEncoder::encode(source, encodedImage))
            return false;
        ASSERT(mimeType == "image/png");
    }

    return true;
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality, CoordinateSystem) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage;
    if (!encodeImage(*context()->platformContext()->bitmap(), mimeType, quality, &encodedImage))
        return "data:,";

    Vector<char> base64Data;
    base64Encode(encodedImage, base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

void ImageBufferData::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this);
    info.addMember(m_canvas);
    info.addMember(m_platformContext);
#if USE(ACCELERATED_COMPOSITING)
    info.addMember(m_layerBridge);
#endif
}

String ImageDataToDataURL(const ImageData& imageData, const String& mimeType, const double* quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage;
    if (!encodeImage(imageData, mimeType, quality, &encodedImage))
        return "data:,";

    Vector<char> base64Data;
    base64Encode(encodedImage, base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

} // namespace WebCore
