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
#include "DocumentFragment.h"
#include "ElementAncestorIterator.h"
#include "ElementTraversal.h"
#include "FragmentScriptingPermission.h"
#include "HTMLAnchorElement.h"
#include "HTMLBRElement.h"
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
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLSpanElement.h"
#include "HTMLUListElement.h"
#include "QualifiedName.h"
#include "Settings.h"
#include <wtf/Span.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

// Captures the potential outcomes for fast path html parser.
enum class HTMLFastPathResult {
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

template<class Char, size_t n> static bool operator==(Span<const Char> span, const char (&s)[n])
{
    if (span.size() != n - 1)
        return false;

    for (size_t i = 0; i < n - 1; ++i) {
        if (span[i] != s[i])
            return false;
    }
    return true;
}

#if ASSERT_ENABLED
template<size_t n> static constexpr bool onlyContainsLowercaseASCIILetters(const char (&s)[n])
{
    for (size_t i = 0; i < n - 1; ++i) {
        if (s[i] < 'a' || s[i] > 'z')
            return false;
    }
    return true;
}
#endif // ASSERT_ENABLED

// A hash function that is just good enough to distinguish the supported tagNames. It needs to be
// adapted as soon as we have colliding tagNames. The implementation was chosen to map to a dense
// integer range to allow for compact switch jump-tables. If adding support for a new tag results
// in a collision, then pick a new function that minimizes the number of operations and results
// in a dense integer range.
template<size_t n> static constexpr uint32_t tagNameHash(const char (&s)[n])
{
    // The fast-path parser only scans for letters in tagNames.
    ASSERT_UNDER_CONSTEXPR_CONTEXT(onlyContainsLowercaseASCIILetters<n>(s));
    ASSERT_UNDER_CONSTEXPR_CONTEXT(s[n - 1] == '\0');
    // This function is called with null-termined string, which should be used in the hash
    // implementation, hence the -2.
    return (s[0] + 17 * s[n - 2]) & 63;
}

template<class Char> static uint32_t tagNameHash(Span<const Char> s)
{
    return (s[0] + 17 * s[s.size() - 1]) & 63;
}

static uint32_t tagNameHash(const String& s)
{
    return (s[0] + 17 * s[s.length() - 1]) & 63;
}

#define FOR_EACH_SUPPORTED_TAG(APPLY) \
    APPLY(a, A)                       \
    APPLY(b, B)                       \
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
// - No duplicate attributes. This restriction could be lifted easily.
// - Unquoted attribute names are very restricted.
// - Many tags are unsupported, but we could support more. For example, <table>
//   because of the complex re-parenting rules
// - Only a few named "&" character references are supported.
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
// - Fails if Document::isDirAttributeDirty() is true and CSSPseudoDirEnabled is
//   enabled. This is necessary as state needed to support css-pseudo dir is set
//   in HTMLElement::beginParsingChildren(), which this does not call.
template<class Char>
class HTMLFastPathParser {
    using CharSpan = Span<const Char>;
    using UCharSpan = Span<const UChar>;
    static_assert(std::is_same_v<Char, UChar> || std::is_same_v<Char, LChar>);

public:
    HTMLFastPathParser(CharSpan source, Document& document, DocumentFragment& fragment)
        : m_source(source)
        , m_document(document)
        , m_fragment(fragment)
    {
    }

