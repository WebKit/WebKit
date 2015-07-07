/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
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

#include "config.h"

#if ENABLE(MATHML)

#include "RenderMathMLRoot.h"

#include "FontCache.h"
#include "GraphicsContext.h"
#include "PaintInfo.h"
#include "RenderIterator.h"
#include "RenderMathMLRadicalOperator.h"

namespace WebCore {
    
// RenderMathMLRoot implements drawing of radicals via the <mroot> and <msqrt> elements. For valid MathML elements, the DOM is
//
// <mroot> Base Index </mroot>
// <msqrt> Child1 Child2 ... ChildN </msqrt>
//
// and the structure of the render tree will be
//
// IndexWrapper RadicalWrapper BaseWrapper
//
// where RadicalWrapper contains an <mo>&#x221A;</mo>.
// For <mroot>, the IndexWrapper and BaseWrapper should contain exactly one child (Index and Base respectively).
// For <msqrt>, the IndexWrapper should be empty and the BaseWrapper can contain any number of children (Child1, ... ChildN).
//
// In order to accept invalid markup and to handle <mroot> and <msqrt> consistently, we will allow any number of children in the BaseWrapper of <mroot> too.
// We will allow the IndexWrapper to be empty and it will always contain the last child of the <mroot> if there are at least 2 elements.

RenderMathMLRoot::RenderMathMLRoot(Element& element, PassRef<RenderStyle> style)
    : RenderMathMLBlock(element, WTF::move(style))
{
}

RenderMathMLRoot::RenderMathMLRoot(Document& document, PassRef<RenderStyle> style)
    : RenderMathMLBlock(document, WTF::move(style))
{
}

RenderMathMLRootWrapper* RenderMathMLRoot::baseWrapper() const
{
    ASSERT(!isEmpty());
    return toRenderMathMLRootWrapper(lastChild());
}

RenderMathMLBlock* RenderMathMLRoot::radicalWrapper() const
{
    ASSERT(!isEmpty());
    return toRenderMathMLBlock(lastChild()->previousSibling());
}

RenderMathMLRootWrapper* RenderMathMLRoot::indexWrapper() const
{
    ASSERT(!isEmpty());
    return isRenderMathMLSquareRoot() ? nullptr : toRenderMathMLRootWrapper(firstChild());
}

RenderMathMLRadicalOperator* RenderMathMLRoot::radicalOperator() const
{
    ASSERT(!isEmpty());
    return toRenderMathMLRadicalOperator(radicalWrapper()->firstChild());
}

void RenderMathMLRoot::restructureWrappers()
{
    ASSERT(!isEmpty());

    auto base = baseWrapper();
    auto index = indexWrapper();
    auto radical = radicalWrapper();

    // For visual consistency with the initial state, we remove the radical when the base/index wrappers become empty.
    if (base->isEmpty() && (!index || index->isEmpty())) {
        if (!radical->isEmpty()) {
            auto child = radicalOperator();
            radical->removeChild(*child);
            child->destroy();
        }
        // FIXME: early return!!!
    }

    if (radical->isEmpty()) {
        // We create the radical operator.
        RenderPtr<RenderMathMLRadicalOperator> radicalOperator = createRenderer<RenderMathMLRadicalOperator>(document(), RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX));
        radicalOperator->initializeStyle();
        radical->addChild(radicalOperator.leakPtr());
    }

    if (isRenderMathMLSquareRoot())
        return;

    if (auto childToMove = base->lastChild()) {
        // We move the last child of the base wrapper into the index wrapper if the index wrapper is empty and the base wrapper has at least two children.
        if (childToMove->previousSibling() && index->isEmpty()) {
            base->removeChildWithoutRestructuring(*childToMove);
            index->addChild(childToMove);
        }
    }

    if (auto childToMove = index->firstChild()) {
        // We move the first child of the index wrapper into the base wrapper if:
        // - either the index wrapper has at least two children.
        // - or the base wrapper is empty but the index wrapper is not.
        if (childToMove->nextSibling() || base->isEmpty()) {
            index->removeChildWithoutRestructuring(*childToMove);
            base->addChild(childToMove);
        }
    }
}

