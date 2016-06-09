/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
 * Copyright (C) 2013 Igalia S.L.
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
    setOperatorFlagFromAttributeValue(flag, element().fastGetAttribute(name));
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
        const AtomicString& form = element().fastGetAttribute(MathMLNames::formAttr);
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

        parseMathMLLength(element().fastGetAttribute(MathMLNames::lspaceAttr), m_leadingSpace, &style(), false); // FIXME: Negative leading space must be implemented (https://bugs.webkit.org/show_bug.cgi?id=124830).
        parseMathMLLength(element().fastGetAttribute(MathMLNames::rspaceAttr), m_trailingSpace, &style(), false); // FIXME: Negative trailing space must be implemented (https://bugs.webkit.org/show_bug.cgi?id=124830).

        parseMathMLLength(element().fastGetAttribute(MathMLNames::minsizeAttr), m_minSize, &style(), false);
        const AtomicString& maxsize = element().fastGetAttribute(MathMLNames::maxsizeAttr);
        if (maxsize != "infinity")
            parseMathMLLength(maxsize, m_maxSize, &style(), false);
    }
}

bool RenderMathMLOperator::isChildAllowed(const RenderObject&, const RenderStyle&) const
{
    return false;
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
        // FIXME: We should read the axis from the MATH table (https://bugs.webkit.org/show_bug.cgi?id=122297). For now, we use the same value as in RenderMathMLFraction::firstLineBaseline().
        LayoutUnit axis = style().fontMetrics().xHeight() / 2;
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
    updateStyle();
}

void RenderMathMLOperator::stretchTo(LayoutUnit width)
{
    ASSERT(hasOperatorFlag(MathMLOperatorDictionary::Stretchy));
    ASSERT(!m_isVertical);

    if (m_isVertical || m_stretchWidth == width)
        return;

    m_stretchWidth = width;

    setOperatorProperties();

    updateStyle();
}

void RenderMathMLOperator::resetStretchSize()
{
    if (m_isVertical) {
        m_stretchHeightAboveBaseline = 0;
        m_stretchDepthBelowBaseline = 0;
    } else
        m_stretchWidth = 0;
}

static inline FloatRect boundsForGlyph(const GlyphData& data)
{
    return data.font && data.glyph ? data.font->boundsForGlyph(data.glyph) : FloatRect();
}

static inline float heightForGlyph(const GlyphData& data)
{
    return boundsForGlyph(data).height();
}

static inline float advanceWidthForGlyph(const GlyphData& data)
{
    return data.font && data.glyph ? data.font->widthForGlyph(data.glyph) : 0;
}

void RenderMathMLOperator::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    setOperatorProperties();
    if (!shouldAllowStretching()) {
        RenderMathMLToken::computePreferredLogicalWidths();
        if (isInvisibleOperator()) {
            // In some fonts, glyphs for invisible operators have nonzero width. Consequently, we subtract that width here to avoid wide gaps.
            GlyphData data = style().fontCascade().glyphDataForCharacter(m_textContent, false);
            float glyphWidth = advanceWidthForGlyph(data);
            ASSERT(glyphWidth <= m_minPreferredLogicalWidth);
            m_minPreferredLogicalWidth -= glyphWidth;
            m_maxPreferredLogicalWidth -= glyphWidth;
        }
        return;
    }

    // FIXME: We should not use stretchSize during the preferred width calculation nor should we leave logical width dirty (http://webkit.org/b/152244).
    MathOperator::Type type;
    if (isLargeOperatorInDisplayStyle())
        type = MathOperator::Type::DisplayOperator;
    else
        type = m_isVertical ? MathOperator::Type::VerticalOperator : MathOperator::Type::HorizontalOperator;
    m_mathOperator.setOperator(style(), m_textContent, type);
    float maximumGlyphWidth;
    if (!m_isVertical) {
        maximumGlyphWidth = m_mathOperator.width();
        if (maximumGlyphWidth < stretchSize())
            maximumGlyphWidth = stretchSize();
    } else
        maximumGlyphWidth = m_mathOperator.maxPreferredWidth();
    m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = m_leadingSpace + maximumGlyphWidth + m_trailingSpace;
}

void RenderMathMLOperator::rebuildTokenContent(const String& operatorString)
{
    // We collapse the whitespace and replace the hyphens by minus signs.
    AtomicString textContent = operatorString.stripWhiteSpace().simplifyWhiteSpace().replace(hyphenMinus, minusSign).impl();

    // We destroy the wrapper and rebuild it.
    // FIXME: Using this RenderText make the text inaccessible to the dumpAsText/selection code (https://bugs.webkit.org/show_bug.cgi?id=125597).
    if (firstChild())
        downcast<RenderElement>(*firstChild()).destroy();
    createWrapperIfNeeded();
    RenderPtr<RenderText> text = createRenderer<RenderText>(document(), textContent);
    downcast<RenderElement>(*firstChild()).addChild(text.leakPtr());

    // We verify whether the operator text can be represented by a single UChar.
    // FIXME: This does not handle surrogate pairs (https://bugs.webkit.org/show_bug.cgi?id=122296).
    // FIXME: This does not handle <mo> operators with multiple characters (https://bugs.webkit.org/show_bug.cgi?id=124828).
    m_textContent = textContent.length() == 1 ? textContent[0] : 0;
    setOperatorProperties();
    updateStyle();
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
    rebuildTokenContent(element().textContent());
}

