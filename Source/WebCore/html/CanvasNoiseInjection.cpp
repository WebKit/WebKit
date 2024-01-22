/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
#include "CanvasNoiseInjection.h"

#include "ByteArrayPixelBuffer.h"
#include "FloatRect.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"

namespace WebCore {

void CanvasNoiseInjection::updateDirtyRect(const IntRect& rect)
{
    m_postProcessDirtyRect.unite(rect);
}

void CanvasNoiseInjection::clearDirtyRect()
{
    m_postProcessDirtyRect = { };
}

static inline bool isIndexInBounds(int size, int index)
{
    ASSERT(index <= size);
    return index < size;
}

static inline bool setTightnessBounds(const std::span<uint8_t>& bytes, std::array<int, 4>& tightestBoundingDiff, int index1, int index2, int index3)
{
    int boundingRedDiff = std::abs(static_cast<int>(bytes[index1]) - static_cast<int>(bytes[index3]));
    int boundingGreenDiff = std::abs(static_cast<int>(bytes[index1 + 1]) - static_cast<int>(bytes[index3 + 1]));
    int boundingBlueDiff = std::abs(static_cast<int>(bytes[index1 + 2]) - static_cast<int>(bytes[index3 + 2]));
    int boundingAlphaDiff = std::abs(static_cast<int>(bytes[index1 + 3]) - static_cast<int>(bytes[index3 + 3]));

    int neighborRedDiff1 = std::abs(static_cast<int>(bytes[index1]) - static_cast<int>(bytes[index2]));
    int neighborGreenDiff1 = std::abs(static_cast<int>(bytes[index1 + 1]) - static_cast<int>(bytes[index2 + 1]));
    int neighborBlueDiff1 = std::abs(static_cast<int>(bytes[index1 + 2]) - static_cast<int>(bytes[index2 + 2]));
    int neighborAlphaDiff1 = std::abs(static_cast<int>(bytes[index1 + 3]) - static_cast<int>(bytes[index2 + 3]));

    int neighborRedDiff2 = std::abs(static_cast<int>(bytes[index2]) - static_cast<int>(bytes[index3]));
    int neighborGreenDiff2 = std::abs(static_cast<int>(bytes[index2 + 1]) - static_cast<int>(bytes[index3 + 1]));
    int neighborBlueDiff2 = std::abs(static_cast<int>(bytes[index2 + 2]) - static_cast<int>(bytes[index3 + 2]));
    int neighborAlphaDiff2 = std::abs(static_cast<int>(bytes[index2 + 3]) - static_cast<int>(bytes[index3 + 3]));

    bool updatedTightnessBounds { false };

    if (boundingRedDiff <= 1 && boundingGreenDiff <= 1 && boundingBlueDiff <= 1 && boundingAlphaDiff <= 1
        && neighborRedDiff1 <= 1 && neighborGreenDiff1 <= 1 && neighborBlueDiff1 <= 1 && neighborAlphaDiff1 <= 1
        && neighborRedDiff2 <= 1 && neighborGreenDiff2 <= 1 && neighborBlueDiff2 <= 1 && neighborAlphaDiff2 <= 1) {
        tightestBoundingDiff[0] = 0;
        tightestBoundingDiff[1] = 0;
        tightestBoundingDiff[2] = 0;
        tightestBoundingDiff[3] = 0;
        updatedTightnessBounds = true;
    } else if (boundingRedDiff < tightestBoundingDiff[0]
        && boundingGreenDiff < tightestBoundingDiff[1]
        && boundingBlueDiff < tightestBoundingDiff[2]
        && boundingAlphaDiff < tightestBoundingDiff[3]) {
        tightestBoundingDiff[0] = boundingRedDiff;
        tightestBoundingDiff[1] = boundingGreenDiff;
        tightestBoundingDiff[2] = boundingBlueDiff;
        tightestBoundingDiff[3] = boundingAlphaDiff;
        updatedTightnessBounds = true;
    }
    return updatedTightnessBounds;
}

static std::pair<std::array<int, 4>, std::array<int, 4>> boundingNeighbors(int index, const std::span<uint8_t>& bytes, const IntSize& size)
{
    constexpr auto bytesPerPixel = 4U;
    auto bufferSize = bytes.size_bytes();
    auto pixelIndex = index / bytesPerPixel;
    bool isInTopRow = pixelIndex < static_cast<size_t>(size.width());
    bool isInBottomRow = pixelIndex > static_cast<size_t>((size.height() - 1) * size.width());
    bool isInLeftColumn = !(pixelIndex % size.width());
    bool isInRightColumn = (pixelIndex % size.width()) == static_cast<unsigned>(size.width()) - 1;
    bool isInTopLeftCorner = isInTopRow && isInLeftColumn;
    bool isInBottomLeftCorner = isInBottomRow && isInLeftColumn;
    bool isInTopRightCorner = isInTopRow && isInRightColumn;
    bool isInBotomRightCorner = isInBottomRow && isInRightColumn;

    constexpr auto leftOffset = -bytesPerPixel;
    constexpr auto rightOffset = bytesPerPixel;
    const auto aboveOffset = -size.width() * bytesPerPixel;
    const auto belowOffset = size.width() * bytesPerPixel;
    const auto aboveLeftOffset = aboveOffset + leftOffset;
    const auto aboveRightOffset = aboveOffset + rightOffset;
    const auto belowLeftOffset = belowOffset + leftOffset;
    const auto belowRightOffset = belowOffset + rightOffset;

    const int leftIndex = index + leftOffset;
    const int rightIndex = index + rightOffset;
    const int aboveIndex = index + aboveOffset;
    const int belowIndex = index + belowOffset;
    const int aboveLeftIndex = index + aboveLeftOffset;
    const int aboveRightIndex = index + aboveRightOffset;
    const int belowLeftIndex = index + belowLeftOffset;
    const int belowRightIndex = index + belowRightOffset;

    const auto areColorsRelated = [&bytes, bufferSize](int index1, int index2, int index3) {
        constexpr auto maxDistanceThreshold = 8;
        if (!isIndexInBounds(bufferSize, index1) || !isIndexInBounds(bufferSize, index1 + 3))
            return false;
        if (!isIndexInBounds(bufferSize, index2) || !isIndexInBounds(bufferSize, index2 + 3))
            return false;
        if (!isIndexInBounds(bufferSize, index3) || !isIndexInBounds(bufferSize, index3 + 3))
            return false;
        bool isColorNearBoundingColor1 = std::abs(static_cast<int>(bytes[index1]) - static_cast<int>(bytes[index2])) <= maxDistanceThreshold
            && std::abs(static_cast<int>(bytes[index1 + 1]) - static_cast<int>(bytes[index2 + 1])) <= maxDistanceThreshold
            && std::abs(static_cast<int>(bytes[index1 + 2]) - static_cast<int>(bytes[index2 + 2])) <= maxDistanceThreshold
            && std::abs(static_cast<int>(bytes[index1 + 3]) - static_cast<int>(bytes[index2 + 3])) <= maxDistanceThreshold;

        bool isColorNearBoundingColor2 =  std::abs(static_cast<int>(bytes[index3]) - static_cast<int>(bytes[index2])) <= maxDistanceThreshold
            && std::abs(static_cast<int>(bytes[index3 + 1]) - static_cast<int>(bytes[index2 + 1])) <= maxDistanceThreshold
            && std::abs(static_cast<int>(bytes[index3 + 2]) - static_cast<int>(bytes[index2 + 2])) <= maxDistanceThreshold
            && std::abs(static_cast<int>(bytes[index3 + 3]) - static_cast<int>(bytes[index2 + 3])) <= maxDistanceThreshold;

        return isColorNearBoundingColor1 || isColorNearBoundingColor2;
    };

    const auto compareColorsAndSetBounds = [&areColorsRelated](const auto& bytes, auto& tightestBoundingColors, auto& tightestBoundingDiff, auto colorIndex, auto neighborIndex1, auto neighborIndex2) {
        if (areColorsRelated(neighborIndex1, colorIndex, neighborIndex2)) {
            if (setTightnessBounds(bytes, tightestBoundingDiff, neighborIndex1, colorIndex, neighborIndex2)) {
                if (WTF::allOf(tightestBoundingDiff, [](auto& item) { return !item; })) {
                    tightestBoundingColors = {
                        { bytes[colorIndex], bytes[colorIndex + 1], bytes[colorIndex + 2], bytes[colorIndex + 3] },
                        { bytes[colorIndex], bytes[colorIndex + 1], bytes[colorIndex + 2], bytes[colorIndex + 3] }
                    };
                } else {
                    tightestBoundingColors = {
                        { bytes[neighborIndex1], bytes[neighborIndex1 + 1], bytes[neighborIndex1 + 2], bytes[neighborIndex1 + 3] },
                        { bytes[neighborIndex2], bytes[neighborIndex2 + 1], bytes[neighborIndex2 + 2], bytes[neighborIndex2 + 3] }
                    };
                }
            }
        }

        return tightestBoundingColors;
    };

    std::pair<std::array<int, 4>, std::array<int, 4>> tightestBoundingColors {
        { 0, 0, 0, 0 },
        { 255, 255, 255, 255 }
    };
    std::array<int, 4> tightestBoundingDiff { 255, 255, 255, 255 };

    if (isInTopLeftCorner || isInBottomLeftCorner || isInTopRightCorner || isInBotomRightCorner)
        return tightestBoundingColors;

    if (isInTopRow || isInBottomRow)
        return compareColorsAndSetBounds(bytes, tightestBoundingColors, tightestBoundingDiff, index, leftIndex, rightIndex);

    if (isInLeftColumn || isInRightColumn)
        return compareColorsAndSetBounds(bytes, tightestBoundingColors, tightestBoundingDiff, index, aboveIndex, belowIndex);

    compareColorsAndSetBounds(bytes, tightestBoundingColors, tightestBoundingDiff, index, leftIndex, rightIndex);
    compareColorsAndSetBounds(bytes, tightestBoundingColors, tightestBoundingDiff, index, aboveIndex, belowIndex);
    compareColorsAndSetBounds(bytes, tightestBoundingColors, tightestBoundingDiff, index, aboveLeftIndex, belowRightIndex);
    compareColorsAndSetBounds(bytes, tightestBoundingColors, tightestBoundingDiff, index, aboveRightIndex, belowLeftIndex);

    return tightestBoundingColors;
}

void CanvasNoiseInjection::postProcessDirtyCanvasBuffer(ImageBuffer* imageBuffer, NoiseInjectionHashSalt salt, CanvasNoiseInjectionPostProcessArea postProcessArea)
{

    if (m_postProcessDirtyRect.isEmpty() && postProcessArea == CanvasNoiseInjectionPostProcessArea::DirtyRect)
        return;

    if (!imageBuffer)
        return;

    auto dirtyRect = postProcessArea == CanvasNoiseInjectionPostProcessArea::DirtyRect ? m_postProcessDirtyRect : IntRect(IntPoint::zero(), imageBuffer->truncatedLogicalSize());

    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, imageBuffer->colorSpace() };
    auto pixelBuffer = imageBuffer->getPixelBuffer(format, dirtyRect);
    if (!is<ByteArrayPixelBuffer>(pixelBuffer))
        return;

