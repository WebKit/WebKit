/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2013 The MathJax Consortium.
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
#include "RenderMathMLScripts.h"

#if ENABLE(MATHML)

#include "MathMLElement.h"
#include "MathMLScriptsElement.h"
#include "RenderMathMLBlockInlines.h"
#include "RenderMathMLOperator.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderMathMLScripts);

static bool isPrescriptDelimiter(const RenderObject& renderObject)
{
    return renderObject.node() && renderObject.node()->hasTagName(MathMLNames::mprescriptsTag);
}

RenderMathMLScripts::RenderMathMLScripts(Type type, MathMLScriptsElement& element, RenderStyle&& style)
    : RenderMathMLRow(type, element, WTFMove(style))
{
}

RenderMathMLScripts::~RenderMathMLScripts() = default;

MathMLScriptsElement& RenderMathMLScripts::element() const
{
    return static_cast<MathMLScriptsElement&>(nodeForNonAnonymous());
}

MathMLScriptsElement::ScriptType RenderMathMLScripts::scriptType() const
{
    return element().scriptType();
}

RenderMathMLOperator* RenderMathMLScripts::unembellishedOperator() const
{
    auto possibleReference = validateAndGetReferenceChildren();
    if (!possibleReference)
        return RenderMathMLRow::unembellishedOperator();

    auto* base = dynamicDowncast<RenderMathMLBlock>(possibleReference.value().base);
    return base ? base->unembellishedOperator() : nullptr;
}

