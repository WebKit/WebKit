/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009, 2015 Apple Inc. All rights reserved.
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

#include "SVGPathConsumer.h"
#include "SVGPathSeg.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class SVGPathByteStream;
class SVGPathSource;

class SVGPathParser {
public:
    static bool parse(SVGPathSource&, SVGPathConsumer&, PathParsingMode = NormalizedParsing, bool checkForInitialMoveTo = true);

    static bool parseToByteStream(SVGPathSource&, SVGPathByteStream&, PathParsingMode = NormalizedParsing, bool checkForInitialMoveTo = true);
    static bool parseToString(SVGPathSource&, String& result, PathParsingMode = NormalizedParsing, bool checkForInitialMoveTo = true);

private:
    SVGPathParser(SVGPathConsumer&, SVGPathSource&, PathParsingMode);
    bool parsePathData(bool checkForInitialMoveTo);

    bool decomposeArcToCubic(float angle, float rx, float ry, FloatPoint&, FloatPoint&, bool largeArcFlag, bool sweepFlag);
    void parseClosePathSegment();
    bool parseMoveToSegment();
    bool parseLineToSegment();
    bool parseLineToHorizontalSegment();
    bool parseLineToVerticalSegment();
    bool parseCurveToCubicSegment();
    bool parseCurveToCubicSmoothSegment();
    bool parseCurveToQuadraticSegment();
    bool parseCurveToQuadraticSmoothSegment();
    bool parseArcToSegment();

    SVGPathSource& m_source;
    SVGPathConsumer& m_consumer;
    FloatPoint m_controlPoint;
    FloatPoint m_currentPoint;
    FloatPoint m_subPathPoint;
    PathCoordinateMode m_mode { AbsoluteCoordinates };
    const PathParsingMode m_pathParsingMode { NormalizedParsing };
    SVGPathSegType m_lastCommand { SVGPathSegType::Unknown };
    bool m_closePath { true };
};

} // namespace WebCore
