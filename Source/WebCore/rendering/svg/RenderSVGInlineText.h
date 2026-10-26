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

#include <WebCore/FontCascade.h>
#include <WebCore/RenderObjectNode.h>
#include <WebCore/RenderText.h>
#include <WebCore/SVGTextMetrics.h>
#include <WebCore/Text.h>

namespace WebCore {

class RenderSVGInlineText final : public RenderText {
    WTF_MAKE_TZONE_ALLOCATED(RenderSVGInlineText);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGInlineText);
public:
    RenderSVGInlineText(Text&, const String&);
    virtual ~RenderSVGInlineText();

    Text& textNode() const { return downcast<Text>(nodeForNonAnonymous()); }

    // Used by SVGTextLayoutAlgorithm, which measures only when the text or the font changes.
    Vector<SVGTextMetrics>& textMetrics() LIFETIME_BOUND { return m_textMetrics; }
    const Vector<SVGTextMetrics>& textMetrics() const LIFETIME_BOUND { return m_textMetrics; }
    bool hasValidTextMetrics() const { return m_hasValidTextMetrics; }
    void setHasValidTextMetrics(bool value) { m_hasValidTextMetrics = value; }

    // computeScalingFactor() returns the font-size scaling factor, ignoring the text-rendering mode.
    // scalingFactor() takes it into account, and thus returns 1 whenever text-rendering is set to 'geometricPrecision'.
    // Therefore if you need access to the vanilla scaling factor, use this method directly (e.g. for non-scaling-stroke).
    static float computeScalingFactorForRenderer(const RenderObject&);

    float scalingFactor() const { return m_scalingFactor; }
    const FontCascade& scaledFont() const { return m_scaledFont; }
    void updateScaledFont();
    static bool computeNewScaledFontForStyle(const RenderObject&, const Style::ComputedStyle&, float& scalingFactor, FontCascade& scaledFont);

    // Preserves floating point precision for the use in DRT. It knows how to round and does a better job than enclosingIntRect.
    FloatRect floatLinesBoundingBox() const;

private:
    ASCIILiteral renderName() const override { return "RenderSVGInlineText"_s; }

    String originalText() const override;
    void styleDidChange(Style::Difference, const Style::ComputedStyle*) override;

    FloatRect objectBoundingBox() const override { return floatLinesBoundingBox(); }

    PositionWithAffinity positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*) override;
    IntRect linesBoundingBox() const override;

    void setTextInternal(const String&, bool force) final;

    float m_scalingFactor;
    FontCascade m_scaledFont;
    Vector<SVGTextMetrics> m_textMetrics;
    bool m_hasValidTextMetrics { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGInlineText, isRenderSVGInlineText())
