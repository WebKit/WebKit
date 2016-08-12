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
#include "FloatRect.h"
#include "ShadowData.h"
#include "TextFlags.h"
#include "TextPaintStyle.h"
#include <wtf/text/AtomicString.h>

namespace WebCore {

class FontCascade;
class RenderCombineText;
class TextRun;

struct TextPaintStyle;

enum RotationDirection { Counterclockwise, Clockwise };

static inline AffineTransform rotation(const FloatRect& boxRect, RotationDirection clockwise)
{
    return clockwise ? AffineTransform(0, 1, -1, 0, boxRect.x() + boxRect.maxY(), boxRect.maxY() - boxRect.x())
        : AffineTransform(0, -1, 1, 0, boxRect.x() - boxRect.maxY(), boxRect.x() + boxRect.maxY());
}

class TextPainter {
public:
    TextPainter(GraphicsContext&);
    
    void setTextPaintStyle(const TextPaintStyle& textPaintStyle) { m_textPaintStyle = textPaintStyle; }
    void setSelectionPaintStyle(const TextPaintStyle& selectionPaintStyle) { m_selectionPaintStyle = selectionPaintStyle; }
    void setIsHorizontal(bool isHorizontal) { m_textBoxIsHorizontal = isHorizontal; }
    void setFont(const FontCascade& font) { m_font = &font; }
    void addEmphasis(const AtomicString& emphasisMark, float emphasisMarkOffset, RenderCombineText*);
    void addTextShadow(const ShadowData* textShadow, const ShadowData* selectionShadow);

    void paintText(const TextRun&, unsigned length, const FloatRect& boxRect, const FloatPoint& textOrigin,
        unsigned selectionStart = 0, unsigned selectionEnd = 0, bool paintSelectedTextOnly = false, bool paintSelectedTextSeparately = false);

private:
    void drawTextOrEmphasisMarks(const FontCascade&, const TextRun&, const AtomicString& emphasisMark, float emphasisMarkOffset,
        const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset);
    void paintTextWithShadows(const ShadowData*, const FontCascade&, const TextRun&, const FloatRect& boxRect, const FloatPoint& textOrigin,
        unsigned startOffset, unsigned endOffset, const AtomicString& emphasisMark, float emphasisMarkOffset, bool stroked);
    void paintTextAndEmphasisMarksIfNeeded(const TextRun&, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset,
        const TextPaintStyle&, const ShadowData*);

    GraphicsContext& m_context;
    const FontCascade* m_font { nullptr };
    TextPaintStyle m_textPaintStyle;
    TextPaintStyle m_selectionPaintStyle;
    const ShadowData* m_textShadow { nullptr };
    const ShadowData* m_selectionShadow { nullptr };
    AtomicString m_emphasisMark;
    RenderCombineText* m_combinedText { nullptr };
    float m_emphasisMarkOffset { 0 };
    bool m_textBoxIsHorizontal { true };
};

inline void TextPainter::addEmphasis(const AtomicString& emphasisMark, float emphasisMarkOffset, RenderCombineText* combinedText)
{
    m_emphasisMark = emphasisMark;
    m_emphasisMarkOffset = emphasisMarkOffset;
    m_combinedText = combinedText;
}

inline void TextPainter::addTextShadow(const ShadowData* textShadow, const ShadowData* selectionShadow)
{
    m_textShadow = textShadow;
    m_selectionShadow = selectionShadow;
}

class ShadowApplier {
public:
    ShadowApplier(GraphicsContext&, const ShadowData*, const FloatRect& textRect, bool lastShadowIterationShouldDrawText = true, bool opaque = false, FontOrientation = Horizontal);
    FloatSize extraOffset() const { return m_extraOffset; }
    bool nothingToDraw() const { return m_nothingToDraw; }
    bool didSaveContext() const { return m_didSaveContext; }
    ~ShadowApplier();

private:
    bool isLastShadowIteration();
    bool shadowIsCompletelyCoveredByText(bool textIsOpaque);

    FloatSize m_extraOffset;
    GraphicsContext& m_context;
    const ShadowData* m_shadow;
    bool m_onlyDrawsShadow : 1;
    bool m_avoidDrawingShadow : 1;
    bool m_nothingToDraw : 1;
    bool m_didSaveContext : 1;
};

inline bool ShadowApplier::isLastShadowIteration()
{
    return m_shadow && !m_shadow->next();
}

inline bool ShadowApplier::shadowIsCompletelyCoveredByText(bool textIsOpaque)
{
    return textIsOpaque && m_shadow && m_shadow->location() == IntPoint() && !m_shadow->radius();
}

} // namespace WebCore

#endif // TextPainter_h