    if (postProcessPixelBufferResults(*pixelBuffer, salt)) {
        imageBuffer->putPixelBuffer(*pixelBuffer, { IntPoint::zero(), dirtyRect.size() }, dirtyRect.location());
        m_postProcessDirtyRect = { };
    }
}

static std::pair<int, int> lowerAndUpperBound(int component1, int component2, int component3)
{
    if (component1 <= component3) {
        if (component1 == component2 || component2 == component3)
            return { component2, component2 };
        if (component1 <= component2 && component2 <= component3)
            return { component1, component3 };
        if (component1 >= component2 && component2 <= component3)
            return { component2, component1 };
        if (component1 <= component2 && component2 >= component3)
            return { component3, component2 };
    } else if (component1 > component3) {
        if (component1 < component2 && component2 > component3)
            return { component3, component1 };
        if (component1 > component2 && component2 < component3)
            return { component2, component3 };
        if (component1 < component2 && component2 > component3)
            return { component1, component2 };
    }
    return { component2, component2 };
}

static void adjustNeighborColorBounds(std::array<int, 4>& neighborColor1, const std::array<int, 4>& color, std::array<int, 4>& neighborColor2)
{
    auto [redLowerBound, redUpperBound] = lowerAndUpperBound(neighborColor1[0], color[0], neighborColor2[0]);
    auto [greenLowerBound, greenUpperBound] = lowerAndUpperBound(neighborColor1[1], color[1], neighborColor2[1]);
    auto [blueLowerBound, blueUpperBound] = lowerAndUpperBound(neighborColor1[2], color[2], neighborColor2[2]);
    auto [alphaLowerBound, alphaUpperBound] = lowerAndUpperBound(neighborColor1[3], color[3], neighborColor2[3]);

    neighborColor1[0] = redLowerBound;
    neighborColor2[0] = redUpperBound;
    neighborColor1[1] = greenLowerBound;
    neighborColor2[1] = greenUpperBound;
    neighborColor1[2] = blueLowerBound;
    neighborColor2[2] = blueUpperBound;
    neighborColor1[3] = alphaLowerBound;
    neighborColor2[3] = alphaUpperBound;
}

