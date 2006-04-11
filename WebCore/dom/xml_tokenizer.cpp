/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Alexey Proskuryakov
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

#include "config.h"
#include "xml_tokenizer.h"

#include "CDATASection.h"
#include "Cache.h"
#include "CachedScript.h"
#include "Comment.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "Document.h"
#include "DocumentType.h"
#include "Frame.h"
#include "KWQLoader.h"
#include "dom_xmlimpl.h"
#include "html_headimpl.h"
#include "html_tableimpl.h"
#include "HTMLTokenizer.h"
#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include "TransferJob.h"

#if SVG_SUPPORT
#include "SVGNames.h"
#include "XLinkNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

const int maxErrors = 25;

typedef HashMap<StringImpl *, StringImpl *> PrefixForNamespaceMap;

class XMLTokenizer : public Tokenizer, public CachedObjectClient
{
public:
    XMLTokenizer(Document *, FrameView * = 0);
    XMLTokenizer(DocumentFragment *, Element *);
    ~XMLTokenizer();

    enum ErrorType { warning, nonFatal, fatal };

    // from Tokenizer
    virtual bool write(const SegmentedString &str, bool);
    virtual void finish();
    virtual bool isWaitingForScripts() const;
    virtual void stopParsing();

    void setIsXHTMLDocument(bool isXHTML) { m_isXHTMLDocument = isXHTML; }
    bool isXHTMLDocument() const { return m_isXHTMLDocument; }

    // from CachedObjectClient
    virtual void notifyFinished(CachedObject *finishedObj);

    // callbacks from parser SAX
    void error(ErrorType, const char *message, va_list args);
    void startElementNs(const xmlChar *xmlLocalName, const xmlChar *xmlPrefix, const xmlChar *xmlURI, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int nb_defaulted, const xmlChar **libxmlAttributes);
    void endElementNs();
    void characters(const xmlChar *s, int len);
    void processingInstruction(const xmlChar *target, const xmlChar *data);
    void cdataBlock(const xmlChar *s, int len);
    void comment(const xmlChar *s);
    void internalSubset(const xmlChar *name, const xmlChar *externalID, const xmlChar *systemID);

private:
    void initializeParserContext();
    void setCurrentNode(Node*);

    int lineNumber() const;
    int columnNumber() const;

    void insertErrorMessageBlock();

    void executeScripts();
    void addScripts(Node *n);

    bool enterText();
    void exitText();

    Document *m_doc;
    FrameView *m_view;
    
    DeprecatedString m_originalSourceForTransform;

    xmlParserCtxtPtr m_context;
    Node *m_currentNode;
    bool m_currentNodeIsReferenced;

    bool m_sawError;
    bool m_sawXSLTransform;
    bool m_sawFirstElement;
    bool m_isXHTMLDocument;
    
    int m_errorCount;
    int m_lastErrorLine;
    int m_lastErrorColumn;
    String m_errorMessages;

    DeprecatedPtrList<Element> m_scripts;
    DeprecatedPtrListIterator<Element> *m_scriptsIt;
    CachedScript *m_cachedScript;
    
    bool m_parsingFragment;
    String m_defaultNamespaceURI;
    PrefixForNamespaceMap m_prefixToNamespaceMap;
};

// --------------------------------

static int globalDescriptor = 0;

static int matchFunc(const char* uri)
{
    return 1; // Match everything.
}

static WebCore::DocLoader *globalDocLoader = 0;

class OffsetBuffer {
public:
    OffsetBuffer(const DeprecatedByteArray &b) : m_buffer(b), m_currentOffset(0) { }
    
    int readOutBytes(char *outputBuffer, unsigned askedToRead) {
        unsigned bytesLeft = m_buffer.size() - m_currentOffset;
        unsigned lenToCopy = kMin(askedToRead, bytesLeft);
        if (lenToCopy) {
            memcpy(outputBuffer, m_buffer.data() + m_currentOffset, lenToCopy);
            m_currentOffset += lenToCopy;
        }
        return lenToCopy;
    }

private:
    DeprecatedByteArray m_buffer;
    unsigned m_currentOffset;
};

static bool shouldAllowExternalLoad(const char* inURI)
{
    DeprecatedString url(inURI);
    if (url.contains("/etc/catalog")
        || url.startsWith("http://www.w3.org/Graphics/SVG")
        || url.startsWith("http://www.w3.org/TR/xhtml"))
        return false;
    return true;
}

static void* openFunc(const char* uri)
{
    if (!globalDocLoader || !shouldAllowExternalLoad(uri))
        return &globalDescriptor;

    KURL finalURL;
    TransferJob* job = new TransferJob(0, "GET", uri);
    DeprecatedString headers;
    DeprecatedByteArray data = KWQServeSynchronousRequest(Cache::loader(), globalDocLoader, job, finalURL, headers);
    
    return new OffsetBuffer(data);
}

