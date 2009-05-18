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
#include "HTMLTokenizer.h" // for decodeNamedEntity
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
#include "XMLTokenizerScope.h"
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <wtf/Platform.h>
#include <wtf/StringExtras.h>
#include <wtf/Threading.h>
#include <wtf/UnusedParam.h>
#include <wtf/Vector.h>

#if ENABLE(XSLT)
#include <libxslt/xslt.h>
#endif

#if ENABLE(XHTMLMP)
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#endif

using namespace std;

namespace WebCore {

class PendingCallbacks : Noncopyable {
public:
    ~PendingCallbacks()
    {
        deleteAllValues(m_callbacks);
    }
    
    void appendStartElementNSCallback(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int nb_namespaces,
                                      const xmlChar** namespaces, int nb_attributes, int nb_defaulted, const xmlChar** attributes)
    {
        PendingStartElementNSCallback* callback = new PendingStartElementNSCallback;
        
        callback->xmlLocalName = xmlStrdup(xmlLocalName);
        callback->xmlPrefix = xmlStrdup(xmlPrefix);
        callback->xmlURI = xmlStrdup(xmlURI);
        callback->nb_namespaces = nb_namespaces;
        callback->namespaces = static_cast<xmlChar**>(xmlMalloc(sizeof(xmlChar*) * nb_namespaces * 2));
        for (int i = 0; i < nb_namespaces * 2 ; i++)
            callback->namespaces[i] = xmlStrdup(namespaces[i]);
        callback->nb_attributes = nb_attributes;
        callback->nb_defaulted = nb_defaulted;
        callback->attributes = static_cast<xmlChar**>(xmlMalloc(sizeof(xmlChar*) * nb_attributes * 5));
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
        OwnPtr<PendingCallback> callback(m_callbacks.first());
        m_callbacks.removeFirst();
        callback->call(tokenizer);
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
                                      nb_namespaces, const_cast<const xmlChar**>(namespaces),
                                      nb_attributes, nb_defaulted, const_cast<const xmlChar**>(attributes));
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
    
