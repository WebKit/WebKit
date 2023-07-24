/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "SVGPathSource.h"
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SVGPathStringViewSource final : public SVGPathSource {
public:
    explicit SVGPathStringViewSource(StringView);

private:
    bool hasMoreData() const final;
    bool moveToNextToken() final;
    SVGPathSegType nextCommand(SVGPathSegType previousCommand) final;

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

    template<typename Function> decltype(auto) parse(Function&&);

    bool m_is8BitSource;
    union {
        StringParsingBuffer<LChar> m_buffer8;
        StringParsingBuffer<UChar> m_buffer16;
    };
};

} // namespace WebCore
