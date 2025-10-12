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

static void fontconfigStyle(const SkFontStyle& style, int& weight, int& width, int& slant)
{
    struct MapRanges {
        SkScalar oldValue;
        SkScalar newValue;
    };

    IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage")
    auto mapRanges = [](SkScalar value, MapRanges const ranges[], int rangesCount) -> SkScalar {
        if (value < ranges[0].oldValue)
            return ranges[0].newValue;

        for (int i = 0; i < rangesCount - 1; ++i) {
            if (value < ranges[i + 1].oldValue)
                return ranges[i].newValue + ((value - ranges[i].oldValue) * (ranges[i + 1].newValue - ranges[i].newValue) / (ranges[i + 1].oldValue - ranges[i].oldValue));
        }
        return ranges[rangesCount - 1].newValue;
    };
    IGNORE_CLANG_WARNINGS_END

    static constexpr MapRanges weightRanges[] = {
        { SkFontStyle::kThin_Weight,       FC_WEIGHT_THIN },
        { SkFontStyle::kExtraLight_Weight, FC_WEIGHT_EXTRALIGHT },
        { SkFontStyle::kLight_Weight,      FC_WEIGHT_LIGHT },
        { 350,                             FC_WEIGHT_DEMILIGHT },
        { 380,                             FC_WEIGHT_BOOK },
        { SkFontStyle::kNormal_Weight,     FC_WEIGHT_REGULAR },
        { SkFontStyle::kMedium_Weight,     FC_WEIGHT_MEDIUM },
        { SkFontStyle::kSemiBold_Weight,   FC_WEIGHT_DEMIBOLD },
        { SkFontStyle::kBold_Weight,       FC_WEIGHT_BOLD },
        { SkFontStyle::kExtraBold_Weight,  FC_WEIGHT_EXTRABOLD },
        { SkFontStyle::kBlack_Weight,      FC_WEIGHT_BLACK },
        { SkFontStyle::kExtraBlack_Weight, FC_WEIGHT_EXTRABLACK },
    };
    weight = mapRanges(style.weight(), weightRanges, std::size(weightRanges));

    static constexpr MapRanges widthRanges[] = {
        { SkFontStyle::kUltraCondensed_Width, FC_WIDTH_ULTRACONDENSED },
        { SkFontStyle::kExtraCondensed_Width, FC_WIDTH_EXTRACONDENSED },
        { SkFontStyle::kCondensed_Width,      FC_WIDTH_CONDENSED },
        { SkFontStyle::kSemiCondensed_Width,  FC_WIDTH_SEMICONDENSED },
        { SkFontStyle::kNormal_Width,         FC_WIDTH_NORMAL },
        { SkFontStyle::kSemiExpanded_Width,   FC_WIDTH_SEMIEXPANDED },
        { SkFontStyle::kExpanded_Width,       FC_WIDTH_EXPANDED },
        { SkFontStyle::kExtraExpanded_Width,  FC_WIDTH_EXTRAEXPANDED },
        { SkFontStyle::kUltraExpanded_Width,  FC_WIDTH_ULTRAEXPANDED },
    };
    width = mapRanges(style.width(), widthRanges, std::size(widthRanges));

    slant = FC_SLANT_ROMAN;
    switch (style.slant()) {
    case SkFontStyle::kUpright_Slant:
        slant = FC_SLANT_ROMAN;
        break;
    case SkFontStyle::kItalic_Slant:
        slant = FC_SLANT_ITALIC;
        break;
    case SkFontStyle::kOblique_Slant:
        slant = FC_SLANT_OBLIQUE;
        break;
    }
}

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

struct FontSetCacheKey {
    FontSetCacheKey() = default;

    FontSetCacheKey(int weight, int width, int slant)
        : weight(weight)
        , width(width)
        , slant(slant)
    {
    }

    explicit FontSetCacheKey(WTF::HashTableDeletedValueType)
        : weight(-1)
        , width(-1)
        , slant(-1)
    {
    }

    bool isHashTableDeletedValue() const { return weight == -1 && width == -1 && slant == -1; }

    bool operator==(const FontSetCacheKey&) const = default;

    int weight { 0 };
    int width { 0 };
    int slant { 0 };
};

inline void add(Hasher& hasher, const FontSetCacheKey& key)
{
    add(hasher, key.weight, key.width, key.slant);
}

struct FontSetCacheKeyHash {
    static unsigned hash(const FontSetCacheKey& key) { return computeHash(key); }
    static bool equal(const FontSetCacheKey& a, const FontSetCacheKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

class FontSetCache {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(FontSetCache);
    WTF_MAKE_NONCOPYABLE(FontSetCache);
public:
    FontSetCache() = default;
    ~FontSetCache() = default;

    struct Font {
        Font(String&& path, int ttcIndex, FcCharSet* charSet)
            : path(WTFMove(path))
            , ttcIndex(ttcIndex)
            , charSet(charSet)
        {
        }

        String path;
        int ttcIndex { 0 };
        FcCharSet* charSet { nullptr }; // Owned by FcFontSet.
    };

    const FontSetCache::Font* fontForCharacterCluster(const SkFontStyle& style, const String& locale, FcCharSet* charSet)
    {
        int weight, width, slant;
        fontconfigStyle(style, weight, width, slant);

        auto& fontSet = m_cache.ensure(FontSetCacheKey(weight, width, slant), [&] {
            return FontSet::create(locale, weight, width, slant);
        }).iterator->value;
        if (!fontSet)
            return nullptr;

        return fontSet->bestForCharacterCluster(charSet);
    }

private:
    class FontSet {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(FontSet);
    public:
        static std::unique_ptr<FontSet> create(const String& locale, int weight, int width, int slant)
        {
            auto* pattern = FcPatternCreate();
            if (!locale.isNull()) {
                FcLangSet* langSet = FcLangSetCreate();
                FcLangSetAdd(langSet, reinterpret_cast<const FcChar8*>(locale.utf8().data()));
                FcPatternAddLangSet(pattern, FC_LANG, langSet);
                FcLangSetDestroy(langSet);
            }

            FcPatternAddInteger(pattern, FC_WEIGHT, weight);
            FcPatternAddInteger(pattern, FC_WIDTH, width);
            FcPatternAddInteger(pattern, FC_SLANT, slant);

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

                m_fallbackList.append(Font(WTFMove(filepath), ttcIndex, charSet));
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
    HashMap<FontSetCacheKey, std::unique_ptr<FontSet>, FontSetCacheKeyHash, SimpleClassHashTraits<FontSetCacheKey>> m_cache;
};

SkiaSystemFallbackFontCache::SkiaSystemFallbackFontCache() = default;

SkiaSystemFallbackFontCache::~SkiaSystemFallbackFontCache() = default;

sk_sp<SkTypeface> SkiaSystemFallbackFontCache::fontForCharacterCluster(const SkFontStyle& style, const String& locale, StringView stringView)
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

    auto& fontSetCache = m_cache.ensure(locale.isNull() ? emptyString() : locale, [] {
        return makeUnique<FontSetCache>();
    }).iterator->value;

    const auto* font = fontSetCache->fontForCharacterCluster(style, locale, charSet);
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
