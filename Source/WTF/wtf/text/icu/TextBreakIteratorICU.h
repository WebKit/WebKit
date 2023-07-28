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
#include <wtf/text/icu/UTextProviderUTF16.h>
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

    TextBreakIteratorICU(StringView string, const UChar* priorContext, unsigned priorContextLength, Mode mode, const AtomString& locale)
    {
        auto type = switchOn(mode, [](LineMode) {
            return UBRK_LINE;
        }, [](CharacterMode) {
            return UBRK_CHARACTER;
        });

        auto localeWithOptionalBreakKeyword = switchOn(mode, [&locale](LineMode lineMode) {
            return makeLocaleWithBreakKeyword(locale, lineMode.behavior);
        }, [&locale](CharacterMode) {
            return locale;
        });

        UErrorCode status = U_ZERO_ERROR;
        m_iterator = ubrk_open(type, localeWithOptionalBreakKeyword.string().utf8().data(), nullptr, 0, &status);
        if (!m_iterator || U_FAILURE(status)) {
            status = U_ZERO_ERROR;
            m_iterator = ubrk_open(type, "", nullptr, 0, &status); // There's no reason for this to ever fail, unless there's an allocation failure, in which case we _should_ crash; that's the behavior of our allocators.
        }
        RELEASE_ASSERT(m_iterator);
        RELEASE_ASSERT(U_SUCCESS(status));

        setText(string, priorContext, priorContextLength);
    }

    TextBreakIteratorICU() = delete;
    TextBreakIteratorICU(const TextBreakIteratorICU&) = delete;

    TextBreakIteratorICU(TextBreakIteratorICU&& other)
        : m_iterator(std::exchange(other.m_iterator, nullptr))
        , m_priorContextLength(other.m_priorContextLength)
    {
    }

    TextBreakIteratorICU& operator=(const TextBreakIteratorICU&) = delete;

    TextBreakIteratorICU& operator=(TextBreakIteratorICU&& other)
    {
        if (m_iterator)
            ubrk_close(m_iterator);
        m_iterator = std::exchange(other.m_iterator, nullptr);
        return *this;
    }

    ~TextBreakIteratorICU()
    {
        if (m_iterator)
            ubrk_close(m_iterator); // FIXME: Use an RAII wrapper for this
    }

    void setText(StringView string, const UChar* priorContext, unsigned priorContextLength)
    {
        ASSERT(m_iterator);

        UTextWithBuffer textLocal;
        textLocal.text = UTEXT_INITIALIZER;
        textLocal.text.extraSize = sizeof(textLocal.buffer);
        textLocal.text.pExtra = textLocal.buffer;

        UErrorCode status = U_ZERO_ERROR;
        UText* text = nullptr;
        if (string.is8Bit())
            text = openLatin1ContextAwareUTextProvider(&textLocal, string.characters8(), string.length(), priorContext, priorContextLength, &status);
        else
            text = openUTF16ContextAwareUTextProvider(&textLocal.text, string.characters16(), string.length(), priorContext, priorContextLength, &status);
        ASSERT(U_SUCCESS(status));
        ASSERT(text);

        if (text && U_SUCCESS(status)) {
            ubrk_setUText(m_iterator, text, &status);
            ASSERT(U_SUCCESS(status));
            utext_close(text);
            m_priorContextLength = priorContextLength;
        } else
            m_priorContextLength = 0;
    }

    std::optional<unsigned> preceding(unsigned location) const
    {
        if (!location)
            return { };
        auto result = ubrk_preceding(m_iterator, location + m_priorContextLength);
        if (result == UBRK_DONE)
            return { };
        return std::max(static_cast<unsigned>(result), m_priorContextLength) - m_priorContextLength;
    }

    std::optional<unsigned> following(unsigned location) const
    {
        auto result = ubrk_following(m_iterator, location + m_priorContextLength);
        if (result == UBRK_DONE)
            return { };
        return result - m_priorContextLength;
    }

    bool isBoundary(unsigned location) const
    {
        return ubrk_isBoundary(m_iterator, location + m_priorContextLength);
    }

private:
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

    UBreakIterator* m_iterator { nullptr };
    unsigned m_priorContextLength { 0 };
};

}
