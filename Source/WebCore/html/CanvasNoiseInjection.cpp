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

static inline bool isIndexInBounds(int size, int index)
{
    ASSERT(index <= size);
    return index < size;
}

static inline void setTightnessBounds(const std::span<uint8_t>& bytes, std::array<int, 4>& tightestBoundingDiff, int index1, int index2)
{
    int redDiff = bytes[index1] - bytes[index2];
    int greenDiff = bytes[index1 + 1] - bytes[index2 + 1];
    int blueDiff = bytes[index1 + 2] - bytes[index2 + 2];
    int alphaDiff = bytes[index1 + 3] - bytes[index2 + 3];

    if (redDiff < tightestBoundingDiff[0]
        && greenDiff < tightestBoundingDiff[1]
        && blueDiff < tightestBoundingDiff[2]
        && alphaDiff < tightestBoundingDiff[3]) {
        tightestBoundingDiff[0] = redDiff;
        tightestBoundingDiff[1] = greenDiff;
        tightestBoundingDiff[2] = blueDiff;
        tightestBoundingDiff[3] = alphaDiff;
    }
}

static std::optional<std::pair<int, int>> getGradientNeighbors(int index, const std::span<uint8_t>& bytes, const IntSize& size)
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

    if (isInTopLeftCorner || isInBottomLeftCorner || isInTopRightCorner || isInBotomRightCorner)
        return std::nullopt;

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

    const auto areColorsDescending = [&bytes, bufferSize](int index1, int index2, int index3) {
        if (!isIndexInBounds(bufferSize, index1) || !isIndexInBounds(bufferSize, index1 + 3))
            return false;
        if (!isIndexInBounds(bufferSize, index2) || !isIndexInBounds(bufferSize, index2 + 3))
            return false;
        if (!isIndexInBounds(bufferSize, index3) || !isIndexInBounds(bufferSize, index3 + 3))
            return false;
        return bytes[index1] <= bytes[index2] && bytes[index2] <= bytes[index3]
            && bytes[index1 + 1] <= bytes[index2 + 1] && bytes[index2 + 1] <= bytes[index3 + 1]
            && bytes[index1 + 2] <= bytes[index2 + 2] && bytes[index2 + 2] <= bytes[index3 + 2]
            && bytes[index1 + 3] <= bytes[index2 + 3] && bytes[index2 + 3] <= bytes[index3 + 3];
    };

    const auto compareColorsAndSetBounds = [&areColorsDescending](const auto& bytes, auto& tightestBoundingIndices, auto& tightestBoundingDiff, auto colorIndex, auto neighborIndex1, auto neighborIndex2) {
        if (areColorsDescending(neighborIndex1, colorIndex, neighborIndex2)) {
            tightestBoundingIndices = { neighborIndex1, neighborIndex2 };
            setTightnessBounds(bytes, tightestBoundingDiff, neighborIndex1, neighborIndex2);
        } else if (areColorsDescending(neighborIndex2, colorIndex, neighborIndex1)) {
            tightestBoundingIndices = { neighborIndex2, neighborIndex1 };
            setTightnessBounds(bytes, tightestBoundingDiff, neighborIndex2, neighborIndex1);
        }
    };

    if (isInTopRow || isInBottomRow) {
        if (areColorsDescending(leftIndex, index, rightIndex))
            return { { leftIndex, rightIndex } };
        if (areColorsDescending(rightIndex, index, leftIndex))
            return { { rightIndex, leftIndex } };
        return std::nullopt;
    }

    if (isInLeftColumn || isInRightColumn) {
        if (areColorsDescending(aboveIndex, index, belowIndex))
            return { { aboveIndex, belowIndex } };
        if (areColorsDescending(belowIndex, index, aboveIndex))
            return { { belowIndex, aboveIndex } };
        return std::nullopt;
    }

    std::optional<std::pair<int, int>> tightestBoundingIndices;
    std::array<int, 4> tightestBoundingDiff { 255, 255, 255, 255 };

    compareColorsAndSetBounds(bytes, tightestBoundingIndices, tightestBoundingDiff, index, leftIndex, rightIndex);
    compareColorsAndSetBounds(bytes, tightestBoundingIndices, tightestBoundingDiff, index, aboveIndex, belowIndex);
    compareColorsAndSetBounds(bytes, tightestBoundingIndices, tightestBoundingDiff, index, aboveLeftIndex, belowRightIndex);
    compareColorsAndSetBounds(bytes, tightestBoundingIndices, tightestBoundingDiff, index, aboveRightIndex, belowLeftIndex);

    return tightestBoundingIndices;
}

