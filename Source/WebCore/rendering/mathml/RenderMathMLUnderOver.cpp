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
#include "RenderMathMLUnderOver.h"

#if ENABLE(MATHML)

#include "MathMLElement.h"
#include "MathMLOperatorDictionary.h"
#include "MathMLUnderOverElement.h"
#include "RenderIterator.h"
#include "RenderMathMLOperator.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLUnderOver);

RenderMathMLUnderOver::RenderMathMLUnderOver(MathMLUnderOverElement& element, RenderStyle&& style)
    : RenderMathMLScripts(element, WTFMove(style))
{
}

MathMLUnderOverElement& RenderMathMLUnderOver::element() const
{
    return static_cast<MathMLUnderOverElement&>(nodeForNonAnonymous());
}

static RenderMathMLOperator* horizontalStretchyOperator(const RenderBox& box)
{
    if (!is<RenderMathMLBlock>(box))
        return nullptr;

    auto* renderOperator = downcast<RenderMathMLBlock>(box).unembellishedOperator();
    if (!renderOperator)
        return nullptr;

    if (!renderOperator->isStretchy() || renderOperator->isVertical() || renderOperator->isStretchWidthLocked())
        return nullptr;

    return renderOperator;
}

static void fixLayoutAfterStretch(RenderBox& ancestor, RenderMathMLOperator& stretchyOperator)
{
    stretchyOperator.setStretchWidthLocked(true);
    stretchyOperator.setNeedsLayout();
    ancestor.layoutIfNeeded();
    stretchyOperator.setStretchWidthLocked(false);
}

void RenderMathMLUnderOver::stretchHorizontalOperatorsAndLayoutChildren()
{
    ASSERT(isValid());
    ASSERT(needsLayout());

    // We apply horizontal stretchy rules from the MathML spec (sections 3.2.5.8.3 and 3.2.5.8.4), which
    // can be roughly summarized as "stretching opersators to the maximum widths of all children" and
    // minor variations of that algorithm do not affect the result. However, the spec is a bit ambiguous
    // for embellished operators (section 3.2.5.7.3) and different approaches can lead to significant
    // stretch size differences. We made the following decisions:
    // - The unstretched size is the embellished operator width with the <mo> at the core unstretched.
    // - In general, the target size is just the maximum widths of non-stretchy children because the
    // embellishments could make widths significantly larger.
    // - In the edge case when all operators of stretchy, we follow the specification and take the
    // maximum of all unstretched sizes.
    // - The <mo> at the core is stretched to cover the target size, even if the embellished operator
    // might become much wider.
    
    Vector<RenderBox*, 3> embellishedOperators;
    Vector<RenderMathMLOperator*, 3> stretchyOperators;
    bool isAllStretchyOperators = true;
    LayoutUnit stretchWidth;

    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (auto* stretchyOperator = horizontalStretchyOperator(*child)) {
            embellishedOperators.append(child);
            stretchyOperators.append(stretchyOperator);
        } else {
            isAllStretchyOperators = false;
            child->layoutIfNeeded();
            stretchWidth = std::max(stretchWidth, child->logicalWidth());
        }
    }

    if (isAllStretchyOperators) {
        for (size_t i = 0; i < embellishedOperators.size(); i++) {
            stretchyOperators[i]->resetStretchSize();
            fixLayoutAfterStretch(*embellishedOperators[i], *stretchyOperators[i]);
            stretchWidth = std::max(stretchWidth, embellishedOperators[i]->logicalWidth());
        }
    }

    for (size_t i = 0; i < embellishedOperators.size(); i++) {
        stretchyOperators[i]->stretchTo(stretchWidth);
        fixLayoutAfterStretch(*embellishedOperators[i], *stretchyOperators[i]);
    }
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
    switch (scriptType()) {
    case MathMLScriptsElement::ScriptType::Over:
    case MathMLScriptsElement::ScriptType::Under:
        return !child;
    case MathMLScriptsElement::ScriptType::UnderOver:
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
    ASSERT(scriptType() == MathMLScriptsElement::ScriptType::Under || scriptType() == MathMLScriptsElement::ScriptType::UnderOver);
    return *firstChildBox()->nextSiblingBox();
}