std::optional<RenderMathMLScripts::ReferenceChildren> RenderMathMLScripts::validateAndGetReferenceChildren() const
{
    // All scripted elements must have at least one in-flow child.
    // The first child is the base.
    auto base = firstInFlowChildBox();
    if (!base)
        return std::nullopt;

    ReferenceChildren reference;
    reference.base = base;
    reference.firstPostScript = nullptr;
    reference.firstPreScript = nullptr;
    reference.prescriptDelimiter = nullptr;

    switch (scriptType()) {
    case MathMLScriptsElement::ScriptType::Sub:
    case MathMLScriptsElement::ScriptType::Super:
    case MathMLScriptsElement::ScriptType::Under:
    case MathMLScriptsElement::ScriptType::Over: {
        // These elements must have exactly two in-flow children.
        // The second child is a postscript and there are no prescripts.
        // <msub> base subscript </msub>
        // <msup> base superscript </msup>
        // <munder> base underscript </munder>
        // <mover> base overscript </mover>
        auto script = base->nextInFlowSiblingBox();
        if (!script || isPrescriptDelimiter(*script) || script->nextInFlowSiblingBox())
            return std::nullopt;
        reference.firstPostScript = script;
        return reference;
    }
    case MathMLScriptsElement::ScriptType::SubSup:
    case MathMLScriptsElement::ScriptType::UnderOver: {
        // These elements must have exactly three in-flow children.
        // The second and third children are postscripts and there are no prescripts.
        // <msubsup> base subscript superscript </msubsup>
        // <munderover> base subscript superscript </munderover>
        auto subScript = base->nextInFlowSiblingBox();
        if (!subScript || isPrescriptDelimiter(*subScript))
            return std::nullopt;
        auto superScript = subScript->nextInFlowSiblingBox();
        if (!superScript || isPrescriptDelimiter(*superScript) || superScript->nextInFlowSiblingBox())
            return std::nullopt;
        reference.firstPostScript = subScript;
        return reference;
    }
    case MathMLScriptsElement::ScriptType::Multiscripts: {
        // This element accepts the following syntax:
        //
        // <mmultiscripts>
        //   base
        //   (subscript superscript)*
        //   [ <mprescripts/> (presubscript presuperscript)* ]
        // </mmultiscripts>
        //
        // https://www.w3.org/TR/MathML3/chapter3.html#presm.mmultiscripts
        //
        // The base cannot be an <mprescripts> element.
        if (isPrescriptDelimiter(*base))
            return std::nullopt;

        // We set the first postscript, unless (subscript superscript)* is empty.
        if (base->nextInFlowSiblingBox() && !isPrescriptDelimiter(*base->nextInFlowSiblingBox()))
            reference.firstPostScript = base->nextInFlowSiblingBox();

        // We browse the children in order to
        // 1) Set the first prescript, unless (presubscript presuperscript)* is empty.
        // 2) Ensure the validity of the element i.e.
        //   a) That the list of postscripts can be grouped into pairs of subscript/superscript.
        //   b) That the list of prescripts can be grouped into pairs of subscript/superscript.
        //   c) That there is at most one <mprescripts/>.
        bool numberOfScriptIsEven = true;
        for (auto script = base->nextInFlowSiblingBox(); script; script = script->nextInFlowSiblingBox()) {
            if (isPrescriptDelimiter(*script)) {
                // This is a <mprescripts/>. Let's check 2a) and 2c).
                if (!numberOfScriptIsEven || reference.firstPreScript)
                    return std::nullopt;
                reference.firstPreScript = script->nextInFlowSiblingBox(); // We do 1).
                reference.prescriptDelimiter = script;
                continue;
            }
            numberOfScriptIsEven = !numberOfScriptIsEven;
        }
        return numberOfScriptIsEven ? std::optional<ReferenceChildren>(reference) : std::nullopt; // We verify 2b).
    }
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

LayoutUnit RenderMathMLScripts::spaceAfterScript()
{
    const Ref primaryFont = style().fontCascade().primaryFont();
    if (RefPtr mathData = primaryFont->mathData())
        return LayoutUnit(mathData->getMathConstant(primaryFont, OpenTypeMathData::SpaceAfterScript));
    return LayoutUnit(style().fontCascade().size() / 5);
}

LayoutUnit RenderMathMLScripts::italicCorrection(const ReferenceChildren& reference)
{
    if (auto* mathMLBlock = dynamicDowncast<RenderMathMLBlock>(*reference.base)) {
        if (auto* renderOperator = mathMLBlock->unembellishedOperator())
            return renderOperator->italicCorrection();
    }
    return 0;
}

void RenderMathMLScripts::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    auto possibleReference = validateAndGetReferenceChildren();
    if (!possibleReference) {
        RenderMathMLRow::computePreferredLogicalWidths();
        return;
    }
    auto& reference = possibleReference.value();

    LayoutUnit baseItalicCorrection = std::min(reference.base->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*reference.base), italicCorrection(reference));
    LayoutUnit space = spaceAfterScript();

    switch (scriptType()) {
    case MathMLScriptsElement::ScriptType::Sub:
    case MathMLScriptsElement::ScriptType::Under:
        m_maxPreferredLogicalWidth += reference.base->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*reference.base);
        m_maxPreferredLogicalWidth += std::max(0_lu, reference.firstPostScript->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*reference.firstPostScript) - baseItalicCorrection + space);
        break;
    case MathMLScriptsElement::ScriptType::Super:
    case MathMLScriptsElement::ScriptType::Over:
        m_maxPreferredLogicalWidth += reference.base->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*reference.base);
        m_maxPreferredLogicalWidth += std::max(0_lu, reference.firstPostScript->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*reference.firstPostScript) + space);
        break;
    case MathMLScriptsElement::ScriptType::SubSup:
    case MathMLScriptsElement::ScriptType::UnderOver:
    case MathMLScriptsElement::ScriptType::Multiscripts: {
        auto subScript = reference.firstPreScript;
        while (subScript) {
            auto supScript = subScript->nextInFlowSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*subScript), supScript->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*supScript));
            m_maxPreferredLogicalWidth += subSupPairWidth + space;
            subScript = supScript->nextInFlowSiblingBox();
        }
        m_maxPreferredLogicalWidth += reference.base->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*reference.base);
        subScript = reference.firstPostScript;
        while (subScript && subScript != reference.prescriptDelimiter) {
            auto supScript = subScript->nextInFlowSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(std::max(0_lu, subScript->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*subScript) - baseItalicCorrection), supScript->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*supScript));
            m_maxPreferredLogicalWidth += subSupPairWidth + space;
            subScript = supScript->nextInFlowSiblingBox();
        }
    }
    }

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;

    auto sizes = sizeAppliedToMathContent(LayoutPhase::CalculatePreferredLogicalWidth);
    applySizeToMathContent(LayoutPhase::CalculatePreferredLogicalWidth, sizes);

    adjustPreferredLogicalWidthsForBorderAndPadding();

    setPreferredLogicalWidthsDirty(false);
}

