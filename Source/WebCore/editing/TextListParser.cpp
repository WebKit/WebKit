/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextListParser.h"

#include <WebCore/CSSSerializationContext.h>
#include <WebCore/CSSValueKeywords.h>
#include <WebCore/CSSValuePool.h>
#include <WebCore/ContainerNodeInlines.h>
#include <WebCore/Document.h>
#include <WebCore/Editing.h>
#include <WebCore/Editor.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/MutableStyleProperties.h>
#include <WebCore/RenderElement.h>
#include <WebCore/StyleProperties.h>
#include <WebCore/StylePropertiesInlines.h>
#include <WebCore/StyledElement.h>
#include <WebCore/VisibleSelection.h>
#include <span>
#include <wtf/ASCIICType.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

// MARK: Helpers

template<typename Character>
constexpr std::optional<int> consumeNumber(StringParsingBuffer<Character>& input)
{
    // Parse the digits until there is no more input left or a non-ASCII digit character has been encountered.
    Checked<int, RecordOverflow> value;
    do {
        auto c = input.consume();
        int digitValue = c - '0';
        value = (value * 10) + digitValue;
    } while (!input.atEnd() && WTF::isASCIIDigit(*input));

    if (value.hasOverflowed())
        return std::nullopt;

    ASSERT(value.value() > 0);
    return value.value();
}

template<typename Character>
void skipToEnd(StringParsingBuffer<Character>& input)
{
    input.advanceBy(input.lengthRemaining());
}

// MARK: Primary consumers

template<typename Character>
std::optional<TextList> tryConsumeUnorderedDiscTextList(StringParsingBuffer<Character>& input)
{
    if (WTF::skipExactly(input, '*') ||  WTF::skipCharactersExactly(input, WTF::spanReinterpretCast<const Character>(WTF::span(WTF::Unicode::bullet)))) {
        if (input.atEnd())
            return { { { CSS::Keyword::Disc { } }, 0, false } };

        skipToEnd(input);
    }

    return std::nullopt;
}

template<typename Character>
std::optional<TextList> tryConsumeUnorderedDashTextList(StringParsingBuffer<Character>& input)
{
    static constexpr std::array marker { WTF::Unicode::emDash, WTF::Unicode::noBreakSpace, WTF::Unicode::noBreakSpace };

    if (WTF::skipExactly(input, WTF::Unicode::hyphenMinus)) {
        if (input.atEnd())
            return { { Style::ListStyleType { AtomString { std::span { marker } } }, 0, false } };

        skipToEnd(input);
    }

    return std::nullopt;
}

template<typename Character>
std::optional<TextList> tryConsumeOrderedDecimalTextList(StringParsingBuffer<Character>& input)
{
    // This algorithm is similar to the one in StringToIntegerConversion.h, but is stricter and simpler; specifically:
    //
    //   - only base 10 is allowed
    //   - whitespace is not allowed anywhere
    //   - the "-" and "+" signs are not allowed (which consequently restricts the output to non-negative values)
    //   - prefixed "0"s are not allowed (which consequently restricts the output to non-zero values)
    //   - "trailing junk" is only allowed if it is either "." or ")"

    // Must start with an ASCII digit that is not 0.
    if (input.atEnd() || !WTF::isASCIIDigit(*input) || *input == '0')
        return std::nullopt;

    // The format is valid iff there is a "." or a ")" immediately after the digits, and nothing afterwards.
    if (auto start = consumeNumber(input)) {
        if (WTF::skipExactly(input, '.') || WTF::skipExactly(input, ')')) {
            if (input.atEnd())
                return { { { CSS::Keyword::Decimal { } }, *start, true } };
        }
    }

    skipToEnd(input);
    return std::nullopt;
}

template<typename Character>
inline std::optional<TextList> consumeTextList(StringParsingBuffer<Character>& input)
{
    if (auto result = tryConsumeUnorderedDiscTextList(input))
        return result;

    if (auto result = tryConsumeUnorderedDashTextList(input))
        return result;

    if (auto result = tryConsumeOrderedDecimalTextList(input))
        return result;

    return std::nullopt;
}

static AtomString inlineStyleForListStyleType(const StyledElement& element, Style::ListStyleType styleType)
{
    CheckedPtr renderer = element.renderer();
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return WTF::nullAtom();
    }

    CheckedRef style = renderer->style();
    auto& pool = CSSValuePool::singleton();

    Ref value = Style::createCSSValue(pool, style, styleType);

    RefPtr inlineStyle = MutableStyleProperties::create();
    if (RefPtr existingInlineStyle = element.inlineStyle())
        inlineStyle = existingInlineStyle->mutableCopy();

    inlineStyle->setProperty(CSSPropertyListStyleType, WTF::move(value));

    return inlineStyle->asTextAtom(CSS::defaultSerializationContext());
}

static AtomString classNameForSmartList(const TextList& textList)
{
    if (textList.ordered) {
        ASSERT(textList.styleType.isDecimal());
        return "Apple-decimal-list"_s;
    }

    if (textList.styleType.isDisc())
        return "Apple-disc-list"_s;

    ASSERT(textList.styleType.isString());
    return "Apple-dash-list"_s;
}

static AtomString startingOrdinalForList(const StyledElement& element, const TextList& textList)
{
    if (!textList.ordered)
        return WTF::nullAtom();

    ASSERT(textList.styleType.isDecimal());
    ASSERT(textList.startingItemNumber > 0);

    // This is either a newly created list, or an existing list that was just appended to.
    // In the case of the latter, the existing list's ordering takes precedent over any new elements.
    if (element.hasAttributeWithoutSynchronization(HTMLNames::startAttr))
        return WTF::nullAtom();

    return AtomString::number(textList.startingItemNumber);
}

// MARK: Entry points

std::optional<TextList> parseTextList(StringView input)
{
    // The input is parsed to a TextList using these rules:
    //
    //  <U+002A | U+2022>EOF                        |= <U+2022>          (unordered, disc)
    //  <U+2010>EOF                                 |= <U+2014  >        (unordered, dash)
    //  <ordinal><U+002E | U+0029>EOF , ordinal > 0 |= <ordinal><U+002E> (ordered, start=ordinal)
    //  otherwise                                   |= invalid

    return WTF::readCharactersForParsing(input, [](auto buffer) -> std::optional<TextList> {
        return consumeTextList(buffer);
    });
}

Vector<std::pair<const QualifiedName&, AtomString>> nodeAttributesForSmartList(const StyledElement& element, const TextList& list)
{
    Vector<std::pair<const QualifiedName&, AtomString>> result;

    if (auto start = startingOrdinalForList(element, list); !start.isNull())
        result.append({ HTMLNames::startAttr, start });

    if (auto style = inlineStyleForListStyleType(element, list.styleType); !style.isNull())
        result.append({ HTMLNames::styleAttr, style });

    if (auto className = classNameForSmartList(list); !className.isNull())
        result.append({ HTMLNames::classAttr, className });

    return result;
}

bool selectionAllowsSmartLists(const String& text, const VisibleSelection& selection)
{
    RefPtr document = selection.document();
    if (!document)
        return false;

    if (!protect(document->editor())->isSmartListsEnabled())
        return false;

    if (text != " "_s) {
        // Smart Lists can only be "activated" by a space character.
        return false;
    }

    if (!selection.isCaret()) {
        // Smart Lists can only be "activated" if the selection does not contain any content.
        return false;
    }

    if (enclosingList(protect(selection.base().anchorNode()).get())) {
        // Smart Lists can not be "activated" if the selection is already within a list.
        return false;
    }

    return true;
}

} // namespace WebCore
