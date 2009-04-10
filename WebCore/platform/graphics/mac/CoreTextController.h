/*
 * Copyright (C) 2007, 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CoreTextController_h
#define CoreTextController_h

#if USE(CORE_TEXT)

#include "Font.h"
#include "GlyphBuffer.h"
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CoreTextController {
public:
    CoreTextController(const Font*, const TextRun&, bool mayUseNaturalWritingDirection = false, HashSet<const SimpleFontData*>* fallbackFonts = 0);

    // Advance and emit glyphs up to the specified character.
    void advance(unsigned to, GlyphBuffer* = 0);

    // Compute the character offset for a given x coordinate.
    int offsetForPosition(int x, bool includePartialGlyphs);

    // Returns the width of everything we've consumed so far.
    float runWidthSoFar() const { return m_runWidthSoFar; }

    float totalWidth() const { return m_totalWidth; }

    // Extra width to the left of the leftmost glyph.
    float finalRoundingWidth() const { return m_finalRoundingWidth; }

private:
    class CoreTextRun {
    public:
        CoreTextRun(CTRunRef, const SimpleFontData*, const UChar* characters, unsigned stringLocation, size_t stringLength);
        CoreTextRun(const SimpleFontData*, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr);

        CTRunRef ctRun() const { return m_CTRun.get(); }
        unsigned glyphCount() const { return m_glyphCount; }
        const SimpleFontData* fontData() const { return m_fontData; }
        const UChar* characters() const { return m_characters; }
        unsigned stringLocation() const { return m_stringLocation; }
        size_t stringLength() const { return m_stringLength; }
        CFIndex indexAt(size_t i) const { return m_indices[i]; }

    private:
        RetainPtr<CTRunRef> m_CTRun;
        unsigned m_glyphCount;
        const SimpleFontData* m_fontData;
        const UChar* m_characters;
        unsigned m_stringLocation;
        size_t m_stringLength;
        const CFIndex* m_indices;
        // Used only if CTRunGet*Ptr fails or if this is a missing glyphs run.
        RetainPtr<CFMutableDataRef> m_indicesData;
    };

    void collectCoreTextRuns();
    void collectCoreTextRunsForCharacters(const UChar*, unsigned length, unsigned stringLocation, const SimpleFontData*);
    void adjustGlyphsAndAdvances();

    const Font& m_font;
    const TextRun& m_run;
    bool m_mayUseNaturalWritingDirection;

    Vector<UChar, 256> m_smallCapsBuffer;

    Vector<CoreTextRun, 16> m_coreTextRuns;
    Vector<CGSize, 256> m_adjustedAdvances;
    Vector<CGGlyph, 256> m_adjustedGlyphs;
 
    unsigned m_currentCharacter;
    int m_end;

    CGFloat m_totalWidth;

    float m_runWidthSoFar;
    unsigned m_numGlyphsSoFar;
    size_t m_currentRun;
    unsigned m_glyphInCurrentRun;
    float m_finalRoundingWidth;
    float m_padding;
    float m_padPerSpace;

    HashSet<const SimpleFontData*>* m_fallbackFonts;

    unsigned m_lastRoundingGlyph;
};

} // namespace WebCore
#endif // USE(CORE_TEXT)
#endif // CoreTextController_h
