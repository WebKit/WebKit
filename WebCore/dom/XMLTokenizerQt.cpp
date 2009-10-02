/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
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
 */

#include "config.h"
#include "XMLTokenizer.h"

#include "CDATASection.h"
#include "CString.h"
#include "CachedScript.h"
#include "Comment.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLLinkElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTokenizer.h"
#include "ProcessingInstruction.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptController.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "TextResourceDecoder.h"
#include "TransformSource.h"
#include <QDebug>
#include <wtf/Platform.h>
#include <wtf/StringExtras.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#if ENABLE(XHTMLMP)
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#endif

using namespace std;

namespace WebCore {

#if QT_VERSION >= 0x040400
class EntityResolver : public QXmlStreamEntityResolver {
    virtual QString resolveUndeclaredEntity(const QString &name);
};

QString EntityResolver::resolveUndeclaredEntity(const QString &name)
{
    UChar c = decodeNamedEntity(name.toUtf8().constData());
    return QString(c);
}
#endif

// --------------------------------

XMLTokenizer::XMLTokenizer(Document* _doc, FrameView* _view)
    : m_doc(_doc)
    , m_view(_view)
    , m_wroteText(false)
    , m_currentNode(_doc)
    , m_currentNodeIsReferenced(false)
    , m_sawError(false)
    , m_sawXSLTransform(false)
    , m_sawFirstElement(false)
    , m_isXHTMLDocument(false)
#if ENABLE(XHTMLMP)
    , m_isXHTMLMPDocument(false)
    , m_hasDocTypeDeclaration(false)
#endif
    , m_parserPaused(false)
    , m_requestingScript(false)
    , m_finishCalled(false)
    , m_errorCount(0)
    , m_lastErrorLine(0)
    , m_lastErrorColumn(0)
    , m_pendingScript(0)
    , m_scriptStartLine(0)
    , m_parsingFragment(false)
{
#if QT_VERSION >= 0x040400
    m_stream.setEntityResolver(new EntityResolver);
#endif
}

XMLTokenizer::XMLTokenizer(DocumentFragment* fragment, Element* parentElement)
    : m_doc(fragment->document())
    , m_view(0)
    , m_wroteText(false)
    , m_currentNode(fragment)
    , m_currentNodeIsReferenced(fragment)
    , m_sawError(false)
    , m_sawXSLTransform(false)
    , m_sawFirstElement(false)
    , m_isXHTMLDocument(false)
#if ENABLE(XHTMLMP)
    , m_isXHTMLMPDocument(false)
    , m_hasDocTypeDeclaration(false)
#endif
    , m_parserPaused(false)
    , m_requestingScript(false)
    , m_finishCalled(false)
    , m_errorCount(0)
    , m_lastErrorLine(0)
    , m_lastErrorColumn(0)
    , m_pendingScript(0)
    , m_scriptStartLine(0)
    , m_parsingFragment(true)
{
    if (fragment)
        fragment->ref();
    if (m_doc)
        m_doc->ref();
          
    // Add namespaces based on the parent node
    Vector<Element*> elemStack;
    while (parentElement) {
        elemStack.append(parentElement);
        
        Node* n = parentElement->parentNode();
        if (!n || !n->isElementNode())
            break;
        parentElement = static_cast<Element*>(n);
    }
    
    if (elemStack.isEmpty())
        return;
    
#if QT_VERSION < 0x040400
    for (Element* element = elemStack.last(); !elemStack.isEmpty(); elemStack.removeLast()) {
        if (NamedNodeMap* attrs = element->attributes()) {
            for (unsigned i = 0; i < attrs->length(); i++) {
                Attribute* attr = attrs->attributeItem(i);
                if (attr->localName() == "xmlns")
                    m_defaultNamespaceURI = attr->value();
                else if (attr->prefix() == "xmlns")
                    m_prefixToNamespaceMap.set(attr->localName(), attr->value());
            }
        }
    }
#else
    QXmlStreamNamespaceDeclarations namespaces;
    for (Element* element = elemStack.last(); !elemStack.isEmpty(); elemStack.removeLast()) {
        if (NamedNodeMap* attrs = element->attributes()) {
            for (unsigned i = 0; i < attrs->length(); i++) {
                Attribute* attr = attrs->attributeItem(i);
                if (attr->localName() == "xmlns")
                    m_defaultNamespaceURI = attr->value();
                else if (attr->prefix() == "xmlns")
                    namespaces.append(QXmlStreamNamespaceDeclaration(attr->localName(), attr->value()));
            }
        }
    }
    m_stream.addExtraNamespaceDeclarations(namespaces);
    m_stream.setEntityResolver(new EntityResolver);
#endif

    // If the parent element is not in document tree, there may be no xmlns attribute; just default to the parent's namespace.
    if (m_defaultNamespaceURI.isNull() && !parentElement->inDocument())
        m_defaultNamespaceURI = parentElement->namespaceURI();
}

XMLTokenizer::~XMLTokenizer()
{
    setCurrentNode(0);
    if (m_parsingFragment && m_doc)
        m_doc->deref();
    if (m_pendingScript)
        m_pendingScript->removeClient(this);
#if QT_VERSION >= 0x040400
    delete m_stream.entityResolver();
#endif
}

void XMLTokenizer::doWrite(const String& parseString)
{
    m_wroteText = true;

    if (m_doc->decoder() && m_doc->decoder()->sawError()) {
        // If the decoder saw an error, report it as fatal (stops parsing)
        handleError(fatal, "Encoding error", lineNumber(), columnNumber());
        return;
    }

    QString data(parseString);
    if (!data.isEmpty()) {
#if QT_VERSION < 0x040400
        if (!m_sawFirstElement) {
            int idx = data.indexOf(QLatin1String("<?xml"));
            if (idx != -1) {
                int start = idx + 5;
                int end = data.indexOf(QLatin1String("?>"), start);
                QString content = data.mid(start, end-start);
                bool ok = true;
                HashMap<String, String> attrs = parseAttributes(content, ok);
                String version = attrs.get("version");
                String encoding = attrs.get("encoding");
                ExceptionCode ec = 0;
                if (!m_parsingFragment) {
                    if (!version.isEmpty())
                        m_doc->setXMLVersion(version, ec);
                    if (!encoding.isEmpty())
                        m_doc->setXMLEncoding(encoding);
                }
            }
        }
#endif
        m_stream.addData(data);
        parse();
    }

    return;
}

void XMLTokenizer::initializeParserContext(const char*)
{
    m_parserStopped = false;
    m_sawError = false;
    m_sawXSLTransform = false;
    m_sawFirstElement = false;
}

void XMLTokenizer::doEnd()
{
#if ENABLE(XSLT)
    if (m_sawXSLTransform) {
        m_doc->setTransformSource(new TransformSource(m_originalSourceForTransform));
        m_doc->setParsing(false); // Make the doc think it's done, so it will apply xsl sheets.
        m_doc->updateStyleSelector();
        m_doc->setParsing(true);
        m_parserStopped = true;
    }
#endif
    
    if (m_stream.error() == QXmlStreamReader::PrematureEndOfDocumentError
        || (m_wroteText && !m_sawFirstElement && !m_sawXSLTransform))
        handleError(fatal, qPrintable(m_stream.errorString()), lineNumber(), columnNumber());
}

int XMLTokenizer::lineNumber() const
{
    return m_stream.lineNumber();
}

int XMLTokenizer::columnNumber() const
{
    return m_stream.columnNumber();
}

void XMLTokenizer::stopParsing()
{
    Tokenizer::stopParsing();
}

void XMLTokenizer::resumeParsing()
{
    ASSERT(m_parserPaused);
    
    m_parserPaused = false;

    // First, execute any pending callbacks
    parse();
    if (m_parserPaused)
        return;

    // Then, write any pending data
    SegmentedString rest = m_pendingSrc;
    m_pendingSrc.clear();
    write(rest, false);

    // Finally, if finish() has been called and write() didn't result
    // in any further callbacks being queued, call end()
    if (m_finishCalled && !m_parserPaused && !m_pendingScript)
        end();
}

bool parseXMLDocumentFragment(const String& chunk, DocumentFragment* fragment, Element* parent)
{
    if (!chunk.length())
        return true;

    XMLTokenizer tokenizer(fragment, parent);
    
    tokenizer.write(String("<qxmlstreamdummyelement>"), false);
    tokenizer.write(chunk, false);
    tokenizer.write(String("</qxmlstreamdummyelement>"), false);
    tokenizer.finish();
    return !tokenizer.hasError();
}

// --------------------------------

struct AttributeParseState {
    HashMap<String, String> attributes;
    bool gotAttributes;
};

static void attributesStartElementNsHandler(AttributeParseState* state, const QXmlStreamAttributes& attrs)
{
    if (attrs.count() <= 0)
        return;

    state->gotAttributes = true;

    for (int i = 0; i < attrs.count(); i++) {
        const QXmlStreamAttribute& attr = attrs[i];
        String attrLocalName = attr.name();
        String attrValue     = attr.value();
        String attrURI       = attr.namespaceUri();
        String attrQName     = attr.qualifiedName();
        state->attributes.set(attrQName, attrValue);
    }
}

HashMap<String, String> parseAttributes(const String& string, bool& attrsOK)
{
    AttributeParseState state;
    state.gotAttributes = false;

    QXmlStreamReader stream;
    QString dummy = QString(QLatin1String("<?xml version=\"1.0\"?><attrs %1 />")).arg(string);
    stream.addData(dummy);
    while (!stream.atEnd()) {
        stream.readNext();
        if (stream.isStartElement()) {
            attributesStartElementNsHandler(&state, stream.attributes());
        }
    }
    attrsOK = state.gotAttributes;
    return state.attributes;
}

static inline String prefixFromQName(const QString& qName)
{
    const int offset = qName.indexOf(QLatin1Char(':'));
    if (offset <= 0)
        return String();
    else
        return qName.left(offset);
}

static inline void handleElementNamespaces(Element* newElement, const QXmlStreamNamespaceDeclarations &ns,
                                           ExceptionCode& ec)
{
    for (int i = 0; i < ns.count(); ++i) {
        const QXmlStreamNamespaceDeclaration &decl = ns[i];
        String namespaceURI = decl.namespaceUri();
        String namespaceQName = decl.prefix().isEmpty() ? String("xmlns") : String("xmlns:") + decl.prefix();
        newElement->setAttributeNS("http://www.w3.org/2000/xmlns/", namespaceQName, namespaceURI, ec);
        if (ec) // exception setting attributes
            return;
    }
}

static inline void handleElementAttributes(Element* newElement, const QXmlStreamAttributes &attrs, ExceptionCode& ec)
{
    for (int i = 0; i < attrs.count(); ++i) {
        const QXmlStreamAttribute &attr = attrs[i];
        String attrLocalName = attr.name();
        String attrValue     = attr.value();
        String attrURI       = attr.namespaceUri().isEmpty() ? String() : String(attr.namespaceUri());
        String attrQName     = attr.qualifiedName();
        newElement->setAttributeNS(attrURI, attrQName, attrValue, ec);
        if (ec) // exception setting attributes
            return;
    }
}

void XMLTokenizer::parse()
{
    while (!m_parserStopped && !m_parserPaused && !m_stream.atEnd()) {
        m_stream.readNext();
        switch (m_stream.tokenType()) {
        case QXmlStreamReader::StartDocument: {
            startDocument();
        }
            break;
        case QXmlStreamReader::EndDocument: {
            endDocument();
        }
            break;
        case QXmlStreamReader::StartElement: {
#if ENABLE(XHTMLMP)
            if (m_doc->isXHTMLMPDocument() && !m_hasDocTypeDeclaration) {
                handleError(fatal, "DOCTYPE declaration lost.", lineNumber(), columnNumber());
                break;
            }
#endif 
            parseStartElement();
        }
            break;
        case QXmlStreamReader::EndElement: {
            parseEndElement();
        }
            break;
        case QXmlStreamReader::Characters: {
            if (m_stream.isCDATA()) {
                //cdata
                parseCdata();
            } else {
                //characters
                parseCharacters();
            }
        }
            break;
        case QXmlStreamReader::Comment: {
            parseComment();
        }
            break;
        case QXmlStreamReader::DTD: {
            //qDebug()<<"------------- DTD";
            parseDtd();
#if ENABLE(XHTMLMP)
            m_hasDocTypeDeclaration = true;
#endif
        }
            break;
        case QXmlStreamReader::EntityReference: {
            //qDebug()<<"---------- ENTITY = "<<m_stream.name().toString()
            //        <<", t = "<<m_stream.text().toString();
            if (isXHTMLDocument()
#if ENABLE(XHTMLMP)
                || isXHTMLMPDocument()
#endif
#if ENABLE(WML)
                || isWMLDocument()
#endif
               ) {
                QString entity = m_stream.name().toString();
                UChar c = decodeNamedEntity(entity.toUtf8().constData());
                if (m_currentNode->isTextNode() || enterText()) {
                    ExceptionCode ec = 0;
                    String str(&c, 1);
                    //qDebug()<<" ------- adding entity "<<str;
                    static_cast<Text*>(m_currentNode)->appendData(str, ec);
                }
            }
        }
            break;
        case QXmlStreamReader::ProcessingInstruction: {
            parseProcessingInstruction();
        }
            break;
        default: {
            if (m_stream.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
                ErrorType type = (m_stream.error() == QXmlStreamReader::NotWellFormedError) ?
                                 fatal : warning;
                handleError(type, qPrintable(m_stream.errorString()), lineNumber(),
                            columnNumber());
            }
        }
            break;
        }
    }
}

void XMLTokenizer::startDocument()
{
    initializeParserContext();
    ExceptionCode ec = 0;

    if (!m_parsingFragment) {
        m_doc->setXMLStandalone(m_stream.isStandaloneDocument(), ec);

#if QT_VERSION >= 0x040400
        QStringRef version = m_stream.documentVersion();
        if (!version.isEmpty())
            m_doc->setXMLVersion(version, ec);
        QStringRef encoding = m_stream.documentEncoding();
        if (!encoding.isEmpty())
            m_doc->setXMLEncoding(encoding);
#endif
    }
}

void XMLTokenizer::parseStartElement()
{
    if (!m_sawFirstElement && m_parsingFragment) {
        // skip dummy element for fragments
        m_sawFirstElement = true;
        return;
    }

    exitText();

    String localName = m_stream.name();
    String uri       = m_stream.namespaceUri();
    String prefix    = prefixFromQName(m_stream.qualifiedName().toString());

    if (m_parsingFragment && uri.isNull()) {
        Q_ASSERT(prefix.isNull());
        uri = m_defaultNamespaceURI;
    }

    QualifiedName qName(prefix, localName, uri);
    RefPtr<Element> newElement = m_doc->createElement(qName, true);
    if (!newElement) {
        stopParsing();
        return;
    }

#if ENABLE(XHTMLMP)
    if (!m_sawFirstElement && isXHTMLMPDocument()) {
        // As per 7.1 section of OMA-WAP-XHTMLMP-V1_1-20061020-A.pdf, 
        // we should make sure that the root element MUST be 'html' and 
        // ensure the name of the default namespace on the root elment 'html' 
        // MUST be 'http://www.w3.org/1999/xhtml'
        if (localName != HTMLNames::htmlTag.localName()) {
            handleError(fatal, "XHTMLMP document expects 'html' as root element.", lineNumber(), columnNumber());
            return;
        } 

        if (uri.isNull()) {
            m_defaultNamespaceURI = HTMLNames::xhtmlNamespaceURI; 
            uri = m_defaultNamespaceURI;
            m_stream.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration(prefix, HTMLNames::xhtmlNamespaceURI));
        }
    }
#endif

