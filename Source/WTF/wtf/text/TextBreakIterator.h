/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <mutex>
#include <optional>
#include <variant>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringView.h>
#include <wtf/text/icu/TextBreakIteratorICU.h>

#if PLATFORM(COCOA)
#include <wtf/text/cf/TextBreakIteratorCF.h>
#else
#include <wtf/text/NullTextBreakIterator.h>
#endif

namespace WTF {

#if PLATFORM(COCOA)
typedef TextBreakIteratorCF TextBreakIteratorPlatform;
#else
typedef NullTextBreakIterator TextBreakIteratorPlatform;
#endif

class TextBreakIteratorCache;

class TextBreakIterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct LineMode {
        using Behavior = TextBreakIteratorICU::LineMode::Behavior;
        Behavior behavior;
        friend bool operator==(LineMode, LineMode) = default;
    };
    struct CaretMode {
        friend bool operator==(CaretMode, CaretMode) = default;
    };
    struct DeleteMode {
        friend bool operator==(DeleteMode, DeleteMode) = default;
    };
    struct CharacterMode {
        friend bool operator==(CharacterMode, CharacterMode) = default;
    };
    using Mode = std::variant<LineMode, CaretMode, DeleteMode, CharacterMode>;

    enum class ContentAnalysis : bool {
        Linguistic,
        Mechanical,
    };

    TextBreakIterator() = delete;
    TextBreakIterator(const TextBreakIterator&) = delete;
    TextBreakIterator(TextBreakIterator&&) = default;
    TextBreakIterator& operator=(const TextBreakIterator&) = delete;
    TextBreakIterator& operator=(TextBreakIterator&&) = default;

    std::optional<unsigned> preceding(unsigned location) const
    {
        return switchOn(m_backing, [&](const auto& iterator) {
            return iterator.preceding(location);
        });
    }

    std::optional<unsigned> following(unsigned location) const
    {
        return switchOn(m_backing, [&](const auto& iterator) {
            return iterator.following(location);
        });
    }

    bool isBoundary(unsigned location) const
    {
        return switchOn(m_backing, [&](const auto& iterator) {
            return iterator.isBoundary(location);
        });
    }

private:
    friend class CachedTextBreakIterator;
    friend class TextBreakIteratorCache;

    using Backing = std::variant<TextBreakIteratorICU, TextBreakIteratorPlatform>;

    static Backing mapModeToBackingIterator(StringView, std::span<const UChar> priorContext, Mode, ContentAnalysis, const AtomString& locale);

    // Use CachedTextBreakIterator instead of constructing one of these directly.
    WTF_EXPORT_PRIVATE TextBreakIterator(StringView, std::span<const UChar> priorContext, Mode, ContentAnalysis, const AtomString& locale);

    void setText(StringView string, std::span<const UChar> priorContext)
    {
        return switchOn(m_backing, [&](auto& iterator) {
            return iterator.setText(string, priorContext);
        });
    }

    Mode mode() const
    {
        return m_mode;
    }

    ContentAnalysis contentAnalysis() const
    {
        return m_contentAnalysis;
    }

    const AtomString& locale() const
    {
        return m_locale;
    }

    Backing m_backing;
    Mode m_mode;
    ContentAnalysis m_contentAnalysis { ContentAnalysis::Mechanical };
    AtomString m_locale;
};

class CachedTextBreakIterator;

class TextBreakIteratorCache {
    WTF_MAKE_FAST_ALLOCATED;
// Use CachedTextBreakIterator instead of dealing with the cache directly.
private:
    friend class LazyNeverDestroyed<TextBreakIteratorCache>;
    friend class CachedTextBreakIterator;

    WTF_EXPORT_PRIVATE static TextBreakIteratorCache& singleton();

    TextBreakIteratorCache(const TextBreakIteratorCache&) = delete;
    TextBreakIteratorCache(TextBreakIteratorCache&&) = delete;
    TextBreakIteratorCache& operator=(const TextBreakIteratorCache&) = delete;
    TextBreakIteratorCache& operator=(TextBreakIteratorCache&&) = delete;

