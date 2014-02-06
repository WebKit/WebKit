/*
 * Copyright (C) 2014 Gurpreet Kaur (k.gurpreet@samsung.com). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _USE_MATH_DEFINES 1
#include "config.h"

#if ENABLE(MATHML)
#include "RenderMathMLMenclose.h"

#include "GraphicsContext.h"
#include "MathMLMencloseElement.h"
#include "PaintInfo.h"
#include "RenderMathMLSquareRoot.h"
#include <wtf/MathExtras.h>

namespace WebCore {

using namespace MathMLNames;

RenderMathMLMenclose::RenderMathMLMenclose(Element& element, PassRef<RenderStyle> style)
    : RenderMathMLRow(element, std::move(style))
{
}

void RenderMathMLMenclose::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    MathMLMencloseElement* menclose = toMathMLMencloseElement(element());
    // Allow an anonymous RenderMathMLSquareRoot to handle drawing the radical
    // notation, rather than duplicating the code needed to paint a root.
    if (!firstChild() && menclose->isRadical())        
        RenderMathMLBlock::addChild(RenderMathMLSquareRoot::createAnonymousWithParentRenderer(*this).leakPtr());
    
    if (newChild) {
        if (firstChild() && menclose->isRadical())
            toRenderElement(firstChild())->addChild(newChild, beforeChild && beforeChild->parent() == firstChild() ? beforeChild : nullptr);
        else
            RenderMathMLBlock::addChild(newChild, beforeChild);
    }
}

void RenderMathMLMenclose::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    RenderMathMLBlock::computePreferredLogicalWidths();
    const int paddingTop = 6;

    MathMLMencloseElement* menclose = toMathMLMencloseElement(element());
    const Vector<String>& notationValues = menclose->notationValues();
    size_t notationalValueSize = notationValues.size();
    for (size_t i = 0; i < notationalValueSize; i++) {
        if (notationValues[i] == "circle") {
            m_minPreferredLogicalWidth = minPreferredLogicalWidth() * float(M_SQRT2);
            m_maxPreferredLogicalWidth = maxPreferredLogicalWidth() * float(M_SQRT2);
        }
    }

    if (menclose->isDefaultLongDiv()) {
        style().setPaddingTop(Length(paddingTop, Fixed));
        style().setPaddingLeft(Length(menclose->longDivLeftPadding().toInt(), Fixed));
    }
    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLMenclose::updateLogicalHeight()
{
    MathMLMencloseElement* menclose = toMathMLMencloseElement(element());
    const Vector<String>& notationValues = menclose->notationValues();
    size_t notationalValueSize = notationValues.size();
    for (size_t i = 0; i < notationalValueSize; i++)
        if (notationValues[i] == "circle")
            setLogicalHeight(logicalHeight() * float(M_SQRT2));
}

void RenderMathMLMenclose::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);

    if (info.context->paintingDisabled() || info.phase != PaintPhaseForeground || style().visibility() != VISIBLE)
        return;
    
    MathMLMencloseElement* menclose = toMathMLMencloseElement(element());
    const Vector<String>& notationValues = menclose->notationValues();
    size_t notationalValueSize = notationValues.size();
    bool isDefaultLongDiv = menclose->isDefaultLongDiv();
    if ((notationalValueSize && checkNotationalValuesValidity(notationValues)) || isDefaultLongDiv) {
        IntRect rect = absoluteBoundingBoxRect();
        int left = rect.x();
        int top = rect.y();
        int boxWidth = rect.width();
        int boxHeight = rect.height();
        int halfboxWidth = rect.width() / 2;
        int halfboxHeight = rect.height() / 2;

        GraphicsContextStateSaver stateSaver(*info.context);
        info.context->setStrokeThickness(1);
        info.context->setStrokeStyle(SolidStroke);
        info.context->setStrokeColor(style().visitedDependentColor(CSSPropertyColor), ColorSpaceDeviceRGB);
        // TODO add support for notation value updiagonalarrow https://bugs.webkit.org/show_bug.cgi?id=127466
        for (size_t i = 0; i < notationalValueSize; i++) {
            if (notationValues[i] == "updiagonalstrike")
                info.context->drawLine(IntPoint(left, top + boxHeight), IntPoint(left + boxWidth, top));
            else if (notationValues[i] == "downdiagonalstrike")
                info.context->drawLine(IntPoint(left, top), IntPoint(left + boxWidth, top + boxHeight));
            else if (notationValues[i] == "verticalstrike")
                info.context->drawLine(IntPoint(left + halfboxWidth, top), IntPoint(left + halfboxWidth, top + boxHeight));
            else if (notationValues[i] == "horizontalstrike")
                info.context->drawLine(IntPoint(left, top + halfboxHeight), IntPoint(left + boxWidth, top + halfboxHeight));
            else if (notationValues[i] == "circle") {
                info.context->setFillColor(Color::transparent, ColorSpaceDeviceRGB);
                info.context->drawEllipse(rect);
            } else if (notationValues[i] == "longdiv")
                isDefaultLongDiv = true;
        }
        if (isDefaultLongDiv) {
            Path root;
            int midxPoint = 0;
            root.moveTo(FloatPoint(left, top));
            int childLeft = firstChild() ? firstChild()->absoluteBoundingBoxRect().x() : 0;
            if (childLeft)
                midxPoint= childLeft - left;
            else
                midxPoint = style().paddingLeft().value();
            root.addBezierCurveTo(FloatPoint(left, top), FloatPoint(left + midxPoint, top + halfboxHeight), FloatPoint(left, top + boxHeight));
            info.context->strokePath(root);
            if (isDefaultLongDiv)
                info.context->drawLine(IntPoint(left, top), IntPoint(left + boxWidth + midxPoint, top));
        }
    }
}

bool RenderMathMLMenclose::checkNotationalValuesValidity(const Vector<String>& attr) const
{
    size_t attrSize = attr.size();
    for (size_t i = 0; i < attrSize; i++) {
        if (attr[i] == "updiagonalstrike" || attr[i] == "downdiagonalstrike" || attr[i] == "horizontalstrike" || attr[i] == "verticalstrike"
            || attr[i] == "circle" || attr[i] == "longdiv")
            return true;
    }
    return false;
}

}
#endif // ENABLE(MATHML)
