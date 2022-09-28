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

#include "FragmentScriptingPermission.h"
#include "PendingScriptClient.h"
#include "ScriptableDocumentParser.h"
#include "SegmentedString.h"
#include "XMLErrors.h"
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <wtf/HashMap.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/CString.h>

namespace WebCore {

class ContainerNode;
class CachedResourceLoader;
class DocumentFragment;
class Element;
class FrameView;
class PendingCallbacks;
class Text;

class XMLParserContext : public RefCounted<XMLParserContext> {
public:
    static RefPtr<XMLParserContext> createMemoryParser(xmlSAXHandlerPtr, void* userData, const CString& chunk);
    static Ref<XMLParserContext> createStringParser(xmlSAXHandlerPtr, void* userData);
    ~XMLParserContext();
    xmlParserCtxtPtr context() const { return m_context; }

private:
    XMLParserContext(xmlParserCtxtPtr context)
        : m_context(context)
    {
    }
    xmlParserCtxtPtr m_context;
};

class XMLDocumentParser final : public ScriptableDocumentParser, public PendingScriptClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<XMLDocumentParser> create(Document& document, FrameView* view, OptionSet<ParserContentPolicy> policy = DefaultParserContentPolicy)
    {
        return adoptRef(*new XMLDocumentParser(document, view, policy));
    }
    static Ref<XMLDocumentParser> create(DocumentFragment& fragment, HashMap<AtomString, AtomString>&& prefixToNamespaceMap, const AtomString& defaultNamespaceURI, OptionSet<ParserContentPolicy> parserContentPolicy)
    {
        return adoptRef(*new XMLDocumentParser(fragment, WTFMove(prefixToNamespaceMap), defaultNamespaceURI, parserContentPolicy));
    }

    ~XMLDocumentParser();

    // Exposed for callbacks:
    void handleError(XMLErrors::ErrorType, const char* message, TextPosition);

    void setIsXHTMLDocument(bool isXHTML) { m_isXHTMLDocument = isXHTML; }
    bool isXHTMLDocument() const { return m_isXHTMLDocument; }

    static bool parseDocumentFragment(const String&, DocumentFragment&, Element* parent = nullptr, OptionSet<ParserContentPolicy> = { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });

    // Used by XMLHttpRequest to check if the responseXML was well formed.
    bool wellFormed() const final { return !m_sawError; }

    static bool supportsXMLVersion(const String&);

private:
    explicit XMLDocumentParser(Document&, FrameView*, OptionSet<ParserContentPolicy>);
    XMLDocumentParser(DocumentFragment&, HashMap<AtomString, AtomString>&&, const AtomString&, OptionSet<ParserContentPolicy>);

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
    void characters(const xmlChar*, int length);
    void processingInstruction(const xmlChar* target, const xmlChar* data);
    void cdataBlock(const xmlChar*, int length);
    void comment(const xmlChar*);
    void startDocument(const xmlChar* version, const xmlChar* encoding, int standalone);
    void internalSubset(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID);
    void endDocument();

    bool isParsingEntityDeclaration() const { return m_isParsingEntityDeclaration; }
    void setIsParsingEntityDeclaration(bool value) { m_isParsingEntityDeclaration = value; }

    int depthTriggeringEntityExpansion() const { return m_depthTriggeringEntityExpansion; }
    void setDepthTriggeringEntityExpansion(int depth) { m_depthTriggeringEntityExpansion = depth; }

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

    FrameView* m_view { nullptr };

    SegmentedString m_originalSourceForTransform;

    RefPtr<XMLParserContext> m_context;
    std::unique_ptr<PendingCallbacks> m_pendingCallbacks;
    Vector<xmlChar> m_bufferedText;
    int m_depthTriggeringEntityExpansion { -1 };
    bool m_isParsingEntityDeclaration { false };

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

std::optional<HashMap<String, String>> parseAttributes(const String&);

} // namespace WebCore
