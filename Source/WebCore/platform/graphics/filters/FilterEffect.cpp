/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FilterEffect.h"

#include "Color.h"
#include "Filter.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "Logging.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <wtf/text/TextStream.h>

#if HAVE(ARM_NEON_INTRINSICS)
#include <arm_neon.h>
#endif

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

namespace WebCore {

FilterEffect::FilterEffect(Filter& filter, Type type)
    : m_filter(filter)
    , m_filterEffectClassType(type)
{
}

FilterEffect::~FilterEffect() = default;

void FilterEffect::determineAbsolutePaintRect()
{
    m_absolutePaintRect = IntRect();
    for (auto& effect : m_inputEffects)
        m_absolutePaintRect.unite(effect->absolutePaintRect());
    clipAbsolutePaintRect();
}

void FilterEffect::clipAbsolutePaintRect()
{
    // Filters in SVG clip to primitive subregion, while CSS doesn't.
    if (m_clipsToBounds)
        m_absolutePaintRect.intersect(enclosingIntRect(m_maxEffectRect));
    else
        m_absolutePaintRect.unite(enclosingIntRect(m_maxEffectRect));
}

FloatPoint FilterEffect::mapPointFromUserSpaceToBuffer(FloatPoint userSpacePoint) const
{
    FloatPoint absolutePoint = mapPoint(userSpacePoint, m_filterPrimitiveSubregion, m_absoluteUnclippedSubregion);
    absolutePoint.moveBy(-m_absolutePaintRect.location());
    return absolutePoint;
}

IntRect FilterEffect::requestedRegionOfInputImageData(const IntRect& effectRect) const
{
    ASSERT(hasResult());
    IntPoint location = m_absolutePaintRect.location();
    location.moveBy(-effectRect.location());
    return IntRect(location, m_absolutePaintRect.size());
}

FloatRect FilterEffect::drawingRegionOfInputImage(const IntRect& srcRect) const
{
    ASSERT(hasResult());

    FloatSize scale;
    ImageBuffer::clampedSize(m_absolutePaintRect.size(), scale);

    AffineTransform transform;
    transform.scale(scale).translate(-m_absolutePaintRect.location());
    return transform.mapRect(srcRect);
}

FloatRect FilterEffect::determineFilterPrimitiveSubregion()
{
    // FETile, FETurbulence, FEFlood don't have input effects, take the filter region as unite rect.
    FloatRect subregion;
    if (unsigned numberOfInputEffects = inputEffects().size()) {
        subregion = inputEffect(0)->determineFilterPrimitiveSubregion();
        for (unsigned i = 1; i < numberOfInputEffects; ++i) {
            auto inputPrimitiveSubregion = inputEffect(i)->determineFilterPrimitiveSubregion();
            subregion.unite(inputPrimitiveSubregion);
        }
    } else
        subregion = m_filter.filterRegionInUserSpace();

    // After calling determineFilterPrimitiveSubregion on the target effect, reset the subregion again for <feTile>.
    if (filterEffectType() == FilterEffectTypeTile)
        subregion = m_filter.filterRegionInUserSpace();

    auto boundaries = effectBoundaries();
    if (hasX())
        subregion.setX(boundaries.x());
    if (hasY())
        subregion.setY(boundaries.y());
    if (hasWidth())
        subregion.setWidth(boundaries.width());
    if (hasHeight())
        subregion.setHeight(boundaries.height());

    setFilterPrimitiveSubregion(subregion);

    auto absoluteSubregion = m_filter.absoluteTransform().mapRect(subregion);
    auto filterResolution = m_filter.filterResolution();
    absoluteSubregion.scale(filterResolution);
    // Save this before clipping so we can use it to map lighting points from user space to buffer coordinates.
    setUnclippedAbsoluteSubregion(absoluteSubregion);

    // Clip every filter effect to the filter region.
    auto absoluteScaledFilterRegion = m_filter.filterRegion();
    absoluteScaledFilterRegion.scale(filterResolution);
    absoluteSubregion.intersect(absoluteScaledFilterRegion);

    setMaxEffectRect(absoluteSubregion);
    return subregion;
}

FilterEffect* FilterEffect::inputEffect(unsigned number) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(number < m_inputEffects.size());
    return m_inputEffects.at(number).get();
}

