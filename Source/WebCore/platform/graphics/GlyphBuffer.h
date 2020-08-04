/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "FloatSize.h"
#include "Glyph.h"
#include <climits>
#include <limits>
#include <wtf/Vector.h>

#if USE(CG)
#include <CoreGraphics/CGGeometry.h>
#endif

namespace WebCore {

class Font;

#if USE(WINGDI)
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
    GlyphBufferAdvance(CGSize size)
        : CGSize(size)
    {
    }
    GlyphBufferAdvance(FloatSize size)
        : CGSize(size)
    {
    }
    GlyphBufferAdvance(float width, float height)
        : CGSize(CGSizeMake(width, height))
    {
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<GlyphBufferAdvance> decode(Decoder&);

    operator FloatSize() { return { static_cast<float>(this->CGSize::width), static_cast<float>(this->CGSize::height) }; }

    void setWidth(CGFloat width) { this->CGSize::width = width; }
    void setHeight(CGFloat height) { this->CGSize::height = height; }
    CGFloat width() const { return this->CGSize::width; }
    CGFloat height() const { return this->CGSize::height; }
};

template<class Encoder>
void GlyphBufferAdvance::encode(Encoder& encoder) const
{
    encoder << width();
    encoder << height();
}

template<class Decoder>
Optional<GlyphBufferAdvance> GlyphBufferAdvance::decode(Decoder& decoder)
{
    Optional<CGFloat> width;
    decoder >> width;
    if (!width)
        return WTF::nullopt;

    Optional<CGFloat> height;
    decoder >> height;
    if (!height)
        return WTF::nullopt;

    return GlyphBufferAdvance(CGSizeMake(*width, *height));
}

using GlyphBufferStringOffset = CFIndex;

#else

using GlyphBufferAdvance = FloatSize;
using GlyphBufferStringOffset = unsigned;

#endif // #if USE(CG)

inline FloatSize toFloatSize(const GlyphBufferAdvance& a)
{
    return FloatSize(a.width(), a.height());
}

class GlyphBuffer {
public:
    bool isEmpty() const { return m_fonts.isEmpty(); }
    unsigned size() const { return m_fonts.size(); }
    
    void clear()
    {
        m_fonts.clear();
        m_glyphs.clear();
        m_advances.clear();
        m_offsetsInString.clear();
    }

    const Font** fonts(unsigned from) { return m_fonts.data() + from; }
    GlyphBufferGlyph* glyphs(unsigned from) { return m_glyphs.data() + from; }
    GlyphBufferAdvance* advances(unsigned from) { return m_advances.data() + from; }
    GlyphBufferStringOffset* offsetsInString(unsigned from) { return m_offsetsInString.data() + from; }
    const Font* const * fonts(unsigned from) const { return m_fonts.data() + from; }
    const GlyphBufferGlyph* glyphs(unsigned from) const { return m_glyphs.data() + from; }
    const GlyphBufferAdvance* advances(unsigned from) const { return m_advances.data() + from; }
    const GlyphBufferStringOffset* offsetsInString(unsigned from) const { return m_offsetsInString.data() + from; }

    const Font* fontAt(unsigned index) const { return m_fonts[index]; }
    Glyph glyphAt(unsigned index) const { return m_glyphs[index]; }
    GlyphBufferAdvance advanceAt(unsigned index) const { return m_advances[index]; }
    GlyphBufferStringOffset stringOffsetAt(unsigned index) const { return m_offsetsInString[index]; }

    void setInitialAdvance(GlyphBufferAdvance initialAdvance) { m_initialAdvance = initialAdvance; }
    const GlyphBufferAdvance& initialAdvance() const { return m_initialAdvance; }
    
    static constexpr GlyphBufferStringOffset noOffset = std::numeric_limits<GlyphBufferStringOffset>::max();
    void add(Glyph glyph, const Font* font, float width, GlyphBufferStringOffset offsetInString = noOffset)
    {
        GlyphBufferAdvance advance;
        advance.setWidth(width);
        advance.setHeight(0);
        add(glyph, font, advance, offsetInString);
    }

    void add(Glyph glyph, const Font* font, GlyphBufferAdvance advance, GlyphBufferStringOffset offsetInString)
    {
        m_fonts.append(font);
        m_glyphs.append(glyph);
        m_advances.append(advance);
        m_offsetsInString.append(offsetInString);
    }

    void remove(unsigned location, unsigned length)
    {
        m_fonts.remove(location, length);
        m_glyphs.remove(location, length);
        m_advances.remove(location, length);
        m_offsetsInString.remove(location, length);
    }

    void makeHole(unsigned location, unsigned length, const Font* font)
    {
        ASSERT(location <= size());

        m_fonts.insertVector(location, Vector<const Font*>(length, font));
        m_glyphs.insertVector(location, Vector<GlyphBufferGlyph>(length, 0xFFFF));
        m_advances.insertVector(location, Vector<GlyphBufferAdvance>(length, GlyphBufferAdvance(0, 0)));
        m_offsetsInString.insertVector(location, Vector<GlyphBufferStringOffset>(length, 0));
    }

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

    void expandLastAdvance(GlyphBufferAdvance expansion)
    {
        ASSERT(!isEmpty());
        GlyphBufferAdvance& lastAdvance = m_advances.last();
        lastAdvance.setWidth(lastAdvance.width() + expansion.width());
        lastAdvance.setHeight(lastAdvance.height() + expansion.height());
    }

    void shrink(unsigned truncationPoint)
    {
        m_fonts.shrink(truncationPoint);
        m_glyphs.shrink(truncationPoint);
        m_advances.shrink(truncationPoint);
        m_offsetsInString.shrink(truncationPoint);
    }

private:
    void swap(unsigned index1, unsigned index2)
    {
        const Font* font = m_fonts[index1];
        m_fonts[index1] = m_fonts[index2];
        m_fonts[index2] = font;

        GlyphBufferGlyph glyph = m_glyphs[index1];
        m_glyphs[index1] = m_glyphs[index2];
        m_glyphs[index2] = glyph;

        GlyphBufferAdvance advance = m_advances[index1];
        m_advances[index1] = m_advances[index2];
        m_advances[index2] = advance;

        GlyphBufferStringOffset offset = m_offsetsInString[index1];
        m_offsetsInString[index1] = m_offsetsInString[index2];
        m_offsetsInString[index2] = offset;
    }

    Vector<const Font*, 1024> m_fonts;
    Vector<GlyphBufferGlyph, 1024> m_glyphs;
    Vector<GlyphBufferAdvance, 1024> m_advances;
    Vector<GlyphBufferStringOffset, 1024> m_offsetsInString;
    GlyphBufferAdvance m_initialAdvance;
};

}
