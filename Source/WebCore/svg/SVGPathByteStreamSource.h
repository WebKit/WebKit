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
    std::optional<MoveToSegment> parseMoveToSegment(FloatPoint) final;
    std::optional<LineToSegment> parseLineToSegment(FloatPoint) final;
    std::optional<LineToHorizontalSegment> parseLineToHorizontalSegment(FloatPoint) final;
    std::optional<LineToVerticalSegment> parseLineToVerticalSegment(FloatPoint) final;
    std::optional<CurveToCubicSegment> parseCurveToCubicSegment(FloatPoint) final;
    std::optional<CurveToCubicSmoothSegment> parseCurveToCubicSmoothSegment(FloatPoint) final;
    std::optional<CurveToQuadraticSegment> parseCurveToQuadraticSegment(FloatPoint) final;
    std::optional<CurveToQuadraticSmoothSegment> parseCurveToQuadraticSmoothSegment(FloatPoint) final;
    std::optional<ArcToSegment> parseArcToSegment(FloatPoint) final;

#if COMPILER(MSVC)
#pragma warning(disable: 4701)
#endif
    template<typename DataType>
    DataType readType()
    {
        DataType data;
        size_t dataSize = sizeof(DataType);

        ASSERT_WITH_SECURITY_IMPLICATION(m_streamCurrent + dataSize <= m_streamEnd);
        memcpy(&data, m_streamCurrent, dataSize);
        m_streamCurrent += dataSize;
        return data;
    }

    bool readFlag()
    {
        return readType<bool>();
    }

    float readFloat()
    {
        return readType<float>();
    }

    SVGPathSegType readSVGSegmentType()
    {
        static_assert(std::is_same_v<std::underlying_type_t<SVGPathSegType>, uint8_t>);
        return static_cast<SVGPathSegType>(readType<uint8_t>());
    }

    FloatPoint readFloatPoint()
    {
        return readType<FloatPoint>();
    }

    SVGPathByteStream::DataIterator m_streamCurrent;
    SVGPathByteStream::DataIterator m_streamEnd;
};

} // namespace WebCore