auto RenderMathMLScripts::verticalParameters() const -> VerticalParameters
{
    VerticalParameters parameters;
    const Ref primaryFont = style().fontCascade().primaryFont();
    if (RefPtr mathData = primaryFont->mathData()) {
        parameters.subscriptShiftDown = mathData->getMathConstant(primaryFont, OpenTypeMathData::SubscriptShiftDown);
        parameters.superscriptShiftUp = mathData->getMathConstant(primaryFont, OpenTypeMathData::SuperscriptShiftUp);
        parameters.subscriptBaselineDropMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::SubscriptBaselineDropMin);
        parameters.superScriptBaselineDropMax = mathData->getMathConstant(primaryFont, OpenTypeMathData::SuperscriptBaselineDropMax);
        parameters.subSuperscriptGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::SubSuperscriptGapMin);
        parameters.superscriptBottomMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::SuperscriptBottomMin);
        parameters.subscriptTopMax = mathData->getMathConstant(primaryFont, OpenTypeMathData::SubscriptTopMax);
        parameters.superscriptBottomMaxWithSubscript = mathData->getMathConstant(primaryFont, OpenTypeMathData::SuperscriptBottomMaxWithSubscript);
    } else {
        // Default heuristic values when you do not have a font.
        float xHeight = style().metricsOfPrimaryFont().xHeight().value_or(0);
        parameters.subscriptShiftDown = xHeight / 3;
        parameters.superscriptShiftUp = xHeight;
        parameters.subscriptBaselineDropMin = xHeight / 2;
        parameters.superScriptBaselineDropMax = xHeight / 2;
        parameters.subSuperscriptGapMin = style().fontCascade().size() / 5;
        parameters.superscriptBottomMin = xHeight / 4;
        parameters.subscriptTopMax = 4 * xHeight / 5;
        parameters.superscriptBottomMaxWithSubscript = 4 * xHeight / 5;
    }
    return parameters;
}

