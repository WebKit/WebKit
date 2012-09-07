/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
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

class FontFallbackList : public RefCounted<FontFallbackList> {
    WTF_MAKE_NONCOPYABLE(FontFallbackList);
public:
    static PassRefPtr<FontFallbackList> create() { return adoptRef(new FontFallbackList()); }

    ~FontFallbackList() { releaseFontData(); }
    void invalidate(PassRefPtr<FontSelector>);
    
    bool isFixedPitch(const Font* f) const { if (m_pitch == UnknownPitch) determinePitch(f); return m_pitch == FixedPitch; };
    void determinePitch(const Font*) const;

    bool loadingCustomFonts() const { return m_loadingCustomFonts; }

    FontSelector* fontSelector() const { return m_fontSelector.get(); }
    // FIXME: It should be possible to combine fontSelectorVersion and generation.
    unsigned fontSelectorVersion() const { return m_fontSelectorVersion; }
    unsigned generation() const { return m_generation; }

    struct GlyphPagesHashTraits : HashTraits<int> {
        static const int minimumTableSize = 16;
    };
    typedef HashMap<int, GlyphPageTreeNode*, DefaultHash<int>::Hash, GlyphPagesHashTraits> GlyphPages;
    GlyphPageTreeNode* glyphPageZero() const { return m_pageZero; }
    const GlyphPages& glyphPages() const { return m_pages; }

private:
    enum IsCustomFontOrNot { IsNotCustomFont, IsCustomFont };

    FontFallbackList();

    const SimpleFontData* primarySimpleFontData(const Font* f)
    { 
        ASSERT(isMainThread());
        if (!m_cachedPrimarySimpleFontData)
            m_cachedPrimarySimpleFontData = primaryFontData(f)->fontDataForCharacter(' ');
        return m_cachedPrimarySimpleFontData;
    }

    const FontData* primaryFontData(const Font* f) const { return fontDataAt(f, 0); }
    const FontData* fontDataAt(const Font*, unsigned index) const;

    void setPlatformFont(const FontPlatformData&);

    void setGlyphPageZero(GlyphPageTreeNode* pageZero) { m_pageZero = pageZero; }
    void setGlyphPages(const GlyphPages& pages) { m_pages = pages; }

    void appendFontData(const FontData*) const;
    void releaseFontData() const;

    // A custom FontData pointer in this Vector is owned by FontFallbackList and
    // deleted when no FontFallbackList is using it. A non-custom FontData pointer
    // is owned by the FontCache and is deleted when the font is purged.
    // FIXME: Shift to RefPtr for all FontData objects and modify this pair to be a custom class.
    // https://bugs.webkit.org/show_bug.cgi?id=95866
    mutable Vector<pair<const FontData*, IsCustomFontOrNot>, 1> m_fontList;
    mutable GlyphPages m_pages;
    mutable GlyphPageTreeNode* m_pageZero;
    mutable const SimpleFontData* m_cachedPrimarySimpleFontData;
    RefPtr<FontSelector> m_fontSelector;
    unsigned m_fontSelectorVersion;
    mutable int m_familyIndex;
    unsigned short m_generation;
    mutable unsigned m_pitch : 3; // Pitch
    mutable bool m_loadingCustomFonts : 1;

    friend class Font;
    friend class SVGTextRunRenderingContext;
};

}

#endif
