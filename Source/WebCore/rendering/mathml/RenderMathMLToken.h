/*
 * Copyright (C) 2014 Frédéric Wang (fred.wang@free.fr). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MATHML)

#include "RenderMathMLBlock.h"
#include <wtf/Function.h>

namespace WebCore {

class MathMLTokenElement;
class RenderText;
class TextPainter;
class GlyphBuffer;
struct SpecializedGlyphSourceRange;
struct TextPaintStyle;

class RenderMathMLToken : public RenderMathMLBlock {
    WTF_MAKE_TZONE_ALLOCATED(RenderMathMLToken);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderMathMLToken);
public:
    RenderMathMLToken(Type, MathMLTokenElement&, Style::ComputedStyle&&);
    RenderMathMLToken(Type, Document&, Style::ComputedStyle&&);
    virtual ~RenderMathMLToken();

    MathMLTokenElement& NODELETE element() const;

    // Geometry of the specialized glyph for intersecting source text, in token coordinates.
    std::optional<LayoutRect> specializedGlyphSelectionRect(const RenderText&, unsigned start, unsigned end) const;
    std::optional<LayoutRect> specializedGlyphRepaintRect(const RenderText&, unsigned start, unsigned end) const;
    void invalidateHighlightOverflow();
    void invalidateHighlightOverflow(const LayoutRect&);

    virtual void updateTokenContent();
    void updateFromElement() override;

protected:
    virtual std::optional<std::pair<unsigned, unsigned>> specializedGlyphSourceRange() const;
    virtual std::optional<LayoutRect> specializedGlyphRect() const;
    virtual LayoutRect specializedGlyphInkRect(float strokeOutset = 0) const;
    virtual LayoutUnit specializedGlyphBaseline() const;
    void addSpecializedGlyphOverflow();
    bool paintHighlightedGlyphs(PaintInfo&, const LayoutPoint& paintOffset, const FloatPoint& origin, char32_t, const GlyphBuffer*, NOESCAPE const Function<void(TextPainter&, const TextPaintStyle&, const FloatRect&, const FloatPoint&)>&, NOESCAPE const Function<Vector<FloatSegment>(const FloatPoint&, const FloatRect&)>* = nullptr);
    RepaintRects localRectsForRepaint(RepaintOutlineBounds) const override;
    void paint(PaintInfo&, const LayoutPoint&) override;
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;
    std::optional<LayoutUnit> firstLineBaseline() const override;
    void layoutBlock(RelayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) override;
    void computeIntrinsicLogicalWidthContributions() override;

private:
    bool isRenderMathMLToken() const final { return true; }
    ASCIILiteral renderName() const override { return "RenderMathMLToken"_s; }
    bool isChildAllowed(const RenderObject&, const Style::ComputedStyle&) const final { return true; };
    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) override;
    std::optional<LayoutRect> specializedGlyphRepaintRect() const;
    std::optional<LayoutRect> specializedGlyphRepaintRect(std::span<const SpecializedGlyphSourceRange>) const;
    void updateMathVariantGlyph();
    void setMathVariantGlyphDirty();

    std::optional<char32_t> m_mathVariantCodePoint { std::nullopt };
    unsigned m_mathVariantSourceStart { 0 };
    unsigned m_mathVariantSourceLength { 0 };
    bool m_mathVariantIsMirrored { false };
    bool m_mathVariantGlyphDirty { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLToken, isRenderMathMLToken())

#endif // ENABLE(MATHML)
