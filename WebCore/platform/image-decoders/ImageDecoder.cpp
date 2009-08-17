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

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)

#include "config.h"
#include "ImageDecoder.h"

#include <algorithm>

namespace WebCore {

namespace {

enum MatchType {
    Exact,
    UpperBound,
    LowerBound
};

}

template <MatchType type> static int getScaledValue(const Vector<int>& scaledValues, int valueToMatch, int searchStart)
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

static inline void fillScaledValues(Vector<int>& scaledValues, double scaleRate, int length)
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

}

#endif // ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