    Deque<PendingCallback*> m_callbacks;
};
// --------------------------------

static int globalDescriptor = 0;
static ThreadIdentifier libxmlLoaderThread = 0;

static int matchFunc(const char*)
{
    // Only match loads initiated due to uses of libxml2 from within XMLTokenizer to avoid
    // interfering with client applications that also use libxml2.  http://bugs.webkit.org/show_bug.cgi?id=17353
    return XMLTokenizerScope::currentDocLoader && currentThread() == libxmlLoaderThread;
}

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

static bool shouldAllowExternalLoad(const KURL& url)
{
    String urlString = url.string();

    // On non-Windows platforms libxml asks for this URL, the
    // "XML_XML_DEFAULT_CATALOG", on initialization.
    if (urlString == "file:///etc/xml/catalog")
        return false;

    // On Windows, libxml computes a URL relative to where its DLL resides.
    if (urlString.startsWith("file:///", false) && urlString.endsWith("/etc/catalog", false))
        return false;

    // The most common DTD.  There isn't much point in hammering www.w3c.org
    // by requesting this URL for every XHTML document.
    if (urlString.startsWith("http://www.w3.org/TR/xhtml", false))
        return false;

    // Similarly, there isn't much point in requesting the SVG DTD.
    if (urlString.startsWith("http://www.w3.org/Graphics/SVG", false))
        return false;

    // The libxml doesn't give us a lot of context for deciding whether to
    // allow this request.  In the worst case, this load could be for an
    // external entity and the resulting document could simply read the
    // retrieved content.  If we had more context, we could potentially allow
    // the parser to load a DTD.  As things stand, we take the conservative
    // route and allow same-origin requests only.
    if (!XMLTokenizerScope::currentDocLoader->doc()->securityOrigin()->canRequest(url)) {
        XMLTokenizerScope::currentDocLoader->printAccessDeniedMessage(url);
        return false;
    }

    return true;
}

static void* openFunc(const char* uri)
{
    ASSERT(XMLTokenizerScope::currentDocLoader);
    ASSERT(currentThread() == libxmlLoaderThread);

    KURL url(KURL(), uri);

    if (!shouldAllowExternalLoad(url))
        return &globalDescriptor;

    ResourceError error;
    ResourceResponse response;
    Vector<char> data;


    {
        DocLoader* docLoader = XMLTokenizerScope::currentDocLoader;
        XMLTokenizerScope scope(0);
        // FIXME: We should restore the original global error handler as well.

        if (docLoader->frame()) 
            docLoader->frame()->loader()->loadResourceSynchronously(url, AllowStoredCredentials, error, response, data);
    }

    // We have to check the URL again after the load to catch redirects.
    // See <https://bugs.webkit.org/show_bug.cgi?id=21963>.
    if (!shouldAllowExternalLoad(response.url()))
        return &globalDescriptor;

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

static int writeFunc(void*, const char*, int)
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

#if ENABLE(XSLT)
static void errorFunc(void*, const char*, ...)
{
    // FIXME: It would be nice to display error messages somewhere.
}
#endif

static bool didInit = false;

static xmlParserCtxtPtr createStringParser(xmlSAXHandlerPtr handlers, void* userData)
{
    if (!didInit) {
        xmlInitParser();
        xmlRegisterInputCallbacks(matchFunc, openFunc, readFunc, closeFunc);
        xmlRegisterOutputCallbacks(matchFunc, openFunc, writeFunc, closeFunc);
        libxmlLoaderThread = currentThread();
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


// Chunk should be encoded in UTF-8
static xmlParserCtxtPtr createMemoryParser(xmlSAXHandlerPtr handlers, void* userData, const char* chunk)
{
    if (!didInit) {
        xmlInitParser();
        xmlRegisterInputCallbacks(matchFunc, openFunc, readFunc, closeFunc);
        xmlRegisterOutputCallbacks(matchFunc, openFunc, writeFunc, closeFunc);
        libxmlLoaderThread = currentThread();
        didInit = true;
    }

    xmlParserCtxtPtr parser = xmlCreateMemoryParserCtxt(chunk, xmlStrlen((const xmlChar*)chunk));

    if (!parser)
        return 0;

    // Copy the sax handler
    memcpy(parser->sax, handlers, sizeof(xmlSAXHandler));

    // Set parser options.
    // XML_PARSE_NODICT: default dictionary option.
    // XML_PARSE_NOENT: force entities substitutions.
    xmlCtxtUseOptions(parser, XML_PARSE_NODICT | XML_PARSE_NOENT);

    // Internal initialization
    parser->sax2 = 1;
    parser->instate = XML_PARSER_CONTENT; // We are parsing a CONTENT
    parser->depth = 0;
    parser->str_xml = xmlDictLookup(parser->dict, BAD_CAST "xml", 3);
    parser->str_xmlns = xmlDictLookup(parser->dict, BAD_CAST "xmlns", 5);
    parser->str_xml_ns = xmlDictLookup(parser->dict, XML_XML_NAMESPACE, 36);
    parser->_private = userData;

    return parser;
}

// --------------------------------

XMLTokenizer::XMLTokenizer(Document* _doc, FrameView* _view)
    : m_doc(_doc)
    , m_view(_view)
    , m_context(0)
    , m_pendingCallbacks(new PendingCallbacks)
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
}

XMLTokenizer::XMLTokenizer(DocumentFragment* fragment, Element* parentElement)
    : m_doc(fragment->document())
    , m_view(0)
    , m_context(0)
    , m_pendingCallbacks(new PendingCallbacks)
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
    if (m_context)
        xmlFreeParserCtxt(m_context);
}

void XMLTokenizer::doWrite(const String& parseString)
{
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

        XMLTokenizerScope scope(m_doc->docLoader());
        xmlParseChunk(m_context, reinterpret_cast<const char*>(parseString.characters()), sizeof(UChar) * parseString.length(), 0);
    }
    
    if (m_doc->decoder() && m_doc->decoder()->sawError()) {
        // If the decoder saw an error, report it as fatal (stops parsing)
        handleError(fatal, "Encoding error", lineNumber(), columnNumber());
    }

    return;
}

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

#if ENABLE(XHTMLMP) 
    // check if the DOCTYPE Declaration of XHTMLMP document exists
    if (!m_hasDocTypeDeclaration && m_doc->isXHTMLMPDocument()) {
        handleError(fatal, "DOCTYPE declaration lost.", lineNumber(), columnNumber());
        return;
    }
#endif

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

#if ENABLE(XHTMLMP)
    if (!m_sawFirstElement && isXHTMLMPDocument()) {
        // As per the section 7.1 of OMA-WAP-XHTMLMP-V1_1-20061020-A.pdf, 
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
        }
    }
