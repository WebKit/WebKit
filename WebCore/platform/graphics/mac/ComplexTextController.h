/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ComplexTextController_h
#define ComplexTextController_h

#include "GlyphBuffer.h"
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

class Font;
class SimpleFontData;
class TextRun;

class ComplexTextController {
public:
    ComplexTextController(const Font*, const TextRun&, bool mayUseNaturalWritingDirection = false, HashSet<const SimpleFontData*>* fallbackFonts = 0);

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
    class ComplexTextRun : public RefCounted<ComplexTextRun> {
    public:
#if USE(CORE_TEXT)
        static PassRefPtr<ComplexTextRun> create(CTRunRef ctRun, const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength)
        {
            return adoptRef(new ComplexTextRun(ctRun, fontData, characters, stringLocation, stringLength));
        }
#elif USE(ATSUI)
        static PassRefPtr<ComplexTextRun> create(ATSUTextLayout atsuTextLayout, const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr, bool directionalOverride)
        {
            return adoptRef(new ComplexTextRun(atsuTextLayout, fontData, characters, stringLocation, stringLength, ltr, directionalOverride));
        }
#endif
        static PassRefPtr<ComplexTextRun> create(const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr)
        {
            return adoptRef(new ComplexTextRun(fontData, characters, stringLocation, stringLength, ltr));
        }

        unsigned glyphCount() const { return m_glyphCount; }
        const SimpleFontData* fontData() const { return m_fontData; }
        const UChar* characters() const { return m_characters; }
        unsigned stringLocation() const { return m_stringLocation; }
        size_t stringLength() const { return m_stringLength; }
        CFIndex indexAt(size_t i) const { return m_indices[i]; }
        const CGGlyph* glyphs() const { return m_glyphs; }
        const CGSize* advances() const { return m_advances; }

    private:
#if USE(CORE_TEXT)
        ComplexTextRun(CTRunRef, const SimpleFontData*, const UChar* characters, unsigned stringLocation, size_t stringLength);
#elif USE(ATSUI)
        ComplexTextRun(ATSUTextLayout, const SimpleFontData*, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr, bool directionalOverride);
#endif
        ComplexTextRun(const SimpleFontData*, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr);

#if USE(ATSUI)
#ifdef BUILDING_ON_TIGER
        typedef UInt32 URefCon;
#endif
        static OSStatus overrideLayoutOperation(ATSULayoutOperationSelector, ATSULineRef, URefCon, void*, ATSULayoutOperationCallbackStatus*);
#endif

#if USE(CORE_TEXT)
        RetainPtr<CTRunRef> m_CTRun;
#endif
        unsigned m_glyphCount;
        const SimpleFontData* m_fontData;
        const UChar* m_characters;
        unsigned m_stringLocation;
        size_t m_stringLength;
#if USE(CORE_TEXT)
        RetainPtr<CFMutableDataRef> m_indicesData;
        const CFIndex* m_indices;
#elif USE(ATSUI)
        Vector<CFIndex, 64> m_indices;
#endif
        Vector<CGGlyph, 64> m_glyphsVector;
        const CGGlyph* m_glyphs;
        Vector<CGSize, 64> m_advancesVector;
        const CGSize* m_advances;
        bool m_directionalOverride;
    };

    void collectComplexTextRuns();
    void collectComplexTextRunsForCharacters(const UChar*, unsigned length, unsigned stringLocation, const SimpleFontData*);
    void adjustGlyphsAndAdvances();

    const Font& m_font;
    const TextRun& m_run;
    bool m_mayUseNaturalWritingDirection;

    Vector<UChar, 256> m_smallCapsBuffer;

    Vector<RefPtr<ComplexTextRun>, 16> m_complexTextRuns;
    Vector<CGSize, 256> m_adjustedAdvances;
    Vector<CGGlyph, 256> m_adjustedGlyphs;
 
    unsigned m_currentCharacter;
    int m_end;

    CGFloat m_totalWidth;

    float m_runWidthSoFar;
    unsigned m_numGlyphsSoFar;
    size_t m_currentRun;
    unsigned m_glyphInCurrentRun;
    unsigned m_characterInCurrentGlyph;
    float m_finalRoundingWidth;
    float m_padding;
    float m_padPerSpace;

    HashSet<const SimpleFontData*>* m_fallbackFonts;

    unsigned m_lastRoundingGlyph;
};

} // namespace WebCore

#endif // ComplexTextController_h
