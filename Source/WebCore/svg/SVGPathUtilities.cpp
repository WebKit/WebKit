/*
 * Copyright (C) Research In Motion Limited 2010, 2012. All rights reserved.
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

// FIXME: are all these globals warranted? Why not just make them on the stack?
static SVGPathBuilder& globalSVGPathBuilder(Path& result)
{
    static NeverDestroyed<SVGPathBuilder> s_builder;
    s_builder.get().setCurrentPath(&result);
    return s_builder.get();
}

static SVGPathSegListBuilder& globalSVGPathSegListBuilder(SVGPathElement& element, SVGPathSegRole role, SVGPathSegList& result)
{
    static NeverDestroyed<SVGPathSegListBuilder> s_builder;

    s_builder.get().setCurrentSVGPathElement(&element);
    s_builder.get().setCurrentSVGPathSegList(result);
    s_builder.get().setCurrentSVGPathSegRole(role);
    return s_builder.get();
}

static SVGPathByteStreamBuilder& globalSVGPathByteStreamBuilder(SVGPathByteStream& result)
{
    static NeverDestroyed<SVGPathByteStreamBuilder> s_builder;
    s_builder.get().setCurrentByteStream(&result);
    return s_builder.get();
}

static SVGPathStringBuilder& globalSVGPathStringBuilder()
{
    static NeverDestroyed<SVGPathStringBuilder> s_builder;
    return s_builder.get();
}

static SVGPathTraversalStateBuilder& globalSVGPathTraversalStateBuilder(PathTraversalState& traversalState, float length)
{
    static NeverDestroyed<SVGPathTraversalStateBuilder> s_parser;

    s_parser.get().setCurrentTraversalState(&traversalState);
    s_parser.get().setDesiredLength(length);
    return s_parser.get();
}

// FIXME: why bother keeping a singleton around? Is it slow to allocate?
static SVGPathParser& globalSVGPathParser(SVGPathSource& source, SVGPathConsumer& consumer)
{
    static NeverDestroyed<SVGPathParser> s_parser;

    s_parser.get().setCurrentSource(&source);
    s_parser.get().setCurrentConsumer(&consumer);
    return s_parser.get();
}

static SVGPathBlender& globalSVGPathBlender()
{
    static NeverDestroyed<SVGPathBlender> s_blender;
    return s_blender.get();
}

bool buildPathFromString(const String& d, Path& result)
{
    if (d.isEmpty())
        return true;

    SVGPathBuilder& builder = globalSVGPathBuilder(result);

    SVGPathStringSource source(d);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(NormalizedParsing);
    parser.cleanup();
    return ok;
}

bool buildSVGPathByteStreamFromSVGPathSegList(const SVGPathSegList& list, SVGPathByteStream& result, PathParsingMode parsingMode)
{
    result.clear();
    if (list.isEmpty())
        return true;

    SVGPathByteStreamBuilder& builder = globalSVGPathByteStreamBuilder(result);

    SVGPathSegListSource source(list);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(parsingMode);
    parser.cleanup();
    return ok;
}

bool appendSVGPathByteStreamFromSVGPathSeg(RefPtr<SVGPathSeg>&& pathSeg, SVGPathByteStream& result, PathParsingMode parsingMode)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=15412 - Implement normalized path segment lists!
    ASSERT(parsingMode == UnalteredParsing);

    SVGPathSegList appendedItemList(PathSegUnalteredRole);
    appendedItemList.append(WTF::move(pathSeg));

    SVGPathByteStream appendedByteStream;
    SVGPathByteStreamBuilder& builder = globalSVGPathByteStreamBuilder(appendedByteStream);
    SVGPathSegListSource source(appendedItemList);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(parsingMode, false);
    parser.cleanup();

    if (ok)
        result.append(appendedByteStream);

    return ok;
}

bool buildPathFromByteStream(const SVGPathByteStream& stream, Path& result)
{
    if (stream.isEmpty())
        return true;

    SVGPathBuilder& builder = globalSVGPathBuilder(result);

    SVGPathByteStreamSource source(stream);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(NormalizedParsing);
    parser.cleanup();
    return ok;
}

bool buildSVGPathSegListFromByteStream(const SVGPathByteStream& stream, SVGPathElement& element, SVGPathSegList& result, PathParsingMode parsingMode)
{
    if (stream.isEmpty())
        return true;

    SVGPathSegListBuilder& builder = globalSVGPathSegListBuilder(element, parsingMode == NormalizedParsing ? PathSegNormalizedRole : PathSegUnalteredRole, result);

    SVGPathByteStreamSource source(stream);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(parsingMode);
    parser.cleanup();
    return ok;
}

bool buildStringFromByteStream(const SVGPathByteStream& stream, String& result, PathParsingMode parsingMode)
{
    if (stream.isEmpty())
        return true;

    SVGPathStringBuilder& builder = globalSVGPathStringBuilder();

    SVGPathByteStreamSource source(stream);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(parsingMode);
    result = builder.result();
    parser.cleanup();
    return ok;
}

bool buildStringFromSVGPathSegList(const SVGPathSegList& list, String& result, PathParsingMode parsingMode)
{
    result = String();
    if (list.isEmpty())
        return true;

    SVGPathStringBuilder& builder = globalSVGPathStringBuilder();

    SVGPathSegListSource source(list);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(parsingMode);
    result = builder.result();
    parser.cleanup();
    return ok;
}

bool buildSVGPathByteStreamFromString(const String& d, SVGPathByteStream& result, PathParsingMode parsingMode)
{
    result.clear();
    if (d.isEmpty())
        return true;

    SVGPathByteStreamBuilder& builder = globalSVGPathByteStreamBuilder(result);

    SVGPathStringSource source(d);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(parsingMode);
    parser.cleanup();
    return ok;
}

bool buildAnimatedSVGPathByteStream(const SVGPathByteStream& fromStream, const SVGPathByteStream& toStream, SVGPathByteStream& result, float progress)
{
    ASSERT(&toStream != &result);
    result.clear();
    if (toStream.isEmpty())
        return true;

    SVGPathByteStreamBuilder& builder = globalSVGPathByteStreamBuilder(result);

    SVGPathByteStreamSource fromSource(fromStream);
    SVGPathByteStreamSource toSource(toStream);
    SVGPathBlender& blender = globalSVGPathBlender();
    bool ok = blender.blendAnimatedPath(progress, &fromSource, &toSource, &builder);
    blender.cleanup();
    return ok;
}

bool addToSVGPathByteStream(SVGPathByteStream& streamToAppendTo, const SVGPathByteStream& byStream, unsigned repeatCount)
{
    // Why return when streamToAppendTo is empty? Don't we still need to append?
    if (streamToAppendTo.isEmpty() || byStream.isEmpty())
        return true;

    // Is it OK to make the SVGPathByteStreamBuilder from a stream, and then clear that stream?
    SVGPathByteStreamBuilder& builder = globalSVGPathByteStreamBuilder(streamToAppendTo);

    SVGPathByteStream fromStreamCopy = streamToAppendTo;
    streamToAppendTo.clear();

    SVGPathByteStreamSource fromSource(fromStreamCopy);
    SVGPathByteStreamSource bySource(byStream);
    SVGPathBlender& blender = globalSVGPathBlender();
    bool ok = blender.addAnimatedPath(&fromSource, &bySource, &builder, repeatCount);
    blender.cleanup();
    return ok;
}

bool getSVGPathSegAtLengthFromSVGPathByteStream(const SVGPathByteStream& stream, float length, unsigned& pathSeg)
{
    if (stream.isEmpty())
        return false;

    PathTraversalState traversalState(PathTraversalState::Action::SegmentAtLength);
    SVGPathTraversalStateBuilder& builder = globalSVGPathTraversalStateBuilder(traversalState, length);

    SVGPathByteStreamSource source(stream);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(NormalizedParsing);
    pathSeg = builder.pathSegmentIndex();
    parser.cleanup();
    return ok;
}

bool getTotalLengthOfSVGPathByteStream(const SVGPathByteStream& stream, float& totalLength)
{
    if (stream.isEmpty())
        return false;

    PathTraversalState traversalState(PathTraversalState::Action::TotalLength);
    SVGPathTraversalStateBuilder& builder = globalSVGPathTraversalStateBuilder(traversalState, 0);

    SVGPathByteStreamSource source(stream);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(NormalizedParsing);
    totalLength = builder.totalLength();
    parser.cleanup();
    return ok;
}

bool getPointAtLengthOfSVGPathByteStream(const SVGPathByteStream& stream, float length, SVGPoint& point)
{
    if (stream.isEmpty())
        return false;

    PathTraversalState traversalState(PathTraversalState::Action::VectorAtLength);
    SVGPathTraversalStateBuilder& builder = globalSVGPathTraversalStateBuilder(traversalState, length);

    SVGPathByteStreamSource source(stream);
    SVGPathParser& parser = globalSVGPathParser(source, builder);
    bool ok = parser.parsePathDataFromSource(NormalizedParsing);
    point = builder.currentPoint();
    parser.cleanup();
    return ok;
}

static void pathIteratorForBuildingString(SVGPathConsumer& consumer, const PathElement& pathElement)
{
    switch (pathElement.type) {
    case PathElementMoveToPoint:
        consumer.moveTo(pathElement.points[0], false, AbsoluteCoordinates);
        break;
    case PathElementAddLineToPoint:
        consumer.lineTo(pathElement.points[0], AbsoluteCoordinates);
        break;
    case PathElementAddQuadCurveToPoint:
        consumer.curveToQuadratic(pathElement.points[0], pathElement.points[1], AbsoluteCoordinates);
        break;
    case PathElementAddCurveToPoint:
        consumer.curveToCubic(pathElement.points[0], pathElement.points[1], pathElement.points[2], AbsoluteCoordinates);
        break;
    case PathElementCloseSubpath:
        consumer.closePath();
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool buildStringFromPath(const Path& path, String& string)
{
    // Ideally we would have a SVGPathPlatformPathSource, but it's not possible to manually iterate
    // a path, only apply a function to all path elements at once.

    SVGPathStringBuilder& builder = globalSVGPathStringBuilder();
    path.apply([&builder](const PathElement& pathElement) {
        pathIteratorForBuildingString(builder, pathElement);
    });
    string = builder.result();
    static_cast<SVGPathConsumer&>(builder).cleanup(); // Wat?

    return true;
}

}