    TextBreakIterator take(StringView string, std::span<const UChar> priorContext, TextBreakIterator::Mode mode, TextBreakIterator::ContentAnalysis contentAnalysis, const AtomString& locale)
    {
        ASSERT(isMainThread());
        auto iter = std::find_if(m_unused.begin(), m_unused.end(), [&](TextBreakIterator& candidate) {
            return candidate.mode() == mode && candidate.contentAnalysis() == contentAnalysis && candidate.locale() == locale;
        });
        if (iter == m_unused.end())
            return TextBreakIterator(string, priorContext, mode, contentAnalysis, locale);
        auto result = WTFMove(*iter);
        m_unused.remove(iter - m_unused.begin());
        result.setText(string, priorContext);
        return result;
    }

    void put(TextBreakIterator&& iterator)
    {
        ASSERT(isMainThread());
        m_unused.append(WTFMove(iterator));
        if (m_unused.size() > capacity)
            m_unused.remove(0);
    }

    TextBreakIteratorCache() = default;

    static constexpr int capacity = 2;
    // FIXME: Break this up into different Vectors per mode.
    Vector<TextBreakIterator, capacity> m_unused;
};

// RAII for TextBreakIterator and TextBreakIteratorCache.
class CachedTextBreakIterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CachedTextBreakIterator(StringView string, std::span<const UChar> priorContext, TextBreakIterator::Mode mode, const AtomString& locale, TextBreakIterator::ContentAnalysis contentAnalysis = TextBreakIterator::ContentAnalysis::Mechanical)
        : m_backing(isMainThread() ? TextBreakIteratorCache::singleton().take(string, priorContext, mode, contentAnalysis, locale) : TextBreakIterator(string, priorContext, mode, contentAnalysis, locale))
    {
    }

    ~CachedTextBreakIterator()
    {
        if (m_backing && isMainThread())
            TextBreakIteratorCache::singleton().put(WTFMove(*m_backing));
    }

    CachedTextBreakIterator() = delete;
    CachedTextBreakIterator(const CachedTextBreakIterator&) = delete;
    CachedTextBreakIterator(CachedTextBreakIterator&& other)
        : m_backing(std::exchange(other.m_backing, { }))
    {
        other.m_backing = std::nullopt;
    }
    CachedTextBreakIterator& operator=(const CachedTextBreakIterator&) = delete;
    CachedTextBreakIterator& operator=(CachedTextBreakIterator&& other)
    {
        m_backing = std::exchange(other.m_backing, { });
        return *this;
    }

    std::optional<unsigned> preceding(unsigned location) const
    {
        return m_backing->preceding(location);
    }

    std::optional<unsigned> following(unsigned location) const
    {
        return m_backing->following(location);
    }

    bool isBoundary(unsigned location) const
    {
        return m_backing->isBoundary(location);
    }

private:
    std::optional<TextBreakIterator> m_backing;
};

WTF_EXPORT_PRIVATE UBreakIterator* wordBreakIterator(StringView);
WTF_EXPORT_PRIVATE UBreakIterator* sentenceBreakIterator(StringView);

WTF_EXPORT_PRIVATE bool isWordTextBreak(UBreakIterator*);

class CachedLineBreakIteratorFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class PriorContext {
    public:
        static constexpr size_t Length = 2;

        PriorContext()
        {
            reset();
        }

        UChar lastCharacter() const
        {
            static_assert(Length >= 1);
            return m_priorContext[m_priorContext.size() - 1];
        }

        UChar secondToLastCharacter() const
        {
            static_assert(Length >= 2);
            return m_priorContext[m_priorContext.size() - 2];
        }

        void set(std::array<UChar, Length>&& newPriorContext)
        {
            m_priorContext = WTFMove(newPriorContext);
        }

        void update(UChar last)
        {
            for (size_t i = 0; i < m_priorContext.size() - 1; ++i)
                m_priorContext[i] = m_priorContext[i + 1];
            m_priorContext[m_priorContext.size() - 1] = last;
        }

        void reset()
        {
            std::fill(std::begin(m_priorContext), std::end(m_priorContext), 0);
        }

        unsigned length() const
        {
            unsigned result = 0;
            for (auto iterator = std::rbegin(m_priorContext); iterator != std::rend(m_priorContext) && *iterator; ++iterator)
                ++result;
            return result;
        }