static unsigned collectEffects(const FilterEffect*effect, HashSet<const FilterEffect*>& allEffects)
{
    allEffects.add(effect);
    unsigned size = effect->numberOfEffectInputs();
    for (unsigned i = 0; i < size; ++i) {
        FilterEffect* in = effect->inputEffect(i);
        collectEffects(in, allEffects);
    }
    return allEffects.size();
}

unsigned FilterEffect::totalNumberOfEffectInputs() const
{
    HashSet<const FilterEffect*> allEffects;
    return collectEffects(this, allEffects);
}

void FilterEffect::apply()
{
    if (hasResult())
        return;
    unsigned size = m_inputEffects.size();
    for (unsigned i = 0; i < size; ++i) {
        FilterEffect* in = m_inputEffects.at(i).get();
        in->apply();
        if (!in->hasResult())
            return;

        // Convert input results to the current effect's color space.
        transformResultColorSpace(in, i);
    }

    determineAbsolutePaintRect();
    setResultColorSpace(m_operatingColorSpace);

    LOG_WITH_STREAM(Filters, stream << "FilterEffect " << filterName() << " " << this << " apply():\n  filterPrimitiveSubregion " << m_filterPrimitiveSubregion << "\n  effectBoundaries " << m_effectBoundaries << "\n  absoluteUnclippedSubregion " << m_absoluteUnclippedSubregion << "\n  absolutePaintRect " << m_absolutePaintRect << "\n  maxEffectRect " << m_maxEffectRect << "\n  filter scale " << m_filter.filterScale() << "\n  filter resolution " << m_filter.filterResolution());

    if (m_absolutePaintRect.isEmpty() || ImageBuffer::sizeNeedsClamping(m_absolutePaintRect.size()))
        return;

    if (requiresValidPreMultipliedPixels()) {
        for (unsigned i = 0; i < size; ++i)
            inputEffect(i)->correctFilterResultIfNeeded();
    }
    
    // Add platform specific apply functions here and return earlier.
    platformApplySoftware();
}

void FilterEffect::forceValidPreMultipliedPixels()
{
    // Must operate on pre-multiplied results; other formats cannot have invalid pixels.
    if (!m_premultipliedImageResult)
        return;

    Uint8ClampedArray* imageArray = m_premultipliedImageResult->data();
    uint8_t* pixelData = imageArray->data();
    int pixelArrayLength = imageArray->length();

    // We must have four bytes per pixel, and complete pixels
    ASSERT(!(pixelArrayLength % 4));

#if HAVE(ARM_NEON_INTRINSICS)
    if (pixelArrayLength >= 64) {
        uint8_t* lastPixel = pixelData + (pixelArrayLength & ~0x3f);
        do {
            // Increments pixelData by 64.
            uint8x16x4_t sixteenPixels = vld4q_u8(pixelData);
            sixteenPixels.val[0] = vminq_u8(sixteenPixels.val[0], sixteenPixels.val[3]);
            sixteenPixels.val[1] = vminq_u8(sixteenPixels.val[1], sixteenPixels.val[3]);
            sixteenPixels.val[2] = vminq_u8(sixteenPixels.val[2], sixteenPixels.val[3]);
            vst4q_u8(pixelData, sixteenPixels);
            pixelData += 64;
        } while (pixelData < lastPixel);

        pixelArrayLength &= 0x3f;
        if (!pixelArrayLength)
            return;
    }
#endif

    int numPixels = pixelArrayLength / 4;

    // Iterate over each pixel, checking alpha and adjusting color components if necessary
    while (--numPixels >= 0) {
        // Alpha is the 4th byte in a pixel
        uint8_t a = *(pixelData + 3);
        // Clamp each component to alpha, and increment the pixel location
        for (int i = 0; i < 3; ++i) {
            if (*pixelData > a)
                *pixelData = a;
            ++pixelData;
        }
        // Increment for alpha
        ++pixelData;
    }
}

void FilterEffect::clearResult()
{
    if (m_imageBufferResult)
        m_imageBufferResult.reset();

    m_unmultipliedImageResult = nullptr;
    m_premultipliedImageResult = nullptr;
}

void FilterEffect::clearResultsRecursive()
{
    // Clear all results, regardless that the current effect has
    // a result. Can be used if an effect is in an erroneous state.
    if (hasResult())
        clearResult();

    unsigned size = m_inputEffects.size();
    for (unsigned i = 0; i < size; ++i)
        m_inputEffects.at(i).get()->clearResultsRecursive();
}

