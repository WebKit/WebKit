/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ScalableImageDecoder.h"

#include "BMPImageDecoder.h"
#include "GIFImageDecoder.h"
#include "ICOImageDecoder.h"
#include "JPEGImageDecoder.h"
#include "PNGImageDecoder.h"
#include "SharedBuffer.h"
#if USE(WEBP)
#include "WEBPImageDecoder.h"
#endif

#include <algorithm>
#include <cmath>


namespace WebCore {
using namespace std;

namespace {

static unsigned copyFromSharedBuffer(char* buffer, unsigned bufferLength, const SharedBuffer& sharedBuffer)
{
    unsigned bytesExtracted = 0;
    for (const auto& element : sharedBuffer) {
        if (bytesExtracted + element.segment->size() <= bufferLength) {
            memcpy(buffer + bytesExtracted, element.segment->data(), element.segment->size());
            bytesExtracted += element.segment->size();
        } else {
            ASSERT(bufferLength - bytesExtracted < element.segment->size());
            memcpy(buffer + bytesExtracted, element.segment->data(), bufferLength - bytesExtracted);
            bytesExtracted = bufferLength;
            break;
        }
    }
    return bytesExtracted;
}

bool matchesGIFSignature(char* contents)
{
    return !memcmp(contents, "GIF87a", 6) || !memcmp(contents, "GIF89a", 6);
}

bool matchesPNGSignature(char* contents)
{
    return !memcmp(contents, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8);
}

bool matchesJPEGSignature(char* contents)
{
    return !memcmp(contents, "\xFF\xD8\xFF", 3);
}

#if USE(WEBP)
bool matchesWebPSignature(char* contents)
{
    return !memcmp(contents, "RIFF", 4) && !memcmp(contents + 8, "WEBPVP", 6);
}
#endif

bool matchesBMPSignature(char* contents)
{
    return !memcmp(contents, "BM", 2);
}

bool matchesICOSignature(char* contents)
{
    return !memcmp(contents, "\x00\x00\x01\x00", 4);
}

bool matchesCURSignature(char* contents)
{
    return !memcmp(contents, "\x00\x00\x02\x00", 4);
}

}

RefPtr<ScalableImageDecoder> ScalableImageDecoder::create(SharedBuffer& data, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    static const unsigned lengthOfLongestSignature = 14; // To wit: "RIFF????WEBPVP"
    char contents[lengthOfLongestSignature];
    unsigned length = copyFromSharedBuffer(contents, lengthOfLongestSignature, data);
    if (length < lengthOfLongestSignature)
        return nullptr;

    if (matchesGIFSignature(contents))
        return GIFImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesPNGSignature(contents))
        return PNGImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesICOSignature(contents) || matchesCURSignature(contents))
        return ICOImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    if (matchesJPEGSignature(contents))
        return JPEGImageDecoder::create(alphaOption, gammaAndColorProfileOption);

#if USE(WEBP)
    if (matchesWebPSignature(contents))
        return WEBPImageDecoder::create(alphaOption, gammaAndColorProfileOption);
#endif

    if (matchesBMPSignature(contents))
        return BMPImageDecoder::create(alphaOption, gammaAndColorProfileOption);

    return nullptr;
}

namespace {

enum MatchType {
    Exact,
    UpperBound,
    LowerBound
};

inline void fillScaledValues(Vector<int>& scaledValues, double scaleRate, int length)
{
    double inflateRate = 1. / scaleRate;
    scaledValues.reserveCapacity(static_cast<int>(length * scaleRate + 0.5));
    for (int scaledIndex = 0; ; ++scaledIndex) {
        int index = static_cast<int>(scaledIndex * inflateRate + 0.5);
        if (index >= length)
            break;
        scaledValues.append(index);
    }
}

template <MatchType type> int getScaledValue(const Vector<int>& scaledValues, int valueToMatch, int searchStart)
{
    if (scaledValues.isEmpty())
        return valueToMatch;

    const int* dataStart = scaledValues.data();
    const int* dataEnd = dataStart + scaledValues.size();
    const int* matched = std::lower_bound(dataStart + searchStart, dataEnd, valueToMatch);
    switch (type) {
    case Exact:
        return matched != dataEnd && *matched == valueToMatch ? matched - dataStart : -1;
    case LowerBound:
        return matched != dataEnd && *matched == valueToMatch ? matched - dataStart : matched - dataStart - 1;
    case UpperBound:
    default:
        return matched != dataEnd ? matched - dataStart : -1;
    }
}

}

