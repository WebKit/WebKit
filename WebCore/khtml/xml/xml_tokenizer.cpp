/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "xml_tokenizer.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom_xmlimpl.h"
#include "html/html_headimpl.h"
#include "html/html_tableimpl.h"
#include "htmlnames.h"
#include "misc/htmlattrs.h"
#include "misc/loader.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include <kdebug.h>
#include <klocale.h>

#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include <qptrstack.h>

using DOM::DocumentImpl;
using DOM::DocumentPtr;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::HTMLNames;
using DOM::HTMLScriptElementImpl;
using DOM::HTMLTableSectionElementImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::ProcessingInstructionImpl;
using DOM::TextImpl;

namespace khtml {

const int maxErrors = 25;

// FIXME: Move to the newer libxml API that handles namespaces and dump XMLNamespace, XMLAttributes, and XMLNamespaceStack.

struct XMLNamespace {
    QString m_prefix;
    QString m_uri;
    XMLNamespace* m_parent;
    
    int m_ref;
    
    XMLNamespace() :m_parent(0), m_ref(0) {}
    
    XMLNamespace(const QString& p, const QString& u, XMLNamespace* parent) 
        :m_prefix(p),
         m_uri(u),
         m_parent(parent), 
         m_ref(0) 
    { 
        if (m_parent) m_parent->ref();
    }
    
    QString uriForPrefix(const QString& prefix) {
        if (prefix == m_prefix)
            return m_uri;
        if (m_parent)
            return m_parent->uriForPrefix(prefix);
        return "";
    }
    
    void ref() { m_ref++; }
    void deref() { if (--m_ref == 0) { if (m_parent) m_parent->deref(); delete this; } }
};

class XMLAttributes {
public:
    XMLAttributes() : _ref(0), _length(0), _names(0), _values(0), _uris(0) { }
    XMLAttributes(const char **expatStyleAttributes);
    ~XMLAttributes();
    
    XMLAttributes(const XMLAttributes &);
    XMLAttributes &operator=(const XMLAttributes &);
    
    int length() const { return _length; }
    QString qName(int index) const { return _names[index]; }
    QString localName(int index) const;
    QString uri(int index) const { if (!_uris) return QString::null; return _uris[index]; }
    QString value(int index) const { return _values[index]; }

    QString value(const QString &) const;

    void split(XMLNamespace* ns);
    
private:
    mutable int *_ref;
    int _length;
    QString *_names;
    QString *_values;
    QString *_uris;
};

class XMLNamespaceStack
{
public:
    ~XMLNamespaceStack();
    XMLNamespace *pushNamespaces(XMLAttributes& attributes);
    void popNamespaces();
private:
    QPtrStack<XMLNamespace> m_namespaceStack;
};

class XMLTokenizer : public Tokenizer, public CachedObjectClient
{
public:
    XMLTokenizer(DocumentPtr *, KHTMLView * = 0);
    ~XMLTokenizer();

    enum ErrorType { warning, nonFatal, fatal };

    // from Tokenizer
    virtual void write(const TokenizerString &str, bool);
    virtual void finish();
    virtual void setOnHold(bool onHold);
    virtual bool isWaitingForScripts() const;

#ifdef KHTML_XSLT
    void setTransformSource(DocumentImpl* doc);
#endif

    // from CachedObjectClient
    virtual void notifyFinished(CachedObject *finishedObj);

    // callbacks from parser SAX
    void error(ErrorType, const char *message, va_list args);
    void startElement(const xmlChar *name, const xmlChar **libxmlAttributes);
    void endElement();
    void characters(const xmlChar *s, int len);
    void processingInstruction(const xmlChar *target, const xmlChar *data);
    void cdataBlock(const xmlChar *s, int len);
    void comment(const xmlChar *s);

private:
    void end();

    int lineNumber() const;
    int columnNumber() const;
    void stopParsing();

    void insertErrorMessageBlock();

    void executeScripts();
    void addScripts(NodeImpl *n);

    XMLNamespace *pushNamespaces(XMLAttributes& attributes) { return m_namespaceStack.pushNamespaces(attributes); }
    void popNamespaces() { m_namespaceStack.popNamespaces(); }

    bool enterText();
    void exitText();

    DocumentPtr *m_doc;
    KHTMLView *m_view;

    QString m_xmlCode;