static int readFunc(void* context, char* buffer, int len)
{
    // Do 0-byte reads in case of a null descriptor
    if (context == &globalDescriptor)
        return 0;
        
    OffsetBuffer *data = static_cast<OffsetBuffer *>(context);
    return data->readOutBytes(buffer, len);
}

static int writeFunc(void* context, const char* buffer, int len)
{
    // Always just do 0-byte writes
    return 0;
}

static int closeFunc(void * context)
{
    if (context != &globalDescriptor) {
        OffsetBuffer *data = static_cast<OffsetBuffer *>(context);
        delete data;
    }
    return 0;
}

void setLoaderForLibXMLCallbacks(DocLoader *docLoader)
{
    globalDocLoader = docLoader;
}

static xmlParserCtxtPtr createQStringParser(xmlSAXHandlerPtr handlers, void *userData)
{
    static bool didInit = false;
    if (!didInit) {
        xmlInitParser();
        xmlRegisterInputCallbacks(matchFunc, openFunc, readFunc, closeFunc);
        xmlRegisterOutputCallbacks(matchFunc, openFunc, writeFunc, closeFunc);
        didInit = true;
    }

    xmlParserCtxtPtr parser = xmlCreatePushParserCtxt(handlers, 0, 0, 0, 0);
    parser->_private = userData;
    parser->replaceEntities = true;
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    xmlSwitchEncoding(parser, BOMHighByte == 0xFF ? XML_CHAR_ENCODING_UTF16LE : XML_CHAR_ENCODING_UTF16BE);
    return parser;
}

// --------------------------------

XMLTokenizer::XMLTokenizer(Document *_doc, FrameView *_view)
    : m_doc(_doc)
    , m_view(_view)
    , m_context(0)
    , m_currentNode(_doc)
    , m_currentNodeIsReferenced(false)
    , m_sawError(false)
    , m_sawXSLTransform(false)
    , m_sawFirstElement(false)
    , m_isXHTMLDocument(false)
    , m_errorCount(0)
    , m_lastErrorLine(0)
    , m_lastErrorColumn(0)
    , m_scriptsIt(0)
    , m_cachedScript(0)
    , m_parsingFragment(false)
{
}

XMLTokenizer::XMLTokenizer(DocumentFragment *fragment, Element *parentElement)
    : m_doc(fragment->document())
    , m_view(0)
    , m_context(0)
    , m_currentNode(fragment)
    , m_currentNodeIsReferenced(fragment)
    , m_sawError(false)
    , m_sawXSLTransform(false)
    , m_sawFirstElement(false)
    , m_isXHTMLDocument(false)
    , m_errorCount(0)
    , m_lastErrorLine(0)
    , m_lastErrorColumn(0)
    , m_scriptsIt(0)
    , m_cachedScript(0)
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
        
        Node *n = parentElement->parentNode();
        if (!n || !n->isElementNode())
            break;
        parentElement = static_cast<Element *>(n);
    }
    for (Element *element = elemStack.last(); !elemStack.isEmpty(); elemStack.removeLast()) {
        if (NamedAttrMap *attrs = element->attributes()) {
            for (unsigned i = 0; i < attrs->length(); i++) {
                Attribute *attr = attrs->attributeItem(i);
                if (attr->localName() == "xmlns")
                    m_defaultNamespaceURI = attr->value();
                else if (attr->prefix() == "xmlns")
                    m_prefixToNamespaceMap.set(attr->localName().impl(), attr->value().impl());
            }
        }
    }
}

XMLTokenizer::~XMLTokenizer()
{
    setCurrentNode(0);
    if (m_parsingFragment && m_doc)
        m_doc->deref();
    delete m_scriptsIt;
    if (m_cachedScript)
        m_cachedScript->deref(this);
}

void XMLTokenizer::setCurrentNode(Node* n)
{
    bool nodeNeedsReference = n && n != m_doc;
    if (nodeNeedsReference)
        n->ref(); 
    if (m_currentNodeIsReferenced) 
        m_currentNode->deref(); 
    m_currentNode = n;
    m_currentNodeIsReferenced = nodeNeedsReference;
}

