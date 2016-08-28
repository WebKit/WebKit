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

namespace WebCore {

static bool isPrescriptDelimiter(const RenderObject& renderObject)
{
    return renderObject.node() && renderObject.node()->hasTagName(MathMLNames::mprescriptsTag);
}

RenderMathMLScripts::RenderMathMLScripts(MathMLScriptsElement& element, RenderStyle&& style)
    : RenderMathMLBlock(element, WTFMove(style))
{
    // Determine what kind of sub/sup expression we have by element name
    if (element.hasTagName(MathMLNames::msubTag))
        m_scriptType = Sub;
    else if (element.hasTagName(MathMLNames::msupTag))
        m_scriptType = Super;
    else if (element.hasTagName(MathMLNames::msubsupTag))
        m_scriptType = SubSup;
    else if (element.hasTagName(MathMLNames::munderTag))
        m_scriptType = Under;
    else if (element.hasTagName(MathMLNames::moverTag))
        m_scriptType = Over;
    else if (element.hasTagName(MathMLNames::munderoverTag))
        m_scriptType = UnderOver;
    else {
        ASSERT(element.hasTagName(MathMLNames::mmultiscriptsTag));
        m_scriptType = Multiscripts;
    }
}

MathMLScriptsElement& RenderMathMLScripts::element() const
{
    return static_cast<MathMLScriptsElement&>(nodeForNonAnonymous());
}

RenderMathMLOperator* RenderMathMLScripts::unembellishedOperator()
{
    auto base = firstChildBox();
    if (!is<RenderMathMLBlock>(base))
        return nullptr;
    return downcast<RenderMathMLBlock>(base)->unembellishedOperator();
}

bool RenderMathMLScripts::getBaseAndScripts(RenderBox*& base, RenderBox*& firstPostScript, RenderBox*& firstPreScript)
{
    // All scripted elements must have at least one child.
    // The first child is the base.
    base = firstChildBox();
    firstPostScript = nullptr;
    firstPreScript = nullptr;
    if (!base)
        return false;

    switch (m_scriptType) {
    case Sub:
    case Super:
    case Under:
    case Over:
        // These elements must have exactly two children.
        // The second child is a postscript and there are no prescripts.
        // <msub> base subscript </msub>
        // <msup> base superscript </msup>
        // <munder> base underscript </munder>
        // <mover> base overscript </mover>
        firstPostScript = base->nextSiblingBox();
        return firstPostScript && !isPrescriptDelimiter(*firstPostScript) && !firstPostScript->nextSiblingBox();
    case SubSup:
    case UnderOver: {
        // These elements must have exactly three children.
        // The second and third children are postscripts and there are no prescripts.
        // <msubsup> base subscript superscript </msubsup>
        // <munderover> base subscript superscript </munderover>
        firstPostScript = base->nextSiblingBox();
        if (!firstPostScript || isPrescriptDelimiter(*firstPostScript))
            return false;
        auto superScript = firstPostScript->nextSiblingBox();
        return superScript && !isPrescriptDelimiter(*superScript) && !superScript->nextSiblingBox();
    }
    case Multiscripts: {
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
            firstPostScript = base->nextSiblingBox();

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
                if (!numberOfScriptIsEven || firstPreScript)
                    return false;
                firstPreScript = script->nextSiblingBox(); // We do 1).
                continue;
            }
            numberOfScriptIsEven = !numberOfScriptIsEven;
        }
        return numberOfScriptIsEven; // We verify 2b).
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

LayoutUnit RenderMathMLScripts::spaceAfterScript()
{
    const auto& primaryFont = style().fontCascade().primaryFont();
    if (auto* mathData = primaryFont.mathData())
        return mathData->getMathConstant(primaryFont, OpenTypeMathData::SpaceAfterScript);
    return style().fontCascade().size() / 5;
}

LayoutUnit RenderMathMLScripts::italicCorrection(RenderBox* base)
{
    if (is<RenderMathMLBlock>(*base)) {
        if (auto* renderOperator = downcast<RenderMathMLBlock>(*base).unembellishedOperator())
            return renderOperator->italicCorrection();
    }
    return 0;
}

void RenderMathMLScripts::computePreferredLogicalWidths()
{
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    RenderBox* base;
    RenderBox* firstPostScript;
    RenderBox* firstPreScript;
    if (!getBaseAndScripts(base, firstPostScript, firstPreScript))
        return;

    LayoutUnit baseItalicCorrection = std::min(base->maxPreferredLogicalWidth(), italicCorrection(base));
    LayoutUnit space = spaceAfterScript();

    switch (m_scriptType) {
    case Sub:
    case Under:
        m_maxPreferredLogicalWidth += base->maxPreferredLogicalWidth();
        m_maxPreferredLogicalWidth += std::max(LayoutUnit(0), firstPostScript->maxPreferredLogicalWidth() - baseItalicCorrection + space);
        break;
    case Super:
    case Over:
        m_maxPreferredLogicalWidth += base->maxPreferredLogicalWidth();
        m_maxPreferredLogicalWidth += std::max(LayoutUnit(0), firstPostScript->maxPreferredLogicalWidth() + space);
        break;
    case SubSup:
    case UnderOver:
    case Multiscripts: {
        RenderBox* supScript;
        for (auto* subScript = firstPreScript; subScript; subScript = supScript->nextSiblingBox()) {
            supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->maxPreferredLogicalWidth(), supScript->maxPreferredLogicalWidth());
            m_maxPreferredLogicalWidth += subSupPairWidth + space;
        }
        m_maxPreferredLogicalWidth += base->maxPreferredLogicalWidth();
        for (auto* subScript = firstPostScript; subScript && !isPrescriptDelimiter(*subScript); subScript = supScript->nextSiblingBox()) {
            supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(std::max(LayoutUnit(0), subScript->maxPreferredLogicalWidth() - baseItalicCorrection), supScript->maxPreferredLogicalWidth());
            m_maxPreferredLogicalWidth += subSupPairWidth + space;
        }
    }
    }

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;
}

void RenderMathMLScripts::getScriptMetricsAndLayoutIfNeeded(RenderBox* base, RenderBox* script, ScriptMetrics& metrics)
{
    LayoutUnit baseAscent = ascentForChild(*base);
    LayoutUnit baseDescent = base->logicalHeight() - baseAscent;
    LayoutUnit subscriptShiftDown;
    LayoutUnit superscriptShiftUp;
    LayoutUnit subscriptBaselineDropMin;
    LayoutUnit superScriptBaselineDropMax;
    LayoutUnit subSuperscriptGapMin;
    LayoutUnit superscriptBottomMin;
    LayoutUnit subscriptTopMax;
    LayoutUnit superscriptBottomMaxWithSubscript;

    const auto& primaryFont = style().fontCascade().primaryFont();
    if (auto* mathData = primaryFont.mathData()) {
        subscriptShiftDown = mathData->getMathConstant(primaryFont, OpenTypeMathData::SubscriptShiftDown);
        superscriptShiftUp = mathData->getMathConstant(primaryFont, OpenTypeMathData::SuperscriptShiftUp);
        subscriptBaselineDropMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::SubscriptBaselineDropMin);
        superScriptBaselineDropMax = mathData->getMathConstant(primaryFont, OpenTypeMathData::SuperscriptBaselineDropMax);
        subSuperscriptGapMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::SubSuperscriptGapMin);
        superscriptBottomMin = mathData->getMathConstant(primaryFont, OpenTypeMathData::SuperscriptBottomMin);
        subscriptTopMax = mathData->getMathConstant(primaryFont, OpenTypeMathData::SubscriptTopMax);
        superscriptBottomMaxWithSubscript = mathData->getMathConstant(primaryFont, OpenTypeMathData::SuperscriptBottomMaxWithSubscript);
    } else {
        // Default heuristic values when you do not have a font.
        subscriptShiftDown = style().fontMetrics().xHeight() / 3;
        superscriptShiftUp = style().fontMetrics().xHeight();
        subscriptBaselineDropMin = style().fontMetrics().xHeight() / 2;
        superScriptBaselineDropMax = style().fontMetrics().xHeight() / 2;
        subSuperscriptGapMin = style().fontCascade().size() / 5;
        superscriptBottomMin = style().fontMetrics().xHeight() / 4;
        subscriptTopMax = 4 * style().fontMetrics().xHeight() / 5;
        superscriptBottomMaxWithSubscript = 4 * style().fontMetrics().xHeight() / 5;
    }

    if (m_scriptType == Sub || m_scriptType == SubSup || m_scriptType == Multiscripts || m_scriptType == Under || m_scriptType == UnderOver) {
        metrics.subShift = std::max(subscriptShiftDown, baseDescent + subscriptBaselineDropMin);
        if (!isRenderMathMLUnderOver()) {
            // It is not clear how to interpret the default shift and it is not available yet anyway.
            // Hence we just pass 0 as the default value used by toUserUnits.
            LayoutUnit specifiedMinSubShift = toUserUnits(element().subscriptShift(), style(), 0);
            metrics.subShift = std::max(metrics.subShift, specifiedMinSubShift);
        }
    }
    if (m_scriptType == Super || m_scriptType == SubSup || m_scriptType == Multiscripts  || m_scriptType == Over || m_scriptType == UnderOver) {
        metrics.supShift = std::max(superscriptShiftUp, baseAscent - superScriptBaselineDropMax);
        if (!isRenderMathMLUnderOver()) {
            // It is not clear how to interpret the default shift and it is not available yet anyway.
            // Hence we just pass 0 as the default value used by toUserUnits.
            LayoutUnit specifiedMinSupShift = toUserUnits(element().superscriptShift(), style(), 0);
            metrics.supShift = std::max(metrics.supShift, specifiedMinSupShift);
        }
    }

    switch (m_scriptType) {
    case Sub:
    case Under: {
        script->layoutIfNeeded();
        LayoutUnit subAscent = ascentForChild(*script);
        LayoutUnit subDescent = script->logicalHeight() - subAscent;
        metrics.descent = subDescent;
        metrics.subShift = std::max(metrics.subShift, subAscent - subscriptTopMax);
    }
        break;
    case Super:
    case Over: {
        script->layoutIfNeeded();
        LayoutUnit supAscent = ascentForChild(*script);
        LayoutUnit supDescent = script->logicalHeight() - supAscent;
        metrics.ascent = supAscent;
        metrics.supShift = std::max(metrics.supShift, superscriptBottomMin + supDescent);
    }
        break;
    case SubSup:
    case UnderOver:
    case Multiscripts: {
        RenderBox* supScript;
        for (auto* subScript = script; subScript && !isPrescriptDelimiter(*subScript); subScript = supScript->nextSiblingBox()) {
            supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            subScript->layoutIfNeeded();
            supScript->layoutIfNeeded();
            LayoutUnit subAscent = ascentForChild(*subScript);
            LayoutUnit subDescent = subScript->logicalHeight() - subAscent;
            LayoutUnit supAscent = ascentForChild(*supScript);
            LayoutUnit supDescent = supScript->logicalHeight() - supAscent;
            metrics.ascent = std::max(metrics.ascent, supAscent);
            metrics.descent = std::max(metrics.descent, subDescent);
            LayoutUnit subScriptShift = std::max(subscriptShiftDown, baseDescent + subscriptBaselineDropMin);
            subScriptShift = std::max(subScriptShift, subAscent - subscriptTopMax);
            LayoutUnit supScriptShift = std::max(superscriptShiftUp, baseAscent - superScriptBaselineDropMax);
            supScriptShift = std::max(supScriptShift, superscriptBottomMin + supDescent);

            LayoutUnit subSuperscriptGap = (subScriptShift - subAscent) + (supScriptShift - supDescent);
            if (subSuperscriptGap < subSuperscriptGapMin) {
                // First, we try and push the superscript up.
                LayoutUnit delta = superscriptBottomMaxWithSubscript - (supScriptShift - supDescent);
                if (delta > 0) {
                    delta = std::min(delta, subSuperscriptGapMin - subSuperscriptGap);
                    supScriptShift += delta;
                    subSuperscriptGap += delta;
                }
                // If that is not enough, we push the subscript down.
                if (subSuperscriptGap < subSuperscriptGapMin)
                    subScriptShift += subSuperscriptGapMin - subSuperscriptGap;
            }

            metrics.subShift = std::max(metrics.subShift, subScriptShift);
            metrics.supShift = std::max(metrics.supShift, supScriptShift);
        }
    }
    }
}

