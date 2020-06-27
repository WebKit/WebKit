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

class SVGPathStringSource final : public SVGPathSource {
public:
    explicit SVGPathStringSource(const String&);

private:
    bool hasMoreData() const final;
    bool moveToNextToken() final;
    SVGPathSegType nextCommand(SVGPathSegType previousCommand) final;

    Optional<SVGPathSegType> parseSVGSegmentType() final;
    Optional<MoveToSegment> parseMoveToSegment() final;
    Optional<LineToSegment> parseLineToSegment() final;
    Optional<LineToHorizontalSegment> parseLineToHorizontalSegment() final;
    Optional<LineToVerticalSegment> parseLineToVerticalSegment() final;
    Optional<CurveToCubicSegment> parseCurveToCubicSegment() final;
    Optional<CurveToCubicSmoothSegment> parseCurveToCubicSmoothSegment() final;
    Optional<CurveToQuadraticSegment> parseCurveToQuadraticSegment() final;
    Optional<CurveToQuadraticSmoothSegment> parseCurveToQuadraticSmoothSegment() final;
    Optional<ArcToSegment> parseArcToSegment() final;

    template<typename Function> decltype(auto) parse(Function&&);

    String m_string;
    bool m_is8BitSource;

    union {
        StringParsingBuffer<LChar> m_buffer8;
        StringParsingBuffer<UChar> m_buffer16;
    };
};

} // namespace WebCore