#endif

    bool isFirstElement = !m_sawFirstElement;
    m_sawFirstElement = true;

    QualifiedName qName(prefix, localName, uri);
    RefPtr<Element> newElement = m_doc->createElement(qName, true);
    if (!newElement) {
        stopParsing();
        return;
    }
    
    ExceptionCode ec = 0;
    handleElementNamespaces(newElement.get(), libxmlNamespaces, nb_namespaces, ec);
    if (ec) {
        stopParsing();
        return;
    }

    ScriptController* jsProxy = m_doc->frame() ? m_doc->frame()->script() : 0;
    if (jsProxy && m_doc->frame()->script()->isEnabled())
        jsProxy->setEventHandlerLineNumber(lineNumber());

    handleElementAttributes(newElement.get(), libxmlAttributes, nb_attributes, ec);
    if (ec) {
        stopParsing();
        return;
    }

    if (jsProxy)
        jsProxy->setEventHandlerLineNumber(0);

    newElement->beginParsingChildren();

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

void XMLTokenizer::characters(const xmlChar* s, int len)
{
    if (m_parserStopped)
        return;
    
    if (m_parserPaused) {
        m_pendingCallbacks->appendCharactersCallback(s, len);
        return;
    }
    
    if (m_currentNode->isTextNode() || enterText())
        m_bufferedText.append(s, len);
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
    if (vasprintf(&m, message, args) == -1)
        return;
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

void XMLTokenizer::endDocument()
{
    exitText();
#if ENABLE(XHTMLMP)
    m_hasDocTypeDeclaration = false;
#endif
}

void XMLTokenizer::internalSubset(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID)
{
    if (m_parserStopped)
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendInternalSubsetCallback(name, externalID, systemID);
        return;
    }
    
    if (m_doc) {
#if ENABLE(WML) || ENABLE(XHTMLMP)
        String extId = toString(externalID);
#endif
#if ENABLE(WML)
        if (isWMLDocument()
            && extId != "-//WAPFORUM//DTD WML 1.3//EN"
            && extId != "-//WAPFORUM//DTD WML 1.2//EN"
            && extId != "-//WAPFORUM//DTD WML 1.1//EN"
            && extId != "-//WAPFORUM//DTD WML 1.0//EN")
            handleError(fatal, "Invalid DTD Public ID", lineNumber(), columnNumber());
#endif
#if ENABLE(XHTMLMP)
        String dtdName = toString(name);
        if (extId == "-//WAPFORUM//DTD XHTML Mobile 1.0//EN"
            || extId == "-//WAPFORUM//DTD XHTML Mobile 1.1//EN") {
            if (dtdName != HTMLNames::htmlTag.localName()) {
                handleError(fatal, "Invalid DOCTYPE declaration, expected 'html' as root element.", lineNumber(), columnNumber());
                return;
            } 

            if (m_doc->isXHTMLMPDocument())
                setIsXHTMLMPDocument(true);
            else
                setIsXHTMLDocument(true);

            m_hasDocTypeDeclaration = true;
        }
#endif

#if ENABLE(XHTMLMP)
        m_doc->addChild(DocumentType::create(m_doc, dtdName, extId, toString(systemID)));
#elif ENABLE(WML)
        m_doc->addChild(DocumentType::create(m_doc, toString(name), extId, toString(systemID)));
#else
        m_doc->addChild(DocumentType::create(m_doc, toString(name), toString(externalID), toString(systemID)));
#endif
    }
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
    UNUSED_PARAM(closure);

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

static void endElementNsHandler(void* closure, const xmlChar*, const xmlChar*, const xmlChar*)
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

WTF_ATTRIBUTE_PRINTF(2, 3)
static void warningHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getTokenizer(closure)->error(XMLTokenizer::warning, message, args);
    va_end(args);
}

