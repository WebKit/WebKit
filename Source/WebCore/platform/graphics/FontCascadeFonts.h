/*
 * Copyright (C) 2006, 2010, 2013-2015 Apple Inc. All rights reserved.
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

#ifndef FontCascadeFonts_h
#define FontCascadeFonts_h

#include "Font.h"
#include "FontRanges.h"
#include "FontSelector.h"
#include "GlyphPage.h"
#include "WidthCache.h"
#include <wtf/Forward.h>
#include <wtf/MainThread.h>

#if PLATFORM(IOS)
#include "WebCoreThread.h"
#endif

namespace WebCore {

class GraphicsContext;
class IntRect;
class FontDescription;
class FontPlatformData;
class FontSelector;

class FontCascadeFonts : public RefCounted<FontCascadeFonts> {
    WTF_MAKE_NONCOPYABLE(FontCascadeFonts);
public:
    static Ref<FontCascadeFonts> create(PassRefPtr<FontSelector> fontSelector) { return adoptRef(*new FontCascadeFonts(fontSelector)); }
    static Ref<FontCascadeFonts> createForPlatformFont(const FontPlatformData& platformData) { return adoptRef(*new FontCascadeFonts(platformData)); }

    WEBCORE_EXPORT ~FontCascadeFonts();

    bool isForPlatformFont() const { return m_isForPlatformFont; }

    GlyphData glyphDataForCharacter(UChar32, const FontDescription&, FontVariant);

    bool isFixedPitch(const FontDescription&);
    void determinePitch(const FontDescription&);

    bool isLoadingCustomFonts() const;

    FontSelector* fontSelector() { return m_fontSelector.get(); }
    // FIXME: It should be possible to combine fontSelectorVersion and generation.
    unsigned fontSelectorVersion() const { return m_fontSelectorVersion; }
    unsigned generation() const { return m_generation; }

    WidthCache& widthCache() { return m_widthCache; }
    const WidthCache& widthCache() const { return m_widthCache; }

    const Font& primaryFont(const FontDescription&);
    WEBCORE_EXPORT const FontRanges& realizeFallbackRangesAt(const FontDescription&, unsigned fallbackIndex);

    void pruneSystemFallbacks();

private:
    FontCascadeFonts(PassRefPtr<FontSelector>);
    FontCascadeFonts(const FontPlatformData&);

    GlyphData glyphDataForSystemFallback(UChar32, const FontDescription&, FontVariant);
    GlyphData glyphDataForNormalVariant(UChar32, const FontDescription&);
    GlyphData glyphDataForVariant(UChar32, const FontDescription&, FontVariant, unsigned fallbackIndex);

    Vector<FontRanges, 1> m_realizedFallbackRanges;
    unsigned m_lastRealizedFallbackIndex { 0 };

    RefPtr<GlyphPage> m_cachedPageZero;
    HashMap<int, RefPtr<GlyphPage>> m_cachedPages;

    HashSet<RefPtr<Font>> m_systemFallbackFontSet;

    const Font* m_cachedPrimaryFont;
    RefPtr<FontSelector> m_fontSelector;

    WidthCache m_widthCache;

    unsigned m_fontSelectorVersion;
    unsigned short m_generation;
    Pitch m_pitch { UnknownPitch };
    bool m_isForPlatformFont { false };
};

inline bool FontCascadeFonts::isFixedPitch(const FontDescription& description)
{
    if (m_pitch == UnknownPitch)
        determinePitch(description);
    return m_pitch == FixedPitch;
};

inline const Font& FontCascadeFonts::primaryFont(const FontDescription& description)
{
    ASSERT(isMainThread());
    if (!m_cachedPrimaryFont) {
        auto& primaryRanges = realizeFallbackRangesAt(description, 0);
        m_cachedPrimaryFont = primaryRanges.fontForCharacter(' ');
        if (!m_cachedPrimaryFont)
            m_cachedPrimaryFont = &primaryRanges.fontForFirstRange();
    }
    return *m_cachedPrimaryFont;
}

}

#endif