    bool parse(Element& contextElement)
    {
        auto contextTag = contextElement.tagQName();
        ASSERT(!contextTag.localName().isEmpty());

        // This switch checks that the context element is supported and applies the
        // same restrictions regarding content as the fast-path parser does for a
        // corresponding nested tag.
        // This is to ensure that we preserve correct HTML structure with respect
        // to the context tag.
        //
        // If this switch has duplicate cases, then `tagNameHash()` needs to be
        // updated.
        switch (tagNameHash(contextTag.localName())) {
#define TAG_CASE(TagName, TagClassName)                                                                      \
        case tagNameHash(TagInfo::TagClassName::tagName):                                                    \
            ASSERT(HTMLNames::TagName##Tag->localName().string().ascii() == TagInfo::TagClassName::tagName); \
            if constexpr (!TagInfo::TagClassName::isVoid) {                                                  \
                /* The hash function won't return collisions for the supported tags, but this function */    \
                /* takes potentially unsupported tags, which may collide. Protect against that by */         \
                /* checking equality. */                                                                     \
                if (contextTag == HTMLNames::TagName##Tag) {                                                 \
                    parseCompleteInput<typename TagInfo::TagClassName>();                                    \
                    return !m_parsingFailed;                                                                 \
                }                                                                                            \
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
    CharSpan m_source;
    Document& m_document;
    DocumentFragment& m_fragment;

    const Char* const m_end { m_source.data() + m_source.size() };
    const Char* m_position { m_source.data() };

    bool m_parsingFailed { false };
    bool m_insideOfTagA { false };
    // Used to limit how deep a hierarchy can be created. Also note that
    // HTMLConstructionSite ends up flattening when this depth is reached.
    unsigned m_elementDepth { 0 };
    // 32 matches that used by HTMLToken::Attribute.
    Vector<Char, 32> m_charBuffer;
    Vector<UChar> m_ucharBuffer;
    // Used if the attribute name contains upper case ascii (which must be mapped to lower case).
    // 32 matches that used by HTMLToken::Attribute.
    Vector<Char, 32> m_attributeNameBuffer;
    Vector<Attribute> m_attributeBuffer;
    Vector<StringImpl*> m_attributeNames;
    HTMLFastPathResult m_parseResult { HTMLFastPathResult::Succeeded };

    enum class PermittedParents {
        PhrasingOrFlowContent, // allowed in phrasing content or flow content
        FlowContent, // only allowed in flow content, not in phrasing content
        Special, // only allowed for special parents
    };

    struct TagInfo {
        template<class T, PermittedParents parents>
        struct Tag {
            using ElemClass = T;
            static constexpr PermittedParents permittedParents = parents;
            static Ref<ElemClass> create(Document& document)
            {
                return ElemClass::create(document);
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

        template<class T, PermittedParents parents>
        struct VoidTag : Tag<T, parents> {
            static constexpr bool isVoid = true;
        };

        template<class T, PermittedParents parents>
        struct ContainerTag : Tag<T, parents> {
            static constexpr bool isVoid = false;

            static RefPtr<Element> parseChild(HTMLFastPathParser& self)
            {
                return self.parseElement</*nonPhrasingContent*/ true>();
            }
        };

        // A tag that can only contain phrasing content. If a tag is considered phrasing content itself is decided by
        // `allowedInPhrasingContent`.
        template<class T, PermittedParents parents>
        struct ContainsPhrasingContentTag : ContainerTag<T, parents> {
            static constexpr bool isVoid = false;

            static RefPtr<Element> parseChild(HTMLFastPathParser& self)
            {
                return self.parseElement</*nonPhrasingContent*/ false>();
            }
        };

        struct A : ContainerTag<HTMLAnchorElement, PermittedParents::FlowContent> {
            static constexpr const char tagName[] = "a";

            static RefPtr<Element> parseChild(HTMLFastPathParser& self)
            {
                ASSERT(!self.m_insideOfTagA);
                self.m_insideOfTagA = true;
                auto result = ContainerTag<HTMLAnchorElement, PermittedParents::FlowContent>::parseChild(self);
                self.m_insideOfTagA = false;
                return result;
            }
        };

        struct AWithPhrasingContent : ContainsPhrasingContentTag<HTMLAnchorElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "a";

            static RefPtr<Element> parseChild(HTMLFastPathParser& self)
            {
                ASSERT(!self.m_insideOfTagA);
                self.m_insideOfTagA = true;
                auto result = ContainsPhrasingContentTag<HTMLAnchorElement, PermittedParents::PhrasingOrFlowContent>::parseChild(self);
                self.m_insideOfTagA = false;
                return result;
            }
        };

        struct B : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "b";

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::bTag, document);
            }
        };

        struct Br : VoidTag<HTMLBRElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "br";
        };

        struct Button : ContainsPhrasingContentTag<HTMLButtonElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "button";
        };

        struct Div : ContainerTag<HTMLDivElement, PermittedParents::FlowContent> {
            static constexpr const char tagName[] = "div";
        };

        struct Footer : ContainerTag<HTMLDivElement, PermittedParents::FlowContent> {
            static constexpr const char tagName[] = "footer";

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::footerTag, document);
            }
        };