    xmlParserCtxtPtr m_context;
    DOM::NodeImpl *m_currentNode;
    XMLNamespaceStack m_namespaceStack;

    bool m_sawError;
    bool m_parserStopped;
    bool m_sawXSLTransform;
    
    int m_errorCount;
    int m_lastErrorLine;
    int m_lastErrorColumn;
    DOMString m_errorMessages;

    QPtrList<HTMLScriptElementImpl> m_scripts;
    QPtrListIterator<HTMLScriptElementImpl> *m_scriptsIt;
    CachedScript *m_cachedScript;
};

// --------------------------------

static int globalDescriptor = 0;

static int matchFunc(const char* uri)
{
    return 1; // Match everything.
}

static void* openFunc(const char * uri) {
    return &globalDescriptor;
}

static int readFunc(void* context, char* buffer, int len)
{
    // Always just do 0-byte reads
    return 0;
}

static int writeFunc(void* context, const char* buffer, int len)
{
    // Always just do 0-byte writes
    return 0;
}

static xmlParserCtxtPtr createQStringParser(xmlSAXHandlerPtr handlers, void *userData, const char* uri = NULL)
{
    static bool didInit = false;
    if (!didInit) {
        xmlInitParser();
        xmlRegisterInputCallbacks(matchFunc, openFunc, readFunc, NULL);
        xmlRegisterOutputCallbacks(matchFunc, openFunc, writeFunc, NULL);
        didInit = true;
    }

    xmlParserCtxtPtr parser = xmlCreatePushParserCtxt(handlers, userData, NULL, 0, uri);
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    xmlSwitchEncoding(parser, BOMHighByte == 0xFF ? XML_CHAR_ENCODING_UTF16LE : XML_CHAR_ENCODING_UTF16BE);
    return parser;
}

static void parseQString(xmlParserCtxtPtr parser, const QString &string)
{
    xmlParseChunk(parser,
        reinterpret_cast<const char *>(string.unicode()),
        string.length() * sizeof(QChar), 1);
}

// --------------------------------

XMLTokenizer::XMLTokenizer(DocumentPtr *_doc, KHTMLView *_view)
    : m_doc(_doc), m_view(_view),
      m_context(NULL), m_currentNode(m_doc->document()),
      m_sawError(false), m_parserStopped(false), m_errorCount(0),
      m_lastErrorLine(0), m_scriptsIt(0), m_cachedScript(0)
{
    if (m_doc)
        m_doc->ref();
    
    //FIXME: XMLTokenizer should use this in a fashion similiar to how
    //HTMLTokenizer uses loadStopped, in the future.
    loadStopped = false;
}

XMLTokenizer::~XMLTokenizer()
{
    if (m_doc)
        m_doc->deref();
    delete m_scriptsIt;
    if (m_cachedScript)
        m_cachedScript->deref(this);
}

void XMLTokenizer::write(const TokenizerString &s, bool /*appendData*/ )
{
    m_xmlCode += s.toString();
}

void XMLTokenizer::setOnHold(bool onHold)
{
    // Will we need to implement this when we do incremental XML parsing?
}

void XMLTokenizer::startElement(const xmlChar *name, const xmlChar **libxmlAttributes)
{
    if (m_parserStopped)
        return;

    XMLAttributes atts(reinterpret_cast<const char **>(libxmlAttributes));
    XMLNamespace *ns = pushNamespaces(atts);
    atts.split(ns);
    
    QString qName = QString::fromUtf8(reinterpret_cast<const char *>(name));
    QString uri;
    QString prefix;
    int colonPos = qName.find(':');
    if (colonPos != -1) {
        prefix = qName.left(colonPos);
    }
    uri = ns->uriForPrefix(prefix);
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();

    int exceptioncode = 0;
    ElementImpl *newElement = m_doc->document()->createElementNS(uri, qName, exceptioncode);
    if (!newElement)
        return;

    int i;
    for (i = 0; i < atts.length(); i++) {
        // FIXME: qualified name not supported for attributes! The prefix has been lost.
        DOMString uri(atts.uri(i));
        DOMString ln(atts.localName(i));
        DOMString val(atts.value(i));
        NodeImpl::Id id = m_doc->document()->attrId(uri.implementation(),
                                                    ln.implementation(),
                                                    false /* allocate */);
        newElement->setAttribute(id, val.implementation(), exceptioncode);
        if (exceptioncode) // exception setting attributes
            return;
    }

    // FIXME: This hack ensures implicit table bodies get constructed in XHTML and XML files.
    // We want to consolidate this with the HTML parser and HTML DOM code at some point.
    // For now, it's too risky to rip that code up.
    if (m_currentNode->hasTagName(HTMLNames::table()) &&
        newElement->hasTagName(HTMLNames::tr()) &&
        m_currentNode->isHTMLElement() && newElement->isHTMLElement()) {
        NodeImpl* implicitTBody =
           new HTMLTableSectionElementImpl(HTMLNames::tbody(), m_doc, true /* implicit */);
        m_currentNode->addChild(implicitTBody);
        if (m_view && !implicitTBody->attached())
            implicitTBody->attach();
        m_currentNode = implicitTBody;
    }

    if (newElement->hasTagName(HTMLNames::script()))
        static_cast<HTMLScriptElementImpl *>(newElement)->setCreatedByParser(true);

    if (m_currentNode->addChild(newElement)) {
        if (m_view && !newElement->attached())
            newElement->attach();
        m_currentNode = newElement;
        return;
    }
    else {
        delete newElement;
        return;
    }

    // ### DOM spec states: "if there is no markup inside an element's content, the text is contained in a
    // single object implementing the Text interface that is the only child of the element."... do we
    // need to ensure that empty elements always have an empty text child?
}

void XMLTokenizer::endElement()
{
    if (m_parserStopped) return;
    
    popNamespaces();

    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    if (m_currentNode->parentNode() != 0) {
        do {
            m_currentNode = m_currentNode->parentNode();
        } while (m_currentNode && m_currentNode->implicitNode());
    }
// ###  else error
}

void XMLTokenizer::characters(const xmlChar *s, int len)
{
    if (m_parserStopped) return;
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE ||
        m_currentNode->nodeType() == Node::CDATA_SECTION_NODE ||
        enterText()) {

        int exceptioncode = 0;
        static_cast<TextImpl*>(m_currentNode)->appendData(QString::fromUtf8(reinterpret_cast<const char *>(s), len),
            exceptioncode);
    }
}

