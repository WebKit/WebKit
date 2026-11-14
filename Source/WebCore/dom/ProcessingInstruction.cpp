/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 */

#include "config.h"
#include "ProcessingInstruction.h"

#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceRequest.h"
#include "CachedXSLStyleSheet.h"
#include "CommonAtomStrings.h"
#include "DocumentInlines.h"
#include "DocumentResourceLoader.h"
#include "DocumentView.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "MediaQueryParser.h"
#include "NameValidation.h"
#include <wtf/Borrow.h>
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "SerializedNode.h"
#include "Settings.h"
#include "StyleDocumentScope.h"
#include "StyleSheetContents.h"
#include "XMLDocumentParser.h"
#include "XSLStyleSheet.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include "CachedResourceLoader.h"
#include "HTMLParserIdioms.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProcessingInstruction);

inline ProcessingInstruction::ProcessingInstruction(Document& document, AtomString target, String data)
    : CharacterData(document, WTF::move(data), NodeType::ProcessingInstruction)
    , m_target(target.releaseString())
{
}

Ref<ProcessingInstruction> ProcessingInstruction::createWithoutValidation(Document& document, AtomString target, String data)
{
    return adoptRef(*new ProcessingInstruction(document, WTF::move(target), WTF::move(data)));
}

ExceptionOr<Ref<ProcessingInstruction>> ProcessingInstruction::create(Document& document, AtomString target, String data)
{
    if (!NameValidation::isValidXMLName(target))
        return Exception { ExceptionCode::InvalidCharacterError, makeString("Invalid processing instruction target: '"_s, target, '\'') };

    if (data.contains("?>"_s))
        return Exception { ExceptionCode::InvalidCharacterError };

    return createWithoutValidation(document, WTF::move(target), WTF::move(data));
}

ProcessingInstruction::~ProcessingInstruction()
{
    if (RefPtr sheet = m_sheet)
        sheet->clearOwnerNode();

    if (RefPtr cachedSheet = m_cachedSheet)
        cachedSheet->removeClient(*this);

    if (isConnected())
        protect(document().styleScope())->removeStyleSheetCandidateNode(*this);
}

void ProcessingInstruction::setSheet(RefPtr<StyleSheet>&& sheet)
{
    ASSERT(!sheet || sheet->ownerNode() == this);
    RefPtr oldSheet = std::exchange(m_sheet, WTF::move(sheet));
    if (oldSheet && oldSheet != m_sheet)
        oldSheet->clearOwnerNode();
}

String ProcessingInstruction::nodeName() const
{
    return m_target;
}

Ref<Node> ProcessingInstruction::cloneNodeInternal(Document& document, CloningOperation, CustomElementRegistry*) const
{
    // FIXME: Is it a problem that this does not copy m_localHref?
    // What about other data members?
    return createWithoutValidation(document, AtomString { m_target }, String { data() });
}

SerializedNode ProcessingInstruction::serializeNode(CloningOperation) const
{
    return { SerializedNode::ProcessingInstruction { { data() }, m_target } };
}

// MARK: - Pseudo attributes

static std::optional<char32_t> parseDecimalDigit(char16_t character)
{
    if (character >= '0' && character <= '9')
        return character - '0';
    return std::nullopt;
}

static std::optional<char32_t> parseHexDigit(char16_t character)
{
    if (character >= '0' && character <= '9')
        return character - '0';
    if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;
    if (character >= 'A' && character <= 'F')
        return character - 'A' + 10;
    return std::nullopt;
}

// Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF]
static bool isXMLChar(char32_t character)
{
    return character == 0x9 || character == 0xA || character == 0xD
        || (character >= 0x20 && character <= 0xD7FF)
        || (character >= 0xE000 && character <= 0xFFFD)
        || (character >= 0x10000 && character <= 0x10FFFF);
}

static bool parsePseudoAttributeReference(StringView source, unsigned& position, StringBuilder& value)
{
    ASSERT(source[position] == '&');
    auto semicolon = source.find(';', position);
    if (semicolon == notFound)
        return false;
    auto reference = source.substring(position + 1, semicolon - position - 1);
    position = semicolon + 1;

    if (reference == "amp"_s) {
        value.append('&');
        return true;
    }
    if (reference == "lt"_s) {
        value.append('<');
        return true;
    }
    if (reference == "gt"_s) {
        value.append('>');
        return true;
    }
    if (reference == "apos"_s) {
        value.append('\'');
        return true;
    }
    if (reference == "quot"_s) {
        value.append('"');
        return true;
    }
    if (reference.startsWith('#')) {
        // CharRef ::= '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'
        auto digits = reference.substring(1);
        bool isHex = digits.startsWith('x');
        if (isHex)
            digits = digits.substring(1);
        if (digits.isEmpty())
            return false;
        char32_t parsedCharacter = 0;
        for (auto digit : digits.codeUnits()) {
            auto digitValue = isHex ? parseHexDigit(digit) : parseDecimalDigit(digit);
            if (!digitValue)
                return false;
            parsedCharacter = parsedCharacter * (isHex ? 16 : 10) + *digitValue;
            if (parsedCharacter > UCHAR_MAX_VALUE)
                return false;
        }
        if (!isXMLChar(parsedCharacter))
            return false;
        value.append(parsedCharacter);
        return true;
    }
    return false;
}

