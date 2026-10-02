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
#include "DeprecatedGlobalSettings.h"
#include "GraphicsContext.h"
#include "ImageAdapter.h"
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
    RefPtr image = bitmap->createImage();
    if (!image)
        return std::nullopt;
    return image.releaseNonNull();
}

bool Image::supportsType(const String& type)
{
    return MIMETypeRegistry::isSupportedImageMIMEType(type);
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

void Image::drawPattern(GraphicsContext& ctxt, ConcreteObjectSize concreteObjectSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, const ImageDrawingExtras* extras)
{
    RefPtr tileImage = currentPreTransformedNativeImage(concreteObjectSize, options.orientation(), extras);
    if (!tileImage)
        return;

    ctxt.drawPattern(*tileImage, destRect, tileRect, patternTransform, phase, spacing, options);

    if (auto observer = imageObserver())
        observer->didDraw(*this);
}

void Image::computeIntrinsicDimensions(float& intrinsicWidth, float& intrinsicHeight, FloatSize& intrinsicRatio)
{
    intrinsicRatio = size();
    intrinsicWidth = intrinsicRatio.width();
    intrinsicHeight = intrinsicRatio.height();
}

FloatSize Image::sourceSize(ImageOrientation orientation) const
{
    return size(orientation);
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

RefPtr<ShareableBitmap> Image::toShareableBitmap() const
{
    return toShareableBitmap(ConcreteObjectSize::fixed(size()));
}

RefPtr<ShareableBitmap> Image::toShareableBitmap(ConcreteObjectSize concreteObjectSize) const
{
    RefPtr bitmap = ShareableBitmap::create({ IntSize(concreteObjectSize.size()) });
    if (!bitmap)
        return nullptr;
    std::unique_ptr graphicsContext = bitmap->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    graphicsContext->drawImage(const_cast<Image&>(*this), concreteObjectSize, IntPoint());
    return bitmap;
}

void Image::dump(TextStream& ts) const
{
    if (isAnimated())
        ts.dumpProperty("animated"_s, isAnimated());

    if (isNull())
        ts.dumpProperty("is-null-image"_s, true);

    ts.dumpProperty("size"_s, size());
}

TextStream& operator<<(TextStream& ts, const Image& image)
{
    TextStream::GroupScope scope(ts);

    if (image.isBitmapImage())
        ts << "bitmap image"_s;
    else if (image.isCrossfadeGeneratedImage())
        ts << "crossfade image"_s;
    else if (image.isNamedImageGeneratedImage())
        ts << "named image"_s;
    else if (image.isGradientImage())
        ts << "gradient image"_s;
    else if (image.isSVGImage())
        ts << "svg image"_s;
    else if (image.isSVGResourceImage())
        ts << "svg resource image"_s;
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
