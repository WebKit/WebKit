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

#include "config.h"
#include "SVGPathUtilities.h"

#include "FloatPoint.h"
#include "Path.h"
#include "PathTraversalState.h"
#include "SVGPathBlender.h"
#include "SVGPathBuilder.h"
#include "SVGPathByteStreamBuilder.h"
#include "SVGPathByteStreamSource.h"
#include "SVGPathConsumer.h"
#include "SVGPathElement.h"
#include "SVGPathParser.h"
#include "SVGPathSegListBuilder.h"
#include "SVGPathSegListSource.h"
#include "SVGPathStringBuilder.h"
#include "SVGPathStringSource.h"
#include "SVGPathTraversalStateBuilder.h"

namespace WebCore {

Path buildPathFromString(const String& d)
{
    if (d.isEmpty())
        return { };

    Path path;
    SVGPathBuilder builder(path);
    SVGPathStringSource source(d);
    SVGPathParser::parse(source, builder);
    return path;
}

String buildStringFromPath(const Path& path)
{
    StringBuilder builder;

    if (!path.isNull() && !path.isEmpty()) {
        path.apply([&builder] (const PathElement& element) {
            switch (element.type) {
            case PathElement::Type::MoveToPoint:
                builder.append('M', element.points[0].x(), ' ', element.points[0].y());
                break;
            case PathElement::Type::AddLineToPoint:
                builder.append('L', element.points[0].x(), ' ', element.points[0].y());
                break;
            case PathElement::Type::AddQuadCurveToPoint:
                builder.append('Q', element.points[0].x(), ' ', element.points[0].y(), ',', element.points[1].x(), ' ', element.points[1].y());
                break;
            case PathElement::Type::AddCurveToPoint:
                builder.append('C', element.points[0].x(), ' ', element.points[0].y(), ',', element.points[1].x(), ' ', element.points[1].y(), ',', element.points[2].x(), ' ', element.points[2].y());
                break;
            case PathElement::Type::CloseSubpath:
                builder.append('Z');
                break;
            }
        });
    }

    return builder.toString();
}

bool buildSVGPathByteStreamFromSVGPathSegList(const SVGPathSegList& list, SVGPathByteStream& stream, PathParsingMode parsingMode, bool checkForInitialMoveTo)
{
    stream.clear();
    if (list.isEmpty())
        return true;

    SVGPathSegListSource source(list);
    return SVGPathParser::parseToByteStream(source, stream, parsingMode, checkForInitialMoveTo);
}

Path buildPathFromByteStream(const SVGPathByteStream& stream)
{
    if (stream.isEmpty())
        return { };

    Path path;
    SVGPathBuilder builder(path);
    SVGPathByteStreamSource source(stream);
    SVGPathParser::parse(source, builder);
    return path;
}

bool buildSVGPathSegListFromByteStream(const SVGPathByteStream& stream, SVGPathSegList& list, PathParsingMode mode)
{
    if (stream.isEmpty())
        return true;

    SVGPathSegListBuilder builder(list);
    SVGPathByteStreamSource source(stream);
    return SVGPathParser::parse(source, builder, mode);
}

bool buildStringFromByteStream(const SVGPathByteStream& stream, String& result, PathParsingMode parsingMode, bool checkForInitialMoveTo)
{
    if (stream.isEmpty())
        return true;

    SVGPathByteStreamSource source(stream);
    return SVGPathParser::parseToString(source, result, parsingMode, checkForInitialMoveTo);
}

bool buildSVGPathByteStreamFromString(const String& d, SVGPathByteStream& result, PathParsingMode parsingMode)
{
    result.clear();
    if (d.isEmpty())
        return true;

    SVGPathStringSource source(d);
    return SVGPathParser::parseToByteStream(source, result, parsingMode);
}

bool canBlendSVGPathByteStreams(const SVGPathByteStream& fromStream, const SVGPathByteStream& toStream)
{
    SVGPathByteStreamSource fromSource(fromStream);
    SVGPathByteStreamSource toSource(toStream);
    return SVGPathBlender::canBlendPaths(fromSource, toSource);
}

bool buildAnimatedSVGPathByteStream(const SVGPathByteStream& fromStream, const SVGPathByteStream& toStream, SVGPathByteStream& result, float progress)
{
    ASSERT(&toStream != &result);
    result.clear();
    if (toStream.isEmpty())
        return true;

    SVGPathByteStreamBuilder builder(result);

    SVGPathByteStreamSource fromSource(fromStream);
    SVGPathByteStreamSource toSource(toStream);
    return SVGPathBlender::blendAnimatedPath(fromSource, toSource, builder, progress);
}

bool addToSVGPathByteStream(SVGPathByteStream& streamToAppendTo, const SVGPathByteStream& byStream, unsigned repeatCount)
{
    // The byStream will be blended with streamToAppendTo. So streamToAppendTo has to have elements.
    if (streamToAppendTo.isEmpty() || byStream.isEmpty())
        return true;

    // builder is the destination of blending fromSource and bySource. The stream of builder
    // (i.e. streamToAppendTo) has to be cleared before calling addAnimatedPath.
    SVGPathByteStreamBuilder builder(streamToAppendTo);

    SVGPathByteStream fromStreamCopy = WTFMove(streamToAppendTo);

    SVGPathByteStreamSource fromSource(fromStreamCopy);
    SVGPathByteStreamSource bySource(byStream);
    return SVGPathBlender::addAnimatedPath(fromSource, bySource, builder, repeatCount);
}

unsigned getSVGPathSegAtLengthFromSVGPathByteStream(const SVGPathByteStream& stream, float length)
{
    if (stream.isEmpty())
        return 0;

    PathTraversalState traversalState(PathTraversalState::Action::SegmentAtLength);
    SVGPathTraversalStateBuilder builder(traversalState, length);

    SVGPathByteStreamSource source(stream);
    SVGPathParser::parse(source, builder);
    return builder.pathSegmentIndex();
}

float getTotalLengthOfSVGPathByteStream(const SVGPathByteStream& stream)
{
    if (stream.isEmpty())
        return 0;

    PathTraversalState traversalState(PathTraversalState::Action::TotalLength);

    SVGPathTraversalStateBuilder builder(traversalState);

    SVGPathByteStreamSource source(stream);
    SVGPathParser::parse(source, builder);
    return builder.totalLength();
}

FloatPoint getPointAtLengthOfSVGPathByteStream(const SVGPathByteStream& stream, float length)
{
    if (stream.isEmpty())
        return { };

    PathTraversalState traversalState(PathTraversalState::Action::VectorAtLength);

    SVGPathTraversalStateBuilder builder(traversalState, length);

    SVGPathByteStreamSource source(stream);
    SVGPathParser::parse(source, builder);
    return builder.currentPoint();
}

}
