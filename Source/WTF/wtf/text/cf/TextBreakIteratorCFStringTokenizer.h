/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>
#include <wtf/spi/cf/CFStringSPI.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringView.h>

namespace WTF {

class TextBreakIteratorCFStringTokenizer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Mode {
        Word,
        Sentence,
        Paragraph,
        LineBreak,
        WordBoundary,
    };

    TextBreakIteratorCFStringTokenizer(StringView string, Mode mode, const AtomString& locale)
    {
        auto options = [mode] {
            switch (mode) {
            case Mode::Word:
                return kCFStringTokenizerUnitWord;
            case Mode::Sentence:
                return kCFStringTokenizerUnitSentence;
            case Mode::Paragraph:
                return kCFStringTokenizerUnitParagraph;
            case Mode::LineBreak:
                return kCFStringTokenizerUnitLineBreak;
            case Mode::WordBoundary:
                return kCFStringTokenizerUnitWordBoundary;
            }
        }();

        auto stringObject = string.createCFStringWithoutCopying();
        m_stringLength = static_cast<unsigned long>(CFStringGetLength(stringObject.get()));
        auto localeObject = adoptCF(CFLocaleCreate(kCFAllocatorDefault, locale.string().createCFString().get()));
        m_stringTokenizer = adoptCF(CFStringTokenizerCreate(kCFAllocatorDefault, stringObject.get(), CFRangeMake(0, m_stringLength), options, localeObject.get()));
    }

    TextBreakIteratorCFStringTokenizer() = delete;
    TextBreakIteratorCFStringTokenizer(const TextBreakIteratorCFStringTokenizer&) = delete;
    TextBreakIteratorCFStringTokenizer(TextBreakIteratorCFStringTokenizer&&) = default;
    TextBreakIteratorCFStringTokenizer& operator=(const TextBreakIteratorCFStringTokenizer&) = delete;
    TextBreakIteratorCFStringTokenizer& operator=(TextBreakIteratorCFStringTokenizer&&) = default;

    void setText(StringView string)
    {
        auto stringObject = string.createCFStringWithoutCopying();
        m_stringLength = static_cast<unsigned long>(CFStringGetLength(stringObject.get()));
        CFStringTokenizerSetString(m_stringTokenizer.get(), stringObject.get(), CFRangeMake(0, m_stringLength));
    }

    std::optional<unsigned> preceding(unsigned location) const
    {
        if (!location)
            return { };
        if (location > m_stringLength)
            return m_stringLength;
        CFStringTokenizerGoToTokenAtIndex(m_stringTokenizer.get(), location - 1);
        auto range = CFStringTokenizerGetCurrentTokenRange(m_stringTokenizer.get());
        return range.location;
    }

    std::optional<unsigned> following(unsigned location) const
    {
        if (location >= m_stringLength)
            return { };
        CFStringTokenizerGoToTokenAtIndex(m_stringTokenizer.get(), location);
        auto range = CFStringTokenizerGetCurrentTokenRange(m_stringTokenizer.get());
        return range.location + range.length;
    }

    bool isBoundary(unsigned location) const
    {
        if (location == m_stringLength)
            return true;
        CFStringTokenizerGoToTokenAtIndex(m_stringTokenizer.get(), location);
        auto range = CFStringTokenizerGetCurrentTokenRange(m_stringTokenizer.get());
        return static_cast<unsigned long>(range.location) == location;
    }

private:
    RetainPtr<CFStringTokenizerRef> m_stringTokenizer;
    unsigned long m_stringLength { 0 };
};

}
