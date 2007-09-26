/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
 */

#include "config.h"
#include "XMLTokenizer.h"

#include "CDATASection.h"
#include "CString.h"
#include "Cache.h"
#include "CachedScript.h"
#include "Comment.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTokenizer.h"
#include "ProcessingInstruction.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#ifndef USE_QXMLSTREAM
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#else
#include <QDebug>
#endif
#include <wtf/Platform.h>
#include <wtf/Vector.h>

#if ENABLE(XSLT)
#include <libxslt/xslt.h>
#endif

#if ENABLE(SVG)
#include "SVGNames.h"
#include "SVGStyleElement.h"
#include "XLinkNames.h"
#endif

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

const int maxErrors = 25;

#ifndef USE_QXMLSTREAM
class PendingCallbacks {
public:
    PendingCallbacks()
    {
        m_callbacks.setAutoDelete(true);
    }
    
    void appendStartElementNSCallback(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int nb_namespaces,
                                      const xmlChar** namespaces, int nb_attributes, int nb_defaulted, const xmlChar** attributes)
    {
        PendingStartElementNSCallback* callback = new PendingStartElementNSCallback;
        
        callback->xmlLocalName = xmlStrdup(xmlLocalName);
        callback->xmlPrefix = xmlStrdup(xmlPrefix);
        callback->xmlURI = xmlStrdup(xmlURI);
        callback->nb_namespaces = nb_namespaces;
        callback->namespaces = reinterpret_cast<xmlChar**>(xmlMalloc(sizeof(xmlChar*) * nb_namespaces * 2));
        for (int i = 0; i < nb_namespaces * 2 ; i++)
            callback->namespaces[i] = xmlStrdup(namespaces[i]);
        callback->nb_attributes = nb_attributes;
        callback->nb_defaulted = nb_defaulted;
        callback->attributes =  reinterpret_cast<xmlChar**>(xmlMalloc(sizeof(xmlChar*) * nb_attributes * 5));
        for (int i = 0; i < nb_attributes; i++) {
            // Each attribute has 5 elements in the array:
            // name, prefix, uri, value and an end pointer.
            
            for (int j = 0; j < 3; j++)
                callback->attributes[i * 5 + j] = xmlStrdup(attributes[i * 5 + j]);
            
            int len = attributes[i * 5 + 4] - attributes[i * 5 + 3];

            callback->attributes[i * 5 + 3] = xmlStrndup(attributes[i * 5 + 3], len);
            callback->attributes[i * 5 + 4] = callback->attributes[i * 5 + 3] + len;
        }
        
        m_callbacks.append(callback);
    }

    void appendEndElementNSCallback()
    {
        PendingEndElementNSCallback* callback = new PendingEndElementNSCallback;
        
        m_callbacks.append(callback);
    }
    
    void appendCharactersCallback(const xmlChar* s, int len)
    {
        PendingCharactersCallback* callback = new PendingCharactersCallback;
        
        callback->s = xmlStrndup(s, len);
        callback->len = len;
        
        m_callbacks.append(callback);        
    }
    
    void appendProcessingInstructionCallback(const xmlChar* target, const xmlChar* data)
    {
        PendingProcessingInstructionCallback* callback = new PendingProcessingInstructionCallback;
        
        callback->target = xmlStrdup(target);
        callback->data = xmlStrdup(data);
        
        m_callbacks.append(callback);
    }
    
    void appendCDATABlockCallback(const xmlChar* s, int len)
    {
        PendingCDATABlockCallback* callback = new PendingCDATABlockCallback;
        
        callback->s = xmlStrndup(s, len);
        callback->len = len;
        
        m_callbacks.append(callback);        
    }

    void appendCommentCallback(const xmlChar* s)
    {
        PendingCommentCallback* callback = new PendingCommentCallback;
        
        callback->s = xmlStrdup(s);
        
        m_callbacks.append(callback);        
    }

    void appendInternalSubsetCallback(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID)
    {
        PendingInternalSubsetCallback* callback = new PendingInternalSubsetCallback;
        
        callback->name = xmlStrdup(name);
        callback->externalID = xmlStrdup(externalID);
        callback->systemID = xmlStrdup(systemID);
        
        m_callbacks.append(callback);        
    }
    
    void appendErrorCallback(XMLTokenizer::ErrorType type, const char* message, int lineNumber, int columnNumber)
    {
        PendingErrorCallback* callback = new PendingErrorCallback;
        
        callback->message = strdup(message);
        callback->type = type;
        callback->lineNumber = lineNumber;
        callback->columnNumber = columnNumber;
        
        m_callbacks.append(callback);
    }

    void callAndRemoveFirstCallback(XMLTokenizer* tokenizer)
    {
        PendingCallback* cb = m_callbacks.getFirst();
            
        cb->call(tokenizer);
        m_callbacks.removeFirst();
    }
    
    bool isEmpty() const { return m_callbacks.isEmpty(); }
    
private:    
    struct PendingCallback {        
        
        virtual ~PendingCallback() { } 

        virtual void call(XMLTokenizer* tokenizer) = 0;
    };  
    
    struct PendingStartElementNSCallback : public PendingCallback {        
        virtual ~PendingStartElementNSCallback() {
            xmlFree(xmlLocalName);
            xmlFree(xmlPrefix);
            xmlFree(xmlURI);
            for (int i = 0; i < nb_namespaces * 2; i++)
                xmlFree(namespaces[i]);
            xmlFree(namespaces);
            for (int i = 0; i < nb_attributes; i++)
                for (int j = 0; j < 4; j++) 
                    xmlFree(attributes[i * 5 + j]);
            xmlFree(attributes);
        }
        
        virtual void call(XMLTokenizer* tokenizer) {
            tokenizer->startElementNs(xmlLocalName, xmlPrefix, xmlURI, 
                                      nb_namespaces, (const xmlChar**)namespaces,
                                      nb_attributes, nb_defaulted, (const xmlChar**)(attributes));
        }