std::optional<Vector<ProcessingInstruction::PseudoAttribute>> ProcessingInstruction::parsePseudoAttributes(StringView source)
{
    Vector<PseudoAttribute> attributes;
    unsigned position = 0;
    // XML S is space, tab, carriage return and line feed only.
    auto isXMLWhitespace = [](char16_t character) {
        return character == ' ' || character == '\t' || character == '\r' || character == '\n';
    };
    auto skipWhitespace = [&] {
        while (position < source.length() && isXMLWhitespace(source[position]))
            ++position;
    };

    skipWhitespace();
    while (position < source.length()) {
        auto nameStart = position;
        while (position < source.length() && !isXMLWhitespace(source[position]) && source[position] != '=')
            ++position;
        auto nameSource = source.substring(nameStart, position - nameStart);
        if (!NameValidation::isValidXMLName(nameSource))
            return std::nullopt;
        AtomString name { nameSource.toString() };
        if (attributes.containsIf([&name](auto& attribute) { return attribute.name == name; }))
            return std::nullopt;

        skipWhitespace();
        if (position == source.length() || source[position] != '=')
            return std::nullopt;
        ++position;
        skipWhitespace();

        if (position == source.length())
            return std::nullopt;
        auto quote = source[position];
        if (quote != '"' && quote != '\'')
            return std::nullopt;
        ++position;

        StringBuilder value;
        bool closed = false;
        while (position < source.length()) {
            auto character = source[position];
            if (character == quote) {
                ++position;
                closed = true;
                break;
            }
            if (character == '<')
                return std::nullopt;
            if (character == '&') {
                if (!parsePseudoAttributeReference(source, position, value))
                    return std::nullopt;
                continue;
            }
            value.append(character);
            ++position;
        }
        if (!closed)
            return std::nullopt;

        auto valueString = value.toString();
        for (auto character : StringView { valueString }.codePoints()) {
            if (!isXMLChar(character))
                return std::nullopt;
        }

        attributes.append({ WTF::move(name), AtomString { valueString } });

        // Attributes have to be separated by whitespace.
        auto beforeTrailingWhitespace = position;
        skipWhitespace();
        if (position < source.length() && position == beforeTrailingWhitespace)
            return std::nullopt;
    }
    return attributes;
}

void ProcessingInstruction::updateAttributesIfNeeded()
{
    if (!m_attributesDirty)
        return;
    m_attributesDirty = false;
    m_attributes.clear();

    if (auto parsed = parsePseudoAttributes(data()))
        m_attributes = WTF::move(*parsed);
}

void ProcessingInstruction::updateDataFromAttributes()
{
    StringBuilder builder;
    for (auto& attribute : borrow(m_attributes)) {
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append(attribute.name, "=\""_s);
        for (auto character : StringView { attribute.value }.codeUnits()) {
            switch (character) {
            case '&':
                builder.append("&amp;"_s);
                break;
            case '<':
                builder.append("&lt;"_s);
                break;
            case '>':
                builder.append("&gt;"_s);
                break;
            case '"':
                builder.append("&quot;"_s);
                break;
            default:
                builder.append(character);
                break;
            }
        }
        builder.append('"');
    }

    // Listeners for DOMCharacterDataModified can change attributes; don't reparse our own output.
    SetForScope updatingData { m_updatingDataFromAttributes, true };
    setData(builder.toString());
}

void ProcessingInstruction::setDataAndUpdate(const String& newData, unsigned offsetOfReplacedData, unsigned oldLength, unsigned newLength, UpdateLiveRanges shouldUpdateLiveRanges)
{
    if (!std::exchange(m_updatingDataFromAttributes, false))
        m_attributesDirty = true;
    CharacterData::setDataAndUpdate(newData, offsetOfReplacedData, oldLength, newLength, shouldUpdateLiveRanges);
}

bool ProcessingInstruction::hasPseudoAttributes()
{
    updateAttributesIfNeeded();
    return !m_attributes.isEmpty();
}

Vector<AtomString> ProcessingInstruction::getAttributeNames()
{
    updateAttributesIfNeeded();
    return m_attributes.map([](auto& attribute) {
        return attribute.name;
    });
}

String ProcessingInstruction::getAttribute(const AtomString& name)
{
    updateAttributesIfNeeded();
    auto index = m_attributes.findIf([&name](auto& attribute) {
        return attribute.name == name;
    });
    if (index == notFound)
        return { };
    return m_attributes[index].value;
}

