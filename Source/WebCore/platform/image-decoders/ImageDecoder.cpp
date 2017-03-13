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
#include "ImageDecoder.h"

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

using namespace std;

namespace WebCore {

namespace {

unsigned copyFromSharedBuffer(char* buffer, unsigned bufferLength, const SharedBuffer& sharedBuffer, unsigned offset)
{
    unsigned bytesExtracted = 0;
    const char* moreData;
    while (unsigned moreDataLength = sharedBuffer.getSomeData(moreData, offset)) {
        unsigned bytesToCopy = min(bufferLength - bytesExtracted, moreDataLength);
        memcpy(buffer + bytesExtracted, moreData, bytesToCopy);
        bytesExtracted += bytesToCopy;
        if (bytesExtracted == bufferLength)
            break;
        offset += bytesToCopy;
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

RefPtr<ImageDecoder> ImageDecoder::create(const SharedBuffer& data, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    static const unsigned lengthOfLongestSignature = 14; // To wit: "RIFF????WEBPVP"
    char contents[lengthOfLongestSignature];
    unsigned length = copyFromSharedBuffer(contents, lengthOfLongestSignature, data, 0);
    if (length < lengthOfLongestSignature)
        return nullptr;

    if (matchesGIFSignature(contents))
        return adoptRef(*new GIFImageDecoder(alphaOption, gammaAndColorProfileOption));

    if (matchesPNGSignature(contents))
        return adoptRef(*new PNGImageDecoder(alphaOption, gammaAndColorProfileOption));

    if (matchesICOSignature(contents) || matchesCURSignature(contents))
        return adoptRef(*new ICOImageDecoder(alphaOption, gammaAndColorProfileOption));

    if (matchesJPEGSignature(contents))
        return adoptRef(*new JPEGImageDecoder(alphaOption, gammaAndColorProfileOption));

#if USE(WEBP)
    if (matchesWebPSignature(contents))
        return adoptRef(*new WEBPImageDecoder(alphaOption, gammaAndColorProfileOption));
#endif

    if (matchesBMPSignature(contents))
        return adoptRef(*new BMPImageDecoder(alphaOption, gammaAndColorProfileOption));

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

bool ImageDecoder::frameIsCompleteAtIndex(size_t index)
{
    ImageFrame* buffer = frameBufferAtIndex(index);
    return buffer && buffer->isComplete();
}

bool ImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    if (m_frameBufferCache.size() <= index)
        return true;
    if (m_frameBufferCache[index].isComplete())
        return m_frameBufferCache[index].hasAlpha();
    return true;
}

unsigned ImageDecoder::frameBytesAtIndex(size_t index) const
{
    if (m_frameBufferCache.size() <= index)
        return 0;
    // FIXME: Use the dimension of the requested frame.
    return (m_size.area() * sizeof(RGBA32)).unsafeGet();
}

float ImageDecoder::frameDurationAtIndex(size_t index)
{
    ImageFrame* buffer = frameBufferAtIndex(index);
    if (!buffer || buffer->isEmpty())
        return 0;
    
    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See <rdar://problem/7689300> and <http://webkit.org/b/36082>
    // for more information.
    const float duration = buffer->duration() / 1000.0f;
    if (duration < 0.011f)
        return 0.100f;
    return duration;
}

NativeImagePtr ImageDecoder::createFrameImageAtIndex(size_t index, SubsamplingLevel, const std::optional<IntSize>&)
{
    // Zero-height images can cause problems for some ports. If we have an empty image dimension, just bail.
    if (size().isEmpty())
        return nullptr;

    ImageFrame* buffer = frameBufferAtIndex(index);
    if (!buffer || buffer->isEmpty() || !buffer->hasBackingStore())
        return nullptr;

    // Return the buffer contents as a native image. For some ports, the data
    // is already in a native container, and this just increments its refcount.
    return buffer->backingStore()->image();
}

void ImageDecoder::prepareScaleDataIfNecessary()
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

int ImageDecoder::upperBoundScaledX(int origX, int searchStart)
{
    return getScaledValue<UpperBound>(m_scaledColumns, origX, searchStart);
}

int ImageDecoder::lowerBoundScaledX(int origX, int searchStart)
{
    return getScaledValue<LowerBound>(m_scaledColumns, origX, searchStart);
}

int ImageDecoder::upperBoundScaledY(int origY, int searchStart)
{
    return getScaledValue<UpperBound>(m_scaledRows, origY, searchStart);
}

int ImageDecoder::lowerBoundScaledY(int origY, int searchStart)
{
    return getScaledValue<LowerBound>(m_scaledRows, origY, searchStart);
}

int ImageDecoder::scaledY(int origY, int searchStart)
{
    return getScaledValue<Exact>(m_scaledRows, origY, searchStart);
}

}
