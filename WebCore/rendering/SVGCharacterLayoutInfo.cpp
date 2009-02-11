/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"

#if ENABLE(SVG)
#include "SVGCharacterLayoutInfo.h"

#include "InlineFlowBox.h"
#include "InlineTextBox.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGTextPositioningElement.h"
#include "RenderSVGTextPath.h"

#include <float.h>

namespace WebCore {

// Helper function
static float calculateBaselineShift(RenderObject* item)
{
    const Font& font = item->style()->font();
    const SVGRenderStyle* svgStyle = item->style()->svgStyle();

    float baselineShift = 0.0f;
    if (svgStyle->baselineShift() == BS_LENGTH) {
        CSSPrimitiveValue* primitive = static_cast<CSSPrimitiveValue*>(svgStyle->baselineShiftValue());
        baselineShift = primitive->getFloatValue();

        if (primitive->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE)
            baselineShift = baselineShift / 100.0f * font.pixelSize();
    } else {
        float baselineAscent = font.ascent() + font.descent();

        switch (svgStyle->baselineShift()) {
        case BS_BASELINE:
            break;
        case BS_SUB:
            baselineShift = -baselineAscent / 2.0f;
            break;
        case BS_SUPER:
            baselineShift = baselineAscent / 2.0f;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return baselineShift;
}

SVGCharacterLayoutInfo::SVGCharacterLayoutInfo(Vector<SVGChar>& chars)
    : curx(0.0f)
    , cury(0.0f)
    , angle(0.0f)
    , dx(0.0f)
    , dy(0.0f)
    , shiftx(0.0f)
    , shifty(0.0f)
    , pathExtraAdvance(0.0f)
    , pathTextLength(0.0f)
    , pathChunkLength(0.0f)
    , svgChars(chars)
    , nextDrawnSeperated(false)
    , xStackChanged(false)
    , yStackChanged(false)
    , dxStackChanged(false)
    , dyStackChanged(false)
    , angleStackChanged(false)
    , baselineShiftStackChanged(false)
    , pathLayout(false)
    , currentOffset(0.0f)
    , startOffset(0.0f)
    , layoutPathLength(0.0f)
{
}

bool SVGCharacterLayoutInfo::xValueAvailable() const
{
    return xStack.isEmpty() ? false : xStack.last().position() < xStack.last().size();
}

bool SVGCharacterLayoutInfo::yValueAvailable() const
{
    return yStack.isEmpty() ? false : yStack.last().position() < yStack.last().size();
}

bool SVGCharacterLayoutInfo::dxValueAvailable() const
{
    return dxStack.isEmpty() ? false : dxStack.last().position() < dxStack.last().size();
}

bool SVGCharacterLayoutInfo::dyValueAvailable() const
{
    return dyStack.isEmpty() ? false : dyStack.last().position() < dyStack.last().size();
}

bool SVGCharacterLayoutInfo::angleValueAvailable() const
{
    return angleStack.isEmpty() ? false : angleStack.last().position() < angleStack.last().size();
}

bool SVGCharacterLayoutInfo::baselineShiftValueAvailable() const
{
    return !baselineShiftStack.isEmpty();
}

float SVGCharacterLayoutInfo::xValueNext() const
{
    ASSERT(!xStack.isEmpty());
    return xStack.last().valueAtCurrentPosition();
}

float SVGCharacterLayoutInfo::yValueNext() const
{
    ASSERT(!yStack.isEmpty());
    return yStack.last().valueAtCurrentPosition();
}

float SVGCharacterLayoutInfo::dxValueNext() const
{
    ASSERT(!dxStack.isEmpty());
    return dxStack.last().valueAtCurrentPosition();
}

float SVGCharacterLayoutInfo::dyValueNext() const
{
    ASSERT(!dyStack.isEmpty());
    return dyStack.last().valueAtCurrentPosition();
}

float SVGCharacterLayoutInfo::angleValueNext() const
{
    ASSERT(!angleStack.isEmpty());
    return angleStack.last().valueAtCurrentPosition();
}

float SVGCharacterLayoutInfo::baselineShiftValueNext() const
{
    ASSERT(!baselineShiftStack.isEmpty());
    return baselineShiftStack.last();
}

void SVGCharacterLayoutInfo::processedSingleCharacter()
{
    xStackWalk();
    yStackWalk();
    dxStackWalk();
    dyStackWalk();
    angleStackWalk();
    baselineShiftStackWalk();
}

void SVGCharacterLayoutInfo::processedChunk(float savedShiftX, float savedShiftY)
{
    // baseline-shift doesn't span across ancestors, unlike dx/dy.
    curx += savedShiftX - shiftx;
    cury += savedShiftY - shifty;

    if (inPathLayout()) {
        shiftx = savedShiftX;
        shifty = savedShiftY;
    }

    // rotation also doesn't span
    angle = 0.0f;

    if (xStackChanged) {
        ASSERT(!xStack.isEmpty());
        xStack.removeLast();
        xStackChanged = false;
    }

    if (yStackChanged) {
        ASSERT(!yStack.isEmpty());
        yStack.removeLast();
        yStackChanged = false;
    }

    if (dxStackChanged) {
        ASSERT(!dxStack.isEmpty());
        dxStack.removeLast();
        dxStackChanged = false;
    }

    if (dyStackChanged) {
        ASSERT(!dyStack.isEmpty());
        dyStack.removeLast();
        dyStackChanged = false;
    }

    if (angleStackChanged) {
        ASSERT(!angleStack.isEmpty());
        angleStack.removeLast();
        angleStackChanged = false;
    }

    if (baselineShiftStackChanged) {
        ASSERT(!baselineShiftStack.isEmpty());
        baselineShiftStack.removeLast();
        baselineShiftStackChanged = false;
    }
}

bool SVGCharacterLayoutInfo::nextPathLayoutPointAndAngle(float glyphAdvance, float extraAdvance, float newOffset)
{
    if (layoutPathLength <= 0.0f)
        return false;

    if (newOffset != FLT_MIN)
        currentOffset = startOffset + newOffset;

    // Respect translation along path (extraAdvance is orthogonal to the path)
    currentOffset += extraAdvance;

    float offset = currentOffset + glyphAdvance / 2.0f;
    currentOffset += glyphAdvance + pathExtraAdvance;

    if (offset < 0.0f || offset > layoutPathLength)
        return false;

    bool ok = false; 
    FloatPoint point = layoutPath.pointAtLength(offset, ok);
    ASSERT(ok);

    curx = point.x();
    cury = point.y();

    angle = layoutPath.normalAngleAtLength(offset, ok);
    ASSERT(ok);

    // fprintf(stderr, "t: %f, x: %f, y: %f, angle: %f, glyphAdvance: %f\n", currentOffset, x, y, angle, glyphAdvance);
    return true;
}

bool SVGCharacterLayoutInfo::inPathLayout() const
{
    return pathLayout;
}

void SVGCharacterLayoutInfo::setInPathLayout(bool value)
{
    pathLayout = value;

    pathExtraAdvance = 0.0f;
    pathTextLength = 0.0f;
    pathChunkLength = 0.0f;
}

void SVGCharacterLayoutInfo::addLayoutInformation(InlineFlowBox* flowBox, float textAnchorStartOffset)
{
    bool isInitialLayout = xStack.isEmpty() && yStack.isEmpty() &&
                           dxStack.isEmpty() && dyStack.isEmpty() &&
                           angleStack.isEmpty() && baselineShiftStack.isEmpty() &&
                           curx == 0.0f && cury == 0.0f;

    RenderSVGTextPath* textPath = static_cast<RenderSVGTextPath*>(flowBox->renderer());
    Path path = textPath->layoutPath();

    float baselineShift = calculateBaselineShift(textPath);

    layoutPath = path;
    layoutPathLength = path.length();

    if (layoutPathLength <= 0.0f)
        return;

    startOffset = textPath->startOffset();

    if (textPath->startOffset() >= 0.0f && textPath->startOffset() <= 1.0f)
        startOffset *= layoutPathLength;

    startOffset += textAnchorStartOffset;
    currentOffset = startOffset;

    // Only baseline-shift is handled through the normal layout system
    addStackContent(BaselineShiftStack, baselineShift);

    if (isInitialLayout) {
        xStackChanged = false;
        yStackChanged = false;
        dxStackChanged = false;
        dyStackChanged = false;
        angleStackChanged = false;
        baselineShiftStackChanged = false;
    }
}

void SVGCharacterLayoutInfo::addLayoutInformation(SVGTextPositioningElement* element)
{
    bool isInitialLayout = xStack.isEmpty() && yStack.isEmpty() &&
                           dxStack.isEmpty() && dyStack.isEmpty() &&
                           angleStack.isEmpty() && baselineShiftStack.isEmpty() &&
                           curx == 0.0f && cury == 0.0f;

    float baselineShift = calculateBaselineShift(element->renderer());

    addStackContent(XStack, element->x(), element);
    addStackContent(YStack, element->y(), element);
    addStackContent(DxStack, element->dx(), element);
    addStackContent(DyStack, element->dy(), element);
    addStackContent(AngleStack, element->rotate());
    addStackContent(BaselineShiftStack, baselineShift);

    if (isInitialLayout) {
        xStackChanged = false;
        yStackChanged = false;
        dxStackChanged = false;
        dyStackChanged = false;
        angleStackChanged = false;
        baselineShiftStackChanged = false;
    }
}

void SVGCharacterLayoutInfo::addStackContent(StackType type, SVGNumberList* list)
{
    unsigned length = list->numberOfItems();
    if (!length)
        return;

    PositionedFloatVector newLayoutInfo;

    // TODO: Convert more efficiently!
    ExceptionCode ec = 0;
    for (unsigned i = 0; i < length; ++i) {
        float value = list->getItem(i, ec);
        ASSERT(ec == 0);

        newLayoutInfo.append(value);
    }

    addStackContent(type, newLayoutInfo);
}

void SVGCharacterLayoutInfo::addStackContent(StackType type, SVGLengthList* list, const SVGElement* context)
{
    unsigned length = list->numberOfItems();
    if (!length)
        return;

    PositionedFloatVector newLayoutInfo;

    ExceptionCode ec = 0;
    for (unsigned i = 0; i < length; ++i) {
        float value = list->getItem(i, ec).value(context);
        ASSERT(ec == 0);

        newLayoutInfo.append(value);
    }

    addStackContent(type, newLayoutInfo);
}

void SVGCharacterLayoutInfo::addStackContent(StackType type, const PositionedFloatVector& list)
{
    switch (type) {
    case XStack:
        xStackChanged = true;
        xStack.append(list);
        break;
    case YStack:
        yStackChanged = true;
        yStack.append(list);
        break;
    case DxStack:
        dxStackChanged = true;
        dxStack.append(list);
        break;
    case DyStack:
        dyStackChanged = true;
        dyStack.append(list);
        break;
   case AngleStack:
        angleStackChanged = true;
        angleStack.append(list);
        break; 
   default:
        ASSERT_NOT_REACHED();
    }
}

void SVGCharacterLayoutInfo::addStackContent(StackType type, float value)
{
    if (value == 0.0f)
        return;

    switch (type) {
    case BaselineShiftStack:
        baselineShiftStackChanged = true;
        baselineShiftStack.append(value);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void SVGCharacterLayoutInfo::xStackWalk()
{
    unsigned i = 1;

    while (!xStack.isEmpty()) {
        PositionedFloatVector& cur = xStack.last();
        if (i + cur.position() < cur.size()) {
            cur.advance(i);
            break;
        }

        i += cur.position();
        xStack.removeLast();
        xStackChanged = false;
    }
}

void SVGCharacterLayoutInfo::yStackWalk()
{
    unsigned i = 1;

    while (!yStack.isEmpty()) {
        PositionedFloatVector& cur = yStack.last();
        if (i + cur.position() < cur.size()) {
            cur.advance(i);
            break;
        }

        i += cur.position();
        yStack.removeLast();
        yStackChanged = false;
    }
}

void SVGCharacterLayoutInfo::dxStackWalk()
{
    unsigned i = 1;

    while (!dxStack.isEmpty()) {
        PositionedFloatVector& cur = dxStack.last();
        if (i + cur.position() < cur.size()) {
            cur.advance(i);
            break;
        }

        i += cur.position();
        dxStack.removeLast();
        dxStackChanged = false;
    }
}

void SVGCharacterLayoutInfo::dyStackWalk()
{
    unsigned i = 1;

    while (!dyStack.isEmpty()) {
        PositionedFloatVector& cur = dyStack.last();
        if (i + cur.position() < cur.size()) {
            cur.advance(i);
            break;
        }

        i += cur.position();
        dyStack.removeLast();
        dyStackChanged = false;
    }
}

void SVGCharacterLayoutInfo::angleStackWalk()
{
    unsigned i = 1;

    while (!angleStack.isEmpty()) {
        PositionedFloatVector& cur = angleStack.last();
        if (i + cur.position() < cur.size()) {
            cur.advance(i);
            break;
        }

        i += cur.position();
        angleStack.removeLast();
        angleStackChanged = false;
    }
}

void SVGCharacterLayoutInfo::baselineShiftStackWalk()
{
    if (!baselineShiftStack.isEmpty()) {
        baselineShiftStack.removeLast();
        baselineShiftStackChanged = false;
    }
}

bool SVGChar::isHidden() const
{
    return pathData && pathData->hidden;
}

TransformationMatrix SVGChar::characterTransform() const
{
    TransformationMatrix ctm;

    // Rotate character around angle, and possibly scale.
    ctm.translate(x, y);
    ctm.rotate(angle);

    if (pathData) {
        ctm.scaleNonUniform(pathData->xScale, pathData->yScale);
        ctm.translate(pathData->xShift, pathData->yShift);
        ctm.rotate(pathData->orientationAngle);
    }

    ctm.translate(orientationShiftX - x, orientationShiftY - y);
    return ctm;
}

} // namespace WebCore

#endif // ENABLE(SVG)