bool XMLTokenizer::write(const SegmentedString &s, bool /*appendData*/ )
{
    DeprecatedString parseString = s.toString();
    if (m_sawXSLTransform || !m_sawFirstElement)
        m_originalSourceForTransform += parseString;

    if (m_parserStopped || m_sawXSLTransform)
        return false;
    if (!m_context)
        initializeParserContext();
    
    // Hack around libxml2's lack of encoding overide support by manually
    // resetting the encoding to UTF-16 before every chunk.  Otherwise libxml
    // will detect <?xml version="1.0" encoding="<encoding name>"?> blocks 
    // and switch encodings, causing the parse to fail.
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    xmlSwitchEncoding(m_context, BOMHighByte == 0xFF ? XML_CHAR_ENCODING_UTF16LE : XML_CHAR_ENCODING_UTF16BE);
    
    xmlParseChunk(m_context, reinterpret_cast<const char *>(parseString.unicode()), sizeof(QChar) * parseString.length(), 0);
    
    return false;
}

inline DeprecatedString toQString(const xmlChar *str, unsigned int len)
{
    return DeprecatedString::fromUtf8(reinterpret_cast<const char *>(str), len);
}

inline DeprecatedString toQString(const xmlChar *str)
{
    return DeprecatedString::fromUtf8(str ? reinterpret_cast<const char *>(str) : "");
}

inline String toString(const xmlChar* str, unsigned int len)
{
    return DeprecatedString::fromUtf8(reinterpret_cast<const char *>(str), len);
}

inline String toString(const xmlChar* str)
{
    return DeprecatedString::fromUtf8(str ? reinterpret_cast<const char *>(str) : "");
}

struct _xmlSAX2Namespace {
    const xmlChar *prefix;
    const xmlChar *uri;
};
typedef struct _xmlSAX2Namespace xmlSAX2Namespace;

static inline void handleElementNamespaces(Element *newElement, const xmlChar **libxmlNamespaces, int nb_namespaces, ExceptionCode& ec)
{
    xmlSAX2Namespace *namespaces = reinterpret_cast<xmlSAX2Namespace *>(libxmlNamespaces);
    for(int i = 0; i < nb_namespaces; i++) {
        String namespaceQName = "xmlns";
        String namespaceURI = toString(namespaces[i].uri);
        if (namespaces[i].prefix)
            namespaceQName = "xmlns:" + toString(namespaces[i].prefix);
        newElement->setAttributeNS("http://www.w3.org/2000/xmlns/", namespaceQName, namespaceURI, ec);
        if (ec) // exception setting attributes
            return;
    }
}

struct _xmlSAX2Attributes {
    const xmlChar *localname;
    const xmlChar *prefix;
    const xmlChar *uri;
    const xmlChar *value;
    const xmlChar *end;
};
typedef struct _xmlSAX2Attributes xmlSAX2Attributes;

static inline void handleElementAttributes(Element *newElement, const xmlChar **libxmlAttributes, int nb_attributes, ExceptionCode& ec)
{
    xmlSAX2Attributes *attributes = reinterpret_cast<xmlSAX2Attributes *>(libxmlAttributes);
    for(int i = 0; i < nb_attributes; i++) {
        String attrLocalName = toQString(attributes[i].localname);
        int valueLength = (int) (attributes[i].end - attributes[i].value);
        String attrValue = toQString(attributes[i].value, valueLength);
        String attrPrefix = toQString(attributes[i].prefix);
        String attrURI = attrPrefix.isEmpty() ? String() : toQString(attributes[i].uri);
        String attrQName = attrPrefix.isEmpty() ? attrLocalName : attrPrefix + ":" + attrLocalName;
        
        newElement->setAttributeNS(attrURI, attrQName, attrValue, ec);
        if (ec) // exception setting attributes
            return;
    }
}

void XMLTokenizer::startElementNs(const xmlChar *xmlLocalName, const xmlChar *xmlPrefix, const xmlChar *xmlURI, int nb_namespaces, const xmlChar **libxmlNamespaces, int nb_attributes, int nb_defaulted, const xmlChar **libxmlAttributes)
{
    if (m_parserStopped)
        return;
    
    m_sawFirstElement = true;

    exitText();

    String localName = toQString(xmlLocalName);
    String uri = toQString(xmlURI);
    String prefix = toQString(xmlPrefix);
    String qName = prefix.isEmpty() ? localName : prefix + ":" + localName;
    
    if (m_parsingFragment && uri.isEmpty()) {
        if (!prefix.isEmpty())
            uri = String(m_prefixToNamespaceMap.get(prefix.impl()));
        else
            uri = m_defaultNamespaceURI;
    }

    ExceptionCode ec = 0;
    RefPtr<Element> newElement = m_doc->createElementNS(uri, qName, ec);
    if (!newElement) {
        stopParsing();
        return;
    }
    
    handleElementNamespaces(newElement.get(), libxmlNamespaces, nb_namespaces, ec);
    if (ec) {
        stopParsing();
        return;
    }
    
    handleElementAttributes(newElement.get(), libxmlAttributes, nb_attributes, ec);
    if (ec) {
        stopParsing();
        return;
    }

    // FIXME: This hack ensures implicit table bodies get constructed in XHTML and XML files.
    // We want to consolidate this with the HTML parser and HTML DOM code at some point.
    // For now, it's too risky to rip that code up.
    if (m_currentNode->hasTagName(tableTag) && newElement->hasTagName(trTag)) {
        RefPtr<Node> implicitTBody = new HTMLTableSectionElement(tbodyTag, m_doc, true /* implicit */);
        m_currentNode->addChild(implicitTBody.get());
        setCurrentNode(implicitTBody.get());
        if (m_view && !implicitTBody->attached())
            implicitTBody->attach();
    }

    if (newElement->hasTagName(scriptTag))
        static_cast<HTMLScriptElement *>(newElement.get())->setCreatedByParser(true);

    if (!m_currentNode->addChild(newElement.get())) {
        stopParsing();
        return;
    }
    
    setCurrentNode(newElement.get());
    if (m_view && !newElement->attached())
        newElement->attach();
}

