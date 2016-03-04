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

#ifndef SVGPathStringSource_h
#define SVGPathStringSource_h

#include "SVGPathSource.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class SVGPathStringSource : public SVGPathSource {
public:
    explicit SVGPathStringSource(const String&);

private:
    bool hasMoreData() const override;
    bool moveToNextToken() override;
    bool parseSVGSegmentType(SVGPathSegType&) override;
    SVGPathSegType nextCommand(SVGPathSegType previousCommand) override;

    bool parseMoveToSegment(FloatPoint&) override;
    bool parseLineToSegment(FloatPoint&) override;
    bool parseLineToHorizontalSegment(float&) override;
    bool parseLineToVerticalSegment(float&) override;
    bool parseCurveToCubicSegment(FloatPoint&, FloatPoint&, FloatPoint&) override;
    bool parseCurveToCubicSmoothSegment(FloatPoint&, FloatPoint&) override;
    bool parseCurveToQuadraticSegment(FloatPoint&, FloatPoint&) override;
    bool parseCurveToQuadraticSmoothSegment(FloatPoint&) override;
    bool parseArcToSegment(float&, float&, float&, bool&, bool&, FloatPoint&) override;

    String m_string;
    bool m_is8BitSource;

    union {
        const LChar* m_character8;
        const UChar* m_character16;
    } m_current;
    union {
        const LChar* m_character8;
        const UChar* m_character16;
    } m_end;
};

} // namespace WebCore

#endif // SVGPathStringSource_h