ImageBuffer* FilterEffect::imageBufferResult()
{
    LOG_WITH_STREAM(Filters, stream << "FilterEffect " << filterName() << " " << this << " imageBufferResult(). Existing image buffer " << m_imageBufferResult.get() <<  " m_premultipliedImageResult " << ValueOrNull(m_premultipliedImageResult.get()) << " m_unmultipliedImageResult " << ValueOrNull(m_unmultipliedImageResult.get()));

    if (!hasResult())
        return nullptr;

    if (m_imageBufferResult)
        return m_imageBufferResult.get();

    m_imageBufferResult = ImageBuffer::create(m_absolutePaintRect.size(), m_filter.renderingMode(), m_filter.filterScale(), m_resultColorSpace);
    if (!m_imageBufferResult)
        return nullptr;

    IntRect destinationRect(IntPoint(), m_absolutePaintRect.size());
    if (m_premultipliedImageResult)
        m_imageBufferResult->putImageData(AlphaPremultiplication::Premultiplied, *m_premultipliedImageResult, destinationRect);
    else
        m_imageBufferResult->putImageData(AlphaPremultiplication::Unpremultiplied, *m_unmultipliedImageResult, destinationRect);
    return m_imageBufferResult.get();
}

RefPtr<Uint8ClampedArray> FilterEffect::unmultipliedResult(const IntRect& rect, Optional<ColorSpace> colorSpace)
{
    IntSize scaledSize(rect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(scaledSize));
    scaledSize.scale(m_filter.filterScale());
    auto imageData = Uint8ClampedArray::tryCreateUninitialized((scaledSize.area() * 4).unsafeGet());
    if (!imageData)
        return nullptr;
    copyUnmultipliedResult(*imageData, rect, colorSpace);
    return imageData;
}

RefPtr<Uint8ClampedArray> FilterEffect::premultipliedResult(const IntRect& rect, Optional<ColorSpace> colorSpace)
{
    IntSize scaledSize(rect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(scaledSize));
    scaledSize.scale(m_filter.filterScale());
    auto imageData = Uint8ClampedArray::tryCreateUninitialized((scaledSize.area() * 4).unsafeGet());
    if (!imageData)
        return nullptr;
    copyPremultipliedResult(*imageData, rect, colorSpace);
    return imageData;
}

void FilterEffect::copyImageBytes(const Uint8ClampedArray& source, Uint8ClampedArray& destination, const IntRect& rect) const
{
    IntRect scaledRect(rect);
    scaledRect.scale(m_filter.filterScale());
    IntSize scaledPaintSize(m_absolutePaintRect.size());
    scaledPaintSize.scale(m_filter.filterScale());

    // Initialize the destination to transparent black, if not entirely covered by the source.
    if (scaledRect.x() < 0 || scaledRect.y() < 0 || scaledRect.maxX() > scaledPaintSize.width() || scaledRect.maxY() > scaledPaintSize.height())
        memset(destination.data(), 0, destination.length());

    // Early return if the rect does not intersect with the source.
    if (scaledRect.maxX() <= 0 || scaledRect.maxY() <= 0 || scaledRect.x() >= scaledPaintSize.width() || scaledRect.y() >= scaledPaintSize.height())
        return;

    int xOrigin = scaledRect.x();
    int xDest = 0;
    if (xOrigin < 0) {
        xDest = -xOrigin;
        xOrigin = 0;
    }
    int xEnd = scaledRect.maxX();
    if (xEnd > scaledPaintSize.width())
        xEnd = scaledPaintSize.width();

    int yOrigin = scaledRect.y();
    int yDest = 0;
    if (yOrigin < 0) {
        yDest = -yOrigin;
        yOrigin = 0;
    }
    int yEnd = scaledRect.maxY();
    if (yEnd > scaledPaintSize.height())
        yEnd = scaledPaintSize.height();

    int size = (xEnd - xOrigin) * 4;
    int destinationScanline = scaledRect.width() * 4;
    int sourceScanline = scaledPaintSize.width() * 4;
    uint8_t* destinationPixel = destination.data() + ((yDest * scaledRect.width()) + xDest) * 4;
    const uint8_t* sourcePixel = source.data() + ((yOrigin * scaledPaintSize.width()) + xOrigin) * 4;

    while (yOrigin < yEnd) {
        memcpy(destinationPixel, sourcePixel, size);
        destinationPixel += destinationScanline;
        sourcePixel += sourceScanline;
        ++yOrigin;
    }
}

