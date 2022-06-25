/*
 * Copyright (C) 2003 - 2020 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
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

#ifndef WidthIterator_h
#define WidthIterator_h

#include "GlyphBuffer.h"
#include <unicode/umachine.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class FontCascade;
class Font;
class TextRun;
struct GlyphData;
struct GlyphIndexRange;
struct OriginalAdvancesForCharacterTreatedAsSpace;

using CharactersTreatedAsSpace = Vector<OriginalAdvancesForCharacterTreatedAsSpace, 64>;

struct WidthIterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WidthIterator(const FontCascade&, const TextRun&, HashSet<const Font*>* fallbackFonts = 0, bool accountForGlyphBounds = false, bool forTextEmphasis = false);

    void advance(unsigned to, GlyphBuffer&);
    bool advanceOneCharacter(float& width, GlyphBuffer&);
    void finalize(GlyphBuffer&);

    float maxGlyphBoundingBoxY() const { ASSERT(m_accountForGlyphBounds); return m_maxGlyphBoundingBoxY; }
    float minGlyphBoundingBoxY() const { ASSERT(m_accountForGlyphBounds); return m_minGlyphBoundingBoxY; }
    float firstGlyphOverflow() const { ASSERT(m_accountForGlyphBounds); return m_firstGlyphOverflow; }
    float lastGlyphOverflow() const { ASSERT(m_accountForGlyphBounds); return m_lastGlyphOverflow; }

    const TextRun& run() const { return m_run; }
    float runWidthSoFar() const { return m_runWidthSoFar; }
    unsigned currentCharacterIndex() const { return m_currentCharacterIndex; }

    static bool characterCanUseSimplifiedTextMeasuring(UChar, bool whitespaceIsCollapsed);

private:
    GlyphData glyphDataForCharacter(UChar32, bool mirror);
    template <typename TextIterator>
    inline void advanceInternal(TextIterator&, GlyphBuffer&);

    enum class TransformsType { None, Forced, NotForced };
    TransformsType shouldApplyFontTransforms(const GlyphBuffer&, unsigned lastGlyphCount, unsigned currentCharacterIndex) const;
    struct ApplyFontTransformsResult {
        float additionalAdvance;
        GlyphBufferAdvance initialAdvance;
    };
    ApplyFontTransformsResult applyFontTransforms(GlyphBuffer&, unsigned lastGlyphCount, const Font&, CharactersTreatedAsSpace&);
    void commitCurrentFontRange(GlyphBuffer&, unsigned& lastGlyphCount, unsigned currentCharacterIndex, const Font*&, const Font& newFont, const Font& primaryFont, UChar32 character, float& widthOfCurrentFontRange, float nextCharacterWidth, CharactersTreatedAsSpace&);
    void applyInitialAdvance(GlyphBuffer&, GlyphBufferAdvance initialAdvance, unsigned lastGlyphCount);

    bool hasExtraSpacing() const;
    void applyExtraSpacingAfterShaping(GlyphBuffer&, unsigned characterStartIndex, unsigned glyphBufferStartIndex, unsigned characterDestinationIndex, float startingRunWidth);
    void applyCSSVisibilityRules(GlyphBuffer&, unsigned glyphBufferStartIndex);

    struct AdditionalWidth {
        float left;
        float right;
        float leftExpansion;
        float rightExpansion;
    };
    AdditionalWidth calculateAdditionalWidth(GlyphBuffer&, GlyphBufferStringOffset currentCharacterIndex, unsigned leadingGlyphIndex, unsigned trailingGlyphIndex, float position) const;
    void applyAdditionalWidth(GlyphBuffer&, GlyphIndexRange, float leftAdditionalWidth, float rightAdditionalWidth, float leftExpansionAdditionalWidth, float rightExpansionAdditionalWidth);

    const FontCascade& m_font;
    const TextRun& m_run;
    HashSet<const Font*>* m_fallbackFonts { nullptr };

    std::optional<unsigned> m_lastCharacterIndex;
    GlyphBufferAdvance m_leftoverInitialAdvance { makeGlyphBufferAdvance() };
    unsigned m_currentCharacterIndex { 0 };
    float m_leftoverJustificationWidth { 0 };
    float m_runWidthSoFar { 0 };
    float m_expansion { 0 };
    float m_expansionPerOpportunity { 0 };
    float m_maxGlyphBoundingBoxY { std::numeric_limits<float>::lowest() };
    float m_minGlyphBoundingBoxY { std::numeric_limits<float>::max() };
    float m_firstGlyphOverflow { 0 };
    float m_lastGlyphOverflow { 0 };
    bool m_containsTabs { false };
    bool m_isAfterExpansion { false };
    bool m_accountForGlyphBounds { false };
    bool m_enableKerning { false };
    bool m_requiresShaping { false };
    bool m_forTextEmphasis { false };
};

}

#endif
