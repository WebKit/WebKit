/*
 * Copyright (C) 2023 The Chromium Authors.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google LLC nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLDocumentParserFastPath.h"

#include "Document.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementTraversal.h"
#include "HTMLAnchorElement.h"
#include "HTMLBRElement.h"
#include "HTMLBodyElement.h"
#include "HTMLButtonElement.h"
#include "HTMLDivElement.h"
#include "HTMLEntityParser.h"
#include "HTMLInputElement.h"
#include "HTMLLIElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNameCache.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLParagraphElement.h"
#include "HTMLSelectElement.h"
#include "HTMLSpanElement.h"
#include "HTMLUListElement.h"
#include "NodeName.h"
#include "ParserContentPolicy.h"
#include "ParsingUtilities.h"
#include "QualifiedName.h"
#include "Settings.h"
#include <span>
#include <wtf/CheckedRef.h>
#include <wtf/Vector.h>
#include <wtf/WeakRef.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/FastCharacterComparison.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

// Captures the potential outcomes for fast path html parser.
enum class HTMLFastPathResult : uint8_t {
    Succeeded,
    FailedTracingEnabled,
    FailedParserContentPolicy,
    FailedInForm,
    FailedUnsupportedContextTag,
    FailedOptionWithChild,
    FailedDidntReachEndOfInput,
    FailedContainsNull,
    FailedParsingTagName,
    FailedParsingQuotedAttributeValue,
    FailedParsingUnquotedAttributeValue,
    FailedParsingQuotedEscapedAttributeValue,
    FailedParsingUnquotedEscapedAttributeValue,
    FailedParsingCharacterReference,
    FailedEndOfInputReached,
    FailedParsingAttributes,
    FailedParsingSpecificElements,
    FailedParsingElement,
    FailedUnsupportedTag,
    FailedEndOfInputReachedForContainer,
    FailedUnexpectedTagNameCloseState,
    FailedEndTagNameMismatch,
    FailedShadowRoots,
    FailedOnAttribute,
    FailedMaxDepth,
    FailedBigText,
    FailedCssPseudoDirEnabledAndDirAttributeDirty
};

template<typename CharacterType> static inline bool isQuoteCharacter(CharacterType character)
{
    return character == '"' || character == '\'';
}

template<typename CharacterType> static inline bool isValidUnquotedAttributeValueChar(CharacterType character)
{
    return isASCIIAlphanumeric(character) || character == '_' || character == '-';
}

// https://html.spec.whatwg.org/#syntax-attribute-name
template<typename CharacterType> static inline bool isValidAttributeNameChar(CharacterType character)
{
    if (character == '=') // Early return for the most common way to end an attribute.
        return false;
    return isASCIIAlphanumeric(character) || character == '-';
}

template<typename CharacterType> static inline bool isCharAfterTagNameOrAttribute(CharacterType character)
{
    return character == ' ' || character == '>' || isASCIIWhitespace(character) || character == '/';
}

template<typename CharacterType> static inline bool isCharAfterUnquotedAttribute(CharacterType character)
{
    return character == ' ' || character == '>' || isASCIIWhitespace(character);
}

template<typename T> static bool insertInUniquedSortedVector(Vector<T>& vector, const T& value)
{
    auto it = std::lower_bound(vector.begin(), vector.end(), value);
    if (UNLIKELY(it != vector.end() && *it == value))
        return false;
    vector.insert(it - vector.begin(), value);
    return true;
}

#define FOR_EACH_SUPPORTED_TAG(APPLY) \
    APPLY(a, A)                       \
    APPLY(b, B)                       \
    APPLY(body, Body)                 \
    APPLY(br, Br)                     \
    APPLY(button, Button)             \
    APPLY(div, Div)                   \
    APPLY(footer, Footer)             \
    APPLY(i, I)                       \
    APPLY(input, Input)               \
    APPLY(li, Li)                     \
    APPLY(label, Label)               \
    APPLY(option, Option)             \
    APPLY(ol, Ol)                     \
    APPLY(p, P)                       \
    APPLY(select, Select)             \
    APPLY(span, Span)                 \
    APPLY(strong, Strong)             \
    APPLY(ul, Ul)

// This HTML parser is used as a fast-path for setting innerHTML.
// It is faster than the general parser by only supporting a subset of valid
// HTML. This way, it can be spec-compliant without following the algorithm
// described in the spec. Unsupported features or parse errors lead to bailout,
// falling back to the general HTML parser.
// It differs from the general HTML parser in the following ways.
//
// Implementation:
// - It uses recursive descent for better CPU branch prediction.
// - It merges tokenization with parsing.
// - Whenever possible, tokens are represented as subsequences of the original
//   input, avoiding allocating memory for them.
//
// Restrictions:
// - No auto-closing of tags.
// - Wrong nesting of HTML elements (for example nested <p>) leads to bailout
//   instead of fix-up.
// - No custom elements, no "is"-attribute.
// - Unquoted attribute names are very restricted.
// - Many tags are unsupported, but we could support more. For example, <table>
//   because of the complex re-parenting rules
// - No '\0'. The handling of '\0' varies depending upon where it is found
//   and in general the correct handling complicates things.
// - Fails if an attribute name starts with 'on'. Such attributes are generally
//   events that may be fired. Allowing this could be problematic if the fast
//   path fails. For example, the 'onload' event of an <img> would be called
//   multiple times if parsing fails.
// - Fails if a text is encountered larger than Text::defaultLengthLimit. This
//   requires special processing.
// - Fails if a deep hierarchy is encountered. This is both to avoid a crash,
//   but also at a certain depth elements get added as siblings vs children (see
//   use of Settings::defaultMaximumHTMLParserDOMTreeDepth).
// - Fails if an <img> is encountered. Image elements request the image early
//   on, resulting in network connections. Additionally, loading the image
//   may consume preloaded resources.
template<typename CharacterType>
class HTMLFastPathParser {
    using CharacterSpan = std::span<const CharacterType>;
    static_assert(std::is_same_v<CharacterType, UChar> || std::is_same_v<CharacterType, LChar>);

public:
    HTMLFastPathParser(CharacterSpan source, Document& document, ContainerNode& destinationParent)
        : m_document(document)
        , m_destinationParent(destinationParent)
        , m_parsingBuffer(source)
    {
    }

    bool parse(Element& contextElement)
    {
        // This switch checks that the context element is supported and applies the
        // same restrictions regarding content as the fast-path parser does for a
        // corresponding nested tag.
        // This is to ensure that we preserve correct HTML structure with respect
        // to the context tag.
        switch (contextElement.elementName()) {
#define TAG_CASE(TagName, TagClassName)                                                                      \
        case ElementNames::HTML::TagName:                                                                    \
            if constexpr (!TagInfo::TagClassName::isVoid) {                                                  \
                parseCompleteInput<typename TagInfo::TagClassName>();                                        \
                return !parsingFailed();                                                                     \
            }                                                                                                \
            break;
        FOR_EACH_SUPPORTED_TAG(TAG_CASE)
        default:
            break;
#undef TAG_CASE
        }

        didFail(HTMLFastPathResult::FailedUnsupportedContextTag);
        return false;
    }

    HTMLFastPathResult parseResult() const { return m_parseResult; }

private:
    Document& m_document; // FIXME: Use a smart pointer.
    WeakRef<ContainerNode, WeakPtrImplWithEventTargetData> m_destinationParent;

    StringParsingBuffer<CharacterType> m_parsingBuffer;

    HTMLFastPathResult m_parseResult { HTMLFastPathResult::Succeeded };
    bool m_insideOfTagA { false };
    bool m_insideOfTagLi { false };
    // Used to limit how deep a hierarchy can be created. Also note that
    // HTMLConstructionSite ends up flattening when this depth is reached.
    unsigned m_elementDepth { 0 };
    // 32 matches that used by HTMLToken::Attribute.
    Vector<CharacterType, 32> m_charBuffer;
    Vector<UChar> m_ucharBuffer;
    // The inline capacity matches HTMLToken::AttributeList.
    Vector<Attribute, 10> m_attributeBuffer;
    Vector<AtomStringImpl*> m_attributeNames;


    enum class PermittedParents : uint8_t {
        PhrasingOrFlowContent, // allowed in phrasing content or flow content
        FlowContent, // only allowed in flow content, not in phrasing content
        Special, // only allowed for special parents
    };

    enum class PhrasingContent : bool { No, Yes };

    struct TagInfo {
        template<typename T, PermittedParents parents>
        struct Tag {
            using HTMLElementClass = T;
            static constexpr PermittedParents permittedParents = parents;
            static Ref<HTMLElementClass> create(Document& document)
            {
                return HTMLElementClass::create(document);
            }
            static constexpr bool allowedInPhrasingOrFlowContent()
            {
                return permittedParents == PermittedParents::PhrasingOrFlowContent;
            }
            static constexpr bool allowedInFlowContent()
            {
                return permittedParents == PermittedParents::PhrasingOrFlowContent || permittedParents == PermittedParents::FlowContent;
            }
        };

        template<typename T, PermittedParents parents>
        struct VoidTag : Tag<T, parents> {
            static constexpr bool isVoid = true;
        };

        template<typename T, PermittedParents parents>
        struct ContainerTag : Tag<T, parents> {
            static constexpr bool isVoid = false;

            static RefPtr<HTMLElement> parseChild(ContainerNode& parent, HTMLFastPathParser& self)
            {
                return self.parseElement<PhrasingContent::No>(parent);
            }
        };

        // A tag that can only contain phrasing content. If a tag is considered phrasing content itself is decided by
        // `allowedInPhrasingContent`.
        template<typename T, PermittedParents parents>
        struct ContainsPhrasingContentTag : ContainerTag<T, parents> {
            static constexpr bool isVoid = false;

            static RefPtr<HTMLElement> parseChild(ContainerNode& parent, HTMLFastPathParser& self)
            {
                return self.parseElement<PhrasingContent::Yes>(parent);
            }
        };

        struct A : ContainerTag<HTMLAnchorElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::a;
            static constexpr CharacterType tagNameCharacters[] = { 'a' };

            static RefPtr<HTMLElement> parseChild(ContainerNode& parent, HTMLFastPathParser& self)
            {
                ASSERT(!self.m_insideOfTagA);
                self.m_insideOfTagA = true;
                auto result = ContainerTag<HTMLAnchorElement, PermittedParents::FlowContent>::parseChild(parent, self);
                self.m_insideOfTagA = false;
                return result;
            }
        };

        struct AWithPhrasingContent : ContainsPhrasingContentTag<HTMLAnchorElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::a;
            static constexpr CharacterType tagNameCharacters[] = { 'a' };

            static RefPtr<HTMLElement> parseChild(ContainerNode& parent, HTMLFastPathParser& self)
            {
                ASSERT(!self.m_insideOfTagA);
                self.m_insideOfTagA = true;
                auto result = ContainsPhrasingContentTag<HTMLAnchorElement, PermittedParents::PhrasingOrFlowContent>::parseChild(parent, self);
                self.m_insideOfTagA = false;
                return result;
            }
        };

        struct B : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::b;
            static constexpr CharacterType tagNameCharacters[] = { 'b' };

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::bTag, document);
            }
        };

        struct Br : VoidTag<HTMLBRElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::br;
            static constexpr CharacterType tagNameCharacters[] = { 'b', 'r' };
        };

        struct Button : ContainsPhrasingContentTag<HTMLButtonElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::button;
            static constexpr CharacterType tagNameCharacters[] = { 'b', 'u', 't', 't', 'o', 'n' };
        };

        struct Div : ContainerTag<HTMLDivElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::div;
            static constexpr CharacterType tagNameCharacters[] = { 'd', 'i', 'v' };
        };

        struct Body : ContainerTag<HTMLBodyElement, PermittedParents::Special> {
            static constexpr ElementName tagName = ElementNames::HTML::body;
            static constexpr CharacterType tagNameCharacters[] = { 'b', 'o', 'd', 'y' };
        };

        struct Footer : ContainerTag<HTMLElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::footer;
            static constexpr CharacterType tagNameCharacters[] = { 'f', 'o', 'o', 't', 'e', 'r' };

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::footerTag, document);
            }
        };

        struct I : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::i;
            static constexpr CharacterType tagNameCharacters[] = { 'i' };

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::iTag, document);
            }
        };

        struct Input : VoidTag<HTMLInputElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::input;
            static constexpr CharacterType tagNameCharacters[] = { 'i', 'n', 'p', 'u', 't' };

            static Ref<HTMLInputElement> create(Document& document)
            {
                return HTMLInputElement::create(HTMLNames::inputTag, document, /* form */ nullptr, /* createdByParser */ true);
            }
        };

        struct Li : ContainerTag<HTMLLIElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::li;
            static constexpr CharacterType tagNameCharacters[] = { 'l', 'i' };
        };

        struct Label : ContainsPhrasingContentTag<HTMLLabelElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::label;
            static constexpr CharacterType tagNameCharacters[] = { 'l', 'a', 'b', 'e', 'l' };
        };

        struct Option : ContainerTag<HTMLOptionElement, PermittedParents::Special> {
            static constexpr ElementName tagName = ElementNames::HTML::option;
            static constexpr CharacterType tagNameCharacters[] = { 'o', 'p', 't', 'i', 'o', 'n' };

            static RefPtr<HTMLElement> parseChild(ContainerNode&, HTMLFastPathParser& self)
            {
                // <option> can only contain a text content.
                return self.didFail(HTMLFastPathResult::FailedOptionWithChild, nullptr);
            }
        };

        struct Ol : ContainerTag<HTMLOListElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::ol;
            static constexpr CharacterType tagNameCharacters[] = { 'o', 'l' };

            static RefPtr<HTMLElement> parseChild(ContainerNode& parent, HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Li>(parent);
            }
        };

        struct P : ContainsPhrasingContentTag<HTMLParagraphElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::p;
            static constexpr CharacterType tagNameCharacters[] = { 'p' };
        };

        struct Select : ContainerTag<HTMLSelectElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::select;
            static constexpr CharacterType tagNameCharacters[] = { 's', 'e', 'l', 'e', 'c', 't' };

            static RefPtr<HTMLElement> parseChild(ContainerNode& parent, HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Option>(parent);
            }
        };

        struct Span : ContainsPhrasingContentTag<HTMLSpanElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::span;
            static constexpr CharacterType tagNameCharacters[] = { 's', 'p', 'a', 'n' };
        };

        struct Strong : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::strong;
            static constexpr CharacterType tagNameCharacters[] = { 's', 't', 'r', 'o', 'n', 'g' };

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::strongTag, document);
            }
        };

        struct Ul : ContainerTag<HTMLUListElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementNames::HTML::ul;
            static constexpr CharacterType tagNameCharacters[] = { 'u', 'l' };

            static RefPtr<HTMLElement> parseChild(ContainerNode& parent, HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Li>(parent);
            }
        };
    };

    template<typename ParentTag> void parseCompleteInput()
    {
        parseChildren<ParentTag>(m_destinationParent.get());
        if (UNLIKELY(m_parsingBuffer.hasCharactersRemaining()))
            didFail(HTMLFastPathResult::FailedDidntReachEndOfInput);
    }

    // We first try to scan text as an unmodified subsequence of the input.
    // However, if there are escape sequences, we have to copy the text to a
    // separate buffer and we might go outside of `Char` range if we are in an
    // `LChar` parser.
    String scanText()
    {
        auto* start = m_parsingBuffer.position();
        const auto* end = start + m_parsingBuffer.lengthRemaining();

        auto scalarMatch = [&](auto character) ALWAYS_INLINE_LAMBDA {
            return character == '<' || character == '&' || character == '\r' || character == '\0';
        };

        auto vectorEquals8Bit = [&](auto input) ALWAYS_INLINE_LAMBDA {
            // https://lemire.me/blog/2024/06/08/scan-html-faster-with-simd-instructions-chrome-edition/
            // By looking up the table via lower 4bit, we can identify the category.
            // '\0' => 0000 0000
            // '&'  => 0010 0110
            // '<'  => 0011 1100
            // '\r' => 0000 1101
            constexpr simde_uint8x16_t lowNibbleMask { '\0', 0, 0, 0, 0, 0, '&', 0, 0, 0, 0, 0, '<', '\r', 0, 0 };
            constexpr simde_uint8x16_t v0f = SIMD::splat8(0x0f);
            return SIMD::equal(simde_vqtbl1q_u8(lowNibbleMask, SIMD::bitAnd(input, v0f)), input);
        };

        const CharacterType* cursor = nullptr;
        if constexpr (sizeof(CharacterType) == 1) {
            auto vectorMatch = [&](auto input) ALWAYS_INLINE_LAMBDA {
                return SIMD::findFirstNonZeroIndex(vectorEquals8Bit(input));
            };
            cursor = SIMD::find(std::span { start, end }, vectorMatch, scalarMatch);
        } else {
            auto vectorMatch = [&](auto input) ALWAYS_INLINE_LAMBDA {
                constexpr simde_uint8x16_t zeros = SIMD::splat8(0);
                return SIMD::findFirstNonZeroIndex(SIMD::bitAnd(vectorEquals8Bit(input.val[0]), SIMD::equal(input.val[1], zeros)));
            };
            cursor = SIMD::findInterleaved(std::span { start, end }, vectorMatch, scalarMatch);
        }
        m_parsingBuffer.setPosition(cursor);

        if (cursor != end) {
            if (UNLIKELY(*cursor == '\0'))
                return didFail(HTMLFastPathResult::FailedContainsNull, String());

            if (*cursor == '&' || *cursor == '\r') {
                m_parsingBuffer.setPosition(start);
                return scanEscapedText();
            }
        }

        unsigned length = cursor - start;
        if (UNLIKELY(length >= Text::defaultLengthLimit))
            return didFail(HTMLFastPathResult::FailedBigText, String());

        return length ? String({ start, length }) : String();
    }

    // Slow-path of `scanText()`, which supports escape sequences by copying to a
    // separate buffer.
    String scanEscapedText()
    {
        m_ucharBuffer.shrink(0);
        while (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer != '<') {
            if (*m_parsingBuffer == '&') {
                scanHTMLCharacterReference(m_ucharBuffer);
                if (parsingFailed())
                    return { };
            } else if (*m_parsingBuffer == '\r') {
                // Normalize "\r\n" to "\n" according to https://infra.spec.whatwg.org/#normalize-newlines.
                m_parsingBuffer.advance();
                if (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer == '\n')
                    m_parsingBuffer.advance();
                m_ucharBuffer.append('\n');
            } else if (UNLIKELY(*m_parsingBuffer == '\0'))
                return didFail(HTMLFastPathResult::FailedContainsNull, String());
            else
                m_ucharBuffer.append(m_parsingBuffer.consume());
        }
        if (UNLIKELY(m_ucharBuffer.size() >= Text::defaultLengthLimit))
            return didFail(HTMLFastPathResult::FailedBigText, String());
        return m_ucharBuffer.isEmpty() ? String() : String(std::exchange(m_ucharBuffer, { }));
    }

    // Scan a tagName and convert to lowercase if necessary.
    ElementName scanTagName()
    {
        auto* start = m_parsingBuffer.position();
        skipWhile<isASCIILower>(m_parsingBuffer);

        if (m_parsingBuffer.atEnd() || !isCharAfterTagNameOrAttribute(*m_parsingBuffer)) {
            // Try parsing a case-insensitive tagName.
            m_charBuffer.shrink(0);
            m_parsingBuffer.setPosition(start);
            while (m_parsingBuffer.hasCharactersRemaining()) {
                auto c = *m_parsingBuffer;
                if (isASCIIUpper(c))
                    c = toASCIILowerUnchecked(c);
                else if (!isASCIILower(c))
                    break;
                m_parsingBuffer.advance();
                m_charBuffer.append(c);
            }
            if (UNLIKELY(m_parsingBuffer.atEnd() || !isCharAfterTagNameOrAttribute(*m_parsingBuffer)))
                return didFail(HTMLFastPathResult::FailedParsingTagName, ElementName::Unknown);
            skipWhile<isASCIIWhitespace>(m_parsingBuffer);
            return findHTMLElementName(m_charBuffer.span());
        }
        auto tagName = findHTMLElementName(std::span { start, static_cast<size_t>(m_parsingBuffer.position() - start) });
        skipWhile<isASCIIWhitespace>(m_parsingBuffer);
        return tagName;
    }

    QualifiedName scanAttributeName()
    {
        // First look for all lower case. This path doesn't require any mapping of
        // input. This path could handle other valid attribute name chars, but they
        // are not as common, so it only looks for lowercase.
        auto* start = m_parsingBuffer.position();
        skipWhile<isASCIILower>(m_parsingBuffer);
        if (UNLIKELY(m_parsingBuffer.atEnd()))
            return didFail(HTMLFastPathResult::FailedEndOfInputReached, nullQName());

        CharacterSpan attributeName;
        if (UNLIKELY(isValidAttributeNameChar(*m_parsingBuffer))) {
            // At this point name does not contain lowercase. It may contain upper-case,
            // which requires mapping. Assume it does.
            m_parsingBuffer.setPosition(start);
            m_charBuffer.shrink(0);
            // isValidAttributeNameChar() returns false if end of input is reached.
            do {
                auto c = m_parsingBuffer.consume();
                if (isASCIIUpper(c))
                    c = toASCIILowerUnchecked(c);
                m_charBuffer.append(c);
            } while (m_parsingBuffer.hasCharactersRemaining() && isValidAttributeNameChar(*m_parsingBuffer));
            attributeName = m_charBuffer.span();
        } else
            attributeName = { start, static_cast<size_t>(m_parsingBuffer.position() - start) };

        if (attributeName.empty())
            return nullQName();
        if (attributeName.size() > 2 && compareCharacters(attributeName.data(), 'o', 'n')) {
            // These attributes likely contain script that may be executed at random
            // points, which could cause problems if parsing via the fast path
            // fails. For example, an image's onload event.
            return nullQName();
        }
        return HTMLNameCache::makeAttributeQualifiedName(attributeName);
    }

    AtomString scanAttributeValue()
    {
        skipWhile<isASCIIWhitespace>(m_parsingBuffer);
        auto* start = m_parsingBuffer.position();
        size_t length = 0;
        if (m_parsingBuffer.hasCharactersRemaining() && isQuoteCharacter(*m_parsingBuffer)) {
            auto quoteChar = m_parsingBuffer.consume();

            auto find = [&]<CharacterType quoteChar>(std::span<const CharacterType> span) ALWAYS_INLINE_LAMBDA {
                auto scalarMatch = [&](auto character) ALWAYS_INLINE_LAMBDA {
                    return character == quoteChar || character == '&' || character == '\r' || character == '\0';
                };

                auto vectorEquals8Bit = [&](auto input) ALWAYS_INLINE_LAMBDA {
                    // https://lemire.me/blog/2024/06/08/scan-html-faster-with-simd-instructions-chrome-edition/
                    // By looking up the table via lower 4bit, we can identify the category.
                    // '\0' => 0000 0000
                    // '&'  => 0010 0110
                    // '\'' => 0010 0111
                    // '\r' => 0000 1101
                    //
                    // OR
                    //
                    // '\0' => 0000 0000
                    // '"'  => 0010 0010
                    // '&'  => 0010 0110
                    // '\r' => 0000 1101
                    constexpr auto lowNibbleMask  = quoteChar == '\'' ? simde_uint8x16_t { '\0', 0, 0, 0, 0, 0, '&', '\'', 0, 0, 0, 0, 0, '\r', 0, 0 } : simde_uint8x16_t { '\0', 0, '"', 0, 0, 0, '&', 0, 0, 0, 0, 0, 0, '\r', 0, 0 };
                    constexpr auto v0f = SIMD::splat8(0x0f);
                    return SIMD::equal(simde_vqtbl1q_u8(lowNibbleMask, SIMD::bitAnd(input, v0f)), input);
                };

                if constexpr (sizeof(CharacterType) == 1) {
                    auto vectorMatch = [&](auto input) ALWAYS_INLINE_LAMBDA {
                        return SIMD::findFirstNonZeroIndex(vectorEquals8Bit(input));
                    };
                    return SIMD::find(span, vectorMatch, scalarMatch);
                } else {
                    auto vectorMatch = [&](auto input) ALWAYS_INLINE_LAMBDA {
                        constexpr simde_uint8x16_t zeros = SIMD::splat8(0);
                        return SIMD::findFirstNonZeroIndex(SIMD::bitAnd(vectorEquals8Bit(input.val[0]), SIMD::equal(input.val[1], zeros)));
                    };
                    return SIMD::findInterleaved(span, vectorMatch, scalarMatch);
                }
            };

            start = m_parsingBuffer.position();
            const auto* end = start + m_parsingBuffer.lengthRemaining();
            const auto* cursor = quoteChar == '\'' ? find.template operator()<'\''>(std::span { start, end }) : find.template operator()<'"'>(std::span { start, end });
            if (UNLIKELY(cursor == end))
                return didFail(HTMLFastPathResult::FailedParsingQuotedAttributeValue, emptyAtom());

            length = cursor - start;
            if (UNLIKELY(*cursor != quoteChar)) {
                if (LIKELY(*cursor == '&' || *cursor == '\r')) {
                    m_parsingBuffer.setPosition(start - 1);
                    return scanEscapedAttributeValue();
                }
                return didFail(HTMLFastPathResult::FailedParsingQuotedAttributeValue, emptyAtom());
            }
            m_parsingBuffer.setPosition(cursor + 1);
        } else {
            skipWhile<isValidUnquotedAttributeValueChar>(m_parsingBuffer);
            length = m_parsingBuffer.position() - start;
            if (UNLIKELY(m_parsingBuffer.atEnd() || !isCharAfterUnquotedAttribute(*m_parsingBuffer)))
                return didFail(HTMLFastPathResult::FailedParsingUnquotedAttributeValue, emptyAtom());
        }
        return HTMLNameCache::makeAttributeValue({ start, length });
    }

    // Slow path for scanning an attribute value. Used for special cases such
    // as '&' and '\r'.
    AtomString scanEscapedAttributeValue()
    {
        skipWhile<isASCIIWhitespace>(m_parsingBuffer);
        m_ucharBuffer.shrink(0);
        if (UNLIKELY(!m_parsingBuffer.hasCharactersRemaining() || !isQuoteCharacter(*m_parsingBuffer)))
            return didFail(HTMLFastPathResult::FailedParsingUnquotedEscapedAttributeValue, emptyAtom());

        auto quoteChar = m_parsingBuffer.consume();
        if (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer != quoteChar) {
            if (parsingFailed())
                return emptyAtom();
            auto c = *m_parsingBuffer;
            if (c == '&')
                scanHTMLCharacterReference(m_ucharBuffer);
            else if (c == '\r') {
                m_parsingBuffer.advance();
                // Normalize "\r\n" to "\n" according to https://infra.spec.whatwg.org/#normalize-newlines.
                if (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer == '\n')
                    m_parsingBuffer.advance();
                m_ucharBuffer.append('\n');
            } else {
                m_ucharBuffer.append(c);
                m_parsingBuffer.advance();
            }
        }
        if (UNLIKELY(m_parsingBuffer.atEnd() || m_parsingBuffer.consume() != quoteChar))
            return didFail(HTMLFastPathResult::FailedParsingQuotedEscapedAttributeValue, emptyAtom());

        return HTMLNameCache::makeAttributeValue({ m_ucharBuffer.data(), m_ucharBuffer.size() });
    }

    void scanHTMLCharacterReference(Vector<UChar>& out)
    {
        ASSERT(*m_parsingBuffer == '&');
        m_parsingBuffer.advance();

        if (LIKELY(m_parsingBuffer.lengthRemaining() >= 2)) {
            if (auto entity = consumeHTMLEntity(m_parsingBuffer); !entity.failed()) {
                out.append(entity.span());
                return;
            }
        }
        out.append('&');
    }

    bool parsingFailed() const { return m_parseResult != HTMLFastPathResult::Succeeded; }

    void didFail(HTMLFastPathResult result)
    {
        if (m_parseResult == HTMLFastPathResult::Succeeded)
            m_parseResult = result;
    }

    template<typename ReturnValueType> ReturnValueType didFail(HTMLFastPathResult result, ReturnValueType returnValue)
    {
        didFail(result);
        return returnValue;
    }

    template<typename ParentTag> void parseChildren(ContainerNode& parent)
    {
        while (true) {
            auto text = scanText();
            if (parsingFailed())
                return;

            if (!text.isNull()) {
                if (!parent.isConnected())
                    parent.parserAppendChildIntoIsolatedTree(Text::create(m_document, WTFMove(text)));
                else
                    parent.parserAppendChild(Text::create(m_document, WTFMove(text)));
            }

            if (m_parsingBuffer.atEnd())
                return;
            ASSERT(*m_parsingBuffer == '<');
            m_parsingBuffer.advance();
            if (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer == '/') {
                // We assume that we found the closing tag. The tagName will be checked by the caller `parseContainerElement()`.
                return;
            }
            if (UNLIKELY(++m_elementDepth == Settings::defaultMaximumHTMLParserDOMTreeDepth))
                return didFail(HTMLFastPathResult::FailedMaxDepth);
            auto child = ParentTag::parseChild(parent, *this);
            --m_elementDepth;
            if (parsingFailed())
                return;
            ASSERT(child);
        }
    }

    void parseAttributes(HTMLElement& parent)
    {
        m_attributeBuffer.shrink(0);
        m_attributeNames.shrink(0);

        bool hasDuplicateAttributes = false;
        while (true) {
            auto attributeName = scanAttributeName();
            if (attributeName == nullQName()) {
                if (LIKELY(m_parsingBuffer.hasCharactersRemaining())) {
                    if (*m_parsingBuffer == '>') {
                        m_parsingBuffer.advance();
                        break;
                    }
                    if (*m_parsingBuffer == '/') {
                        m_parsingBuffer.advance();
                        skipWhile<isASCIIWhitespace>(m_parsingBuffer);
                        if (UNLIKELY(m_parsingBuffer.atEnd() || m_parsingBuffer.consume() != '>'))
                            return didFail(HTMLFastPathResult::FailedParsingAttributes);
                        break;
                    }
                }
                return didFail(HTMLFastPathResult::FailedParsingAttributes);
            }
            skipWhile<isASCIIWhitespace>(m_parsingBuffer);
            AtomString attributeValue { emptyAtom() };
            if (skipExactly(m_parsingBuffer, '=')) {
                attributeValue = scanAttributeValue();
                skipWhile<isASCIIWhitespace>(m_parsingBuffer);
            }
            if (UNLIKELY(!insertInUniquedSortedVector(m_attributeNames, attributeName.localName().impl()))) {
                hasDuplicateAttributes = true;
                continue;
            }
            m_attributeBuffer.append(Attribute { WTFMove(attributeName), WTFMove(attributeValue) });
        }
        parent.parserSetAttributes(m_attributeBuffer);
        if (UNLIKELY(hasDuplicateAttributes))
            parent.setHasDuplicateAttribute(true);
    }

    template<typename... Tags> RefPtr<HTMLElement> parseSpecificElements(ContainerNode& parent)
    {
        auto tagName = scanTagName();
        return parseSpecificElements<Tags...>(tagName, parent);
    }

    template<void* = nullptr> RefPtr<HTMLElement> parseSpecificElements(ElementName, ContainerNode&)
    {
        return didFail(HTMLFastPathResult::FailedParsingSpecificElements, nullptr);
    }

    template<typename Tag, typename... OtherTags> RefPtr<HTMLElement> parseSpecificElements(ElementName tagName, ContainerNode& parent)
    {
        if (tagName == Tag::tagName)
            return parseElementAfterTagName<Tag>(parent);
        return parseSpecificElements<OtherTags...>(tagName, parent);
    }

    template<PhrasingContent phrasingContent> RefPtr<HTMLElement> parseElement(ContainerNode& parent)
    {
        auto tagName = scanTagName();

        // HTML has complicated rules around auto-closing tags and re-parenting
        // DOM nodes. We avoid complications with auto-closing rules by disallowing
        // certain nesting. In particular, we bail out if non-phrasing-content
        // elements are nested into elements that require phrasing content.
        // Similarly, we disallow nesting <a> tags. But tables for example have
        // complex re-parenting rules that cannot be captured in this way, so we
        // cannot support them.
#define TAG_CASE(TagName, TagClassName)                                            \
        case ElementNames::HTML::TagName:                                          \
        if constexpr (phrasingContent == PhrasingContent::No ? TagInfo::TagClassName::allowedInFlowContent() : TagInfo::TagClassName::allowedInPhrasingOrFlowContent()) \
                return parseElementAfterTagName<typename TagInfo::TagClassName>(parent); \
            break;

        switch (tagName) {
        case ElementNames::HTML::a:
            // <a> tags must not be nested, because HTML parsing would auto-close
            // the outer one when encountering a nested one.
            if (!m_insideOfTagA)
                return phrasingContent == PhrasingContent::No ? parseElementAfterTagName<typename TagInfo::A>(parent) : parseElementAfterTagName<typename TagInfo::AWithPhrasingContent>(parent);
            break;
            TAG_CASE(b, B)
            TAG_CASE(br, Br)
            TAG_CASE(button, Button)
            TAG_CASE(div, Div)
            TAG_CASE(footer, Footer)
            TAG_CASE(i, I)
            TAG_CASE(input, Input)
            case ElementNames::HTML::li:
                if constexpr (phrasingContent == PhrasingContent::No ? TagInfo::Li::allowedInFlowContent() : TagInfo::Li::allowedInPhrasingOrFlowContent()) {
                    // <li>s autoclose when multiple are encountered. For example,
                    // <li><li></li></li> results in sibling <li>s, not nested <li>s. Fail
                    // in such a case.
                    if (!m_insideOfTagLi) {
                        m_insideOfTagLi = true;
                        auto result = parseElementAfterTagName<typename TagInfo::Li>(parent);
                        m_insideOfTagLi = false;
                        return result;
                    }
                }
                break;
            TAG_CASE(label, Label)
            TAG_CASE(option, Option)
            TAG_CASE(ol, Ol)
            TAG_CASE(p, P)
            TAG_CASE(select, Select)
            TAG_CASE(span, Span)
            TAG_CASE(strong, Strong)
            TAG_CASE(ul, Ul)
#undef TAG_CASE
        default:
            break;
        }
        return didFail(HTMLFastPathResult::FailedUnsupportedTag, nullptr);
    }

    template<typename Tag> Ref<typename Tag::HTMLElementClass> parseElementAfterTagName(ContainerNode& parent)
    {
        if constexpr (Tag::isVoid)
            return parseVoidElement(Tag::create(m_document), parent);
        else
            return parseContainerElement<Tag>(Tag::create(m_document), parent);
    }

    template<typename Tag> Ref<typename Tag::HTMLElementClass> parseContainerElement(Ref<typename Tag::HTMLElementClass>&& element, ContainerNode& parent)
    {
        parseAttributes(element);
        if (parsingFailed())
            return WTFMove(element);
        if (!parent.isConnected())
            parent.parserAppendChildIntoIsolatedTree(element);
        else
            parent.parserAppendChild(element);
        element->beginParsingChildren();
        parseChildren<Tag>(element);
        if (UNLIKELY(parsingFailed() || m_parsingBuffer.atEnd()))
            return didFail(HTMLFastPathResult::FailedEndOfInputReachedForContainer, element);

        // parseChildren<Tag>(element) stops after the (hopefully) closing tag's `<`
        // and fails if the the current char is not '/'.
        ASSERT(*m_parsingBuffer == '/');
        m_parsingBuffer.advance();

        if (UNLIKELY(!skipCharactersExactly(m_parsingBuffer, Tag::tagNameCharacters))) {
            if (UNLIKELY(!skipLettersExactlyIgnoringASCIICase(m_parsingBuffer, Tag::tagNameCharacters)))
                return didFail(HTMLFastPathResult::FailedEndTagNameMismatch, element);
        }
        skipWhile<isASCIIWhitespace>(m_parsingBuffer);

        if (UNLIKELY(m_parsingBuffer.atEnd() || m_parsingBuffer.consume() != '>'))
            return didFail(HTMLFastPathResult::FailedUnexpectedTagNameCloseState, element);

        element->finishParsingChildren();
        return WTFMove(element);
    }

    template<typename HTMLElementType> Ref<HTMLElementType> parseVoidElement(Ref<HTMLElementType>&& element, ContainerNode& parent)
    {
        parseAttributes(element);
        if (parsingFailed())
            return WTFMove(element);
        if (!parent.isConnected())
            parent.parserAppendChildIntoIsolatedTree(element);
        else
            parent.parserAppendChild(element);
        element->beginParsingChildren();
        element->finishParsingChildren();
        return WTFMove(element);
    }
};

