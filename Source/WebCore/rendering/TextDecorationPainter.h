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
    TextDecorationPainter(GraphicsContext&, const FontCascade&, InlineIterator::TextBoxIterator, float width, const ShadowData*, const FilterOperations*, Styles);

    void paintBackgroundDecorations(const TextRun&, const FloatPoint& textOrigin, const FloatPoint& boxOrigin);
    void paintForegroundDecorations(const FloatPoint& boxOrigin);

    struct Styles {
        bool operator==(const Styles&) const;
        bool operator!=(const Styles& other) const { return !(*this == other); }

        struct DecorationStyleAndColor {
            Color color;
            TextDecorationStyle decorationStyle;
        };
        DecorationStyleAndColor underline;
        DecorationStyleAndColor overline;
        DecorationStyleAndColor linethrough;
    };
    static Color decorationColor(const RenderStyle&);
    static Styles stylesForRenderer(const RenderObject&, OptionSet<TextDecorationLine> requestedDecorations, bool firstLineStyle = false, PseudoId = PseudoId::None);

private:
    void paintLineThrough(const Color&, float thickness, const FloatPoint& localOrigin);

    GraphicsContext& m_context;
    OptionSet<TextDecorationLine> m_textBoxDecorations;
    float m_wavyOffset { 0 };
    float m_width { 0 };
    FloatPoint m_boxOrigin;
    bool m_isPrinting { false };
    bool m_isHorizontal { true };
    const ShadowData* m_shadow { nullptr };
    const FilterOperations* m_shadowColorFilter { nullptr };
    InlineIterator::TextBoxIterator m_textBox;
    const FontCascade& m_font;
    float m_deviceScaleFactor { 0 };

    Styles m_styles;
    const RenderStyle& m_textBoxStyle;
};
    
} // namespace WebCore
