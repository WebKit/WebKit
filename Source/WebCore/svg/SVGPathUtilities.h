/*
 * Copyright (C) Research In Motion Limited 2010, 2012. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include <wtf/text/WTFString.h>

namespace WebCore {

class FloatPoint;
class Path;
class SVGPathByteStream;
class SVGPathElement;
class SVGPathSeg;
class SVGPathSegListValues;

// String/SVGPathByteStream -> Path
Path buildPathFromString(const String&);
Path buildPathFromByteStream(const SVGPathByteStream&);

// Path -> String
String buildStringFromPath(const Path&);

// SVGPathSegListValues/String -> SVGPathByteStream
bool buildSVGPathByteStreamFromSVGPathSegListValues(const SVGPathSegListValues&, SVGPathByteStream& result, PathParsingMode);
bool appendSVGPathByteStreamFromSVGPathSeg(RefPtr<SVGPathSeg>&&, SVGPathByteStream&, PathParsingMode);
bool buildSVGPathByteStreamFromString(const String&, SVGPathByteStream&, PathParsingMode);

// SVGPathByteStream/SVGPathSegListValues -> String
bool buildStringFromByteStream(const SVGPathByteStream&, String&, PathParsingMode);
bool buildStringFromSVGPathSegListValues(const SVGPathSegListValues&, String&, PathParsingMode);

// SVGPathByteStream -> SVGPathSegListValues
bool buildSVGPathSegListValuesFromByteStream(const SVGPathByteStream&, SVGPathElement&, SVGPathSegListValues&, PathParsingMode);

bool canBlendSVGPathByteStreams(const SVGPathByteStream& from, const SVGPathByteStream& to);

bool buildAnimatedSVGPathByteStream(const SVGPathByteStream& from, const SVGPathByteStream& to, SVGPathByteStream& result, float progress);
bool addToSVGPathByteStream(SVGPathByteStream& streamToAppendTo, const SVGPathByteStream& from, unsigned repeatCount = 1);

bool getSVGPathSegAtLengthFromSVGPathByteStream(const SVGPathByteStream&, float length, unsigned& pathSeg);
bool getTotalLengthOfSVGPathByteStream(const SVGPathByteStream&, float& totalLength);
bool getPointAtLengthOfSVGPathByteStream(const SVGPathByteStream&, float length, FloatPoint&);

} // namespace WebCore
