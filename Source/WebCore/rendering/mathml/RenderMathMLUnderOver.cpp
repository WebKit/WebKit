/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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

#include "RenderMathMLUnderOver.h"

#include "MathMLElement.h"
#include "MathMLNames.h"
#include "RenderIterator.h"
#include "RenderMathMLOperator.h"

namespace WebCore {

using namespace MathMLNames;
    
RenderMathMLUnderOver::RenderMathMLUnderOver(Element& element, Ref<RenderStyle>&& style)
    : RenderMathMLBlock(element, WTFMove(style))
{
    // Determine what kind of under/over expression we have by element name
    if (element.hasTagName(MathMLNames::munderTag))
        m_scriptType = Under;
    else if (element.hasTagName(MathMLNames::moverTag))
        m_scriptType = Over;
    else {
        ASSERT(element.hasTagName(MathMLNames::munderoverTag));
        m_scriptType = UnderOver;
    }
}

RenderMathMLOperator* RenderMathMLUnderOver::unembellishedOperator()
{
    RenderObject* base = firstChild();
    if (!is<RenderMathMLBlock>(base))
        return nullptr;
    return downcast<RenderMathMLBlock>(*base).unembellishedOperator();
}

Optional<int> RenderMathMLUnderOver::firstLineBaseline() const
{
    RenderBox* base = firstChildBox();
    if (!base)
        return Optional<int>();

    return Optional<int>(static_cast<int>(lroundf(ascentForChild(*base) + base->logicalTop())));
}

void RenderMathMLUnderOver::computeOperatorsHorizontalStretch()
{
    LayoutUnit stretchWidth = 0;
    Vector<RenderMathMLOperator*, 2> renderOperators;

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->needsLayout()) {
            if (is<RenderMathMLBlock>(child)) {
                if (auto renderOperator = downcast<RenderMathMLBlock>(*child).unembellishedOperator()) {
                    if (renderOperator->hasOperatorFlag(MathMLOperatorDictionary::Stretchy) && !renderOperator->isVertical()) {
                        renderOperator->resetStretchSize();
                        renderOperators.append(renderOperator);
                    }
                }
            }

            downcast<RenderElement>(*child).layout();
        }

        // Skipping the embellished op does not work for nested structures like
        // <munder><mover><mo>_</mo>...</mover> <mo>_</mo></munder>.
        if (is<RenderBox>(*child))
            stretchWidth = std::max<LayoutUnit>(stretchWidth, downcast<RenderBox>(*child).logicalWidth());
    }

    // Set the sizes of (possibly embellished) stretchy operator children.
    for (auto& renderOperator : renderOperators)
        renderOperator->stretchTo(stretchWidth);
}

bool RenderMathMLUnderOver::isValid() const
{
    // Verify whether the list of children is valid:
    // <munder> base under </munder>
    // <mover> base over </mover>
    // <munderover> base under over </munderover>
    RenderBox* child = firstChildBox();
    if (!child)
        return false;
    child = child->nextSiblingBox();
    if (!child)
        return false;
    child = child->nextSiblingBox();
    switch (m_scriptType) {
    case Over:
    case Under:
        return !child;
    case UnderOver:
        return child && !child->nextSiblingBox();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

RenderBox& RenderMathMLUnderOver::base() const
{
    ASSERT(isValid());
    return *firstChildBox();
}

RenderBox& RenderMathMLUnderOver::under() const
{
    ASSERT(isValid());
    ASSERT(m_scriptType == Under || m_scriptType == UnderOver);
    return *firstChildBox()->nextSiblingBox();
}

RenderBox& RenderMathMLUnderOver::over() const
{
    ASSERT(isValid());
    ASSERT(m_scriptType == Over || m_scriptType == UnderOver);
    RenderBox* secondChild = firstChildBox()->nextSiblingBox();
    return m_scriptType == Over ? *secondChild : *secondChild->nextSiblingBox();
}


void RenderMathMLUnderOver::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    if (!isValid()) {
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;
        setPreferredLogicalWidthsDirty(false);
        return;
    }

    LayoutUnit preferredWidth = base().maxPreferredLogicalWidth();

    if (m_scriptType == Under || m_scriptType == UnderOver)
        preferredWidth = std::max(preferredWidth, under().maxPreferredLogicalWidth());

    if (m_scriptType == Over || m_scriptType == UnderOver)
        preferredWidth = std::max(preferredWidth, over().maxPreferredLogicalWidth());

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = preferredWidth;

    setPreferredLogicalWidthsDirty(false);
}

LayoutUnit RenderMathMLUnderOver::horizontalOffset(const RenderBox& child) const
{
    return (logicalWidth() - child.logicalWidth()) / 2;
}

void RenderMathMLUnderOver::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    if (!isValid()) {
        setLogicalWidth(0);
        setLogicalHeight(0);
        clearNeedsLayout();
        return;
    }

    recomputeLogicalWidth();

    computeOperatorsHorizontalStretch();

    base().layoutIfNeeded();
    if (m_scriptType == Under || m_scriptType == UnderOver)
        under().layoutIfNeeded();
    if (m_scriptType == Over || m_scriptType == UnderOver)
        over().layoutIfNeeded();

    LayoutUnit logicalWidth = base().logicalWidth();
    if (m_scriptType == Under || m_scriptType == UnderOver)
        logicalWidth = std::max(logicalWidth, under().logicalWidth());
    if (m_scriptType == Over || m_scriptType == UnderOver)
        logicalWidth = std::max(logicalWidth, over().logicalWidth());
    setLogicalWidth(logicalWidth);

    LayoutUnit verticalOffset = 0;
    if (m_scriptType == Over || m_scriptType == UnderOver) {
        over().setLocation(LayoutPoint(horizontalOffset(over()), 0));
        verticalOffset += over().logicalHeight();
    }
    base().setLocation(LayoutPoint(horizontalOffset(base()), verticalOffset));
    verticalOffset += base().logicalHeight();
    if (m_scriptType == Under || m_scriptType == UnderOver) {
        under().setLocation(LayoutPoint(horizontalOffset(under()), verticalOffset));
        verticalOffset += under().logicalHeight();
    }

    setLogicalHeight(verticalOffset);

    clearNeedsLayout();
}

void RenderMathMLUnderOver::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!paintChild(*child, paintInfo, paintOffset, paintInfoForChild, usePrintRect, PaintAsInlineBlock))
            return;
    }
}

}

#endif // ENABLE(MATHML)
