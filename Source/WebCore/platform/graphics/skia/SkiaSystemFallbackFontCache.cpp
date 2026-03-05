/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SkiaSystemFallbackFontCache.h"

#if USE(SKIA) && !OS(ANDROID) && !PLATFORM(WIN)
#include "FontCache.h"
#include <fontconfig/fontconfig.h>
#include <wtf/FileSystem.h>
#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/text/CharacterProperties.h>

namespace WebCore {

static String filePathFromPattern(FcPattern* pattern)
{
    FcChar8* filename = nullptr;
    if (FcPatternGetString(pattern, FC_FILE, 0, &filename) != FcResultMatch || !filename)
        return { };

    const char* sysroot = reinterpret_cast<const char*>(FcConfigGetSysRoot(nullptr));
    if (!sysroot)
        return String::fromUTF8(reinterpret_cast<const char*>(filename));

    return FileSystem::pathByAppendingComponent(String::fromUTF8(sysroot), String::fromUTF8(reinterpret_cast<const char*>(filename)));
}

class FontSetCache {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(FontSetCache);
    WTF_MAKE_NONCOPYABLE(FontSetCache);
    class FontSet;
public:
    FontSetCache(UniqueRef<FontSet>&& fontSet)
        : m_fontSet(WTF::move(fontSet))
    {
    }
    ~FontSetCache() = default;

    static std::unique_ptr<FontSetCache> create(const String& locale)
    {
        auto fontSet = FontSet::create(locale);
        if (!fontSet)
            return nullptr;
        return makeUnique<FontSetCache>(makeUniqueRefFromNonNullUniquePtr(WTF::move(fontSet)));
    }

    struct Font {
        Font(String&& path, int ttcIndex, FcCharSet* charSet)
            : path(WTF::move(path))
            , ttcIndex(ttcIndex)
            , charSet(charSet)
        {
        }

        String path;
        int ttcIndex { 0 };
        FcCharSet* charSet { nullptr }; // Owned by FcFontSet.
    };

    const FontSetCache::Font* fontForCharacterCluster(FcCharSet* charSet)
    {
        return m_fontSet->bestForCharacterCluster(charSet);
    }

private:
    class FontSet {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(FontSet);
    public:
        static std::unique_ptr<FontSet> create(const String& locale)
        {
            auto* pattern = FcPatternCreate();
            if (!locale.isNull()) {
                FcLangSet* langSet = FcLangSetCreate();
                FcLangSetAdd(langSet, reinterpret_cast<const FcChar8*>(locale.utf8().data()));
                FcPatternAddLangSet(pattern, FC_LANG, langSet);
                FcLangSetDestroy(langSet);
            }

            FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

            FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
            FcDefaultSubstitute(pattern);

            FcResult result;
            FcFontSet* fontSet = FcFontSort(nullptr, pattern, FcFalse, nullptr, &result);
            FcPatternDestroy(pattern);

            if (!fontSet)
                return nullptr;

            return makeUnique<FontSet>(fontSet);
        }

        FontSet(FcFontSet* fontSet)
            : m_fontSet(fontSet)
        {
            for (int i = 0; i < m_fontSet->nfont; ++i) {
                IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage")
                FcPattern* pattern = m_fontSet->fonts[i];
                IGNORE_CLANG_WARNINGS_END
                if (!pattern)
                    continue;

                FcCharSet* charSet = nullptr;
                if (FcPatternGetCharSet(pattern, FC_CHARSET, 0, &charSet) != FcResultMatch)
                    continue;

                auto filepath = filePathFromPattern(pattern);
                if (filepath.isEmpty() || !FileSystem::fileExists(filepath))
                    continue;

                int ttcIndex;
                if (FcPatternGetInteger(pattern, FC_INDEX, 0, &ttcIndex) != FcResultMatch)
                    ttcIndex = 0;

                m_fallbackList.append(Font(WTF::move(filepath), ttcIndex, charSet));
            }
        }

        ~FontSet()
        {
            FcFontSetDestroy(m_fontSet);
        }

        const FontSetCache::Font* bestForCharacterCluster(FcCharSet* charSet)
        {
            if (m_fallbackList.isEmpty())
                return nullptr;

            const FontSetCache::Font* bestFont = nullptr;
            int minScore = std::numeric_limits<int>::max();
            for (const auto& font : m_fallbackList) {
                int score = FcCharSetSubtractCount(charSet, font.charSet);
                if (!score) {
                    bestFont = &font;
                    break;
                }

                if (score < minScore) {
                    bestFont = &font;
                    minScore = score;
                }
            }

            return bestFont;
        }

    private:
        FcFontSet* m_fontSet { nullptr };
        Vector<FontSetCache::Font> m_fallbackList;

    };
    UniqueRef<FontSet> m_fontSet;
};

SkiaSystemFallbackFontCache::SkiaSystemFallbackFontCache() = default;

SkiaSystemFallbackFontCache::~SkiaSystemFallbackFontCache() = default;

sk_sp<SkTypeface> SkiaSystemFallbackFontCache::fontForCharacterCluster(const String& locale, StringView stringView)
{
    FcCharSet* charSet = FcCharSetCreate();
    bool hasNonIgnorableCharacters = false;
    for (char32_t character : stringView.codePoints()) {
        if (!isDefaultIgnorableCodePoint(character)) {
            FcCharSetAddChar(charSet, character);
            hasNonIgnorableCharacters = true;
        }
    }

    if (!hasNonIgnorableCharacters) {
        FcCharSetDestroy(charSet);
        return nullptr;
    }

    auto& fontSetCache = m_cache.ensure(locale.isNull() ? emptyString() : locale, [&] {
        return FontSetCache::create(locale);
    }).iterator->value;

    if (!fontSetCache)
        return nullptr;

    const auto* font = fontSetCache->fontForCharacterCluster(charSet);
    FcCharSetDestroy(charSet);
    if (!font)
        return nullptr;

    return m_typefaceCache.ensure({ font->path, font->ttcIndex }, [font] -> sk_sp<SkTypeface> {
        return FontCache::forCurrentThread()->fontManager().makeFromFile(font->path.utf8().data(), font->ttcIndex);
    }).iterator->value;
}

void SkiaSystemFallbackFontCache::clear()
{
    m_cache.clear();
    m_typefaceCache.clear();
}

} // namepsace WebCore

#endif // USE(SKIA) && !OS(ANDROID) && !PLATFORM(WIN)
