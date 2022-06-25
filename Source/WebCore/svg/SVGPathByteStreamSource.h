/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#pragma once

#include "FloatPoint.h"
#include "SVGPathByteStream.h"
#include "SVGPathSource.h"

namespace WebCore {

class SVGPathByteStreamSource final : public SVGPathSource {
public:
    explicit SVGPathByteStreamSource(const SVGPathByteStream&);

private:
    bool hasMoreData() const final;
    bool moveToNextToken() final { return true; }
    SVGPathSegType nextCommand(SVGPathSegType) final;

    std::optional<SVGPathSegType> parseSVGSegmentType() final;
    std::optional<MoveToSegment> parseMoveToSegment() final;
    std::optional<LineToSegment> parseLineToSegment() final;
    std::optional<LineToHorizontalSegment> parseLineToHorizontalSegment() final;
    std::optional<LineToVerticalSegment> parseLineToVerticalSegment() final;
    std::optional<CurveToCubicSegment> parseCurveToCubicSegment() final;
    std::optional<CurveToCubicSmoothSegment> parseCurveToCubicSmoothSegment() final;
    std::optional<CurveToQuadraticSegment> parseCurveToQuadraticSegment() final;
    std::optional<CurveToQuadraticSmoothSegment> parseCurveToQuadraticSmoothSegment() final;
    std::optional<ArcToSegment> parseArcToSegment() final;

#if COMPILER(MSVC)
#pragma warning(disable: 4701)
#endif
    template<typename DataType, typename ByteType>
    DataType readType()
    {
        ByteType data;
        size_t typeSize = sizeof(ByteType);

        for (size_t i = 0; i < typeSize; ++i) {
            ASSERT_WITH_SECURITY_IMPLICATION(m_streamCurrent < m_streamEnd);
            data.bytes[i] = *m_streamCurrent;
            ++m_streamCurrent;
        }

        return data.value;
    }

    bool readFlag()
    {
        return readType<bool, BoolByte>();
    }

    float readFloat()
    {
        return readType<float, FloatByte>();
    }

    unsigned short readSVGSegmentType()
    {
        return readType<unsigned short, UnsignedShortByte>();
    }

    FloatPoint readFloatPoint()
    {
        float x = readType<float, FloatByte>();
        float y = readType<float, FloatByte>();
        return FloatPoint(x, y);
    }

    SVGPathByteStream::DataIterator m_streamCurrent;
    SVGPathByteStream::DataIterator m_streamEnd;
};

} // namespace WebCore