ExceptionOr<void> ProcessingInstruction::setAttribute(const AtomString& name, const AtomString& value)
{
    if (!NameValidation::isValidXMLName(name))
        return Exception { ExceptionCode::InvalidCharacterError };

    for (auto character : StringView { value }.codePoints()) {
        if (!isXMLChar(character))
            return Exception { ExceptionCode::InvalidCharacterError };
    }

    updateAttributesIfNeeded();
    auto index = m_attributes.findIf([&name](auto& attribute) {
        return attribute.name == name;
    });
    if (index == notFound)
        m_attributes.append({ name, value });
    else
        m_attributes[index].value = value;
    updateDataFromAttributes();
    return { };
}

void ProcessingInstruction::removeAttribute(const AtomString& name)
{
    updateAttributesIfNeeded();
    if (!m_attributes.removeAllMatching([&name](auto& attribute) { return attribute.name == name; }))
        return;
    updateDataFromAttributes();
}

ExceptionOr<bool> ProcessingInstruction::toggleAttribute(const AtomString& name, std::optional<bool> force)
{
    if (!NameValidation::isValidXMLName(name))
        return Exception { ExceptionCode::InvalidCharacterError };

    if (hasAttribute(name)) {
        if (force && *force)
            return true;
        removeAttribute(name);
        return false;
    }
    if (force && !*force)
        return false;
    // The name was validated above, so this cannot throw.
    setAttribute(name, emptyAtom());
    return true;
}

bool ProcessingInstruction::hasAttribute(const AtomString& name)
{
    updateAttributesIfNeeded();
    return m_attributes.containsIf([&name](auto& attribute) {
        return attribute.name == name;
    });
}

void ProcessingInstruction::checkStyleSheet()
{
    Ref document = this->document();
    if (m_target == "xml-stylesheet"_s && document->frame() && parentNode() == document.ptr()) {
        // see http://www.w3.org/TR/xml-stylesheet/
        // ### support stylesheet included in a fragment of this (or another) document
        // ### make sure this gets called when adding from javascript
        auto attributes = parseAttributes(protect(document->cachedResourceLoader()), data());
        if (!attributes)
            return;
        String type = attributes->get<HashTranslatorASCIILiteral>("type"_s);

        m_isCSS = type.isEmpty() || type == cssContentTypeAtom();
#if ENABLE(XSLT)
        bool isXSLTSupported = document->settings().isXSLTEnabled();
        m_isXSL = isXSLTSupported && (type == "text/xml"_s || type == "text/xsl"_s || type == "application/xml"_s || type == "application/xhtml+xml"_s || type == "application/rss+xml"_s || type == "application/atom+xml"_s);
        if (!m_isCSS && !m_isXSL)
#else
        if (!m_isCSS)
#endif
            return;

        String href = attributes->get<HashTranslatorASCIILiteral>("href"_s);
        String alternate = attributes->get<HashTranslatorASCIILiteral>("alternate"_s);
        m_alternate = alternate == "yes"_s;
        m_title = attributes->get<HashTranslatorASCIILiteral>("title"_s);
        m_media = attributes->get<HashTranslatorASCIILiteral>("media"_s);

        if (m_alternate && m_title.isEmpty())
            return;

        if (href.length() > 1 && href[0] == '#') {
            m_localHref = href.substring(1);
#if ENABLE(XSLT)
            // We need to make a synthetic XSLStyleSheet that is embedded.  It needs to be able
            // to kick off import/include loads that can hang off some parent sheet.
            if (m_isXSL) {
                URL finalURL({ }, m_localHref);
                setSheet(XSLStyleSheet::createEmbedded(*this, finalURL));
                m_loading = false;
                document->scheduleToApplyXSLTransforms();
            }
#endif
        } else {
            if (RefPtr cachedSheet = std::exchange(m_cachedSheet, nullptr))
                cachedSheet->removeClient(*this);

            if (!m_loading) {
                m_loading = true;
                document->styleScope().addPendingSheet(*this);
            }

            ASSERT_WITH_SECURITY_IMPLICATION(!m_cachedSheet);

#if ENABLE(XSLT)
            if (m_isXSL) {
                auto options = CachedResourceLoader::defaultCachedResourceOptions();
                options.mode = FetchOptions::Mode::SameOrigin;
                if (auto result = protect(document->cachedResourceLoader())->requestXSLStyleSheet({ ResourceRequest(document->encodingParseURL(href)), options }))
                    m_cachedSheet = WTF::move(result.value());
                else
                    m_cachedSheet = nullptr;
            } else
#endif
            {
                String charset = attributes->get<HashTranslatorASCIILiteral>("charset"_s);
                CachedResourceRequest request(document->encodingParseURL(href), CachedResourceLoader::defaultCachedResourceOptions(), std::nullopt, charset.isEmpty() ? String::fromLatin1(document->charset()) : WTF::move(charset));

                if (auto result = protect(document->cachedResourceLoader())->requestCSSStyleSheet(WTF::move(request)))
                    m_cachedSheet = WTF::move(result.value());
                else
                    m_cachedSheet = nullptr;
            }
            if (RefPtr cachedSheet = m_cachedSheet)
                cachedSheet->addClient(*this);
            else {
                // The request may have been denied if (for example) the stylesheet is local and the document is remote.
                m_loading = false;
                document->styleScope().removePendingSheet(*this);
#if ENABLE(XSLT)
                if (m_isXSL)
                    document->scheduleToApplyXSLTransforms();
#endif
            }
        }
    }
}

