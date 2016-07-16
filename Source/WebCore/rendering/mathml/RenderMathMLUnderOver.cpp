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
#include "MathMLOperatorDictionary.h"
#include "RenderIterator.h"
#include "RenderMathMLOperator.h"

namespace WebCore {

using namespace MathMLNames;

RenderMathMLUnderOver::RenderMathMLUnderOver(Element& element, RenderStyle&& style)
    : RenderMathMLScripts(element, WTFMove(style))
{
}

void RenderMathMLUnderOver::computeOperatorsHorizontalStretch()
{
    LayoutUnit stretchWidth = 0;
    Vector<RenderMathMLOperator*, 2> renderOperators;

    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->needsLayout()) {
            if (is<RenderMathMLBlock>(child)) {
                if (auto renderOperator = downcast<RenderMathMLBlock>(*child).unembellishedOperator()) {
                    if (renderOperator->hasOperatorFlag(MathMLOperatorDictionary::Stretchy) && !renderOperator->isVertical()) {
                        renderOperator->resetStretchSize();
                        renderOperators.append(renderOperator);
                    }
                }
            }

            child->layout();
        }

        // Skipping the embellished op does not work for nested structures like
        // <munder><mover><mo>_</mo>...</mover> <mo>_</mo></munder>.
        stretchWidth = std::max(stretchWidth, child->logicalWidth());
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
    auto* child = firstChildBox();
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

bool RenderMathMLUnderOver::shouldMoveLimits()
{
    if (auto* renderOperator = unembellishedOperator())
        return renderOperator->shouldMoveLimits();
    return false;
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
    auto* secondChild = firstChildBox()->nextSiblingBox();
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

    if (shouldMoveLimits()) {
        RenderMathMLScripts::computePreferredLogicalWidths();
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

bool RenderMathMLUnderOver::hasAccent(bool accentUnder) const
{
    ASSERT(m_scriptType == UnderOver || (accentUnder && m_scriptType == Under) || (!accentUnder && m_scriptType == Over));

    const AtomicString& attributeValue = element()->attributeWithoutSynchronization(accentUnder ? accentunderAttr : accentAttr);
    if (attributeValue == "true")
        return true;
    if (attributeValue == "false")
        return false;
    RenderBox& script = accentUnder ? under() : over();
    if (!is<RenderMathMLBlock>(script))
        return false;
    auto* scriptOperator = downcast<RenderMathMLBlock>(script).unembellishedOperator();
    return scriptOperator && scriptOperator->hasOperatorFlag(MathMLOperatorDictionary::Accent);
}

bool RenderMathMLUnderOver::getVerticalParameters(LayoutUnit& underGapMin, LayoutUnit& overGapMin, LayoutUnit& underShiftMin, LayoutUnit& overShiftMin, LayoutUnit& underExtraDescender, LayoutUnit& overExtraAscender, LayoutUnit& accentBaseHeight) const
{
    // By default, we set all values to zero.
    underGapMin = overGapMin = underShiftMin = overShiftMin = underExtraDescender = overExtraAscender = accentBaseHeight = 0;

    const auto& primaryFont = style().fontCascade().primaryFont();
    auto* mathData = primaryFont.mathData();
    if (!mathData) {
        // The MATH table specification does not really provide any suggestions, except for some underbar/overbar values and AccentBaseHeight.
        LayoutUnit defaultLineThickness = ruleThicknessFallback();
        underGapMin = overGapMin = 3 * defaultLineThickness;
        underExtraDescender = overExtraAscender = defaultLineThickness;
        accentBaseHeight = style().fontMetrics().xHeight();
        return true;
    }

    if (is<RenderMathMLBlock>(base())) {
        if (auto* baseOperator = downcast<RenderMathMLBlock>(base()).unembellishedOperator()) {
            if (baseOperator->hasOperatorFlag(MathMLOperatorDictionary::LargeOp)) {
                // The base is a large operator so we read UpperLimit/LowerLimit constants from the MATH table.
                underGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::LowerLimitGapMin);
                overGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::UpperLimitGapMin);
                underShiftMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::LowerLimitBaselineDropMin);
                overShiftMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::UpperLimitBaselineRiseMin);
                return false;
            }
            if (baseOperator->hasOperatorFlag(MathMLOperatorDictionary::Stretchy) && !baseOperator->isVertical()) {
                // The base is a horizontal stretchy operator, so we read StretchStack constants from the MATH table.
                underGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::StretchStackGapBelowMin);
                overGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::StretchStackGapAboveMin);
                underShiftMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::StretchStackBottomShiftDown);
                overShiftMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::StretchStackTopShiftUp);
                return false;
            }
        }
    }

    // By default, we just use the underbar/overbar constants.
    underGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::UnderbarVerticalGap);
    overGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::OverbarVerticalGap);
    underExtraDescender = mathData->getMathConstant(primaryFont, OpenTypeMathData::UnderbarExtraDescender);
    overExtraAscender = mathData->getMathConstant(primaryFont, OpenTypeMathData::OverbarExtraAscender);
    accentBaseHeight = mathData->getMathConstant(primaryFont, OpenTypeMathData::AccentBaseHeight);
    return true;
}

void RenderMathMLUnderOver::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
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

    if (shouldMoveLimits()) {
        RenderMathMLScripts::layoutBlock(relayoutChildren, pageLogicalHeight);
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

    LayoutUnit underGapMin, overGapMin, underShiftMin, overShiftMin, underExtraDescender, overExtraAscender, accentBaseHeight;
    bool underOverBarFall = getVerticalParameters(underGapMin, overGapMin, underShiftMin, overShiftMin, underExtraDescender, overExtraAscender, accentBaseHeight);
    LayoutUnit verticalOffset = 0;
    if (m_scriptType == Over || m_scriptType == UnderOver) {
        verticalOffset += overExtraAscender;
        over().setLocation(LayoutPoint(horizontalOffset(over()), verticalOffset));
        if (underOverBarFall) {
            verticalOffset += over().logicalHeight();
            if (hasAccent()) {
                LayoutUnit baseAscent = ascentForChild(base());
                if (baseAscent < accentBaseHeight)
                    verticalOffset += accentBaseHeight - baseAscent;
            } else
                verticalOffset += overGapMin;
        } else {
            LayoutUnit overAscent = ascentForChild(over());
            verticalOffset += std::max(over().logicalHeight() + overGapMin, overAscent + overShiftMin);
        }
    }
    base().setLocation(LayoutPoint(horizontalOffset(base()), verticalOffset));
    verticalOffset += base().logicalHeight();
    if (m_scriptType == Under || m_scriptType == UnderOver) {
        if (underOverBarFall) {
            if (!hasAccentUnder())
                verticalOffset += underGapMin;
        } else {
            LayoutUnit underAscent = ascentForChild(under());
            verticalOffset += std::max(underGapMin, underShiftMin - underAscent);
        }
        under().setLocation(LayoutPoint(horizontalOffset(under()), verticalOffset));
        verticalOffset += under().logicalHeight();
        verticalOffset += underExtraDescender;
    }

    setLogicalHeight(verticalOffset);

    clearNeedsLayout();
}

}

#endif // ENABLE(MATHML)
