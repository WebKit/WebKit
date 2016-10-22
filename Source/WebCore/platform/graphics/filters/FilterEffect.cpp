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

#include "Filter.h"
#include "ImageBuffer.h"
#include "TextStream.h"
#include <runtime/JSCInlines.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/Uint8ClampedArray.h>

#if HAVE(ARM_NEON_INTRINSICS)
#include <arm_neon.h>
#endif

namespace WebCore {

FilterEffect::FilterEffect(Filter& filter)
    : m_alphaImage(false)
    , m_filter(filter)
    , m_hasX(false)
    , m_hasY(false)
    , m_hasWidth(false)
    , m_hasHeight(false)
    , m_clipsToBounds(true)
    , m_operatingColorSpace(ColorSpaceLinearRGB)
    , m_resultColorSpace(ColorSpaceSRGB)
{
}

FilterEffect::~FilterEffect()
{
}

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

    Uint8ClampedArray* imageArray = m_premultipliedImageResult.get();
    unsigned char* pixelData = imageArray->data();
    int pixelArrayLength = imageArray->length();

    // We must have four bytes per pixel, and complete pixels
    ASSERT(!(pixelArrayLength % 4));

#if HAVE(ARM_NEON_INTRINSICS)
    if (pixelArrayLength >= 64) {
        unsigned char* lastPixel = pixelData + (pixelArrayLength & ~0x3f);
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
        unsigned char a = *(pixelData + 3);
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

ImageBuffer* FilterEffect::asImageBuffer()
{
    if (!hasResult())
        return nullptr;
    if (m_imageBufferResult)
        return m_imageBufferResult.get();
    m_imageBufferResult = ImageBuffer::create(m_absolutePaintRect.size(), m_filter.renderingMode(), m_filter.filterScale(), m_resultColorSpace);
    if (!m_imageBufferResult)
        return nullptr;

    IntRect destinationRect(IntPoint(), m_absolutePaintRect.size());
    if (m_premultipliedImageResult)
        m_imageBufferResult->putByteArray(Premultiplied, m_premultipliedImageResult.get(), destinationRect.size(), destinationRect, IntPoint());
    else
        m_imageBufferResult->putByteArray(Unmultiplied, m_unmultipliedImageResult.get(), destinationRect.size(), destinationRect, IntPoint());
    return m_imageBufferResult.get();
}

PassRefPtr<Uint8ClampedArray> FilterEffect::asUnmultipliedImage(const IntRect& rect)
{
    IntSize scaledSize(rect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(scaledSize));
    scaledSize.scale(m_filter.filterScale());
    auto imageData = Uint8ClampedArray::createUninitialized((scaledSize.area() * 4).unsafeGet());
    copyUnmultipliedImage(imageData.get(), rect);
    return WTFMove(imageData);
}

PassRefPtr<Uint8ClampedArray> FilterEffect::asPremultipliedImage(const IntRect& rect)
{
    IntSize scaledSize(rect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(scaledSize));
    scaledSize.scale(m_filter.filterScale());
    auto imageData = Uint8ClampedArray::createUninitialized((scaledSize.area() * 4).unsafeGet());
    copyPremultipliedImage(imageData.get(), rect);
    return WTFMove(imageData);
}

inline void FilterEffect::copyImageBytes(Uint8ClampedArray* source, Uint8ClampedArray* destination, const IntRect& rect)
{
    IntRect scaledRect(rect);
    scaledRect.scale(m_filter.filterScale());
    IntSize scaledPaintSize(m_absolutePaintRect.size());
    scaledPaintSize.scale(m_filter.filterScale());

    if (!source || !destination)
        return;

    // Initialize the destination to transparent black, if not entirely covered by the source.
    if (scaledRect.x() < 0 || scaledRect.y() < 0 || scaledRect.maxX() > scaledPaintSize.width() || scaledRect.maxY() > scaledPaintSize.height())
        memset(destination->data(), 0, destination->length());

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
    unsigned char *destinationPixel = destination->data() + ((yDest * scaledRect.width()) + xDest) * 4;
    unsigned char *sourcePixel = source->data() + ((yOrigin * scaledPaintSize.width()) + xOrigin) * 4;

    while (yOrigin < yEnd) {
        memcpy(destinationPixel, sourcePixel, size);
        destinationPixel += destinationScanline;
        sourcePixel += sourceScanline;
        ++yOrigin;
    }
}

void FilterEffect::copyUnmultipliedImage(Uint8ClampedArray* destination, const IntRect& rect)
{
    ASSERT(hasResult());

    if (!m_unmultipliedImageResult) {
        // We prefer a conversion from the image buffer.
        if (m_imageBufferResult)
            m_unmultipliedImageResult = m_imageBufferResult->getUnmultipliedImageData(IntRect(IntPoint(), m_absolutePaintRect.size()));
        else {
            IntSize inputSize(m_absolutePaintRect.size());
            ASSERT(!ImageBuffer::sizeNeedsClamping(inputSize));
            inputSize.scale(m_filter.filterScale());
            m_unmultipliedImageResult = Uint8ClampedArray::createUninitialized((inputSize.area() * 4).unsafeGet());
            if (!m_unmultipliedImageResult) {
                WTFLogAlways("FilterEffect::copyUnmultipliedImage Unable to create buffer. Requested size was %d x %d\n", inputSize.width(), inputSize.height());
                return;
            }
            unsigned char* sourceComponent = m_premultipliedImageResult->data();
            unsigned char* destinationComponent = m_unmultipliedImageResult->data();
            unsigned char* end = sourceComponent + (inputSize.area() * 4).unsafeGet();
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
        }
    }
    copyImageBytes(m_unmultipliedImageResult.get(), destination, rect);
}

void FilterEffect::copyPremultipliedImage(Uint8ClampedArray* destination, const IntRect& rect)
{
    ASSERT(hasResult());

    if (!m_premultipliedImageResult) {
        // We prefer a conversion from the image buffer.
        if (m_imageBufferResult)
            m_premultipliedImageResult = m_imageBufferResult->getPremultipliedImageData(IntRect(IntPoint(), m_absolutePaintRect.size()));
        else {
            IntSize inputSize(m_absolutePaintRect.size());
            ASSERT(!ImageBuffer::sizeNeedsClamping(inputSize));
            inputSize.scale(m_filter.filterScale());
            m_premultipliedImageResult = Uint8ClampedArray::createUninitialized((inputSize.area() * 4).unsafeGet());
            if (!m_premultipliedImageResult) {
                WTFLogAlways("FilterEffect::copyPremultipliedImage Unable to create buffer. Requested size was %d x %d\n", inputSize.width(), inputSize.height());
                return;
            }
            unsigned char* sourceComponent = m_unmultipliedImageResult->data();
            unsigned char* destinationComponent = m_premultipliedImageResult->data();
            unsigned char* end = sourceComponent + (inputSize.area() * 4).unsafeGet();
            while (sourceComponent < end) {
                int alpha = sourceComponent[3];
                destinationComponent[0] = static_cast<int>(sourceComponent[0]) * alpha / 255;
                destinationComponent[1] = static_cast<int>(sourceComponent[1]) * alpha / 255;
                destinationComponent[2] = static_cast<int>(sourceComponent[2]) * alpha / 255;
                destinationComponent[3] = alpha;
                sourceComponent += 4;
                destinationComponent += 4;
            }
        }
    }
    copyImageBytes(m_premultipliedImageResult.get(), destination, rect);
}

ImageBuffer* FilterEffect::createImageBufferResult()
{
    // Only one result type is allowed.
    ASSERT(!hasResult());
    if (m_absolutePaintRect.isEmpty())
        return nullptr;

    FloatSize clampedSize = ImageBuffer::clampedSize(m_absolutePaintRect.size());
    m_imageBufferResult = ImageBuffer::create(clampedSize, m_filter.renderingMode(), m_filter.filterScale(), m_resultColorSpace);
    if (!m_imageBufferResult)
        return nullptr;

    return m_imageBufferResult.get();
}

Uint8ClampedArray* FilterEffect::createUnmultipliedImageResult()
{
    // Only one result type is allowed.
    ASSERT(!hasResult());
    if (m_absolutePaintRect.isEmpty())
        return nullptr;

    IntSize resultSize(m_absolutePaintRect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(resultSize));
    resultSize.scale(m_filter.filterScale());
    m_unmultipliedImageResult = Uint8ClampedArray::createUninitialized((resultSize.area() * 4).unsafeGet());
    return m_unmultipliedImageResult.get();
}

Uint8ClampedArray* FilterEffect::createPremultipliedImageResult()
{
    // Only one result type is allowed.
    ASSERT(!hasResult());
    if (m_absolutePaintRect.isEmpty())
        return nullptr;

    IntSize resultSize(m_absolutePaintRect.size());
    ASSERT(!ImageBuffer::sizeNeedsClamping(resultSize));
    resultSize.scale(m_filter.filterScale());
    m_premultipliedImageResult = Uint8ClampedArray::createUninitialized((resultSize.area() * 4).unsafeGet());
    return m_premultipliedImageResult.get();
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
    asImageBuffer()->transformColorSpace(m_resultColorSpace, dstColorSpace);

    m_resultColorSpace = dstColorSpace;

    if (m_unmultipliedImageResult)
        m_unmultipliedImageResult = nullptr;
    if (m_premultipliedImageResult)
        m_premultipliedImageResult = nullptr;
#endif
}

TextStream& FilterEffect::externalRepresentation(TextStream& ts, int) const
{
    // FIXME: We should dump the subRegions of the filter primitives here later. This isn't
    // possible at the moment, because we need more detailed informations from the target object.
    return ts;
}

} // namespace WebCore
