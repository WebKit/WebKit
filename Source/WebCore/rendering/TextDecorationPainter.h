/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2015 Apple Inc. All rights reserved.
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

#ifndef TextDecorationPainter_h
#define TextDecorationPainter_h

#include "Color.h"
#include "FloatPoint.h"
#include "RenderStyleConstants.h"

namespace WebCore {

class FontCascade;
class GraphicsContext;
class InlineTextBox;
class RenderStyle;
class RenderText;
class ShadowData;
class TextRun;
    
class TextDecorationPainter {
public:
    TextDecorationPainter(GraphicsContext&, TextDecoration, const RenderText&, bool isFirstLine);
    
    void setInlineTextBox(const InlineTextBox* inlineTextBox) { m_inlineTextBox = inlineTextBox; }
    void setFont(const FontCascade& font) { m_font = &font; }
    void setIsHorizontal(bool isHorizontal) { m_isHorizontal = isHorizontal; }
    void setWidth(float width) { m_width = width; }
    void setBaseline(int baseline) { m_baseline = baseline; }
    void addTextShadow(const ShadowData* textShadow) { m_shadow = textShadow; }

    void paintTextDecoration(const TextRun&, const FloatPoint& textOrigin, const FloatPoint& boxOrigin);
        
private:
    GraphicsContext& m_context;
    TextDecoration m_decoration;
    int m_wavyOffset { 0 };
    bool m_isPrinting { false };
    float m_width { 0 };
    int m_baseline { 0 };
    FloatPoint m_boxOrigin;
    bool m_isHorizontal { true };
    const ShadowData* m_shadow { nullptr };
    const InlineTextBox* m_inlineTextBox { nullptr };
    const FontCascade* m_font { nullptr };
    
    Color m_underlineColor;
    Color m_overlineColor;
    Color m_linethroughColor;
    TextDecorationStyle m_underlineStyle;
    TextDecorationStyle m_overlineStyle;
    TextDecorationStyle m_linethroughStyle;
    const RenderStyle& m_lineStyle;
};
    
} // namespace WebCore

#endif // TextDecorationPainter_h