bool ScalableImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    LockHolder lockHolder(m_mutex);
    if (index >= m_frameBufferCache.size())
        return false;

    auto& frame = m_frameBufferCache[index];
    return frame.isComplete();
}

bool ScalableImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    LockHolder lockHolder(m_mutex);
    if (m_frameBufferCache.size() <= index)
        return true;

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete())
        return true;
    return frame.hasAlpha();
}

unsigned ScalableImageDecoder::frameBytesAtIndex(size_t index, SubsamplingLevel) const
{
    LockHolder lockHolder(m_mutex);
    if (m_frameBufferCache.size() <= index)
        return 0;
    // FIXME: Use the dimension of the requested frame.
    return (m_size.area() * sizeof(uint32_t)).unsafeGet();
}

Seconds ScalableImageDecoder::frameDurationAtIndex(size_t index) const
{
    LockHolder lockHolder(m_mutex);
    if (index >= m_frameBufferCache.size())
        return 0_s;

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete())
        return 0_s;

    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See <rdar://problem/7689300> and <http://webkit.org/b/36082>
    // for more information.
    auto duration = frame.duration();
    if (duration < 11_ms)
        return 100_ms;
    return duration;
}

NativeImagePtr ScalableImageDecoder::createFrameImageAtIndex(size_t index, SubsamplingLevel, const DecodingOptions&)
{
    LockHolder lockHolder(m_mutex);
    // Zero-height images can cause problems for some ports. If we have an empty image dimension, just bail.
    if (size().isEmpty())
        return nullptr;

    auto* buffer = frameBufferAtIndex(index);
    if (!buffer || buffer->isInvalid() || !buffer->hasBackingStore())
        return nullptr;

    // Return the buffer contents as a native image. For some ports, the data
    // is already in a native container, and this just increments its refcount.
    return buffer->backingStore()->image();
}

void ScalableImageDecoder::prepareScaleDataIfNecessary()
{
    m_scaled = false;
    m_scaledColumns.clear();
    m_scaledRows.clear();

    int width = size().width();
    int height = size().height();
    int numPixels = height * width;
    if (m_maxNumPixels <= 0 || numPixels <= m_maxNumPixels)
        return;

    m_scaled = true;
    double scale = sqrt(m_maxNumPixels / (double)numPixels);
    fillScaledValues(m_scaledColumns, scale, width);
    fillScaledValues(m_scaledRows, scale, height);
}

int ScalableImageDecoder::upperBoundScaledX(int origX, int searchStart)
{
    return getScaledValue<UpperBound>(m_scaledColumns, origX, searchStart);
}

int ScalableImageDecoder::lowerBoundScaledX(int origX, int searchStart)
{
    return getScaledValue<LowerBound>(m_scaledColumns, origX, searchStart);
}

int ScalableImageDecoder::upperBoundScaledY(int origY, int searchStart)
{
    return getScaledValue<UpperBound>(m_scaledRows, origY, searchStart);
}

int ScalableImageDecoder::lowerBoundScaledY(int origY, int searchStart)
{
    return getScaledValue<LowerBound>(m_scaledRows, origY, searchStart);
}

int ScalableImageDecoder::scaledY(int origY, int searchStart)
{
    return getScaledValue<Exact>(m_scaledRows, origY, searchStart);
}

}
