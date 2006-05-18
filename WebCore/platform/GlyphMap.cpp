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

#include "config.h"
#include "GlyphMap.h"
#include "FontData.h"

#include <unicode/uchar.h>

#define NO_BREAK_SPACE 0x00A0
#define ZERO_WIDTH_SPACE 0x200B

namespace WebCore
{

GlyphData GlyphMap::glyphDataForCharacter(UChar32 c, const FontData* fontData)
{
    unsigned pageNumber = (c / cGlyphPageSize);
    GlyphPage* page = locatePage(pageNumber, fontData);
    if (page)
        return page->glyphDataForCharacter(c);
    GlyphData data = { 0, fontData };
    return data;
}

void GlyphMap::setGlyphDataForCharacter(UChar32 c, Glyph glyph, const FontData* fontData)
{
    unsigned pageNumber = (c / cGlyphPageSize);
    GlyphPage* page = locatePage(pageNumber, fontData);
    if (page)
        page->setGlyphDataForCharacter(c, glyph, fontData);
}

inline GlyphMap::GlyphPage* GlyphMap::locatePage(unsigned pageNumber, const FontData* fontData)
{
    GlyphPage* page;
    if (pageNumber == 0) {
        if (m_filledPrimaryPage)
            return &m_primaryPage;
        page = &m_primaryPage; 
    } else {
        if (m_pages) {
            GlyphPage* result = m_pages->get(pageNumber);
            if (result)
                return result;
        }
        page = new GlyphPage;
    }
      
    unsigned start = pageNumber * cGlyphPageSize;
    unsigned short buffer[cGlyphPageSize * 2 + 2];
    unsigned bufferLength;
    unsigned i;

    // Fill in a buffer with the entire "page" of characters that we want to look up glyphs for.
    if (start < 0x10000) {
        bufferLength = cGlyphPageSize;
        for (i = 0; i < cGlyphPageSize; i++)
            buffer[i] = start + i;

        if (start == 0) {
            // Control characters must not render at all.
            for (i = 0; i < 0x20; ++i)
                buffer[i] = ZERO_WIDTH_SPACE;
            for (i = 0x7F; i < 0xA0; i++)
                buffer[i] = ZERO_WIDTH_SPACE;

            // \n, \t, and nonbreaking space must render as a space.
            buffer[(int)'\n'] = ' ';
            buffer[(int)'\t'] = ' ';
            buffer[NO_BREAK_SPACE] = ' ';
        }
    } else {
        bufferLength = cGlyphPageSize * 2;
        for (i = 0; i < cGlyphPageSize; i++) {
            int c = i + start;
            buffer[i * 2] = U16_LEAD(c);
            buffer[i * 2 + 1] = U16_TRAIL(c);
        }
    }
    
    // Now that we have a buffer full of characters, we want to get back an array
    // of glyph indices.  This part involves calling into the platform-specific 
    // routine of our glyph map for actually filling in the page with the glyphs.
    bool success = fillPage(page, buffer, bufferLength, fontData);
    if (!success) {
        if (pageNumber != 0)
            delete page;
        return 0;
    }
    
    if (pageNumber == 0)
        m_filledPrimaryPage = true;
    else {
        if (!m_pages)
            m_pages = new HashMap<int, GlyphPage*>;
        m_pages->set(pageNumber, page);
    }

    return page;
}

}