static void copyPremultiplyingAlpha(const Uint8ClampedArray& source, Uint8ClampedArray& destination, const IntSize& inputSize)
{
#if USE(ACCELERATE)
    size_t rowBytes = inputSize.width() * 4;

    vImage_Buffer src;
    src.width = inputSize.width();
    src.height = inputSize.height();
    src.rowBytes = rowBytes;
    src.data = reinterpret_cast<void*>(source.data());

    vImage_Buffer dest;
    dest.width = inputSize.width();
    dest.height = inputSize.height();
    dest.rowBytes = rowBytes;
    dest.data = reinterpret_cast<void*>(destination.data());

    vImagePremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
#else
    const uint8_t* sourceComponent = source.data();
    const uint8_t* end = sourceComponent + (inputSize.area() * 4).unsafeGet();
    uint8_t* destinationComponent = destination.data();

    while (sourceComponent < end) {
        int alpha = sourceComponent[3];
        destinationComponent[0] = static_cast<int>(sourceComponent[0]) * alpha / 255;
        destinationComponent[1] = static_cast<int>(sourceComponent[1]) * alpha / 255;
        destinationComponent[2] = static_cast<int>(sourceComponent[2]) * alpha / 255;
        destinationComponent[3] = alpha;
        sourceComponent += 4;
        destinationComponent += 4;
    }
#endif
}

static void copyUnpremultiplyingAlpha(const Uint8ClampedArray& source, Uint8ClampedArray& destination, const IntSize& inputSize)
{
#if USE(ACCELERATE)
    size_t rowBytes = inputSize.width() * 4;

    vImage_Buffer src;
    src.width = inputSize.width();
    src.height = inputSize.height();
    src.rowBytes = rowBytes;
    src.data = reinterpret_cast<void*>(source.data());

    vImage_Buffer dest;
    dest.width = inputSize.width();
    dest.height = inputSize.height();
    dest.rowBytes = rowBytes;
    dest.data = reinterpret_cast<void*>(destination.data());

    vImageUnpremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
#else
    const uint8_t* sourceComponent = source.data();
    const uint8_t* end = sourceComponent + (inputSize.area() * 4).unsafeGet();
    uint8_t* destinationComponent = destination.data();
    while (sourceComponent < end) {
        int alpha = sourceComponent[3];
        if (alpha) {
            destinationComponent[0] = static_cast<int>(sourceComponent[0]) * 255 / alpha;
            destinationComponent[1] = static_cast<int>(sourceComponent[1]) * 255 / alpha;
            destinationComponent[2] = static_cast<int>(sourceComponent[2]) * 255 / alpha;
        } else {
            destinationComponent[0] = 0;
            destinationComponent[1] = 0;
            destinationComponent[2] = 0;
        }
        destinationComponent[3] = alpha;
        sourceComponent += 4;
        destinationComponent += 4;
    }
#endif
}

RefPtr<ImageData> FilterEffect::convertImageDataToColorSpace(ColorSpace targetColorSpace, ImageData& inputData, AlphaPremultiplication outputFormat)
{
    IntRect destinationRect(IntPoint(), inputData.size());
    FloatSize clampedSize = ImageBuffer::clampedSize(inputData.size());
    // Create an ImageBuffer to store incoming ImageData
    auto buffer = ImageBuffer::create(clampedSize, m_filter.renderingMode(), m_filter.filterScale(), operatingColorSpace());
    if (!buffer)
        return nullptr;
    buffer->putImageData(outputFormat, inputData, destinationRect);
    return convertImageBufferToColorSpace(targetColorSpace, *buffer, destinationRect, outputFormat);
}

RefPtr<ImageData> FilterEffect::convertImageBufferToColorSpace(ColorSpace targetColorSpace, ImageBuffer& inputBuffer, const IntRect& rect, AlphaPremultiplication outputFormat)
{
    FloatSize clampedSize = ImageBuffer::clampedSize(rect.size());
    // Create an ImageBuffer with the correct color space and utilize CG to handle color space conversion
    auto convertedBuffer = ImageBuffer::create(clampedSize, m_filter.renderingMode(), m_filter.filterScale(), targetColorSpace);
    if (!convertedBuffer)
        return nullptr;
    // Color space conversion happens internally when drawing from one image buffer to another
    convertedBuffer->context().drawImageBuffer(inputBuffer, rect);
    return convertedBuffer->getImageData(outputFormat, rect);
}