        xmlChar* xmlLocalName;
        xmlChar* xmlPrefix;
        xmlChar* xmlURI;
        int nb_namespaces;
        xmlChar** namespaces;
        int nb_attributes;
        int nb_defaulted;
        xmlChar** attributes;
    };
    
    struct PendingEndElementNSCallback : public PendingCallback {
        virtual void call(XMLTokenizer* tokenizer) 
        {
            tokenizer->endElementNs();
        }
    };
    
    struct PendingCharactersCallback : public PendingCallback {
        virtual ~PendingCharactersCallback() 
        {
            xmlFree(s);
        }
    
        virtual void call(XMLTokenizer* tokenizer) 
        {
            tokenizer->characters(s, len);
        }
        
        xmlChar* s;
        int len;
    };

    struct PendingProcessingInstructionCallback : public PendingCallback {
        virtual ~PendingProcessingInstructionCallback() 
        {
            xmlFree(target);
            xmlFree(data);
        }
        
        virtual void call(XMLTokenizer* tokenizer) 
        {
            tokenizer->processingInstruction(target, data);
        }
        
        xmlChar* target;
        xmlChar* data;
    };
    
    struct PendingCDATABlockCallback : public PendingCallback {
        virtual ~PendingCDATABlockCallback() 
        {
            xmlFree(s);
        }
        
        virtual void call(XMLTokenizer* tokenizer) 
        {
            tokenizer->cdataBlock(s, len);
        }
        
        xmlChar* s;
        int len;
    };

    struct PendingCommentCallback : public PendingCallback {
        virtual ~PendingCommentCallback() 
        {
            xmlFree(s);
        }
        
        virtual void call(XMLTokenizer* tokenizer) 
        {
            tokenizer->comment(s);
        }

        xmlChar* s;
    };
    
    struct PendingInternalSubsetCallback : public PendingCallback {
        virtual ~PendingInternalSubsetCallback() 
        {
            xmlFree(name);
            xmlFree(externalID);
            xmlFree(systemID);
        }
        
        virtual void call(XMLTokenizer* tokenizer)
        {
            tokenizer->internalSubset(name, externalID, systemID);
        }
        
        xmlChar* name;
        xmlChar* externalID;
        xmlChar* systemID;        
    };
    
    struct PendingErrorCallback: public PendingCallback {
        virtual ~PendingErrorCallback() 
        {
            free (message);
        }
        
        virtual void call(XMLTokenizer* tokenizer) 
        {
            tokenizer->handleError(type, message, lineNumber, columnNumber);
        }
        
        XMLTokenizer::ErrorType type;
        char* message;
        int lineNumber;
        int columnNumber;
    };
    
public:
    DeprecatedPtrList<PendingCallback> m_callbacks;
};
#endif
// --------------------------------

static int globalDescriptor = 0;

static int matchFunc(const char* uri)
{
    return 1; // Match everything.
}

static DocLoader* globalDocLoader = 0;

class OffsetBuffer {
public:
    OffsetBuffer(const Vector<char>& b) : m_buffer(b), m_currentOffset(0) { }
    
    int readOutBytes(char* outputBuffer, unsigned askedToRead) {
        unsigned bytesLeft = m_buffer.size() - m_currentOffset;
        unsigned lenToCopy = min(askedToRead, bytesLeft);
        if (lenToCopy) {
            memcpy(outputBuffer, m_buffer.data() + m_currentOffset, lenToCopy);
            m_currentOffset += lenToCopy;
        }
        return lenToCopy;
    }

private:
    Vector<char> m_buffer;
    unsigned m_currentOffset;
};

#ifndef USE_QXMLSTREAM
static bool shouldAllowExternalLoad(const char* inURI)
{
    if (strstr(inURI, "/etc/xml/catalog")
            || strstr(inURI, "http://www.w3.org/Graphics/SVG") == inURI
            || strstr(inURI, "http://www.w3.org/TR/xhtml") == inURI)
        return false;
    return true;
}
static void* openFunc(const char* uri)
{
    if (!globalDocLoader || !shouldAllowExternalLoad(uri))
        return &globalDescriptor;

    ResourceError error;
    ResourceResponse response;
    Vector<char> data;
    
    if (globalDocLoader->frame()) 
        globalDocLoader->frame()->loader()->loadResourceSynchronously(KURL(uri), error, response, data);

    return new OffsetBuffer(data);
}

static int readFunc(void* context, char* buffer, int len)
{
    // Do 0-byte reads in case of a null descriptor
    if (context == &globalDescriptor)
        return 0;
        
    OffsetBuffer* data = static_cast<OffsetBuffer*>(context);
    return data->readOutBytes(buffer, len);
}

static int writeFunc(void* context, const char* buffer, int len)
{
    // Always just do 0-byte writes
    return 0;
}

static int closeFunc(void* context)
{
    if (context != &globalDescriptor) {
        OffsetBuffer* data = static_cast<OffsetBuffer*>(context);
        delete data;
    }
    return 0;
}

static void errorFunc(void*, const char*, ...)
{
    // FIXME: It would be nice to display error messages somewhere.
}

void setLoaderForLibXMLCallbacks(DocLoader* docLoader)
{
    globalDocLoader = docLoader;
}

static xmlParserCtxtPtr createStringParser(xmlSAXHandlerPtr handlers, void* userData)
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
    const UChar BOM = 0xFEFF;
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&BOM);
    xmlSwitchEncoding(parser, BOMHighByte == 0xFF ? XML_CHAR_ENCODING_UTF16LE : XML_CHAR_ENCODING_UTF16BE);
    return parser;
}
#endif

// --------------------------------

XMLTokenizer::XMLTokenizer(Document* _doc, FrameView* _view)
    : m_doc(_doc)
    , m_view(_view)
#ifndef USE_QXMLSTREAM
    , m_context(0)