    bool isFirstElement = !m_sawFirstElement;
    m_sawFirstElement = true;

    ExceptionCode ec = 0;
    handleElementNamespaces(newElement.get(), m_stream.namespaceDeclarations(), ec);
    if (ec) {
        stopParsing();
        return;
    }

    handleElementAttributes(newElement.get(), m_stream.attributes(), ec);
    if (ec) {
        stopParsing();
        return;
    }

    ScriptElement* scriptElement = toScriptElement(newElement.get());
    if (scriptElement)
        m_scriptStartLine = lineNumber();

    if (!m_currentNode->addChild(newElement.get())) {
        stopParsing();
        return;
    }

    setCurrentNode(newElement.get());
    if (m_view && !newElement->attached())
        newElement->attach();

    if (isFirstElement && m_doc->frame())
        m_doc->frame()->loader()->dispatchDocumentElementAvailable();
}

void XMLTokenizer::parseEndElement()
{
    exitText();

    Node* n = m_currentNode;
    RefPtr<Node> parent = n->parentNode();
    n->finishParsingChildren();

    if (!n->isElementNode() || !m_view) {
        setCurrentNode(parent.get());
        return;
    }

    Element* element = static_cast<Element*>(n);
    ScriptElement* scriptElement = toScriptElement(element);
    if (!scriptElement) {
        setCurrentNode(parent.get());
        return;
    }

    // don't load external scripts for standalone documents (for now)
    ASSERT(!m_pendingScript);
    m_requestingScript = true;

#if ENABLE(XHTMLMP)
    if (!scriptElement->shouldExecuteAsJavaScript())
        m_doc->setShouldProcessNoscriptElement(true);
    else
#endif
    {
        String scriptHref = scriptElement->sourceAttributeValue();
        if (!scriptHref.isEmpty()) {
            // we have a src attribute 
            String scriptCharset = scriptElement->scriptCharset();
            if ((m_pendingScript = m_doc->docLoader()->requestScript(scriptHref, scriptCharset))) {
                m_scriptElement = element;
                m_pendingScript->addClient(this);

                // m_pendingScript will be 0 if script was already loaded and ref() executed it
                if (m_pendingScript)
                    pauseParsing();
            } else 
                m_scriptElement = 0;
        } else
            m_view->frame()->loader()->executeScript(ScriptSourceCode(scriptElement->scriptContent(), m_doc->url(), m_scriptStartLine));
    }
    m_requestingScript = false;
    setCurrentNode(parent.get());
}

