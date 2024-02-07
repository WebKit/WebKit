/*
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2009-2022 Google Inc. All rights reserved.
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
#include "MarkupAccumulator.h"

#include "Attr.h"
#include "CDATASection.h"
#include "Comment.h"
#include "CommonAtomStrings.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "FrameLoader.h"
#include "HTMLElement.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "HTMLTemplateElement.h"
#include "NodeName.h"
#include "ProcessingInstruction.h"
#include "ScriptController.h"
#include "ShadowRoot.h"
#include "TemplateContentDocumentFragment.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include <memory>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

struct EntityDescription {
    const char* characters;
    uint8_t length;
    std::optional<EntityMask> mask;
};

static const EntityDescription entitySubstitutionList[] = {
    { "", 0, std::nullopt },
    { "&amp;", 5, EntityMask::Amp },
    { "&lt;", 4, EntityMask::Lt },
    { "&gt;", 4, EntityMask::Gt },
    { "&quot;", 6, EntityMask::Quot },
    { "&nbsp;", 6, EntityMask::Nbsp },
    { "&#9;", 4, EntityMask::Tab },
    { "&#10;", 5, EntityMask::LineFeed },
    { "&#13;", 5, EntityMask::CarriageReturn },
};

namespace EntitySubstitutionIndex {
constexpr uint8_t Null = 0;
constexpr uint8_t Amp = 1;
constexpr uint8_t Lt = 2;
constexpr uint8_t Gt = 3;
constexpr uint8_t Quot = 4;
constexpr uint8_t Nbsp = 5;
constexpr uint8_t Tab = 6;
constexpr uint8_t LineFeed = 7;
constexpr uint8_t CarriageReturn = 8;
};

static const unsigned maximumEscapedentityCharacter = noBreakSpace;
static const uint8_t entityMap[maximumEscapedentityCharacter + 1] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    EntitySubstitutionIndex::Tab, // '\t'.
    EntitySubstitutionIndex::LineFeed, // '\n'.
    0, 0,
    EntitySubstitutionIndex::CarriageReturn, // '\r'.
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    EntitySubstitutionIndex::Quot, // '"'.
    0, 0, 0,
    EntitySubstitutionIndex::Amp, // '&'.
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    EntitySubstitutionIndex::Lt, // '<'.
    0,
    EntitySubstitutionIndex::Gt, // '>'.
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    EntitySubstitutionIndex::Nbsp // noBreakSpace.
};

static bool elementCannotHaveEndTag(const Node& node)
{
    using namespace ElementNames;
    RefPtr element = dynamicDowncast<Element>(node);
    if (!element)
        return false;

    switch (element->elementName()) {
        // https://html.spec.whatwg.org/#void-elements
    case HTML::area:
    case HTML::base:
    case HTML::br:
    case HTML::col:
    case HTML::embed:
    case HTML::hr:
    case HTML::img:
    case HTML::input:
    case HTML::link:
    case HTML::meta:
    case HTML::source:
    case HTML::track:
    case HTML::wbr:
        // https://html.spec.whatwg.org/#serializes-as-void
    case HTML::basefont:
    case HTML::bgsound:
    case HTML::frame:
    case HTML::keygen:
    case HTML::param:
        return true;
    default:
        break;
    }

    return false;
}

// Rules of self-closure
// 1. No elements in HTML documents use the self-closing syntax.
// 2. Elements w/ children never self-close because they use a separate end tag.
// 3. HTML elements which do not have a "forbidden" end tag will close with a separate end tag.
// 4. Other elements self-close.
static bool shouldSelfClose(const Element& element, SerializationSyntax syntax)
{
    if (syntax != SerializationSyntax::XML)
        return false;
    if (element.hasChildNodes())
        return false;
    if (element.isHTMLElement() && !elementCannotHaveEndTag(element))
        return false;
    return true;
}

template<typename CharacterType>
static inline void appendCharactersReplacingEntitiesInternal(StringBuilder& result, const String& source, unsigned offset, unsigned length, OptionSet<EntityMask> entityMask)
{
    const CharacterType* text = source.characters<CharacterType>() + offset;

    size_t positionAfterLastEntity = 0;
    for (size_t i = 0; i < length; ++i) {
        CharacterType character = text[i];
        uint8_t substitution = character < std::size(entityMap) ? entityMap[character] : static_cast<uint8_t>(EntitySubstitutionIndex::Null);
        if (UNLIKELY(substitution != EntitySubstitutionIndex::Null) && entityMask.contains(*entitySubstitutionList[substitution].mask)) {
            result.appendSubstring(source, offset + positionAfterLastEntity, i - positionAfterLastEntity);
            result.appendCharacters(entitySubstitutionList[substitution].characters, entitySubstitutionList[substitution].length);
            positionAfterLastEntity = i + 1;
        }
    }
    result.appendSubstring(source, offset + positionAfterLastEntity, length - positionAfterLastEntity);
}

void MarkupAccumulator::appendCharactersReplacingEntities(StringBuilder& result, const String& source, unsigned offset, unsigned length, OptionSet<EntityMask> entityMask)
{
    ASSERT(offset + length <= source.length());

    if (!length)
        return;

    if (source.is8Bit())
        appendCharactersReplacingEntitiesInternal<LChar>(result, source, offset, length, entityMask);
    else
        appendCharactersReplacingEntitiesInternal<UChar>(result, source, offset, length, entityMask);
}

MarkupAccumulator::MarkupAccumulator(Vector<Ref<Node>>* nodes, ResolveURLs resolveURLs, SerializationSyntax serializationSyntax, HashMap<String, String>&& replacementURLStrings, HashMap<RefPtr<CSSStyleSheet>, String>&& replacementURLStringsForCSSStyleSheet, ShouldIncludeShadowDOM shouldIncludeShadowDOM, const Vector<MarkupExclusionRule>& exclusionRules)
    : m_nodes(nodes)
    , m_resolveURLs(resolveURLs)
    , m_serializationSyntax(serializationSyntax)
    , m_replacementURLStrings(WTFMove(replacementURLStrings))
    , m_replacementURLStringsForCSSStyleSheet(WTFMove(replacementURLStringsForCSSStyleSheet))
    , m_shouldIncludeShadowDOM(shouldIncludeShadowDOM == ShouldIncludeShadowDOM::Yes)
    , m_exclusionRules(exclusionRules)
{
}

MarkupAccumulator::~MarkupAccumulator() = default;

String MarkupAccumulator::serializeNodes(Node& targetNode, SerializedNodes root)
{
    serializeNodesWithNamespaces(targetNode, root, 0);
    return m_markup.toString();
}

bool MarkupAccumulator::appendContentsForNode(StringBuilder& result, const Node& targetNode)
{
    RefPtr styleElement = dynamicDowncast<HTMLStyleElement>(targetNode);
    if (!styleElement)
        return false;

    if (m_replacementURLStrings.isEmpty() && m_replacementURLStringsForCSSStyleSheet.isEmpty())
        return false;

    RefPtr cssStyleSheet = styleElement->sheet();
    if (!cssStyleSheet)
        return false;

    result.append(cssStyleSheet->cssTextWithReplacementURLs(m_replacementURLStrings, m_replacementURLStringsForCSSStyleSheet));
    return true;
}

void MarkupAccumulator::serializeNodesWithNamespaces(Node& targetNode, SerializedNodes root, const Namespaces* namespaces)
{
    WTF::Vector<Namespaces> namespaceStack;
    if (namespaces)
        namespaceStack.append(*namespaces);
    else if (inXMLFragmentSerialization()) {
        // Make sure xml prefix and namespace are always known to uphold the constraints listed at http://www.w3.org/TR/xml-names11/#xmlReserved.
        Namespaces namespaceHash;
        namespaceHash.set(xmlAtom().impl(), XMLNames::xmlNamespaceURI->impl());
        namespaceHash.set(XMLNames::xmlNamespaceURI->impl(), xmlAtom().impl());
        namespaceStack.append(WTFMove(namespaceHash));
    } else
        namespaceStack.constructAndAppend();

    RefPtr<const Node> current = &targetNode;
    do {
        bool shouldSkipNode = false;
        if (RefPtr element = dynamicDowncast<const Element>(current); element && shouldExcludeElement(*element))
            shouldSkipNode = true;

        bool shouldAppendNode = !shouldSkipNode && !(current == &targetNode && root != SerializedNodes::SubtreeIncludingNode);
        if (shouldAppendNode)
            startAppendingNode(*current, &namespaceStack.last());

        bool shouldEmitCloseTag = !(targetNode.document().isHTMLDocument() && elementCannotHaveEndTag(*current));
        shouldSkipNode = shouldSkipNode || !shouldEmitCloseTag;
        if (!shouldSkipNode) {
            if (m_shouldIncludeShadowDOM) {
                RefPtr shadowRoot = current->shadowRoot();
                if (shadowRoot && shadowRoot->mode() != ShadowRootMode::UserAgent) {
                    current = shadowRoot;
                    namespaceStack.append(namespaceStack.last());
                    continue;
                }
            }

            bool shouldSkipChidren = appendContentsForNode(m_markup, *current);
            RefPtr currentTemplate = dynamicDowncast<HTMLTemplateElement>(*current);
            auto firstChild = currentTemplate ? currentTemplate->content().firstChild() : current->firstChild();
            if (!shouldSkipChidren && firstChild) {
                current = firstChild;
                namespaceStack.append(namespaceStack.last());
                continue;
            }
        }

        if (shouldAppendNode && shouldEmitCloseTag)
            endAppendingNode(*current);

        while (current != &targetNode) {
            if (RefPtr nextSibling = current->nextSibling()) {
                current = WTFMove(nextSibling);
                namespaceStack.removeLast();
                namespaceStack.append(namespaceStack.last());
                break;
            }

            if (m_shouldIncludeShadowDOM && current->isShadowRoot())
                current = current->shadowHost();
            else
                current = current->parentNode();

            namespaceStack.removeLast();
            if (RefPtr fragment = dynamicDowncast<TemplateContentDocumentFragment>(current.get())) {
                if (current != &targetNode)
                    current = fragment->host();
            }

            ASSERT(current);
            if (!current)
                break;

            shouldAppendNode = !(current == &targetNode && root != SerializedNodes::SubtreeIncludingNode);
            shouldEmitCloseTag = !(targetNode.document().isHTMLDocument() && elementCannotHaveEndTag(*current));
            if (shouldAppendNode && shouldEmitCloseTag)
                endAppendingNode(*current);
        }
    } while (current != &targetNode);
}

String MarkupAccumulator::resolveURLIfNeeded(const Element& element, const String& urlString) const
{
    if (RefPtr link = dynamicDowncast<HTMLLinkElement>(element); link && !m_replacementURLStringsForCSSStyleSheet.isEmpty()) {
        if (RefPtr cssStyleSheet = link->sheet()) {
            auto replacementURLString = m_replacementURLStringsForCSSStyleSheet.get(cssStyleSheet);
            if (!replacementURLString.isEmpty())
                return replacementURLString;
        }
    }

    if (!m_replacementURLStrings.isEmpty()) {
        if (auto frame = frameForAttributeReplacement(element)) {
            auto replacementURLString = m_replacementURLStrings.get(frame->frameID().toString());
            if (!replacementURLString.isEmpty())
                return replacementURLString;
        }

        auto resolvedURLString = element.resolveURLStringIfNeeded(urlString);
        auto replacementURLString = m_replacementURLStrings.get(resolvedURLString);
        if (!replacementURLString.isEmpty())
            return replacementURLString;
    }

    return element.resolveURLStringIfNeeded(urlString, m_resolveURLs);
}

RefPtr<Element> MarkupAccumulator::replacementElement(const Node& node)
{
    if (!m_shouldIncludeShadowDOM)
        return nullptr;

    RefPtr shadowRoot = dynamicDowncast<ShadowRoot>(node);
    if (!shadowRoot)
        return nullptr;

    if (shadowRoot->mode() == ShadowRootMode::UserAgent)
        return nullptr;

    auto element = HTMLTemplateElement::create(HTMLNames::templateTag, node.document());
    if (shadowRoot->mode() == ShadowRootMode::Open)
        element->setShadowRootMode(AtomString { "open"_s });
    else if (shadowRoot->mode() == ShadowRootMode::Closed)
        element->setShadowRootMode(AtomString { "closed"_s });

    return element;
}

void MarkupAccumulator::startAppendingNode(const Node& node, Namespaces* namespaces)
{
    if (RefPtr element = dynamicDowncast<Element>(node))
        appendStartTag(m_markup, *element, namespaces);
    else if (RefPtr element = replacementElement(node))
        appendStartTag(m_markup, *element, namespaces);
    else
        appendNonElementNode(m_markup, node, namespaces);

    if (m_nodes)
        m_nodes->append(const_cast<Node&>(node));
}

void MarkupAccumulator::appendEndTag(StringBuilder& result, const Element& element)
{
    if (shouldSelfClose(element, m_serializationSyntax) || (!element.hasChildNodes() && elementCannotHaveEndTag(element)))
        return;
    result.append("</", element.tagQName().toString(), '>');
}

StringBuilder MarkupAccumulator::takeMarkup()
{
    return std::exchange(m_markup, { });
}

void MarkupAccumulator::appendAttributeValue(StringBuilder& result, const String& attribute, bool isSerializingHTML)
{
    appendCharactersReplacingEntities(result, attribute, 0, attribute.length(),
        isSerializingHTML ? EntityMaskInHTMLAttributeValue : EntityMaskInAttributeValue);
}

void MarkupAccumulator::appendCustomAttributes(StringBuilder&, const Element&, Namespaces*)
{
}

static bool shouldAddNamespaceElement(const Element& element)
{
    // Don't add namespace attribute if it is already defined for this elem.
    auto& prefix = element.prefix();
    if (prefix.isEmpty())
        return !element.hasAttribute(xmlnsAtom());
    return !element.hasAttribute(makeAtomString("xmlns:"_s, prefix));
}

static bool shouldAddNamespaceAttribute(const Attribute& attribute, Namespaces& namespaces)
{
    namespaces.checkConsistency();

    // Don't add namespace attributes twice
    // HTML Parser will create xmlns attributes without namespace for HTML elements, allow those as well.
    if (attribute.name().localName() == xmlnsAtom() && (attribute.namespaceURI().isEmpty() || attribute.namespaceURI() == XMLNSNames::xmlnsNamespaceURI)) {
        namespaces.set(emptyAtom().impl(), attribute.value().impl());
        return false;
    }

    QualifiedName xmlnsPrefixAttr(xmlnsAtom(), attribute.localName(), XMLNSNames::xmlnsNamespaceURI);
    if (attribute.name() == xmlnsPrefixAttr) {
        namespaces.set(attribute.localName().impl(), attribute.value().impl());
        namespaces.set(attribute.value().impl(), attribute.localName().impl());
        return false;
    }

    return true;
}

void MarkupAccumulator::appendNamespace(StringBuilder& result, const AtomString& prefix, const AtomString& namespaceURI, Namespaces& namespaces, bool allowEmptyDefaultNS)
{
    namespaces.checkConsistency();
    if (namespaceURI.isEmpty()) {
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-xhtml-syntax.html#xml-fragment-serialization-algorithm
        if (allowEmptyDefaultNS && namespaces.get(emptyAtom().impl()))
            result.append(' ', xmlnsAtom(), "=\"\"");
        return;
    }

    // Use emptyAtom()s's impl() for null strings since this HashMap can't handle nullptr as a key
    auto addResult = namespaces.add(prefix.isNull() ? emptyAtom().impl() : prefix.impl(), namespaceURI.impl());
    if (!addResult.isNewEntry) {
        if (addResult.iterator->value == namespaceURI.impl())
            return;
        addResult.iterator->value = namespaceURI.impl();
    }

    // Add namespace to prefix pair so we can do constraint checking later.
    if (inXMLFragmentSerialization() && !prefix.isEmpty())
        namespaces.set(namespaceURI.impl(), prefix.impl());

    // Make sure xml prefix and namespace are always known to uphold the constraints listed at http://www.w3.org/TR/xml-names11/#xmlReserved.
    if (namespaceURI == XMLNames::xmlNamespaceURI)
        return;

    result.append(' ', xmlnsAtom(), prefix.isEmpty() ? "" : ":", prefix, "=\"");
    appendAttributeValue(result, namespaceURI, false);
    result.append('"');
}

static inline bool isScriptEnabled(Node& node)
{
    RefPtr frame = node.document().frame();
    return frame && frame->script().canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript);
}

OptionSet<EntityMask> MarkupAccumulator::entityMaskForText(const Text& text) const
{
    using namespace ElementNames;

    if (inXMLFragmentSerialization())
        return EntityMaskInPCDATA;

    if (RefPtr element = text.parentElement()) {
        switch (element->elementName()) {
        case HTML::noscript:
            if (!isScriptEnabled(*element))
                break;
            FALLTHROUGH;
        case HTML::iframe:
        case HTML::noembed:
        case HTML::noframes:
        case HTML::plaintext:
        case HTML::script:
        case HTML::style:
        case HTML::xmp:
            return EntityMaskInCDATA;
        default:
            break;
        }
    }
    return EntityMaskInHTMLPCDATA;
}

void MarkupAccumulator::appendText(StringBuilder& result, const Text& text)
{
    appendCharactersReplacingEntities(result, text.data(), 0, text.length(), entityMaskForText(text));
}

static void appendXMLDeclaration(StringBuilder& result, const Document& document)
{
    if (!document.hasXMLDeclaration())
        return;

    auto encoding = document.xmlEncoding();
    bool isStandaloneSpecified = document.xmlStandaloneStatus() != Document::StandaloneStatus::Unspecified;

    result.append("<?xml version=\"",
        document.xmlVersion(),
        encoding.isEmpty() ? "" : "\" encoding=\"",
        encoding,
        isStandaloneSpecified ? (document.xmlStandalone() ? "\" standalone=\"yes" : "\" standalone=\"no") : "",
        "\"?>");
}

static void appendDocumentType(StringBuilder& result, const DocumentType& documentType)
{
    if (documentType.name().isEmpty())
        return;

    result.append(
        "<!DOCTYPE ",
        documentType.name(),
        documentType.publicId().isEmpty() ? "" : " PUBLIC \"",
        documentType.publicId(),
        documentType.publicId().isEmpty() ? "" : "\"",
        documentType.systemId().isEmpty() ? "" : (documentType.publicId().isEmpty() ? " SYSTEM \"" : " \""),
        documentType.systemId(),
        documentType.systemId().isEmpty() ? ">" : "\">"
    );
}

void MarkupAccumulator::appendStartTag(StringBuilder& result, const Element& element, Namespaces* namespaces)
{
    appendOpenTag(result, element, namespaces);

    bool hasURLAttribute = false;
    if (element.hasAttributes()) {
        for (const Attribute& attribute : element.attributesIterator()) {
            if (!hasURLAttribute && (element.isURLAttribute(attribute) || element.isHTMLContentAttribute(attribute)))
                hasURLAttribute = true;
            auto updatedAttribute = replaceAttributeIfNecessary(element, attribute);
            appendAttribute(result, element, updatedAttribute, namespaces);
        }
    }

    if (!hasURLAttribute)
        appendURLAttributeIfNecessary(result, element, namespaces);

    // Give an opportunity to subclasses to add their own attributes.
    appendCustomAttributes(result, element, namespaces);

    appendCloseTag(result, element);
}

void MarkupAccumulator::appendOpenTag(StringBuilder& result, const Element& element, Namespaces* namespaces)
{
    result.append('<');
    if (inXMLFragmentSerialization() && namespaces && element.prefix().isEmpty()) {
        // According to http://www.w3.org/TR/DOM-Level-3-Core/namespaces-algorithms.html#normalizeDocumentAlgo we now should create
        // a default namespace declaration to make this namespace well-formed. However, http://www.w3.org/TR/xml-names11/#xmlReserved states
        // "The prefix xml MUST NOT be declared as the default namespace.", so use the xml prefix explicitly.
        if (element.namespaceURI() == XMLNames::xmlNamespaceURI) {
            result.append(xmlAtom());
            result.append(':');
        }
    }
    result.append(element.tagQName().toString());
    if (inXMLFragmentSerialization() && namespaces && shouldAddNamespaceElement(element))
        appendNamespace(result, element.prefix(), element.namespaceURI(), *namespaces, inXMLFragmentSerialization());
}

void MarkupAccumulator::appendCloseTag(StringBuilder& result, const Element& element)
{
    if (shouldSelfClose(element, m_serializationSyntax)) {
        if (element.isHTMLElement())
            result.append(' '); // XHTML 1.0 <-> HTML compatibility.
        result.append('/');
    }
    result.append('>');
}

void MarkupAccumulator::generateUniquePrefix(QualifiedName& prefixedName, const Namespaces& namespaces)
{
    // http://www.w3.org/TR/DOM-Level-3-Core/namespaces-algorithms.html#normalizeDocumentAlgo
    // Find a prefix following the pattern "NS" + index (starting at 1) and make sure this
    // prefix is not declared in the current scope.
    AtomString name;
    do {
        // FIXME: We should create makeAtomString, which would be more efficient.
        name = makeAtomString("ns"_s, ++m_prefixLevel);
    } while (namespaces.get(name.impl()));
    prefixedName.setPrefix(name);
}

// https://html.spec.whatwg.org/#attribute's-serialised-name
static String htmlAttributeSerialization(const Attribute& attribute)
{
    if (attribute.namespaceURI().isEmpty())
        return attribute.name().localName();

    QualifiedName prefixedName = attribute.name();
    if (attribute.namespaceURI() == XMLNames::xmlNamespaceURI)
        prefixedName.setPrefix(xmlAtom());
    else if (attribute.namespaceURI() == XMLNSNames::xmlnsNamespaceURI) {
        if (prefixedName.localName() == xmlnsAtom())
            return xmlnsAtom();
        prefixedName.setPrefix(xmlnsAtom());
    } else if (attribute.namespaceURI() == XLinkNames::xlinkNamespaceURI)
        prefixedName.setPrefix(AtomString("xlink"_s));
    return prefixedName.toString();
}

// https://w3c.github.io/DOM-Parsing/#dfn-xml-serialization-of-the-attributes
QualifiedName MarkupAccumulator::xmlAttributeSerialization(const Attribute& attribute, Namespaces* namespaces)
{
    QualifiedName prefixedName = attribute.name();
    if (!attribute.namespaceURI().isEmpty()) {
        if (attribute.namespaceURI() == XMLNames::xmlNamespaceURI) {
            // Always use xml as prefix if the namespace is the XML namespace.
            prefixedName.setPrefix(xmlAtom());
        } else {
            AtomStringImpl* foundNS = namespaces && attribute.prefix().impl() ? namespaces->get(attribute.prefix().impl()) : nullptr;
            bool prefixIsAlreadyMappedToOtherNS = foundNS && foundNS != attribute.namespaceURI().impl();
            if (attribute.prefix().isEmpty() || !foundNS || prefixIsAlreadyMappedToOtherNS) {
                if (RefPtr prefix = namespaces ? namespaces->get(attribute.namespaceURI().impl()) : nullptr)
                    prefixedName.setPrefix(AtomString(WTFMove(prefix)));
                else {
                    bool shouldBeDeclaredUsingAppendNamespace = !attribute.prefix().isEmpty() && !foundNS;
                    if (!shouldBeDeclaredUsingAppendNamespace && attribute.localName() != xmlnsAtom() && namespaces)
                        generateUniquePrefix(prefixedName, *namespaces);
                }
            }
        }
    }
    return prefixedName;
}

LocalFrame* MarkupAccumulator::frameForAttributeReplacement(const Element& element) const
{
    if (inXMLFragmentSerialization() || m_replacementURLStrings.isEmpty())
        return nullptr;

    RefPtr frameElement = dynamicDowncast<HTMLFrameElementBase>(element);
    if (!frameElement)
        return nullptr;

    return dynamicDowncast<LocalFrame>(frameElement->contentFrame());
}

Attribute MarkupAccumulator::replaceAttributeIfNecessary(const Element& element, const Attribute& attribute)
{
    if (element.isHTMLContentAttribute(attribute)) {
        auto frame = frameForAttributeReplacement(element);
        if (!frame || !frame->loader().documentLoader()->response().url().isAboutSrcDoc())
            return attribute;

        auto replacementURLString = m_replacementURLStrings.get(frame->frameID().toString());
        if (replacementURLString.isNull())
            return attribute;

        return { srcAttr, AtomString { replacementURLString } };
    }

    return element.replaceURLsInAttributeValue(attribute, m_replacementURLStrings);
}

void MarkupAccumulator::appendURLAttributeIfNecessary(StringBuilder& result, const Element& element, Namespaces* namespaces)
{
    auto frame = frameForAttributeReplacement(element);
    if (!frame)
        return;

    auto replacementURLString = m_replacementURLStrings.get(frame->frameID().toString());
    if (!replacementURLString.isNull())
        appendAttribute(result, element, Attribute { srcAttr, AtomString { replacementURLString } }, namespaces);
}

void MarkupAccumulator::appendAttribute(StringBuilder& result, const Element& element, const Attribute& attribute, Namespaces* namespaces)
{
    bool isSerializingHTML = !inXMLFragmentSerialization();

    std::optional<QualifiedName> effectiveXMLPrefixedName;

    // Per https://w3c.github.io/DOM-Parsing/#dfn-xml-serialization-of-the-attributes the xmlns attribute is serialized first
    if (!isSerializingHTML) {
        effectiveXMLPrefixedName = xmlAttributeSerialization(attribute, namespaces);
        if (namespaces && shouldAddNamespaceAttribute(attribute, *namespaces))
            appendNamespace(result, effectiveXMLPrefixedName->prefix(), effectiveXMLPrefixedName->namespaceURI(), *namespaces);
    }

    result.append(' ');

    if (isSerializingHTML)
        result.append(htmlAttributeSerialization(attribute));
    else
        result.append(effectiveXMLPrefixedName->toString());

    result.append('=');

    result.append('"');
    if (element.isURLAttribute(attribute)) {
        // FIXME: This does not fully match other browsers. Firefox percent-escapes
        // non-ASCII characters for innerHTML.
        appendAttributeValue(result, resolveURLIfNeeded(element, attribute.value()), isSerializingHTML);
    } else
        appendAttributeValue(result, attribute.value(), isSerializingHTML);
    result.append('"');
}

void MarkupAccumulator::appendNonElementNode(StringBuilder& result, const Node& node, Namespaces* namespaces)
{
    if (namespaces)
        namespaces->checkConsistency();

    switch (node.nodeType()) {
    case Node::TEXT_NODE:
        appendText(result, uncheckedDowncast<Text>(node));
        break;
    case Node::COMMENT_NODE:
        // FIXME: Comment content is not escaped, but that may be OK because XMLSerializer (and possibly other callers) should raise an exception if it includes "-->".
        result.append("<!--", uncheckedDowncast<Comment>(node).data(), "-->");
        break;
    case Node::DOCUMENT_NODE:
        appendXMLDeclaration(result, uncheckedDowncast<Document>(node));
        break;
    case Node::DOCUMENT_FRAGMENT_NODE:
        break;
    case Node::DOCUMENT_TYPE_NODE:
        appendDocumentType(result, uncheckedDowncast<DocumentType>(node));
        break;
    case Node::PROCESSING_INSTRUCTION_NODE: {
        auto& instruction = uncheckedDowncast<ProcessingInstruction>(node);
        // FIXME: PI data is not escaped, but XMLSerializer (and possibly other callers) this should raise an exception if it includes "?>".
        result.append("<?", instruction.target(), ' ', instruction.data(), "?>");
        break;
    }
    case Node::ELEMENT_NODE:
        ASSERT_NOT_REACHED();
        break;
    case Node::CDATA_SECTION_NODE:
        // FIXME: CDATA content is not escaped, but XMLSerializer (and possibly other callers) should raise an exception if it includes "]]>".
        result.append("<![CDATA[", uncheckedDowncast<CDATASection>(node).data(), "]]>");
        break;
    case Node::ATTRIBUTE_NODE:
        // Only XMLSerializer can pass an Attr. So, |documentIsHTML| flag is false.
        appendAttributeValue(result, uncheckedDowncast<Attr>(node).value(), false);
        break;
    }
}

static bool isElementExcludedByRule(const MarkupExclusionRule& rule, const Element& element)
{
    if (!rule.elementLocalName.isNull() && !equalIgnoringASCIICase(rule.elementLocalName, element.localName()))
        return false;

    unsigned matchedAttributes = 0;
    if (element.hasAttributes()) {
        for (auto& [attributeLocalName, attributeValue] : rule.attributes) {
            if (attributeLocalName.isNull()) {
                ++matchedAttributes;
                continue;
            }

            // FIXME: We might optimize this by using a HashMap when there are too many attributes.
            for (const Attribute& attribute : element.attributesIterator()) {
                if (!equalIgnoringASCIICase(attribute.localName(), attributeLocalName))
                    continue;
                if (attributeValue.isNull() || equalIgnoringASCIICase(attribute.value(), attributeValue)) {
                    ++matchedAttributes;
                    break;
                }
            }
        }
    }
    return matchedAttributes == rule.attributes.size();
}

bool MarkupAccumulator::shouldExcludeElement(const Element& element)
{
    return WTF::anyOf(m_exclusionRules, [&](auto& rule) {
        return isElementExcludedByRule(rule, element);
    });
}

}