#endif
    , m_currentNode(_doc)
    , m_currentNodeIsReferenced(false)
    , m_sawError(false)
    , m_sawXSLTransform(false)
    , m_sawFirstElement(false)
    , m_isXHTMLDocument(false)
    , m_parserPaused(false)
    , m_requestingScript(false)
    , m_finishCalled(false)
    , m_errorCount(0)
    , m_lastErrorLine(0)
    , m_lastErrorColumn(0)
    , m_pendingScript(0)
    , m_scriptStartLine(0)
    , m_parsingFragment(false)
#ifndef USE_QXMLSTREAM
    , m_pendingCallbacks(new PendingCallbacks)
#endif
{
}

XMLTokenizer::XMLTokenizer(DocumentFragment* fragment, Element* parentElement)
    : m_doc(fragment->document())
    , m_view(0)
#ifndef USE_QXMLSTREAM
    , m_context(0)
#endif
    , m_currentNode(fragment)
    , m_currentNodeIsReferenced(fragment)
    , m_sawError(false)
    , m_sawXSLTransform(false)
    , m_sawFirstElement(false)
    , m_isXHTMLDocument(false)
    , m_parserPaused(false)
    , m_requestingScript(false)
    , m_finishCalled(false)
    , m_errorCount(0)
    , m_lastErrorLine(0)
    , m_lastErrorColumn(0)
    , m_pendingScript(0)
    , m_scriptStartLine(0)
    , m_parsingFragment(true)
#ifndef USE_QXMLSTREAM
    , m_pendingCallbacks(new PendingCallbacks)
#endif
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
    
    for (Element* element = elemStack.last(); !elemStack.isEmpty(); elemStack.removeLast()) {
        if (NamedAttrMap* attrs = element->attributes()) {
            for (unsigned i = 0; i < attrs->length(); i++) {
                Attribute* attr = attrs->attributeItem(i);
                if (attr->localName() == "xmlns")
                    m_defaultNamespaceURI = attr->value();
                else if (attr->prefix() == "xmlns")
                    m_prefixToNamespaceMap.set(attr->localName(), attr->value());
            }
        }
    }
}

XMLTokenizer::~XMLTokenizer()
{
    setCurrentNode(0);
    if (m_parsingFragment && m_doc)
        m_doc->deref();
    if (m_pendingScript)
        m_pendingScript->deref(this);
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

bool XMLTokenizer::write(const SegmentedString& s, bool /*appendData*/)
{
    String parseString = s.toString();
    
    if (m_sawXSLTransform || !m_sawFirstElement)
        m_originalSourceForTransform += parseString;

    if (m_parserStopped || m_sawXSLTransform)
        return false;
    
    if (m_parserPaused) {
        m_pendingSrc.append(s);
        return false;
    }
    
#ifndef USE_QXMLSTREAM
    if (!m_context)
        initializeParserContext();
    
    // libXML throws an error if you try to switch the encoding for an empty string.
    if (parseString.length()) {
        // Hack around libxml2's lack of encoding overide support by manually
        // resetting the encoding to UTF-16 before every chunk.  Otherwise libxml
        // will detect <?xml version="1.0" encoding="<encoding name>"?> blocks 
        // and switch encodings, causing the parse to fail.
        const UChar BOM = 0xFEFF;
        const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&BOM);
        xmlSwitchEncoding(m_context, BOMHighByte == 0xFF ? XML_CHAR_ENCODING_UTF16LE : XML_CHAR_ENCODING_UTF16BE);

        xmlParseChunk(m_context, reinterpret_cast<const char*>(parseString.characters()), sizeof(UChar) * parseString.length(), 0);
    }
#else
    QString data(parseString);
    if (!data.isEmpty()) {
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
                if (!version.isEmpty())
                    m_doc->setXMLVersion(version, ec);
                if (!encoding.isEmpty())
                    m_doc->setXMLEncoding(encoding);
            }
        }
        m_stream.addData(data);
        parse();
    }
#endif
    
    return false;
}
#ifndef USE_QXMLSTREAM
static inline String toString(const xmlChar* str, unsigned len)
{
    return UTF8Encoding().decode(reinterpret_cast<const char*>(str), len);
}

static inline String toString(const xmlChar* str)
{
    if (!str)
        return String();
    
    return UTF8Encoding().decode(reinterpret_cast<const char*>(str), strlen(reinterpret_cast<const char*>(str)));
}

struct _xmlSAX2Namespace {
    const xmlChar* prefix;
    const xmlChar* uri;
};
typedef struct _xmlSAX2Namespace xmlSAX2Namespace;

static inline void handleElementNamespaces(Element* newElement, const xmlChar** libxmlNamespaces, int nb_namespaces, ExceptionCode& ec)
{
    xmlSAX2Namespace* namespaces = reinterpret_cast<xmlSAX2Namespace*>(libxmlNamespaces);
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
    const xmlChar* localname;
    const xmlChar* prefix;
    const xmlChar* uri;
    const xmlChar* value;
    const xmlChar* end;
};
typedef struct _xmlSAX2Attributes xmlSAX2Attributes;

static inline void handleElementAttributes(Element* newElement, const xmlChar** libxmlAttributes, int nb_attributes, ExceptionCode& ec)
{
    xmlSAX2Attributes* attributes = reinterpret_cast<xmlSAX2Attributes*>(libxmlAttributes);
    for(int i = 0; i < nb_attributes; i++) {
        String attrLocalName = toString(attributes[i].localname);
        int valueLength = (int) (attributes[i].end - attributes[i].value);
        String attrValue = toString(attributes[i].value, valueLength);
        String attrPrefix = toString(attributes[i].prefix);
        String attrURI = attrPrefix.isEmpty() ? String() : toString(attributes[i].uri);
        String attrQName = attrPrefix.isEmpty() ? attrLocalName : attrPrefix + ":" + attrLocalName;
        
        newElement->setAttributeNS(attrURI, attrQName, attrValue, ec);
        if (ec) // exception setting attributes
            return;
    }
}

