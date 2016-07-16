/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
 * Copyright (C) 2013, 2016 Igalia S.L.
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

#include "RenderMathMLOperator.h"

#include "FontSelector.h"
#include "MathMLNames.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderText.h"
#include "ScaleTransformOperation.h"
#include "TransformOperations.h"
#include <cmath>
#include <wtf/MathExtras.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace MathMLNames;

RenderMathMLOperator::RenderMathMLOperator(MathMLElement& element, RenderStyle&& style)
    : RenderMathMLToken(element, WTFMove(style))
    , m_stretchHeightAboveBaseline(0)
    , m_stretchDepthBelowBaseline(0)
    , m_textContent(0)
    , m_isVertical(true)
{
    updateTokenContent();
}

RenderMathMLOperator::RenderMathMLOperator(Document& document, RenderStyle&& style, const String& operatorString, MathMLOperatorDictionary::Form form, unsigned short flags)
    : RenderMathMLToken(document, WTFMove(style))
    , m_stretchHeightAboveBaseline(0)
    , m_stretchDepthBelowBaseline(0)
    , m_textContent(0)
    , m_isVertical(true)
    , m_operatorForm(form)
    , m_operatorFlags(flags)
{
    updateTokenContent(operatorString);
}

void RenderMathMLOperator::setOperatorFlagFromAttribute(MathMLOperatorDictionary::Flag flag, const QualifiedName& name)
{
    setOperatorFlagFromAttributeValue(flag, element().attributeWithoutSynchronization(name));
}

void RenderMathMLOperator::setOperatorFlagFromAttributeValue(MathMLOperatorDictionary::Flag flag, const AtomicString& attributeValue)
{
    ASSERT(!isAnonymous());

    if (attributeValue == "true")
        m_operatorFlags |= flag;
    else if (attributeValue == "false")
        m_operatorFlags &= ~flag;
    // We ignore absent or invalid attributes.
}

void RenderMathMLOperator::setOperatorPropertiesFromOpDictEntry(const MathMLOperatorDictionary::Entry* entry)
{
    // If this operator is anonymous, we preserve the Fence and Separator properties. This is to handle the case of RenderMathMLFenced.
    if (isAnonymous())
        m_operatorFlags = (m_operatorFlags & (MathMLOperatorDictionary::Fence | MathMLOperatorDictionary::Separator)) | entry->flags;
    else
        m_operatorFlags = entry->flags;

    // Leading and trailing space is specified as multiple of 1/18em.
    m_leadingSpace = entry->lspace * style().fontCascade().size() / 18;
    m_trailingSpace = entry->rspace * style().fontCascade().size() / 18;
}