bool XMLTokenizer::enterText()
{
    NodeImpl *newNode = m_doc->document()->createTextNode("");
    if (m_currentNode->addChild(newNode)) {
        m_currentNode = newNode;
        return true;
    }
    else {
        delete newNode;
        return false;
    }
}

void XMLTokenizer::exitText()
{
    if (m_view && m_currentNode && !m_currentNode->attached())
        m_currentNode->attach();
    
    NodeImpl* par = m_currentNode->parentNode();
    if (par != 0)
        m_currentNode = par;
}

void XMLTokenizer::error(ErrorType type, const char *message, va_list args)
{
    if (m_parserStopped) {
        return;
    }

    if (type == fatal || (m_errorCount < maxErrors && m_lastErrorLine != lineNumber() && m_lastErrorColumn != columnNumber())) {

        QString format;
        switch (type) {
            case warning:
#if APPLE_CHANGES
                format = QString("warning on line %2 at column %3: %1");
#else
                format = i18n( "warning: %1 in line %2, column %3\n" );
#endif
                break;
            case fatal:
#if APPLE_CHANGES
                // fall through
#else
                format = i18n( "fatal error: %1 in line %2, column %3\n" );
                break;
#endif
            default:
#if APPLE_CHANGES
                format = QString("error on line %2 at column %3: %1");
#else
                format = i18n( "error: %1 in line %2, column %3\n" );
#endif
        }

        char *m;
        vasprintf(&m, message, args);
        m_errorMessages += format.arg(m).arg(lineNumber()).arg(columnNumber());
        free(m);

        m_lastErrorLine = lineNumber();
        m_lastErrorColumn = columnNumber();
        ++m_errorCount;
    }

    if (type != warning)
        m_sawError = true;

    if (type == fatal)
        stopParsing();
}

void XMLTokenizer::processingInstruction(const xmlChar *target, const xmlChar *data)
{
    if (m_parserStopped) {
        return;
    }

    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    // ### handle exceptions
    ProcessingInstructionImpl *pi = m_doc->document()->createProcessingInstruction(
        QString::fromUtf8(reinterpret_cast<const char *>(target)),
        QString::fromUtf8(reinterpret_cast<const char *>(data)));
    m_currentNode->addChild(pi);
    // don't load stylesheets for standalone documents
    if (m_doc->document()->part()) {
	m_sawXSLTransform = !pi->checkStyleSheet();
        if (m_sawXSLTransform)
            // Stop the SAX parser.
            stopParsing();
    }
}