void XMLTokenizer::startElementNs(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int nb_namespaces,
                                  const xmlChar** libxmlNamespaces, int nb_attributes, int nb_defaulted, const xmlChar** libxmlAttributes)
{
    if (m_parserStopped)
        return;
    
    if (m_parserPaused) {
        m_pendingCallbacks->appendStartElementNSCallback(xmlLocalName, xmlPrefix, xmlURI, nb_namespaces, libxmlNamespaces,
                                                         nb_attributes, nb_defaulted, libxmlAttributes);
        return;
    }
    
    m_sawFirstElement = true;

    exitText();

    String localName = toString(xmlLocalName);
    String uri = toString(xmlURI);
    String prefix = toString(xmlPrefix);
    
    if (m_parsingFragment && uri.isNull()) {
        if (!prefix.isNull())
            uri = m_prefixToNamespaceMap.get(prefix);
        else
            uri = m_defaultNamespaceURI;
    }

    ExceptionCode ec = 0;
    QualifiedName qName(prefix, localName, uri);
    RefPtr<Element> newElement = m_doc->createElement(qName, true, ec);
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

    if (newElement->hasTagName(scriptTag))
        static_cast<HTMLScriptElement*>(newElement.get())->setCreatedByParser(true);
    else if (newElement->hasTagName(HTMLNames::styleTag))
        static_cast<HTMLStyleElement*>(newElement.get())->setCreatedByParser(true);
#if ENABLE(SVG)
    else if (newElement->hasTagName(SVGNames::styleTag))
        static_cast<SVGStyleElement*>(newElement.get())->setCreatedByParser(true);
#endif
    
    if (newElement->hasTagName(HTMLNames::scriptTag)
#if ENABLE(SVG)
        || newElement->hasTagName(SVGNames::scriptTag)
#endif
        )
        m_scriptStartLine = lineNumber();
    
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

    if (m_parserPaused) {
        m_pendingCallbacks->appendEndElementNSCallback();
        return;
    }
    
    exitText();

    Node* n = m_currentNode;
    RefPtr<Node> parent = n->parentNode();
    n->finishedParsing();
    
    // don't load external scripts for standalone documents (for now)
    if (n->isElementNode() && m_view && (static_cast<Element*>(n)->hasTagName(scriptTag) 
#if ENABLE(SVG)
                                         || static_cast<Element*>(n)->hasTagName(SVGNames::scriptTag)
#endif
                                         )) {

                                         
        ASSERT(!m_pendingScript);
        
        m_requestingScript = true;
        
        Element* scriptElement = static_cast<Element*>(n);        
        String scriptHref;
        
        if (static_cast<Element*>(n)->hasTagName(scriptTag))
            scriptHref = scriptElement->getAttribute(srcAttr);
#if ENABLE(SVG)
        else if (static_cast<Element*>(n)->hasTagName(SVGNames::scriptTag))
            scriptHref = scriptElement->getAttribute(XLinkNames::hrefAttr);
#endif
        
        if (!scriptHref.isEmpty()) {
            // we have a src attribute 
            const AtomicString& charset = scriptElement->getAttribute(charsetAttr);
            if ((m_pendingScript = m_doc->docLoader()->requestScript(scriptHref, charset))) {
                m_scriptElement = scriptElement;
                m_pendingScript->ref(this);
                    
                // m_pendingScript will be 0 if script was already loaded and ref() executed it
                if (m_pendingScript)
                    pauseParsing();
            } else 
                m_scriptElement = 0;

        } else {
            String scriptCode = "";
            for (Node* child = scriptElement->firstChild(); child; child = child->nextSibling()) {
                if (child->isTextNode() || child->nodeType() == Node::CDATA_SECTION_NODE)
                    scriptCode += static_cast<CharacterData*>(child)->data();
            }
            m_view->frame()->loader()->executeScript(m_doc->URL(), m_scriptStartLine - 1, scriptCode);
        }
        
        m_requestingScript = false;
    }

    setCurrentNode(parent.get());
}

void XMLTokenizer::characters(const xmlChar* s, int len)
{
    if (m_parserStopped)
        return;
    
    if (m_parserPaused) {
        m_pendingCallbacks->appendCharactersCallback(s, len);
        return;
    }
    
    if (m_currentNode->isTextNode() || enterText()) {
        ExceptionCode ec = 0;
        static_cast<Text*>(m_currentNode)->appendData(toString(s, len), ec);
    }
}

void XMLTokenizer::error(ErrorType type, const char* message, va_list args)
{
    if (m_parserStopped)
        return;

#if PLATFORM(WIN_OS)
    char m[1024];
    vsnprintf(m, sizeof(m) - 1, message, args);
#else
    char* m;
    vasprintf(&m, message, args);
#endif
    
    if (m_parserPaused)
        m_pendingCallbacks->appendErrorCallback(type, m, lineNumber(), columnNumber());
    else
        handleError(type, m, lineNumber(), columnNumber());

#if !PLATFORM(WIN_OS)
    free(m);
#endif
}

void XMLTokenizer::processingInstruction(const xmlChar* target, const xmlChar* data)
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendProcessingInstructionCallback(target, data);
        return;
    }
    
    exitText();

    // ### handle exceptions
    int exception = 0;
    RefPtr<ProcessingInstruction> pi = m_doc->createProcessingInstruction(
        toString(target), toString(data), exception);
    if (exception)
        return;

    if (!m_currentNode->addChild(pi.get()))
        return;
    if (m_view && !pi->attached())
        pi->attach();

    // don't load stylesheets for standalone documents
    if (m_doc->frame()) {
        m_sawXSLTransform = !m_sawFirstElement && !pi->checkStyleSheet();
#if ENABLE(XSLT)
        // Pretend we didn't see this PI if we're the result of a transform.
        if (m_sawXSLTransform && !m_doc->transformSourceDocument())
#else
        if (m_sawXSLTransform)
#endif
            // Stop the SAX parser.
            stopParsing();
    }
}

