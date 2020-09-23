/*
 * Copyright (C) Research In Motion Limited 2010, 2011. All rights reserved.
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
#include "SVGPathBlender.h"

#include "AnimationUtilities.h"
#include "SVGPathSeg.h"
#include "SVGPathSource.h"
#include <functional>
#include <wtf/SetForScope.h>

namespace WebCore {

bool SVGPathBlender::addAnimatedPath(SVGPathSource& fromSource, SVGPathSource& toSource, SVGPathConsumer& consumer, unsigned repeatCount)
{
    SVGPathBlender blender(fromSource, toSource, &consumer);
    return blender.addAnimatedPath(repeatCount);
}

bool SVGPathBlender::blendAnimatedPath(SVGPathSource& fromSource, SVGPathSource& toSource, SVGPathConsumer& consumer, float progress)
{
    SVGPathBlender blender(fromSource, toSource, &consumer);
    return blender.blendAnimatedPath(progress);
}

bool SVGPathBlender::canBlendPaths(SVGPathSource& fromSource, SVGPathSource& toSource)
{
    SVGPathBlender blender(fromSource, toSource);
    return blender.canBlendPaths();
}

SVGPathBlender::SVGPathBlender(SVGPathSource& fromSource, SVGPathSource& toSource, SVGPathConsumer* consumer)
    : m_fromSource(fromSource)
    , m_toSource(toSource)
    , m_consumer(consumer)
{
}

// Helper functions
static inline FloatPoint blendFloatPoint(const FloatPoint& a, const FloatPoint& b, float progress)
{
    return FloatPoint(blend(a.x(), b.x(), progress), blend(a.y(), b.y(), progress));
}

float SVGPathBlender::blendAnimatedDimensonalFloat(float from, float to, FloatBlendMode blendMode, float progress)
{
    if (m_addTypesCount) {
        ASSERT(m_fromMode == m_toMode);
        return from + to * m_addTypesCount;
    }

    if (m_fromMode == m_toMode)
        return blend(from, to, progress);
    
    float fromValue = blendMode == BlendHorizontal ? m_fromCurrentPoint.x() : m_fromCurrentPoint.y();
    float toValue = blendMode == BlendHorizontal ? m_toCurrentPoint.x() : m_toCurrentPoint.y();

    // Transform toY to the coordinate mode of fromY
    float animValue = blend(from, m_fromMode == AbsoluteCoordinates ? to + toValue : to - toValue, progress);
    
    if (m_isInFirstHalfOfAnimation)
        return animValue;
    
    // Transform the animated point to the coordinate mode, needed for the current progress.
    float currentValue = blend(fromValue, toValue, progress);
    return m_toMode == AbsoluteCoordinates ? animValue + currentValue : animValue - currentValue;
}

FloatPoint SVGPathBlender::blendAnimatedFloatPoint(const FloatPoint& fromPoint, const FloatPoint& toPoint, float progress)
{
    if (m_addTypesCount) {
        ASSERT(m_fromMode == m_toMode);
        FloatPoint repeatedToPoint = toPoint;
        repeatedToPoint.scale(m_addTypesCount);
        return fromPoint + repeatedToPoint;
    }

    if (m_fromMode == m_toMode)
        return blendFloatPoint(fromPoint, toPoint, progress);

    // Transform toPoint to the coordinate mode of fromPoint
    FloatPoint animatedPoint = toPoint;
    if (m_fromMode == AbsoluteCoordinates)
        animatedPoint += m_toCurrentPoint;
    else
        animatedPoint.move(-m_toCurrentPoint.x(), -m_toCurrentPoint.y());

    animatedPoint = blendFloatPoint(fromPoint, animatedPoint, progress);

    if (m_isInFirstHalfOfAnimation)
        return animatedPoint;

    // Transform the animated point to the coordinate mode, needed for the current progress.
    FloatPoint currentPoint = blendFloatPoint(m_fromCurrentPoint, m_toCurrentPoint, progress);
    if (m_toMode == AbsoluteCoordinates)
        return animatedPoint + currentPoint;

    animatedPoint.move(-currentPoint.x(), -currentPoint.y());
    return animatedPoint;
}

template<typename Function> using InvokeResult = typename std::invoke_result_t<Function, SVGPathSource>::value_type;
template<typename Function> using ResultPair = std::pair<InvokeResult<Function>, InvokeResult<Function>>;
template<typename Function> static Optional<ResultPair<Function>> pullFromSources(SVGPathSource& fromSource, SVGPathSource& toSource, Function&& function)
{
    InvokeResult<Function> fromResult;
    if (fromSource.hasMoreData()) {
        auto parsedFrom = std::invoke(function, fromSource);
        if (!parsedFrom)
            return WTF::nullopt;
        fromResult = WTFMove(*parsedFrom);
    }

    auto parsedTo = std::invoke(std::forward<Function>(function), toSource);
    if (!parsedTo)
        return WTF::nullopt;

    return ResultPair<Function> { WTFMove(fromResult), WTFMove(*parsedTo) };
}

bool SVGPathBlender::blendMoveToSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseMoveToSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    m_consumer->moveTo(blendAnimatedFloatPoint(from.targetPoint, to.targetPoint, progress), false, m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? from.targetPoint : m_fromCurrentPoint + from.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? to.targetPoint : m_toCurrentPoint + to.targetPoint;
    return true;
}

bool SVGPathBlender::blendLineToSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseLineToSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    m_consumer->lineTo(blendAnimatedFloatPoint(from.targetPoint, to.targetPoint, progress), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? from.targetPoint : m_fromCurrentPoint + from.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? to.targetPoint : m_toCurrentPoint + to.targetPoint;
    return true;
}

bool SVGPathBlender::blendLineToHorizontalSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseLineToHorizontalSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    m_consumer->lineToHorizontal(blendAnimatedDimensonalFloat(from.x, to.x, BlendHorizontal, progress), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint.setX(m_fromMode == AbsoluteCoordinates ? from.x : m_fromCurrentPoint.x() + from.x);
    m_toCurrentPoint.setX(m_toMode == AbsoluteCoordinates ? to.x : m_toCurrentPoint.x() + to.x);
    return true;
}

bool SVGPathBlender::blendLineToVerticalSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseLineToVerticalSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    m_consumer->lineToVertical(blendAnimatedDimensonalFloat(from.y, to.y, BlendVertical, progress), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint.setY(m_fromMode == AbsoluteCoordinates ? from.y : m_fromCurrentPoint.y() + from.y);
    m_toCurrentPoint.setY(m_toMode == AbsoluteCoordinates ? to.y : m_toCurrentPoint.y() + to.y);
    return true;
}

bool SVGPathBlender::blendCurveToCubicSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseCurveToCubicSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    m_consumer->curveToCubic(blendAnimatedFloatPoint(from.point1, to.point1, progress),
        blendAnimatedFloatPoint(from.point2, to.point2, progress),
        blendAnimatedFloatPoint(from.targetPoint, to.targetPoint, progress),
        m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? from.targetPoint : m_fromCurrentPoint + from.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? to.targetPoint : m_toCurrentPoint + to.targetPoint;
    return true;
}

bool SVGPathBlender::blendCurveToCubicSmoothSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseCurveToCubicSmoothSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    m_consumer->curveToCubicSmooth(blendAnimatedFloatPoint(from.point2, to.point2, progress),
        blendAnimatedFloatPoint(from.targetPoint, to.targetPoint, progress),
        m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? from.targetPoint : m_fromCurrentPoint + from.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? to.targetPoint : m_toCurrentPoint + to.targetPoint;
    return true;
}

bool SVGPathBlender::blendCurveToQuadraticSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseCurveToQuadraticSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    m_consumer->curveToQuadratic(blendAnimatedFloatPoint(from.point1, to.point1, progress),
        blendAnimatedFloatPoint(from.targetPoint, to.targetPoint, progress),
        m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? from.targetPoint : m_fromCurrentPoint + from.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? to.targetPoint : m_toCurrentPoint + to.targetPoint;
    return true;
}

bool SVGPathBlender::blendCurveToQuadraticSmoothSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseCurveToQuadraticSmoothSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    m_consumer->curveToQuadraticSmooth(blendAnimatedFloatPoint(from.targetPoint, to.targetPoint, progress), m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? from.targetPoint : m_fromCurrentPoint + from.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? to.targetPoint : m_toCurrentPoint + to.targetPoint;
    return true;
}

bool SVGPathBlender::blendArcToSegment(float progress)
{
    auto result = pullFromSources(m_fromSource, m_toSource, &SVGPathSource::parseArcToSegment);
    if (!result)
        return false;

    if (!m_consumer)
        return true;

    auto [from, to] = *result;

    if (m_addTypesCount) {
        ASSERT(m_fromMode == m_toMode);
        auto scaledToTargetPoint = to.targetPoint;
        scaledToTargetPoint.scale(m_addTypesCount);
        m_consumer->arcTo(from.rx + to.rx * m_addTypesCount,
            from.ry + to.ry * m_addTypesCount,
            from.angle + to.angle * m_addTypesCount,
            from.largeArc || to.largeArc,
            from.sweep || to.sweep,
            from.targetPoint + scaledToTargetPoint,
            m_fromMode);
    } else {
        m_consumer->arcTo(blend(from.rx, to.rx, progress),
            blend(from.ry, to.ry, progress),
            blend(from.angle, to.angle, progress),
            m_isInFirstHalfOfAnimation ? from.largeArc : to.largeArc,
            m_isInFirstHalfOfAnimation ? from.sweep : to.sweep,
            blendAnimatedFloatPoint(from.targetPoint, to.targetPoint, progress),
            m_isInFirstHalfOfAnimation ? m_fromMode : m_toMode);
    }
    m_fromCurrentPoint = m_fromMode == AbsoluteCoordinates ? from.targetPoint : m_fromCurrentPoint + from.targetPoint;
    m_toCurrentPoint = m_toMode == AbsoluteCoordinates ? to.targetPoint : m_toCurrentPoint + to.targetPoint;
    return true;
}

static inline PathCoordinateMode coordinateModeOfCommand(const SVGPathSegType& type)
{
    if (type < PathSegMoveToAbs)
        return AbsoluteCoordinates;

    // Odd number = relative command
    if (type % 2)
        return RelativeCoordinates;

    return AbsoluteCoordinates;
}

static inline bool isSegmentEqual(const SVGPathSegType& fromType, const SVGPathSegType& toType, const PathCoordinateMode& fromMode, const PathCoordinateMode& toMode)
{
    if (fromType == toType && (fromType == PathSegUnknown || fromType == PathSegClosePath))
        return true;

    unsigned short from = fromType;
    unsigned short to = toType;
    if (fromMode == toMode)
        return from == to;
    if (fromMode == AbsoluteCoordinates)
        return from == to - 1;
    return to == from - 1;
}

bool SVGPathBlender::addAnimatedPath(unsigned repeatCount)
{
    SetForScope<unsigned> change(m_addTypesCount, repeatCount);
    return blendAnimatedPath(0);
}

bool SVGPathBlender::canBlendPaths()
{
    float progress = 0.5;
    bool fromSourceHadData = m_fromSource.hasMoreData();
    while (m_toSource.hasMoreData()) {
        SVGPathSegType fromCommand;
        if (fromSourceHadData) {
            auto parsedFromCommand = m_fromSource.parseSVGSegmentType();
            if (!parsedFromCommand)
                return false;
            fromCommand = *parsedFromCommand;
        }
        auto parsedtoCommand = m_toSource.parseSVGSegmentType();
        if (!parsedtoCommand)
            return false;
        SVGPathSegType toCommand = *parsedtoCommand;

        m_toMode = coordinateModeOfCommand(toCommand);
        m_fromMode = fromSourceHadData ? coordinateModeOfCommand(fromCommand) : m_toMode;
        if (m_fromMode != m_toMode && m_addTypesCount)
            return false;

        if (fromSourceHadData && !isSegmentEqual(fromCommand, toCommand, m_fromMode, m_toMode))
            return false;

        switch (toCommand) {
        case PathSegMoveToRel:
        case PathSegMoveToAbs:
            if (!blendMoveToSegment(progress))
                return false;
            break;
        case PathSegLineToRel:
        case PathSegLineToAbs:
            if (!blendLineToSegment(progress))
                return false;
            break;
        case PathSegLineToHorizontalRel:
        case PathSegLineToHorizontalAbs:
            if (!blendLineToHorizontalSegment(progress))
                return false;
            break;
        case PathSegLineToVerticalRel:
        case PathSegLineToVerticalAbs:
            if (!blendLineToVerticalSegment(progress))
                return false;
            break;
        case PathSegClosePath:
            break;
        case PathSegCurveToCubicRel:
        case PathSegCurveToCubicAbs:
            if (!blendCurveToCubicSegment(progress))
                return false;
            break;
        case PathSegCurveToCubicSmoothRel:
        case PathSegCurveToCubicSmoothAbs:
            if (!blendCurveToCubicSmoothSegment(progress))
                return false;
            break;
        case PathSegCurveToQuadraticRel:
        case PathSegCurveToQuadraticAbs:
            if (!blendCurveToQuadraticSegment(progress))
                return false;
            break;
        case PathSegCurveToQuadraticSmoothRel:
        case PathSegCurveToQuadraticSmoothAbs:
            if (!blendCurveToQuadraticSmoothSegment(progress))
                return false;
            break;
        case PathSegArcRel:
        case PathSegArcAbs:
            if (!blendArcToSegment(progress))
                return false;
            break;
        case PathSegUnknown:
            return false;
        }

        if (!fromSourceHadData)
            continue;
        if (m_fromSource.hasMoreData() != m_toSource.hasMoreData())
            return false;
        if (!m_fromSource.hasMoreData() || !m_toSource.hasMoreData())
            return true;
    }

    return true;
}

bool SVGPathBlender::blendAnimatedPath(float progress)
{
    m_isInFirstHalfOfAnimation = progress < 0.5f;

    bool fromSourceHadData = m_fromSource.hasMoreData();
    while (m_toSource.hasMoreData()) {
        SVGPathSegType fromCommand;
        if (fromSourceHadData) {
            auto parsedFromCommand = m_fromSource.parseSVGSegmentType();
            if (!parsedFromCommand)
                return false;
            fromCommand = *parsedFromCommand;
        }
        auto parsedToCommand = m_toSource.parseSVGSegmentType();
        if (!parsedToCommand)
            return false;
        SVGPathSegType toCommand = *parsedToCommand;

        m_toMode = coordinateModeOfCommand(toCommand);
        m_fromMode = fromSourceHadData ? coordinateModeOfCommand(fromCommand) : m_toMode;
        if (m_fromMode != m_toMode && m_addTypesCount)
            return false;

        if (fromSourceHadData && !isSegmentEqual(fromCommand, toCommand, m_fromMode, m_toMode))
            return false;

        switch (toCommand) {
        case PathSegMoveToRel:
        case PathSegMoveToAbs:
            if (!blendMoveToSegment(progress))
                return false;
            break;
        case PathSegLineToRel:
        case PathSegLineToAbs:
            if (!blendLineToSegment(progress))
                return false;
            break;
        case PathSegLineToHorizontalRel:
        case PathSegLineToHorizontalAbs:
            if (!blendLineToHorizontalSegment(progress))
                return false;
            break;
        case PathSegLineToVerticalRel:
        case PathSegLineToVerticalAbs:
            if (!blendLineToVerticalSegment(progress))
                return false;
            break;
        case PathSegClosePath:
            m_consumer->closePath();
            break;
        case PathSegCurveToCubicRel:
        case PathSegCurveToCubicAbs:
            if (!blendCurveToCubicSegment(progress))
                return false;
            break;
        case PathSegCurveToCubicSmoothRel:
        case PathSegCurveToCubicSmoothAbs:
            if (!blendCurveToCubicSmoothSegment(progress))
                return false;
            break;
        case PathSegCurveToQuadraticRel:
        case PathSegCurveToQuadraticAbs:
            if (!blendCurveToQuadraticSegment(progress))
                return false;
            break;
        case PathSegCurveToQuadraticSmoothRel:
        case PathSegCurveToQuadraticSmoothAbs:
            if (!blendCurveToQuadraticSmoothSegment(progress))
                return false;
            break;
        case PathSegArcRel:
        case PathSegArcAbs:
            if (!blendArcToSegment(progress))
                return false;
            break;
        case PathSegUnknown:
            return false;
        }

        if (!fromSourceHadData)
            continue;
        if (m_fromSource.hasMoreData() != m_toSource.hasMoreData())
            return false;
        if (!m_fromSource.hasMoreData() || !m_toSource.hasMoreData())
            return true;
    }

    return true;
}

}