        struct I : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "i";

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::iTag, document);
            }
        };

        struct Input : VoidTag<HTMLInputElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "input";

            static Ref<HTMLInputElement> create(Document& document)
            {
                return HTMLInputElement::create(HTMLNames::inputTag, document, /* form */ nullptr, /* createdByParser */ true);
            }
        };

        struct Li : ContainerTag<HTMLLIElement, PermittedParents::Special> {
            static constexpr const char tagName[] = "li";
        };

        struct Label : ContainsPhrasingContentTag<HTMLLabelElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "label";
        };

        struct Option : ContainerTag<HTMLOptionElement, PermittedParents::Special> {
            static constexpr const char tagName[] = "option";

            static RefPtr<Element> parseChild(HTMLFastPathParser& self)
            {
                // <option> can only contain a text content.
                return self.didFail(HTMLFastPathResult::FailedOptionWithChild, nullptr);
            }
        };

        struct Ol : ContainerTag<HTMLOListElement, PermittedParents::FlowContent> {
            static constexpr const char tagName[] = "ol";

            static RefPtr<Element> parseChild(HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Li>();
            }
        };

        struct P : ContainsPhrasingContentTag<HTMLParagraphElement, PermittedParents::FlowContent> {
            static constexpr const char tagName[] = "p";
        };

        struct Select : ContainerTag<HTMLSelectElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "select";

            static RefPtr<Element> parseChild(HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Option>();
            }
        };

        struct Span : ContainsPhrasingContentTag<HTMLSpanElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "span";
        };

        struct Strong : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr const char tagName[] = "strong";

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::strongTag, document);
            }
        };

        struct Ul : ContainerTag<HTMLUListElement, PermittedParents::FlowContent> {
            static constexpr const char tagName[] = "ul";

            static RefPtr<Element> parseChild(HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Li>();
            }
        };
    };

    template<class ParentTag> void parseCompleteInput()
    {
        parseChildren<ParentTag>(m_fragment);
        if (m_position != m_end)
            didFail(HTMLFastPathResult::FailedDidntReachEndOfInput);
    }

    bool isValidUnquotedAttributeValueChar(Char c)
    {
        // FIXME: We should probably levegate isASCIIAlphanumeric() here.
        return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '_' || c == '-';
    }

    // https://html.spec.whatwg.org/#syntax-attribute-name
    bool isValidAttributeNameChar(Char c)
    {
        if (c == '=') // Early return for the most common way to end an attribute.
            return false;
        // FIXME: We should probably levegate isASCIIAlphanumeric() here.
        return ('a' <= c && c <= 'z') || c == '-' || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9');
    }

    bool isCharAfterTagNameOrAttribute(Char c)
    {
        return c == ' ' || c == '>' || isHTMLSpace(c) || c == '/';
    }

    bool isCharAfterUnquotedAttribute(Char c)
    {
        return c == ' ' || c == '>' || isHTMLSpace(c);
    }

    void skipWhitespace()
    {
        while (m_position != m_end && isHTMLSpace(*m_position))
            ++m_position;
    }

    // We first try to scan text as an unmodified subsequence of the input.
    // However, if there are escape sequences, we have to copy the text to a
    // separate buffer and we might go outside of `Char` range if we are in an
    // `LChar` parser. Therefore, this function returns either a `Span` or a
    // `UCharSpan`. Callers distinguish the two cases by checking if the `Span` is
    // empty, as only one of them can be non-empty.
    std::pair<CharSpan, UCharSpan> scanText()
    {
        const Char* start = m_position;
        while (m_position != m_end && *m_position != '<') {
            // '&' indicates escape sequences, '\r' might require
            // https://infra.spec.whatwg.org/#normalize-newlines
            if (*m_position == '&' || *m_position == '\r') {
                m_position = start;
                return { CharSpan { }, scanEscapedText() };
            }
            if (UNLIKELY(*m_position == '\0'))
                return didFail(HTMLFastPathResult::FailedContainsNull, std::pair { CharSpan { }, UCharSpan { } } );

            ++m_position;
        }
        return { { start, static_cast<size_t>(m_position - start) }, UCharSpan { } };
    }

    // Slow-path of `scanText()`, which supports escape sequences by copying to a
    // separate buffer.
    UCharSpan scanEscapedText()
    {
        m_ucharBuffer.resize(0);
        while (m_position != m_end && *m_position != '<') {
            if (*m_position == '&') {
                scanHTMLCharacterReference(m_ucharBuffer);
                if (m_parsingFailed)
                    return UCharSpan { };
            } else if (*m_position == '\r') {
                // Normalize "\r\n" to "\n" according to https://infra.spec.whatwg.org/#normalize-newlines.
                if (m_position + 1 != m_end && m_position[1] == '\n')
                    ++m_position;
                m_ucharBuffer.append('\n');
                ++m_position;
            } else if (UNLIKELY(*m_position == '\0'))
                return didFail(HTMLFastPathResult::FailedContainsNull, UCharSpan { });
            else {
                m_ucharBuffer.append(*m_position);
                ++m_position;
            }
        }
        return { m_ucharBuffer.data(), m_ucharBuffer.size() };
    }

    // Scan a tagName and convert to lowercase if necessary.
    CharSpan scanTagName()
    {
        const Char* start = m_position;
        while (m_position != m_end && 'a' <= *m_position && *m_position <= 'z')
            ++m_position;

        if (m_position == m_end || !isCharAfterTagNameOrAttribute(*m_position)) {
            // Try parsing a case-insensitive tagName.
            m_charBuffer.resize(0);
            m_position = start;
            while (m_position != m_end) {
                Char c = *m_position;
                if ('A' <= c && c <= 'Z')
                    c = c - ('A' - 'a');
                else if (!('a' <= c && c <= 'z'))
                    break;
                ++m_position;
                m_charBuffer.append(c);
            }
            if (m_position == m_end || !isCharAfterTagNameOrAttribute(*m_position))
                return didFail(HTMLFastPathResult::FailedParsingTagName, CharSpan { });
            skipWhitespace();
            return CharSpan { m_charBuffer.data(), m_charBuffer.size() };
        }
        CharSpan result { start, static_cast<size_t>(m_position - start) };
        skipWhitespace();
        return result;
    }

    CharSpan scanAttributeName()
    {
        // First look for all lower case. This path doesn't require any mapping of
        // input. This path could handle other valid attribute name chars, but they
        // are not as common, so it only looks for lowercase.
        const Char* start = m_position;
        while (m_position != m_end && *m_position >= 'a' && *m_position <= 'z')
            ++m_position;
        if (UNLIKELY(m_position == m_end))
            return didFail(HTMLFastPathResult::FailedEndOfInputReached, CharSpan());
        if (!isValidAttributeNameChar(*m_position))
            return CharSpan { start, static_cast<size_t>(m_position - start) };

        // At this point name does not contain lowercase. It may contain upper-case,
        // which requires mapping. Assume it does.
        m_position = start;
        m_attributeNameBuffer.resize(0);
        // isValidAttributeNameChar() returns false if end of input is reached.
        for (Char c = peekNext(); isValidAttributeNameChar(c); c = peekNext()) {
            if ('A' <= c && c <= 'Z')
                c = c - ('A' - 'a');
            m_attributeNameBuffer.append(c);
            ++m_position;
        }
        return CharSpan { m_attributeNameBuffer.data(), static_cast<size_t>(m_attributeNameBuffer.size()) };
    }

    std::pair<CharSpan, UCharSpan> scanAttributeValue()
    {
        CharSpan result;
        skipWhitespace();
        const Char* start = m_position;
        if (Char quoteChar = peekNext(); quoteChar == '"' || quoteChar == '\'') {
            start = ++m_position;
            while (m_position != m_end && peekNext() != quoteChar) {
                if (peekNext() == '&' || peekNext() == '\r') {
                    m_position = start - 1;
                    return { CharSpan { }, scanEscapedAttributeValue() };
                }
                ++m_position;
            }
            if (m_position == m_end)
                return didFail(HTMLFastPathResult::FailedParsingQuotedAttributeValue, std::pair { CharSpan { }, UCharSpan { } });

            result = CharSpan { start, static_cast<size_t>(m_position - start) };
            if (consumeNext() != quoteChar)
                return didFail(HTMLFastPathResult::FailedParsingQuotedAttributeValue, std::pair { CharSpan { }, UCharSpan { } });
        } else {
            while (isValidUnquotedAttributeValueChar(peekNext()))
                ++m_position;
            result = CharSpan { start, static_cast<size_t>(m_position - start) };
            if (!isCharAfterUnquotedAttribute(peekNext()))
                return didFail(HTMLFastPathResult::FailedParsingUnquotedAttributeValue, std::pair { CharSpan { }, UCharSpan { } });
        }
        return { result, UCharSpan { } };
    }

    // Slow path for scanning an attribute value. Used for special cases such
    // as '&' and '\r'.
    UCharSpan scanEscapedAttributeValue()
    {
        CharSpan result;
        skipWhitespace();
        m_ucharBuffer.resize(0);
        const Char* start = m_position;
        if (Char quoteChar = peekNext(); quoteChar == '"' || quoteChar == '\'') {
            start = ++m_position;
            while (m_position != m_end && peekNext() != quoteChar) {
                if (m_parsingFailed)
                    return UCharSpan { };
                if (peekNext() == '&')
                    scanHTMLCharacterReference(m_ucharBuffer);
                else if (peekNext() == '\r') {
                    // Normalize "\r\n" to "\n" according to https://infra.spec.whatwg.org/#normalize-newlines.
                    if (m_position + 1 != m_end && m_position[1] == '\n')
                        ++m_position;
                    m_ucharBuffer.append('\n');
                    ++m_position;
                } else {
                    m_ucharBuffer.append(*m_position);
                    ++m_position;
                }
            }
            if (m_position == m_end)
                return didFail(HTMLFastPathResult::FailedParsingQuotedEscapedAttributeValue, UCharSpan { });

            result = CharSpan { start, static_cast<size_t>(m_position - start) };
            if (consumeNext() != quoteChar)
                return didFail( HTMLFastPathResult::FailedParsingQuotedEscapedAttributeValue, UCharSpan { });
        } else
            return didFail(HTMLFastPathResult::FailedParsingUnquotedEscapedAttributeValue, UCharSpan { });
        return UCharSpan { m_ucharBuffer.data(), m_ucharBuffer.size() };
    }

    void scanHTMLCharacterReference(Vector<UChar>& out)
    {
        ASSERT(*m_position == '&');
        ++m_position;
        const Char* start = m_position;
        while (true) {
            // A rather arbitrary constant to prevent unbounded lookahead in the case of ill-formed input.
            constexpr int maxLength = 20;
            if (m_position == m_end || m_position - start > maxLength || UNLIKELY(*m_position == '\0'))
                return didFail(HTMLFastPathResult::FailedParsingCharacterReference);
            if (consumeNext() == ';')
                break;
        }

        CharSpan reference = CharSpan { start, static_cast<size_t>(m_position - start) - 1 };
        // There are no valid character references shorter than that. The check protects the indexed accesses below.
        constexpr size_t minLength = 2;
        if (reference.size() < minLength)
            return didFail(HTMLFastPathResult::FailedParsingCharacterReference);

        if (reference[0] == '#') {
            UChar32 result = 0;
            if (reference[1] == 'x' || reference[1] == 'X') {
                for (size_t i = 2; i < reference.size(); ++i) {
                    Char c = reference[i];
                    result *= 16;
                    if (c >= '0' && c <= '9')
                        result += c - '0';
                    else if (c >= 'a' && c <= 'f')
                        result += c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        result += c - 'A' + 10;
                    else
                        return didFail(HTMLFastPathResult::FailedParsingCharacterReference);

                    if (result > UCHAR_MAX_VALUE)
                        return didFail(HTMLFastPathResult::FailedParsingCharacterReference);
                }
            } else {
                for (size_t i = 1; i < reference.size(); ++i) {
                    Char c = reference[i];
                    result *= 10;
                    if (c >= '0' && c <= '9')
                        result += c - '0';
                    else
                        return didFail(HTMLFastPathResult::FailedParsingCharacterReference);

                    if (result > UCHAR_MAX_VALUE)
                        return didFail(HTMLFastPathResult::FailedParsingCharacterReference);
                }
            }
            appendLegalEntityFor(result, out);
            // Handle the most common named references.
        } else if (reference == "amp")
            out.append('&');
        else if (reference == "lt")
            out.append('<');
        else if (reference == "gt")
            out.append('>');
        else if (reference == "nbsp")
            out.append(0xa0);
        else {
            // This handles uncommon named references.
            String inputString { reference.data(), static_cast<unsigned>(reference.size()) };
            SegmentedString inputSegmented { inputString };
            StringBuilder entity;
            bool notEnoughCharacters = false;
            if (!consumeHTMLEntity(inputSegmented, entity, notEnoughCharacters) || notEnoughCharacters)
                return didFail(HTMLFastPathResult::FailedParsingCharacterReference);

            for (unsigned i = 0; i < entity.length(); ++i)
                out.append(entity[i]);
        }
    }

    void didFail(HTMLFastPathResult result)
    {
        // This function may be called multiple times. Only record the result the first time it's called.
        if (m_parsingFailed)
            return;

        m_parseResult = result;
        m_parsingFailed = true;
    }

    template<class ReturnValueType> ReturnValueType didFail(HTMLFastPathResult result, ReturnValueType returnValue)
    {
        didFail(result);
        return returnValue;
    }

    Char peekNext()
    {
        ASSERT(m_position <= m_end);
        if (m_position == m_end) {
            didFail(HTMLFastPathResult::FailedEndOfInputReached);
            return '\0';
        }
        return *m_position;
    }

    Char consumeNext()
    {
        if (m_position == m_end)
            return didFail(HTMLFastPathResult::FailedEndOfInputReached, '\0');
        return *(m_position++);
    }

    template<class ParentTag> void parseChildren(ContainerNode& parent)
    {
        while (true) {
            std::pair<CharSpan, UCharSpan> text = scanText();
            if (m_parsingFailed)
                return;

            ASSERT(text.first.empty() || text.second.empty());
            if (!text.first.empty()) {
                if (text.first.size() >= Text::defaultLengthLimit)
                    return didFail(HTMLFastPathResult::FailedBigText);
                parent.parserAppendChild(Text::create(m_document, String(text.first.data(), static_cast<unsigned>(text.first.size()))));
            } else if (!text.second.empty()) {
                if (text.second.size() >= Text::defaultLengthLimit)
                    return didFail(HTMLFastPathResult::FailedBigText);
                parent.parserAppendChild(Text::create(m_document, String(text.second.data(), static_cast<unsigned>(text.second.size()))));
            }
            if (m_position == m_end)
                return;
            ASSERT(*m_position == '<');
            ++m_position;
            if (peekNext() == '/') {
                // We assume that we found the closing tag. The tagName will be checked by the caller `parseContainerElement()`.
                return;
            }
            if (++m_elementDepth == Settings::defaultMaximumHTMLParserDOMTreeDepth)
                return didFail(HTMLFastPathResult::FailedMaxDepth);
            auto child = ParentTag::parseChild(*this);
            --m_elementDepth;
            if (m_parsingFailed)
                return;
            ASSERT(child);
            parent.parserAppendChild(*child);
        }
    }

    Attribute processAttribute(CharSpan nameSpan, std::pair<CharSpan, UCharSpan> valueSpan)
    {
        QualifiedName name = HTMLNameCache::makeAttributeQualifiedName(nameSpan);

        // The string pointer in |value| is null for attributes with no values, but
        // the null atom is used to represent absence of attributes; attributes with
        // no values have the value set to an empty atom instead.
        AtomString value;
        if (valueSpan.second.empty())
            value = AtomString(valueSpan.first.data(), static_cast<unsigned>(valueSpan.first.size()));
        else
            value = AtomString(valueSpan.second.data(), static_cast<unsigned>(valueSpan.second.size()));
        if (value.isNull())
            value = emptyAtom();
        return Attribute { WTFMove(name), WTFMove(value) };
    }

    void parseAttributes(Element& parent)
    {
        ASSERT(m_attributeBuffer.isEmpty());
        ASSERT(m_attributeNames.isEmpty());
        while (true) {
            CharSpan attributeName = scanAttributeName();
            if (attributeName.empty()) {
                if (peekNext() == '>') {
                    ++m_position;
                    break;
                }
                if (peekNext() == '/') {
                    ++m_position;
                    skipWhitespace();
                    if (consumeNext() != '>')
                        return didFail(HTMLFastPathResult::FailedParsingAttributes);
                    break;
                }
                return didFail(HTMLFastPathResult::FailedParsingAttributes);
            }
            if (attributeName.size() >= 2 && attributeName[0] == 'o' && attributeName[1] == 'n') {
                // These attributes likely contain script that may be executed at random
                // points, which could cause problems if parsing via the fast path
                // fails. For example, an image's onload event.
                return didFail(HTMLFastPathResult::FailedOnAttribute);
            }
            skipWhitespace();
            std::pair<CharSpan, UCharSpan> attributeValue;
            if (peekNext() == '=') {
                ++m_position;
                attributeValue = scanAttributeValue();
                skipWhitespace();
            }
            Attribute attribute = processAttribute(attributeName, attributeValue);
            m_attributeBuffer.append(attribute);
            if (attribute.name() == HTMLNames::isAttr)
                return didFail(HTMLFastPathResult::FailedParsingAttributes);
            m_attributeNames.append(attribute.localName().impl());
        }
        // FIXME: Consider using a HashSet instead of std::sort + std::adjacent_find.
        std::sort(m_attributeNames.begin(), m_attributeNames.end());
        if (std::adjacent_find(m_attributeNames.begin(), m_attributeNames.end()) != m_attributeNames.end()) {
            // Found duplicate attributes. We would have to ignore repeated attributes, but leave this to the general parser instead.
            return didFail(HTMLFastPathResult::FailedParsingAttributes);
        }
        parent.parserSetAttributes(m_attributeBuffer);
        m_attributeBuffer.clear();
        m_attributeNames.resize(0);
    }

    template<class... Tags> RefPtr<Element> parseSpecificElements()
    {
        CharSpan tagName = scanTagName();
        return parseSpecificElements<Tags...>(tagName);
    }

    template<void* = nullptr> RefPtr<Element> parseSpecificElements(CharSpan)
    {
        return didFail(HTMLFastPathResult::FailedParsingSpecificElements, nullptr);
    }

    template<class Tag, class... OtherTags> RefPtr<Element> parseSpecificElements(CharSpan tagName)
    {
        if (tagName == Tag::tagName)
            return parseElementAfterTagName<Tag>();
        return parseSpecificElements<OtherTags...>(tagName);
    }

    template<bool nonPhrasingContent> RefPtr<Element> parseElement()
    {
        CharSpan tagName = scanTagName();
        if (tagName.empty())
            return didFail(HTMLFastPathResult::FailedParsingElement, nullptr);

        // HTML has complicated rules around auto-closing tags and re-parenting
        // DOM nodes. We avoid complications with auto-closing rules by disallowing
        // certain nesting. In particular, we bail out if non-phrasing-content
        // elements are nested into elements that require phrasing content.
        // Similarly, we disallow nesting <a> tags. But tables for example have
        // complex re-parenting rules that cannot be captured in this way, so we
        // cannot support them.
        //
        // If this switch has duplicate cases, then `tagNameHash()` needs to be
        // updated.
        switch (tagNameHash(tagName)) {
#define TAG_CASE(TagName, TagClassName)                                                  \
        case tagNameHash(TagInfo::TagClassName::tagName):                                \
            if (std::is_same_v<typename TagInfo::A, typename TagInfo::TagClassName>)     \
                goto caseA;                                                             \
            if constexpr (nonPhrasingContent ? TagInfo::TagClassName::allowedInFlowContent() : TagInfo::TagClassName::allowedInPhrasingOrFlowContent()) { \
                /* See comment in parse() for details on why equality is checked here */ \
                if (tagName == TagInfo::TagClassName::tagName)                           \
                    return parseElementAfterTagName<typename TagInfo::TagClassName>();   \
            }                                                                            \
            break;

        FOR_EACH_SUPPORTED_TAG(TAG_CASE)
#undef TAG_CASE

            caseA:
            // <a> tags must not be nested, because HTML parsing would auto-close
            // the outer one when encountering a nested one.
            if (tagName == TagInfo::A::tagName && !m_insideOfTagA) {
                return nonPhrasingContent
                    ? parseElementAfterTagName<typename TagInfo::A>()
                    : parseElementAfterTagName<typename TagInfo::AWithPhrasingContent>();
            }
            break;
        default:
            break;
        }
        return didFail(HTMLFastPathResult::FailedUnsupportedTag, nullptr);
    }

    template<class Tag> Ref<Element> parseElementAfterTagName()
    {
        if constexpr (Tag::isVoid)
            return parseVoidElement(Tag::create(m_document));
        else
            return parseContainerElement<Tag>(Tag::create(m_document));
    }

    template<class Tag> Ref<Element> parseContainerElement(Ref<Element>&& element)
    {
        parseAttributes(element);
        if (m_parsingFailed)
            return WTFMove(element);
        parseChildren<Tag>(element);
        if (m_parsingFailed || m_position == m_end)
            return didFail(HTMLFastPathResult::FailedEndOfInputReachedForContainer, element);

        // parseChildren<Tag>(element) stops after the (hopefully) closing tag's `<`
        // and fails if the the current char is not '/'.
        ASSERT(*m_position == '/');
        ++m_position;
        CharSpan endtag = scanTagName();
        if (endtag == Tag::tagName) {
            if (consumeNext() != '>')
                return didFail(HTMLFastPathResult::FailedUnexpectedTagNameCloseState, element);
        } else
            return didFail(HTMLFastPathResult::FailedEndTagNameMismatch, element);
        return WTFMove(element);
    }

    Ref<Element> parseVoidElement(Ref<Element>&& element)
    {
        parseAttributes(element);
        return WTFMove(element);
    }
};

