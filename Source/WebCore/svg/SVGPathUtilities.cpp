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
#include "SVGPathAbsoluteConverter.h"
#include "SVGPathBlender.h"
#include "SVGPathBuilder.h"
#include "SVGPathByteStreamBuilder.h"
#include "SVGPathByteStreamSource.h"
#include "SVGPathConsumer.h"
#include "SVGPathElement.h"
#include "SVGPathParser.h"
#include "SVGPathStringBuilder.h"
#include "SVGPathStringViewSource.h"
#include "SVGPathTraversalStateBuilder.h"

namespace WebCore {

Path buildPathFromString(StringView d)
{
    if (d.isEmpty())
        return { };

    Path path;
    SVGPathBuilder builder(path);
    SVGPathStringViewSource source(d);
    SVGPathParser::parse(source, builder);
    return path;
}

String buildStringFromPath(const Path& path)
{
    StringBuilder builder;

    if (!path.isEmpty()) {
        path.applyElements([&builder] (const PathElement& element) {
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

Path buildPathFromByteStream(const SVGPathByteStream& stream)
{
    if (stream.isEmpty())
        return { };

    if (auto path = stream.cachedPath())
        return path.value();

    Path path;
    SVGPathBuilder builder(path);
    SVGPathByteStreamSource source(stream);
    SVGPathParser::parse(source, builder);
    stream.cachePath(path);
    return path;
}

bool buildStringFromByteStream(const SVGPathByteStream& stream, String& result, PathParsingMode parsingMode, bool checkForInitialMoveTo)
{
    if (stream.isEmpty())
        return true;

    SVGPathByteStreamSource source(stream);
    return SVGPathParser::parseToString(source, result, parsingMode, checkForInitialMoveTo);
}

bool buildSVGPathByteStreamFromString(StringView d, SVGPathByteStream& result, PathParsingMode parsingMode)
{
    result.clear();
    if (d.isEmpty())
        return true;

    SVGPathStringViewSource source(d);
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

    SVGPathByteStream fromStreamCopy = WTF::move(streamToAppendTo);

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

namespace {

// Records the running length at the end of every source segment. The parser calls
// incrementPathSegmentCount() once per source command, after that command's
// callbacks and before the next one's, so one arc contributes one entry even
// though it decomposes into several cubics. That is what makes these boundaries
// line up with the segments getPathData() returns.
//
// PathTraversalState only understands the normalized command set, so this runs in
// NormalizedParsing, exactly as the legacy segment-index query does.
class SVGPathSegmentEndLengthBuilder final : public SVGPathConsumer {
public:
    explicit SVGPathSegmentEndLengthBuilder(PathTraversalState& state)
        : m_traversalState(state)
    {
    }

    // One entry per segment except the last, whose end is the total length.
    const Vector<float>& segmentEndLengths() const LIFETIME_BOUND { return m_segmentEndLengths; }

private:
    void incrementPathSegmentCount() final { m_segmentEndLengths.append(m_traversalState.totalLength()); }
    bool continueConsuming() final { return true; }

    void moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode) final
    {
        m_traversalState.processPathElement(PathElement::Type::MoveToPoint, singleElementSpan(targetPoint));
    }

    void lineTo(const FloatPoint& targetPoint, PathCoordinateMode) final
    {
        m_traversalState.processPathElement(PathElement::Type::AddLineToPoint, singleElementSpan(targetPoint));
    }

    void curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode) final
    {
        std::array points { point1, point2, targetPoint };
        m_traversalState.processPathElement(PathElement::Type::AddCurveToPoint, std::span<FloatPoint> { points });
    }

    void closePath() final
    {
        m_traversalState.processPathElement(PathElement::Type::CloseSubpath, { });
    }

    // Not reachable in NormalizedParsing.
    void lineToHorizontal(float, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void lineToVertical(float, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToCubicSmooth(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToQuadratic(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToQuadraticSmooth(const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void arcTo(float, float, float, bool, bool, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }

    PathTraversalState& m_traversalState;
    Vector<float> m_segmentEndLengths;
};

} // namespace

std::optional<unsigned> getSVGPathSegmentAtLengthFromSVGPathByteStream(const SVGPathByteStream& stream, float distance)
{
    if (stream.isEmpty())
        return std::nullopt;

    PathTraversalState traversalState(PathTraversalState::Action::TotalLength);
    SVGPathSegmentEndLengthBuilder builder(traversalState);

    SVGPathByteStreamSource source(stream);
    SVGPathParser::parse(source, builder);

    // The first segment whose end lies strictly beyond the distance. Strictly, so a
    // distance sitting exactly on a boundary belongs to the later segment.
    auto& segmentEndLengths = builder.segmentEndLengths();
    for (unsigned index = 0; index < segmentEndLengths.size(); ++index) {
        if (segmentEndLengths[index] > distance)
            return index;
    }

    // Past every recorded boundary, so it is in the last segment. There is always
    // one more segment than there are recorded ends.
    return segmentEndLengths.size();
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

std::optional<SVGPathByteStream> convertSVGPathByteStreamToAbsoluteCoordinates(const SVGPathByteStream& stream)
{
    SVGPathByteStream result;
    if (stream.isEmpty())
        return result;

    SVGPathByteStreamBuilder builder(result);
    SVGPathAbsoluteConverter converter(builder);

    SVGPathByteStreamSource source(stream);

    if (!SVGPathParser::parse(source, converter, UnalteredParsing, false))
        return std::nullopt;

    return result;
}

}