void XMLTokenizer::parseCharacters()
{
    if (m_currentNode->isTextNode() || enterText()) {
        ExceptionCode ec = 0;
        static_cast<Text*>(m_currentNode)->appendData(m_stream.text(), ec);
    }
}

void XMLTokenizer::parseProcessingInstruction()
{
    exitText();

    // ### handle exceptions
    int exception = 0;
    RefPtr<ProcessingInstruction> pi = m_doc->createProcessingInstruction(
        m_stream.processingInstructionTarget(),
        m_stream.processingInstructionData(), exception);
    if (exception)
        return;

    pi->setCreatedByParser(true);

    if (!m_currentNode->addChild(pi.get()))
        return;
    if (m_view && !pi->attached())
        pi->attach();

    pi->finishParsingChildren();

#if ENABLE(XSLT)
    m_sawXSLTransform = !m_sawFirstElement && pi->isXSL();
    if (m_sawXSLTransform && !m_doc->transformSourceDocument())
        stopParsing();
#endif
}

void XMLTokenizer::parseCdata()
{
    exitText();

    RefPtr<Node> newNode = CDATASection::create(m_doc, m_stream.text());
    if (!m_currentNode->addChild(newNode.get()))
        return;
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::parseComment()
{
    exitText();

    RefPtr<Node> newNode = Comment::create(m_doc, m_stream.text());
    m_currentNode->addChild(newNode.get());
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::endDocument()
{
#if ENABLE(XHTMLMP)
    m_hasDocTypeDeclaration = false;
#endif
}

bool XMLTokenizer::hasError() const
{
    return m_stream.hasError();
}

#if QT_VERSION < 0x040400
static QString parseId(const QString &dtd, int *pos, bool *ok)
{
    *ok = true;
    int start = *pos + 1;
    int end = start;
    if (dtd.at(*pos) == QLatin1Char('\''))
        while (start < dtd.length() && dtd.at(end) != QLatin1Char('\''))
            ++end;
    else if (dtd.at(*pos) == QLatin1Char('\"'))
        while (start < dtd.length() && dtd.at(end) != QLatin1Char('\"'))
            ++end;
    else {
        *ok = false;
        return QString();
    }
    *pos = end + 1;
    return dtd.mid(start, end - start);
}
#endif

void XMLTokenizer::parseDtd()
{
#if QT_VERSION >= 0x040400
    QStringRef name = m_stream.dtdName();
    QStringRef publicId = m_stream.dtdPublicId();
    QStringRef systemId = m_stream.dtdSystemId();
#else
    QString dtd = m_stream.text().toString();

    int start = dtd.indexOf("<!DOCTYPE ") + 10;
    while (start < dtd.length() && dtd.at(start).isSpace())
        ++start;
    int end = start;
    while (start < dtd.length() && !dtd.at(end).isSpace())
        ++end;
    QString name = dtd.mid(start, end - start);

    start = end;
    while (start < dtd.length() && dtd.at(start).isSpace())
        ++start;
    end = start;
    while (start < dtd.length() && !dtd.at(end).isSpace())
        ++end;
    QString id = dtd.mid(start, end - start);
    start = end;
    while (start < dtd.length() && dtd.at(start).isSpace())
        ++start;
    QString publicId;
    QString systemId;
    if (id == QLatin1String("PUBLIC")) {
        bool ok;
        publicId = parseId(dtd, &start, &ok);
        if (!ok) {
            handleError(fatal, "Invalid DOCTYPE", lineNumber(), columnNumber());
            return;
        }
        while (start < dtd.length() && dtd.at(start).isSpace())
            ++start;
        systemId = parseId(dtd, &start, &ok);
        if (!ok) {
            handleError(fatal, "Invalid DOCTYPE", lineNumber(), columnNumber());
            return;
        }
    } else if (id == QLatin1String("SYSTEM")) {
        bool ok;
        systemId = parseId(dtd, &start, &ok);
        if (!ok) {
            handleError(fatal, "Invalid DOCTYPE", lineNumber(), columnNumber());
            return;
        }
    } else if (id == QLatin1String("[") || id == QLatin1String(">")) {
    } else {
        handleError(fatal, "Invalid DOCTYPE", lineNumber(), columnNumber());
        return;
    }
#endif    

    //qDebug() << dtd << name << publicId << systemId;
    if ((publicId == QLatin1String("-//W3C//DTD XHTML 1.0 Transitional//EN"))
        || (publicId == QLatin1String("-//W3C//DTD XHTML 1.1//EN"))
        || (publicId == QLatin1String("-//W3C//DTD XHTML 1.0 Strict//EN"))
        || (publicId == QLatin1String("-//W3C//DTD XHTML 1.0 Frameset//EN"))
        || (publicId == QLatin1String("-//W3C//DTD XHTML Basic 1.0//EN"))
        || (publicId == QLatin1String("-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN"))
        || (publicId == QLatin1String("-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN"))
#if !ENABLE(XHTMLMP)
        || (publicId == QLatin1String("-//WAPFORUM//DTD XHTML Mobile 1.0//EN"))
#endif
       )
        setIsXHTMLDocument(true); // controls if we replace entities or not.
#if ENABLE(XHTMLMP)
    else if ((publicId == QLatin1String("-//WAPFORUM//DTD XHTML Mobile 1.1//EN"))
             || (publicId == QLatin1String("-//WAPFORUM//DTD XHTML Mobile 1.0//EN"))) {
        if (AtomicString(name) != HTMLNames::htmlTag.localName()) {
            handleError(fatal, "Invalid DOCTYPE declaration, expected 'html' as root element.", lineNumber(), columnNumber());
            return;
        } 

        if (m_doc->isXHTMLMPDocument()) // check if the MIME type is correct with this method
            setIsXHTMLMPDocument(true);
        else
            setIsXHTMLDocument(true);
    }
#endif
#if ENABLE(WML)
    else if (m_doc->isWMLDocument()
             && publicId != QLatin1String("-//WAPFORUM//DTD WML 1.3//EN")
             && publicId != QLatin1String("-//WAPFORUM//DTD WML 1.2//EN")
             && publicId != QLatin1String("-//WAPFORUM//DTD WML 1.1//EN")
             && publicId != QLatin1String("-//WAPFORUM//DTD WML 1.0//EN"))
        handleError(fatal, "Invalid DTD Public ID", lineNumber(), columnNumber());
#endif
    if (!m_parsingFragment)
        m_doc->addChild(DocumentType::create(m_doc, name, publicId, systemId));
    
}
}

