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

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GlyphDisplayListCache.h"
#include "RotationDirection.h"
#include "TextFlags.h"
#include "TextPaintStyle.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

class FilterOperations;
class FontCascade;
class RenderCombineText;
class ShadowData;
class TextRun;
class Text;

struct TextPaintStyle;

static inline AffineTransform rotation(const FloatRect& boxRect, RotationDirection direction)
{
    return direction == RotationDirection::Clockwise
        ? AffineTransform(0, 1, -1, 0, boxRect.x() + boxRect.maxY(), boxRect.maxY() - boxRect.x())
        : AffineTransform(0, -1, 1, 0, boxRect.x() - boxRect.maxY(), boxRect.x() + boxRect.maxY());
}

class TextPainter {
public:
    TextPainter(GraphicsContext&, const FontCascade&, const RenderStyle&);

    void setStyle(const TextPaintStyle& textPaintStyle) { m_style = textPaintStyle; }
    void setShadow(const ShadowData* shadow) { m_shadow = shadow; }
    void setShadowColorFilter(const FilterOperations* colorFilter) { m_shadowColorFilter = colorFilter; }
    void setIsHorizontal(bool isHorizontal) { m_textBoxIsHorizontal = isHorizontal; }
    void setEmphasisMark(const AtomString& mark, float offset, const RenderCombineText*);

    void paintRange(const TextRun&, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned start, unsigned end);

    template<typename LayoutRun>
    void setGlyphDisplayListIfNeeded(const LayoutRun& run, const PaintInfo& paintInfo, const TextRun& textRun)
    {
        if (!TextPainter::shouldUseGlyphDisplayList(paintInfo))
            const_cast<LayoutRun&>(run).removeFromGlyphDisplayListCache();
        else
            m_glyphDisplayList = GlyphDisplayListCache::singleton().get(run, m_font, m_context, textRun);
    }

    static bool shouldUseGlyphDisplayList(const PaintInfo&);
    WEBCORE_EXPORT static void setForceUseGlyphDisplayListForTesting(bool);
    WEBCORE_EXPORT static String cachedGlyphDisplayListsForTextNodeAsText(Text&, OptionSet<DisplayList::AsTextFlag>);
    WEBCORE_EXPORT static void clearGlyphDisplayListCacheForTesting();

private:
    template<typename LayoutRun>
    static DisplayList::DisplayList* glyphDisplayListIfExists(const LayoutRun& run)
    {
        return GlyphDisplayListCache::singleton().getIfExists(run);
    }

    void paintTextOrEmphasisMarks(const FontCascade&, const TextRun&, const AtomString& emphasisMark, float emphasisMarkOffset,
        const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset);
    void paintTextWithShadows(const ShadowData*, const FilterOperations*, const FontCascade&, const TextRun&, const FloatRect& boxRect, const FloatPoint& textOrigin,
        unsigned startOffset, unsigned endOffset, const AtomString& emphasisMark, float emphasisMarkOffset, bool stroked);
    void paintTextAndEmphasisMarksIfNeeded(const TextRun&, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset,
        const TextPaintStyle&, const ShadowData*, const FilterOperations*);

    GraphicsContext& m_context;
    const FontCascade& m_font;
    const RenderStyle& m_renderStyle;
    TextPaintStyle m_style;
    AtomString m_emphasisMark;
    const ShadowData* m_shadow { nullptr };
    const FilterOperations* m_shadowColorFilter { nullptr };
    const RenderCombineText* m_combinedText { nullptr };
    DisplayList::DisplayList* m_glyphDisplayList { nullptr };
    float m_emphasisMarkOffset { 0 };
    bool m_textBoxIsHorizontal { true };
};

inline void TextPainter::setEmphasisMark(const AtomString& mark, float offset, const RenderCombineText* combinedText)
{
    m_emphasisMark = mark;
    m_emphasisMarkOffset = offset;
    m_combinedText = combinedText;
}

class ShadowApplier {
public:
    ShadowApplier(const RenderStyle&, GraphicsContext&, const ShadowData*, const FilterOperations* colorFilter, const FloatRect& textRect, bool lastShadowIterationShouldDrawText = true, bool opaque = false, FontOrientation = FontOrientation::Horizontal);
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

} // namespace WebCore
