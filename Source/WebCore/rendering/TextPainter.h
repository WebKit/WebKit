/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextPainter_h
#define TextPainter_h

#include "AffineTransform.h"
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
    
    const FloatRect& boxRect() const { return m_boxRect; }
    void paintText();
    void paintTextInContext(GraphicsContext&, float amountToIncreaseStrokeWidthBy);

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
