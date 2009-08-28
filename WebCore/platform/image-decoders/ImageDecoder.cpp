/*
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
#include <algorithm>
#endif

namespace WebCore {

#if !PLATFORM(SKIA)

RGBA32Buffer::RGBA32Buffer()
    : m_hasAlpha(false)
    , m_status(FrameEmpty)
    , m_duration(0)
    , m_disposalMethod(DisposeNotSpecified)
{
} 

void RGBA32Buffer::clear()
{
    m_bytes.clear();
    m_status = FrameEmpty;
    // NOTE: Do not reset other members here; clearFrameBufferCache()
    // calls this to free the bitmap data, but other functions like
    // initFrameBuffer() and frameComplete() may still need to read
    // other metadata out of this frame later.
}

void RGBA32Buffer::zeroFill()
{
    m_bytes.fill(0);
    m_hasAlpha = true;
}

void RGBA32Buffer::copyBitmapData(const RGBA32Buffer& other)
{
    if (this == &other)
        return;

    m_bytes = other.m_bytes;
    m_size = other.m_size;
    setHasAlpha(other.m_hasAlpha);
}

bool RGBA32Buffer::setSize(int newWidth, int newHeight)
{
    // NOTE: This has no way to check for allocation failure if the
    // requested size was too big...
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

RGBA32Buffer& RGBA32Buffer::operator=(const RGBA32Buffer& other)
{
    if (this == &other)
        return *this;

    copyBitmapData(other);
    setRect(other.rect());
    setStatus(other.status());
    setDuration(other.duration());
    setDisposalMethod(other.disposalMethod());
    return *this;
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

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)

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
    for (int scaledIndex = 0;;) {
        int index = static_cast<int>(scaledIndex * inflateRate + 0.5);
        if (index < length) {
            scaledValues.append(index);
            ++scaledIndex;
        } else
            break;
    }
}

template <MatchType type> int getScaledValue(const Vector<int>& scaledValues, int valueToMatch, int searchStart)
{
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
    int width = m_size.width();
    int height = m_size.height();
    int numPixels = height * width;
    if (m_maxNumPixels <= 0 || numPixels <= m_maxNumPixels) {
        m_scaled = false;
        return;
    }

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

int ImageDecoder::scaledY(int origY, int searchStart)
{
    return getScaledValue<Exact>(m_scaledRows, origY, searchStart);
}

#endif // ENABLE(IMAGE_DECODER_DOWN_SAMPLING)

}
