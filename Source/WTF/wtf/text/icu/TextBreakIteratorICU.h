/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

namespace WTF {

class TextBreakIteratorICU {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Mode {
        Line,
        Character,
    };

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

    TextBreakIteratorICU(StringView string, Mode mode, const char *locale)
    {
        UBreakIteratorType type;
        switch (mode) {
        case Mode::Line:
            type = UBRK_LINE;
            break;
        case Mode::Character:
            type = UBRK_CHARACTER;
            break;
        default:
            ASSERT_NOT_REACHED();
            type = UBRK_CHARACTER;
            break;
        }

        bool requiresSet8BitText = string.is8Bit();

        const UChar *text = requiresSet8BitText ? nullptr : string.characters16();
        int32_t textLength = requiresSet8BitText ? 0 : string.length();

        // FIXME: Handle weak / normal / strict line breaking.
        UErrorCode status = U_ZERO_ERROR;
        m_iterator = ubrk_open(type, locale, text, textLength, &status);
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

    void setText(StringView string)
    {
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
    UBreakIterator* m_iterator;
};

}
