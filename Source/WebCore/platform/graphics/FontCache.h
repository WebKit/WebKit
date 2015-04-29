/*
 * Copyright (C) 2006, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#ifndef FontCache_h
#define FontCache_h

#include "FontDescription.h"
#include "Timer.h"
#include <limits.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include <CoreText/CTFont.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

namespace WebCore {

class FontCascade;
class FontPlatformData;
class FontSelector;
class OpenTypeVerticalData;
class Font;

#if PLATFORM(WIN)
#if USE(IMLANG_FONT_LINK2)
typedef IMLangFontLink2 IMLangFontLinkType;
#else
typedef IMLangFontLink IMLangFontLinkType;
#endif
#endif

// This key contains the FontDescription fields other than family that matter when fetching FontDatas (platform fonts).
struct FontDescriptionFontDataCacheKey {
    explicit FontDescriptionFontDataCacheKey(unsigned size = 0)
        : size(size)
        , weight(0)
        , flags(0)
    { }
    FontDescriptionFontDataCacheKey(const FontDescription& description)
        : size(description.computedPixelSize())
        , weight(description.weight())
        , flags(makeFlagKey(description))
    { }
    static unsigned makeFlagKey(const FontDescription& description)
    {
        return static_cast<unsigned>(description.fontSynthesis()) << 6
            | static_cast<unsigned>(description.widthVariant()) << 4
            | static_cast<unsigned>(description.nonCJKGlyphOrientation()) << 3
            | static_cast<unsigned>(description.orientation()) << 2
            | static_cast<unsigned>(description.italic()) << 1
            | static_cast<unsigned>(description.renderingMode());
    }
    bool operator==(const FontDescriptionFontDataCacheKey& other) const
    {
        return size == other.size && weight == other.weight && flags == other.flags;
    }
    bool operator!=(const FontDescriptionFontDataCacheKey& other) const
    {
        return !(*this == other);
    }
    inline unsigned computeHash() const
    {
        return StringHasher::hashMemory<sizeof(FontDescriptionFontDataCacheKey)>(this);
    }
    unsigned size;
    unsigned weight;
    unsigned flags;
};

class FontCache {
    friend class WTF::NeverDestroyed<FontCache>;

    WTF_MAKE_NONCOPYABLE(FontCache); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static FontCache& singleton();

    // This method is implemented by the platform.
    RefPtr<Font> systemFallbackForCharacters(const FontDescription&, const Font* originalFontData, bool isPlatformFont, const UChar* characters, unsigned length);

    // Also implemented by the platform.
    void platformInit();

#if PLATFORM(IOS)
    static float weightOfCTFont(CTFontRef);
#endif
#if PLATFORM(MAC)
    WEBCORE_EXPORT static void setFontWhitelist(const Vector<String>&);
#endif
#if PLATFORM(WIN)
    IMLangFontLinkType* getFontLinkInterface();
    static void comInitialize();
    static void comUninitialize();
    static IMultiLanguage* getMultiLanguageInterface();
#endif

    void getTraitsInFamily(const AtomicString&, Vector<unsigned>&);

    WEBCORE_EXPORT RefPtr<Font> fontForFamily(const FontDescription&, const AtomicString&, bool checkingAlternateName = false);
    WEBCORE_EXPORT Ref<Font> lastResortFallbackFont(const FontDescription&);
    WEBCORE_EXPORT Ref<Font> fontForPlatformData(const FontPlatformData&);
    RefPtr<Font> similarFont(const FontDescription&);

    void addClient(FontSelector*);
    void removeClient(FontSelector*);

    unsigned short generation();
    WEBCORE_EXPORT void invalidate();

    WEBCORE_EXPORT size_t fontCount();
    WEBCORE_EXPORT size_t inactiveFontCount();
    WEBCORE_EXPORT void purgeInactiveFontData(unsigned count = UINT_MAX);

#if PLATFORM(WIN)
    RefPtr<Font> fontFromDescriptionAndLogFont(const FontDescription&, const LOGFONT&, AtomicString& outFontFamilyName);
#endif

#if ENABLE(OPENTYPE_VERTICAL)
    typedef AtomicString FontFileKey;
    PassRefPtr<OpenTypeVerticalData> getVerticalData(const FontFileKey&, const FontPlatformData&);
#endif

private:
    FontCache();
    ~FontCache() = delete;

    void purgeTimerFired();

    WEBCORE_EXPORT void purgeInactiveFontDataIfNeeded();

    // FIXME: This method should eventually be removed.
    FontPlatformData* getCachedFontPlatformData(const FontDescription&, const AtomicString& family, bool checkingAlternateName = false);

    // These methods are implemented by each platform.
#if PLATFORM(IOS)
    FontPlatformData* getCustomFallbackFont(const UInt32, const FontDescription&);
    PassRefPtr<Font> getSystemFontFallbackForCharacters(const FontDescription&, const Font*, const UChar* characters, unsigned length);
#endif
    std::unique_ptr<FontPlatformData> createFontPlatformData(const FontDescription&, const AtomicString& family);

    Timer m_purgeTimer;

#if PLATFORM(COCOA)
    friend class ComplexTextController;
#endif
    friend class Font;
};

}

#endif