void XMLTokenizer::cdataBlock(const xmlChar* s, int len)
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCDATABlockCallback(s, len);
        return;
    }
    
    exitText();

    RefPtr<Node> newNode = new CDATASection(m_doc, toString(s, len));
    if (!m_currentNode->addChild(newNode.get()))
        return;
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::comment(const xmlChar* s)
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCommentCallback(s);
        return;
    }
    
    exitText();

    RefPtr<Node> newNode = new Comment(m_doc, toString(s));
    m_currentNode->addChild(newNode.get());
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::startDocument(const xmlChar* version, const xmlChar* encoding, int standalone)
{
    ExceptionCode ec = 0;
    
    if (version)
        m_doc->setXMLVersion(toString(version), ec);
    m_doc->setXMLStandalone(standalone == 1, ec); // possible values are 0, 1, and -1
    if (encoding)
        m_doc->setXMLEncoding(toString(encoding));
}

void XMLTokenizer::internalSubset(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID)
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendInternalSubsetCallback(name, externalID, systemID);
        return;
    }
    
    Document* doc = m_doc;
    if (!doc)
        return;

    doc->setDocType(new DocumentType(doc, toString(name), toString(externalID), toString(systemID)));
}

static inline XMLTokenizer* getTokenizer(void* closure)
{
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
    return static_cast<XMLTokenizer*>(ctxt->_private);
}

// This is a hack around http://bugzilla.gnome.org/show_bug.cgi?id=159219
// Otherwise libxml seems to call all the SAX callbacks twice for any replaced entity.
static inline bool hackAroundLibXMLEntityBug(void* closure)
{
#if LIBXML_VERSION >= 20627
    // This bug has been fixed in libxml 2.6.27.
    return false;
#else
    return static_cast<xmlParserCtxtPtr>(closure)->node;
#endif
}

static void startElementNsHandler(void* closure, const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri, int nb_namespaces, const xmlChar** namespaces, int nb_attributes, int nb_defaulted, const xmlChar** libxmlAttributes)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;

    getTokenizer(closure)->startElementNs(localname, prefix, uri, nb_namespaces, namespaces, nb_attributes, nb_defaulted, libxmlAttributes);
}

static void endElementNsHandler(void* closure, const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;
    
    getTokenizer(closure)->endElementNs();
}

static void charactersHandler(void* closure, const xmlChar* s, int len)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;
    
    getTokenizer(closure)->characters(s, len);
}

static void processingInstructionHandler(void* closure, const xmlChar* target, const xmlChar* data)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;
    
    getTokenizer(closure)->processingInstruction(target, data);
}

static void cdataBlockHandler(void* closure, const xmlChar* s, int len)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;
    
    getTokenizer(closure)->cdataBlock(s, len);
}

static void commentHandler(void* closure, const xmlChar* comment)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;
    
    getTokenizer(closure)->comment(comment);
}

static void warningHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getTokenizer(closure)->error(XMLTokenizer::warning, message, args);
    va_end(args);
}

static void fatalErrorHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getTokenizer(closure)->error(XMLTokenizer::fatal, message, args);
    va_end(args);
}

static void normalErrorHandler(void* closure, const char* message, ...)
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
    UChar c = decodeNamedEntity(reinterpret_cast<const char*>(name));
    if (!c)
        return 0;

    CString value = String(&c, 1).utf8();
    ASSERT(value.length() < 5);
    sharedXHTMLEntity.length = value.length();
    sharedXHTMLEntity.name = name;
    memcpy(sharedXHTMLEntityResult, value.data(), sharedXHTMLEntity.length + 1);

    return &sharedXHTMLEntity;
}

static xmlEntityPtr getEntityHandler(void* closure, const xmlChar* name)
{
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
    xmlEntityPtr ent = xmlGetPredefinedEntity(name);
    if (ent) {
        ent->etype = XML_INTERNAL_PREDEFINED_ENTITY;
        return ent;
    }

    ent = xmlGetDocEntity(ctxt->myDoc, name);
    if (!ent && getTokenizer(closure)->isXHTMLDocument()) {
        ent = getXHTMLEntity(name);
        if (ent)
            ent->etype = XML_INTERNAL_GENERAL_ENTITY;
    }
    
    return ent;
}

static void startDocumentHandler(void* closure)
{
    xmlParserCtxt* ctxt = static_cast<xmlParserCtxt*>(closure);
    getTokenizer(closure)->startDocument(ctxt->version, ctxt->encoding, ctxt->standalone);
    xmlSAX2StartDocument(closure);
}

static void internalSubsetHandler(void* closure, const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID)
{
    getTokenizer(closure)->internalSubset(name, externalID, systemID);
    xmlSAX2InternalSubset(closure, name, externalID, systemID);
}

