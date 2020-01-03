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

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThread.h"
#endif

namespace WebCore {

class FontCascadeDescription;
class FontPlatformData;
class FontSelector;
class GraphicsContext;
class IntRect;
class MixedFontGlyphPage;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(FontCascadeFonts);
class FontCascadeFonts : public RefCounted<FontCascadeFonts> {
    WTF_MAKE_NONCOPYABLE(FontCascadeFonts);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FontCascadeFonts);
public:
    static Ref<FontCascadeFonts> create(RefPtr<FontSelector>&& fontSelector) { return adoptRef(*new FontCascadeFonts(WTFMove(fontSelector))); }
    static Ref<FontCascadeFonts> createForPlatformFont(const FontPlatformData& platformData) { return adoptRef(*new FontCascadeFonts(platformData)); }

    WEBCORE_EXPORT ~FontCascadeFonts();

    bool isForPlatformFont() const { return m_isForPlatformFont; }

    GlyphData glyphDataForCharacter(UChar32, const FontCascadeDescription&, FontVariant);

    bool isFixedPitch(const FontCascadeDescription&);
    void determinePitch(const FontCascadeDescription&);

    bool isLoadingCustomFonts() const;

    FontSelector* fontSelector() { return m_fontSelector.get(); }
    // FIXME: It should be possible to combine fontSelectorVersion and generation.
    unsigned fontSelectorVersion() const { return m_fontSelectorVersion; }
    unsigned generation() const { return m_generation; }

    WidthCache& widthCache() { return m_widthCache; }
    const WidthCache& widthCache() const { return m_widthCache; }

    const Font& primaryFont(const FontCascadeDescription&);
    WEBCORE_EXPORT const FontRanges& realizeFallbackRangesAt(const FontCascadeDescription&, unsigned fallbackIndex);

    void pruneSystemFallbacks();

private:
    FontCascadeFonts(RefPtr<FontSelector>&&);
    FontCascadeFonts(const FontPlatformData&);

    GlyphData glyphDataForSystemFallback(UChar32, const FontCascadeDescription&, FontVariant, bool systemFallbackShouldBeInvisible);
    GlyphData glyphDataForVariant(UChar32, const FontCascadeDescription&, FontVariant, unsigned fallbackIndex = 0);

    Vector<FontRanges, 1> m_realizedFallbackRanges;
    unsigned m_lastRealizedFallbackIndex { 0 };

    class GlyphPageCacheEntry {
    public:
        GlyphData glyphDataForCharacter(UChar32);

        void setSingleFontPage(RefPtr<GlyphPage>&&);
        void setGlyphDataForCharacter(UChar32, GlyphData);

        bool isNull() const { return !m_singleFont && !m_mixedFont; }
        bool isMixedFont() const { return !!m_mixedFont; }
    
    private:
        // Only one of these is non-null.
        RefPtr<GlyphPage> m_singleFont;
        std::unique_ptr<MixedFontGlyphPage> m_mixedFont;
    };

    GlyphPageCacheEntry m_cachedPageZero;
    HashMap<int, GlyphPageCacheEntry> m_cachedPages;

    HashSet<RefPtr<Font>> m_systemFallbackFontSet;

    const Font* m_cachedPrimaryFont;
    RefPtr<FontSelector> m_fontSelector;

    WidthCache m_widthCache;

    unsigned m_fontSelectorVersion;
    unsigned short m_generation;
    Pitch m_pitch { UnknownPitch };
    bool m_isForPlatformFont { false };
};

inline bool FontCascadeFonts::isFixedPitch(const FontCascadeDescription& description)
{
    if (m_pitch == UnknownPitch)
        determinePitch(description);
    return m_pitch == FixedPitch;
};

inline const Font& FontCascadeFonts::primaryFont(const FontCascadeDescription& description)
{
    ASSERT(isMainThread());
    if (!m_cachedPrimaryFont) {
        auto& primaryRanges = realizeFallbackRangesAt(description, 0);
        m_cachedPrimaryFont = primaryRanges.glyphDataForCharacter(' ', ExternalResourceDownloadPolicy::Allow).font;
        if (!m_cachedPrimaryFont)
            m_cachedPrimaryFont = &primaryRanges.fontForFirstRange();
        else if (m_cachedPrimaryFont->isInterstitial()) {
            for (unsigned index = 1; ; ++index) {
                auto& localRanges = realizeFallbackRangesAt(description, index);
                if (localRanges.isNull())
                    break;
                auto* font = localRanges.glyphDataForCharacter(' ', ExternalResourceDownloadPolicy::Forbid).font;
                if (font && !font->isInterstitial()) {
                    m_cachedPrimaryFont = font;
                    break;
                }
            }
        }
    }
    return *m_cachedPrimaryFont;
}

}

#endif