void FilterEffect::copyConvertedImageBufferToDestination(Uint8ClampedArray& destination, ColorSpace colorSpace, AlphaPremultiplication outputFormat, const IntRect& destRect)
{
    // Converts the data stored in m_imageBufferResult, and save to destination
    auto convertedImageData = convertImageBufferToColorSpace(colorSpace, *m_imageBufferResult, { IntPoint(), m_absolutePaintRect.size() }, outputFormat);
    if (!convertedImageData)
        return;
    copyImageBytes(*convertedImageData->data(), destination, destRect);
}

void FilterEffect::copyConvertedImageDataToDestination(Uint8ClampedArray& destination, ImageData& imageData, ColorSpace colorSpace, AlphaPremultiplication outputFormat, const IntRect& destRect)
{
    // Converts the data stored in m_unmultipliedImageResult/m_premultipliedImageResult,
    // whichever isn't null, and save to destination
    auto convertedImageData = convertImageDataToColorSpace(colorSpace, imageData, outputFormat);
    if (!convertedImageData)
        return;
    copyImageBytes(*convertedImageData->data(), destination, destRect);
}

void FilterEffect::copyUnmultipliedResult(Uint8ClampedArray& destination, const IntRect& rect, Optional<ColorSpace> colorSpace)
{
    ASSERT(hasResult());
    
    LOG_WITH_STREAM(Filters, stream << "FilterEffect " << filterName() << " " << this << " copyUnmultipliedResult(). Existing image buffer " << m_imageBufferResult.get() <<  " m_premultipliedImageResult " << ValueOrNull(m_premultipliedImageResult.get()) << " m_unmultipliedImageResult " << ValueOrNull(m_unmultipliedImageResult.get()));

    if (!m_unmultipliedImageResult) {
        // We prefer a conversion from the image buffer.
        if (m_imageBufferResult) {
            if (requiresImageDataColorSpaceConversion(colorSpace)) {
                copyConvertedImageBufferToDestination(destination, *colorSpace, AlphaPremultiplication::Unpremultiplied, rect);
                return;
            }
            m_unmultipliedImageResult = m_imageBufferResult->getImageData(AlphaPremultiplication::Unpremultiplied, { IntPoint(), m_absolutePaintRect.size() });
            if (!m_unmultipliedImageResult)
                return;
        } else {
            IntSize inputSize(m_absolutePaintRect.size());
            ASSERT(!ImageBuffer::sizeNeedsClamping(inputSize));
            inputSize.scale(m_filter.filterScale());
            m_unmultipliedImageResult = ImageData::create(inputSize);
            if (!m_unmultipliedImageResult)
                return;
            copyUnpremultiplyingAlpha(*m_premultipliedImageResult->data(), *m_unmultipliedImageResult->data(), inputSize);
        }
    }
    if (requiresImageDataColorSpaceConversion(colorSpace)) {
        copyConvertedImageDataToDestination(destination, *m_unmultipliedImageResult, *colorSpace, AlphaPremultiplication::Unpremultiplied, rect);
        return;
    }
    copyImageBytes(*m_unmultipliedImageResult->data(), destination, rect);
}

void FilterEffect::copyPremultipliedResult(Uint8ClampedArray& destination, const IntRect& rect, Optional<ColorSpace> colorSpace)
{
    ASSERT(hasResult());

    LOG_WITH_STREAM(Filters, stream << "FilterEffect " << filterName() << " " << this << " copyPremultipliedResult(). Existing image buffer " << m_imageBufferResult.get() <<  " m_premultipliedImageResult " << ValueOrNull(m_premultipliedImageResult.get()) << " m_unmultipliedImageResult " << ValueOrNull(m_unmultipliedImageResult.get()));

    if (!m_premultipliedImageResult) {
        // We prefer a conversion from the image buffer.
        if (m_imageBufferResult) {
            if (requiresImageDataColorSpaceConversion(colorSpace)) {
                copyConvertedImageBufferToDestination(destination, *colorSpace, AlphaPremultiplication::Premultiplied, rect);
                return;
            }
            m_premultipliedImageResult = m_imageBufferResult->getImageData(AlphaPremultiplication::Premultiplied, { IntPoint(), m_absolutePaintRect.size() });
            if (!m_premultipliedImageResult)
                return;
        } else {
            IntSize inputSize(m_absolutePaintRect.size());
            ASSERT(!ImageBuffer::sizeNeedsClamping(inputSize));
            inputSize.scale(m_filter.filterScale());
            m_premultipliedImageResult = ImageData::create(inputSize);
            if (!m_premultipliedImageResult)
                return;
            copyPremultiplyingAlpha(*m_unmultipliedImageResult->data(), *m_premultipliedImageResult->data(), inputSize);
        }
    }
    
    RefPtr<ImageData> convertedImageData;
    if (requiresImageDataColorSpaceConversion(colorSpace)) {
        copyConvertedImageDataToDestination(destination, *m_premultipliedImageResult, *colorSpace, AlphaPremultiplication::Premultiplied, rect);
        return;
    }
    copyImageBytes(*m_premultipliedImageResult->data(), destination, rect);
}