static void externalSubsetHandler(void* closure, const xmlChar* name, const xmlChar* externalId, const xmlChar* systemId)
{
    String extId = toString(externalId);
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

static void ignorableWhitespaceHandler(void* ctx, const xmlChar* ch, int len)
{
    // nothing to do, but we need this to work around a crasher
    // http://bugzilla.gnome.org/show_bug.cgi?id=172255
    // http://bugs.webkit.org/show_bug.cgi?id=5792
}
#endif

void XMLTokenizer::handleError(ErrorType type, const char* m, int lineNumber, int columnNumber)
{
    if (type == fatal || (m_errorCount < maxErrors && m_lastErrorLine != lineNumber && m_lastErrorColumn != columnNumber)) {
        switch (type) {
            case warning:
                m_errorMessages += String::format("warning on line %d at column %d: %s", lineNumber, columnNumber, m);
                break;
            case fatal:
            case nonFatal:
                m_errorMessages += String::format("error on line %d at column %d: %s", lineNumber, columnNumber, m);
        }
        
        m_lastErrorLine = lineNumber;
        m_lastErrorColumn = columnNumber;
        ++m_errorCount;
    }
    
    if (type != warning)
        m_sawError = true;
    
    if (type == fatal)
        stopParsing();    
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

void XMLTokenizer::initializeParserContext()
{
#ifndef USE_QXMLSTREAM
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
    sax.startDocument = startDocumentHandler;
    sax.internalSubset = internalSubsetHandler;
    sax.externalSubset = externalSubsetHandler;
    sax.ignorableWhitespace = ignorableWhitespaceHandler;
    sax.entityDecl = xmlSAX2EntityDecl;
    sax.initialized = XML_SAX2_MAGIC;
#endif
    m_parserStopped = false;
    m_sawError = false;
    m_sawXSLTransform = false;
    m_sawFirstElement = false;
    
#ifndef USE_QXMLSTREAM
    m_context = createStringParser(&sax, this);
#endif
}

void XMLTokenizer::end()
{
#if ENABLE(XSLT)
    if (m_sawXSLTransform) {
        m_doc->setTransformSource(xmlDocPtrForString(m_doc->docLoader(), m_originalSourceForTransform, m_doc->URL()));
        
        m_doc->setParsing(false); // Make the doc think it's done, so it will apply xsl sheets.
        m_doc->updateStyleSelector();
        m_doc->setParsing(true);
        m_parserStopped = true;
    }
#endif

#ifndef USE_QXMLSTREAM
    if (m_context) {
        // Tell libxml we're done.
        xmlParseChunk(m_context, 0, 0, 1);
        
        if (m_context->myDoc)
            xmlFreeDoc(m_context->myDoc);
        xmlFreeParserCtxt(m_context);
        m_context = 0;
    }
#else
    if (m_stream.error() == QXmlStreamReader::PrematureEndOfDocumentError) {
        handleError(fatal, qPrintable(m_stream.errorString()), lineNumber(),
                    columnNumber());
    }
#endif
    
    if (m_sawError)
        insertErrorMessageBlock();
    else {
        exitText();
        m_doc->updateStyleSelector();
    }
    
    setCurrentNode(0);
    m_doc->finishedParsing();    
}

void XMLTokenizer::finish()
{
    if (m_parserPaused)
        m_finishCalled = true;
    else
        end();
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
    Document* doc = m_doc;
    Node* documentElement = doc->documentElement();
    if (!documentElement) {
        RefPtr<Node> rootElement = doc->createElementNS(xhtmlNamespaceURI, "html", ec);
        doc->appendChild(rootElement, ec);
        RefPtr<Node> body = doc->createElementNS(xhtmlNamespaceURI, "body", ec);
        rootElement->appendChild(body, ec);
        documentElement = body.get();
    }
#if ENABLE(SVG)
    else if (documentElement->namespaceURI() == SVGNames::svgNamespaceURI) {
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
#if ENABLE(XSLT)
    if (doc->transformSourceDocument()) {
        RefPtr<Element> par = doc->createElementNS(xhtmlNamespaceURI, "p", ec);
        reportElement->appendChild(par, ec);
        par->setAttribute(styleAttr, "white-space: normal");
        par->appendChild(doc->createTextNode("This document was created as the result of an XSL transformation. The line and column numbers given are from the transformed result."), ec);
    }
#endif
    doc->updateRendering();
}

void XMLTokenizer::notifyFinished(CachedResource* finishedObj)
{
    ASSERT(m_pendingScript == finishedObj);
    ASSERT(m_pendingScript->accessCount() > 0);
        
    String cachedScriptUrl = m_pendingScript->url();
    String scriptSource = m_pendingScript->script();
    bool errorOccurred = m_pendingScript->errorOccurred();
    m_pendingScript->deref(this);
    m_pendingScript = 0;
    
    RefPtr<Element> e = m_scriptElement;
    m_scriptElement = 0;
    
    if (errorOccurred) 
        EventTargetNodeCast(e.get())->dispatchHTMLEvent(errorEvent, true, false);
    else {
        m_view->frame()->loader()->executeScript(cachedScriptUrl, 0, scriptSource);
        EventTargetNodeCast(e.get())->dispatchHTMLEvent(loadEvent, false, false);
    }
    
    m_scriptElement = 0;
    
    if (!m_requestingScript)
        resumeParsing();
}

bool XMLTokenizer::isWaitingForScripts() const
{
    return m_pendingScript != 0;
}

#if ENABLE(XSLT)
void* xmlDocPtrForString(DocLoader* docLoader, const String& source, const DeprecatedString& url)
{
    if (source.isEmpty())
        return 0;

    // Parse in a single chunk into an xmlDocPtr
    // FIXME: Hook up error handlers so that a failure to parse the main document results in
    // good error messages.
    const UChar BOM = 0xFEFF;
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&BOM);

    xmlGenericErrorFunc oldErrorFunc = xmlGenericError;
    void* oldErrorContext = xmlGenericErrorContext;
    
    setLoaderForLibXMLCallbacks(docLoader);        
    xmlSetGenericErrorFunc(0, errorFunc);
    
    xmlDocPtr sourceDoc = xmlReadMemory(reinterpret_cast<const char*>(source.characters()),
                                        source.length() * sizeof(UChar),
                                        url.ascii(),
                                        BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", 
                                        XSLT_PARSE_OPTIONS);
    
    setLoaderForLibXMLCallbacks(0);
    xmlSetGenericErrorFunc(oldErrorContext, oldErrorFunc);
    
    return sourceDoc;
}
#endif

int XMLTokenizer::lineNumber() const
{
#ifndef USE_QXMLSTREAM
    return m_context->input->line;
#else
    return m_stream.lineNumber();
#endif
}

int XMLTokenizer::columnNumber() const
{
#ifndef USE_QXMLSTREAM
    return m_context->input->col;
#else
    return m_stream.columnNumber();
#endif
}

void XMLTokenizer::stopParsing()
{
    Tokenizer::stopParsing();
#ifndef USE_QXMLSTREAM
    xmlStopParser(m_context);
#endif
}

void XMLTokenizer::pauseParsing()
{
    if (m_parsingFragment)
        return;
    
    m_parserPaused = true;
}

void XMLTokenizer::resumeParsing()
{
    ASSERT(m_parserPaused);
    
    m_parserPaused = false;

    // First, execute any pending callbacks
#ifndef USE_QXMLSTREAM
    while (!m_pendingCallbacks->isEmpty()) {
        m_pendingCallbacks->callAndRemoveFirstCallback(this);
        
        // A callback paused the parser
        if (m_parserPaused)
            return;
    }
#else
    parse();
    if (m_parserPaused)
        return;
#endif

    // Then, write any pending data
    SegmentedString rest = m_pendingSrc;
    m_pendingSrc.clear();
    write(rest, false);

    // Finally, if finish() has been called and write() didn't result
    // in any further callbacks being queued, call end()
    if (m_finishCalled
#ifndef USE_QXMLSTREAM
        && m_pendingCallbacks->isEmpty())
#else
        )
#endif
        end();
}

#ifndef USE_QXMLSTREAM
static void balancedStartElementNsHandler(void* closure, const xmlChar* localname, const xmlChar* prefix,
                                          const xmlChar* uri, int nb_namespaces, const xmlChar** namespaces,
                                          int nb_attributes, int nb_defaulted, const xmlChar** libxmlAttributes)
{
   static_cast<XMLTokenizer*>(closure)->startElementNs(localname, prefix, uri, nb_namespaces, namespaces, nb_attributes, nb_defaulted, libxmlAttributes);
}

static void balancedEndElementNsHandler(void* closure, const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri)
{
    static_cast<XMLTokenizer*>(closure)->endElementNs();
}

static void balancedCharactersHandler(void* closure, const xmlChar* s, int len)
{
    static_cast<XMLTokenizer*>(closure)->characters(s, len);
}

static void balancedProcessingInstructionHandler(void* closure, const xmlChar* target, const xmlChar* data)
{
    static_cast<XMLTokenizer*>(closure)->processingInstruction(target, data);
}

static void balancedCdataBlockHandler(void* closure, const xmlChar* s, int len)
{
    static_cast<XMLTokenizer*>(closure)->cdataBlock(s, len);
}

static void balancedCommentHandler(void* closure, const xmlChar* comment)
{
    static_cast<XMLTokenizer*>(closure)->comment(comment);
}

static void balancedWarningHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    static_cast<XMLTokenizer*>(closure)->error(XMLTokenizer::warning, message, args);
    va_end(args);
}
#endif
bool parseXMLDocumentFragment(const String& string, DocumentFragment* fragment, Element* parent)
{
    XMLTokenizer tokenizer(fragment, parent);
    
#ifndef USE_QXMLSTREAM
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
    
    int result = xmlParseBalancedChunkMemory(0, &sax, &tokenizer, 0, (const xmlChar*)string.utf8().data(), 0);
    return result == 0;
#else
    tokenizer.write(string, false);
    tokenizer.finish();
    return tokenizer.hasError();
#endif
}

