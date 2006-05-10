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

#ifndef GLYPH_BUFFER_H
#define GLYPH_BUFFER_H

// MAX_GLYPH_EXPANSION is the maximum numbers of glyphs that may be
// use to represent a single Unicode code point.
#define MAX_GLYPH_EXPANSION 4
#define GLYPH_BUFFER_SIZE 2048

#if __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

#include <wtf/Vector.h>

namespace WebCore
{
typedef unsigned short Glyph;
class FontData;

class GlyphBuffer
{
public:
    GlyphBuffer() {};
    
    bool isEmpty() const { return m_fontData.isEmpty(); }
    int size() const { return m_fontData.size(); }
    
#if __APPLE__
    Glyph* glyphs(int from) const { return ((Glyph*)m_glyphs.data()) + from; }
    CGSize* advances(int from) const { return ((CGSize*)m_advances.data()) + from; }
#endif

    const FontData* fontDataAt(int index) const { return m_fontData[index]; }
    
    void swap(int index1, int index2)
    {
        const FontData* f = m_fontData[index1];
        m_fontData[index1] = m_fontData[index2];
        m_fontData[index2] = f;

#if __APPLE__
        Glyph g = m_glyphs[index1];
        m_glyphs[index1] = m_glyphs[index2];
        m_glyphs[index2] = g;

        CGSize s = m_advances[index1];
        m_advances[index1] = m_advances[index2];
        m_advances[index2] = s;
#endif
    }

    Glyph glyphAt(int index) const
    {
#if __APPLE__
        return m_glyphs[index];
#else
        return 0;
#endif
    }

    float advanceAt(int index) const
    {
#if __APPLE__
        return m_advances[index].width;
#else
        return 0;
#endif
    }

    void add(Glyph glyph, const FontData* font, float width)
    {
        m_fontData.append(font);
#if __APPLE__
        m_glyphs.append(glyph);
        CGSize advance;
        advance.width = width;
        advance.height = 0;
        m_advances.append(advance);
#endif
    }
    
private:
    Vector<const FontData*, GLYPH_BUFFER_SIZE> m_fontData;
#if __APPLE__
    // Store the advances as CGSizes separately from the glyph indices.
    Vector<Glyph, GLYPH_BUFFER_SIZE> m_glyphs;
    Vector<CGSize, GLYPH_BUFFER_SIZE> m_advances;
#else
    // We will store cairo_glyphs, and they incorporate the glyph index as well as
    // the advances.
#endif
};

}
#endif