        std::span<const UChar> characters() const
        {
            return std::span<const UChar>(m_priorContext).last(length());
        }

        friend bool operator==(const PriorContext&, const PriorContext&) = default;

    private:
        std::array<UChar, Length> m_priorContext;
    };

    CachedLineBreakIteratorFactory() = default;

    explicit CachedLineBreakIteratorFactory(StringView stringView, const AtomString& locale = AtomString(), TextBreakIterator::LineMode::Behavior mode = TextBreakIterator::LineMode::Behavior::Default, TextBreakIterator::ContentAnalysis contentAnalysis = TextBreakIterator::ContentAnalysis::Mechanical)
        : m_stringView(stringView)
        , m_locale(locale)
        , m_mode(mode)
        , m_contentAnalysis(contentAnalysis)
    {
    }

    StringView stringView() const { return m_stringView; }
    TextBreakIterator::LineMode::Behavior mode() const { return m_mode; }
    TextBreakIterator::ContentAnalysis contentAnalysis() const { return m_contentAnalysis; }

    CachedTextBreakIterator& get()
    {
        auto priorContext = m_priorContext.characters();
        if (!m_iterator) {
            m_iterator = CachedTextBreakIterator(m_stringView, priorContext, WTF::TextBreakIterator::LineMode { m_mode }, m_locale, m_contentAnalysis);
            m_cachedPriorContext = priorContext.data();
        } else if (priorContext.data() != m_cachedPriorContext) {
            resetStringAndReleaseIterator(m_stringView, m_locale, m_mode, m_contentAnalysis);
            return get();
        }
        return *m_iterator;
    }

    void resetStringAndReleaseIterator(StringView stringView, const AtomString& locale, TextBreakIterator::LineMode::Behavior mode, TextBreakIterator::ContentAnalysis contentAnalysis = TextBreakIterator::ContentAnalysis::Mechanical)
    {
        m_stringView = stringView;
        m_locale = locale;
        m_iterator = std::nullopt;
        m_cachedPriorContext = nullptr;
        m_mode = mode;
        m_contentAnalysis = contentAnalysis;
    }

    const PriorContext& priorContext() const
    {
        return m_priorContext;
    }

    PriorContext& priorContext()
    {
        return m_priorContext;
    }

private:
    StringView m_stringView;
    AtomString m_locale;
    std::optional<CachedTextBreakIterator> m_iterator;
    const UChar* m_cachedPriorContext { nullptr };
    TextBreakIterator::LineMode::Behavior m_mode { TextBreakIterator::LineMode::Behavior::Default };
    TextBreakIterator::ContentAnalysis m_contentAnalysis { TextBreakIterator::ContentAnalysis::Mechanical };
    PriorContext m_priorContext;
};

// Iterates over "extended grapheme clusters", as defined in UAX #29.
// Note that platform implementations may be less sophisticated - e.g. ICU prior to
// version 4.0 only supports "legacy grapheme clusters".
// Use this for general text processing, e.g. string truncation.

class NonSharedCharacterBreakIterator {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(NonSharedCharacterBreakIterator);
public:
    WTF_EXPORT_PRIVATE NonSharedCharacterBreakIterator(StringView);
    WTF_EXPORT_PRIVATE ~NonSharedCharacterBreakIterator();

    NonSharedCharacterBreakIterator(NonSharedCharacterBreakIterator&&);

    operator UBreakIterator*() const { return m_iterator; }

private:
    UBreakIterator* m_iterator;
};

// Counts the number of grapheme clusters. A surrogate pair or a sequence
// of a non-combining character and following combining characters is
// counted as 1 grapheme cluster.
WTF_EXPORT_PRIVATE unsigned numGraphemeClusters(StringView);

// Returns the number of code units that create the specified number of
// grapheme clusters. If there are fewer clusters in the string than specified,
// the length of the string is returned.
WTF_EXPORT_PRIVATE unsigned numCodeUnitsInGraphemeClusters(StringView, unsigned);

}

using WTF::CachedTextBreakIterator;
using WTF::CachedLineBreakIteratorFactory;
using WTF::NonSharedCharacterBreakIterator;
using WTF::TextBreakIterator;
using WTF::TextBreakIteratorCache;
using WTF::isWordTextBreak;
