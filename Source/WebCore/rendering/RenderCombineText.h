/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "FontCascade.h"
#include "RenderText.h"
#include "Text.h"

namespace WebCore {

class RenderCombineText final : public RenderText {
    WTF_MAKE_ISO_ALLOCATED(RenderCombineText);
public:
    RenderCombineText(Text&, const String&);

    Text& textNode() const { return downcast<Text>(nodeForNonAnonymous()); }

    void combineTextIfNeeded();
    std::optional<FloatPoint> computeTextOrigin(const FloatRect& boxRect) const;
    String combinedStringForRendering() const;
    bool isCombined() const { return m_isCombined; }
    float combinedTextWidth(const FontCascade& font) const { return font.size(); }
    const FontCascade& originalFont() const { return parent()->style().fontCascade(); }
    const FontCascade& textCombineFont() const { return m_combineFontStyle->fontCascade(); }

private:
    void node() const = delete;

    ASCIILiteral renderName() const override { return "RenderCombineText"_s; }
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    void setRenderedText(const String&) override;

    std::unique_ptr<RenderStyle> m_combineFontStyle;
    float m_combinedTextWidth { 0 };
    float m_combinedTextAscent { 0 };
    float m_combinedTextDescent { 0 };
    bool m_isCombined : 1;
    bool m_needsFontUpdate : 1;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderCombineText, isRenderCombineText())