void RenderMathMLOperator::setOperatorProperties()
{
    // We determine the stretch direction (default is vertical).
    m_isVertical = MathMLOperatorDictionary::isVertical(m_textContent);

    // We determine the form of the operator.
    bool explicitForm = true;
    if (!isAnonymous()) {
        const AtomicString& form = element().attributeWithoutSynchronization(MathMLNames::formAttr);
        if (form == "prefix")
            m_operatorForm = MathMLOperatorDictionary::Prefix;
        else if (form == "infix")
            m_operatorForm = MathMLOperatorDictionary::Infix;
        else if (form == "postfix")
            m_operatorForm = MathMLOperatorDictionary::Postfix;
        else {
            // FIXME: We should use more advanced heuristics indicated in the specification to determine the operator form (https://bugs.webkit.org/show_bug.cgi?id=124829).
            explicitForm = false;
            if (!element().previousSibling() && element().nextSibling())
                m_operatorForm = MathMLOperatorDictionary::Prefix;
            else if (element().previousSibling() && !element().nextSibling())
                m_operatorForm = MathMLOperatorDictionary::Postfix;
            else
                m_operatorForm = MathMLOperatorDictionary::Infix;
        }
    }

    // We determine the default values of the operator properties.

    // First we initialize with the default values for unknown operators.
    if (isAnonymous())
        m_operatorFlags &= MathMLOperatorDictionary::Fence | MathMLOperatorDictionary::Separator; // This resets all but the Fence and Separator properties.
    else
        m_operatorFlags = 0; // This resets all the operator properties.
    m_leadingSpace = 5 * style().fontCascade().size() / 18; // This sets leading space to "thickmathspace".
    m_trailingSpace = 5 * style().fontCascade().size() / 18; // This sets trailing space to "thickmathspace".
    m_minSize = style().fontCascade().size(); // This sets minsize to "1em".
    m_maxSize = intMaxForLayoutUnit; // This sets maxsize to "infinity".

    if (m_textContent) {
        // Then we try to find the default values from the operator dictionary.
        if (const MathMLOperatorDictionary::Entry* entry = MathMLOperatorDictionary::getEntry(m_textContent, m_operatorForm))
            setOperatorPropertiesFromOpDictEntry(entry);
        else if (!explicitForm) {
            // If we did not find the desired operator form and if it was not set explicitely, we use the first one in the following order: Infix, Prefix, Postfix.
            // This is to handle bad MathML markup without explicit <mrow> delimiters like "<mo>(</mo><mi>a</mi><mo>)</mo><mo>(</mo><mi>b</mi><mo>)</mo>" where the inner parenthesis should not be considered infix.
            if (const MathMLOperatorDictionary::Entry* entry = MathMLOperatorDictionary::getEntry(m_textContent)) {
                m_operatorForm = static_cast<MathMLOperatorDictionary::Form>(entry->form); // We override the form previously determined.
                setOperatorPropertiesFromOpDictEntry(entry);
            }
        }
    }

    if (!isAnonymous()) {
        // Finally, we make the attribute values override the default.

        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Fence, MathMLNames::fenceAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Separator, MathMLNames::separatorAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Stretchy, MathMLNames::stretchyAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Symmetric, MathMLNames::symmetricAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::LargeOp, MathMLNames::largeopAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::MovableLimits, MathMLNames::movablelimitsAttr);
        setOperatorFlagFromAttribute(MathMLOperatorDictionary::Accent, MathMLNames::accentAttr);

        parseMathMLLength(element().attributeWithoutSynchronization(MathMLNames::lspaceAttr), m_leadingSpace, &style(), false); // FIXME: Negative leading space must be implemented (https://bugs.webkit.org/show_bug.cgi?id=124830).
        parseMathMLLength(element().attributeWithoutSynchronization(MathMLNames::rspaceAttr), m_trailingSpace, &style(), false); // FIXME: Negative trailing space must be implemented (https://bugs.webkit.org/show_bug.cgi?id=124830).

        parseMathMLLength(element().attributeWithoutSynchronization(MathMLNames::minsizeAttr), m_minSize, &style(), false);
        const AtomicString& maxsize = element().attributeWithoutSynchronization(MathMLNames::maxsizeAttr);
        if (maxsize != "infinity")
            parseMathMLLength(maxsize, m_maxSize, &style(), false);
    }
}