void RenderMathMLRoot::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (isEmpty()) {
        if (!isRenderMathMLSquareRoot()) {
            // We add the IndexWrapper.
            RenderMathMLBlock::addChild(RenderMathMLRootWrapper::createAnonymousWrapper(this).leakPtr());
        }

        // We create the radicalWrapper
        RenderMathMLBlock::addChild(RenderMathMLBlock::createAnonymousMathMLBlock().leakPtr());

        // We create the BaseWrapper.
        RenderMathMLBlock::addChild(RenderMathMLRootWrapper::createAnonymousWrapper(this).leakPtr());

        updateStyle();
    }

    // We insert the child.
    auto base = baseWrapper();
    auto index = indexWrapper();
    RenderElement* actualParent;
    RenderElement* actualBeforeChild;
    if (isRenderMathMLSquareRoot()) {
        // For square root, we always insert the child into the base wrapper.
        actualParent = base;
        if (beforeChild && beforeChild->parent() == base)
            actualBeforeChild = toRenderElement(beforeChild);
        else
            actualBeforeChild = nullptr;
    } else {
        // For mroot, we insert the child into the parent of beforeChild, or at the end of the index. The wrapper structure is reorganize below.
        actualParent = beforeChild ? beforeChild->parent() : nullptr;
        if (actualParent == base || actualParent == index)
            actualBeforeChild = toRenderElement(beforeChild);
        else {
            actualParent = index;
            actualBeforeChild = nullptr;
        }
    }
    actualParent->addChild(newChild, actualBeforeChild);
    restructureWrappers();
}

void RenderMathMLRoot::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    if (!isEmpty())
        updateStyle();
}

void RenderMathMLRoot::updateFromElement()
{
    RenderMathMLBlock::updateFromElement();
    if (!isEmpty())
        updateStyle();
}

void RenderMathMLRoot::updateStyle()
{
    ASSERT(!isEmpty());

    // We set some constants to draw the radical, as defined in the OpenType MATH tables.

    m_ruleThickness = 0.05f * style().font().size();

    // FIXME: The recommended default for m_verticalGap in displaystyle is rule thickness + 1/4 x-height (https://bugs.webkit.org/show_bug.cgi?id=118737).
    m_verticalGap = 11 * m_ruleThickness / 4;
    m_extraAscender = m_ruleThickness;
    LayoutUnit kernBeforeDegree = 5 * style().font().size() / 18;
    LayoutUnit kernAfterDegree = -10 * style().font().size() / 18;
    m_degreeBottomRaisePercent = 0.6f;

    const auto& primaryFontData = style().font().primaryFont();
    if (primaryFontData && primaryFontData->mathData()) {
        // FIXME: m_verticalGap should use RadicalDisplayStyleVertical in display mode (https://bugs.webkit.org/show_bug.cgi?id=118737).
        m_verticalGap = primaryFontData->mathData()->getMathConstant(primaryFontData, OpenTypeMathData::RadicalVerticalGap);
        m_ruleThickness = primaryFontData->mathData()->getMathConstant(primaryFontData, OpenTypeMathData::RadicalRuleThickness);
        m_extraAscender = primaryFontData->mathData()->getMathConstant(primaryFontData, OpenTypeMathData::RadicalExtraAscender);

        if (!isRenderMathMLSquareRoot()) {
            kernBeforeDegree = primaryFontData->mathData()->getMathConstant(primaryFontData, OpenTypeMathData::RadicalKernBeforeDegree);
            kernAfterDegree = primaryFontData->mathData()->getMathConstant(primaryFontData, OpenTypeMathData::RadicalKernAfterDegree);
            m_degreeBottomRaisePercent = primaryFontData->mathData()->getMathConstant(primaryFontData, OpenTypeMathData::RadicalDegreeBottomRaisePercent);
        }
    }

    // We set the style of the anonymous wrappers.

    auto radical = radicalWrapper();
    auto radicalStyle = RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX);
    radicalStyle.get().setMarginTop(Length(0, Fixed)); // This will be updated in RenderMathMLRoot::layout().
    radical->setStyle(WTF::move(radicalStyle));
    radical->setNeedsLayoutAndPrefWidthsRecalc();

    auto base = baseWrapper();
    auto baseStyle = RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX);
    baseStyle.get().setMarginTop(Length(0, Fixed)); // This will be updated in RenderMathMLRoot::layout().
    baseStyle.get().setAlignItems(AlignBaseline);
    base->setStyle(WTF::move(baseStyle));
    base->setNeedsLayoutAndPrefWidthsRecalc();

    if (!isRenderMathMLSquareRoot()) {
        // For mroot, we also set the style of the index wrapper.
        auto index = indexWrapper();
        auto indexStyle = RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX);
        indexStyle.get().setMarginTop(Length(0, Fixed)); // This will be updated in RenderMathMLRoot::layout().
        indexStyle.get().setMarginStart(Length(kernBeforeDegree, Fixed));
        indexStyle.get().setMarginEnd(Length(kernAfterDegree, Fixed));
        indexStyle.get().setAlignItems(AlignBaseline);
        index->setStyle(WTF::move(indexStyle));
        index->setNeedsLayoutAndPrefWidthsRecalc();
    }
}