void XMLTokenizer::cdataBlock(const xmlChar *s, int len)
{
    if (m_parserStopped) {
        return;
    }

    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();

    NodeImpl *newNode = m_doc->document()->createCDATASection("");
    if (m_currentNode->addChild(newNode)) {
        if (m_view && !newNode->attached())
            newNode->attach();
        m_currentNode = newNode;
    }
    else {
        delete newNode;
        return;
    }

    characters(s, len);

    if (m_currentNode->parentNode() != 0)
        m_currentNode = m_currentNode->parentNode();
}

void XMLTokenizer::comment(const xmlChar *s)
{
    if (m_parserStopped) return;
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    // ### handle exceptions
    m_currentNode->addChild(m_doc->document()->createComment(QString::fromUtf8(reinterpret_cast<const char *>(s))));
}

static void startElementHandler(void *userData, const xmlChar *name, const xmlChar **libxmlAttributes)
{
    static_cast<XMLTokenizer *>(userData)->startElement(name, libxmlAttributes);
}

static void endElementHandler(void *userData, const xmlChar *name)
{
    static_cast<XMLTokenizer *>(userData)->endElement();
}

static void charactersHandler(void *userData, const xmlChar *s, int len)
{
    static_cast<XMLTokenizer *>(userData)->characters(s, len);
}

static void processingInstructionHandler(void *userData, const xmlChar *target, const xmlChar *data)
{
    static_cast<XMLTokenizer *>(userData)->processingInstruction(target, data);
}

static void cdataBlockHandler(void *userData, const xmlChar *s, int len)
{
    static_cast<XMLTokenizer *>(userData)->cdataBlock(s, len);
}

static void commentHandler(void *userData, const xmlChar *comment)
{
    static_cast<XMLTokenizer *>(userData)->comment(comment);
}

static void warningHandler(void *userData, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    static_cast<XMLTokenizer *>(userData)->error(XMLTokenizer::warning, message, args);
    va_end(args);
}

static void fatalErrorHandler(void *userData, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    static_cast<XMLTokenizer *>(userData)->error(XMLTokenizer::fatal, message, args);
    va_end(args);
}

static void normalErrorHandler(void *userData, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    static_cast<XMLTokenizer *>(userData)->error(XMLTokenizer::nonFatal, message, args);
    va_end(args);
}

void XMLTokenizer::finish()
{
    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));
    sax.error = normalErrorHandler;
    sax.fatalError = fatalErrorHandler;
    sax.characters = charactersHandler;
    sax.endElement = endElementHandler;
    sax.processingInstruction = processingInstructionHandler;
    sax.startElement = startElementHandler;
    sax.cdataBlock = cdataBlockHandler;
    sax.comment = commentHandler;
    sax.warning = warningHandler;
    m_parserStopped = false;
    m_sawError = false;
    m_sawXSLTransform = false;
    m_context = createQStringParser(&sax, this, m_doc->document()->URL().ascii());
    parseQString(m_context, m_xmlCode);
    xmlFreeParserCtxt(m_context);
    m_context = NULL;

    if (m_sawError) {
        insertErrorMessageBlock();
    } else {
        // Parsing was successful. Now locate all html <script> tags in the document and execute them
        // one by one.
        addScripts(m_doc->document());
        m_scriptsIt = new QPtrListIterator<HTMLScriptElementImpl>(m_scripts);
        executeScripts();
    }

    emit finishedParsing();
}

