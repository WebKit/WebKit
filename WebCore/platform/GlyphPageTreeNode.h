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

#ifndef GlyphMap_h
#define GlyphMap_h

#include <wtf/unicode/Unicode.h>
#include <wtf/Noncopyable.h>
#include <wtf/HashMap.h>

namespace WebCore {

class FontData;
class GlyphPageTreeNode;

typedef unsigned short Glyph;

struct GlyphData {    
    Glyph glyph;
    const FontData* fontData;
};

struct GlyphPage {
    GlyphPage()
        : m_owner(0)
    {
    }

    GlyphPage(GlyphPageTreeNode* owner)
        : m_owner(owner)
    {
    }

    static const size_t size = 256; // Covers Latin-1 in a single page.
    GlyphData m_glyphs[size];
    GlyphPageTreeNode* m_owner;

    const GlyphData& glyphDataForCharacter(UChar32 c) const { return m_glyphs[c % size]; }
    void setGlyphDataForCharacter(UChar32 c, Glyph g, const FontData* f)
    {
        setGlyphDataForIndex(c % size, g, f);
    }
    void setGlyphDataForIndex(unsigned index, Glyph g, const FontData* f)
    {
        m_glyphs[index].glyph = g;
        m_glyphs[index].fontData = f;
    }
    GlyphPageTreeNode* owner() const { return m_owner; }
    // Implemented by the platform.
    bool fill(UChar* characterBuffer, unsigned bufferLength, const FontData* fontData);
};

class GlyphPageTreeNode {
public:
    GlyphPageTreeNode()
        : m_parent(0)
        , m_page(0)
        , m_level(0)
        , m_isSystemFallback(false)
        , m_systemFallbackChild(0)
#ifndef NDEBUG
        , m_pageNumber(0)
#endif
    {
    }

    static GlyphPageTreeNode* getRootChild(const FontData* fontData, unsigned pageNumber)
    {
        return getRoot(pageNumber)->getChild(fontData, pageNumber);
    }

    GlyphPageTreeNode* parent() const { return m_parent; }
    GlyphPageTreeNode* getChild(const FontData*, unsigned pageNumber);

    GlyphPage* page() const { return m_page; }
    unsigned level() const { return m_level; }
    bool isSystemFallback() const { return m_isSystemFallback; }

private:
    static GlyphPageTreeNode* getRoot(unsigned pageNumber);
    void initializePage(const FontData*, unsigned pageNumber);

    GlyphPageTreeNode* m_parent;
    GlyphPage* m_page;
    unsigned m_level;
    bool m_isSystemFallback;
    HashMap<const FontData*, GlyphPageTreeNode*> m_children;
    GlyphPageTreeNode* m_systemFallbackChild;
#ifndef NDEBUG
    unsigned m_pageNumber;
#endif
};

} // namespace WebCore

#endif // GlyphPageTreeNode_h