void XMLTokenizer::endElementNs()
{
    if (m_parserStopped)
        return;

    exitText();

    Node *n = m_currentNode;
    while (n->implicitNode())
        n = n->parentNode();
    RefPtr<Node> parent = n->parentNode();
    n->closeRenderer();
    setCurrentNode(parent.get());
}

void XMLTokenizer::characters(const xmlChar *s, int len)
{
    if (m_parserStopped)
        return;
    
    if (m_currentNode->isTextNode() || enterText()) {
        ExceptionCode ec = 0;
        static_cast<Text*>(m_currentNode)->appendData(toQString(s, len), ec);
    }
}

bool XMLTokenizer::enterText()
{
    RefPtr<Node> newNode = new Text(m_doc, "");
    if (!m_currentNode->addChild(newNode.get()))
        return false;
    setCurrentNode(newNode.get());
    return true;
}

void XMLTokenizer::exitText()
{
    if (m_parserStopped)
        return;

    if (!m_currentNode || !m_currentNode->isTextNode())
        return;

    if (m_view && m_currentNode && !m_currentNode->attached())
        m_currentNode->attach();

    // FIXME: What's the right thing to do if the parent is really 0?
    // Just leaving the current node set to the text node doesn't make much sense.
    if (Node* par = m_currentNode->parentNode())
        setCurrentNode(par);
}

