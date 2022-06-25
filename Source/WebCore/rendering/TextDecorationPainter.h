/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "FloatPoint.h"
#include "InlineIteratorTextBox.h"
#include "RenderStyleConstants.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class FilterOperations;
class FontCascade;
class FloatRect;
class GraphicsContext;
class LegacyInlineTextBox;
class RenderObject;
class RenderStyle;
class RenderText;
class ShadowData;
class TextRun;
    
class TextDecorationPainter {
public:
    struct Styles;
    TextDecorationPainter(GraphicsContext&, OptionSet<TextDecorationLine> decorations, const RenderText&, bool isFirstLine, const FontCascade&, std::optional<Styles> = std::nullopt);
    
    void setTextBox(InlineIterator::TextBoxIterator textBox) { m_textBox = textBox; }
    void setIsHorizontal(bool isHorizontal) { m_isHorizontal = isHorizontal; }
    void setWidth(float width) { m_width = width; }
    void setTextShadow(const ShadowData* textShadow) { m_shadow = textShadow; }
    void setShadowColorFilter(const FilterOperations* colorFilter) { m_shadowColorFilter = colorFilter; }

    void paintBackgroundDecorations(const TextRun&, const FloatPoint& textOrigin, const FloatPoint& boxOrigin);
    void paintForegroundDecorations(const FloatPoint& boxOrigin);
    void paintLineThrough(const Color&, float thickness, const FloatPoint& localOrigin);

    struct Styles {
        bool operator==(const Styles&) const;
        bool operator!=(const Styles& other) const { return !(*this == other); }

        Color underlineColor;
        Color overlineColor;
        Color linethroughColor;
        TextDecorationStyle underlineStyle;
        TextDecorationStyle overlineStyle;
        TextDecorationStyle linethroughStyle;
    };
    static Color decorationColor(const RenderStyle&);
    static OptionSet<TextDecorationLine> textDecorationsInEffectForStyle(const Styles&);
    static Styles stylesForRenderer(const RenderObject&, OptionSet<TextDecorationLine> requestedDecorations, bool firstLineStyle = false, PseudoId = PseudoId::None);

private:
    GraphicsContext& m_context;
    OptionSet<TextDecorationLine> m_decorations;
    float m_wavyOffset;
    float m_width { 0 };
    FloatPoint m_boxOrigin;
    bool m_isPrinting;
    bool m_isHorizontal { true };
    const ShadowData* m_shadow { nullptr };
    const FilterOperations* m_shadowColorFilter { nullptr };
    InlineIterator::TextBoxIterator m_textBox;
    const FontCascade& m_font;

    Styles m_styles;
    const RenderStyle& m_lineStyle;
};
    
} // namespace WebCore
