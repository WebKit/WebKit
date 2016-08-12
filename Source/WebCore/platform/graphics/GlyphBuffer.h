/*
 * Copyright (C) 2006, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile Inc.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#ifndef GlyphBuffer_h
#define GlyphBuffer_h

#include "FloatSize.h"
#include "Glyph.h"
#include <climits>
#include <wtf/Vector.h>

#if USE(CG)
#include <CoreGraphics/CGGeometry.h>
#endif

#if USE(CAIRO)
#include <cairo.h>
#endif

namespace WebCore {

class Font;

#if USE(CAIRO)
// FIXME: Why does Cairo use such a huge struct instead of just an offset into an array?
typedef cairo_glyph_t GlyphBufferGlyph;
#elif USE(WINGDI)
typedef wchar_t GlyphBufferGlyph;
#else
typedef Glyph GlyphBufferGlyph;
#endif

// CG uses CGSize instead of FloatSize so that the result of advances()
// can be passed directly to CGContextShowGlyphsWithAdvances in FontMac.mm
#if USE(CG)
struct GlyphBufferAdvance : CGSize {
public:
    GlyphBufferAdvance() : CGSize(CGSizeZero) { }
    GlyphBufferAdvance(CGSize size) : CGSize(size)
    {
    }

    void setWidth(CGFloat width) { this->CGSize::width = width; }
    CGFloat width() const { return this->CGSize::width; }
    CGFloat height() const { return this->CGSize::height; }
};
#else
typedef FloatSize GlyphBufferAdvance;
#endif

class GlyphBuffer {
public:
    bool isEmpty() const { return m_font.isEmpty(); }
    unsigned size() const { return m_font.size(); }
    
    void clear()
    {
        m_font.clear();
        m_glyphs.clear();
        m_advances.clear();
        if (m_offsetsInString)
            m_offsetsInString->clear();
#if PLATFORM(WIN)
        m_offsets.clear();
#endif
    }

    GlyphBufferGlyph* glyphs(unsigned from) { return m_glyphs.data() + from; }
    GlyphBufferAdvance* advances(unsigned from) { return m_advances.data() + from; }
    const GlyphBufferGlyph* glyphs(unsigned from) const { return m_glyphs.data() + from; }
    const GlyphBufferAdvance* advances(unsigned from) const { return m_advances.data() + from; }

    const Font* fontAt(unsigned index) const { return m_font[index]; }

    void setInitialAdvance(GlyphBufferAdvance initialAdvance) { m_initialAdvance = initialAdvance; }
    const GlyphBufferAdvance& initialAdvance() const { return m_initialAdvance; }

    void setLeadingExpansion(float leadingExpansion) { m_leadingExpansion = leadingExpansion; }
    float leadingExpansion() const { return m_leadingExpansion; }
    
    Glyph glyphAt(unsigned index) const
    {
#if USE(CAIRO)
        return m_glyphs[index].index;
#else
        return m_glyphs[index];
#endif
    }

    GlyphBufferAdvance advanceAt(unsigned index) const
    {
        return m_advances[index];
    }

    FloatSize offsetAt(unsigned index) const
    {
#if PLATFORM(WIN)
        return m_offsets[index];
#else
        UNUSED_PARAM(index);
        return FloatSize();
#endif
    }
    
    static const unsigned noOffset = UINT_MAX;
    void add(Glyph glyph, const Font* font, float width, unsigned offsetInString = noOffset, const FloatSize* offset = 0)
    {
        m_font.append(font);

#if USE(CAIRO)
        cairo_glyph_t cairoGlyph;
        cairoGlyph.index = glyph;
        m_glyphs.append(cairoGlyph);
#else
        m_glyphs.append(glyph);
#endif

#if USE(CG)
        CGSize advance = { width, 0 };
        m_advances.append(advance);
#else
        m_advances.append(FloatSize(width, 0));
#endif

#if PLATFORM(WIN)
        if (offset)
            m_offsets.append(*offset);
        else
            m_offsets.append(FloatSize());
#else
        UNUSED_PARAM(offset);
#endif
        
        if (offsetInString != noOffset && m_offsetsInString)
            m_offsetsInString->append(offsetInString);
    }
    
#if !USE(WINGDI)
    void add(Glyph glyph, const Font* font, GlyphBufferAdvance advance, unsigned offsetInString = noOffset)
    {
        m_font.append(font);
#if USE(CAIRO)
        cairo_glyph_t cairoGlyph;
        cairoGlyph.index = glyph;
        m_glyphs.append(cairoGlyph);
#else
        m_glyphs.append(glyph);
#endif

        m_advances.append(advance);
        
        if (offsetInString != noOffset && m_offsetsInString)
            m_offsetsInString->append(offsetInString);
    }
#endif

    void reverse(unsigned from, unsigned length)
    {
        for (unsigned i = from, end = from + length - 1; i < end; ++i, --end)
            swap(i, end);
    }

    void expandLastAdvance(float width)
    {
        ASSERT(!isEmpty());
        GlyphBufferAdvance& lastAdvance = m_advances.last();
        lastAdvance.setWidth(lastAdvance.width() + width);
    }
    
    void saveOffsetsInString()
    {
        m_offsetsInString.reset(new Vector<unsigned, 2048>());
    }

    int offsetInString(unsigned index) const
    {
        ASSERT(m_offsetsInString);
        return (*m_offsetsInString)[index];
    }

    void shrink(unsigned truncationPoint)
    {
        m_font.shrink(truncationPoint);
        m_glyphs.shrink(truncationPoint);
        m_advances.shrink(truncationPoint);
        if (m_offsetsInString)
            m_offsetsInString->shrink(truncationPoint);
#if PLATFORM(WIN)
        m_offsets.shrink(truncationPoint);
#endif
    }

private:
    void swap(unsigned index1, unsigned index2)
    {
        const Font* f = m_font[index1];
        m_font[index1] = m_font[index2];
        m_font[index2] = f;

        GlyphBufferGlyph g = m_glyphs[index1];
        m_glyphs[index1] = m_glyphs[index2];
        m_glyphs[index2] = g;

        GlyphBufferAdvance s = m_advances[index1];
        m_advances[index1] = m_advances[index2];
        m_advances[index2] = s;

#if PLATFORM(WIN)
        FloatSize offset = m_offsets[index1];
        m_offsets[index1] = m_offsets[index2];
        m_offsets[index2] = offset;
#endif
    }

    Vector<const Font*, 2048> m_font;
    Vector<GlyphBufferGlyph, 2048> m_glyphs;
    Vector<GlyphBufferAdvance, 2048> m_advances;
    GlyphBufferAdvance m_initialAdvance;
    std::unique_ptr<Vector<unsigned, 2048>> m_offsetsInString;
#if PLATFORM(WIN)
    Vector<FloatSize, 2048> m_offsets;
#endif
    float m_leadingExpansion;
};

}
#endif
