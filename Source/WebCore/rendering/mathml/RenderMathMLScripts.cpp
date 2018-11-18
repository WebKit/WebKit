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
#include "RenderMathMLOperator.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLScripts);

static bool isPrescriptDelimiter(const RenderObject& renderObject)
{
    return renderObject.node() && renderObject.node()->hasTagName(MathMLNames::mprescriptsTag);
}

RenderMathMLScripts::RenderMathMLScripts(MathMLScriptsElement& element, RenderStyle&& style)
    : RenderMathMLBlock(element, WTFMove(style))
{
}

MathMLScriptsElement& RenderMathMLScripts::element() const
{
    return static_cast<MathMLScriptsElement&>(nodeForNonAnonymous());
}

ScriptType RenderMathMLScripts::scriptType() const
{
    return element().scriptType();
}

RenderMathMLOperator* RenderMathMLScripts::unembellishedOperator() const
{
    auto base = firstChildBox();
    if (!is<RenderMathMLBlock>(base))
        return nullptr;
    return downcast<RenderMathMLBlock>(base)->unembellishedOperator();
}

std::optional<RenderMathMLScripts::ReferenceChildren> RenderMathMLScripts::validateAndGetReferenceChildren()
{
    // All scripted elements must have at least one child.
    // The first child is the base.
    auto base = firstChildBox();
    if (!base)
        return std::nullopt;

    ReferenceChildren reference;
    reference.base = base;
    reference.firstPostScript = nullptr;
    reference.firstPreScript = nullptr;
    reference.prescriptDelimiter = nullptr;

    switch (scriptType()) {
    case ScriptType::Sub:
    case ScriptType::Super:
    case ScriptType::Under:
    case ScriptType::Over: {
        // These elements must have exactly two children.
        // The second child is a postscript and there are no prescripts.
        // <msub> base subscript </msub>
        // <msup> base superscript </msup>
        // <munder> base underscript </munder>
        // <mover> base overscript </mover>
        auto script = base->nextSiblingBox();
        if (!script || isPrescriptDelimiter(*script) || script->nextSiblingBox())
            return std::nullopt;
        reference.firstPostScript = script;
        return reference;
    }
    case ScriptType::SubSup:
    case ScriptType::UnderOver: {
        // These elements must have exactly three children.
        // The second and third children are postscripts and there are no prescripts.
        // <msubsup> base subscript superscript </msubsup>
        // <munderover> base subscript superscript </munderover>
        auto subScript = base->nextSiblingBox();
        if (!subScript || isPrescriptDelimiter(*subScript))
            return std::nullopt;
        auto superScript = subScript->nextSiblingBox();
        if (!superScript || isPrescriptDelimiter(*superScript) || superScript->nextSiblingBox())
            return std::nullopt;
        reference.firstPostScript = subScript;
        return reference;
    }
    case ScriptType::Multiscripts: {
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
        // We set the first postscript, unless (subscript superscript)* is empty.
        if (base->nextSiblingBox() && !isPrescriptDelimiter(*base->nextSiblingBox()))
            reference.firstPostScript = base->nextSiblingBox();

        // We browse the children in order to
        // 1) Set the first prescript, unless (presubscript presuperscript)* is empty.
        // 2) Ensure the validity of the element i.e.
        //   a) That the list of postscripts can be grouped into pairs of subscript/superscript.
        //   b) That the list of prescripts can be grouped into pairs of subscript/superscript.
        //   c) That there is at most one <mprescripts/>.
        bool numberOfScriptIsEven = true;
        for (auto script = base->nextSiblingBox(); script; script = script->nextSiblingBox()) {
            if (isPrescriptDelimiter(*script)) {
                // This is a <mprescripts/>. Let's check 2a) and 2c).
                if (!numberOfScriptIsEven || reference.firstPreScript)
                    return std::nullopt;
                reference.firstPreScript = script->nextSiblingBox(); // We do 1).
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
    const auto& primaryFont = style().fontCascade().primaryFont();
    if (auto* mathData = primaryFont.mathData())
        return mathData->getMathConstant(primaryFont, OpenTypeMathData::SpaceAfterScript);
    return style().fontCascade().size() / 5;
}

LayoutUnit RenderMathMLScripts::italicCorrection(const ReferenceChildren& reference)
{
    if (is<RenderMathMLBlock>(*reference.base)) {
        if (auto* renderOperator = downcast<RenderMathMLBlock>(*reference.base).unembellishedOperator())
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
        setPreferredLogicalWidthsDirty(false);
        return;
    }
    auto& reference = possibleReference.value();

    LayoutUnit baseItalicCorrection = std::min(reference.base->maxPreferredLogicalWidth(), italicCorrection(reference));
    LayoutUnit space = spaceAfterScript();

    switch (scriptType()) {
    case ScriptType::Sub:
    case ScriptType::Under:
        m_maxPreferredLogicalWidth += reference.base->maxPreferredLogicalWidth();
        m_maxPreferredLogicalWidth += std::max(LayoutUnit(0), reference.firstPostScript->maxPreferredLogicalWidth() - baseItalicCorrection + space);
        break;
    case ScriptType::Super:
    case ScriptType::Over:
        m_maxPreferredLogicalWidth += reference.base->maxPreferredLogicalWidth();
        m_maxPreferredLogicalWidth += std::max(LayoutUnit(0), reference.firstPostScript->maxPreferredLogicalWidth() + space);
        break;
    case ScriptType::SubSup:
    case ScriptType::UnderOver:
    case ScriptType::Multiscripts: {
        auto subScript = reference.firstPreScript;
        while (subScript) {
            auto supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->maxPreferredLogicalWidth(), supScript->maxPreferredLogicalWidth());
            m_maxPreferredLogicalWidth += subSupPairWidth + space;
            subScript = supScript->nextSiblingBox();
        }
        m_maxPreferredLogicalWidth += reference.base->maxPreferredLogicalWidth();
        subScript = reference.firstPostScript;
        while (subScript && subScript != reference.prescriptDelimiter) {
            auto supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(std::max(LayoutUnit(0), subScript->maxPreferredLogicalWidth() - baseItalicCorrection), supScript->maxPreferredLogicalWidth());
            m_maxPreferredLogicalWidth += subSupPairWidth + space;
            subScript = supScript->nextSiblingBox();
        }
    }
    }

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;

    setPreferredLogicalWidthsDirty(false);
}

auto RenderMathMLScripts::verticalParameters() const -> VerticalParameters
{
    VerticalParameters parameters;
    const auto& primaryFont = style().fontCascade().primaryFont();
    if (auto* mathData = primaryFont.mathData()) {
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
        parameters.subscriptShiftDown = style().fontMetrics().xHeight() / 3;
        parameters.superscriptShiftUp = style().fontMetrics().xHeight();
        parameters.subscriptBaselineDropMin = style().fontMetrics().xHeight() / 2;
        parameters.superScriptBaselineDropMax = style().fontMetrics().xHeight() / 2;
        parameters.subSuperscriptGapMin = style().fontCascade().size() / 5;
        parameters.superscriptBottomMin = style().fontMetrics().xHeight() / 4;
        parameters.subscriptTopMax = 4 * style().fontMetrics().xHeight() / 5;
        parameters.superscriptBottomMaxWithSubscript = 4 * style().fontMetrics().xHeight() / 5;
    }
    return parameters;
}

RenderMathMLScripts::VerticalMetrics RenderMathMLScripts::verticalMetrics(const ReferenceChildren& reference)
{
    VerticalParameters parameters = verticalParameters();
    VerticalMetrics metrics = { 0, 0, 0, 0 };

    LayoutUnit baseAscent = ascentForChild(*reference.base);
    LayoutUnit baseDescent = reference.base->logicalHeight() - baseAscent;
    if (scriptType() == ScriptType::Sub || scriptType() == ScriptType::SubSup || scriptType() == ScriptType::Multiscripts || scriptType() == ScriptType::Under || scriptType() == ScriptType::UnderOver) {
        metrics.subShift = std::max(parameters.subscriptShiftDown, baseDescent + parameters.subscriptBaselineDropMin);
        if (!isRenderMathMLUnderOver()) {
            // It is not clear how to interpret the default shift and it is not available yet anyway.
            // Hence we just pass 0 as the default value used by toUserUnits.
            LayoutUnit specifiedMinSubShift = toUserUnits(element().subscriptShift(), style(), 0);
            metrics.subShift = std::max(metrics.subShift, specifiedMinSubShift);
        }
    }
    if (scriptType() == ScriptType::Super || scriptType() == ScriptType::SubSup || scriptType() == ScriptType::Multiscripts  || scriptType() == ScriptType::Over || scriptType() == ScriptType::UnderOver) {
        metrics.supShift = std::max(parameters.superscriptShiftUp, baseAscent - parameters.superScriptBaselineDropMax);
        if (!isRenderMathMLUnderOver()) {
            // It is not clear how to interpret the default shift and it is not available yet anyway.
            // Hence we just pass 0 as the default value used by toUserUnits.
            LayoutUnit specifiedMinSupShift = toUserUnits(element().superscriptShift(), style(), 0);
            metrics.supShift = std::max(metrics.supShift, specifiedMinSupShift);
        }
    }

    switch (scriptType()) {
    case ScriptType::Sub:
    case ScriptType::Under: {
        LayoutUnit subAscent = ascentForChild(*reference.firstPostScript);
        LayoutUnit subDescent = reference.firstPostScript->logicalHeight() - subAscent;
        metrics.descent = subDescent;
        metrics.subShift = std::max(metrics.subShift, subAscent - parameters.subscriptTopMax);
    }
        break;
    case ScriptType::Super:
    case ScriptType::Over: {
        LayoutUnit supAscent = ascentForChild(*reference.firstPostScript);
        LayoutUnit supDescent = reference.firstPostScript->logicalHeight() - supAscent;
        metrics.ascent = supAscent;
        metrics.supShift = std::max(metrics.supShift, parameters.superscriptBottomMin + supDescent);
    }
        break;
    case ScriptType::SubSup:
    case ScriptType::UnderOver:
    case ScriptType::Multiscripts: {
        // FIXME: We should move the code updating VerticalMetrics for each sub/sup pair in a helper
        // function. That way, SubSup/UnderOver can just make one call and the loop for Multiscripts
        // can be rewritten in a more readable.
        auto subScript = reference.firstPostScript ? reference.firstPostScript : reference.firstPreScript;
        while (subScript) {
            auto supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subAscent = ascentForChild(*subScript);
            LayoutUnit subDescent = subScript->logicalHeight() - subAscent;
            LayoutUnit supAscent = ascentForChild(*supScript);
            LayoutUnit supDescent = supScript->logicalHeight() - supAscent;
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

            subScript = supScript->nextSiblingBox();
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

    if (!relayoutChildren && simplifiedLayout())
        return;

    auto possibleReference = validateAndGetReferenceChildren();
    if (!possibleReference) {
        layoutInvalidMarkup(relayoutChildren);
        return;
    }
    auto& reference = possibleReference.value();

    recomputeLogicalWidth();
    for (auto child = firstChildBox(); child; child = child->nextSiblingBox())
        child->layoutIfNeeded();

    LayoutUnit space = spaceAfterScript();

    // We determine the minimal shift/size of each script and take the maximum of the values.
    VerticalMetrics metrics = verticalMetrics(reference);

    LayoutUnit baseAscent = ascentForChild(*reference.base);
    LayoutUnit baseDescent = reference.base->logicalHeight() - baseAscent;
    LayoutUnit baseItalicCorrection = std::min(reference.base->logicalWidth(), italicCorrection(reference));
    LayoutUnit horizontalOffset;

    LayoutUnit ascent = std::max(baseAscent, metrics.ascent + metrics.supShift);
    LayoutUnit descent = std::max(baseDescent, metrics.descent + metrics.subShift);
    setLogicalHeight(ascent + descent);

    switch (scriptType()) {
    case ScriptType::Sub:
    case ScriptType::Under: {
        setLogicalWidth(reference.base->logicalWidth() + std::max(LayoutUnit(0), reference.firstPostScript->logicalWidth() - baseItalicCorrection + space));
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset, *reference.base), ascent - baseAscent);
        reference.base->setLocation(baseLocation);
        horizontalOffset += reference.base->logicalWidth();
        LayoutUnit scriptAscent = ascentForChild(*reference.firstPostScript);
        LayoutPoint scriptLocation(mirrorIfNeeded(horizontalOffset - baseItalicCorrection, *reference.firstPostScript), ascent + metrics.subShift - scriptAscent);
        reference.firstPostScript->setLocation(scriptLocation);
    }
        break;
    case ScriptType::Super:
    case ScriptType::Over: {
        setLogicalWidth(reference.base->logicalWidth() + std::max(LayoutUnit(0), reference.firstPostScript->logicalWidth() + space));
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset, *reference.base), ascent - baseAscent);
        reference.base->setLocation(baseLocation);
        horizontalOffset += reference.base->logicalWidth();
        LayoutUnit scriptAscent = ascentForChild(*reference.firstPostScript);
        LayoutPoint scriptLocation(mirrorIfNeeded(horizontalOffset, *reference.firstPostScript), ascent - metrics.supShift - scriptAscent);
        reference.firstPostScript->setLocation(scriptLocation);
    }
        break;
    case ScriptType::SubSup:
    case ScriptType::UnderOver:
    case ScriptType::Multiscripts: {
        // Calculate the logical width.
        LayoutUnit logicalWidth;
        auto subScript = reference.firstPreScript;
        while (subScript) {
            auto supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth(), supScript->logicalWidth());
            logicalWidth += subSupPairWidth + space;
            subScript = supScript->nextSiblingBox();
        }
        logicalWidth += reference.base->logicalWidth();
        subScript = reference.firstPostScript;
        while (subScript && subScript != reference.prescriptDelimiter) {
            auto supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(std::max(LayoutUnit(0), subScript->logicalWidth() - baseItalicCorrection), supScript->logicalWidth());
            logicalWidth += subSupPairWidth + space;
            subScript = supScript->nextSiblingBox();
        }
        setLogicalWidth(logicalWidth);

        subScript = reference.firstPreScript;
        while (subScript) {
            auto supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth(), supScript->logicalWidth());
            horizontalOffset += space + subSupPairWidth;
            LayoutUnit subAscent = ascentForChild(*subScript);
            LayoutPoint subScriptLocation(mirrorIfNeeded(horizontalOffset - subScript->logicalWidth(), *subScript), ascent + metrics.subShift - subAscent);
            subScript->setLocation(subScriptLocation);
            LayoutUnit supAscent = ascentForChild(*supScript);
            LayoutPoint supScriptLocation(mirrorIfNeeded(horizontalOffset - supScript->logicalWidth(), *supScript), ascent - metrics.supShift - supAscent);
            supScript->setLocation(supScriptLocation);
            subScript = supScript->nextSiblingBox();
        }
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset, *reference.base), ascent - baseAscent);
        reference.base->setLocation(baseLocation);
        horizontalOffset += reference.base->logicalWidth();
        subScript = reference.firstPostScript;
        while (subScript && subScript != reference.prescriptDelimiter) {
            auto supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subAscent = ascentForChild(*subScript);
            LayoutPoint subScriptLocation(mirrorIfNeeded(horizontalOffset - baseItalicCorrection, *subScript), ascent + metrics.subShift - subAscent);
            subScript->setLocation(subScriptLocation);
            LayoutUnit supAscent = ascentForChild(*supScript);
            LayoutPoint supScriptLocation(mirrorIfNeeded(horizontalOffset, *supScript), ascent - metrics.supShift - supAscent);
            supScript->setLocation(supScriptLocation);

            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth(), supScript->logicalWidth());
            horizontalOffset += subSupPairWidth + space;
            subScript = supScript->nextSiblingBox();
        }
    }
    }

    layoutPositionedObjects(relayoutChildren);

    clearNeedsLayout();
}

std::optional<int> RenderMathMLScripts::firstLineBaseline() const
{
    auto* base = firstChildBox();
    if (!base)
        return std::optional<int>();
    return std::optional<int>(static_cast<int>(lroundf(ascentForChild(*base) + base->logicalTop())));
}

}

#endif // ENABLE(MATHML)