static bool canUseFastPath(Element& contextElement, OptionSet<ParserContentPolicy> policy)
{
    // We could probably allow other content policies too, as we do not support scripts or plugins anyway.
    if (!policy.contains(ParserContentPolicy::AllowScriptingContent))
        return false;

    // If we are within a form element, we would need to create associations, which we do not. Therefore, we do not
    // support this case. See HTMLConstructionSite::initFragmentParsing() and HTMLConstructionSite::createElement()
    // for the corresponding code on the slow-path.
    if (!contextElement.document().isTemplateDocument() && lineageOfType<HTMLFormElement>(contextElement).first())
        return false;

    return true;
}

template<typename CharacterType>
static bool tryFastParsingHTMLFragmentImpl(std::span<const CharacterType> source, Document& document, ContainerNode& destinationParent, Element& contextElement)
{
    HTMLFastPathParser parser { source, document, destinationParent };
    return parser.parse(contextElement);
}

bool tryFastParsingHTMLFragment(StringView source, Document& document, ContainerNode& destinationParent, Element& contextElement, OptionSet<ParserContentPolicy> policy)
{
    if (!canUseFastPath(contextElement, policy))
        return false;

    if (source.is8Bit())
        return tryFastParsingHTMLFragmentImpl(source.span8(), document, destinationParent, contextElement);
    return tryFastParsingHTMLFragmentImpl(source.span16(), document, destinationParent, contextElement);
}

#undef FOR_EACH_SUPPORTED_TAG

} // namespace WebCore