// --------------------------------

struct AttributeParseState {
    HashMap<String, String> attributes;
    bool gotAttributes;
};

#ifndef USE_QXMLSTREAM
static void attributesStartElementNsHandler(void* closure, const xmlChar* xmlLocalName, const xmlChar* xmlPrefix,
                                            const xmlChar* xmlURI, int nb_namespaces, const xmlChar** namespaces,
                                            int nb_attributes, int nb_defaulted, const xmlChar** libxmlAttributes)
{
    if (strcmp(reinterpret_cast<const char*>(xmlLocalName), "attrs") != 0)
        return;
    
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
    AttributeParseState* state = static_cast<AttributeParseState*>(ctxt->_private);
    
    state->gotAttributes = true;
    
    xmlSAX2Attributes* attributes = reinterpret_cast<xmlSAX2Attributes*>(libxmlAttributes);
    for(int i = 0; i < nb_attributes; i++) {
        String attrLocalName = toString(attributes[i].localname);
        int valueLength = (int) (attributes[i].end - attributes[i].value);
        String attrValue = toString(attributes[i].value, valueLength);
        String attrPrefix = toString(attributes[i].prefix);
        String attrQName = attrPrefix.isEmpty() ? attrLocalName : attrPrefix + ":" + attrLocalName;
        
        state->attributes.set(attrQName, attrValue);
    }
}
#else
static void attributesStartElementNsHandler(AttributeParseState* state, const QXmlStreamAttributes& attrs)
{
    if (attrs.count() <= 0)
        return;

    state->gotAttributes = true;

    for(int i = 0; i < attrs.count(); i++) {
        const QXmlStreamAttribute& attr = attrs[i];
        String attrLocalName = attr.name();
        String attrValue     = attr.value();
        String attrURI       = attr.namespaceUri();
        String attrQName     = attr.qualifiedName();
        state->attributes.set(attrQName, attrValue);
    }
}
#endif

HashMap<String, String> parseAttributes(const String& string, bool& attrsOK)
{
    AttributeParseState state;
    state.gotAttributes = false;

#ifndef USE_QXMLSTREAM
    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));
    sax.startElementNs = attributesStartElementNsHandler;
    sax.initialized = XML_SAX2_MAGIC;
    xmlParserCtxtPtr parser = createStringParser(&sax, &state);
    String parseString = "<?xml version=\"1.0\"?><attrs " + string + " />";
    xmlParseChunk(parser, reinterpret_cast<const char*>(parseString.characters()), parseString.length() * sizeof(UChar), 1);
    if (parser->myDoc)
        xmlFreeDoc(parser->myDoc);
    xmlFreeParserCtxt(parser);