bool ProcessingInstruction::isLoading() const
{
    if (m_loading)
        return true;
    if (RefPtr sheet = m_sheet)
        return sheet->isLoading();
    return false;
}

bool ProcessingInstruction::sheetLoaded()
{
    if (!isLoading()) {
        Ref document = this->document();
        if (CheckedRef styleScope = document->styleScope(); styleScope->hasPendingSheet(*this))
            styleScope->removePendingSheet(protect(*this));
#if ENABLE(XSLT)
        if (m_isXSL)
            document->scheduleToApplyXSLTransforms();
#endif
        return true;
    }
    return false;
}

void ProcessingInstruction::setCSSStyleSheet(const String& href, const URL& baseURL, ASCIILiteral charset, const CachedCSSStyleSheet& sheet)
{
    if (!isConnected()) {
        ASSERT(!m_sheet);
        return;
    }

    Ref document = this->document();
    ASSERT(m_isCSS);
    CSSParserContext parserContext(document, baseURL, charset);

    Ref cssSheet = CSSStyleSheet::create(StyleSheetContents::create(href, parserContext), *this, sheet.isCORSSameOrigin());
    cssSheet->setDisabled(m_alternate);
    cssSheet->setTitle(m_title);
    cssSheet->setMediaQueries(MQ::MediaQueryParser::parse(m_media, document->cssParserContext()));

    setSheet(WTF::move(cssSheet));

    // We don't need the cross-origin security check here because we are
    // getting the sheet text in "strict" mode. This enforces a valid CSS MIME
    // type.
    parseStyleSheet(sheet.sheetText().value_or(nullString()));
}

#if ENABLE(XSLT)
void ProcessingInstruction::setXSLStyleSheet(const String& href, const URL& baseURL, const String& sheet)
{
    ASSERT(m_isXSL);
    setSheet(XSLStyleSheet::create(*this, href, baseURL));
    Ref protectedDocument { document() };
    parseStyleSheet(sheet);
}
#endif

void ProcessingInstruction::parseStyleSheet(const String& sheet)
{
    Ref styleSheet = *m_sheet;
    if (m_isCSS)
        protect(downcast<CSSStyleSheet>(styleSheet.get()).contents())->parseString(sheet);
#if ENABLE(XSLT)
    else if (m_isXSL)
        downcast<XSLStyleSheet>(styleSheet.get()).parseString(sheet);
#endif

    if (RefPtr cachedSheet = std::exchange(m_cachedSheet, nullptr))
        cachedSheet->removeClient(*this);

    m_loading = false;

    if (m_isCSS)
        protect(downcast<CSSStyleSheet>(styleSheet.get()).contents())->checkLoaded();
#if ENABLE(XSLT)
    else if (m_isXSL)
        downcast<XSLStyleSheet>(styleSheet.get()).checkLoaded();
#endif
}

void ProcessingInstruction::addSubresourceAttributeURLs(OrderedHashSet<URL>& urls) const
{
    if (!sheet())
        return;
    
    addSubresourceURL(urls, protect(sheet())->baseURL());
}

Node::NeedsPostConnectionSteps ProcessingInstruction::insertionSteps(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    CharacterData::insertionSteps(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return NeedsPostConnectionSteps::No;
    protect(document().styleScope())->addStyleSheetCandidateNode(*this, m_createdByParser);
    return NeedsPostConnectionSteps::Yes;
}

void ProcessingInstruction::postConnectionSteps()
{
    checkStyleSheet();
}

void ProcessingInstruction::removingSteps(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    CharacterData::removingSteps(removalType, oldParentOfRemovedTree);
    if (!removalType.disconnectedFromDocument)
        return;
    
    Ref document = this->document();
    CheckedRef styleScope = document->styleScope();
    styleScope->removeStyleSheetCandidateNode(*this);

    setSheet(nullptr);

    if (m_loading) {
        m_loading = false;
        styleScope->removePendingSheet(*this);
    }

    styleScope->didChangeActiveStyleSheetCandidates();
}

} // namespace
