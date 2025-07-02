/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
class TextRun;
class Text;

struct TextPaintStyle;

namespace Style {
struct TextShadow;
template<typename> struct Shadows;
using TextShadows = Shadows<TextShadow>;
}

static inline AffineTransform rotation(const FloatRect& boxRect, RotationDirection direction)
{
    return direction == RotationDirection::Clockwise
        ? AffineTransform(0, 1, -1, 0, boxRect.x() + boxRect.maxY(), boxRect.maxY() - boxRect.x())
        : AffineTransform(0, -1, 1, 0, boxRect.x() - boxRect.maxY(), boxRect.x() + boxRect.maxY());
}

class TextPainter {
public:
    TextPainter(GraphicsContext&, const FontCascade&, const RenderStyle&, const TextPaintStyle&, const Style::TextShadows&, const FilterOperations*, const AtomString& emphasisMark, float emphasisMarkOffset, const RenderCombineText*);

    void paintRange(const TextRun&, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned start, unsigned end);

    template<typename LayoutRun>
    void setGlyphDisplayListIfNeeded(const LayoutRun& run, const PaintInfo& paintInfo, const RenderStyle& style, const TextRun& textRun)
    {
        if (!TextPainter::shouldUseGlyphDisplayList(paintInfo, style))
            const_cast<LayoutRun&>(run).removeFromGlyphDisplayListCache();
        else
            m_glyphDisplayList = GlyphDisplayListCache::singleton().get(run, m_font, m_context, textRun, paintInfo);
    }

    static bool shouldUseGlyphDisplayList(const PaintInfo&, const RenderStyle&);
    WEBCORE_EXPORT static void setForceUseGlyphDisplayListForTesting(bool);
    WEBCORE_EXPORT static String cachedGlyphDisplayListsForTextNodeAsText(Text&, OptionSet<DisplayList::AsTextFlag>);
    WEBCORE_EXPORT static void clearGlyphDisplayListCacheForTesting();

private:
    template<typename LayoutRun>
    static RefPtr<const DisplayList::DisplayList> glyphDisplayListIfExists(const LayoutRun& run)
    {
        return GlyphDisplayListCache::singleton().getIfExists(run);
    }

    void paintTextOrEmphasisMarks(const FontCascade&, const TextRun&, const AtomString& emphasisMark, float emphasisMarkOffset,
        const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset);
    void paintTextWithShadows(const Style::TextShadows*, const FilterOperations*, const FontCascade&, const TextRun&, const FloatRect& boxRect, const FloatPoint& textOrigin,
        unsigned startOffset, unsigned endOffset, const AtomString& emphasisMark, float emphasisMarkOffset, bool stroked);
    void paintTextAndEmphasisMarksIfNeeded(const TextRun&, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset,
        const TextPaintStyle&, const Style::TextShadows&, const FilterOperations*);

    GraphicsContext& m_context;
    const CheckedRef<const FontCascade> m_font;
    const CheckedRef<const RenderStyle> m_renderStyle;
    TextPaintStyle m_style;
    AtomString m_emphasisMark;
    const Style::TextShadows& m_shadow;
    const FilterOperations* m_shadowColorFilter { nullptr };
    const CheckedPtr<const RenderCombineText> m_combinedText;
    RefPtr<const DisplayList::DisplayList> m_glyphDisplayList { nullptr };
    float m_emphasisMarkOffset { 0 };
    WritingMode m_writingMode;
};

class ShadowApplier {
public:
    ShadowApplier(const RenderStyle&, GraphicsContext&, const Style::TextShadow*, const FilterOperations* colorFilter, const FloatRect& textRect, bool isLastShadowIteration, bool lastShadowIterationShouldDrawText = true, bool opaque = false, bool ignoreWritingMode = false);
    FloatSize extraOffset() const { return m_extraOffset; }
    bool nothingToDraw() const { return m_nothingToDraw; }
    bool didSaveContext() const { return m_didSaveContext; }
    ~ShadowApplier();

private:
    bool isLastShadowIteration();
    bool shadowIsCompletelyCoveredByText(bool textIsOpaque);

    FloatSize m_extraOffset;
    GraphicsContext& m_context;
    const Style::TextShadow* m_shadow;
    bool m_onlyDrawsShadow : 1;
    bool m_avoidDrawingShadow : 1;
    bool m_nothingToDraw : 1;
    bool m_didSaveContext : 1;
};

} // namespace WebCore