int RenderMathMLRoot::firstLineBaseline() const
{
    if (!isEmpty()) {
        auto base = baseWrapper();
        return static_cast<int>(lroundf(base->firstLineBaseline() + base->marginTop()));
    }

    return RenderMathMLBlock::firstLineBaseline();
}

void RenderMathMLRoot::layout()
{
    if (isEmpty()) {
        RenderMathMLBlock::layout();
        return;
    }

    // FIXME: It seems that changing the top margin of the base below modifies its logical height and leads to reftest failures.
    // For now, we workaround that by avoiding to recompute the child margins if they were not reset in updateStyle().
    auto base = baseWrapper();
    if (base->marginTop() > 0) {
        RenderMathMLBlock::layout();
        return;
    }

    // We layout the children.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->needsLayout())
            toRenderElement(child)->layout();
    }

    auto radical = radicalOperator();
    if (radical) {
        // We stretch the radical sign to cover the height of the base wrapper.
        float baseHeight = base->logicalHeight();
        float baseHeightAboveBaseline = base->firstLineBaseline();
        if (baseHeightAboveBaseline == -1)
            baseHeightAboveBaseline = baseHeight;
        float baseDepthBelowBaseline = baseHeight - baseHeightAboveBaseline;
        baseHeightAboveBaseline += m_verticalGap;
        radical->stretchTo(baseHeightAboveBaseline, baseDepthBelowBaseline);

        // We modify the top margins to adjust the vertical positions of wrappers.
        float radicalTopMargin = m_extraAscender;
        float baseTopMargin = m_verticalGap + m_ruleThickness + m_extraAscender;
        if (!isRenderMathMLSquareRoot()) {
            // For mroot, we try to place the index so the space below its baseline is m_degreeBottomRaisePercent times the height of the radical.
            auto index = indexWrapper();
            float indexHeight = 0;
            if (!index->isEmpty())
                indexHeight = toRenderBlock(index->firstChild())->logicalHeight();
            float indexTopMargin = (1.0 - m_degreeBottomRaisePercent) * radical->stretchSize() + radicalTopMargin - indexHeight;
            if (indexTopMargin < 0) {
                // If the index is too tall, we must add space at the top of renderer.
                radicalTopMargin -= indexTopMargin;
                baseTopMargin -= indexTopMargin;
                indexTopMargin = 0;
            }
            index->style().setMarginTop(Length(indexTopMargin, Fixed));
        }
        radical->style().setMarginTop(Length(radicalTopMargin, Fixed));
        base->style().setMarginTop(Length(baseTopMargin, Fixed));
    }

    RenderMathMLBlock::layout();
}

void RenderMathMLRoot::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);
    
    if (isEmpty() || info.context->paintingDisabled() || style().visibility() != VISIBLE)
        return;

    auto base = baseWrapper();
    auto radical = radicalOperator();
    if (!base || !radical || !m_ruleThickness)
        return;

    // We draw the radical line.
    GraphicsContextStateSaver stateSaver(*info.context);

    info.context->setStrokeThickness(m_ruleThickness);
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeColor(style().visitedDependentColor(CSSPropertyColor), ColorSpaceDeviceRGB);

    // The preferred width of the radical is sometimes incorrect, so we draw a slightly longer line to ensure it touches the radical symbol (https://bugs.webkit.org/show_bug.cgi?id=130326).
    LayoutUnit sizeError = radical->trailingSpaceError();
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location() + base->location() + LayoutPoint(-sizeError, -(m_verticalGap + m_ruleThickness / 2)));
    info.context->drawLine(adjustedPaintOffset, IntPoint(adjustedPaintOffset.x() + base->pixelSnappedOffsetWidth() + sizeError, adjustedPaintOffset.y()));
}

RenderPtr<RenderMathMLRootWrapper> RenderMathMLRootWrapper::createAnonymousWrapper(RenderMathMLRoot* renderObject)
{
    RenderPtr<RenderMathMLRootWrapper> newBlock = createRenderer<RenderMathMLRootWrapper>(renderObject->document(), RenderStyle::createAnonymousStyleWithDisplay(&renderObject->style(), FLEX));
    newBlock->initializeStyle();
    return newBlock;
}

RenderObject* RenderMathMLRootWrapper::removeChildWithoutRestructuring(RenderObject& child)
{
    return RenderMathMLBlock::removeChild(child);
}

RenderObject* RenderMathMLRootWrapper::removeChild(RenderObject& child)
{
    RenderObject* next = RenderMathMLBlock::removeChild(child);

    if (!(beingDestroyed() || documentBeingDestroyed()))
        toRenderMathMLRoot(parent())->restructureWrappers();

    return next;
}

}

#endif // ENABLE(MATHML)