RenderBox& RenderMathMLUnderOver::over() const
{
    ASSERT(isValid());
    ASSERT(scriptType() == MathMLScriptsElement::ScriptType::Over || scriptType() == MathMLScriptsElement::ScriptType::UnderOver);
    auto* secondChild = firstChildBox()->nextSiblingBox();
    return scriptType() == MathMLScriptsElement::ScriptType::Over ? *secondChild : *secondChild->nextSiblingBox();
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

    if (scriptType() == MathMLScriptsElement::ScriptType::Under || scriptType() == MathMLScriptsElement::ScriptType::UnderOver)
        preferredWidth = std::max(preferredWidth, under().maxPreferredLogicalWidth());

    if (scriptType() == MathMLScriptsElement::ScriptType::Over || scriptType() == MathMLScriptsElement::ScriptType::UnderOver)
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
    ASSERT(scriptType() == MathMLScriptsElement::ScriptType::UnderOver || (accentUnder && scriptType() == MathMLScriptsElement::ScriptType::Under) || (!accentUnder && scriptType() == MathMLScriptsElement::ScriptType::Over));

    const MathMLElement::BooleanValue& attributeValue = accentUnder ? element().accentUnder() : element().accent();
    if (attributeValue == MathMLElement::BooleanValue::True)
        return true;
    if (attributeValue == MathMLElement::BooleanValue::False)
        return false;
    RenderBox& script = accentUnder ? under() : over();
    if (!is<RenderMathMLBlock>(script))
        return false;
    auto* scriptOperator = downcast<RenderMathMLBlock>(script).unembellishedOperator();
    return scriptOperator && scriptOperator->hasOperatorFlag(MathMLOperatorDictionary::Accent);
}

RenderMathMLUnderOver::VerticalParameters RenderMathMLUnderOver::verticalParameters() const
{
    VerticalParameters parameters;

    // By default, we set all values to zero.
    parameters.underGapMin = 0;
    parameters.overGapMin = 0;
    parameters.underShiftMin = 0;
    parameters.overShiftMin = 0;
    parameters.underExtraDescender = 0;
    parameters.overExtraAscender = 0;
    parameters.accentBaseHeight = 0;

    const auto& primaryFont = style().fontCascade().primaryFont();
    auto* mathData = primaryFont.mathData();
    if (!mathData) {
        // The MATH table specification does not really provide any suggestions, except for some underbar/overbar values and AccentBaseHeight.
        LayoutUnit defaultLineThickness = ruleThicknessFallback();
        parameters.underGapMin = 3 * defaultLineThickness;
        parameters.overGapMin = 3 * defaultLineThickness;
        parameters.underExtraDescender = defaultLineThickness;
        parameters.overExtraAscender = defaultLineThickness;
        parameters.accentBaseHeight = style().fontMetrics().xHeight();
        parameters.useUnderOverBarFallBack = true;
        return parameters;
    }

    if (is<RenderMathMLBlock>(base())) {
        if (auto* baseOperator = downcast<RenderMathMLBlock>(base()).unembellishedOperator()) {
            if (baseOperator->hasOperatorFlag(MathMLOperatorDictionary::LargeOp)) {
                // The base is a large operator so we read UpperLimit/LowerLimit constants from the MATH table.
                parameters.underGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::LowerLimitGapMin);
                parameters.overGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::UpperLimitGapMin);
                parameters.underShiftMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::LowerLimitBaselineDropMin);
                parameters.overShiftMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::UpperLimitBaselineRiseMin);
                parameters.useUnderOverBarFallBack = false;
                return parameters;
            }
            if (baseOperator->isStretchy() && !baseOperator->isVertical()) {
                // The base is a horizontal stretchy operator, so we read StretchStack constants from the MATH table.
                parameters.underGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::StretchStackGapBelowMin);
                parameters.overGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::StretchStackGapAboveMin);
                parameters.underShiftMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::StretchStackBottomShiftDown);
                parameters.overShiftMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::StretchStackTopShiftUp);
                parameters.useUnderOverBarFallBack = false;
                return parameters;
            }
        }
    }

    // By default, we just use the underbar/overbar constants.
    parameters.underGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::UnderbarVerticalGap);
    parameters.overGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::OverbarVerticalGap);
    parameters.underExtraDescender = mathData->getMathConstant(primaryFont, OpenTypeMathData::UnderbarExtraDescender);
    parameters.overExtraAscender = mathData->getMathConstant(primaryFont, OpenTypeMathData::OverbarExtraAscender);
    parameters.accentBaseHeight = mathData->getMathConstant(primaryFont, OpenTypeMathData::AccentBaseHeight);
    parameters.useUnderOverBarFallBack = true;
    return parameters;
}