RenderMathMLScripts::VerticalMetrics RenderMathMLScripts::verticalMetrics(const ReferenceChildren& reference)
{
    VerticalParameters parameters = verticalParameters();
    VerticalMetrics metrics = { 0, 0, 0, 0 };

    LayoutUnit baseAscent = ascentForChild(*reference.base) + reference.base->marginBefore();
    LayoutUnit baseDescent = reference.base->logicalHeight() + reference.base->marginLogicalHeight() - baseAscent;
    if (scriptType() == MathMLScriptsElement::ScriptType::Sub || scriptType() == MathMLScriptsElement::ScriptType::SubSup || scriptType() == MathMLScriptsElement::ScriptType::Multiscripts || scriptType() == MathMLScriptsElement::ScriptType::Under || scriptType() == MathMLScriptsElement::ScriptType::UnderOver) {
        metrics.subShift = std::max(parameters.subscriptShiftDown, baseDescent + parameters.subscriptBaselineDropMin);
        if (!isRenderMathMLUnderOver()) {
            // It is not clear how to interpret the default shift and it is not available yet anyway.
            // Hence we just pass 0 as the default value used by toUserUnits.
            LayoutUnit specifiedMinSubShift = toUserUnits(element().subscriptShift(), style(), 0);
            metrics.subShift = std::max(metrics.subShift, specifiedMinSubShift);
        }
    }
    if (scriptType() == MathMLScriptsElement::ScriptType::Super || scriptType() == MathMLScriptsElement::ScriptType::SubSup || scriptType() == MathMLScriptsElement::ScriptType::Multiscripts  || scriptType() == MathMLScriptsElement::ScriptType::Over || scriptType() == MathMLScriptsElement::ScriptType::UnderOver) {
        metrics.supShift = std::max(parameters.superscriptShiftUp, baseAscent - parameters.superScriptBaselineDropMax);
        if (!isRenderMathMLUnderOver()) {
            // It is not clear how to interpret the default shift and it is not available yet anyway.
            // Hence we just pass 0 as the default value used by toUserUnits.
            LayoutUnit specifiedMinSupShift = toUserUnits(element().superscriptShift(), style(), 0);
            metrics.supShift = std::max(metrics.supShift, specifiedMinSupShift);
        }
    }

    switch (scriptType()) {
    case MathMLScriptsElement::ScriptType::Sub:
    case MathMLScriptsElement::ScriptType::Under: {
        LayoutUnit subAscent = ascentForChild(*reference.firstPostScript) + reference.firstPostScript->marginBefore();
        LayoutUnit subDescent = reference.firstPostScript->logicalHeight() + reference.firstPostScript->marginLogicalHeight() - subAscent;
        metrics.descent = subDescent;
        metrics.subShift = std::max(metrics.subShift, subAscent - parameters.subscriptTopMax);
    }
        break;
    case MathMLScriptsElement::ScriptType::Super:
    case MathMLScriptsElement::ScriptType::Over: {
        LayoutUnit supAscent = ascentForChild(*reference.firstPostScript) + reference.firstPostScript->marginBefore();
        LayoutUnit supDescent = reference.firstPostScript->logicalHeight() + reference.firstPostScript->marginLogicalHeight() - supAscent;
        metrics.ascent = supAscent;
        metrics.supShift = std::max(metrics.supShift, parameters.superscriptBottomMin + supDescent);
    }
        break;
    case MathMLScriptsElement::ScriptType::SubSup:
    case MathMLScriptsElement::ScriptType::UnderOver:
    case MathMLScriptsElement::ScriptType::Multiscripts: {
        // FIXME: We should move the code updating VerticalMetrics for each sub/sup pair in a helper
        // function. That way, SubSup/UnderOver can just make one call and the loop for Multiscripts
        // can be rewritten in a more readable.
        auto subScript = reference.firstPostScript ? reference.firstPostScript : reference.firstPreScript;
        while (subScript) {
            auto supScript = subScript->nextInFlowSiblingBox();
            ASSERT(supScript);
            LayoutUnit subAscent = ascentForChild(*subScript) + subScript->marginBefore();
            LayoutUnit subDescent = subScript->logicalHeight() + subScript->marginLogicalHeight() - subAscent;
            LayoutUnit supAscent = ascentForChild(*supScript) + supScript->marginBefore();
            LayoutUnit supDescent = supScript->logicalHeight() + supScript->marginLogicalHeight() - supAscent;
            metrics.ascent = std::max(metrics.ascent, supAscent);
            metrics.descent = std::max(metrics.descent, subDescent);
            LayoutUnit subScriptShift = std::max(parameters.subscriptShiftDown, baseDescent + parameters.subscriptBaselineDropMin);
            subScriptShift = std::max(subScriptShift, subAscent - parameters.subscriptTopMax);
            LayoutUnit supScriptShift = std::max(parameters.superscriptShiftUp, baseAscent - parameters.superScriptBaselineDropMax);
            supScriptShift = std::max(supScriptShift, parameters.superscriptBottomMin + supDescent);

            LayoutUnit subSuperscriptGap = (subScriptShift - subAscent) + (supScriptShift - supDescent);
            if (subSuperscriptGap < parameters.subSuperscriptGapMin) {
                // First, we try and push the superscript up.
                LayoutUnit delta = parameters.superscriptBottomMaxWithSubscript - (supScriptShift - supDescent);
                if (delta > 0) {
                    delta = std::min(delta, parameters.subSuperscriptGapMin - subSuperscriptGap);
                    supScriptShift += delta;
                    subSuperscriptGap += delta;
                }
                // If that is not enough, we push the subscript down.
                if (subSuperscriptGap < parameters.subSuperscriptGapMin)
                    subScriptShift += parameters.subSuperscriptGapMin - subSuperscriptGap;
            }

            metrics.subShift = std::max(metrics.subShift, subScriptShift);
            metrics.supShift = std::max(metrics.supShift, supScriptShift);

            subScript = supScript->nextInFlowSiblingBox();
            if (subScript == reference.prescriptDelimiter)
                subScript = reference.firstPreScript;
        }
    }
    }

    return metrics;
}

