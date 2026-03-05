/*
 * Copyright (C) 2014 Frédéric Wang (fred.wang@free.fr). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "MathMLElement.h"
#include "MathMLNames.h"
#include "MathMLTokenElement.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElement.h"
#include "RenderIterator.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderMathMLToken);

RenderMathMLToken::RenderMathMLToken(Type type, MathMLTokenElement& element, RenderStyle&& style)
    : RenderMathMLBlock(type, element, WTF::move(style))
{
}

RenderMathMLToken::RenderMathMLToken(Type type, Document& document, RenderStyle&& style)
    : RenderMathMLBlock(type, document, WTF::move(style))
{
}

RenderMathMLToken::~RenderMathMLToken() = default;

MathMLTokenElement& RenderMathMLToken::element()
{
    return static_cast<MathMLTokenElement&>(nodeForNonAnonymous());
}

void RenderMathMLToken::updateTokenContent()
{
    RenderMathMLBlock::updateFromElement();
    setMathVariantGlyphDirty();
}

void RenderMathMLToken::computePreferredLogicalWidths()
{
    ASSERT(needsPreferredLogicalWidthsUpdate());

    if (m_mathVariantGlyphDirty)
        updateMathVariantGlyph();

    if (m_mathVariantCodePoint) {
        auto mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);
        if (mathVariantGlyph.font) {
            m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = mathVariantGlyph.font->widthForGlyph(mathVariantGlyph.glyph);
            adjustPreferredLogicalWidthsForBorderAndPadding();
            clearNeedsPreferredWidthsUpdate();
            return;
        }
    }

    RenderMathMLBlock::computePreferredLogicalWidths();
}

void RenderMathMLToken::updateMathVariantGlyph()
{
    ASSERT(m_mathVariantGlyphDirty);

    m_mathVariantCodePoint = std::nullopt;
    m_mathVariantGlyphDirty = false;

    // Early return if the token element contains RenderElements.
    // Note that the renderers corresponding to the children of the token element are wrapped inside an anonymous RenderBlock.
    if (const auto& block = downcast<RenderElement>(firstChild())) {
        if (childrenOfType<RenderElement>(*block).first())
            return;
    }

    const Ref tokenElement = element();
    auto text = tokenElement->textContent();
    StringView view = text;
    if (auto codePoint = view.trim(isASCIIWhitespaceWithoutFF<char16_t>).convertToSingleCodePoint()) {
        MathVariant mathvariant = mathMLStyle().mathVariant();
        if (mathvariant == MathVariant::None)
            mathvariant = tokenElement->hasTagName(MathMLNames::miTag) ? MathVariant::Italic : MathVariant::Normal;
        char32_t transformedCodePoint = mathVariantMapCodePoint(codePoint.value(), mathvariant);
        if (transformedCodePoint != codePoint.value()) {
            m_mathVariantCodePoint = transformedCodePoint;
            m_mathVariantIsMirrored = writingMode().isBidiRTL();
        }
    }
}

void RenderMathMLToken::setMathVariantGlyphDirty()
{
    m_mathVariantGlyphDirty = true;
    setNeedsLayoutAndPreferredWidthsUpdate();
}

void RenderMathMLToken::styleDidChange(Style::Difference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    setMathVariantGlyphDirty();
}

void RenderMathMLToken::updateFromElement()
{
    RenderMathMLBlock::updateFromElement();
    setMathVariantGlyphDirty();
}

std::optional<LayoutUnit> RenderMathMLToken::firstLineBaseline() const
{
    if (m_mathVariantCodePoint) {
        auto mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);
        if (mathVariantGlyph.font) {
            auto baseline = settings().subpixelInlineLayoutEnabled() ? LayoutUnit(-mathVariantGlyph.font->boundsForGlyph(mathVariantGlyph.glyph).y()) : LayoutUnit(roundf(-mathVariantGlyph.font->boundsForGlyph(mathVariantGlyph.glyph).y()));
            return { borderAndPaddingBefore() + baseline };
        }
    }
    return RenderMathMLBlock::firstLineBaseline();
}

void RenderMathMLToken::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    insertPositionedChildrenIntoContainingBlock();

    if (relayoutChildren == RelayoutChildren::No && simplifiedLayout())
        return;

    layoutFloatingChildren();

    GlyphData mathVariantGlyph;
    if (m_mathVariantCodePoint)
        mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);

    if (!mathVariantGlyph.font) {
        RenderMathMLBlock::layoutBlock(relayoutChildren, pageLogicalHeight);
        return;
    }

    recomputeLogicalWidth();
    for (CheckedPtr child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox())
        child->layoutIfNeeded();
    setLogicalWidth(LayoutUnit(mathVariantGlyph.font->widthForGlyph(mathVariantGlyph.glyph)));
    setLogicalHeight(LayoutUnit(mathVariantGlyph.font->boundsForGlyph(mathVariantGlyph.glyph).height()));

    adjustLayoutForBorderAndPadding();

    layoutOutOfFlowBoxes(relayoutChildren);
}

void RenderMathMLToken::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);

    // FIXME: Instead of using DrawGlyph, we may consider using the more general TextPainter so that we can apply mathvariant to strings with an arbitrary number of characters and preserve advanced CSS effects (text-shadow, etc).
    if (info.context().paintingDisabled() || info.phase != PaintPhase::Foreground || style().usedVisibility() != Visibility::Visible || !m_mathVariantCodePoint)
        return;

    auto mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);
    if (!mathVariantGlyph.font)
        return;

    GraphicsContextStateSaver stateSaver(info.context());
    info.context().setFillColor(style().visitedDependentColorApplyingColorFilter());

    auto glyphAscent = settings().subpixelInlineLayoutEnabled() ? -mathVariantGlyph.font->boundsForGlyph(mathVariantGlyph.glyph).y() : roundf(-mathVariantGlyph.font->boundsForGlyph(mathVariantGlyph.glyph).y());
    // FIXME: If we're just drawing a single glyph, why do we need to compute an advance?
    auto advance = makeGlyphBufferAdvance(mathVariantGlyph.font->widthForGlyph(mathVariantGlyph.glyph));
    auto location = paintOffset + this->location() + LayoutPoint { borderLeft() + paddingLeft(), glyphAscent + borderAndPaddingBefore() };
    if (style().writingMode().isHorizontal())
        location.setY(roundToDevicePixel(LayoutUnit { location.y() }, document().deviceScaleFactor()));
    else
        location.setX(roundToDevicePixel(LayoutUnit { location.x() }, document().deviceScaleFactor()));

    info.context().drawGlyphs(*mathVariantGlyph.font, singleElementSpan(mathVariantGlyph.glyph), singleElementSpan(advance), location, style().fontCascade().fontDescription().usedFontSmoothing());
}

void RenderMathMLToken::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    if (m_mathVariantCodePoint) {
        auto mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);
        if (mathVariantGlyph.font)
            return;
    }

    RenderMathMLBlock::paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
}

}

#endif // ENABLE(MATHML)