void RenderMathMLOperator::stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline)
{
    ASSERT(hasOperatorFlag(MathMLOperatorDictionary::Stretchy));
    ASSERT(m_isVertical);

    if (!m_isVertical || (heightAboveBaseline == m_stretchHeightAboveBaseline && depthBelowBaseline == m_stretchDepthBelowBaseline))
        return;

    m_stretchHeightAboveBaseline = heightAboveBaseline;
    m_stretchDepthBelowBaseline = depthBelowBaseline;

    setOperatorProperties();
    if (hasOperatorFlag(MathMLOperatorDictionary::Symmetric)) {
        // We make the operator stretch symmetrically above and below the axis.
        LayoutUnit axis = mathAxisHeight();
        LayoutUnit halfStretchSize = std::max(m_stretchHeightAboveBaseline - axis, m_stretchDepthBelowBaseline + axis);
        m_stretchHeightAboveBaseline = halfStretchSize + axis;
        m_stretchDepthBelowBaseline = halfStretchSize - axis;
    }
    // We try to honor the minsize/maxsize condition by increasing or decreasing both height and depth proportionately.
    // The MathML specification does not indicate what to do when maxsize < minsize, so we follow Gecko and make minsize take precedence.
    LayoutUnit size = stretchSize();
    float aspect = 1.0;
    if (size > 0) {
        if (size < m_minSize)
            aspect = float(m_minSize) / size;
        else if (m_maxSize < size)
            aspect = float(m_maxSize) / size;
    }
    m_stretchHeightAboveBaseline *= aspect;
    m_stretchDepthBelowBaseline *= aspect;

    m_mathOperator.stretchTo(style(), m_stretchHeightAboveBaseline + m_stretchDepthBelowBaseline);

    setLogicalHeight(m_mathOperator.ascent() + m_mathOperator.descent());
}

void RenderMathMLOperator::stretchTo(LayoutUnit width)
{
    ASSERT(hasOperatorFlag(MathMLOperatorDictionary::Stretchy));
    ASSERT(!m_isVertical);

    if (m_isVertical || m_stretchWidth == width)
        return;

    m_stretchWidth = width;
    m_mathOperator.stretchTo(style(), width);

    setOperatorProperties();

    setLogicalHeight(m_mathOperator.ascent() + m_mathOperator.descent());
}

void RenderMathMLOperator::resetStretchSize()
{
    if (m_isVertical) {
        m_stretchHeightAboveBaseline = 0;
        m_stretchDepthBelowBaseline = 0;
    } else
        m_stretchWidth = 0;
}

void RenderMathMLOperator::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    LayoutUnit preferredWidth = 0;

    setOperatorProperties();
    if (!useMathOperator()) {
        RenderMathMLToken::computePreferredLogicalWidths();
        preferredWidth = m_maxPreferredLogicalWidth;
        if (isInvisibleOperator()) {
            // In some fonts, glyphs for invisible operators have nonzero width. Consequently, we subtract that width here to avoid wide gaps.
            GlyphData data = style().fontCascade().glyphDataForCharacter(m_textContent, false);
            float glyphWidth = data.font ? data.font->widthForGlyph(data.glyph) : 0;
            ASSERT(glyphWidth <= preferredWidth);
            preferredWidth -= glyphWidth;
        }
    } else
        preferredWidth = m_mathOperator.maxPreferredWidth();

    // FIXME: The spacing should be added to the whole embellished operator (https://webkit.org/b/124831).
    // FIXME: The spacing should only be added inside (perhaps inferred) mrow (http://www.w3.org/TR/MathML/chapter3.html#presm.opspacing).
    preferredWidth = m_leadingSpace + preferredWidth + m_trailingSpace;

    m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = preferredWidth;

    setPreferredLogicalWidthsDirty(false);
}

void RenderMathMLOperator::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    if (useMathOperator()) {
        for (auto child = firstChildBox(); child; child = child->nextSiblingBox())
            child->layoutIfNeeded();
        setLogicalWidth(m_leadingSpace + m_mathOperator.width() + m_trailingSpace);
        setLogicalHeight(m_mathOperator.ascent() + m_mathOperator.descent());
    } else {
        // We first do the normal layout without spacing.
        recomputeLogicalWidth();
        LayoutUnit width = logicalWidth();
        setLogicalWidth(width - m_leadingSpace - m_trailingSpace);
        RenderMathMLToken::layoutBlock(relayoutChildren, pageLogicalHeight);
        setLogicalWidth(width);

        // We then move the children to take spacing into account.
        LayoutPoint horizontalShift(style().direction() == LTR ? m_leadingSpace : -m_leadingSpace, 0);
        for (auto* child = firstChildBox(); child; child = child->nextSiblingBox())
            child->setLocation(child->location() + horizontalShift);
    }

    clearNeedsLayout();
}