#else
    QXmlStreamReader stream;
    QString dummy = QString("<?xml version=\"1.0\"?><attrs %1 />").arg(string);
    stream.addData(dummy);
    while (!stream.atEnd()) {
        stream.readNext();
        if (stream.isStartElement()) {
            attributesStartElementNsHandler(&state, stream.attributes());
        }
    }
#endif
    attrsOK = state.gotAttributes;
    return state.attributes;
}

#ifdef USE_QXMLSTREAM
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
        }
            break;
        case QXmlStreamReader::EntityReference: {
            //qDebug()<<"---------- ENTITY = "<<m_stream.name().toString()
            //        <<", t = "<<m_stream.text().toString();
            if (isXHTMLDocument()) {
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

    m_doc->setXMLStandalone(m_stream.isStandaloneDocument(), ec);
}

void XMLTokenizer::parseStartElement()
{
    m_sawFirstElement = true;

    exitText();

    String localName = m_stream.name();
    String uri       = m_stream.namespaceUri();
    String prefix    = prefixFromQName(m_stream.qualifiedName().toString());

    if (m_parsingFragment && uri.isNull()) {
        if (!prefix.isNull())
            uri = m_prefixToNamespaceMap.get(prefix);
        else
            uri = m_defaultNamespaceURI;
    }

    ExceptionCode ec = 0;
    QualifiedName qName(prefix, localName, uri);
    RefPtr<Element> newElement = m_doc->createElement(qName, true, ec);
    if (!newElement) {
        stopParsing();
        return;
    }

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

    if (newElement->hasTagName(scriptTag))
        static_cast<HTMLScriptElement*>(newElement.get())->setCreatedByParser(true);

    if (newElement->hasTagName(HTMLNames::scriptTag)
#if ENABLE(SVG)
        || newElement->hasTagName(SVGNames::scriptTag)
#endif
        )
        m_scriptStartLine = lineNumber();

    if (!m_currentNode->addChild(newElement.get())) {
        stopParsing();
        return;
    }

    setCurrentNode(newElement.get());
    if (m_view && !newElement->attached())
        newElement->attach();
}

void XMLTokenizer::parseEndElement()
{
    exitText();

    Node* n = m_currentNode;
    RefPtr<Node> parent = n->parentNode();
    n->finishedParsing();

    // don't load external scripts for standalone documents (for now)
    if (n->isElementNode() && m_view && (static_cast<Element*>(n)->hasTagName(scriptTag) 
#if ENABLE(SVG)
                                         || static_cast<Element*>(n)->hasTagName(SVGNames::scriptTag)
#endif
                                         )) {


        ASSERT(!m_pendingScript);

        m_requestingScript = true;

        Element* scriptElement = static_cast<Element*>(n);
        String scriptHref;

        if (static_cast<Element*>(n)->hasTagName(scriptTag))
            scriptHref = scriptElement->getAttribute(srcAttr);
#if ENABLE(SVG)
        else if (static_cast<Element*>(n)->hasTagName(SVGNames::scriptTag))
            scriptHref = scriptElement->getAttribute(XLinkNames::hrefAttr);
#endif
        if (!scriptHref.isEmpty()) {
            // we have a src attribute
            const AtomicString& charset = scriptElement->getAttribute(charsetAttr);
            if ((m_pendingScript = m_doc->docLoader()->requestScript(scriptHref, charset))) {
                m_scriptElement = scriptElement;
                m_pendingScript->ref(this);

                // m_pendingScript will be 0 if script was already loaded and ref() executed it
                if (m_pendingScript)
                    pauseParsing();
            } else
                m_scriptElement = 0;

        } else {
            String scriptCode = "";
            for (Node* child = scriptElement->firstChild(); child; child = child->nextSibling()) {
                if (child->isTextNode() || child->nodeType() == Node::CDATA_SECTION_NODE)
                    scriptCode += static_cast<CharacterData*>(child)->data();
            }
            m_view->frame()->loader()->executeScript(m_doc->URL(), m_scriptStartLine - 1, scriptCode);
        }
        m_requestingScript = false;
    }

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

    if (!m_currentNode->addChild(pi.get()))
        return;
    if (m_view && !pi->attached())
        pi->attach();

    // don't load stylesheets for standalone documents
    if (m_doc->frame()) {
        m_sawXSLTransform = !m_sawFirstElement && !pi->checkStyleSheet();
        if (m_sawXSLTransform)
            stopParsing();
    }
}

void XMLTokenizer::parseCdata()
{
    exitText();

    RefPtr<Node> newNode = new CDATASection(m_doc, m_stream.text());
    if (!m_currentNode->addChild(newNode.get()))
        return;
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::parseComment()
{
    exitText();

    RefPtr<Node> newNode = new Comment(m_doc, m_stream.text());
    m_currentNode->addChild(newNode.get());
    if (m_view && !newNode->attached())
        newNode->attach();
}

void XMLTokenizer::endDocument()
{
}

bool XMLTokenizer::hasError() const
{
    return m_stream.hasError();
}

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

void XMLTokenizer::parseDtd()
{
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
    
    //qDebug() << dtd << name << publicId << systemId;
    const QXmlStreamNotationDeclarations& decls = m_stream.notationDeclarations();

    if ((publicId == "-//W3C//DTD XHTML 1.0 Transitional//EN")
        || (publicId == "-//W3C//DTD XHTML 1.1//EN")
        || (publicId == "-//W3C//DTD XHTML 1.0 Strict//EN")
        || (publicId == "-//W3C//DTD XHTML 1.0 Frameset//EN")
        || (publicId == "-//W3C//DTD XHTML Basic 1.0//EN")
        || (publicId == "-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN")
        || (publicId == "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN")
        || (publicId == "-//WAPFORUM//DTD XHTML Mobile 1.0//EN")) {
        setIsXHTMLDocument(true); // controls if we replace entities or not.
    }
    m_doc->setDocType(new DocumentType(m_doc, name, publicId, systemId));
    
}
#endif
}