void XMLTokenizer::error(ErrorType type, const char *message, va_list args)
{
    if (m_parserStopped)
        return;

    if (type == fatal || (m_errorCount < maxErrors && m_lastErrorLine != lineNumber() && m_lastErrorColumn != columnNumber())) {
#if WIN32
        char m[1024];
        vsnprintf(m, sizeof(m) - 1, message, args);
#else
        char *m;
        vasprintf(&m, message, args);
#endif
        switch (type) {
            case warning:
                m_errorMessages += String::sprintf("warning on line %d at column %d: %s", lineNumber(), columnNumber(), m);
                break;
            case fatal:
            case nonFatal:
                m_errorMessages += String::sprintf("error on line %d at column %d: %s", lineNumber(), columnNumber(), m);
        }
#if !WIN32
        free(m);
#endif

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
    if (m_parserStopped)
        return;

    exitText();

    // ### handle exceptions
    int exception = 0;
    RefPtr<ProcessingInstruction> pi = m_doc->createProcessingInstruction(
        toQString(target), toQString(data), exception);
    if (exception)
        return;

    if (!m_currentNode->addChild(pi.get()))
        return;
    if (m_view && !pi->attached())
        pi->attach();

    // don't load stylesheets for standalone documents
    if (m_doc->frame()) {
        m_sawXSLTransform = !pi->checkStyleSheet();
#ifdef KHTML_XSLT
        // Pretend we didn't see this PI if we're the result of a transform.
        if (m_sawXSLTransform && !m_doc->transformSourceDocument())
#else
        if (m_sawXSLTransform)
#endif
            // Stop the SAX parser.
            stopParsing();
    }
}

void XMLTokenizer::cdataBlock(const xmlChar *s, int len)
{
    if (m_parserStopped)
        return;

    exitText();

    RefPtr<Node> newNode = new CDATASection(m_doc, toQString(s, len));
    if (!m_currentNode->addChild(newNode.get()))
        return;
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::comment(const xmlChar *s)
{
    if (m_parserStopped)
        return;

    exitText();

    RefPtr<Node> newNode = new Comment(m_doc, toQString(s));
    m_currentNode->addChild(newNode.get());
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::internalSubset(const xmlChar *name, const xmlChar *externalID, const xmlChar *systemID)
{
    if (m_parserStopped)
        return;

    Document *doc = m_doc;
    if (!doc)
        return;

    doc->setDocType(new DocumentType(doc, toQString(name), toQString(externalID), toQString(systemID)));
}

inline XMLTokenizer *getTokenizer(void *closure)
{
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
    return static_cast<XMLTokenizer *>(ctxt->_private);
}

static void startElementNsHandler(void *closure, const xmlChar *localname, const xmlChar *prefix, const xmlChar *uri, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int nb_defaulted, const xmlChar **libxmlAttributes)
{
    getTokenizer(closure)->startElementNs(localname, prefix, uri, nb_namespaces, namespaces, nb_attributes, nb_defaulted, libxmlAttributes);
}

static void endElementNsHandler(void *closure, const xmlChar *localname, const xmlChar *prefix, const xmlChar *uri)
{
    getTokenizer(closure)->endElementNs();
}

static void charactersHandler(void *closure, const xmlChar *s, int len)
{
    getTokenizer(closure)->characters(s, len);
}

static void processingInstructionHandler(void *closure, const xmlChar *target, const xmlChar *data)
{
    getTokenizer(closure)->processingInstruction(target, data);
}

static void cdataBlockHandler(void *closure, const xmlChar *s, int len)
{
    getTokenizer(closure)->cdataBlock(s, len);
}

static void commentHandler(void *closure, const xmlChar *comment)
{
    getTokenizer(closure)->comment(comment);
}

static void warningHandler(void *closure, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    getTokenizer(closure)->error(XMLTokenizer::warning, message, args);
    va_end(args);
}

static void fatalErrorHandler(void *closure, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    getTokenizer(closure)->error(XMLTokenizer::fatal, message, args);
    va_end(args);
}

static void normalErrorHandler(void *closure, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    getTokenizer(closure)->error(XMLTokenizer::nonFatal, message, args);
    va_end(args);
}

// Using a global variable entity and marking it XML_INTERNAL_PREDEFINED_ENTITY is
// a hack to avoid malloc/free. Using a global variable like this could cause trouble
// if libxml implementation details were to change
static xmlChar sharedXHTMLEntityResult[5] = {0,0,0,0,0};
static xmlEntity sharedXHTMLEntity = {
    0, XML_ENTITY_DECL, 0, 0, 0, 0, 0, 0, 0, 
    sharedXHTMLEntityResult, sharedXHTMLEntityResult, 0,
    XML_INTERNAL_PREDEFINED_ENTITY, 0, 0, 0, 0, 0
};

static xmlEntityPtr getXHTMLEntity(const xmlChar* name)
{
    unsigned short c = decodeNamedEntity(reinterpret_cast<const char*>(name));
    if (!c)
        return 0;

    DeprecatedCString value = DeprecatedString(QChar(c)).utf8();
    assert(value.length() < 5);
    sharedXHTMLEntity.length = value.length();
    sharedXHTMLEntity.name = name;
    memcpy(sharedXHTMLEntityResult, value.data(), sharedXHTMLEntity.length);

    return &sharedXHTMLEntity;
}

static xmlEntityPtr getEntityHandler(void *closure, const xmlChar *name)
{
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
    xmlEntityPtr ent = xmlGetPredefinedEntity(name);
    if (ent)
        return ent;

    ent = xmlGetDocEntity(ctxt->myDoc, name);
    if (!ent && getTokenizer(closure)->isXHTMLDocument())
        ent = getXHTMLEntity(name);

    // Work around a libxml SAX2 bug that causes charactersHandler to be called twice.
    if (ent)
        ctxt->replaceEntities = (ctxt->instate == XML_PARSER_ATTRIBUTE_VALUE) || (ent->etype != XML_INTERNAL_GENERAL_ENTITY);
    
    return ent;
}

static void internalSubsetHandler(void *closure, const xmlChar *name, const xmlChar *externalID, const xmlChar *systemID)
{
    getTokenizer(closure)->internalSubset(name, externalID, systemID);
    xmlSAX2InternalSubset(closure, name, externalID, systemID);
}

static void externalSubsetHandler(void *closure, const xmlChar *name, const xmlChar *externalId, const xmlChar *systemId)
{
    DeprecatedString extId = toQString(externalId);
    if ((extId == "-//W3C//DTD XHTML 1.0 Transitional//EN")
        || (extId == "-//W3C//DTD XHTML 1.1//EN")
        || (extId == "-//W3C//DTD XHTML 1.0 Strict//EN")
        || (extId == "-//W3C//DTD XHTML 1.0 Frameset//EN")
        || (extId == "-//W3C//DTD XHTML Basic 1.0//EN")
        || (extId == "-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN")
        || (extId == "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN")
        || (extId == "-//WAPFORUM//DTD XHTML Mobile 1.0//EN"))
        getTokenizer(closure)->setIsXHTMLDocument(true); // controls if we replace entities or not.
}

static void ignorableWhitespaceHandler(void *ctx, const xmlChar *ch, int len)
{
    // nothing to do, but we need this to work around a crasher
    // http://bugzilla.gnome.org/show_bug.cgi?id=172255
    // http://bugzilla.opendarwin.org/show_bug.cgi?id=5792
}

void XMLTokenizer::initializeParserContext()
{
    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));
    sax.error = normalErrorHandler;
    sax.fatalError = fatalErrorHandler;
    sax.characters = charactersHandler;
    sax.processingInstruction = processingInstructionHandler;
    sax.cdataBlock = cdataBlockHandler;
    sax.comment = commentHandler;
    sax.warning = warningHandler;
    sax.startElementNs = startElementNsHandler;
    sax.endElementNs = endElementNsHandler;
    sax.getEntity = getEntityHandler;
    sax.startDocument = xmlSAX2StartDocument;
    sax.internalSubset = internalSubsetHandler;
    sax.externalSubset = externalSubsetHandler;
    sax.ignorableWhitespace = ignorableWhitespaceHandler;
    sax.entityDecl = xmlSAX2EntityDecl;
    sax.initialized = XML_SAX2_MAGIC;
    
    m_parserStopped = false;
    m_sawError = false;
    m_sawXSLTransform = false;
    m_sawFirstElement = false;
    m_context = createQStringParser(&sax, this);
}

void XMLTokenizer::finish()
{
    if (m_sawXSLTransform) {
        m_doc->setTransformSource(xmlDocPtrForString(m_originalSourceForTransform, m_doc->URL()));
        
        m_doc->setParsing(false); // Make the doc think it's done, so it will apply xsl sheets.
        m_doc->updateStyleSelector();
        m_doc->setParsing(true);
        m_parserStopped = true;
    }
    
    if (m_context) {
        // Tell libxml we're done.
        xmlParseChunk(m_context, 0, 0, 1);
        
        if (m_context->myDoc)
            xmlFreeDoc(m_context->myDoc);
        xmlFreeParserCtxt(m_context);
        m_context = 0;
    }
    
    if (m_sawError)
        insertErrorMessageBlock();
    else {
        // Parsing was successful. Now locate all html <script> tags in the document and execute them
        // one by one.
        exitText();
        addScripts(m_doc);
        m_scriptsIt = new DeprecatedPtrListIterator<Element>(m_scripts);
        executeScripts();
    }

    setCurrentNode(0);
    m_doc->finishedParsing();
}

static inline RefPtr<Element> createXHTMLParserErrorHeader(Document* doc, const String& errorMessages) 
{
    ExceptionCode ec = 0;
    RefPtr<Element> reportElement = doc->createElementNS(xhtmlNamespaceURI, "parsererror", ec);
    reportElement->setAttribute(styleAttr, "white-space: pre; border: 2px solid #c77; padding: 0 1em 0 1em; margin: 1em; background-color: #fdd; color: black");
    
    RefPtr<Element> h3 = doc->createElementNS(xhtmlNamespaceURI, "h3", ec);
    reportElement->appendChild(h3.get(), ec);
    h3->appendChild(doc->createTextNode("This page contains the following errors:"), ec);
    
    RefPtr<Element> fixed = doc->createElementNS(xhtmlNamespaceURI, "div", ec);
    reportElement->appendChild(fixed.get(), ec);
    fixed->setAttribute(styleAttr, "font-family:monospace;font-size:12px");
    fixed->appendChild(doc->createTextNode(errorMessages), ec);
    
    h3 = doc->createElementNS(xhtmlNamespaceURI, "h3", ec);
    reportElement->appendChild(h3.get(), ec);
    h3->appendChild(doc->createTextNode("Below is a rendering of the page up to the first error."), ec);
    
    return reportElement;
}

void XMLTokenizer::insertErrorMessageBlock()
{
    // One or more errors occurred during parsing of the code. Display an error block to the user above
    // the normal content (the DOM tree is created manually and includes line/col info regarding 
    // where the errors are located)

    // Create elements for display
    ExceptionCode ec = 0;
    Document *doc = m_doc;
    Node* documentElement = doc->documentElement();
    if (!documentElement) {
        RefPtr<Node> rootElement = doc->createElementNS(xhtmlNamespaceURI, "html", ec);
        doc->appendChild(rootElement, ec);
        RefPtr<Node> body = doc->createElementNS(xhtmlNamespaceURI, "body", ec);
        rootElement->appendChild(body, ec);
        documentElement = body.get();
    }
#if SVG_SUPPORT
    else if (documentElement->namespaceURI() == WebCore::SVGNames::svgNamespaceURI) {
        // Until our SVG implementation has text support, it is best if we 
        // wrap the erroneous SVG document in an xhtml document and render
        // the combined document with error messages.
        RefPtr<Node> rootElement = doc->createElementNS(xhtmlNamespaceURI, "html", ec);
        RefPtr<Node> body = doc->createElementNS(xhtmlNamespaceURI, "body", ec);
        rootElement->appendChild(body, ec);
        body->appendChild(documentElement, ec);
        doc->appendChild(rootElement.get(), ec);
        documentElement = body.get();
    }
#endif

    RefPtr<Element> reportElement = createXHTMLParserErrorHeader(doc, m_errorMessages);
    documentElement->insertBefore(reportElement, documentElement->firstChild(), ec);
#ifdef KHTML_XSLT
    if (doc->transformSourceDocument()) {
        RefPtr<Element> par = doc->createElementNS(xhtmlNamespaceURI, "p", ec);
        reportElement->appendChild(par, ec);
        par->setAttribute(styleAttr, "white-space: normal");
        par->appendChild(doc->createTextNode("This document was created as the result of an XSL transformation. The line and column numbers given are from the transformed result."), ec);
    }
#endif
    doc->updateRendering();
}

void XMLTokenizer::addScripts(Node *n)
{
    // Recursively go through the entire document tree, looking for html <script> tags. For each of these
    // that is found, add it to the m_scripts list from which they will be executed
    if (n->hasTagName(scriptTag)
#if SVG_SUPPORT
        || n->hasTagName(WebCore::SVGNames::scriptTag)
#endif
        )
        m_scripts.append(static_cast<Element*>(n));

    Node *child;
    for (child = n->firstChild(); child; child = child->nextSibling())
        addScripts(child);
}

void XMLTokenizer::executeScripts()
{
    // Iterate through all of the html <script> tags in the document. For those that have a src attribute,
    // start loading the script and return (executeScripts() will be called again once the script is loaded
    // and continue where it left off). For scripts that don't have a src attribute, execute the code
    // inside the tag
    while (Element *scriptElement = m_scriptsIt->current()) {
        ++(*m_scriptsIt);
        String scriptHref = scriptElement->getAttribute(srcAttr);
#if SVG_SUPPORT
        if (scriptElement->hasTagName(WebCore::SVGNames::scriptTag))
            scriptHref = scriptElement->getAttribute(XLinkNames::hrefAttr);
#endif
        // don't load external scripts for standalone documents (for now)
        if (!scriptHref.isEmpty() && m_doc->frame()) {
            // we have a src attribute
            DeprecatedString charset = scriptElement->getAttribute(charsetAttr).deprecatedString();
            m_cachedScript = m_doc->docLoader()->requestScript(scriptHref, charset);
            m_cachedScript->ref(this); // will call executeScripts() again if already cached
            return;
        } else {
            // no src attribute - execute from contents of tag
            DeprecatedString scriptCode = "";
            for (Node *child = scriptElement->firstChild(); child; child = child->nextSibling()) {
                if (child->isTextNode() || child->nodeType() == Node::CDATA_SECTION_NODE)
                    scriptCode += static_cast<CharacterData*>(child)->data().deprecatedString();
            }
            // the script cannot do document.write until we support incremental parsing
            // ### handle the case where the script deletes the node or redirects to
            // another page, etc. (also in notifyFinished())
            // ### the script may add another script node after this one which should be executed
            if (m_view)
                m_view->frame()->executeScript(0, scriptCode);
        }
    }

    // All scripts have finished executing, so calculate the style for the document and close
    // the last element
    m_doc->updateStyleSelector();
}

void XMLTokenizer::notifyFinished(CachedObject *finishedObj)
{
    // This is called when a script has finished loading that was requested from executeScripts(). We execute
    // the script, and then call executeScripts() again to continue iterating through the list of scripts in
    // the document
    if (finishedObj == m_cachedScript) {
        String scriptSource = m_cachedScript->script();
        m_cachedScript->deref(this);
        m_cachedScript = 0;
        m_view->frame()->executeScript(0, scriptSource.deprecatedString());
        executeScripts();
    }
}

bool XMLTokenizer::isWaitingForScripts() const
{
    return m_cachedScript != 0;
}

#ifdef KHTML_XSLT
void *xmlDocPtrForString(const DeprecatedString &source, const DeprecatedString &url)
{
    if (source.isEmpty())
            return 0;
    // Parse in a single chunk into an xmlDocPtr
    // FIXME: Hook up error handlers so that a failure to parse the main document results in
    // good error messages.
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    xmlDocPtr sourceDoc = xmlReadMemory(reinterpret_cast<const char *>(source.unicode()),
                                        source.length() * sizeof(QChar),
                                        url.ascii(),
                                        BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", 
                                        XML_PARSE_NOCDATA|XML_PARSE_DTDATTR|XML_PARSE_NOENT);
    return sourceDoc;
}
#endif

Tokenizer *newXMLTokenizer(Document *d, FrameView *v)
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
    Tokenizer::stopParsing();
    xmlStopParser(m_context);
}

