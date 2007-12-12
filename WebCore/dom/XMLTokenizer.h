/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2007 Trolltech ASA
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

#ifndef XMLTokenizer_h
#define XMLTokenizer_h

#include "CachedResourceClient.h"
#include "SegmentedString.h"
#include "StringHash.h"
#include "Tokenizer.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

#if PLATFORM(QT)
#if !ENABLE(XSLT)
#define USE_QXMLSTREAM
#endif
#endif

#ifndef USE_QXMLSTREAM
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#else
#include <QtXml/qxmlstream.h>
#endif

namespace WebCore {

    class Node;
    class CachedScript;
    class DocLoader;
    class DocumentFragment;
    class Document;
    class Element;
    class FrameView;
    class PendingCallbacks;

    class XMLTokenizer : public Tokenizer, public CachedResourceClient {
    public:
        XMLTokenizer(Document*, FrameView* = 0);
        XMLTokenizer(DocumentFragment*, Element*);
        ~XMLTokenizer();

        enum ErrorType { warning, nonFatal, fatal };

        // from Tokenizer
        virtual bool write(const SegmentedString&, bool appendData);
        virtual void finish();
        virtual bool isWaitingForScripts() const;
        virtual void stopParsing();

        void end();

        void pauseParsing();
        void resumeParsing();

        void setIsXHTMLDocument(bool isXHTML) { m_isXHTMLDocument = isXHTML; }
        bool isXHTMLDocument() const { return m_isXHTMLDocument; }

        // from CachedResourceClient
        virtual void notifyFinished(CachedResource* finishedObj);

#ifndef USE_QXMLSTREAM
        // callbacks from parser SAX
        void error(ErrorType, const char* message, va_list args) WTF_ATTRIBUTE_PRINTF(3, 0); 
        void startElementNs(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int nb_namespaces,
                            const xmlChar** namespaces, int nb_attributes, int nb_defaulted, const xmlChar** libxmlAttributes);
        void endElementNs();
        void characters(const xmlChar* s, int len);
        void processingInstruction(const xmlChar* target, const xmlChar* data);
        void cdataBlock(const xmlChar* s, int len);
        void comment(const xmlChar* s);
        void startDocument(const xmlChar* version, const xmlChar* encoding, int standalone);
        void internalSubset(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID);
        void endDocument();
#else
        void parse();
        void startDocument();
        void parseStartElement();
        void parseEndElement();
        void parseCharacters();
        void parseProcessingInstruction();
        void parseCdata();
        void parseComment();
        void endDocument();
        void parseDtd();
        bool hasError() const;
#endif

        void handleError(ErrorType type, const char* m, int lineNumber, int columnNumber);

        virtual bool wellFormed() const { return !m_sawError; }

        int lineNumber() const;
        int columnNumber() const;

    private:
        void initializeParserContext();
        void setCurrentNode(Node*);

        void insertErrorMessageBlock();

        bool enterText();
        void exitText();

        Document* m_doc;
        FrameView* m_view;

        String m_originalSourceForTransform;

#ifdef USE_QXMLSTREAM
        QXmlStreamReader m_stream;
        bool m_wroteText;
#else
        xmlParserCtxtPtr m_context;
#endif
        Node* m_currentNode;
        bool m_currentNodeIsReferenced;

        bool m_sawError;
        bool m_sawXSLTransform;
        bool m_sawFirstElement;
        bool m_isXHTMLDocument;

        bool m_parserPaused;
        bool m_requestingScript;
        bool m_finishCalled;

        int m_errorCount;
        int m_lastErrorLine;
        int m_lastErrorColumn;
        String m_errorMessages;

        CachedScript* m_pendingScript;
        RefPtr<Element> m_scriptElement;
        int m_scriptStartLine;

        bool m_parsingFragment;
        String m_defaultNamespaceURI;

        typedef HashMap<String, String> PrefixForNamespaceMap;
        PrefixForNamespaceMap m_prefixToNamespaceMap;
#ifndef USE_QXMLSTREAM
        OwnPtr<PendingCallbacks> m_pendingCallbacks;
        Vector<xmlChar> m_bufferedText;
#endif
        SegmentedString m_pendingSrc;
    };

#if ENABLE(XSLT)
void* xmlDocPtrForString(DocLoader*, const String& source, const DeprecatedString& url);
void setLoaderForLibXMLCallbacks(DocLoader*);
#endif

HashMap<String, String> parseAttributes(const String&, bool& attrsOK);
bool parseXMLDocumentFragment(const String&, DocumentFragment*, Element* parent = 0);

} // namespace WebCore

#endif // XMLTokenizer_h