bool CanvasNoiseInjection::postProcessPixelBufferResults(PixelBuffer& pixelBuffer, NoiseInjectionHashSalt salt) const
{
    ASSERT(pixelBuffer.format().pixelFormat == PixelFormat::RGBA8);

    constexpr int bytesPerPixel = 4;
    std::span<uint8_t> bytes = std::span(pixelBuffer.bytes(), pixelBuffer.sizeInBytes());
    bool wasPixelBufferModified { false };

    // Salt value 0 is used for testing.
    if (!salt)
        return true;

    for (size_t i = 0; i < bytes.size_bytes(); i += bytesPerPixel) {
        auto& redChannel = bytes[i];
        auto& greenChannel = bytes[i + 1];
        auto& blueChannel = bytes[i + 2];
        auto& alphaChannel = bytes[i + 3];
        bool isBlack { !redChannel && !greenChannel && !blueChannel };

        if (!alphaChannel)
            continue;

        const auto clampedColorComponentOffset = [](int colorComponentOffset, int originalComponentValue, int minValue, int maxValue) {
            if (originalComponentValue > maxValue)
                maxValue = 255;
            if (originalComponentValue < minValue)
                minValue = 0;
            if (colorComponentOffset + originalComponentValue > maxValue)
                colorComponentOffset = maxValue - originalComponentValue;
            else if (colorComponentOffset + originalComponentValue < minValue)
                colorComponentOffset = minValue - originalComponentValue;
            return colorComponentOffset;
        };

        std::array<int, 4> lowerBoundColor { 0, 0, 0, 0 };
        std::array<int, 4> upperBoundColor { 255, 255, 255, 255 };
        auto [neighborColor1, neighborColor2] = boundingNeighbors(i, bytes, pixelBuffer.size());

        // +/- 1 is roughly ~0.5% of the 255 max value.
        // +/- 3 is roughly ~1% of the 255 max value.
        int maxNoiseValue = 3;
        if (neighborColor1 != neighborColor2 && neighborColor1 != lowerBoundColor && neighborColor2 != upperBoundColor) {
            maxNoiseValue = 1;
            adjustNeighborColorBounds(neighborColor1, { redChannel, greenChannel, blueChannel, alphaChannel }, neighborColor2);
        }

        lowerBoundColor = neighborColor1;
        upperBoundColor = neighborColor2;

        const uint64_t pixelHash = computeHash(salt, redChannel, greenChannel, blueChannel, alphaChannel);
        auto noiseValue = static_cast<int8_t>(((pixelHash * 2 * maxNoiseValue) / std::numeric_limits<uint32_t>::max()) - maxNoiseValue);

        // If alpha is non-zero and the color channels are zero, then only tweak the alpha channel's value;
        if (isBlack)
            alphaChannel += clampedColorComponentOffset(noiseValue, alphaChannel, lowerBoundColor[3], upperBoundColor[3]);
        else {
            // If alpha and any of the color channels are non-zero, then tweak all of the channels;
            redChannel += clampedColorComponentOffset(noiseValue, redChannel, lowerBoundColor[0], upperBoundColor[0]);
            greenChannel += clampedColorComponentOffset(noiseValue, greenChannel, lowerBoundColor[1], upperBoundColor[1]);
            blueChannel += clampedColorComponentOffset(noiseValue, blueChannel, lowerBoundColor[2], upperBoundColor[2]);
            alphaChannel += clampedColorComponentOffset(noiseValue, alphaChannel, lowerBoundColor[3], upperBoundColor[3]);
        }
        wasPixelBufferModified = true;
    }
    return wasPixelBufferModified;
}

} // namespace WebCore
