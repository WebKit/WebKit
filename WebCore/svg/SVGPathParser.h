/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 *               2006       Alexander Kellett <lypanov@kde.org>
 *               2006, 2007 Rob Buis <buis@kde.org>
 * Copyrigth (C) 2007, 2009 Apple, Inc.  All rights reserved.
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
 *
 */

#ifndef SVGPathParser_h
#define SVGPathParser_h

#if ENABLE(SVG)
#include "PlatformString.h"
#include "SVGPathConsumer.h"

namespace WebCore {

class SVGPathParser {
public:
    SVGPathParser(SVGPathConsumer*);
    ~SVGPathParser();

    bool parsePathDataString(const String&, bool normalized = false);

private:
    bool decomposeArcToCubic(float, float, float, FloatPoint&, FloatPoint&, bool largeArcFlag, bool sweepFlag);
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

    SVGPathConsumer* m_consumer;
    PathCoordinateMode m_mode;
    const UChar* m_ptr;
    const UChar* m_end;
    char m_lastCommand;
    bool m_normalized;
    bool m_pathClosed;
    FloatPoint m_controlPoint;
    FloatPoint m_currentPoint;
    FloatPoint m_subPathPoint;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGPathParser_h
