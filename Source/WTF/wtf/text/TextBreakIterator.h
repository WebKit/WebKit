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
        bool operator==(const LineMode&) const = default;
    };
    struct CaretMode {
        bool operator==(const CaretMode&) const = default;
    };
    struct DeleteMode {
        bool operator==(const DeleteMode&) const = default;
    };
    struct CharacterMode {
        bool operator==(const CharacterMode&) const = default;
    };
    using Mode = std::variant<LineMode, CaretMode, DeleteMode, CharacterMode>;

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
    friend class TextBreakIteratorCache;

    using Backing = std::variant<TextBreakIteratorICU, TextBreakIteratorPlatform>;

    static Backing mapModeToBackingIterator(StringView, const UChar* priorContext, unsigned priorContextLength, TextBreakIterator::Mode, const AtomString& locale);

    // Use CachedTextBreakIterator instead of constructing one of these directly.
    WTF_EXPORT_PRIVATE TextBreakIterator(StringView, const UChar* priorContext, unsigned priorContextLength, Mode, const AtomString& locale);

    void setText(StringView string, const UChar* priorContext, unsigned priorContextLength)
    {
        return switchOn(m_backing, [&](auto& iterator) {
            return iterator.setText(string, priorContext, priorContextLength);
        });
    }

    Mode mode() const
    {
        return m_mode;
    }

    const AtomString& locale() const
    {
        return m_locale;
    }

    Backing m_backing;
    Mode m_mode;
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

    TextBreakIterator take(StringView string, const UChar* priorContext, unsigned priorContextLength, TextBreakIterator::Mode mode, const AtomString& locale)
    {
        auto iter = std::find_if(m_unused.begin(), m_unused.end(), [&](TextBreakIterator& candidate) {
            return candidate.mode() == mode && candidate.locale() == locale;
        });
        if (iter == m_unused.end())
            return TextBreakIterator(string, priorContext, priorContextLength, mode, locale);
        auto result = WTFMove(*iter);
        m_unused.remove(iter - m_unused.begin());
        result.setText(string, priorContext, priorContextLength);
        return result;
    }

    void put(TextBreakIterator&& iterator)
    {
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
    CachedTextBreakIterator(StringView string, const UChar* priorContext, unsigned priorContextLength, TextBreakIterator::Mode mode, const AtomString& locale)
        : m_backing(TextBreakIteratorCache::singleton().take(string, priorContext, priorContextLength, mode, locale))
    {
    }

    ~CachedTextBreakIterator()
    {
        TextBreakIteratorCache::singleton().put(WTFMove(m_backing));
    }

    CachedTextBreakIterator() = delete;
    CachedTextBreakIterator(const CachedTextBreakIterator&) = delete;
    CachedTextBreakIterator(CachedTextBreakIterator&&) = default;
    CachedTextBreakIterator& operator=(const CachedTextBreakIterator&) = delete;
    CachedTextBreakIterator& operator=(CachedTextBreakIterator&&) = default;

    std::optional<unsigned> preceding(unsigned location) const
    {
        return m_backing.preceding(location);
    }

    std::optional<unsigned> following(unsigned location) const
    {
        return m_backing.following(location);
    }

    bool isBoundary(unsigned location) const
    {
        return m_backing.isBoundary(location);
    }

private:
    TextBreakIterator m_backing;
};

// Note: The returned iterator is good only until you get another iterator, with the exception of acquireLineBreakIterator.

using LineBreakIteratorMode = TextBreakIteratorICU::LineMode::Behavior;

WTF_EXPORT_PRIVATE UBreakIterator* wordBreakIterator(StringView);
WTF_EXPORT_PRIVATE UBreakIterator* sentenceBreakIterator(StringView);

WTF_EXPORT_PRIVATE UBreakIterator* acquireLineBreakIterator(StringView, const AtomString& locale, const UChar* priorContext, unsigned priorContextLength, LineBreakIteratorMode);
WTF_EXPORT_PRIVATE void releaseLineBreakIterator(UBreakIterator*);
UBreakIterator* openLineBreakIterator(const AtomString& locale);
void closeLineBreakIterator(UBreakIterator*&);

WTF_EXPORT_PRIVATE bool isWordTextBreak(UBreakIterator*);

// FIXME: This should be named "CachedTextBreakIteratorFactory" or "CachedTextBreakIteratorContext".
// The purpose of this class is to hold the parameters of the CachedTextBreakIterator() constructor,
// so we can create one lazily when it's needed.
class LazyLineBreakIterator {
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

        const UChar* characters() const
        {
            return m_priorContext.data() + (m_priorContext.size() - length());
        }

        bool operator==(const PriorContext& other) const = default;

    private:
        std::array<UChar, Length> m_priorContext;
    };

    LazyLineBreakIterator() = default;

    explicit LazyLineBreakIterator(StringView stringView, const AtomString& locale = AtomString(), LineBreakIteratorMode mode = LineBreakIteratorMode::Default)
        : m_stringView(stringView)
        , m_locale(locale)
        , m_mode(mode)
    {
    }

    ~LazyLineBreakIterator()
    {
        if (m_iterator)
            releaseLineBreakIterator(m_iterator);
    }

    StringView stringView() const { return m_stringView; }
    LineBreakIteratorMode mode() const { return m_mode; }

    UBreakIterator* get()
    {
        const UChar* priorContext = m_priorContext.characters();
        if (!m_iterator) {
            m_iterator = acquireLineBreakIterator(m_stringView, m_locale, priorContext, m_priorContext.length(), m_mode);
            m_cachedPriorContext = priorContext;
        } else if (priorContext != m_cachedPriorContext) {
            resetStringAndReleaseIterator(m_stringView, m_locale, m_mode);
            return get();
        }
        return m_iterator;
    }

    void resetStringAndReleaseIterator(StringView stringView, const AtomString& locale, LineBreakIteratorMode mode)
    {
        if (m_iterator)
            releaseLineBreakIterator(m_iterator);
        m_stringView = stringView;
        m_locale = locale;
        m_iterator = nullptr;
        m_cachedPriorContext = nullptr;
        m_mode = mode;
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
    UBreakIterator* m_iterator { nullptr };
    const UChar* m_cachedPriorContext { nullptr };
    LineBreakIteratorMode m_mode { LineBreakIteratorMode::Default };
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
using WTF::LazyLineBreakIterator;
using WTF::LineBreakIteratorMode;
using WTF::NonSharedCharacterBreakIterator;
using WTF::TextBreakIterator;
using WTF::TextBreakIteratorCache;
using WTF::isWordTextBreak;
