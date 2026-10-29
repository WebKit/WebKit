/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#include "Image.h"

#include "AffineTransform.h"
#include "BitmapImage.h"
#include "DefaultSizing.h"
#include "DeprecatedGlobalSettings.h"
#include "GraphicsContext.h"
#include "ImageAdapter.h"
#include "ImageBuffer.h"
#include "ImageDecoder.h"
#include "ImageDrawingExtras.h"
#include "ImageObserver.h"
#include "MIMETypeRegistry.h"
#include "NativeImage.h"
#include "SVGImage.h"
#include "ShareableBitmap.h"
#include "SharedBuffer.h"
#include <math.h>
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/TextStream.h>

#if USE(CG)
#include "PDFDocumentImage.h"
#include "UTIRegistry.h"
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WebCore {

Image::Image(ImageObserver* observer)
    : m_imageObserver(observer)
{
}

Image::~Image() = default;

RefPtr<ImageObserver> Image::imageObserver() const
{
    return m_imageObserver.get();
}

void Image::setImageObserver(RefPtr<ImageObserver>&& observer)
{
    m_imageObserver = observer.get();
}

ImageAdapter& Image::adapter()
{
    if (!m_adapter)
        lazyInitialize(m_adapter, makeUnique<ImageAdapter>(*this));
    return *m_adapter;
}

void Image::invalidateAdapter()
{
    if (!m_adapter)
        return;
    m_adapter->invalidate();
}

Image& Image::nullImage()
{
    return BitmapImage::nullImage();
}

static bool isPDFResource(const String& mimeType, const URL& url)
{
    if (mimeType.isEmpty())
        return url.path().endsWithIgnoringASCIICase(".pdf"_s);
    return MIMETypeRegistry::isPDFMIMEType(mimeType);
}

RefPtr<Image> Image::create(ImageObserver& observer)
{
    // SVGImage and PDFDocumentImage are not safe to use off the main thread.
    // Workers can use BitmapImage directly.
    ASSERT(isMainThread());

    auto mimeType = observer.mimeType();
    if (mimeType == "image/svg+xml"_s)
        return SVGImage::create(&observer);

    auto url = observer.sourceUrl();
    if (isPDFResource(mimeType, url)) {
#if USE(CG) && !USE(WEBKIT_IMAGE_DECODERS)
        if (!DeprecatedGlobalSettings::arePDFImagesEnabled())
            return nullptr;
        return PDFDocumentImage::create(&observer);
#else
        return nullptr;
#endif
    }

    return BitmapImage::create(&observer);
}

std::optional<Ref<Image>> Image::create(RefPtr<ShareableBitmap>&& bitmap)
{
    if (!bitmap)
        return std::nullopt;
    RefPtr image = BitmapImage::create(*bitmap);
    if (!image)
        return std::nullopt;
    return image.releaseNonNull();
}

bool Image::supportsType(const String& type)
{
    return MIMETypeRegistry::isSupportedImageMIMEType(type);
}

static RefPtr<NativeImage> tryCreateNativeImageFromBitmapImageData(std::span<const uint8_t> data, std::optional<FloatSize> preferredSize)
{
    Ref buffer = SharedBuffer::create(data);
    RefPtr imageDecoder = ImageDecoder::create(buffer.get(), { }, AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    if (!imageDecoder)
        return nullptr;

    imageDecoder->setData(buffer.get(), true);
    if (imageDecoder->encodedDataStatus() == EncodedDataStatus::Error)
        return nullptr;

#if USE(CG)
    if (!isSupportedImageType(imageDecoder->uti()))
        return nullptr;
#endif

    auto preferredIndex = [&] -> size_t {
        if (!preferredSize)
            return imageDecoder->primaryFrameIndex();
        size_t frameCount = imageDecoder->frameCount();
        for (size_t index = 0; index < frameCount; ++index) {
            if (imageDecoder->frameSizeAtIndex(index) == *preferredSize)
                return index;
        }
        return imageDecoder->primaryFrameIndex();
    };
    auto image = imageDecoder->createFrameImageAtIndex(preferredIndex());
    if (!image)
        return nullptr;

    return NativeImage::create(WTF::move(image));
}

static void tryCreateNativeImageFromData(std::span<const uint8_t> data, std::optional<FloatSize> preferredSize, CompletionHandler<void(RefPtr<NativeImage>&&)>&& completionHandler)
{
    if (RefPtr nativeImage = tryCreateNativeImageFromBitmapImageData(data, preferredSize)) {
        completionHandler(WTF::move(nativeImage));
        return;
    }
    SVGImage::tryCreateFromData(data, [preferredSize, completionHandler = WTF::move(completionHandler)](auto svgImage) mutable {
        if (!svgImage) {
            completionHandler(nullptr);
            return;
        }
        completionHandler(svgImage->nativeImage(DefaultSizing { preferredSize }.resolve(svgImage->naturalDimensions()).size()));
    });
}

static Vector<Ref<ShareableBitmap>> createBitmapsFromNativeImage(NativeImage& image, std::span<const unsigned> lengths)
{
    Vector<Ref<ShareableBitmap>> bitmaps;
    auto sourceColorSpace = image.colorSpace();
    // The conversion could lead to loss of HDR contents.
    auto destinationColorSpace = sourceColorSpace.supportsOutput() ? sourceColorSpace : ColorSpace::SRGB();
    for (auto length : lengths) {
        RefPtr bitmap = ShareableBitmap::createFromImageDraw(image, destinationColorSpace, { (int)length, (int)length }, image.size());
        if (!bitmap)
            return { };

        bitmaps.append(bitmap.releaseNonNull());
    }

    return bitmaps;
}

static RefPtr<NativeImage> createNativeImageFromSVGImage(SVGImage& image, const IntSize& size)
{
    RefPtr buffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, ColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return nullptr;

    image.draw(buffer->context(), ConcreteObjectSize::fixed(size));

    return ImageBuffer::sinkIntoNativeImage(WTF::move(buffer));
}

static Vector<Ref<ShareableBitmap>> createBitmapsFromSVGImage(SVGImage& image, std::span<const unsigned> lengths)
{
    Vector<Ref<ShareableBitmap>> bitmaps;
    for (auto length : lengths) {
        IntSize size { (int)length, (int)length };
        RefPtr nativeImage = createNativeImageFromSVGImage(image, size);
        if (!nativeImage)
            return { };

        RefPtr bitmap = ShareableBitmap::createFromImageDraw(*nativeImage, ColorSpace::SRGB());
        if (!bitmap)
            return { };

        bitmaps.append(bitmap.releaseNonNull());
    }

    return bitmaps;
}

void Image::decodeToShareableBitmaps(std::span<const uint8_t> data, std::span<const unsigned> lengths, CompletionHandler<void(Vector<Ref<ShareableBitmap>>&&)>&& completionHandler)
{
    if (RefPtr nativeImage = tryCreateNativeImageFromBitmapImageData(data, std::nullopt)) {
        completionHandler(createBitmapsFromNativeImage(*nativeImage, lengths));
        return;
    }

    SVGImage::tryCreateFromData(data, [lengthsVector = Vector<unsigned> { lengths }, completionHandler = WTF::move(completionHandler)](auto svgImage) mutable {
        if (!svgImage) {
            completionHandler({ });
            return;
        }
        completionHandler(createBitmapsFromSVGImage(*svgImage, lengthsVector.span()));
    });
}

void Image::decodeToShareableBitmap(std::span<const uint8_t> data, std::optional<FloatSize> preferredSize, CompletionHandler<void(RefPtr<ShareableBitmap>&&)>&& completionHandler)
{
    tryCreateNativeImageFromData(data, preferredSize, [completionHandler = WTF::move(completionHandler)](auto nativeImage) mutable {
        if (!nativeImage) {
            completionHandler(nullptr);
            return;
        }

        auto sourceColorSpace = nativeImage->colorSpace();
        auto destinationColorSpace = sourceColorSpace.supportsOutput() ? sourceColorSpace : ColorSpace::SRGB();
        RefPtr bitmap = ShareableBitmap::create({ nativeImage->size(), destinationColorSpace });
        if (!bitmap) {
            completionHandler(nullptr);
            return;
        }

        auto context = bitmap->createGraphicsContext();
        if (!context) {
            completionHandler(nullptr);
            return;
        }

        FloatRect rect { { }, nativeImage->size() };
        context->drawNativeImage(*nativeImage, rect, rect, { CompositeOperator::Copy });
        completionHandler(WTF::move(bitmap));
    });
}

void Image::subresourcesAreFinished(Document*, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

EncodedDataStatus Image::setData(RefPtr<FragmentedSharedBuffer>&& data, bool allDataReceived)
{
    m_encodedImageData = WTF::move(data);

    // Don't do anything; it is an empty image.
    if (!m_encodedImageData.get() || !m_encodedImageData->size())
        return EncodedDataStatus::Complete;

    return dataChanged(allDataReceived);
}

bool Image::tryReplaceData(Ref<FragmentedSharedBuffer>&& data)
{
    if (!canReplaceData())
        return false;

    // replaceData should only be called with an identical copy of previously set encoded data.
    ASSERT(m_encodedImageData && *m_encodedImageData == data.get());
    m_encodedImageData = WTF::move(data);
    dataReplaced();

    return true;
}

URL Image::sourceURL() const
{
    return imageObserver() ? imageObserver()->sourceUrl() : URL();
}

String Image::mimeType() const
{
    return imageObserver() ? imageObserver()->mimeType() : emptyString();
}

long long Image::expectedContentLength() const
{
    return imageObserver() ? imageObserver()->expectedContentLength() : 0;
}

ImageDrawResult Image::draw(GraphicsContext& context, ConcreteObjectSize concreteObjectSize, FloatPoint destination, ImageDrawingOptions options, const ImageDrawingExtras* extras)
{
    return draw(context, concreteObjectSize, FloatRect(destination, concreteObjectSize.size()), FloatRect({ }, concreteObjectSize.size()), options, extras);
}

ImageDrawResult Image::draw(GraphicsContext& context, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, ImageDrawingOptions options, const ImageDrawingExtras* extras)
{
    return draw(context, concreteObjectSize, destination, FloatRect({ }, concreteObjectSize.size()), options, extras);
}

ImageDrawResult Image::draw(GraphicsContext& context, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options, const ImageDrawingExtras* extras)
{
    if (context.paintingDisabled())
        return ImageDrawResult::DidNothing;
    return drawImage(context, concreteObjectSize, destination, source, options, extras);
}

void Image::fillWithSolidColor(GraphicsContext& ctxt, const FloatRect& dstRect, const Color& color, CompositeOperator op)
{
    if (!color.isVisible())
        return;

    CompositeOperator previousOperator = ctxt.compositeOperation();
    ctxt.setCompositeOperation(color.isOpaque() && op == CompositeOperator::SourceOver ? CompositeOperator::Copy : op);
    ctxt.fillRect(dstRect, color);
    ctxt.setCompositeOperation(previousOperator);
}

RefPtr<NativeImage> Image::nativeImage(ConcreteObjectSize, const ColorSpace&, const ImageDrawingExtras*)
{
    return nullptr;
}

RefPtr<NativeImage> Image::nativeImageAtIndex(unsigned, ConcreteObjectSize concreteObjectSize, const ImageDrawingExtras* extras)
{
    return nativeImage(concreteObjectSize, ColorSpace::SRGB(), extras);
}

RefPtr<NativeImage> Image::currentNativeImage(ConcreteObjectSize concreteObjectSize, const ImageDrawingExtras* extras)
{
    return nativeImage(concreteObjectSize, ColorSpace::SRGB(), extras);
}

RefPtr<NativeImage> Image::currentPreTransformedNativeImage(ConcreteObjectSize concreteObjectSize, ImageOrientation, const ImageDrawingExtras* extras)
{
    return currentNativeImage(concreteObjectSize, extras);
}

std::optional<Image::PatternTile> Image::currentPatternTile(GraphicsContext&, ConcreteObjectSize concreteObjectSize, ImageDrawingOptions options, const ImageDrawingExtras* extras)
{
    RefPtr tileImage = currentPreTransformedNativeImage(concreteObjectSize, options.orientation(), extras);
    if (!tileImage)
        return std::nullopt;

    if (auto observer = imageObserver())
        observer->didDraw(*this);

    return PatternTile { SourceImage { tileImage.releaseNonNull() } };
}

void Image::startAnimationAsynchronously()
{
    if (!m_animationStartTimer)
        lazyInitialize(m_animationStartTimer, makeUnique<Timer>(*this, &Image::startAnimation));
    if (m_animationStartTimer->isActive())
        return;
    m_animationStartTimer->startOneShot(0_s);
}

ColorSpace Image::colorSpace()
{
    return ColorSpace::SRGB();
}

RefPtr<ShareableBitmap> Image::toShareableBitmap(ConcreteObjectSize concreteObjectSize) const
{
    RefPtr bitmap = ShareableBitmap::create({ IntSize(concreteObjectSize.size()) });
    if (!bitmap)
        return nullptr;
    std::unique_ptr graphicsContext = bitmap->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    const_cast<Image&>(*this).draw(*graphicsContext, concreteObjectSize);
    return bitmap;
}

void Image::dump(TextStream& ts) const
{
    if (isAnimated())
        ts.dumpProperty("animated"_s, isAnimated());

    if (!hasSomethingToDraw())
        ts.dumpProperty("is-null-image"_s, true);

    auto naturalDimensions = unorientedNaturalDimensions();
    if (naturalDimensions.width)
        ts.dumpProperty("width"_s, *naturalDimensions.width);
    if (naturalDimensions.height)
        ts.dumpProperty("height"_s, *naturalDimensions.height);
    if (naturalDimensions.aspectRatio)
        ts.dumpProperty("aspect-ratio"_s, *naturalDimensions.aspectRatio);
}

TextStream& operator<<(TextStream& ts, const Image& image)
{
    TextStream::GroupScope scope(ts);

    if (image.isBitmapImage())
        ts << "bitmap image"_s;
    else if (image.isSVGImage())
        ts << "svg image"_s;
    else if (image.isPDFDocumentImage())
        ts << "pdf image"_s;

    image.dump(ts);
    return ts;
}

bool Image::animationPending() const
{
    return m_animationStartTimer && m_animationStartTimer->isActive();
}

bool Image::gSystemAllowsAnimationControls = false;

void Image::setSystemAllowsAnimationControls(bool allowsControls)
{
    gSystemAllowsAnimationControls = allowsControls;
}

std::optional<Color> Image::singlePixelSolidColor() const
{
    return std::nullopt;
}

ImageObserverDisableScope::ImageObserverDisableScope(Image& image, bool disable)
    : m_image(image)
    , m_observer(disable ? image.imageObserver() : nullptr)
    , m_disable(disable)
{
    if (m_disable)
        m_image->setImageObserver(nullptr);
}

ImageObserverDisableScope::~ImageObserverDisableScope()
{
    if (m_disable)
        m_image->setImageObserver(WTF::move(m_observer));
}

} // namespace WebCore