ImageBuffer* FilterEffect::createImageBufferResult()
{
    LOG(Filters, "FilterEffect %s %p createImageBufferResult %dx%d", filterName(), this, m_absolutePaintRect.size().width(), m_absolutePaintRect.size().height());

    // Only one result type is allowed.
    ASSERT(!hasResult());
    if (m_absolutePaintRect.isEmpty())
        return nullptr;

    FloatSize clampedSize = ImageBuffer::clampedSize(m_absolutePaintRect.size());
    m_imageBufferResult = ImageBuffer::create(clampedSize, m_filter.renderingMode(), m_filter.filterScale(), m_resultColorSpace);
    return m_imageBufferResult.get();
}

ImageData* FilterEffect::createUnmultipliedImageResult()
{
    LOG(Filters, "FilterEffect %s %p createUnmultipliedImageResult", filterName(), this);

    // Only one result type is allowed.
    ASSERT(!hasResult());
    if (m_absolutePaintRect.isEmpty())
        return nullptr;

    IntSize resultSize(m_absolutePaintRect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(resultSize));
    resultSize.scale(m_filter.filterScale());
    m_unmultipliedImageResult = ImageData::create(resultSize);
    return m_unmultipliedImageResult.get();
}

ImageData* FilterEffect::createPremultipliedImageResult()
{
    LOG(Filters, "FilterEffect %s %p createPremultipliedImageResult", filterName(), this);

    // Only one result type is allowed.
    ASSERT(!hasResult());
    if (m_absolutePaintRect.isEmpty())
        return nullptr;

    IntSize resultSize(m_absolutePaintRect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(resultSize));
    resultSize.scale(m_filter.filterScale());
    m_premultipliedImageResult = ImageData::create(resultSize);
    return m_premultipliedImageResult.get();
}

bool FilterEffect::requiresImageDataColorSpaceConversion(Optional<ColorSpace> dstColorSpace)
{
#if USE(CG)
    // This function determines whether we need the step of an extra color space conversion
    // We only need extra color conversion when 1) color space is different in the input
    // AND 2) the filter is manipulating raw pixels
    return dstColorSpace && resultColorSpace() != *dstColorSpace;
#else
    // Additional color space conversion is not needed on non-CG
    UNUSED_PARAM(dstColorSpace);
    return false;
#endif
}

void FilterEffect::transformResultColorSpace(ColorSpace dstColorSpace)
{
#if USE(CG)
    // CG handles color space adjustments internally.
    UNUSED_PARAM(dstColorSpace);
#else
    if (!hasResult() || dstColorSpace == m_resultColorSpace)
        return;

    // FIXME: We can avoid this potentially unnecessary ImageBuffer conversion by adding
    // color space transform support for the {pre,un}multiplied arrays.
    imageBufferResult()->transformColorSpace(m_resultColorSpace, dstColorSpace);

    m_resultColorSpace = dstColorSpace;

    if (m_unmultipliedImageResult)
        m_unmultipliedImageResult = nullptr;
    if (m_premultipliedImageResult)
        m_premultipliedImageResult = nullptr;
#endif
}

TextStream& FilterEffect::externalRepresentation(TextStream& ts, RepresentationType representationType) const
{
    // FIXME: We should dump the subRegions of the filter primitives here later. This isn't
    // possible at the moment, because we need more detailed informations from the target object.
    
    if (representationType == RepresentationType::Debugging) {
        TextStream::IndentScope indentScope(ts);
        ts.dumpProperty("alpha image", m_alphaImage);
        ts.dumpProperty("operating colorspace", m_operatingColorSpace);
        ts.dumpProperty("result colorspace", m_resultColorSpace);
        ts << "\n" << indent;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const FilterEffect& filter)
{
    // Use a new stream because we want multiline mode for logging filters.
    TextStream filterStream;
    filter.externalRepresentation(filterStream, FilterEffect::RepresentationType::Debugging);
    
    return ts << filterStream.release();
}

} // namespace WebCore
