/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "FontCascade.h"
#include "RenderText.h"
#include "SVGTextLayoutAttributes.h"
#include "Text.h"

namespace WebCore {

class SVGInlineTextBox;

class RenderSVGInlineText final : public RenderText {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGInlineText);
public:
    RenderSVGInlineText(Text&, const String&);

    Text& textNode() const { return downcast<Text>(nodeForNonAnonymous()); }

    bool characterStartsNewTextChunk(int position) const;
    SVGTextLayoutAttributes* layoutAttributes() { return &m_layoutAttributes; }

    float scalingFactor() const { return m_scalingFactor; }
    const FontCascade& scaledFont() const { return m_scaledFont; }
    void updateScaledFont();
    static void computeNewScaledFontForStyle(const RenderObject&, const RenderStyle&, float& scalingFactor, FontCascade& scaledFont);

    // Preserves floating point precision for the use in DRT. It knows how to round and does a better job than enclosingIntRect.
    FloatRect floatLinesBoundingBox() const;

private:
    const char* renderName() const override { return "RenderSVGInlineText"; }

    String originalText() const override;
    void setRenderedText(const String&) override;
    void styleDidChange(StyleDifference, const RenderStyle*) override;

    FloatRect objectBoundingBox() const override { return floatLinesBoundingBox(); }

    bool isSVGInlineText() const override { return true; }

    VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*) override;
    LayoutRect localCaretRect(InlineBox*, unsigned caretOffset, LayoutUnit* extraWidthToEndOfLine = 0) override;
    IntRect linesBoundingBox() const override;
    std::unique_ptr<InlineTextBox> createTextBox() override;

    float m_scalingFactor;
    FontCascade m_scaledFont;
    SVGTextLayoutAttributes m_layoutAttributes;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGInlineText, isSVGInlineText())