void XMLTokenizer::insertErrorMessageBlock()
{
    // One or more errors occurred during parsing of the code. Display an error block to the user above
    // the normal content (the DOM tree is created manually and includes line/col info regarding 
    // where the errors are located)

    // Create elements for display
    int exceptioncode = 0;
    DocumentImpl *doc = m_doc->document();
    NodeImpl* root = doc->documentElement();
    if (!root) {
        root = doc->createElementNS(HTMLNames::xhtmlNamespaceURI(), "html", exceptioncode);
        NodeImpl* body = doc->createElementNS(HTMLNames::xhtmlNamespaceURI(), "body", exceptioncode);
        root->appendChild(body, exceptioncode);
        doc->appendChild(root, exceptioncode);
        root = body;
    }

    ElementImpl* reportElement = doc->createElementNS(HTMLNames::xhtmlNamespaceURI(), "parsererror", exceptioncode);
    reportElement->setAttribute(ATTR_STYLE, "white-space: pre; border: 2px solid #c77; padding: 0 1em 0 1em; margin: 1em; background-color: #fdd; color: black");
    ElementImpl* h3 = doc->createElementNS(HTMLNames::xhtmlNamespaceURI(), "h3", exceptioncode);
    h3->appendChild(doc->createTextNode("This page contains the following errors:"), exceptioncode);
    reportElement->appendChild(h3, exceptioncode);
    ElementImpl* fixed = doc->createElementNS(HTMLNames::xhtmlNamespaceURI(), "div", exceptioncode);
    fixed->setAttribute(ATTR_STYLE, "font-family:monospace;font-size:12px");
    NodeImpl* textNode = doc->createTextNode(m_errorMessages);
    fixed->appendChild(textNode, exceptioncode);
    reportElement->appendChild(fixed, exceptioncode);
    h3 = doc->createElementNS(HTMLNames::xhtmlNamespaceURI(), "h3", exceptioncode);
    reportElement->appendChild(h3, exceptioncode);
    
    h3->appendChild(doc->createTextNode("Below is a rendering of the page up to the first error."), exceptioncode);
#ifdef KHTML_XSLT
    if (doc->transformSourceDocument()) {
        ElementImpl* par = doc->createElementNS(HTMLNames::xhtmlNamespaceURI(), "p", exceptioncode);
        reportElement->appendChild(par, exceptioncode);
        par->setAttribute(ATTR_STYLE, "white-space: normal");
        par->appendChild(doc->createTextNode("This document was created as the result of an XSL transformation. The line and column numbers given are from the transformed result."), exceptioncode);
    }
#endif
    root->insertBefore(reportElement, root->firstChild(), exceptioncode);

    doc->updateRendering();
}

void XMLTokenizer::addScripts(NodeImpl *n)
{
    // Recursively go through the entire document tree, looking for html <script> tags. For each of these
    // that is found, add it to the m_scripts list from which they will be executed

    if (n->hasTagName(HTMLNames::script())) {
        m_scripts.append(static_cast<HTMLScriptElementImpl*>(n));
    }

    NodeImpl *child;
    for (child = n->firstChild(); child; child = child->nextSibling())
        addScripts(child);
}

void XMLTokenizer::executeScripts()
{
    // Iterate through all of the html <script> tags in the document. For those that have a src attribute,
    // start loading the script and return (executeScripts() will be called again once the script is loaded
    // and continue where it left off). For scripts that don't have a src attribute, execute the code
    // inside the tag
    while (m_scriptsIt->current()) {
        DOMString scriptSrc = m_scriptsIt->current()->getAttribute(ATTR_SRC);
        QString charset = m_scriptsIt->current()->getAttribute(ATTR_CHARSET).string();

	// don't load external scripts for standalone documents (for now)
        if (scriptSrc != "" && m_doc->document()->part()) {
            // we have a src attribute
            m_cachedScript = m_doc->document()->docLoader()->requestScript(scriptSrc, charset);
            ++(*m_scriptsIt);
            m_cachedScript->ref(this); // will call executeScripts() again if already cached
            return;
        }
        else {
            // no src attribute - execute from contents of tag
            QString scriptCode = "";
            NodeImpl *child;
            for (child = m_scriptsIt->current()->firstChild(); child; child = child->nextSibling()) {
                if (child->nodeType() == Node::TEXT_NODE || child->nodeType() == Node::CDATA_SECTION_NODE) {
                    scriptCode += static_cast<TextImpl*>(child)->data().string();
                }
            }
            // the script cannot do document.write until we support incremental parsing
            // ### handle the case where the script deletes the node or redirects to
            // another page, etc. (also in notifyFinished())
            // ### the script may add another script node after this one which should be executed
            if (m_view) {
                m_view->part()->executeScript(scriptCode);
            }
            ++(*m_scriptsIt);
        }
    }

    // All scripts have finished executing, so calculate the style for the document and close
    // the last element
    m_doc->document()->updateStyleSelector();
}

