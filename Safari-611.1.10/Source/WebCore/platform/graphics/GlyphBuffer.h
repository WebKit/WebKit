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

#include "FloatPoint.h"
#include "FloatSize.h"
#include "Glyph.h"
#include "GlyphBufferMembers.h"
#include <climits>
#include <limits>
#include <wtf/Vector.h>

namespace WebCore {

class Font;

class GlyphBuffer {
public:
    bool isEmpty() const { return m_fonts.isEmpty(); }
    unsigned size() const { return m_fonts.size(); }
    
    void clear()
    {
        m_fonts.clear();
        m_glyphs.clear();
        m_advances.clear();
        m_origins.clear();
        m_offsetsInString.clear();
    }

    const Font** fonts(unsigned from) { return m_fonts.data() + from; }
    GlyphBufferGlyph* glyphs(unsigned from) { return m_glyphs.data() + from; }
    GlyphBufferAdvance* advances(unsigned from) { return m_advances.data() + from; }
    GlyphBufferOrigin* origins(unsigned from) { return m_origins.data() + from; }
    GlyphBufferStringOffset* offsetsInString(unsigned from) { return m_offsetsInString.data() + from; }
    const Font* const * fonts(unsigned from) const { return m_fonts.data() + from; }
    const GlyphBufferGlyph* glyphs(unsigned from) const { return m_glyphs.data() + from; }
    const GlyphBufferAdvance* advances(unsigned from) const { return m_advances.data() + from; }
    const GlyphBufferOrigin* origins(unsigned from) const { return m_origins.data() + from; }
    const GlyphBufferStringOffset* offsetsInString(unsigned from) const { return m_offsetsInString.data() + from; }

    const Font& fontAt(unsigned index) const
    {
        ASSERT(m_fonts[index]);
        return *m_fonts[index];
    }
    Glyph glyphAt(unsigned index) const { return m_glyphs[index]; }
    GlyphBufferAdvance advanceAt(unsigned index) const { return m_advances[index]; }
    GlyphBufferOrigin originAt(unsigned index) const { return m_origins[index]; }
    GlyphBufferStringOffset stringOffsetAt(unsigned index) const { return m_offsetsInString[index]; }

    void setInitialAdvance(GlyphBufferAdvance initialAdvance) { m_initialAdvance = initialAdvance; }
    const GlyphBufferAdvance& initialAdvance() const { return m_initialAdvance; }
    void expandInitialAdvance(float width) { setWidth(m_initialAdvance, WebCore::width(m_initialAdvance) + width); }
    void expandInitialAdvance(GlyphBufferAdvance additionalAdvance)
    {
        setWidth(m_initialAdvance, width(m_initialAdvance) + width(additionalAdvance));
        setHeight(m_initialAdvance, height(m_initialAdvance) + height(additionalAdvance));
    }
    
    static constexpr GlyphBufferStringOffset noOffset = std::numeric_limits<GlyphBufferStringOffset>::max();
    void add(Glyph glyph, const Font& font, float width, GlyphBufferStringOffset offsetInString = noOffset)
    {
        GlyphBufferAdvance advance = makeGlyphBufferAdvance(width, 0);
        add(glyph, font, advance, offsetInString);
    }

    void add(Glyph glyph, const Font& font, GlyphBufferAdvance advance, GlyphBufferStringOffset offsetInString)
    {
        m_fonts.append(&font);
        m_glyphs.append(glyph);
        m_advances.append(advance);
        m_origins.append(makeGlyphBufferOrigin());
        m_offsetsInString.append(offsetInString);
    }

    void remove(unsigned location, unsigned length)
    {
        m_fonts.remove(location, length);
        m_glyphs.remove(location, length);
        m_advances.remove(location, length);
        m_origins.remove(location, length);
        m_offsetsInString.remove(location, length);
    }

    void makeHole(unsigned location, unsigned length, const Font* font)
    {
        ASSERT(location <= size());

        m_fonts.insertVector(location, Vector<const Font*>(length, font));
        m_glyphs.insertVector(location, Vector<GlyphBufferGlyph>(length, std::numeric_limits<GlyphBufferGlyph>::max()));
        m_advances.insertVector(location, Vector<GlyphBufferAdvance>(length, makeGlyphBufferAdvance()));
        m_origins.insertVector(location, Vector<GlyphBufferOrigin>(length, makeGlyphBufferOrigin()));
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
        setWidth(lastAdvance, WebCore::width(lastAdvance) + width);
    }

    void expandAdvance(unsigned index, float width)
    {
        ASSERT(index < size());
        auto& lastAdvance = m_advances[index];
        setWidth(lastAdvance, WebCore::width(lastAdvance) + width);
    }

    void expandLastAdvance(GlyphBufferAdvance expansion)
    {
        ASSERT(!isEmpty());
        GlyphBufferAdvance& lastAdvance = m_advances.last();
        setWidth(lastAdvance, width(lastAdvance) + width(expansion));
        setHeight(lastAdvance, height(lastAdvance) + height(expansion));
    }

    void shrink(unsigned truncationPoint)
    {
        m_fonts.shrink(truncationPoint);
        m_glyphs.shrink(truncationPoint);
        m_advances.shrink(truncationPoint);
        m_origins.shrink(truncationPoint);
        m_offsetsInString.shrink(truncationPoint);
    }

    // FontCascade::layoutText() returns a GlyphBuffer which includes layout information that is split
    // into "advances" and "origins". See the ASCII-art diagram in ComplexTextController.h
    // In order to get paint advances, we need to run this "flatten" operation.
    // This merges the layout advances and origins together,
    // leaves the paint advances in the "m_advances" field,
    // and zeros-out the origins in the "m_origins" field.
    void flatten()
    {
        for (unsigned i = 0; i < size(); ++i) {
            m_advances[i] = makeGlyphBufferAdvance(
                -x(m_origins[i]) + width(m_advances[i]) + (i + 1 < size() ? x(m_origins[i + 1]) : 0),
                -y(m_origins[i]) + height(m_advances[i]) + (i + 1 < size() ? y(m_origins[i + 1]) : 0)
            );
            m_origins[i] = makeGlyphBufferOrigin();
        }
    }

    bool isFlattened() const
    {
        for (unsigned i = 0; i < size(); ++i) {
            if (m_origins[i] != makeGlyphBufferOrigin())
                return false;
        }
        return true;
    }

private:
    void swap(unsigned index1, unsigned index2)
    {
        auto font = m_fonts[index1];
        m_fonts[index1] = m_fonts[index2];
        m_fonts[index2] = font;

        auto glyph = m_glyphs[index1];
        m_glyphs[index1] = m_glyphs[index2];
        m_glyphs[index2] = glyph;

        auto advance = m_advances[index1];
        m_advances[index1] = m_advances[index2];
        m_advances[index2] = advance;

        auto origin = m_origins[index1];
        m_origins[index1] = m_origins[index2];
        m_origins[index2] = origin;

        auto offset = m_offsetsInString[index1];
        m_offsetsInString[index1] = m_offsetsInString[index2];
        m_offsetsInString[index2] = offset;
    }

    Vector<const Font*, 1024> m_fonts;
    Vector<GlyphBufferGlyph, 1024> m_glyphs;
    Vector<GlyphBufferAdvance, 1024> m_advances;
    Vector<GlyphBufferOrigin, 1024> m_origins;
    Vector<GlyphBufferStringOffset, 1024> m_offsetsInString;
    GlyphBufferAdvance m_initialAdvance { makeGlyphBufferAdvance() };
};

}