void RenderMathMLOperator::updateFromElement()
{
    setOperatorProperties();
    RenderMathMLToken::updateFromElement();
}

void RenderMathMLOperator::updateOperatorProperties()
{
    setOperatorProperties();
    if (!isEmpty())
        updateStyle();
    setNeedsLayoutAndPrefWidthsRecalc();
}

bool RenderMathMLOperator::shouldAllowStretching() const
{
    return m_textContent && (hasOperatorFlag(MathMLOperatorDictionary::Stretchy) || isLargeOperatorInDisplayStyle());
}

void RenderMathMLOperator::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    updateOperatorProperties();
}

void RenderMathMLOperator::updateStyle()
{
    ASSERT(firstChild());
    if (!firstChild())
        return;

    m_mathOperator.unstretch();
    m_mathOperator.m_italicCorrection = 0;
    // We add spacing around the operator.
    // FIXME: The spacing should be added to the whole embellished operator (https://bugs.webkit.org/show_bug.cgi?id=124831).
    // FIXME: The spacing should only be added inside (perhaps inferred) mrow (http://www.w3.org/TR/MathML/chapter3.html#presm.opspacing).
    const auto& wrapper = downcast<RenderElement>(firstChild());
    auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(style(), FLEX);
    newStyle.setMarginStart(Length(m_leadingSpace, Fixed));
    newStyle.setMarginEnd(Length(m_trailingSpace, Fixed));
    wrapper->setStyle(WTFMove(newStyle));
    wrapper->setNeedsLayoutAndPrefWidthsRecalc();

    if (!shouldAllowStretching())
        return;

    MathOperator::Type type;
    if (isLargeOperatorInDisplayStyle())
        type = MathOperator::Type::DisplayOperator;
    else
        type = m_isVertical ? MathOperator::Type::VerticalOperator : MathOperator::Type::HorizontalOperator;
    m_mathOperator.setOperator(style(), m_textContent, type);
    if (m_isVertical && isLargeOperatorInDisplayStyle())
        m_mathOperator.calculateDisplayStyleLargeOperator(style());
    else {
        m_mathOperator.calculateStretchyData(style(), false, stretchSize());
        if (!m_mathOperator.isStretched())
            return;
    }

    if (m_isVertical && m_mathOperator.m_stretchType == MathOperator::StretchType::SizeVariant) {
        // We resize the operator to match the one of the size variant.
        if (isLargeOperatorInDisplayStyle()) {
            // The stretch size is actually not involved in the selection of the size variant in findDisplayStyleLargeOperator.
            // We simply use the height and depth of the selected size variant glyph.
            FloatRect glyphBounds = boundsForGlyph(m_mathOperator.m_variant);
            m_stretchHeightAboveBaseline = -glyphBounds.y();
            m_stretchDepthBelowBaseline = glyphBounds.maxY();
        } else {
            // We rescale the height and depth proportionately.
            float variantSize = heightForGlyph(m_mathOperator.m_variant);
            float size = stretchSize();
            float aspect = size > 0 ? variantSize / size : 1.0;
            m_stretchHeightAboveBaseline *= aspect;
            m_stretchDepthBelowBaseline *= aspect;
        }
    }

    if (!m_isVertical) {
        if (m_mathOperator.m_stretchType == MathOperator::StretchType::SizeVariant) {
            FloatRect glyphBounds = boundsForGlyph(m_mathOperator.m_variant);
            m_stretchHeightAboveBaseline = -glyphBounds.y();
            m_stretchDepthBelowBaseline = glyphBounds.maxY();
            m_stretchWidth = advanceWidthForGlyph(m_mathOperator.m_variant);
        } else if (m_mathOperator.m_stretchType == MathOperator::StretchType::GlyphAssembly) {
            FloatRect glyphBounds;
            m_stretchHeightAboveBaseline = 0;
            m_stretchDepthBelowBaseline = 0;

            glyphBounds = boundsForGlyph(m_mathOperator.m_assembly.bottomOrLeft);
            m_stretchHeightAboveBaseline = std::max<LayoutUnit>(m_stretchHeightAboveBaseline, -glyphBounds.y());
            m_stretchDepthBelowBaseline = std::max<LayoutUnit>(m_stretchDepthBelowBaseline, glyphBounds.maxY());

            glyphBounds = boundsForGlyph(m_mathOperator.m_assembly.topOrRight);
            m_stretchHeightAboveBaseline = std::max<LayoutUnit>(m_stretchHeightAboveBaseline, -glyphBounds.y());
            m_stretchDepthBelowBaseline = std::max<LayoutUnit>(m_stretchDepthBelowBaseline, glyphBounds.maxY());

            glyphBounds = boundsForGlyph(m_mathOperator.m_assembly.extension);
            m_stretchHeightAboveBaseline = std::max<LayoutUnit>(m_stretchHeightAboveBaseline, -glyphBounds.y());
            m_stretchDepthBelowBaseline = std::max<LayoutUnit>(m_stretchDepthBelowBaseline, glyphBounds.maxY());

            if (m_mathOperator.m_assembly.middle.isValid()) {
                glyphBounds = boundsForGlyph(m_mathOperator.m_assembly.middle);
                m_stretchHeightAboveBaseline = std::max<LayoutUnit>(m_stretchHeightAboveBaseline, -glyphBounds.y());
                m_stretchDepthBelowBaseline = std::max<LayoutUnit>(m_stretchDepthBelowBaseline, glyphBounds.maxY());
            }
        }
    }
}