static void balancedStartElementNsHandler(void *closure, const xmlChar *localname, const xmlChar *prefix, const xmlChar *uri, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int nb_defaulted, const xmlChar **libxmlAttributes)
{
   static_cast<XMLTokenizer *>(closure)->startElementNs(localname, prefix, uri, nb_namespaces, namespaces, nb_attributes, nb_defaulted, libxmlAttributes);
}

static void balancedEndElementNsHandler(void *closure, const xmlChar *localname, const xmlChar *prefix, const xmlChar *uri)
{
    static_cast<XMLTokenizer *>(closure)->endElementNs();
}

static void balancedCharactersHandler(void *closure, const xmlChar *s, int len)
{
    static_cast<XMLTokenizer *>(closure)->characters(s, len);
}

static void balancedProcessingInstructionHandler(void *closure, const xmlChar *target, const xmlChar *data)
{
    static_cast<XMLTokenizer *>(closure)->processingInstruction(target, data);
}

static void balancedCdataBlockHandler(void *closure, const xmlChar *s, int len)
{
    static_cast<XMLTokenizer *>(closure)->cdataBlock(s, len);
}

static void balancedCommentHandler(void *closure, const xmlChar *comment)
{
    static_cast<XMLTokenizer *>(closure)->comment(comment);
}

