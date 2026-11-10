/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#include "CustomElementRegistry.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLElement.h"
#include "HTMLFrameElement.h"
#include "HTMLHeadElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "HTMLTemplateElement.h"
#include "NodeName.h"
#include "ProcessingInstruction.h"
#include "ScriptController.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TemplateContentDocumentFragment.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include <algorithm>
#include <memory>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

struct EntityDescription {
    ASCIILiteral characters;
    std::optional<EntityMask> mask;
};

constexpr std::array entitySubstitutionList {
    EntityDescription { ""_s, std::nullopt },
    EntityDescription { "&amp;"_s, EntityMask::Amp },
    EntityDescription { "&lt;"_s, EntityMask::Lt },
    EntityDescription { "&gt;"_s, EntityMask::Gt },
    EntityDescription { "&quot;"_s, EntityMask::Quot },
    EntityDescription { "&nbsp;"_s, EntityMask::Nbsp },
    EntityDescription { "&#9;"_s, EntityMask::Tab },
    EntityDescription { "&#10;"_s, EntityMask::LineFeed },
    EntityDescription { "&#13;"_s, EntityMask::CarriageReturn },
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

static constexpr unsigned maximumEscapedentityCharacter = noBreakSpace;
static constexpr std::array<uint8_t, maximumEscapedentityCharacter + 1> entityMap {
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

NODELETE static bool elementCannotHaveEndTag(const Node& node)
{
    using namespace ElementNames;
    auto* element = dynamicDowncast<Element>(node);
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
NODELETE static bool shouldSelfClose(const Element& element, SerializationSyntax syntax)
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
static inline void appendCharactersReplacingEntitiesInternal(StringBuilder& result, StringImpl* source, OptionSet<EntityMask> entityMask)
{
    unsigned length = WTF::length(source);
    size_t positionAfterLastEntity = 0;
    for (size_t i = 0; i < length; ++i) {
        CharacterType character = codeUnitAt(source, i);
        uint8_t substitution = character < std::size(entityMap) ? entityMap[character] : static_cast<uint8_t>(EntitySubstitutionIndex::Null);
        if (substitution != EntitySubstitutionIndex::Null) [[unlikely]] {
            if (entityMask.contains(*entitySubstitutionList[substitution].mask)) {
                result.appendSubstring(source, positionAfterLastEntity, i - positionAfterLastEntity);
                result.append(entitySubstitutionList[substitution].characters);
                positionAfterLastEntity = i + 1;
            }
        }
    }
    result.appendSubstring(source, positionAfterLastEntity, length - positionAfterLastEntity);
}

void MarkupAccumulator::appendCharactersReplacingEntities(StringBuilder& result, StringImpl* source, OptionSet<EntityMask> entityMask)
{
    if (!WTF::length(source))
        return;

    if (is8Bit(source))
        appendCharactersReplacingEntitiesInternal<Latin1Character>(result, source, entityMask);
    else
        appendCharactersReplacingEntitiesInternal<char16_t>(result, source, entityMask);
}

MarkupAccumulator::MarkupAccumulator(Vector<Ref<Node>>* nodes, ResolveURLs resolveURLs, SerializationSyntax serializationSyntax, SerializeShadowRoots serializeShadowRoots, Vector<Ref<ShadowRoot>>&& explicitShadowRoots, const Vector<MarkupExclusionRule>& exclusionRules)
    : m_nodes(nodes)
    , m_resolveURLs(resolveURLs)
    , m_serializationSyntax(serializationSyntax)
    , m_serializeShadowRoots(serializeShadowRoots)
    , m_explicitShadowRoots(WTF::move(explicitShadowRoots))
    , m_exclusionRules(exclusionRules)
{
    ASSERT(serializeShadowRoots != SerializeShadowRoots::AllForInterchange || explicitShadowRoots.isEmpty());
}

MarkupAccumulator::~MarkupAccumulator() = default;

void MarkupAccumulator::enableURLReplacement(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>&& replacementURLStrings, HashMap<Ref<CSSStyleSheet>, RefPtr<StringImpl>>&& replacementURLStringsForCSSStyleSheet)
{
    m_serializationContext = CSS::SerializationContext {
        WTF::move(replacementURLStrings),
        WTF::move(replacementURLStringsForCSSStyleSheet),
        true,
    };
}

RefPtr<StringImpl> MarkupAccumulator::serializeNodes(Node& targetNode, SerializedNodes root)
{
    serializeNodesWithNamespaces(targetNode, root, 0);
    return m_markup.toString();
}

bool MarkupAccumulator::appendContentsForNode(StringBuilder& result, const Node& targetNode)
{
    if (!m_serializationContext)
        return false;

    RefPtr styleElement = dynamicDowncast<HTMLStyleElement>(targetNode);
    if (!styleElement)
        return false;

    RefPtr cssStyleSheet = styleElement->sheet();
    if (!cssStyleSheet)
        return false;

    result.append(cssStyleSheet->cssText(*m_serializationContext));
    return true;
}

bool MarkupAccumulator::shouldIncludeShadowRoots() const
{
    return m_serializeShadowRoots != SerializeShadowRoots::Explicit || !m_explicitShadowRoots.isEmpty();
}

bool MarkupAccumulator::includeShadowRoot(const ShadowRoot& shadowRoot) const
{
    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        return false;

    if (m_serializeShadowRoots == SerializeShadowRoots::AllForInterchange)
        return true;

    if (m_serializeShadowRoots == SerializeShadowRoots::Serializable && shadowRoot.serializable())
        return true;

    if (m_explicitShadowRoots.containsIf([&shadowRoot](const auto& item) { return item.ptr() == &shadowRoot; }))
        return true;

    return false;
}

void MarkupAccumulator::serializeNodesWithNamespaces(Node& targetNode, SerializedNodes root, const Namespaces* namespaces)
{
    WTF::Vector<Namespaces> namespaceStack;
    if (namespaces)
        namespaceStack.append(*namespaces);
    else if (inXMLFragmentSerialization()) {
        // Make sure xml prefix and namespace are always known to uphold the constraints listed at http://www.w3.org/TR/xml-names11/#xmlReserved.
        Namespaces namespaceHash;
        namespaceHash.set(xmlAtom().get(), XMLNames::xmlNamespaceURI->get());
        namespaceHash.set(XMLNames::xmlNamespaceURI->get(), xmlAtom().get());
        namespaceStack.append(WTF::move(namespaceHash));
    } else
        namespaceStack.constructAndAppend();

    auto firstChild = [&](auto& current) -> Node* {
        bool shouldSkipChidren = appendContentsForNode(m_markup, current);
        if (shouldSkipChidren)
            return nullptr;
        RefPtr currentTemplate = dynamicDowncast<HTMLTemplateElement>(current);
        return currentTemplate ? currentTemplate->content().firstChild() : current.firstChild();
    };

    RefPtr<const Node> current = targetNode;
    do {
        bool shouldSkipNode = false;
        if (RefPtr element = dynamicDowncast<const Element>(current); element && shouldExcludeElement(*element))
            shouldSkipNode = true;

        bool shouldAppendNode = !shouldSkipNode && !(current == &targetNode && root != SerializedNodes::SubtreeIncludingNode);
        if (shouldAppendNode)
            startAppendingNode(*current, &namespaceStack.last());

        bool shouldEmitCloseTag = m_serializationSyntax == SerializationSyntax::XML || !(targetNode.document().isHTMLDocument() && elementCannotHaveEndTag(*current));
        shouldSkipNode = shouldSkipNode || !shouldEmitCloseTag;
        if (!shouldSkipNode) {
            if (shouldIncludeShadowRoots()) {
                RefPtr shadowRoot = current->shadowRoot();
                if (shadowRoot && includeShadowRoot(*shadowRoot)) {
                    current = shadowRoot;
                    namespaceStack.append(namespaceStack.last());
                    continue;
                }
            }

            if (RefPtr child = firstChild(*current)) {
                current = child;
                namespaceStack.append(namespaceStack.last());
                continue;
            }
        }

        if (shouldAppendNode && shouldEmitCloseTag)
            endAppendingNode(*current);

        while (current != &targetNode) {
            if (RefPtr nextSibling = current->nextSibling()) {
                current = WTF::move(nextSibling);
                namespaceStack.removeLast();
                namespaceStack.append(namespaceStack.last());
                break;
            }

            if (shouldIncludeShadowRoots() && current->isShadowRoot()) {
                current = current->shadowHost();
                if (m_serializeShadowRoots != SerializeShadowRoots::AllForInterchange) {
                    if (RefPtr child = firstChild(*current)) {
                        current = child;
                        namespaceStack.append(namespaceStack.last());
                        break;
                    }
                }
            } else
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
            shouldEmitCloseTag = m_serializationSyntax == SerializationSyntax::XML || !(targetNode.document().isHTMLDocument() && elementCannotHaveEndTag(*current));
            if (shouldAppendNode && shouldEmitCloseTag)
                endAppendingNode(*current);
        }
    } while (current != &targetNode);
}

std::pair<RefPtr<StringImpl>, MarkupAccumulator::IsCreatedByURLReplacement> MarkupAccumulator::resolveURLIfNeeded(const Element& element, StringImpl* urlString) const
{
    if (auto* link = dynamicDowncast<HTMLLinkElement>(element); link && m_serializationContext) {
        if (RefPtr cssStyleSheet = link->sheet()) {
            auto replacementURLString = m_serializationContext->replacementURLStringsForCSSStyleSheet.get(*cssStyleSheet);
            if (!isEmpty(replacementURLString))
                return { replacementURLString, IsCreatedByURLReplacement::Yes };
        }
    }

    if (m_serializationContext) {
        if (RefPtr frame = frameForAttributeReplacement(element)) {
            auto replacementURLString = m_serializationContext->replacementURLStrings.get(makeString(frame->frameID().toUInt64()));
            if (!isEmpty(replacementURLString))
                return { replacementURLString, IsCreatedByURLReplacement::Yes };
        }

        auto resolvedURLString = element.resolveURLStringIfNeeded(urlString);
        auto replacementURLString = m_serializationContext->replacementURLStrings.get(resolvedURLString);
        if (!isEmpty(replacementURLString))
            return { replacementURLString, IsCreatedByURLReplacement::Yes };
    }

    return { element.resolveURLStringIfNeeded(urlString, m_resolveURLs), IsCreatedByURLReplacement::No };
}

RefPtr<const ShadowRoot> MarkupAccumulator::suitableShadowRoot(const Node& node)
{
    if (!shouldIncludeShadowRoots())
        return nullptr;

    RefPtr shadowRoot = dynamicDowncast<ShadowRoot>(node);
    if (!shadowRoot || !includeShadowRoot(*shadowRoot))
        return nullptr;
    return shadowRoot;
}

void MarkupAccumulator::startAppendingNode(const Node& node, Namespaces* namespaces)
{
    if (RefPtr element = dynamicDowncast<Element>(node)) {
        appendStartTag(m_markup, *element, namespaces);

        // Currently URL Replacement only happens when saving markup to disk and the file uses UTF-8 encoding.
        // To ensure the file can be loaded correctly, change the specified encoding to UTF-8.
        // FIXME: This can be dropped when file encoding matches declared encoding.
        if (m_serializationContext && is<HTMLHeadElement>(element))
            m_markup.append("<meta charset=\"UTF-8\"><!-- Encoding specified by WebKit -->"_s);

    } else if (RefPtr shadowRoot = suitableShadowRoot(node)) {
        m_markup.append("<template shadowrootmode=\""_s, serializeShadowRootMode(shadowRoot->mode()), '"');
        if (shadowRoot->delegatesFocus())
            m_markup.append(" shadowrootdelegatesfocus=\"\""_s);
        if (shadowRoot->serializable())
            m_markup.append(" shadowrootserializable=\"\""_s);
        if (shadowRoot->slotAssignmentMode() == SlotAssignmentMode::Manual)
            m_markup.append(" shadowrootslotassignment=\"manual\""_s);
        if (shadowRoot->isClonable())
            m_markup.append(" shadowrootclonable=\"\""_s);
        bool shouldAppendRegistryAttribute = [&] {
            auto& document = shadowRoot->document();
            if (document.usesNullCustomElementRegistry() && shadowRoot->usesNullCustomElementRegistry())
                return false;

            auto* documentRegistry = document.customElementRegistry();
            auto* shadowRegistry = shadowRoot->customElementRegistry();
            bool documentHasGlobalRegistry = (documentRegistry && !documentRegistry->isScoped()) || document.window();
            bool shadowHasGlobalRegistry = (shadowRegistry && !shadowRegistry->isScoped())
                || (!shadowRegistry && !shadowRoot->usesNullCustomElementRegistry() && document.window());
            return !(documentHasGlobalRegistry && shadowHasGlobalRegistry);
        }();
        if (shouldAppendRegistryAttribute)
            m_markup.append(" shadowrootcustomelementregistry=\"\""_s);
        m_markup.append('>');
    } else
        appendNonElementNode(m_markup, node, namespaces);

    if (m_nodes)
        m_nodes->append(const_cast<Node&>(node));
}

void MarkupAccumulator::appendEndTag(StringBuilder& result, const Element& element)
{
    RefPtr<AtomStringImpl> resolvedQualifiedName;
    if (!m_resolvedElementQualifiedNameStack.isEmpty() && m_resolvedElementQualifiedNameStack.last().first.ptr() == &element)
        resolvedQualifiedName = m_resolvedElementQualifiedNameStack.takeLast().second;

    if (shouldSelfClose(element, m_serializationSyntax) || (!element.hasChildNodes() && elementCannotHaveEndTag(element)))
        return;

    if (resolvedQualifiedName)
        result.append("</"_s, resolvedQualifiedName, '>');
    else
        result.append("</"_s, element.tagQName().toString(), '>');
}

StringBuilder MarkupAccumulator::takeMarkup()
{
    return std::exchange(m_markup, { });
}

void MarkupAccumulator::appendAttributeValue(StringBuilder& result, StringImpl* attribute)
{
    auto entityMask = [&] -> OptionSet<EntityMask> {
        switch (m_serializationSyntax) {
        case SerializationSyntax::XML:
            return EntityMaskInAttributeValue;
        case SerializationSyntax::HTML:
            return EntityMaskInHTMLAttributeValue;
        }
        ASSERT_NOT_REACHED();
        return EntityMaskInAttributeValue;
    }();
    appendCharactersReplacingEntities(result, attribute, entityMask);
}

void MarkupAccumulator::appendCustomAttributes(StringBuilder&, const Element&, Namespaces*)
{
}

static bool shouldAddNamespaceElement(const Element& element)
{
    // Don't add namespace attribute if it is already defined for this elem.
    auto& prefix = element.prefix();
    if (isEmpty(prefix))
        return !element.hasAttribute(xmlnsAtom());
    return !element.hasAttribute(makeAtomString("xmlns:"_s, prefix));
}

static bool shouldAddNamespaceAttribute(const Attribute& attribute, Namespaces& namespaces)
{
    namespaces.checkConsistency();

    // Don't add namespace attributes twice
    // HTML Parser will create xmlns attributes without namespace for HTML elements, allow those as well.
    if (attribute.name().localName() == xmlnsAtom() && (isEmpty(attribute.namespaceURI()) || attribute.namespaceURI() == XMLNSNames::xmlnsNamespaceURI)) {
        namespaces.set(emptyAtom().get(), attribute.value().get());
        return false;
    }

    QualifiedName xmlnsPrefixAttr(xmlnsAtom(), attribute.localName(), XMLNSNames::xmlnsNamespaceURI);
    if (attribute.name() == xmlnsPrefixAttr) {
        namespaces.set(attribute.localName().get(), attribute.value().get());
        namespaces.set(attribute.value().get(), attribute.localName().get());
        return false;
    }

    return true;
}

void MarkupAccumulator::appendNamespace(StringBuilder& result, AtomStringImpl* prefix, AtomStringImpl* namespaceURI, Namespaces& namespaces, bool allowEmptyDefaultNS)
{
    namespaces.checkConsistency();
    if (isEmpty(namespaceURI)) {
        // https://html.spec.whatwg.org/multipage/xhtml.html#xml-fragment-serialisation-algorithm
        if (allowEmptyDefaultNS) {
            RefPtr currentDefaultNamespace = namespaces.get(emptyAtom().get());
            if (currentDefaultNamespace && !currentDefaultNamespace->isEmpty())
                result.append(' ', xmlnsAtom(), "=\"\""_s);
        }
        return;
    }

    // Use emptyAtom()s's impl() for null strings since this HashMap can't handle nullptr as a key
    auto addResult = namespaces.add(!prefix ? emptyAtom().get() : prefix, namespaceURI);
    if (!addResult.isNewEntry) {
        if (addResult.iterator->value == namespaceURI)
            return;
        addResult.iterator->value = namespaceURI;
    }

    // Add namespace to prefix pair so we can do constraint checking later.
    if (inXMLFragmentSerialization() && !isEmpty(prefix))
        namespaces.set(namespaceURI, prefix);

    // Make sure xml prefix and namespace are always known to uphold the constraints listed at http://www.w3.org/TR/xml-names11/#xmlReserved.
    if (namespaceURI == XMLNames::xmlNamespaceURI)
        return;

    result.append(' ', xmlnsAtom(), isEmpty(prefix) ? ""_s : ":"_s, prefix, "=\""_s);
    appendAttributeValue(result, namespaceURI);
    result.append('"');
}

static inline bool isScriptEnabled(Node& node)
{
    RefPtr frame = node.document().frame();
    return frame && protect(frame->script())->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript);
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
            [[fallthrough]];
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
    appendCharactersReplacingEntities(result, text.data(), entityMaskForText(text));
}

static void appendXMLDeclaration(StringBuilder& result, const Document& document)
{
    if (!document.hasXMLDeclaration())
        return;

    auto encoding = document.xmlEncoding();
    bool isStandaloneSpecified = document.xmlStandaloneStatus() != Document::StandaloneStatus::Unspecified;

    result.append("<?xml version=\""_s,
        document.xmlVersion(),
        isEmpty(encoding) ? ""_s : "\" encoding=\""_s,
        encoding,
        isStandaloneSpecified ? (document.xmlStandalone() ? "\" standalone=\"yes"_s : "\" standalone=\"no"_s) : ""_s,
        "\"?>"_s);
}

static void appendDocumentType(StringBuilder& result, const DocumentType& documentType)
{
    if (isEmpty(documentType.name()))
        return;

    result.append(
        "<!DOCTYPE "_s,
        documentType.name(),
        isEmpty(documentType.publicId()) ? ""_s : " PUBLIC \""_s,
        documentType.publicId(),
        isEmpty(documentType.publicId()) ? ""_s : "\""_s,
        isEmpty(documentType.systemId()) ? ""_s : (isEmpty(documentType.publicId()) ? " SYSTEM \""_s : " \""_s),
        documentType.systemId(),
        isEmpty(documentType.systemId()) ? ">"_s : "\">"_s
    );
}

NODELETE static bool isURLAttributeForElement(const Element& element, const Attribute& attribute)
{
    return element.isURLAttribute(attribute) || element.isHTMLContentAttribute(attribute);
}

void MarkupAccumulator::appendStartTagWithURLReplacement(StringBuilder& result, const Element& element, Namespaces* namespaces)
{
    bool hasURLAttribute = false;
    bool isURLReplaced = false;
    Vector<Attribute> attributesToAppendIfURLNotReplaced;

    if (element.hasAttributes()) {
        for (auto& attribute : element.attributes()) {
            if (attribute.name() == crossoriginAttr || attribute.name() == integrityAttr) {
                attributesToAppendIfURLNotReplaced.append(attribute);
                continue;
            }

            if (!hasURLAttribute && isURLAttributeForElement(element, attribute))
                hasURLAttribute = true;

            auto updatedAttribute = replaceAttributeIfNecessary(element, attribute);
            if (appendAttribute(result, element, updatedAttribute, namespaces))
                isURLReplaced = true;
        }
    }

    if (!hasURLAttribute && appendURLAttributeForReplacementIfNecessary(result, element, namespaces))
        isURLReplaced = true;

    if (isURLReplaced)
        return;

    for (auto& attribute : attributesToAppendIfURLNotReplaced)
        appendAttribute(result, element, attribute, namespaces);
}

void MarkupAccumulator::appendStartTag(StringBuilder& result, const Element& element, Namespaces* namespaces)
{
    appendOpenTag(result, element, namespaces);

    if (m_serializationContext) {
        // This function might change ordering of attributes in markup, so it should only be used for URL replacement case.
        appendStartTagWithURLReplacement(result, element, namespaces);
    } else {
        if (element.hasAttributes()) {
            for (auto& attribute : element.attributes())
                appendAttribute(result, element, attribute, namespaces);
        }
    }

    // Give an opportunity to subclasses to add their own attributes.
    appendCustomAttributes(result, element, namespaces);

    appendCloseTag(result, element);
}

void MarkupAccumulator::appendOpenTag(StringBuilder& result, const Element& element, Namespaces* namespaces)
{
    result.append('<');
    if (inXMLFragmentSerialization() && namespaces && isEmpty(element.prefix())) {
        // According to http://www.w3.org/TR/DOM-Level-3-Core/namespaces-algorithms.html#normalizeDocumentAlgo we now should create
        // a default namespace declaration to make this namespace well-formed. However, http://www.w3.org/TR/xml-names11/#xmlReserved states
        // "The prefix xml MUST NOT be declared as the default namespace.", so use the xml prefix explicitly.
        if (element.namespaceURI() == XMLNames::xmlNamespaceURI) {
            auto qualifiedName = makeAtomString(xmlAtom(), ':', element.localName());
            result.append(qualifiedName);
            m_resolvedElementQualifiedNameStack.append({ element, WTF::move(qualifiedName) });
            return;
        } else if (!isEmpty(element.namespaceURI()) && !element.hasAttribute(xmlnsAtom())) {
            // If the element doesn't carry its own default namespace declaration,
            // look for an existing prefix mapped to this namespace URI in an ancestor
            // and use it instead of emitting a new default namespace declaration.
            RefPtr<AtomStringImpl> inheritedDefaultNS = namespaces->get(emptyAtom().get());
            if (!inheritedDefaultNS || inheritedDefaultNS != element.namespaceURI()) {
                RefPtr<AtomStringImpl> existingPrefix = namespaces->get(element.namespaceURI().get());
                if (!isEmpty(existingPrefix)) {
                    auto qualifiedName = makeAtomString(existingPrefix, ':', element.localName());
                    result.append(qualifiedName);
                    m_resolvedElementQualifiedNameStack.append({ element, WTF::move(qualifiedName) });
                    return;
                }
            }
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
    RefPtr<AtomStringImpl> name;
    do {
        // FIXME: We should create makeAtomString, which would be more efficient.
        name = makeAtomString("ns"_s, ++m_prefixLevel);
    } while (namespaces.get(name.get()));
    prefixedName.setPrefix(name);
}

// https://html.spec.whatwg.org/#attribute's-serialised-name
static RefPtr<StringImpl> htmlAttributeSerialization(const Attribute& attribute)
{
    if (isEmpty(attribute.namespaceURI()))
        return attribute.name().localName();

    QualifiedName prefixedName = attribute.name();
    if (attribute.namespaceURI() == XMLNames::xmlNamespaceURI)
        prefixedName.setPrefix(xmlAtom());
    else if (attribute.namespaceURI() == XMLNSNames::xmlnsNamespaceURI) {
        if (prefixedName.localName() == xmlnsAtom())
            return xmlnsAtom();
        prefixedName.setPrefix(xmlnsAtom());
    } else if (attribute.namespaceURI() == XLinkNames::xlinkNamespaceURI)
        prefixedName.setPrefix("xlink"_atom);
    return prefixedName.toString();
}

// https://w3c.github.io/DOM-Parsing/#dfn-xml-serialization-of-the-attributes
QualifiedName MarkupAccumulator::xmlAttributeSerialization(const Attribute& attribute, Namespaces* namespaces)
{
    QualifiedName prefixedName = attribute.name();
    if (!isEmpty(attribute.namespaceURI())) {
        if (attribute.namespaceURI() == XMLNames::xmlNamespaceURI) {
            // Always use xml as prefix if the namespace is the XML namespace.
            prefixedName.setPrefix(xmlAtom());
        } else {
            RefPtr foundNS = namespaces && attribute.prefix().get() ? namespaces->get(attribute.prefix().get()) : nullptr;
            bool prefixIsAlreadyMappedToOtherNS = foundNS && foundNS != attribute.namespaceURI().get();
            if (isEmpty(attribute.prefix()) || !foundNS || prefixIsAlreadyMappedToOtherNS) {
                if (RefPtr prefix = namespaces ? namespaces->get(attribute.namespaceURI().get()) : nullptr)
                    prefixedName.setPrefix(RefPtr<AtomStringImpl>(WTF::move(prefix)));
                else {
                    bool shouldBeDeclaredUsingAppendNamespace = !isEmpty(attribute.prefix()) && !foundNS;
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
    if (inXMLFragmentSerialization())
        return nullptr;

    auto* frameElement = dynamicDowncast<HTMLFrameElementBase>(element);
    if (!frameElement)
        return nullptr;

    return dynamicDowncast<LocalFrame>(frameElement->contentFrame());
}

Attribute MarkupAccumulator::replaceAttributeIfNecessary(const Element& element, const Attribute& attribute)
{
    if (!m_serializationContext)
        return attribute;

    if (element.isHTMLContentAttribute(attribute)) {
        RefPtr frame = frameForAttributeReplacement(element);
        if (!frame || !frame->loader().documentLoader()->response().url().isAboutSrcDoc())
            return attribute;

        auto replacementURLString = m_serializationContext->replacementURLStrings.get(makeString(frame->frameID().toUInt64()));
        if (isEmpty(replacementURLString))
            return attribute;

        return { srcAttr, AtomStringImpl::add(replacementURLString).get() };
    }

    return element.replaceURLsInAttributeValue(attribute, *m_serializationContext);
}

bool MarkupAccumulator::appendURLAttributeForReplacementIfNecessary(StringBuilder& result, const Element& element, Namespaces* namespaces)
{
    if (!m_serializationContext)
        return false;

    RefPtr frame = frameForAttributeReplacement(element);
    if (!frame)
        return false;

    auto replacementURLString = m_serializationContext->replacementURLStrings.get(makeString(frame->frameID().toUInt64()));
    if (isEmpty(replacementURLString))
        return false;

    appendAttribute(result, element, Attribute { srcAttr, AtomStringImpl::add(replacementURLString).get() }, namespaces);
    return true;
}

bool MarkupAccumulator::appendAttribute(StringBuilder& result, const Element& element, const Attribute& attribute, Namespaces* namespaces)
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
    bool isURLAttributeValueReplaced = false;
    if (element.isURLAttribute(attribute)) {
        // FIXME: This does not fully match other browsers. Firefox percent-escapes
        // non-ASCII characters for innerHTML.
        auto [resolvedURL, isCreatedByURLReplacement] = resolveURLIfNeeded(element, attribute.value());
        appendAttributeValue(result, resolvedURL);
        isURLAttributeValueReplaced = isCreatedByURLReplacement == IsCreatedByURLReplacement::Yes;
    } else
        appendAttributeValue(result, attribute.value());
    result.append('"');

    return isURLAttributeValueReplaced;
}

void MarkupAccumulator::appendNonElementNode(StringBuilder& result, const Node& node, Namespaces* namespaces)
{
    if (namespaces)
        namespaces->checkConsistency();

    switch (node.nodeType()) {
    case NodeType::Text:
        appendText(result, uncheckedDowncast<Text>(node));
        break;
    case NodeType::Comment:
        // FIXME: Comment content is not escaped, but that may be OK because XMLSerializer (and possibly other callers) should raise an exception if it includes "-->".
        result.append("<!--"_s, uncheckedDowncast<Comment>(node).data(), "-->"_s);
        break;
    case NodeType::Document:
        appendXMLDeclaration(result, uncheckedDowncast<Document>(node));
        break;
    case NodeType::DocumentFragment:
        break;
    case NodeType::DocumentType:
        appendDocumentType(result, uncheckedDowncast<DocumentType>(node));
        break;
    case NodeType::ProcessingInstruction: {
        auto& instruction = uncheckedDowncast<ProcessingInstruction>(node);
        // FIXME: PI data is not escaped, but XMLSerializer (and possibly other callers) this should raise an exception if it includes "?>".
        result.append("<?"_s, instruction.target(), ' ', instruction.data(), "?>"_s);
        break;
    }
    case NodeType::Element:
        ASSERT_NOT_REACHED();
        break;
    case NodeType::CDATASection:
        if (inXMLFragmentSerialization()) {
            // FIXME: CDATA content is not escaped, but XMLSerializer (and possibly other callers) should raise an exception if it includes "]]>".
            result.append("<![CDATA["_s, uncheckedDowncast<CDATASection>(node).data(), "]]>"_s);
        } else
            appendText(result, uncheckedDowncast<Text>(node));
        break;
    case NodeType::Attribute:
        appendAttributeValue(result, uncheckedDowncast<Attr>(node).value());
        break;
    }
}

static bool isElementExcludedByRule(const MarkupExclusionRule& rule, const Element& element)
{
    if (rule.elementLocalName && !equalIgnoringASCIICase(rule.elementLocalName, element.localName()))
        return false;

    unsigned matchedAttributes = 0;
    if (element.hasAttributes()) {
        for (auto& [attributeLocalName, attributeValue] : rule.attributes) {
            if (!attributeLocalName) {
                ++matchedAttributes;
                continue;
            }

            // FIXME: We might optimize this by using a HashMap when there are too many attributes.
            for (auto& attribute : element.attributes()) {
                if (!equalIgnoringASCIICase(attribute.localName(), attributeLocalName))
                    continue;
                if (!attributeValue || equalIgnoringASCIICase(attribute.value(), attributeValue)) {
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
    return m_exclusionRules.containsIf([&](auto& rule) {
        return isElementExcludedByRule(rule, element);
    });
}

SerializationSyntax MarkupAccumulator::serializationSyntax(Document& document)
{
    return document.isHTMLDocument() ? SerializationSyntax::HTML : SerializationSyntax::XML;
}

}