WTF_ATTRIBUTE_PRINTF(2, 3)
static void fatalErrorHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getTokenizer(closure)->error(XMLTokenizer::fatal, message, args);
    va_end(args);
}

WTF_ATTRIBUTE_PRINTF(2, 3)
static void normalErrorHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getTokenizer(closure)->error(XMLTokenizer::nonFatal, message, args);
    va_end(args);
}

// Using a static entity and marking it XML_INTERNAL_PREDEFINED_ENTITY is
// a hack to avoid malloc/free. Using a global variable like this could cause trouble
// if libxml implementation details were to change
static xmlChar sharedXHTMLEntityResult[5] = {0, 0, 0, 0, 0};

static xmlEntityPtr sharedXHTMLEntity()
{
    static xmlEntity entity;
    if (!entity.type) {
        entity.type = XML_ENTITY_DECL;
        entity.orig = sharedXHTMLEntityResult;
        entity.content = sharedXHTMLEntityResult;
        entity.etype = XML_INTERNAL_PREDEFINED_ENTITY;
    }
    return &entity;
}

static xmlEntityPtr getXHTMLEntity(const xmlChar* name)
{
    UChar c = decodeNamedEntity(reinterpret_cast<const char*>(name));
    if (!c)
        return 0;

    CString value = String(&c, 1).utf8();
    ASSERT(value.length() < 5);
    xmlEntityPtr entity = sharedXHTMLEntity();
    entity->length = value.length();
    entity->name = name;
    memcpy(sharedXHTMLEntityResult, value.data(), entity->length + 1);

    return entity;
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
    if (!ent && (getTokenizer(closure)->isXHTMLDocument()
#if ENABLE(XHTMLMP)
                 || getTokenizer(closure)->isXHTMLMPDocument()
#endif
#if ENABLE(WML)
                 || getTokenizer(closure)->isWMLDocument()
#endif
       )) {
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

static void endDocumentHandler(void* closure)
{
    getTokenizer(closure)->endDocument();
    xmlSAX2EndDocument(closure);
}

static void internalSubsetHandler(void* closure, const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID)
{
    getTokenizer(closure)->internalSubset(name, externalID, systemID);
    xmlSAX2InternalSubset(closure, name, externalID, systemID);
}

static void externalSubsetHandler(void* closure, const xmlChar*, const xmlChar* externalId, const xmlChar*)
{
    String extId = toString(externalId);
    if ((extId == "-//W3C//DTD XHTML 1.0 Transitional//EN")
        || (extId == "-//W3C//DTD XHTML 1.1//EN")
        || (extId == "-//W3C//DTD XHTML 1.0 Strict//EN")
        || (extId == "-//W3C//DTD XHTML 1.0 Frameset//EN")
        || (extId == "-//W3C//DTD XHTML Basic 1.0//EN")
        || (extId == "-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN")
        || (extId == "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN")
#if !ENABLE(XHTMLMP)
        || (extId == "-//WAPFORUM//DTD XHTML Mobile 1.0//EN")
#endif
       )
        getTokenizer(closure)->setIsXHTMLDocument(true); // controls if we replace entities or not.
}

static void ignorableWhitespaceHandler(void*, const xmlChar*, int)
{
    // nothing to do, but we need this to work around a crasher
    // http://bugzilla.gnome.org/show_bug.cgi?id=172255
    // http://bugs.webkit.org/show_bug.cgi?id=5792
}

void XMLTokenizer::initializeParserContext(const char* chunk)
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
    sax.startDocument = startDocumentHandler;
    sax.endDocument = endDocumentHandler;
    sax.internalSubset = internalSubsetHandler;
    sax.externalSubset = externalSubsetHandler;
    sax.ignorableWhitespace = ignorableWhitespaceHandler;
    sax.entityDecl = xmlSAX2EntityDecl;
    sax.initialized = XML_SAX2_MAGIC;
    m_parserStopped = false;
    m_sawError = false;
    m_sawXSLTransform = false;
    m_sawFirstElement = false;

    XMLTokenizerScope scope(m_doc->docLoader());
    if (m_parsingFragment)
        m_context = createMemoryParser(&sax, this, chunk);
    else
        m_context = createStringParser(&sax, this);
}

void XMLTokenizer::doEnd()
{
#if ENABLE(XSLT)
    if (m_sawXSLTransform) {
        m_doc->setTransformSource(xmlDocPtrForString(m_doc->docLoader(), m_originalSourceForTransform, m_doc->url().string()));
        
        m_doc->setParsing(false); // Make the doc think it's done, so it will apply xsl sheets.
        m_doc->updateStyleSelector();
        m_doc->setParsing(true);
        m_parserStopped = true;
    }
#endif

    if (m_context) {
        // Tell libxml we're done.
        {
            XMLTokenizerScope scope(m_doc->docLoader());
            xmlParseChunk(m_context, 0, 0, 1);
        }

        if (m_context->myDoc)
            xmlFreeDoc(m_context->myDoc);
        xmlFreeParserCtxt(m_context);
        m_context = 0;
    }
}

#if ENABLE(XSLT)
void* xmlDocPtrForString(DocLoader* docLoader, const String& source, const String& url)
{
    if (source.isEmpty())
        return 0;

    // Parse in a single chunk into an xmlDocPtr
    // FIXME: Hook up error handlers so that a failure to parse the main document results in
    // good error messages.
    const UChar BOM = 0xFEFF;
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&BOM);

    XMLTokenizerScope scope(docLoader, errorFunc, 0);
    xmlDocPtr sourceDoc = xmlReadMemory(reinterpret_cast<const char*>(source.characters()),
                                        source.length() * sizeof(UChar),
                                        url.latin1().data(),
                                        BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", 
                                        XSLT_PARSE_OPTIONS);
    return sourceDoc;
}
#endif