void RenderMathMLScripts::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    insertPositionedChildrenIntoContainingBlock();

    if (!relayoutChildren && simplifiedLayout())
        return;

    auto possibleReference = validateAndGetReferenceChildren();
    if (!possibleReference) {
        RenderMathMLRow::layoutBlock(relayoutChildren);
        return;
    }
    auto& reference = possibleReference.value();

    layoutFloatingChildren();

    recomputeLogicalWidth();
    computeAndSetBlockDirectionMarginsOfChildren();

    for (auto* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox())
        child->layoutIfNeeded();

    LayoutUnit space = spaceAfterScript();

    // We determine the minimal shift/size of each script and take the maximum of the values.
    VerticalMetrics metrics = verticalMetrics(reference);

    LayoutUnit baseAscent = ascentForChild(*reference.base) + reference.base->marginBefore();
    LayoutUnit baseDescent = reference.base->logicalHeight() + reference.base->marginLogicalHeight() - baseAscent;
    LayoutUnit baseItalicCorrection = std::min(reference.base->logicalWidth() + reference.base->marginLogicalWidth(), italicCorrection(reference));
    LayoutUnit horizontalOffset = 0;

    LayoutUnit ascent = std::max(baseAscent, metrics.ascent + metrics.supShift);
    LayoutUnit descent = std::max(baseDescent, metrics.descent + metrics.subShift);
    setLogicalHeight(ascent + descent);

    switch (scriptType()) {
    case MathMLScriptsElement::ScriptType::Sub:
    case MathMLScriptsElement::ScriptType::Under: {
        LayoutUnit baseWidth = reference.base->logicalWidth() + reference.base->marginLogicalWidth();
        LayoutUnit contentWidth = baseWidth + std::max(0_lu, reference.firstPostScript->logicalWidth() + reference.firstPostScript->marginLogicalWidth() - baseItalicCorrection + space);
        setLogicalWidth(contentWidth);
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset + reference.base->marginStart(), *reference.base), ascent - baseAscent + reference.base->marginBefore());
        reference.base->setLocation(baseLocation);
        horizontalOffset += baseWidth;
        LayoutUnit scriptAscent = ascentForChild(*reference.firstPostScript);
        LayoutPoint scriptLocation(mirrorIfNeeded(horizontalOffset - baseItalicCorrection + reference.firstPostScript->marginStart(), *reference.firstPostScript), ascent + metrics.subShift - scriptAscent);
        reference.firstPostScript->setLocation(scriptLocation);
    }
        break;
    case MathMLScriptsElement::ScriptType::Super:
    case MathMLScriptsElement::ScriptType::Over: {
        LayoutUnit baseWidth = reference.base->logicalWidth() + reference.base->marginLogicalWidth();
        LayoutUnit contentWidth = baseWidth + std::max(0_lu, reference.firstPostScript->logicalWidth() + reference.firstPostScript->marginLogicalWidth() + space);
        setLogicalWidth(contentWidth);
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset + reference.base->marginStart(), *reference.base), ascent - baseAscent + reference.base->marginBefore());
        reference.base->setLocation(baseLocation);
        horizontalOffset += baseWidth;
        LayoutUnit scriptAscent = ascentForChild(*reference.firstPostScript);
        LayoutPoint scriptLocation(mirrorIfNeeded(horizontalOffset + reference.firstPostScript->marginStart(), *reference.firstPostScript), ascent - metrics.supShift - scriptAscent);
        reference.firstPostScript->setLocation(scriptLocation);
    }
        break;
    case MathMLScriptsElement::ScriptType::SubSup:
    case MathMLScriptsElement::ScriptType::UnderOver:
    case MathMLScriptsElement::ScriptType::Multiscripts: {
        // Calculate the logical width.
        LayoutUnit logicalWidth = 0;
        auto subScript = reference.firstPreScript;
        while (subScript) {
            auto supScript = subScript->nextInFlowSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth() + subScript->marginLogicalWidth(), supScript->logicalWidth() + supScript->marginLogicalWidth());
            logicalWidth += subSupPairWidth + space;
            subScript = supScript->nextInFlowSiblingBox();
        }
        logicalWidth += reference.base->logicalWidth() + reference.base->marginLogicalWidth();
        subScript = reference.firstPostScript;
        while (subScript && subScript != reference.prescriptDelimiter) {
            auto supScript = subScript->nextInFlowSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(std::max(0_lu, subScript->logicalWidth() + subScript->marginLogicalWidth() - baseItalicCorrection), supScript->logicalWidth() + supScript->marginLogicalWidth());
            logicalWidth += subSupPairWidth + space;
            subScript = supScript->nextInFlowSiblingBox();
        }
        setLogicalWidth(logicalWidth);

        subScript = reference.firstPreScript;
        while (subScript) {
            auto supScript = subScript->nextInFlowSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth() + subScript->marginLogicalWidth(), supScript->logicalWidth() + supScript->marginLogicalWidth());
            horizontalOffset += space + subSupPairWidth;
            LayoutUnit subAscent = ascentForChild(*subScript);
            LayoutPoint subScriptLocation(mirrorIfNeeded(horizontalOffset - subScript->marginEnd() - subScript->logicalWidth(), *subScript), ascent + metrics.subShift - subAscent);
            subScript->setLocation(subScriptLocation);
            LayoutUnit supAscent = ascentForChild(*supScript);
            LayoutPoint supScriptLocation(mirrorIfNeeded(horizontalOffset - supScript->marginEnd() - supScript->logicalWidth(), *supScript), ascent - metrics.supShift - supAscent);
            supScript->setLocation(supScriptLocation);
            subScript = supScript->nextInFlowSiblingBox();
        }
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset + reference.base->marginStart(), *reference.base), ascent - baseAscent + reference.base->marginBefore());
        reference.base->setLocation(baseLocation);
        horizontalOffset += reference.base->logicalWidth() + reference.base->marginLogicalWidth();
        subScript = reference.firstPostScript;
        while (subScript && subScript != reference.prescriptDelimiter) {
            auto supScript = subScript->nextInFlowSiblingBox();
            ASSERT(supScript);
            LayoutUnit subAscent = ascentForChild(*subScript);
            LayoutPoint subScriptLocation(mirrorIfNeeded(horizontalOffset - baseItalicCorrection + subScript->marginStart(), *subScript), ascent + metrics.subShift - subAscent);
            subScript->setLocation(subScriptLocation);
            LayoutUnit supAscent = ascentForChild(*supScript);
            LayoutPoint supScriptLocation(mirrorIfNeeded(horizontalOffset + supScript->marginStart(), *supScript), ascent - metrics.supShift - supAscent);
            supScript->setLocation(supScriptLocation);

            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth() + subScript->marginLogicalWidth(), supScript->logicalWidth() + supScript->marginLogicalWidth());
            horizontalOffset += subSupPairWidth + space;
            subScript = supScript->nextInFlowSiblingBox();
        }
    }
    }

    auto sizes = sizeAppliedToMathContent(LayoutPhase::Layout);
    auto shift = applySizeToMathContent(LayoutPhase::Layout, sizes);
    shiftInFlowChildren(shift, 0);

    adjustLayoutForBorderAndPadding();

    layoutPositionedObjects(relayoutChildren);

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

std::optional<LayoutUnit> RenderMathMLScripts::firstLineBaseline() const
{
    auto possibleReference = validateAndGetReferenceChildren();
    if (!possibleReference)
        return RenderMathMLRow::firstLineBaseline();

    auto& base = *possibleReference.value().base;
    return LayoutUnit { roundf(ascentForChild(base) + base.marginBefore() + base.logicalTop()) };
}

}

#endif // ENABLE(MATHML)