void XMLTokenizer::notifyFinished(CachedObject *finishedObj)
{
    // This is called when a script has finished loading that was requested from executeScripts(). We execute
    // the script, and then call executeScripts() again to continue iterating through the list of scripts in
    // the document
    if (finishedObj == m_cachedScript) {
        DOMString scriptSource = m_cachedScript->script();
        m_cachedScript->deref(this);
        m_cachedScript = 0;
        m_view->part()->executeScript(scriptSource.string());
        executeScripts();
    }
}

bool XMLTokenizer::isWaitingForScripts() const
{
    return m_cachedScript != 0;
}

#ifdef KHTML_XSLT
void XMLTokenizer::setTransformSource(DocumentImpl* doc)
{
    // Time to spin up a new parse and save the xmlDocPtr.
    // Parse in a single chunk into an xmlDocPtr
    // FIXME: Hook up error handlers so that a failure to parse the main document results in
    // good error messages.
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    xmlDocPtr sourceDoc = xmlReadMemory(reinterpret_cast<const char *>(m_xmlCode.unicode()),
                                        m_xmlCode.length() * sizeof(QChar),
                                        doc->URL().ascii(),
                                        BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", 
                                        XML_PARSE_NOCDATA|XML_PARSE_DTDATTR|XML_PARSE_NOENT);
    doc->setTransformSource(sourceDoc);
}
#endif

Tokenizer *newXMLTokenizer(DocumentPtr *d, KHTMLView *v)
{
    return new XMLTokenizer(d, v);
}

int XMLTokenizer::lineNumber() const
{
    return m_context->input->line;
}

int XMLTokenizer::columnNumber() const
{
    return m_context->input->col;
}

void XMLTokenizer::stopParsing()
{
    xmlStopParser(m_context);
    m_parserStopped = true;
}

#if 0

bool XMLHandler::attributeDecl(const QString &/*eName*/, const QString &/*aName*/, const QString &/*type*/,
                               const QString &/*valueDefault*/, const QString &/*value*/)
{
    // qt's xml parser (as of 2.2.3) does not currently give us values for type, valueDefault and
    // value. When it does, we can store these somewhere and have default attributes on elements
    return true;
}

bool XMLHandler::externalEntityDecl(const QString &/*name*/, const QString &/*publicId*/, const QString &/*systemId*/)
{
    // ### insert these too - is there anything special we have to do here?
    return true;
}

bool XMLHandler::internalEntityDecl(const QString &name, const QString &value)
{
    EntityImpl *e = new EntityImpl(m_doc,name);
    // ### further parse entities inside the value and add them as separate nodes (or entityreferences)?
    e->addChild(m_doc->document()->createTextNode(value));
// ### FIXME
//     if (m_doc->document()->doctype())
//         static_cast<GenericRONamedNodeMapImpl*>(m_doc->document()->doctype()->entities())->addNode(e);
    return true;
}

bool XMLHandler::notationDecl(const QString &name, const QString &publicId, const QString &systemId)
{
// ### FIXME
//     if (m_doc->document()->doctype()) {
//         NotationImpl *n = new NotationImpl(m_doc,name,publicId,systemId);
//         static_cast<GenericRONamedNodeMapImpl*>(m_doc->document()->doctype()->notations())->addNode(n);
//     }
    return true;
}

#endif

// --------------------------------

XMLNamespaceStack::~XMLNamespaceStack()
{
    while (XMLNamespace *ns = m_namespaceStack.pop())
        ns->deref();
}

void XMLNamespaceStack::popNamespaces()
{
    XMLNamespace *ns = m_namespaceStack.pop();
    if (ns)
        ns->deref();
}

XMLNamespace *XMLNamespaceStack::pushNamespaces(XMLAttributes& attrs)
{
    XMLNamespace *ns = m_namespaceStack.current();
    if (!ns)
        ns = new XMLNamespace;

    // Search for any xmlns attributes.
    for (int i = 0; i < attrs.length(); i++) {
        QString qName = attrs.qName(i);
        if (qName == "xmlns")
            ns = new XMLNamespace(QString::null, attrs.value(i), ns);
        else if (qName.startsWith("xmlns:"))
            ns = new XMLNamespace(qName.right(qName.length()-6), attrs.value(i), ns);
    }

    m_namespaceStack.push(ns);
    ns->ref();
    return ns;
}

