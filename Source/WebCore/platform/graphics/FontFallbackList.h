/*
 * Copyright (C) 2006, 2010, 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FontFallbackList_h
#define FontFallbackList_h

#include "FontSelector.h"
#include "SimpleFontData.h"
#include "WidthCache.h"
#include <wtf/Forward.h>
#include <wtf/MainThread.h>

namespace WebCore {

class Font;
class GlyphPageTreeNode;
class GraphicsContext;
class IntRect;
class FontDescription;
class FontPlatformData;
class FontSelector;

const int cAllFamiliesScanned = -1;

class FontGlyphs : public RefCounted<FontGlyphs> {
    WTF_MAKE_NONCOPYABLE(FontGlyphs);
public:
    typedef HashMap<int, GlyphPageTreeNode*, DefaultHash<int>::Hash> GlyphPages;

    class GlyphPagesStateSaver {
    public:
        GlyphPagesStateSaver(FontGlyphs& glyphs)
            : m_glyphs(glyphs)
            , m_pages(glyphs.m_pages)
            , m_pageZero(glyphs.m_pageZero)
        {
        }

        ~GlyphPagesStateSaver()
        {
            m_glyphs.m_pages = m_pages;
            m_glyphs.m_pageZero = m_pageZero;
        }

    private:
        FontGlyphs& m_glyphs;
        GlyphPages& m_pages;
        GlyphPageTreeNode* m_pageZero;
    };

    static PassRefPtr<FontGlyphs> create() { return adoptRef(new FontGlyphs()); }

    ~FontGlyphs() { releaseFontData(); }
    void invalidate(PassRefPtr<FontSelector>);
    
    bool isFixedPitch(const Font* f) const { if (m_pitch == UnknownPitch) determinePitch(f); return m_pitch == FixedPitch; };
    void determinePitch(const Font*) const;

    bool loadingCustomFonts() const { return m_loadingCustomFonts; }

    FontSelector* fontSelector() const { return m_fontSelector.get(); }
    // FIXME: It should be possible to combine fontSelectorVersion and generation.
    unsigned fontSelectorVersion() const { return m_fontSelectorVersion; }
    unsigned generation() const { return m_generation; }

    WidthCache& widthCache() const { return m_widthCache; }

private:
    FontGlyphs();

    const SimpleFontData* primarySimpleFontData(const Font* f)
    { 
        ASSERT(isMainThread());
        if (!m_cachedPrimarySimpleFontData)
            m_cachedPrimarySimpleFontData = primaryFontData(f)->fontDataForCharacter(' ');
        return m_cachedPrimarySimpleFontData;
    }

    const FontData* primaryFontData(const Font* f) const { return realizeFontDataAt(f, 0); }
    const FontData* realizeFontDataAt(const Font*, unsigned index) const;

    void setPlatformFont(const FontPlatformData&);

    void releaseFontData();
    
    mutable Vector<RefPtr<FontData>, 1> m_realizedFontData;
    mutable GlyphPages m_pages;
    mutable GlyphPageTreeNode* m_pageZero;
    mutable const SimpleFontData* m_cachedPrimarySimpleFontData;
    RefPtr<FontSelector> m_fontSelector;
    mutable WidthCache m_widthCache;
    unsigned m_fontSelectorVersion;
    mutable int m_familyIndex;
    unsigned short m_generation;
    mutable unsigned m_pitch : 3; // Pitch
    mutable bool m_loadingCustomFonts : 1;

    friend class Font;
};

}

#endif