void RenderMathMLScripts::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    RenderBox* base;
    RenderBox* firstPostScript;
    RenderBox* firstPreScript;
    if (!getBaseAndScripts(base, firstPostScript, firstPreScript)) {
        setLogicalWidth(0);
        setLogicalHeight(0);
        clearNeedsLayout();
        return;
    }
    recomputeLogicalWidth();
    base->layoutIfNeeded();

    LayoutUnit space = spaceAfterScript();

    // We determine the minimal shift/size of each script and take the maximum of the values.
    ScriptMetrics metrics;
    metrics.subShift = 0;
    metrics.supShift = 0;
    metrics.descent = 0;
    metrics.ascent = 0;
    if (m_scriptType == Multiscripts)
        getScriptMetricsAndLayoutIfNeeded(base, firstPreScript, metrics);
    getScriptMetricsAndLayoutIfNeeded(base, firstPostScript, metrics);

    LayoutUnit baseAscent = ascentForChild(*base);
    LayoutUnit baseDescent = base->logicalHeight() - baseAscent;
    LayoutUnit baseItalicCorrection = std::min(base->logicalWidth(), italicCorrection(base));
    LayoutUnit horizontalOffset = 0;

    LayoutUnit ascent = std::max(baseAscent, metrics.ascent + metrics.supShift);
    LayoutUnit descent = std::max(baseDescent, metrics.descent + metrics.subShift);
    setLogicalHeight(ascent + descent);

    switch (m_scriptType) {
    case Sub:
    case Under: {
        setLogicalWidth(base->logicalWidth() + std::max(LayoutUnit(0), firstPostScript->logicalWidth() - baseItalicCorrection + space));
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset, *base), ascent - baseAscent);
        base->setLocation(baseLocation);
        horizontalOffset += base->logicalWidth();
        LayoutUnit scriptAscent = ascentForChild(*firstPostScript);
        LayoutPoint scriptLocation(mirrorIfNeeded(horizontalOffset - baseItalicCorrection, *firstPostScript), ascent + metrics.subShift - scriptAscent);
        firstPostScript->setLocation(scriptLocation);
    }
        break;
    case Super:
    case Over: {
        setLogicalWidth(base->logicalWidth() + std::max(LayoutUnit(0), firstPostScript->logicalWidth() + space));
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset, *base), ascent - baseAscent);
        base->setLocation(baseLocation);
        horizontalOffset += base->logicalWidth();
        LayoutUnit scriptAscent = ascentForChild(*firstPostScript);
        LayoutPoint scriptLocation(mirrorIfNeeded(horizontalOffset, *firstPostScript), ascent - metrics.supShift - scriptAscent);
        firstPostScript->setLocation(scriptLocation);
    }
        break;
    case SubSup:
    case UnderOver:
    case Multiscripts: {
        RenderBox* supScript;

        // Calculate the logical width.
        LayoutUnit logicalWidth = 0;
        for (auto* subScript = firstPreScript; subScript; subScript = supScript->nextSiblingBox()) {
            supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth(), supScript->logicalWidth());
            logicalWidth += subSupPairWidth + space;
        }
        logicalWidth += base->logicalWidth();
        for (auto* subScript = firstPostScript; subScript && !isPrescriptDelimiter(*subScript); subScript = supScript->nextSiblingBox()) {
            supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(std::max(LayoutUnit(0), subScript->logicalWidth() - baseItalicCorrection), supScript->logicalWidth());
            logicalWidth += subSupPairWidth + space;
        }
        setLogicalWidth(logicalWidth);

        for (auto* subScript = firstPreScript; subScript; subScript = supScript->nextSiblingBox()) {
            supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth(), supScript->logicalWidth());
            horizontalOffset += space + subSupPairWidth;
            LayoutUnit subAscent = ascentForChild(*subScript);
            LayoutPoint subScriptLocation(mirrorIfNeeded(horizontalOffset - subScript->logicalWidth(), *subScript), ascent + metrics.subShift - subAscent);
            subScript->setLocation(subScriptLocation);
            LayoutUnit supAscent = ascentForChild(*supScript);
            LayoutPoint supScriptLocation(mirrorIfNeeded(horizontalOffset - supScript->logicalWidth(), *supScript), ascent - metrics.supShift - supAscent);
            supScript->setLocation(supScriptLocation);
        }
        LayoutPoint baseLocation(mirrorIfNeeded(horizontalOffset, *base), ascent - baseAscent);
        base->setLocation(baseLocation);
        horizontalOffset += base->logicalWidth();
        for (auto* subScript = firstPostScript; subScript && !isPrescriptDelimiter(*subScript); subScript = supScript->nextSiblingBox()) {
            supScript = subScript->nextSiblingBox();
            ASSERT(supScript);
            LayoutUnit subAscent = ascentForChild(*subScript);
            LayoutPoint subScriptLocation(mirrorIfNeeded(horizontalOffset - baseItalicCorrection, *subScript), ascent + metrics.subShift - subAscent);
            subScript->setLocation(subScriptLocation);
            LayoutUnit supAscent = ascentForChild(*supScript);
            LayoutPoint supScriptLocation(mirrorIfNeeded(horizontalOffset, *supScript), ascent - metrics.supShift - supAscent);
            supScript->setLocation(supScriptLocation);

            LayoutUnit subSupPairWidth = std::max(subScript->logicalWidth(), supScript->logicalWidth());
            horizontalOffset += subSupPairWidth + space;
        }
    }
    }

    clearNeedsLayout();
}

Optional<int> RenderMathMLScripts::firstLineBaseline() const
{
    ASSERT(!needsLayout());
    auto* base = firstChildBox();
    if (!base)
        return Optional<int>();
    return Optional<int>(static_cast<int>(lroundf(ascentForChild(*base) + base->logicalTop())));
}

}

#endif // ENABLE(MATHML)