// --------------------------------

struct AttributeParseState {
    QMap<QString, QString> attributes;
    bool gotAttributes;
};

static void attributesStartElementHandler(void *userData, const xmlChar *name, const xmlChar **libxmlAttributes)
{
    if (strcmp(reinterpret_cast<const char *>(name), "attrs") != 0) {
        return;
    }
        
    AttributeParseState *state = static_cast<AttributeParseState *>(userData);
    
    state->gotAttributes = true;
    
    XMLAttributes attributes(reinterpret_cast<const char **>(libxmlAttributes));
    XMLNamespaceStack stack;
    attributes.split(stack.pushNamespaces(attributes));
    int length = attributes.length();
    for (int i = 0; i != length; ++i) {
        state->attributes.insert(attributes.qName(i), attributes.value(i));
    }
}

QMap<QString, QString> parseAttributes(const DOMString &string, bool &attrsOK)
{
    AttributeParseState state;
    state.gotAttributes = false;

    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));
    sax.startElement = attributesStartElementHandler;
    xmlParserCtxtPtr parser = createQStringParser(&sax, &state);
    parseQString(parser, "<?xml version=\"1.0\"?><attrs " + string.string() + " />");
    xmlFreeParserCtxt(parser);

    attrsOK = state.gotAttributes;
    return state.attributes;
}

// --------------------------------

XMLAttributes::XMLAttributes(const char **saxStyleAttributes)
    : _ref(0), _uris(0)
{
    int length = 0;
    if (saxStyleAttributes) {
        for (const char **p = saxStyleAttributes; *p; p += 2) {
            ++length;
        }
    }

    _length = length;
    if (!length) {
        _names = 0;
        _values = 0;
        _uris = 0;
    } else {
        _names = new QString [length];
        _values = new QString [length];
    }

    if (saxStyleAttributes) {
        int i = 0;
        for (const char **p = saxStyleAttributes; *p; p += 2) {
            _names[i] = QString::fromUtf8(p[0]);
            _values[i] = QString::fromUtf8(p[1]);
            ++i;
        }
    }
}

XMLAttributes::~XMLAttributes()
{
    if (_ref && !--*_ref) {
        delete _ref;
        _ref = 0;
    }
    if (!_ref) {
        delete [] _names;
        delete [] _values;
        delete [] _uris;
    }
}

XMLAttributes::XMLAttributes(const XMLAttributes &other)
    : _ref(other._ref)
    , _length(other._length)
    , _names(other._names)
    , _values(other._values)
    , _uris(other._uris)
{
    if (!_ref) {
        _ref = new int (2);
        other._ref = _ref;
    } else {
        ++*_ref;
    }
}

XMLAttributes &XMLAttributes::operator=(const XMLAttributes &other)
{
    if (_ref && !--*_ref) {
        delete _ref;
        _ref = 0;
    }
    if (!_ref) {
        delete [] _names;
        delete [] _values;
        delete [] _uris;
    }

    _ref = other._ref;
    _length = other._length;
    _names = other._names;
    _values = other._values;
    _uris = other._uris;

    if (!_ref) {
        _ref = new int (2);
        other._ref = _ref;
    } else {
        ++*_ref;
    }
    
    return *this;
}

QString XMLAttributes::localName(int index) const
{
    int colonPos = _names[index].find(':');
    if (colonPos != -1)
        // Peel off the prefix to return the localName.
        return _names[index].right(_names[index].length() - colonPos - 1);
    return _names[index];
}

QString XMLAttributes::value(const QString &name) const
{
    for (int i = 0; i != _length; ++i) {
        if (name == _names[i]) {
            return _values[i];
        }
    }
    return QString::null;
}

void XMLAttributes::split(XMLNamespace* ns)
{
    for (int i = 0; i < _length; ++i) {
        int colonPos = _names[i].find(':');
        if (colonPos != -1) {
            QString prefix = _names[i].left(colonPos);
            QString uri;
            if (prefix == "xmlns") {
                // FIXME: The URI is the xmlns namespace? I seem to recall DOM lvl 3 saying something about this.
            }
            else
                uri = ns->uriForPrefix(prefix);
            
            if (!uri.isEmpty()) {
                if (!_uris)
                    _uris = new QString[_length];
                _uris[i] = uri;
            }
        }
    }
}

}

#include "xml_tokenizer.moc"
