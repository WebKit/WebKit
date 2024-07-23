/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "LocalFrameView.h"
#include "ParserContentPolicy.h"
#include "PendingScriptClient.h"
#include "ScriptableDocumentParser.h"
#include "SegmentedString.h"
#include "XMLErrors.h"
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <wtf/CheckedRef.h>
#include <wtf/HashMap.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/CString.h>

namespace WebCore {

class ContainerNode;
class CachedResourceLoader;
class DocumentFragment;
class Element;
class PendingCallbacks;
class Text;

class XMLParserContext : public RefCounted<XMLParserContext> {
public:
    static RefPtr<XMLParserContext> createMemoryParser(xmlSAXHandlerPtr, void* userData, const CString& chunk);
    static Ref<XMLParserContext> createStringParser(xmlSAXHandlerPtr, void* userData);
    XMLParserContext() = delete;
    ~XMLParserContext();
    xmlParserCtxtPtr context() const { return m_context; }

private:
    XMLParserContext(xmlParserCtxtPtr context)
        : m_context(context)
    {
    }
    xmlParserCtxtPtr m_context;
};

class XMLDocumentParser final : public ScriptableDocumentParser, public PendingScriptClient, public CanMakeCheckedPtr<XMLDocumentParser> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(XMLDocumentParser);
public:
    enum class IsInFrameView : bool { No, Yes };
    static Ref<XMLDocumentParser> create(Document& document, IsInFrameView isInFrameView, OptionSet<ParserContentPolicy> policy = DefaultParserContentPolicy)
    {
        return adoptRef(*new XMLDocumentParser(document, isInFrameView, policy));
    }
    static Ref<XMLDocumentParser> create(DocumentFragment& fragment, HashMap<AtomString, AtomString>&& prefixToNamespaceMap, const AtomString& defaultNamespaceURI, OptionSet<ParserContentPolicy> parserContentPolicy)
    {
        return adoptRef(*new XMLDocumentParser(fragment, WTFMove(prefixToNamespaceMap), defaultNamespaceURI, parserContentPolicy));
    }

    XMLDocumentParser() = delete;
    ~XMLDocumentParser();

    // Exposed for callbacks:
    void handleError(XMLErrors::ErrorType, const char* message, TextPosition);

    void setIsXHTMLDocument(bool isXHTML) { m_isXHTMLDocument = isXHTML; }
    bool isXHTMLDocument() const { return m_isXHTMLDocument; }

    static bool parseDocumentFragment(const String&, DocumentFragment&, Element* parent = nullptr, OptionSet<ParserContentPolicy> = { ParserContentPolicy::AllowScriptingContent });

    // Used by XMLHttpRequest to check if the responseXML was well formed.
    bool wellFormed() const final { return !m_sawError; }

    static bool supportsXMLVersion(const String&);

private:
    explicit XMLDocumentParser(Document&, IsInFrameView, OptionSet<ParserContentPolicy>);
    XMLDocumentParser(DocumentFragment&, HashMap<AtomString, AtomString>&&, const AtomString&, OptionSet<ParserContentPolicy>);

    // CheckedPtr interface
    uint32_t ptrCount() const final { return CanMakeCheckedPtr::ptrCount(); }
    uint32_t ptrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::ptrCountWithoutThreadCheck(); }
    void incrementPtrCount() const final { CanMakeCheckedPtr::incrementPtrCount(); }
    void decrementPtrCount() const final { CanMakeCheckedPtr::decrementPtrCount(); }

    void insert(SegmentedString&&) final;
    void append(RefPtr<StringImpl>&&) final;
    void finish() final;
    void stopParsing() final;
    void detach() final;

    TextPosition textPosition() const final;
    bool shouldAssociateConsoleMessagesWithTextPosition() const final;

    void notifyFinished(PendingScript&) final;

    void end();

    void pauseParsing();
    void resumeParsing();

    bool appendFragmentSource(const String&);

public:
    // Callbacks from parser SAX, and other functions needed inside
    // the parser implementation, but outside this class.

    void error(XMLErrors::ErrorType, const char* message, va_list args) WTF_ATTRIBUTE_PRINTF(3, 0);
    void startElementNs(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI,
        int numNamespaces, const xmlChar** namespaces,
        int numAttributes, int numDefaulted, const xmlChar** libxmlAttributes);
    void endElementNs();
    void characters(std::span<const xmlChar>);
    void processingInstruction(const xmlChar* target, const xmlChar* data);
    void cdataBlock(const xmlChar*, int length);
    void comment(const xmlChar*);
    void startDocument(const xmlChar* version, const xmlChar* encoding, int standalone);
    void internalSubset(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID);
    void endDocument();

private:
    void initializeParserContext(const CString& chunk = CString());

    void pushCurrentNode(ContainerNode*);
    void popCurrentNode();
    void clearCurrentNodeStack();

    void insertErrorMessageBlock();

    void createLeafTextNode();
    bool updateLeafTextNode();

    void doWrite(const String&);
    void doEnd();

    xmlParserCtxtPtr context() const { return m_context ? m_context->context() : nullptr; };

    IsInFrameView m_isInFrameView { IsInFrameView::No };

    SegmentedString m_originalSourceForTransform;

    RefPtr<XMLParserContext> m_context;
    std::unique_ptr<PendingCallbacks> m_pendingCallbacks;
    Vector<xmlChar> m_bufferedText;

    ContainerNode* m_currentNode { nullptr };
    Vector<ContainerNode*> m_currentNodeStack;

    RefPtr<Text> m_leafTextNode;

    bool m_sawError { false };
    bool m_sawCSS { false };
    bool m_sawXSLTransform { false };
    bool m_sawFirstElement { false };
    bool m_isXHTMLDocument { false };
    bool m_parserPaused { false };
    bool m_requestingScript { false };
    bool m_finishCalled { false };

    std::unique_ptr<XMLErrors> m_xmlErrors;

    RefPtr<PendingScript> m_pendingScript;
    TextPosition m_scriptStartPosition;

    bool m_parsingFragment { false };

    HashMap<AtomString, AtomString> m_prefixToNamespaceMap;
    AtomString m_defaultNamespaceURI;

    SegmentedString m_pendingSrc;
};

#if ENABLE(XSLT)
xmlDocPtr xmlDocPtrForString(CachedResourceLoader&, const String& source, const String& url);
#endif

xmlParserInputPtr externalEntityLoader(const char* url, const char* id, xmlParserCtxtPtr);
void initializeXMLParser();

std::optional<HashMap<String, String>> parseAttributes(CachedResourceLoader&, const String&);

} // namespace WebCore