static bool canUseFastPath(Document& document, Element& contextElement, OptionSet<ParserContentPolicy> policy)
{
    // We could probably allow other content policies too, as we do not support scripts or plugins anyway.
    if (!policy.contains(ParserContentPolicy::AllowScriptingContent))
        return false;

    // If we are within a form element, we would need to create associations, which we do not. Therefore, we do not
    // support this case. See HTMLConstructionSite::initFragmentParsing() and HTMLConstructionSite::createElement()
    // for the corresponding code on the slow-path.
    if (!contextElement.document().isTemplateDocument() && lineageOfType<HTMLFormElement>(contextElement).first())
        return false;

    // State used for this is updated in BeginParsingChildren() and FinishParsingChildren(), which this does not call.
    if (document.isDirAttributeDirty() && document.settings().dirPseudoEnabled())
        return false;

    return true;
}

template<class Char>
static bool tryFastParsingHTMLFragmentImpl(const Span<const Char>& source, Document& document, Ref<DocumentFragment>& fragment, Element& contextElement)
{
    HTMLFastPathParser<Char> parser { source, document, fragment };
    bool success = parser.parse(contextElement);
    // The direction attribute may change as a result of parsing. Check again.
    if (document.isDirAttributeDirty() && document.settings().dirPseudoEnabled())
        success = false;
    if (!success)
        fragment = DocumentFragment::create(document);
    return success;
}

bool tryFastParsingHTMLFragment(const String& source, Document& document, Ref<DocumentFragment>& fragment, Element& contextElement, OptionSet<ParserContentPolicy> policy)
{
    if (!canUseFastPath(document, contextElement, policy))
        return false;

    if (source.is8Bit())
        return tryFastParsingHTMLFragmentImpl<LChar>(source.span8(), document, fragment, contextElement);
    return tryFastParsingHTMLFragmentImpl<UChar>(source.span16(), document, fragment, contextElement);
}

#undef FOR_EACH_SUPPORTED_TAG

} // namespace WebCore
