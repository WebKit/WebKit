/*
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef GlyphWidthMap_h
#define GlyphWidthMap_h

#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

typedef unsigned short Glyph;

const float cGlyphWidthUnknown = -1;

class GlyphWidthMap : public Noncopyable {
public:
    GlyphWidthMap() : m_filledPrimaryPage(false) { }
    ~GlyphWidthMap() { if (m_pages) { deleteAllValues(*m_pages); } }

    float widthForGlyph(Glyph glyph)
    {
        return locatePage(glyph / GlyphWidthPage::size)->widthForGlyph(glyph);
    }

    void setWidthForGlyph(Glyph glyph, float width)
    {
        locatePage(glyph / GlyphWidthPage::size)->setWidthForGlyph(glyph, width);
    }

private:
    struct GlyphWidthPage {
        static const size_t size = 256; // Usually covers Latin-1 in a single page.
        float m_widths[size];

        float widthForGlyph(Glyph glyph) const { return m_widths[glyph % size]; }
        void setWidthForGlyph(Glyph glyph, float width)
        {
            setWidthForIndex(glyph % size, width);
        }
        void setWidthForIndex(unsigned index, float width)
        {
            m_widths[index] = width;
        }
    };
    
    GlyphWidthPage* locatePage(unsigned pageNumber)
    {
        if (!pageNumber && m_filledPrimaryPage)
            return &m_primaryPage;
        return locatePageSlowCase(pageNumber);
    }

    GlyphWidthPage* locatePageSlowCase(unsigned pageNumber);
    
    bool m_filledPrimaryPage;
    GlyphWidthPage m_primaryPage; // We optimize for the page that contains glyph indices 0-255.
    OwnPtr<HashMap<int, GlyphWidthPage*> > m_pages;
};

}

#endif
