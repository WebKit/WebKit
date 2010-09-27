/*
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

#include <algorithm>
#include <cmath>

#include "BMPImageDecoder.h"
#include "GIFImageDecoder.h"
#include "ICOImageDecoder.h"
#include "JPEGImageDecoder.h"
#include "PNGImageDecoder.h"
#include "SharedBuffer.h"

using namespace std;

namespace WebCore {

static unsigned copyFromSharedBuffer(char* buffer, unsigned bufferLength, const SharedBuffer& sharedBuffer, unsigned offset)
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

ImageDecoder* ImageDecoder::create(const SharedBuffer& data, bool premultiplyAlpha)
{
    // We need at least 4 bytes to figure out what kind of image we're dealing
    // with.
    static const unsigned maxMarkerLength = 4;
    char contents[maxMarkerLength];
    unsigned length = copyFromSharedBuffer(contents, maxMarkerLength, data, 0);
    if (length < maxMarkerLength)
        return 0;

    // GIFs begin with GIF8(7 or 9).
    if (strncmp(contents, "GIF8", 4) == 0)
        return new GIFImageDecoder(premultiplyAlpha);

    // Test for PNG.
    if (!memcmp(contents, "\x89\x50\x4E\x47", 4))
        return new PNGImageDecoder(premultiplyAlpha);

    // JPEG
    if (!memcmp(contents, "\xFF\xD8\xFF", 3))
        return new JPEGImageDecoder(premultiplyAlpha);

    // BMP
    if (strncmp(contents, "BM", 2) == 0)
        return new BMPImageDecoder(premultiplyAlpha);

    // ICOs always begin with a 2-byte 0 followed by a 2-byte 1.
    // CURs begin with 2-byte 0 followed by 2-byte 2.
    if (!memcmp(contents, "\x00\x00\x01\x00", 4) || !memcmp(contents, "\x00\x00\x02\x00", 4))
        return new ICOImageDecoder(premultiplyAlpha);

    // Give up. We don't know what the heck this is.
    return 0;
}

#if !PLATFORM(SKIA)

RGBA32Buffer::RGBA32Buffer()
    : m_hasAlpha(false)
    , m_status(FrameEmpty)
    , m_duration(0)
    , m_disposalMethod(DisposeNotSpecified)
    , m_premultiplyAlpha(true)
{
} 

RGBA32Buffer& RGBA32Buffer::operator=(const RGBA32Buffer& other)
{
    if (this == &other)
        return *this;

    copyBitmapData(other);
    setRect(other.rect());
    setStatus(other.status());
    setDuration(other.duration());
    setDisposalMethod(other.disposalMethod());
    setPremultiplyAlpha(other.premultiplyAlpha());
    return *this;
}

void RGBA32Buffer::clear()
{
    m_bytes.clear();
    m_status = FrameEmpty;
    // NOTE: Do not reset other members here; clearFrameBufferCache() calls this
    // to free the bitmap data, but other functions like initFrameBuffer() and
    // frameComplete() may still need to read other metadata out of this frame
    // later.
}

void RGBA32Buffer::zeroFill()
{
    m_bytes.fill(0);
    m_hasAlpha = true;
}

bool RGBA32Buffer::copyBitmapData(const RGBA32Buffer& other)
{
    if (this == &other)
        return true;

    m_bytes = other.m_bytes;
    m_size = other.m_size;
    setHasAlpha(other.m_hasAlpha);
    return true;
}

bool RGBA32Buffer::setSize(int newWidth, int newHeight)
{
    // NOTE: This has no way to check for allocation failure if the requested
    // size was too big...
    m_bytes.resize(newWidth * newHeight);
    m_size = IntSize(newWidth, newHeight);

    // Zero the image.
    zeroFill();

    return true;
}

bool RGBA32Buffer::hasAlpha() const
{
    return m_hasAlpha;
}

void RGBA32Buffer::setHasAlpha(bool alpha)
{
    m_hasAlpha = alpha;
}

void RGBA32Buffer::setStatus(FrameStatus status)
{
    m_status = status;
}

int RGBA32Buffer::width() const
{
    return m_size.width();
}

int RGBA32Buffer::height() const
{
    return m_size.height();
}

#endif

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
