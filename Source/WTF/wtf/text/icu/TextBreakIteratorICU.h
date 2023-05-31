/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include <unicode/ubrk.h>
#include <wtf/text/StringView.h>
#include <wtf/text/icu/UTextProviderLatin1.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace WTF {

class TextBreakIteratorICU {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct LineMode {
        enum class Behavior: uint8_t {
            Default,
            Loose,
            Normal,
            Strict,
        };
        Behavior behavior;
    };
    struct CharacterMode {
    };
    using Mode = std::variant<LineMode, CharacterMode>;

    void set8BitText(const LChar* buffer, unsigned length)
    {
        UTextWithBuffer textLocal;
        textLocal.text = UTEXT_INITIALIZER;
        textLocal.text.extraSize = sizeof(textLocal.buffer);
        textLocal.text.pExtra = textLocal.buffer;

        UErrorCode status = U_ZERO_ERROR;
        UText* text = openLatin1UTextProvider(&textLocal, buffer, length, &status);
        ASSERT(U_SUCCESS(status));
        ASSERT(text);

        ubrk_setUText(m_iterator, text, &status);
        ASSERT(U_SUCCESS(status));

        utext_close(text);
    }

    TextBreakIteratorICU(StringView string, Mode mode, const AtomString& locale)
    {
        auto type = switchOn(mode, [](LineMode) {
            return UBRK_LINE;
        }, [](CharacterMode) {
            return UBRK_CHARACTER;
        });

        bool requiresSet8BitText = string.is8Bit();

        const UChar *text = requiresSet8BitText ? nullptr : string.characters16();
        int32_t textLength = requiresSet8BitText ? 0 : string.length();

        auto localeWithOptionalBreakKeyword = switchOn(mode, [&locale](LineMode lineMode) {
            return makeLocaleWithBreakKeyword(locale, lineMode.behavior);
        }, [&locale](CharacterMode) {
            return locale;
        });

        UErrorCode status = U_ZERO_ERROR;
        m_iterator = ubrk_open(type, localeWithOptionalBreakKeyword.string().utf8().data(), text, textLength, &status);
        ASSERT(U_SUCCESS(status));

        if (requiresSet8BitText)
            set8BitText(string.characters8(), string.length());
    }

    TextBreakIteratorICU() = delete;
    TextBreakIteratorICU(const TextBreakIteratorICU&) = delete;

    TextBreakIteratorICU(TextBreakIteratorICU&& other)
        : m_iterator(other.m_iterator)
    {
        other.m_iterator = nullptr;
    }

    TextBreakIteratorICU& operator=(const TextBreakIteratorICU&) = delete;

    TextBreakIteratorICU& operator=(TextBreakIteratorICU&& other)
    {
        if (m_iterator)
            ubrk_close(m_iterator);
        m_iterator = other.m_iterator;
        other.m_iterator = nullptr;
        return *this;
    }

    ~TextBreakIteratorICU()
    {
        if (m_iterator)
            ubrk_close(m_iterator);
    }

    void setText(StringView string, StringView priorContext)
    {
        UNUSED_PARAM(priorContext); // FIXME: Implement prior context.

        if (string.is8Bit()) {
            set8BitText(string.characters8(), string.length());
            return;
        }
        UErrorCode status = U_ZERO_ERROR;
        ubrk_setText(m_iterator, string.characters16(), string.length(), &status);
        ASSERT(U_SUCCESS(status));
    }

    std::optional<unsigned> preceding(unsigned location) const
    {
        auto result = ubrk_preceding(m_iterator, location);
        if (result == UBRK_DONE)
            return { };
        return result;
    }

    std::optional<unsigned> following(unsigned location) const
    {
        auto result = ubrk_following(m_iterator, location);
        if (result == UBRK_DONE)
            return { };
        return result;
    }

    bool isBoundary(unsigned location) const
    {
        return ubrk_isBoundary(m_iterator, location);
    }

private:
    friend class LineBreakIteratorPool;

    // This is temporarily duplicated from LineBreakIteratorPool.
    // Eventually, LineBreakIteratorPool will be replaced with TextBreakIteratorCache,
    // and this won't be duplicated any more.
    static AtomString makeLocaleWithBreakKeyword(const AtomString& locale, LineMode::Behavior behavior)
    {
        if (behavior == LineMode::Behavior::Default)
            return locale;

        // The uloc functions model locales as char*, so we have to downconvert our AtomString.
        auto utf8Locale = locale.string().utf8();
        if (!utf8Locale.length())
            return locale;
        Vector<char> scratchBuffer(utf8Locale.length() + 11, 0);
        memcpy(scratchBuffer.data(), utf8Locale.data(), utf8Locale.length());

        const char* keywordValue = nullptr;
        switch (behavior) {
        case LineMode::Behavior::Default:
            // nullptr will cause any existing values to be removed.
            ASSERT_NOT_REACHED();
            break;
        case LineMode::Behavior::Loose:
            keywordValue = "loose";
            break;
        case LineMode::Behavior::Normal:
            keywordValue = "normal";
            break;
        case LineMode::Behavior::Strict:
            keywordValue = "strict";
            break;
        }

        UErrorCode status = U_ZERO_ERROR;
        int32_t lengthNeeded = uloc_setKeywordValue("lb", keywordValue, scratchBuffer.data(), scratchBuffer.size(), &status);
        if (U_SUCCESS(status))
            return AtomString::fromUTF8(scratchBuffer.data(), lengthNeeded);
        if (needsToGrowToProduceBuffer(status)) {
            scratchBuffer.grow(lengthNeeded + 1);
            memset(scratchBuffer.data() + utf8Locale.length(), 0, scratchBuffer.size() - utf8Locale.length());
            status = U_ZERO_ERROR;
            int32_t lengthNeeded2 = uloc_setKeywordValue("lb", keywordValue, scratchBuffer.data(), scratchBuffer.size(), &status);
            if (!U_SUCCESS(status) || lengthNeeded != lengthNeeded2)
                return locale;
            return AtomString::fromUTF8(scratchBuffer.data(), lengthNeeded);
        }
        return locale;
    }

    UBreakIterator* m_iterator;
};

}