void RenderMathMLUnderOver::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    if (!isValid()) {
        layoutInvalidMarkup(relayoutChildren);
        return;
    }

    if (shouldMoveLimits()) {
        RenderMathMLScripts::layoutBlock(relayoutChildren, pageLogicalHeight);
        return;
    }

    recomputeLogicalWidth();

    stretchHorizontalOperatorsAndLayoutChildren();

    ASSERT(!base().needsLayout());
    ASSERT(scriptType() == MathMLScriptsElement::ScriptType::Over || !under().needsLayout());
    ASSERT(scriptType() == MathMLScriptsElement::ScriptType::Under || !over().needsLayout());

    LayoutUnit logicalWidth = base().logicalWidth();
    if (scriptType() == MathMLScriptsElement::ScriptType::Under || scriptType() == MathMLScriptsElement::ScriptType::UnderOver)
        logicalWidth = std::max(logicalWidth, under().logicalWidth());
    if (scriptType() == MathMLScriptsElement::ScriptType::Over || scriptType() == MathMLScriptsElement::ScriptType::UnderOver)
        logicalWidth = std::max(logicalWidth, over().logicalWidth());
    setLogicalWidth(logicalWidth);

    VerticalParameters parameters = verticalParameters();
    LayoutUnit verticalOffset;
    if (scriptType() == MathMLScriptsElement::ScriptType::Over || scriptType() == MathMLScriptsElement::ScriptType::UnderOver) {
        verticalOffset += parameters.overExtraAscender;
        over().setLocation(LayoutPoint(horizontalOffset(over()), verticalOffset));
        if (parameters.useUnderOverBarFallBack) {
            verticalOffset += over().logicalHeight();
            if (hasAccent()) {
                LayoutUnit baseAscent = ascentForChild(base());
                if (baseAscent < parameters.accentBaseHeight)
                    verticalOffset += parameters.accentBaseHeight - baseAscent;
            } else
                verticalOffset += parameters.overGapMin;
        } else {
            LayoutUnit overAscent = ascentForChild(over());
            verticalOffset += std::max(over().logicalHeight() + parameters.overGapMin, overAscent + parameters.overShiftMin);
        }
    }
    base().setLocation(LayoutPoint(horizontalOffset(base()), verticalOffset));
    verticalOffset += base().logicalHeight();
    if (scriptType() == MathMLScriptsElement::ScriptType::Under || scriptType() == MathMLScriptsElement::ScriptType::UnderOver) {
        if (parameters.useUnderOverBarFallBack) {
            if (!hasAccentUnder())
                verticalOffset += parameters.underGapMin;
        } else {
            LayoutUnit underAscent = ascentForChild(under());
            verticalOffset += std::max(parameters.underGapMin, parameters.underShiftMin - underAscent);
        }
        under().setLocation(LayoutPoint(horizontalOffset(under()), verticalOffset));
        verticalOffset += under().logicalHeight();
        verticalOffset += parameters.underExtraDescender;
    }

    setLogicalHeight(verticalOffset);

    layoutPositionedObjects(relayoutChildren);

    clearNeedsLayout();
}

}

#endif // ENABLE(MATHML)