int XMLTokenizer::lineNumber() const
{
    return m_context ? m_context->input->line : 1;
}

int XMLTokenizer::columnNumber() const
{
    return m_context ? m_context->input->col : 1;
}

void XMLTokenizer::stopParsing()
{
    Tokenizer::stopParsing();
    xmlStopParser(m_context);
}

void XMLTokenizer::resumeParsing()
{
    ASSERT(m_parserPaused);
    
    m_parserPaused = false;

    // First, execute any pending callbacks
    while (!m_pendingCallbacks->isEmpty()) {
        m_pendingCallbacks->callAndRemoveFirstCallback(this);
        
        // A callback paused the parser
        if (m_parserPaused)
            return;
    }

    // Then, write any pending data
    SegmentedString rest = m_pendingSrc;
    m_pendingSrc.clear();
    write(rest, false);

    // Finally, if finish() has been called and write() didn't result
    // in any further callbacks being queued, call end()
    if (m_finishCalled && m_pendingCallbacks->isEmpty())
        end();
}

bool parseXMLDocumentFragment(const String& chunk, DocumentFragment* fragment, Element* parent)
{
    if (!chunk.length())
        return true;

    XMLTokenizer tokenizer(fragment, parent);
    
    CString chunkAsUtf8 = chunk.utf8();
    tokenizer.initializeParserContext(chunkAsUtf8.data());

    xmlParseContent(tokenizer.m_context);

    tokenizer.endDocument();

    // Check if all the chunk has been processed.
    long bytesProcessed = xmlByteConsumed(tokenizer.m_context);
    if (bytesProcessed == -1 || ((unsigned long)bytesProcessed) != chunkAsUtf8.length())
        return false;

    // No error if the chunk is well formed or it is not but we have no error.
    return tokenizer.m_context->wellFormed || xmlCtxtGetLastError(tokenizer.m_context) == 0;
}

// --------------------------------

struct AttributeParseState {
    HashMap<String, String> attributes;
    bool gotAttributes;
};

static void attributesStartElementNsHandler(void* closure, const xmlChar* xmlLocalName, const xmlChar* /*xmlPrefix*/,
                                            const xmlChar* /*xmlURI*/, int /*nb_namespaces*/, const xmlChar** /*namespaces*/,
                                            int nb_attributes, int /*nb_defaulted*/, const xmlChar** libxmlAttributes)
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

HashMap<String, String> parseAttributes(const String& string, bool& attrsOK)
{
    AttributeParseState state;
    state.gotAttributes = false;

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
    attrsOK = state.gotAttributes;
    return state.attributes;
}

}
