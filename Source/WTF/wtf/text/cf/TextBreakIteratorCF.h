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

#include <wtf/text/cf/TextBreakIteratorCFCharacterCluster.h>
#include <wtf/text/cf/TextBreakIteratorCFStringTokenizer.h>

namespace WTF {

class TextBreakIteratorCF {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Mode {
        ComposedCharacter,
        BackwardDeletion,
        Word,
        Sentence,
        Paragraph,
        LineBreak,
        WordBoundary,
    };

    TextBreakIteratorCF(StringView string, StringView priorContext, Mode mode, const AtomString& locale)
        : m_backing(mapModeToBackingIterator(string, priorContext, mode, locale))
    {
    }

    TextBreakIteratorCF() = delete;
    TextBreakIteratorCF(const TextBreakIteratorCF&) = delete;
    TextBreakIteratorCF(TextBreakIteratorCF&&) = default;
    TextBreakIteratorCF& operator=(const TextBreakIteratorCF&) = delete;
    TextBreakIteratorCF& operator=(TextBreakIteratorCF&&) = default;

    void setText(StringView string, std::span<const char16_t> priorContext)
    {
        return switchOn(m_backing, [&](auto& iterator) {
            return iterator.setText(string, priorContext);
        });
    }

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
    using BackingVariant = Variant<TextBreakIteratorCFCharacterCluster, TextBreakIteratorCFStringTokenizer>;

    static BackingVariant mapModeToBackingIterator(StringView string, StringView priorContext, Mode mode, const AtomString& locale)
    {
        switch (mode) {
        case Mode::ComposedCharacter:
            return TextBreakIteratorCFCharacterCluster(string, priorContext, TextBreakIteratorCFCharacterCluster::Mode::ComposedCharacter);
        case Mode::BackwardDeletion:
            return TextBreakIteratorCFCharacterCluster(string, priorContext, TextBreakIteratorCFCharacterCluster::Mode::BackwardDeletion);
        case Mode::Word:
            return TextBreakIteratorCFStringTokenizer(string, priorContext, TextBreakIteratorCFStringTokenizer::Mode::Word, locale);
        case Mode::Sentence:
            return TextBreakIteratorCFStringTokenizer(string, priorContext, TextBreakIteratorCFStringTokenizer::Mode::Sentence, locale);
        case Mode::Paragraph:
            return TextBreakIteratorCFStringTokenizer(string, priorContext, TextBreakIteratorCFStringTokenizer::Mode::Paragraph, locale);
        case Mode::LineBreak:
            return TextBreakIteratorCFStringTokenizer(string, priorContext, TextBreakIteratorCFStringTokenizer::Mode::LineBreak, locale);
        case Mode::WordBoundary:
            return TextBreakIteratorCFStringTokenizer(string, priorContext, TextBreakIteratorCFStringTokenizer::Mode::WordBoundary, locale);
        }
    }

    BackingVariant m_backing;
};

} // namespace WTF
