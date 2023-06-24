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

#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>
#include <wtf/spi/cf/CFStringSPI.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringView.h>
#include <wtf/text/cocoa/ContextualizedCFString.h>

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

    TextBreakIteratorCFStringTokenizer(StringView string, StringView priorContext, Mode mode, const AtomString& locale)
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

        auto stringObject = createString(string, priorContext);
        m_stringLength = string.length();
        m_priorContextLength = priorContext.length();
        auto localeObject = adoptCF(CFLocaleCreate(kCFAllocatorDefault, locale.string().createCFString().get()));
        m_stringTokenizer = adoptCF(CFStringTokenizerCreate(kCFAllocatorDefault, stringObject.get(), CFRangeMake(0, m_stringLength + m_priorContextLength), options, localeObject.get()));
        if (!m_stringTokenizer)
            m_stringTokenizer = adoptCF(CFStringTokenizerCreate(kCFAllocatorDefault, stringObject.get(), CFRangeMake(0, m_stringLength + m_priorContextLength), options, nullptr));
        ASSERT(m_stringTokenizer);
    }

    TextBreakIteratorCFStringTokenizer() = delete;
    TextBreakIteratorCFStringTokenizer(const TextBreakIteratorCFStringTokenizer&) = delete;
    TextBreakIteratorCFStringTokenizer(TextBreakIteratorCFStringTokenizer&&) = default;
    TextBreakIteratorCFStringTokenizer& operator=(const TextBreakIteratorCFStringTokenizer&) = delete;
    TextBreakIteratorCFStringTokenizer& operator=(TextBreakIteratorCFStringTokenizer&&) = default;

    void setText(StringView string, StringView priorContext)
    {
        auto stringObject = createString(string, priorContext);
        m_stringLength = string.length();
        m_priorContextLength = priorContext.length();
        CFStringTokenizerSetString(m_stringTokenizer.get(), stringObject.get(), CFRangeMake(0, m_stringLength));
    }

    std::optional<unsigned> preceding(unsigned location) const
    {
        if (!location)
            return { };
        if (location > m_stringLength)
            return m_stringLength;
        CFStringTokenizerGoToTokenAtIndex(m_stringTokenizer.get(), location - 1 + m_priorContextLength);
        auto range = CFStringTokenizerGetCurrentTokenRange(m_stringTokenizer.get());
        if (range.location == kCFNotFound)
            return { };
        return std::max(static_cast<unsigned long>(range.location), m_priorContextLength) - m_priorContextLength;
    }

    std::optional<unsigned> following(unsigned location) const
    {
        if (location >= m_stringLength)
            return { };
        CFStringTokenizerGoToTokenAtIndex(m_stringTokenizer.get(), location + m_priorContextLength);
        auto range = CFStringTokenizerGetCurrentTokenRange(m_stringTokenizer.get());
        if (range.location == kCFNotFound)
            return { };
        return range.location + range.length - m_priorContextLength;
    }

    bool isBoundary(unsigned location) const
    {
        if (location == m_stringLength)
            return true;
        CFStringTokenizerGoToTokenAtIndex(m_stringTokenizer.get(), location + m_priorContextLength);
        auto range = CFStringTokenizerGetCurrentTokenRange(m_stringTokenizer.get());
        if (range.location == kCFNotFound)
            return true;
        return static_cast<unsigned long>(range.location) == location + m_priorContextLength;
    }

private:
    RetainPtr<CFStringRef> createString(StringView string, StringView priorContext)
    {
        if (priorContext.isEmpty())
            return string.createCFStringWithoutCopying();
        return createContextualizedCFString(string, priorContext);
    }

    RetainPtr<CFStringTokenizerRef> m_stringTokenizer;
    unsigned long m_stringLength { 0 };
    unsigned long m_priorContextLength { 0 };
};

}