void RenderMathMLOperator::rebuildTokenContent(const String& operatorString)
{
    // We collapse the whitespace and replace the hyphens by minus signs.
    AtomicString textContent = operatorString.stripWhiteSpace().simplifyWhiteSpace().replace(hyphenMinus, minusSign).impl();

    // We verify whether the operator text can be represented by a single UChar.
    // FIXME: This does not handle surrogate pairs (https://bugs.webkit.org/show_bug.cgi?id=122296).
    // FIXME: This does not handle <mo> operators with multiple characters (https://bugs.webkit.org/show_bug.cgi?id=124828).
    m_textContent = textContent.length() == 1 ? textContent[0] : 0;
    setOperatorProperties();

    if (useMathOperator()) {
        MathOperator::Type type;
        if (!shouldAllowStretching())
            type = MathOperator::Type::NormalOperator;
        else if (isLargeOperatorInDisplayStyle())
            type = MathOperator::Type::DisplayOperator;
        else
            type = m_isVertical ? MathOperator::Type::VerticalOperator : MathOperator::Type::HorizontalOperator;
        m_mathOperator.setOperator(style(), m_textContent, type);
    }

    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMathMLOperator::updateTokenContent(const String& operatorString)
{
    ASSERT(isAnonymous());
    rebuildTokenContent(operatorString);
}

void RenderMathMLOperator::updateTokenContent()
{
    ASSERT(!isAnonymous());
    RenderMathMLToken::updateTokenContent();
    rebuildTokenContent(element().textContent());
}

void RenderMathMLOperator::updateFromElement()
{
    updateTokenContent();
}

void RenderMathMLOperator::updateOperatorProperties()
{
    setOperatorProperties();
    setNeedsLayoutAndPrefWidthsRecalc();
}

bool RenderMathMLOperator::shouldAllowStretching() const
{
    return m_textContent && (hasOperatorFlag(MathMLOperatorDictionary::Stretchy) || isLargeOperatorInDisplayStyle());
}

bool RenderMathMLOperator::useMathOperator() const
{
    // We use the MathOperator class to handle the following cases:
    // 1) Stretchy and large operators, since they require special painting.
    // 2) The minus sign, since it can be obtained from a hyphen in the DOM.
    // 3) The anonymous operators created by mfenced, since they do not have text content in the DOM.
    return shouldAllowStretching() || m_textContent == minusSign || isAnonymous();
}

void RenderMathMLOperator::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    m_mathOperator.reset(style());
    updateOperatorProperties();
}

LayoutUnit RenderMathMLOperator::verticalStretchedOperatorShift() const
{
    if (!m_isVertical || !stretchSize())
        return 0;

    return (m_stretchDepthBelowBaseline - m_stretchHeightAboveBaseline - m_mathOperator.descent() + m_mathOperator.ascent()) / 2;
}

Optional<int> RenderMathMLOperator::firstLineBaseline() const
{
    if (useMathOperator())
        return Optional<int>(std::lround(static_cast<float>(m_mathOperator.ascent() - verticalStretchedOperatorShift())));
    return RenderMathMLToken::firstLineBaseline();
}

void RenderMathMLOperator::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLToken::paint(info, paintOffset);
    if (!useMathOperator())
        return;

    LayoutPoint operatorTopLeft = paintOffset + location();
    operatorTopLeft.move(style().isLeftToRightDirection() ? m_leadingSpace : m_trailingSpace, 0);

    // Center horizontal operators.
    if (!m_isVertical)
        operatorTopLeft.move(-(m_mathOperator.width() - width()) / 2, 0);

    m_mathOperator.paint(style(), info, operatorTopLeft);
}

void RenderMathMLOperator::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    // We skip painting for invisible operators too to avoid some "missing character" glyph to appear if appropriate math fonts are not available.
    if (useMathOperator() || isInvisibleOperator())
        return;
    RenderMathMLToken::paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
}

}

#endif
