/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef GlyphPage_h
#define GlyphPage_h

#include "Glyph.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

class SimpleFontData;
class GlyphPageTreeNode;

// Holds the glyph index and the corresponding SimpleFontData information for a given
// character.
struct GlyphData {
    GlyphData(Glyph g = 0, const SimpleFontData* f = 0)
        : glyph(g)
        , fontData(f)
    {
    }
    Glyph glyph;
    const SimpleFontData* fontData;
};

// A GlyphPage contains a fixed-size set of GlyphData mappings for a contiguous
// range of characters in the Unicode code space. GlyphPages are indexed
// starting from 0 and incrementing for each 256 glyphs.
//
// One page may actually include glyphs from other fonts if the characters are
// missing in the primary font. It is owned by exactly one GlyphPageTreeNode,
// although multiple nodes may reference it as their "page" if they are supposed
// to be overriding the parent's node, but provide no additional information.
class GlyphPage : public RefCounted<GlyphPage> {
public:
    static PassRefPtr<GlyphPage> createUninitialized(GlyphPageTreeNode* owner)
    {
        return adoptRef(new GlyphPage(owner, false));
    }

    static PassRefPtr<GlyphPage> createZeroedSystemFallbackPage(GlyphPageTreeNode* owner)
    {
        return adoptRef(new GlyphPage(owner, true));
    }

    PassRefPtr<GlyphPage> createCopiedSystemFallbackPage(GlyphPageTreeNode* owner) const
    {
        RefPtr<GlyphPage> page = GlyphPage::createUninitialized(owner);
        memcpy(page->m_glyphs, m_glyphs, sizeof(m_glyphs));
        page->m_fontDataForAllGlyphs = m_fontDataForAllGlyphs;
        if (m_perGlyphFontData) {
            page->m_perGlyphFontData = static_cast<const SimpleFontData**>(fastMalloc(size * sizeof(SimpleFontData*)));
            memcpy(page->m_perGlyphFontData, m_perGlyphFontData, size * sizeof(SimpleFontData*));
        }
        return page.release();
    }

    ~GlyphPage()
    {
        if (m_perGlyphFontData)
            fastFree(m_perGlyphFontData);
    }

    static const size_t size = 256; // Covers Latin-1 in a single page.

    unsigned indexForCharacter(UChar32 c) const { return c % size; }
    GlyphData glyphDataForCharacter(UChar32 c) const
    {
        return glyphDataForIndex(indexForCharacter(c));
    }

    GlyphData glyphDataForIndex(unsigned index) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(index < size);
        Glyph glyph = m_glyphs[index];
        if (!glyph)
            return GlyphData(0, 0);
        if (m_perGlyphFontData)
            return GlyphData(glyph, m_perGlyphFontData[index]);
        return GlyphData(glyph, m_fontDataForAllGlyphs);
    }

    Glyph glyphAt(unsigned index) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(index < size);
        return m_glyphs[index];
    }

    const SimpleFontData* fontDataForCharacter(UChar32 c) const
    {
        return glyphDataForIndex(indexForCharacter(c)).fontData;
    }

    void setGlyphDataForCharacter(UChar32 c, Glyph g, const SimpleFontData* f)
    {
        setGlyphDataForIndex(indexForCharacter(c), g, f);
    }

    void setGlyphDataForIndex(unsigned index, Glyph glyph, const SimpleFontData* fontData)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(index < size);
        m_glyphs[index] = glyph;

        // GlyphPage getters will always return a null SimpleFontData* for glyph #0, so don't worry about the pointer for them.
        if (!glyph)
            return;

        // A glyph index without a font data pointer makes no sense.
        ASSERT(fontData);

        if (m_perGlyphFontData) {
            m_perGlyphFontData[index] = fontData;
            return;
        }

        if (!m_fontDataForAllGlyphs)
            m_fontDataForAllGlyphs = fontData;

        if (m_fontDataForAllGlyphs == fontData)
            return;

        // This GlyphPage houses glyphs from multiple fonts, transition to an array of SimpleFontData pointers.
        const SimpleFontData* oldFontData = m_fontDataForAllGlyphs;
        m_perGlyphFontData = static_cast<const SimpleFontData**>(fastMalloc(size * sizeof(SimpleFontData*)));
        for (unsigned i = 0; i < size; ++i)
            m_perGlyphFontData[i] = oldFontData;
        m_perGlyphFontData[index] = fontData;
    }

    void setGlyphDataForIndex(unsigned index, const GlyphData& glyphData)
    {
        setGlyphDataForIndex(index, glyphData.glyph, glyphData.fontData);
    }

    void clearForFontData(const SimpleFontData* fontData)
    {
        if (!m_perGlyphFontData) {
            if (m_fontDataForAllGlyphs == fontData) {
                memset(m_glyphs, 0, sizeof(m_glyphs));
                m_fontDataForAllGlyphs = 0;
            }
            return;
        }
        for (size_t i = 0; i < size; ++i) {
            if (m_perGlyphFontData[i] == fontData) {
                m_glyphs[i] = 0;
                m_perGlyphFontData[i] = 0;
            }
        }
    }

    GlyphPageTreeNode* owner() const { return m_owner; }

    // Implemented by the platform.
    bool fill(unsigned offset, unsigned length, UChar* characterBuffer, unsigned bufferLength, const SimpleFontData*);

private:
    GlyphPage(GlyphPageTreeNode* owner, bool clearGlyphs)
        : m_fontDataForAllGlyphs(0)
        , m_perGlyphFontData(0)
        , m_owner(owner)
    {
        if (clearGlyphs)
            memset(m_glyphs, 0, sizeof(m_glyphs));
    }

    const SimpleFontData* m_fontDataForAllGlyphs;
    const SimpleFontData** m_perGlyphFontData;

    GlyphPageTreeNode* m_owner;
    Glyph m_glyphs[size];
};

} // namespace WebCore

#endif // GlyphPage_h
