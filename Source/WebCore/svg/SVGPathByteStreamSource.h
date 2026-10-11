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
#include <wtf/StdLibExtras.h>
#include <wtf/text/ParsingUtilities.h>

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

    template<typename DataType>
    std::optional<DataType> readType()
    {
        // The byte stream may come from an untrusted process, so it can be truncated.
        if (m_streamCurrent.size() < sizeof(DataType)) {
            m_streamCurrent = { };
            return std::nullopt;
        }
        DataType data;
        memcpySpan(asMutableByteSpan(data), consumeSpan(m_streamCurrent, sizeof(DataType)));
        return data;
    }

    std::optional<bool> readFlag()
    {
        auto flag = readType<uint8_t>();
        if (!flag || *flag > 1)
            return std::nullopt;
        return !!*flag;
    }

    std::optional<float> readFloat()
    {
        return readType<float>();
    }

    std::optional<SVGPathSegType> readSVGSegmentType()
    {
        static_assert(std::is_same_v<std::underlying_type_t<SVGPathSegType>, uint8_t>);
        auto type = readType<uint8_t>();
        if (!type)
            return std::nullopt;
        return static_cast<SVGPathSegType>(*type);
    }

    std::optional<FloatPoint> readFloatPoint()
    {
        return readType<FloatPoint>();
    }

    std::span<const uint8_t> m_streamCurrent;
};

} // namespace WebCore
