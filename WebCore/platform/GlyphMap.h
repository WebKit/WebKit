/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef GLYPH_MAP_H
#define GLYPH_MAP_H

#include <unicode/umachine.h>
#include "GlyphBuffer.h"
#include <wtf/Noncopyable.h>
#include <wtf/HashMap.h>

namespace WebCore {

// Covers Latin-1.
const unsigned cGlyphPageSize = 256;

class FontData;

struct GlyphData
{    
    Glyph glyph;
    const FontData* fontData;
};

class GlyphMap : Noncopyable
{
public:
    GlyphMap() : m_filledPrimaryPage(false), m_pages(0) {}
    ~GlyphMap() { deleteAllValues(*m_pages); delete m_pages; }

    GlyphData glyphDataForCharacter(UChar32, const FontData*);
    void setGlyphDataForCharacter(UChar32, Glyph, const FontData*);
    
private:
    struct GlyphPage {
        GlyphData m_glyphs[cGlyphPageSize];

        const GlyphData& glyphDataForCharacter(UChar32 c) const { return m_glyphs[c % cGlyphPageSize]; }
        void setGlyphDataForCharacter(UChar32 c, Glyph g, const FontData* f)
        {
            setGlyphDataForIndex(c % cGlyphPageSize, g, f);
        }
        
        void setGlyphDataForIndex(unsigned index, Glyph g, const FontData* f) {
            m_glyphs[index].glyph = g;
            m_glyphs[index].fontData = f;
        }
    };
    
    GlyphPage* locatePage(unsigned page, const FontData* fontData);
    bool fillPage(GlyphPage*, UChar* characterBuffer, unsigned bufferLength, const FontData* fontData);
    
    bool m_filledPrimaryPage;
    GlyphPage m_primaryPage; // We optimize for the page that contains Latin1.
    HashMap<int, GlyphPage*>* m_pages;
};

}
#endif
