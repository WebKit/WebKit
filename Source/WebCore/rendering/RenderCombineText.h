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

#ifndef RenderCombineText_h
#define RenderCombineText_h

#include "FontCascade.h"
#include "RenderElement.h"
#include "RenderText.h"
#include "Text.h"

namespace WebCore {

class RenderCombineText final : public RenderText {
public:
    RenderCombineText(Text&, const String&);

    Text& textNode() const { return downcast<Text>(nodeForNonAnonymous()); }

    void combineText();
    void adjustTextOrigin(FloatPoint& textOrigin, const FloatRect& boxRect) const;
    void getStringToRender(int, String&, int& length) const;
    bool isCombined() const { return m_isCombined; }
    float combinedTextWidth(const FontCascade& font) const { return font.size(); }
    const FontCascade& originalFont() const { return parent()->style().fontCascade(); }
    const FontCascade& textCombineFont() const { return m_combineFontStyle->fontCascade(); }

private:
    void node() const = delete;

    virtual bool isCombineText() const override { return true; }
    virtual float width(unsigned from, unsigned length, const FontCascade&, float xPosition, HashSet<const Font*>* fallbackFonts = 0, GlyphOverflow* = 0) const override;
    virtual const char* renderName() const override { return "RenderCombineText"; }
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual void setRenderedText(const String&) override;

    RefPtr<RenderStyle> m_combineFontStyle;
    FloatSize m_combinedTextSize;
    bool m_isCombined : 1;
    bool m_needsFontUpdate : 1;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderCombineText, isCombineText())

#endif // RenderCombineText_h
