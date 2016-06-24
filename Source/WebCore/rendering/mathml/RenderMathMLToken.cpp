/*
 * Copyright (C) 2014 Frédéric Wang (fred.wang@free.fr). All rights reserved.
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
#include "RenderMathMLToken.h"

#if ENABLE(MATHML)

#include "MathMLNames.h"
#include "PaintInfo.h"
#include "RenderElement.h"
#include "RenderIterator.h"

namespace WebCore {

using namespace MathMLNames;

RenderMathMLToken::RenderMathMLToken(Element& element, RenderStyle&& style)
    : RenderMathMLBlock(element, WTFMove(style))
    , m_mathVariantGlyph()
    , m_mathVariantGlyphDirty(false)
{
}

RenderMathMLToken::RenderMathMLToken(Document& document, RenderStyle&& style)
    : RenderMathMLBlock(document, WTFMove(style))
    , m_mathVariantGlyph()
    , m_mathVariantGlyphDirty(false)
{
}

void RenderMathMLToken::updateTokenContent()
{
    RenderMathMLBlock::updateFromElement();
    setMathVariantGlyphDirty();
}

static bool transformToItalic(UChar32& codePoint)
{
    const UChar32 lowerAlpha = 0x3B1;
    const UChar32 lowerOmega = 0x3C9;
    const UChar32 mathItalicLowerA = 0x1D44E;
    const UChar32 mathItalicLowerAlpha = 0x1D6FC;
    const UChar32 mathItalicLowerH = 0x210E;
    const UChar32 mathItalicUpperA = 0x1D434;

    // FIXME: We should also transform dotless i, dotless j and more greek letters.
    if ('a' <= codePoint && codePoint <= 'z') {
        if (codePoint == 'h')
            codePoint = mathItalicLowerH;
        else
            codePoint += mathItalicLowerA - 'a';
        return true;
    }
    if ('A' <= codePoint && codePoint <= 'Z') {
        codePoint += mathItalicUpperA - 'A';
        return true;
    }
    if (lowerAlpha <= codePoint && codePoint <= lowerOmega) {
        codePoint += mathItalicLowerAlpha - lowerAlpha;
        return true;
    }

    return false;
}

void RenderMathMLToken::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    if (m_mathVariantGlyphDirty)
        updateMathVariantGlyph();

    if (m_mathVariantGlyph.isValid()) {
        m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = m_mathVariantGlyph.font->widthForGlyph(m_mathVariantGlyph.glyph);
        setPreferredLogicalWidthsDirty(false);
        return;
    }

    RenderMathMLBlock::computePreferredLogicalWidths();
}

void RenderMathMLToken::updateMathVariantGlyph()
{
    ASSERT(m_mathVariantGlyphDirty);

    // This implements implicit italic mathvariant for single-char <mi>.
    // FIXME: Add full support for the mathvariant attribute (https://webkit.org/b/85735)
    m_mathVariantGlyph = GlyphData();
    m_mathVariantGlyphDirty = false;

    // Early return if the token element contains RenderElements.
    // Note that the renderers corresponding to the children of the token element are wrapped inside an anonymous RenderBlock.
    if (const auto& block = downcast<RenderElement>(firstChild())) {
        if (childrenOfType<RenderElement>(*block).first())
            return;
    }

    const auto& tokenElement = element();
    if (tokenElement.hasTagName(MathMLNames::miTag) && !tokenElement.hasAttribute(mathvariantAttr)) {
        AtomicString textContent = element().textContent().stripWhiteSpace().simplifyWhiteSpace();
        if (textContent.length() == 1) {
            UChar32 codePoint = textContent[0];
            if (transformToItalic(codePoint))
                m_mathVariantGlyph = style().fontCascade().glyphDataForCharacter(codePoint, !style().isLeftToRightDirection());
        }
    }
}

void RenderMathMLToken::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    setMathVariantGlyphDirty();
}

void RenderMathMLToken::updateFromElement()
{
    RenderMathMLBlock::updateFromElement();
    setMathVariantGlyphDirty();
}

Optional<int> RenderMathMLToken::firstLineBaseline() const
{
    if (m_mathVariantGlyph.isValid())
        return Optional<int>(static_cast<int>(lroundf(-m_mathVariantGlyph.font->boundsForGlyph(m_mathVariantGlyph.glyph).y())));
    return RenderMathMLBlock::firstLineBaseline();
}

void RenderMathMLToken::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    if (!m_mathVariantGlyph.isValid()) {
        RenderMathMLBlock::layoutBlock(relayoutChildren, pageLogicalHeight);
        return;
    }

    for (auto* child = firstChildBox(); child; child = child->nextSiblingBox())
        child->layoutIfNeeded();

    setLogicalWidth(m_mathVariantGlyph.font->widthForGlyph(m_mathVariantGlyph.glyph));
    setLogicalHeight(m_mathVariantGlyph.font->boundsForGlyph(m_mathVariantGlyph.glyph).height());

    clearNeedsLayout();
}

void RenderMathMLToken::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);

    // FIXME: Instead of using DrawGlyph, we may consider using the more general TextPainter so that we can apply mathvariant to strings with an arbitrary number of characters and preserve advanced CSS effects (text-shadow, etc).
    if (info.context().paintingDisabled() || info.phase != PaintPhaseForeground || style().visibility() != VISIBLE || !m_mathVariantGlyph.isValid())
        return;

    GraphicsContextStateSaver stateSaver(info.context());
    info.context().setFillColor(style().visitedDependentColor(CSSPropertyColor));

    GlyphBuffer buffer;
    buffer.add(m_mathVariantGlyph.glyph, m_mathVariantGlyph.font, m_mathVariantGlyph.font->widthForGlyph(m_mathVariantGlyph.glyph));
    LayoutUnit glyphAscent = static_cast<int>(lroundf(-m_mathVariantGlyph.font->boundsForGlyph(m_mathVariantGlyph.glyph).y()));
    info.context().drawGlyphs(style().fontCascade(), *m_mathVariantGlyph.font, buffer, 0, 1, paintOffset + location() + LayoutPoint(0, glyphAscent));
}

void RenderMathMLToken::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    if (m_mathVariantGlyph.isValid())
        return;
    RenderMathMLBlock::paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
}

}

#endif // ENABLE(MATHML)