Optional<int> RenderMathMLOperator::firstLineBaseline() const
{
    if (m_mathOperator.isStretched())
        return Optional<int>(m_stretchHeightAboveBaseline);
    return RenderMathMLToken::firstLineBaseline();
}

void RenderMathMLOperator::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    if (m_mathOperator.isStretched())
        logicalHeight = m_stretchHeightAboveBaseline + m_stretchDepthBelowBaseline;
    RenderBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

void RenderMathMLOperator::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLToken::paint(info, paintOffset);

    if (info.context().paintingDisabled() || info.phase != PaintPhaseForeground || style().visibility() != VISIBLE || !m_mathOperator.isStretched())
        return;

    GraphicsContextStateSaver stateSaver(info.context());
    info.context().setFillColor(style().visitedDependentColor(CSSPropertyColor));

    if (m_mathOperator.m_stretchType == MathOperator::StretchType::SizeVariant) {
        ASSERT(m_mathOperator.m_variant.isValid());
        GlyphBuffer buffer;
        buffer.add(m_mathOperator.m_variant.glyph, m_mathOperator.m_variant.font, advanceWidthForGlyph(m_mathOperator.m_variant));
        LayoutPoint operatorTopLeft = ceiledIntPoint(paintOffset + location());
        FloatRect glyphBounds = boundsForGlyph(m_mathOperator.m_variant);
        LayoutPoint operatorOrigin(operatorTopLeft.x(), operatorTopLeft.y() - glyphBounds.y());
        info.context().drawGlyphs(style().fontCascade(), *m_mathOperator.m_variant.font, buffer, 0, 1, operatorOrigin);
        return;
    }

    m_mathOperator.m_ascent = m_stretchHeightAboveBaseline;
    m_mathOperator.m_descent = m_stretchDepthBelowBaseline;
    m_mathOperator.m_width = logicalWidth();
    LayoutPoint operatorTopLeft = paintOffset + location();
    operatorTopLeft.move(style().isLeftToRightDirection() ? m_leadingSpace : m_trailingSpace, 0);
    if (m_isVertical)
        m_mathOperator.paintVerticalGlyphAssembly(style(), info, operatorTopLeft);
    else
        m_mathOperator.paintHorizontalGlyphAssembly(style(), info, operatorTopLeft);
}

void RenderMathMLOperator::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    // We skip painting for invisible operators too to avoid some "missing character" glyph to appear if appropriate math fonts are not available.
    if (m_mathOperator.isStretched() || isInvisibleOperator())
        return;
    RenderMathMLToken::paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
}

LayoutUnit RenderMathMLOperator::trailingSpaceError()
{
    const auto& primaryFont = style().fontCascade().primaryFont();
    if (!primaryFont.mathData())
        return 0;

    // For OpenType MATH font, the layout is based on RenderMathOperator for which the preferred width is sometimes overestimated (bug https://bugs.webkit.org/show_bug.cgi?id=130326).
    // Hence we determine the error in the logical width with respect to the actual width of the glyph(s) used to paint the operator.
    LayoutUnit width = logicalWidth();

    if (!m_mathOperator.isStretched()) {
        GlyphData data = style().fontCascade().glyphDataForCharacter(textContent(), !style().isLeftToRightDirection());
        return width - advanceWidthForGlyph(data);
    }

    if (m_mathOperator.m_stretchType == MathOperator::StretchType::SizeVariant)
        return width - advanceWidthForGlyph(m_mathOperator.m_variant);

    float assemblyWidth = advanceWidthForGlyph(m_mathOperator.m_assembly.topOrRight);
    assemblyWidth = std::max(assemblyWidth, advanceWidthForGlyph(m_mathOperator.m_assembly.bottomOrLeft));
    assemblyWidth = std::max(assemblyWidth, advanceWidthForGlyph(m_mathOperator.m_assembly.extension));
    if (m_mathOperator.m_assembly.middle.isValid())
        assemblyWidth = std::max(assemblyWidth, advanceWidthForGlyph(m_mathOperator.m_assembly.middle));
    return width - assemblyWidth;
}

}

#endif