static void balancedWarningHandler(void *closure, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    static_cast<XMLTokenizer *>(closure)->error(XMLTokenizer::warning, message, args);
    va_end(args);
}

bool parseXMLDocumentFragment(const String &string, DocumentFragment *fragment, Element *parent)
{
    XMLTokenizer tokenizer(fragment, parent);
    
    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));

    sax.characters = balancedCharactersHandler;
    sax.processingInstruction = balancedProcessingInstructionHandler;
    sax.startElementNs = balancedStartElementNsHandler;
    sax.endElementNs = balancedEndElementNsHandler;
    sax.cdataBlock = balancedCdataBlockHandler;
    sax.ignorableWhitespace = balancedCdataBlockHandler;
    sax.comment = balancedCommentHandler;
    sax.warning = balancedWarningHandler;
    sax.initialized = XML_SAX2_MAGIC;
    
    int result = xmlParseBalancedChunkMemory(0, &sax, &tokenizer, 0, 
                                            (const xmlChar*)(const char*)(string.deprecatedString().utf8()), 0);
    return result == 0;
}

// --------------------------------

struct AttributeParseState {
    HashMap<String, String> attributes;
    bool gotAttributes;
};


static void attributesStartElementNsHandler(void *closure, const xmlChar *xmlLocalName, const xmlChar *xmlPrefix, const xmlChar *xmlURI, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int nb_defaulted, const xmlChar **libxmlAttributes)
{
    if (strcmp(reinterpret_cast<const char *>(xmlLocalName), "attrs") != 0)
        return;
    
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
    AttributeParseState *state = static_cast<AttributeParseState *>(ctxt->_private);
    
    state->gotAttributes = true;
    
    xmlSAX2Attributes *attributes = reinterpret_cast<xmlSAX2Attributes *>(libxmlAttributes);
    for(int i = 0; i < nb_attributes; i++) {
        DeprecatedString attrLocalName = toQString(attributes[i].localname);
        int valueLength = (int) (attributes[i].end - attributes[i].value);
        String attrValue = toString(attributes[i].value, valueLength);
        String attrPrefix = toString(attributes[i].prefix);
        String attrQName = attrPrefix.isEmpty() ? attrLocalName : attrPrefix + ":" + attrLocalName;
        
        state->attributes.set(attrQName, attrValue);
    }
}

HashMap<String, String> parseAttributes(const String& string, bool& attrsOK)
{
    AttributeParseState state;
    state.gotAttributes = false;

    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));
    sax.startElementNs = attributesStartElementNsHandler;
    sax.initialized = XML_SAX2_MAGIC;
    xmlParserCtxtPtr parser = createQStringParser(&sax, &state);
    DeprecatedString parseString = "<?xml version=\"1.0\"?><attrs " + string.deprecatedString() + " />";
    xmlParseChunk(parser, reinterpret_cast<const char *>(parseString.unicode()), parseString.length() * sizeof(QChar), 1);
    if (parser->myDoc)
        xmlFreeDoc(parser->myDoc);
    xmlFreeParserCtxt(parser);

    attrsOK = state.gotAttributes;
    return state.attributes;
}

}