void CanvasNoiseInjection::postProcessDirtyCanvasBuffer(ImageBuffer* imageBuffer, NoiseInjectionHashSalt salt)
{
    ASSERT(salt);

    if (m_postProcessDirtyRect.isEmpty())
        return;

    if (!imageBuffer)
        return;

    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, imageBuffer->colorSpace() };
    auto pixelBuffer = imageBuffer->getPixelBuffer(format, m_postProcessDirtyRect);
    if (!is<ByteArrayPixelBuffer>(pixelBuffer))
        return;

    if (postProcessPixelBufferResults(*pixelBuffer, salt)) {
        imageBuffer->putPixelBuffer(*pixelBuffer, m_postProcessDirtyRect, m_postProcessDirtyRect.location());
        m_postProcessDirtyRect = { };
    }
}

bool CanvasNoiseInjection::postProcessPixelBufferResults(PixelBuffer& pixelBuffer, NoiseInjectionHashSalt salt) const
{
    ASSERT(salt);
    ASSERT(pixelBuffer.format().pixelFormat == PixelFormat::RGBA8);

    constexpr int bytesPerPixel = 4;
    std::span<uint8_t> bytes = std::span(pixelBuffer.bytes(), pixelBuffer.sizeInBytes());
    bool wasPixelBufferModified { false };

    for (size_t i = 0; i < bytes.size_bytes(); i += bytesPerPixel) {
        auto& redChannel = bytes[i];
        auto& greenChannel = bytes[i + 1];
        auto& blueChannel = bytes[i + 2];
        auto& alphaChannel = bytes[i + 3];
        bool isBlack { !redChannel && !greenChannel && !blueChannel };

        if (!alphaChannel)
            continue;

        const uint64_t pixelHash = computeHash(salt, redChannel, greenChannel, blueChannel, alphaChannel);
        // +/- 3 is roughly ~1% of the 255 max value.
        const auto clampedOnePercent = static_cast<int8_t>(((pixelHash * 6) / std::numeric_limits<uint32_t>::max()) - 3);

        const auto clampedColorComponentOffset = [](int colorComponentOffset, int originalComponentValue, int minBoundingColor, int maxBoundingColor) {
            if (colorComponentOffset + originalComponentValue > maxBoundingColor)
                return maxBoundingColor - originalComponentValue;
            if (colorComponentOffset + originalComponentValue < minBoundingColor)
                return minBoundingColor - originalComponentValue;
            return colorComponentOffset;
        };

        std::array<int, 4> minNeighborColor { 0, 0, 0, 0 };
        std::array<int, 4> maxNeighborColor { 255, 255, 255, 255 };
        if (auto neighbors = getGradientNeighbors(i, bytes, pixelBuffer.size())) {
            auto [minNeighborColorIndex, maxNeighborColorIndex] = *neighbors;
            minNeighborColor[0] = bytes[minNeighborColorIndex];
            minNeighborColor[1] = bytes[minNeighborColorIndex + 1];
            minNeighborColor[2] = bytes[minNeighborColorIndex + 2];
            minNeighborColor[3] = bytes[minNeighborColorIndex + 3];

            maxNeighborColor[0] = bytes[maxNeighborColorIndex];
            maxNeighborColor[1] = bytes[maxNeighborColorIndex + 1];
            maxNeighborColor[2] = bytes[maxNeighborColorIndex + 2];
            maxNeighborColor[3] = bytes[maxNeighborColorIndex + 3];
        }

        // If alpha is non-zero and the color channels are zero, then only tweak the alpha channel's value;
        if (isBlack)
            alphaChannel += clampedColorComponentOffset(clampedOnePercent, alphaChannel, minNeighborColor[3], maxNeighborColor[3]);
        else {
            // If alpha and any of the color channels are non-zero, then tweak all of the channels;
            redChannel += clampedColorComponentOffset(clampedOnePercent, redChannel, minNeighborColor[0], maxNeighborColor[0]);
            greenChannel += clampedColorComponentOffset(clampedOnePercent, greenChannel, minNeighborColor[1], maxNeighborColor[1]);
            blueChannel += clampedColorComponentOffset(clampedOnePercent, blueChannel, minNeighborColor[2], maxNeighborColor[2]);
            alphaChannel += clampedColorComponentOffset(clampedOnePercent, alphaChannel, minNeighborColor[3], maxNeighborColor[3]);
        }
        wasPixelBufferModified = true;
    }
    return wasPixelBufferModified;
}

} // namespace WebCore
