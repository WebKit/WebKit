/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef TextPainter_h
#define TextPainter_h

#include "AffineTransform.h"
#include "DashArray.h"
#include "RenderText.h"

namespace WebCore {

class RenderCombineText;

struct TextPaintStyle;

enum RotationDirection { Counterclockwise, Clockwise };

static inline AffineTransform rotation(const FloatRect& boxRect, RotationDirection clockwise)
{
    return clockwise ? AffineTransform(0, 1, -1, 0, boxRect.x() + boxRect.maxY(), boxRect.maxY() - boxRect.x())
        : AffineTransform(0, -1, 1, 0, boxRect.x() - boxRect.maxY(), boxRect.x() + boxRect.maxY());
}

struct SavedDrawingStateForMask {
    SavedDrawingStateForMask(GraphicsContext* context, TextPaintStyle* textPaintStyle, TextPaintStyle* selectionPaintStyle,
    const ShadowData* textShadow, const ShadowData* selectionShadow)
        : m_context(context)
        , m_textPaintStyle(textPaintStyle)
        , m_selectionPaintStyle(selectionPaintStyle)
        , m_textShadow(textShadow)
        , m_selectionShadow(selectionShadow)
    {
    }
    GraphicsContext* m_context;
    TextPaintStyle* m_textPaintStyle;
    TextPaintStyle* m_selectionPaintStyle;
    const ShadowData* m_textShadow;
    const ShadowData* m_selectionShadow;
};

class TextPainter {
public:
    TextPainter(GraphicsContext&, bool paintSelectedTextOnly, bool paintSelectedTextSeparately, const Font&,
    int startPositionInTextRun, int endPositionInTextBoxString, int length, const AtomicString& emphasisMark, RenderCombineText*,
    TextRun&, FloatRect& boxRect, FloatPoint& textOrigin, int emphasisMarkOffset, const ShadowData* textShadow, const ShadowData* selectionShadow,
    bool textBoxIsHorizontal, TextPaintStyle& nonSelectionPaintStyle, TextPaintStyle& selectionPaintStyle);
    
    void paintText();
    void paintTextInContext(GraphicsContext&, float amountToIncreaseStrokeWidthBy);

    DashArray dashesForIntersectionsWithRect(const FloatRect& lineExtents);

private:
    bool m_paintSelectedTextOnly;
    bool m_paintSelectedTextSeparately;
    const Font& m_font;
    int m_startPositionInTextRun;
    int m_endPositionInTextRun;
    int m_length;
    const AtomicString& m_emphasisMark;
    RenderCombineText* m_combinedText;
    TextRun& m_textRun;
    FloatRect& m_boxRect;
    FloatPoint& m_textOrigin;
    int m_emphasisMarkOffset;
    bool m_textBoxIsHorizontal;
    SavedDrawingStateForMask m_savedDrawingStateForMask;
};

} // namespace WebCore

#endif // TextPainter_h
