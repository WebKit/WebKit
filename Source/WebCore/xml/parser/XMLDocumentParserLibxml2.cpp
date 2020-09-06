/*
 * Copyright (C) 2000 Peter Kelly <pmk@post.com>
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
#include "XMLDocumentParser.h"

#include "CDATASection.h"
#include "Comment.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLEntityParser.h"
#include "HTMLHtmlElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLTemplateElement.h"
#include "HTTPParsers.h"
#include "InlineClassicScript.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PendingScript.h"
#include "ProcessingInstruction.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "SVGElement.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "StyleScope.h"
#include "TransformSource.h"
#include "XMLNSNames.h"
#include "XMLDocumentParserScope.h"
#include <libxml/parserInternals.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/UTF8Conversion.h>

#if ENABLE(XSLT)
#include "XMLTreeViewer.h"
#include <libxslt/xslt.h>
#endif

namespace WebCore {

#if ENABLE(XSLT)

static inline bool shouldRenderInXMLTreeViewerMode(Document& document)
{
    if (document.sawElementsInKnownNamespaces())
        return false;

    if (document.transformSourceDocument())
        return false;

    auto* frame = document.frame();
    if (!frame)
        return false;

    if (!frame->settings().developerExtrasEnabled())
        return false;

    if (frame->tree().parent())
        return false; // This document is not in a top frame

    return true;
}

#endif

class PendingCallbacks {
    WTF_MAKE_FAST_ALLOCATED;
public:
    void appendStartElementNSCallback(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int numNamespaces, const xmlChar** namespaces, int numAttributes, int numDefaulted, const xmlChar** attributes)
    {
        auto callback = makeUnique<PendingStartElementNSCallback>();

        callback->xmlLocalName = xmlStrdup(xmlLocalName);
        callback->xmlPrefix = xmlStrdup(xmlPrefix);
        callback->xmlURI = xmlStrdup(xmlURI);
        callback->numNamespaces = numNamespaces;
        callback->namespaces = static_cast<xmlChar**>(xmlMalloc(sizeof(xmlChar*) * numNamespaces * 2));
        for (int i = 0; i < numNamespaces * 2 ; i++)
            callback->namespaces[i] = xmlStrdup(namespaces[i]);
        callback->numAttributes = numAttributes;
        callback->numDefaulted = numDefaulted;
        callback->attributes = static_cast<xmlChar**>(xmlMalloc(sizeof(xmlChar*) * numAttributes * 5));
        for (int i = 0; i < numAttributes; i++) {
            // Each attribute has 5 elements in the array:
            // name, prefix, uri, value and an end pointer.

            for (int j = 0; j < 3; j++)
                callback->attributes[i * 5 + j] = xmlStrdup(attributes[i * 5 + j]);

            int len = attributes[i * 5 + 4] - attributes[i * 5 + 3];

            callback->attributes[i * 5 + 3] = xmlStrndup(attributes[i * 5 + 3], len);
            callback->attributes[i * 5 + 4] = callback->attributes[i * 5 + 3] + len;
        }

        m_callbacks.append(WTFMove(callback));
    }

    void appendEndElementNSCallback()
    {
        m_callbacks.append(makeUnique<PendingEndElementNSCallback>());
    }

    void appendCharactersCallback(const xmlChar* s, int len)
    {
        auto callback = makeUnique<PendingCharactersCallback>();

        callback->s = xmlStrndup(s, len);
        callback->len = len;

        m_callbacks.append(WTFMove(callback));
    }

    void appendProcessingInstructionCallback(const xmlChar* target, const xmlChar* data)
    {
        auto callback = makeUnique<PendingProcessingInstructionCallback>();

        callback->target = xmlStrdup(target);
        callback->data = xmlStrdup(data);

        m_callbacks.append(WTFMove(callback));
    }

    void appendCDATABlockCallback(const xmlChar* s, int len)
    {
        auto callback = makeUnique<PendingCDATABlockCallback>();

        callback->s = xmlStrndup(s, len);
        callback->len = len;

        m_callbacks.append(WTFMove(callback));
    }

    void appendCommentCallback(const xmlChar* s)
    {
        auto callback = makeUnique<PendingCommentCallback>();

        callback->s = xmlStrdup(s);

        m_callbacks.append(WTFMove(callback));
    }

    void appendInternalSubsetCallback(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID)
    {
        auto callback = makeUnique<PendingInternalSubsetCallback>();

        callback->name = xmlStrdup(name);
        callback->externalID = xmlStrdup(externalID);
        callback->systemID = xmlStrdup(systemID);

        m_callbacks.append(WTFMove(callback));
    }

    void appendErrorCallback(XMLErrors::ErrorType type, const xmlChar* message, OrdinalNumber lineNumber, OrdinalNumber columnNumber)
    {
        auto callback = makeUnique<PendingErrorCallback>();

        callback->message = xmlStrdup(message);
        callback->type = type;
        callback->lineNumber = lineNumber;
        callback->columnNumber = columnNumber;

        m_callbacks.append(WTFMove(callback));
    }

    void callAndRemoveFirstCallback(XMLDocumentParser* parser)
    {
        std::unique_ptr<PendingCallback> callback = m_callbacks.takeFirst();
        callback->call(parser);
    }

    bool isEmpty() const { return m_callbacks.isEmpty(); }

private:
    struct PendingCallback {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        virtual ~PendingCallback() = default;
        virtual void call(XMLDocumentParser* parser) = 0;
    };

    struct PendingStartElementNSCallback : public PendingCallback {
        virtual ~PendingStartElementNSCallback()
        {
            xmlFree(xmlLocalName);
            xmlFree(xmlPrefix);
            xmlFree(xmlURI);
            for (int i = 0; i < numNamespaces * 2; i++)
                xmlFree(namespaces[i]);
            xmlFree(namespaces);
            for (int i = 0; i < numAttributes; i++) {
                for (int j = 0; j < 4; j++)
                    xmlFree(attributes[i * 5 + j]);
            }
            xmlFree(attributes);
        }

        void call(XMLDocumentParser* parser) override
        {
            parser->startElementNs(xmlLocalName, xmlPrefix, xmlURI, numNamespaces, const_cast<const xmlChar**>(namespaces), numAttributes, numDefaulted, const_cast<const xmlChar**>(attributes));
        }

        xmlChar* xmlLocalName;
        xmlChar* xmlPrefix;
        xmlChar* xmlURI;
        int numNamespaces;
        xmlChar** namespaces;
        int numAttributes;
        int numDefaulted;
        xmlChar** attributes;
    };

    struct PendingEndElementNSCallback : public PendingCallback {
        void call(XMLDocumentParser* parser) override
        {
            parser->endElementNs();
        }
    };

    struct PendingCharactersCallback : public PendingCallback {
        virtual ~PendingCharactersCallback()
        {
            xmlFree(s);
        }

        void call(XMLDocumentParser* parser) override
        {
            parser->characters(s, len);
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

        void call(XMLDocumentParser* parser) override
        {
            parser->processingInstruction(target, data);
        }

        xmlChar* target;
        xmlChar* data;
    };

    struct PendingCDATABlockCallback : public PendingCallback {
        virtual ~PendingCDATABlockCallback()
        {
            xmlFree(s);
        }

        void call(XMLDocumentParser* parser) override
        {
            parser->cdataBlock(s, len);
        }

        xmlChar* s;
        int len;
    };

    struct PendingCommentCallback : public PendingCallback {
        virtual ~PendingCommentCallback()
        {
            xmlFree(s);
        }

        void call(XMLDocumentParser* parser) override
        {
            parser->comment(s);
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

        void call(XMLDocumentParser* parser) override
        {
            parser->internalSubset(name, externalID, systemID);
        }

        xmlChar* name;
        xmlChar* externalID;
        xmlChar* systemID;
    };

    struct PendingErrorCallback: public PendingCallback {
        virtual ~PendingErrorCallback()
        {
            xmlFree(message);
        }

        void call(XMLDocumentParser* parser) override
        {
            parser->handleError(type, reinterpret_cast<char*>(message), TextPosition(lineNumber, columnNumber));
        }

        XMLErrors::ErrorType type;
        xmlChar* message;
        OrdinalNumber lineNumber;
        OrdinalNumber columnNumber;
    };

    Deque<std::unique_ptr<PendingCallback>> m_callbacks;
};
// --------------------------------

static int globalDescriptor = 0;
static Thread* libxmlLoaderThread { nullptr };

static int matchFunc(const char*)
{
    // Only match loads initiated due to uses of libxml2 from within XMLDocumentParser to avoid
    // interfering with client applications that also use libxml2.  http://bugs.webkit.org/show_bug.cgi?id=17353
    return XMLDocumentParserScope::currentCachedResourceLoader && libxmlLoaderThread == &Thread::current();
}

class OffsetBuffer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    OffsetBuffer(Vector<char> buffer)
        : m_buffer(WTFMove(buffer))
        , m_currentOffset(0)
    {
    }

    int readOutBytes(char* outputBuffer, unsigned askedToRead)
    {
        unsigned bytesLeft = m_buffer.size() - m_currentOffset;
        unsigned lenToCopy = std::min(askedToRead, bytesLeft);
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

static bool externalEntityMimeTypeAllowed(const ResourceResponse& response)
{
    String contentType = response.httpHeaderField(HTTPHeaderName::ContentType);
    String mimeType = extractMIMETypeFromMediaType(contentType);
    if (mimeType.isEmpty()) {
        // Same logic as XMLHttpRequest::responseMIMEType(). Keep them in sync.
        if (response.isInHTTPFamily())
            mimeType = contentType;
        else
            mimeType = response.mimeType();
    }
    return MIMETypeRegistry::isXMLMIMEType(mimeType) || MIMETypeRegistry::isXMLEntityMIMEType(mimeType);
}

static inline void setAttributes(Element* element, Vector<Attribute>& attributeVector, ParserContentPolicy parserContentPolicy)
{
    if (!scriptingContentIsAllowed(parserContentPolicy))
        element->stripScriptingAttributes(attributeVector);
    element->parserSetAttributes(attributeVector);
}

static void switchToUTF16(xmlParserCtxtPtr ctxt)
{
    // Hack around libxml2's lack of encoding overide support by manually
    // resetting the encoding to UTF-16 before every chunk.  Otherwise libxml
    // will detect <?xml version="1.0" encoding="<encoding name>"?> blocks
    // and switch encodings, causing the parse to fail.

    // FIXME: Can we just use XML_PARSE_IGNORE_ENC now?

    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&byteOrderMark);
    xmlSwitchEncoding(ctxt, BOMHighByte == 0xFF ? XML_CHAR_ENCODING_UTF16LE : XML_CHAR_ENCODING_UTF16BE);
}

static bool shouldAllowExternalLoad(const URL& url)
{
    String urlString = url.string();

    // On non-Windows platforms libxml asks for this URL, the "XML_XML_DEFAULT_CATALOG", on initialization.
    if (urlString == "file:///etc/xml/catalog")
        return false;

    // On Windows, libxml computes a URL relative to where its DLL resides.
    if (startsWithLettersIgnoringASCIICase(urlString, "file:///") && urlString.endsWithIgnoringASCIICase("/etc/catalog"))
        return false;

    // The most common DTD. There isn't much point in hammering www.w3c.org by requesting this for every XHTML document.
    if (startsWithLettersIgnoringASCIICase(urlString, "http://www.w3.org/tr/xhtml"))
        return false;

    // Similarly, there isn't much point in requesting the SVG DTD.
    if (startsWithLettersIgnoringASCIICase(urlString, "http://www.w3.org/graphics/svg"))
        return false;

    // The libxml doesn't give us a lot of context for deciding whether to
    // allow this request.  In the worst case, this load could be for an
    // external entity and the resulting document could simply read the
    // retrieved content.  If we had more context, we could potentially allow
    // the parser to load a DTD.  As things stand, we take the conservative
    // route and allow same-origin requests only.
    if (!XMLDocumentParserScope::currentCachedResourceLoader->document()->securityOrigin().canRequest(url)) {
        XMLDocumentParserScope::currentCachedResourceLoader->printAccessDeniedMessage(url);
        return false;
    }

    return true;
}

static void* openFunc(const char* uri)
{
    ASSERT(XMLDocumentParserScope::currentCachedResourceLoader);
    ASSERT(libxmlLoaderThread == &Thread::current());

    CachedResourceLoader& cachedResourceLoader = *XMLDocumentParserScope::currentCachedResourceLoader;
    Document* document = cachedResourceLoader.document();
    // Same logic as HTMLBaseElement::href(). Keep them in sync.
    auto* encoding = (document && document->decoder()) ? document->decoder()->encodingForURLParsing() : nullptr;
    URL url(document ? document->url() : URL(), stripLeadingAndTrailingHTMLSpaces(uri), encoding);

    if (!shouldAllowExternalLoad(url))
        return &globalDescriptor;

    ResourceResponse response;
    RefPtr<SharedBuffer> data;

    {
        ResourceError error;
        XMLDocumentParserScope scope(nullptr);
        // FIXME: We should restore the original global error handler as well.

        if (cachedResourceLoader.frame()) {
            FetchOptions options;
            options.mode = FetchOptions::Mode::SameOrigin;
            options.credentials = FetchOptions::Credentials::Include;
            cachedResourceLoader.frame()->loader().loadResourceSynchronously(url, ClientCredentialPolicy::MayAskClientForCredentials, options, { }, error, response, data);

            if (response.url().isEmpty()) {
                if (Page* page = document ? document->page() : nullptr)
                    page->console().addMessage(MessageSource::Security, MessageLevel::Error, makeString("Did not parse external entity resource at '", url.stringCenterEllipsizedToLength(), "' because cross-origin loads are not allowed."));
                return &globalDescriptor;
            }
            if (!externalEntityMimeTypeAllowed(response)) {
                if (Page* page = document ? document->page() : nullptr)
                    page->console().addMessage(MessageSource::Security, MessageLevel::Error, makeString("Did not parse external entity resource at '", url.stringCenterEllipsizedToLength(), "' because only XML MIME types are allowed."));
                return &globalDescriptor;
            }
        }
    }

    Vector<char> buffer;
    if (data)
        buffer.append(data->data(), data->size());
    return new OffsetBuffer(WTFMove(buffer));
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

static void initializeXMLParser()
{
    static std::once_flag flag;
    std::call_once(flag, [&] {
        xmlInitParser();
        xmlRegisterInputCallbacks(matchFunc, openFunc, readFunc, closeFunc);
        xmlRegisterOutputCallbacks(matchFunc, openFunc, writeFunc, closeFunc);
        libxmlLoaderThread = &Thread::current();
    });
}

Ref<XMLParserContext> XMLParserContext::createStringParser(xmlSAXHandlerPtr handlers, void* userData)
{
    initializeXMLParser();

    xmlParserCtxtPtr parser = xmlCreatePushParserCtxt(handlers, 0, 0, 0, 0);
    parser->_private = userData;

    // Substitute entities.
    xmlCtxtUseOptions(parser, XML_PARSE_NOENT | XML_PARSE_HUGE);

    switchToUTF16(parser);

    return adoptRef(*new XMLParserContext(parser));
}


// Chunk should be encoded in UTF-8
RefPtr<XMLParserContext> XMLParserContext::createMemoryParser(xmlSAXHandlerPtr handlers, void* userData, const CString& chunk)
{
    initializeXMLParser();

    // appendFragmentSource() checks that the length doesn't overflow an int.
    xmlParserCtxtPtr parser = xmlCreateMemoryParserCtxt(chunk.data(), chunk.length());

    if (!parser)
        return 0;

    memcpy(parser->sax, handlers, sizeof(xmlSAXHandler));

    // Substitute entities.
    // FIXME: Why is XML_PARSE_NODICT needed? This is different from what createStringParser does.
    xmlCtxtUseOptions(parser, XML_PARSE_NODICT | XML_PARSE_NOENT | XML_PARSE_HUGE);

    // Internal initialization
    parser->sax2 = 1;
    parser->instate = XML_PARSER_CONTENT; // We are parsing a CONTENT
    parser->depth = 0;
    parser->str_xml = xmlDictLookup(parser->dict, reinterpret_cast<xmlChar*>(const_cast<char*>("xml")), 3);
    parser->str_xmlns = xmlDictLookup(parser->dict, reinterpret_cast<xmlChar*>(const_cast<char*>("xmlns")), 5);
    parser->str_xml_ns = xmlDictLookup(parser->dict, XML_XML_NAMESPACE, 36);
    parser->_private = userData;

    return adoptRef(*new XMLParserContext(parser));
}

// --------------------------------

bool XMLDocumentParser::supportsXMLVersion(const String& version)
{
    return version == "1.0";
}

XMLDocumentParser::XMLDocumentParser(Document& document, FrameView* frameView)
    : ScriptableDocumentParser(document)
    , m_view(frameView)
    , m_pendingCallbacks(makeUnique<PendingCallbacks>())
    , m_currentNode(&document)
    , m_scriptStartPosition(TextPosition::belowRangePosition())
{
}

XMLDocumentParser::XMLDocumentParser(DocumentFragment& fragment, HashMap<AtomString, AtomString>&& prefixToNamespaceMap, const AtomString& defaultNamespaceURI, ParserContentPolicy parserContentPolicy)
    : ScriptableDocumentParser(fragment.document(), parserContentPolicy)
    , m_pendingCallbacks(makeUnique<PendingCallbacks>())
    , m_currentNode(&fragment)
    , m_scriptStartPosition(TextPosition::belowRangePosition())
    , m_parsingFragment(true)
    , m_prefixToNamespaceMap(WTFMove(prefixToNamespaceMap))
    , m_defaultNamespaceURI(defaultNamespaceURI)
{
    fragment.ref();
}

XMLParserContext::~XMLParserContext()
{
    if (m_context->myDoc)
        xmlFreeDoc(m_context->myDoc);
    xmlFreeParserCtxt(m_context);
}

XMLDocumentParser::~XMLDocumentParser()
{
    // The XMLDocumentParser will always be detached before being destroyed.
    ASSERT(m_currentNodeStack.isEmpty());
    ASSERT(!m_currentNode);

    // FIXME: m_pendingScript handling should be moved into XMLDocumentParser.cpp!
    if (m_pendingScript)
        m_pendingScript->clearClient();
}

void XMLDocumentParser::doWrite(const String& parseString)
{
    ASSERT(!isDetached());
    if (!m_context)
        initializeParserContext();

    // Protect the libxml context from deletion during a callback
    RefPtr<XMLParserContext> context = m_context;

    // libXML throws an error if you try to switch the encoding for an empty string.
    if (parseString.length()) {
        // JavaScript may cause the parser to detach during xmlParseChunk
        // keep this alive until this function is done.
        Ref<XMLDocumentParser> protectedThis(*this);

        XMLDocumentParserScope scope(&document()->cachedResourceLoader());

        // FIXME: Can we parse 8-bit strings directly as Latin-1 instead of upconverting to UTF-16?
        switchToUTF16(context->context());
        xmlParseChunk(context->context(), reinterpret_cast<const char*>(StringView(parseString).upconvertedCharacters().get()), sizeof(UChar) * parseString.length(), 0);

        // JavaScript (which may be run under the xmlParseChunk callstack) may
        // cause the parser to be stopped or detached.
        if (isStopped())
            return;
    }

    // FIXME: Why is this here?  And why is it after we process the passed source?
    if (document()->decoder() && document()->decoder()->sawError()) {
        // If the decoder saw an error, report it as fatal (stops parsing)
        TextPosition position(OrdinalNumber::fromOneBasedInt(context->context()->input->line), OrdinalNumber::fromOneBasedInt(context->context()->input->col));
        handleError(XMLErrors::fatal, "Encoding error", position);
    }
}

static inline String toString(const xmlChar* string, size_t size)
{
    return String::fromUTF8(reinterpret_cast<const char*>(string), size);
}

static inline String toString(const xmlChar* string)
{
    return String::fromUTF8(reinterpret_cast<const char*>(string));
}

static inline AtomString toAtomString(const xmlChar* string, size_t size)
{
    return AtomString::fromUTF8(reinterpret_cast<const char*>(string), size);
}

static inline AtomString toAtomString(const xmlChar* string)
{
    return AtomString::fromUTF8(reinterpret_cast<const char*>(string));
}

struct _xmlSAX2Namespace {
    const xmlChar* prefix;
    const xmlChar* uri;
};
typedef struct _xmlSAX2Namespace xmlSAX2Namespace;

static inline bool handleNamespaceAttributes(Vector<Attribute>& prefixedAttributes, const xmlChar** libxmlNamespaces, int numNamespaces)
{
    xmlSAX2Namespace* namespaces = reinterpret_cast<xmlSAX2Namespace*>(libxmlNamespaces);
    for (int i = 0; i < numNamespaces; i++) {
        AtomString namespaceQName = xmlnsAtom();
        AtomString namespaceURI = toAtomString(namespaces[i].uri);
        if (namespaces[i].prefix)
            namespaceQName = "xmlns:" + toString(namespaces[i].prefix);

        auto result = Element::parseAttributeName(XMLNSNames::xmlnsNamespaceURI, namespaceQName);
        if (result.hasException())
            return false;

        prefixedAttributes.append(Attribute(result.releaseReturnValue(), namespaceURI));
    }
    return true;
}

struct _xmlSAX2Attributes {
    const xmlChar* localname;
    const xmlChar* prefix;
    const xmlChar* uri;
    const xmlChar* value;
    const xmlChar* end;
};
typedef struct _xmlSAX2Attributes xmlSAX2Attributes;

static inline bool handleElementAttributes(Vector<Attribute>& prefixedAttributes, const xmlChar** libxmlAttributes, int numAttributes)
{
    xmlSAX2Attributes* attributes = reinterpret_cast<xmlSAX2Attributes*>(libxmlAttributes);
    for (int i = 0; i < numAttributes; i++) {
        int valueLength = static_cast<int>(attributes[i].end - attributes[i].value);
        AtomString attrValue = toAtomString(attributes[i].value, valueLength);
        String attrPrefix = toString(attributes[i].prefix);
        AtomString attrURI = attrPrefix.isEmpty() ? nullAtom() : toAtomString(attributes[i].uri);
        AtomString attrQName = attrPrefix.isEmpty() ? toAtomString(attributes[i].localname) : attrPrefix + ":" + toString(attributes[i].localname);

        auto result = Element::parseAttributeName(attrURI, attrQName);
        if (result.hasException())
            return false;

        prefixedAttributes.append(Attribute(result.releaseReturnValue(), attrValue));
    }
    return true;
}

// This is a hack around https://bugzilla.gnome.org/show_bug.cgi?id=502960
// Otherwise libxml doesn't include namespace for parsed entities, breaking entity
// expansion for all entities containing elements.
static inline bool hackAroundLibXMLEntityParsingBug()
{
#if LIBXML_VERSION >= 20704
    // This bug has been fixed in libxml 2.7.4.
    return false;
#else
    return true;
#endif
}

void XMLDocumentParser::startElementNs(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int numNamespaces, const xmlChar** libxmlNamespaces, int numAttributes, int numDefaulted, const xmlChar** libxmlAttributes)
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendStartElementNSCallback(xmlLocalName, xmlPrefix, xmlURI, numNamespaces, libxmlNamespaces, numAttributes, numDefaulted, libxmlAttributes);
        return;
    }

    if (!updateLeafTextNode())
        return;

    AtomString localName = toAtomString(xmlLocalName);
    AtomString uri = toAtomString(xmlURI);
    AtomString prefix = toAtomString(xmlPrefix);

    if (m_parsingFragment && uri.isNull()) {
        if (!prefix.isNull())
            uri = m_prefixToNamespaceMap.get(prefix);
        else if (is<SVGElement>(m_currentNode) || localName == SVGNames::svgTag->localName())
            uri = SVGNames::svgNamespaceURI;
        else
            uri = m_defaultNamespaceURI;
    }

    // If libxml entity parsing is broken, transfer the currentNodes' namespaceURI to the new node,
    // if we're currently expanding elements which originate from an entity declaration.
    if (hackAroundLibXMLEntityParsingBug() && depthTriggeringEntityExpansion() != -1 && context()->depth > depthTriggeringEntityExpansion() && uri.isNull() && prefix.isNull())
        uri = m_currentNode->namespaceURI();

    bool isFirstElement = !m_sawFirstElement;
    m_sawFirstElement = true;

    QualifiedName qName(prefix, localName, uri);
    auto newElement = m_currentNode->document().createElement(qName, true);

    Vector<Attribute> prefixedAttributes;
    if (!handleNamespaceAttributes(prefixedAttributes, libxmlNamespaces, numNamespaces)) {
        setAttributes(newElement.ptr(), prefixedAttributes, parserContentPolicy());
        stopParsing();
        return;
    }

    bool success = handleElementAttributes(prefixedAttributes, libxmlAttributes, numAttributes);
    setAttributes(newElement.ptr(), prefixedAttributes, parserContentPolicy());
    if (!success) {
        stopParsing();
        return;
    }

    newElement->beginParsingChildren();

    if (isScriptElement(newElement.get()))
        m_scriptStartPosition = textPosition();

    m_currentNode->parserAppendChild(newElement);
    if (!m_currentNode) // Synchronous DOM events may have removed the current node.
        return;

    if (is<HTMLTemplateElement>(newElement))
        pushCurrentNode(&downcast<HTMLTemplateElement>(newElement.get()).content());
    else
        pushCurrentNode(newElement.ptr());

    if (is<HTMLHtmlElement>(newElement))
        downcast<HTMLHtmlElement>(newElement.get()).insertedByParser();

    if (!m_parsingFragment && isFirstElement && document()->frame())
        document()->frame()->injectUserScripts(UserScriptInjectionTime::DocumentStart);
}

void XMLDocumentParser::endElementNs()
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendEndElementNSCallback();
        return;
    }

    // JavaScript can detach the parser.  Make sure this is not released
    // before the end of this method.
    Ref<XMLDocumentParser> protectedThis(*this);

    if (!updateLeafTextNode())
        return;

    RefPtr<ContainerNode> node = m_currentNode;
    node->finishParsingChildren();

    // Once we reach the depth again where entity expansion started, stop executing the work-around.
    if (hackAroundLibXMLEntityParsingBug() && context()->depth <= depthTriggeringEntityExpansion())
        setDepthTriggeringEntityExpansion(-1);

    if (!scriptingContentIsAllowed(parserContentPolicy()) && is<Element>(*node) && isScriptElement(downcast<Element>(*node))) {
        popCurrentNode();
        node->remove();
        return;
    }

    if (!node->isElementNode() || !m_view) {
        popCurrentNode();
        return;
    }

    auto& element = downcast<Element>(*node);

    // The element's parent may have already been removed from document.
    // Parsing continues in this case, but scripts aren't executed.
    if (!element.isConnected()) {
        popCurrentNode();
        return;
    }

    if (!isScriptElement(element)) {
        popCurrentNode();
        return;
    }

    // Don't load external scripts for standalone documents (for now).
    ASSERT(!m_pendingScript);
    m_requestingScript = true;

    auto& scriptElement = downcastScriptElement(element);
    if (scriptElement.prepareScript(m_scriptStartPosition, ScriptElement::AllowLegacyTypeInTypeAttribute)) {
        // FIXME: Script execution should be shared between
        // the libxml2 and Qt XMLDocumentParser implementations.

        if (scriptElement.readyToBeParserExecuted())
            scriptElement.executeClassicScript(ScriptSourceCode(scriptElement.scriptContent(), URL(document()->url()), m_scriptStartPosition, JSC::SourceProviderSourceType::Program, InlineClassicScript::create(scriptElement)));
        else if (scriptElement.willBeParserExecuted() && scriptElement.loadableScript()) {
            m_pendingScript = PendingScript::create(scriptElement, *scriptElement.loadableScript());
            m_pendingScript->setClient(*this);

            // m_pendingScript will be nullptr if script was already loaded and setClient() executed it.
            if (m_pendingScript)
                pauseParsing();
        }

        // JavaScript may have detached the parser
        if (isDetached())
            return;
    }
    m_requestingScript = false;
    popCurrentNode();
}

void XMLDocumentParser::characters(const xmlChar* characters, int length)
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCharactersCallback(characters, length);
        return;
    }

    if (!m_leafTextNode)
        createLeafTextNode();
    m_bufferedText.append(characters, length);
}

void XMLDocumentParser::error(XMLErrors::ErrorType type, const char* message, va_list args)
{
    if (isStopped())
        return;

    va_list preflightArgs;
    va_copy(preflightArgs, args);
    size_t stringLength = vsnprintf(nullptr, 0, message, preflightArgs);
    va_end(preflightArgs);

    Vector<char, 1024> buffer(stringLength + 1);
    vsnprintf(buffer.data(), stringLength + 1, message, args);

    TextPosition position = textPosition();
    if (m_parserPaused)
        m_pendingCallbacks->appendErrorCallback(type, reinterpret_cast<const xmlChar*>(buffer.data()), position.m_line, position.m_column);
    else
        handleError(type, buffer.data(), textPosition());
}

void XMLDocumentParser::processingInstruction(const xmlChar* target, const xmlChar* data)
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendProcessingInstructionCallback(target, data);
        return;
    }

    if (!updateLeafTextNode())
        return;

    auto result = m_currentNode->document().createProcessingInstruction(toString(target), toString(data));
    if (result.hasException())
        return;
    auto pi = result.releaseReturnValue();

    pi->setCreatedByParser(true);

    m_currentNode->parserAppendChild(pi);

    pi->finishParsingChildren();

    if (pi->isCSS())
        m_sawCSS = true;

#if ENABLE(XSLT)
    m_sawXSLTransform = !m_sawFirstElement && pi->isXSL();
    if (m_sawXSLTransform && !document()->transformSourceDocument())
        stopParsing();
#endif
}

void XMLDocumentParser::cdataBlock(const xmlChar* s, int len)
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCDATABlockCallback(s, len);
        return;
    }

    if (!updateLeafTextNode())
        return;

    m_currentNode->parserAppendChild(CDATASection::create(m_currentNode->document(), toString(s, len)));
}

void XMLDocumentParser::comment(const xmlChar* s)
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCommentCallback(s);
        return;
    }

    if (!updateLeafTextNode())
        return;

    m_currentNode->parserAppendChild(Comment::create(m_currentNode->document(), toString(s)));
}

enum StandaloneInfo {
    StandaloneUnspecified = -2,
    NoXMlDeclaration,
    StandaloneNo,
    StandaloneYes
};

void XMLDocumentParser::startDocument(const xmlChar* version, const xmlChar* encoding, int standalone)
{
    StandaloneInfo standaloneInfo = (StandaloneInfo)standalone;
    if (standaloneInfo == NoXMlDeclaration) {
        document()->setHasXMLDeclaration(false);
        return;
    }

    if (version)
        document()->setXMLVersion(toString(version));
    if (standalone != StandaloneUnspecified)
        document()->setXMLStandalone(standaloneInfo == StandaloneYes);
    if (encoding)
        document()->setXMLEncoding(toString(encoding));
    document()->setHasXMLDeclaration(true);
}

void XMLDocumentParser::endDocument()
{
    updateLeafTextNode();
}

void XMLDocumentParser::internalSubset(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID)
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendInternalSubsetCallback(name, externalID, systemID);
        return;
    }

    if (document())
        document()->parserAppendChild(DocumentType::create(*document(), toString(name), toString(externalID), toString(systemID)));
}

static inline XMLDocumentParser* getParser(void* closure)
{
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);
    return static_cast<XMLDocumentParser*>(ctxt->_private);
}

// This is a hack around http://bugzilla.gnome.org/show_bug.cgi?id=159219
// Otherwise libxml seems to call all the SAX callbacks twice for any replaced entity.
static inline bool hackAroundLibXMLEntityBug(void* closure)
{
#if LIBXML_VERSION >= 20627
    // This bug has been fixed in libxml 2.6.27.
    UNUSED_PARAM(closure);
    return false;
#else
    return static_cast<xmlParserCtxtPtr>(closure)->node;
#endif
}

static void startElementNsHandler(void* closure, const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri, int numNamespaces, const xmlChar** namespaces, int numAttributes, int numDefaulted, const xmlChar** libxmlAttributes)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;

    getParser(closure)->startElementNs(localname, prefix, uri, numNamespaces, namespaces, numAttributes, numDefaulted, libxmlAttributes);
}

static void endElementNsHandler(void* closure, const xmlChar*, const xmlChar*, const xmlChar*)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;

    getParser(closure)->endElementNs();
}

static void charactersHandler(void* closure, const xmlChar* s, int len)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;

    getParser(closure)->characters(s, len);
}

static void processingInstructionHandler(void* closure, const xmlChar* target, const xmlChar* data)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;

    getParser(closure)->processingInstruction(target, data);
}

static void cdataBlockHandler(void* closure, const xmlChar* s, int len)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;

    getParser(closure)->cdataBlock(s, len);
}

static void commentHandler(void* closure, const xmlChar* comment)
{
    if (hackAroundLibXMLEntityBug(closure))
        return;

    getParser(closure)->comment(comment);
}

WTF_ATTRIBUTE_PRINTF(2, 3)
static void warningHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getParser(closure)->error(XMLErrors::warning, message, args);
    va_end(args);
}

WTF_ATTRIBUTE_PRINTF(2, 3)
static void fatalErrorHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getParser(closure)->error(XMLErrors::fatal, message, args);
    va_end(args);
}

WTF_ATTRIBUTE_PRINTF(2, 3)
static void normalErrorHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getParser(closure)->error(XMLErrors::nonFatal, message, args);
    va_end(args);
}

// Using a static entity and marking it XML_INTERNAL_PREDEFINED_ENTITY is
// a hack to avoid malloc/free. Using a global variable like this could cause trouble
// if libxml implementation details were to change
static xmlChar sharedXHTMLEntityResult[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

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

static size_t convertUTF16EntityToUTF8(const UChar* utf16Entity, size_t numberOfCodeUnits, char* target, size_t targetSize)
{
    const char* originalTarget = target;
    auto conversionResult = WTF::Unicode::convertUTF16ToUTF8(&utf16Entity, utf16Entity + numberOfCodeUnits, &target, target + targetSize);
    if (conversionResult != WTF::Unicode::ConversionOK)
        return 0;

    // Even though we must pass the length, libxml expects the entity string to be null terminated.
    ASSERT(target >= originalTarget + 1);
    *target = '\0';
    return target - originalTarget;
}

static xmlEntityPtr getXHTMLEntity(const xmlChar* name)
{
    UChar utf16DecodedEntity[4];
    size_t numberOfCodeUnits = decodeNamedEntityToUCharArray(reinterpret_cast<const char*>(name), utf16DecodedEntity);
    if (!numberOfCodeUnits)
        return 0;

    ASSERT(numberOfCodeUnits <= 4);
    size_t entityLengthInUTF8 = convertUTF16EntityToUTF8(utf16DecodedEntity, numberOfCodeUnits,
        reinterpret_cast<char*>(sharedXHTMLEntityResult), WTF_ARRAY_LENGTH(sharedXHTMLEntityResult));
    if (!entityLengthInUTF8)
        return 0;

    xmlEntityPtr entity = sharedXHTMLEntity();
    entity->length = entityLengthInUTF8;
    entity->name = name;
    return entity;
}

static void entityDeclarationHandler(void* closure, const xmlChar* name, int type, const xmlChar* publicId, const xmlChar* systemId, xmlChar* content)
{
    // Prevent the next call to getEntityHandler() to record the entity expansion depth.
    // We're parsing the entity declaration, so there's no need to record anything.
    // We only need to record the depth, if we're actually expanding the entity, when it's referenced.
    if (hackAroundLibXMLEntityParsingBug())
        getParser(closure)->setIsParsingEntityDeclaration(true);
    xmlSAX2EntityDecl(closure, name, type, publicId, systemId, content);
}

static xmlEntityPtr getEntityHandler(void* closure, const xmlChar* name)
{
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);

    XMLDocumentParser* parser = getParser(closure);
    if (hackAroundLibXMLEntityParsingBug()) {
        if (parser->isParsingEntityDeclaration()) {
            // We're parsing the entity declarations (not an entity reference), no need to do anything special.
            parser->setIsParsingEntityDeclaration(false);
            ASSERT(parser->depthTriggeringEntityExpansion() == -1);
        } else {
            // The entity will be used and eventually expanded. Record the current parser depth
            // so the next call to startElementNs() knows that the new element originates from
            // an entity declaration.
            parser->setDepthTriggeringEntityExpansion(ctxt->depth);
        }
    }

    xmlEntityPtr ent = xmlGetPredefinedEntity(name);
    if (ent) {
        ent->etype = XML_INTERNAL_PREDEFINED_ENTITY;
        return ent;
    }

    ent = xmlGetDocEntity(ctxt->myDoc, name);
    if (!ent && parser->isXHTMLDocument()) {
        ent = getXHTMLEntity(name);
        if (ent)
            ent->etype = XML_INTERNAL_GENERAL_ENTITY;
    }

    return ent;
}

static void startDocumentHandler(void* closure)
{
    xmlParserCtxt* ctxt = static_cast<xmlParserCtxt*>(closure);
    switchToUTF16(ctxt);
    getParser(closure)->startDocument(ctxt->version, ctxt->encoding, ctxt->standalone);
    xmlSAX2StartDocument(closure);
}

static void endDocumentHandler(void* closure)
{
    getParser(closure)->endDocument();
    xmlSAX2EndDocument(closure);
}

static void internalSubsetHandler(void* closure, const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID)
{
    getParser(closure)->internalSubset(name, externalID, systemID);
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
        || (extId == "-//W3C//DTD MathML 2.0//EN")
        || (extId == "-//WAPFORUM//DTD XHTML Mobile 1.0//EN")
        || (extId == "-//WAPFORUM//DTD XHTML Mobile 1.1//EN")
        || (extId == "-//WAPFORUM//DTD XHTML Mobile 1.2//EN"))
        getParser(closure)->setIsXHTMLDocument(true); // controls if we replace entities or not.
}

static void ignorableWhitespaceHandler(void*, const xmlChar*, int)
{
    // nothing to do, but we need this to work around a crasher
    // http://bugzilla.gnome.org/show_bug.cgi?id=172255
    // http://bugs.webkit.org/show_bug.cgi?id=5792
}

void XMLDocumentParser::initializeParserContext(const CString& chunk)
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
    sax.entityDecl = entityDeclarationHandler;
    sax.initialized = XML_SAX2_MAGIC;
    DocumentParser::startParsing();
    m_sawError = false;
    m_sawCSS = false;
    m_sawXSLTransform = false;
    m_sawFirstElement = false;

    XMLDocumentParserScope scope(&document()->cachedResourceLoader());
    if (m_parsingFragment)
        m_context = XMLParserContext::createMemoryParser(&sax, this, chunk);
    else {
        ASSERT(!chunk.data());
        m_context = XMLParserContext::createStringParser(&sax, this);
    }
}

void XMLDocumentParser::doEnd()
{
    if (!isStopped()) {
        if (m_context) {
            // Tell libxml we're done.
            {
                XMLDocumentParserScope scope(&document()->cachedResourceLoader());
                xmlParseChunk(context(), 0, 0, 1);
            }

            m_context = nullptr;
        }
    }

#if ENABLE(XSLT)
    bool xmlViewerMode = !m_sawError && !m_sawCSS && !m_sawXSLTransform && shouldRenderInXMLTreeViewerMode(*document());
    if (xmlViewerMode) {
        XMLTreeViewer xmlTreeViewer(*document());
        xmlTreeViewer.transformDocumentToTreeView();
    } else if (m_sawXSLTransform) {
        xmlDocPtr doc = xmlDocPtrForString(document()->cachedResourceLoader(), m_originalSourceForTransform.toString(), document()->url().string());
        document()->setTransformSource(makeUnique<TransformSource>(doc));

        document()->setParsing(false); // Make the document think it's done, so it will apply XSL stylesheets.
        document()->applyPendingXSLTransformsNowIfScheduled();

        // styleResolverChanged() call can detach the parser and null out its document.
        // In that case, we just bail out.
        if (isDetached())
            return;

        document()->setParsing(true);
        DocumentParser::stopParsing();
    }
#endif
}

#if ENABLE(XSLT)
static inline const char* nativeEndianUTF16Encoding()
{
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&byteOrderMark);
    return BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE";
}

xmlDocPtr xmlDocPtrForString(CachedResourceLoader& cachedResourceLoader, const String& source, const String& url)
{
    if (source.isEmpty())
        return nullptr;

    // Parse in a single chunk into an xmlDocPtr
    // FIXME: Hook up error handlers so that a failure to parse the main document results in
    // good error messages.

    const bool is8Bit = source.is8Bit();
    const char* characters = is8Bit ? reinterpret_cast<const char*>(source.characters8()) : reinterpret_cast<const char*>(source.characters16());
    size_t sizeInBytes = source.length() * (is8Bit ? sizeof(LChar) : sizeof(UChar));
    const char* encoding = is8Bit ? "iso-8859-1" : nativeEndianUTF16Encoding();

    XMLDocumentParserScope scope(&cachedResourceLoader, errorFunc);
    return xmlReadMemory(characters, sizeInBytes, url.latin1().data(), encoding, XSLT_PARSE_OPTIONS);
}
#endif

TextPosition XMLDocumentParser::textPosition() const
{
    xmlParserCtxtPtr context = this->context();
    if (!context)
        return TextPosition();
    return TextPosition(OrdinalNumber::fromOneBasedInt(context->input->line),
                        OrdinalNumber::fromOneBasedInt(context->input->col));
}

bool XMLDocumentParser::shouldAssociateConsoleMessagesWithTextPosition() const
{
    return !m_parserPaused && !m_requestingScript;
}

void XMLDocumentParser::stopParsing()
{
    if (m_sawError)
        insertErrorMessageBlock();

    DocumentParser::stopParsing();
    if (context())
        xmlStopParser(context());
}

void XMLDocumentParser::resumeParsing()
{
    ASSERT(!isDetached());
    ASSERT(m_parserPaused);

    m_parserPaused = false;

    // First, execute any pending callbacks
    while (!m_pendingCallbacks->isEmpty()) {
        m_pendingCallbacks->callAndRemoveFirstCallback(this);

        // A callback paused the parser
        if (m_parserPaused)
            return;
    }

    // There is normally only one string left, so toString() shouldn't copy.
    // In any case, the XML parser runs on the main thread and it's OK if
    // the passed string has more than one reference.
    auto rest = m_pendingSrc.toString();
    m_pendingSrc.clear();
    append(rest.impl());

    // Finally, if finish() has been called and write() didn't result
    // in any further callbacks being queued, call end()
    if (m_finishCalled && m_pendingCallbacks->isEmpty())
        end();
}

bool XMLDocumentParser::appendFragmentSource(const String& chunk)
{
    ASSERT(!m_context);
    ASSERT(m_parsingFragment);

    CString chunkAsUtf8 = chunk.utf8();
    
    // libxml2 takes an int for a length, and therefore can't handle XML chunks larger than 2 GiB.
    if (chunkAsUtf8.length() > INT_MAX)
        return false;

    initializeParserContext(chunkAsUtf8);
    xmlParseContent(context());
    endDocument(); // Close any open text nodes.

    // FIXME: If this code is actually needed, it should probably move to finish()
    // XMLDocumentParserQt has a similar check (m_stream.error() == QXmlStreamReader::PrematureEndOfDocumentError) in doEnd().
    // Check if all the chunk has been processed.
    long bytesProcessed = xmlByteConsumed(context());
    if (bytesProcessed == -1 || ((unsigned long)bytesProcessed) != chunkAsUtf8.length()) {
        // FIXME: I don't believe we can hit this case without also having seen an error or a null byte.
        // If we hit this ASSERT, we've found a test case which demonstrates the need for this code.
        ASSERT(m_sawError || (bytesProcessed >= 0 && !chunkAsUtf8.data()[bytesProcessed]));
        return false;
    }

    // No error if the chunk is well formed or it is not but we have no error.
    return context()->wellFormed || !xmlCtxtGetLastError(context());
}

// --------------------------------

using AttributeParseState = Optional<HashMap<String, String>>;

static void attributesStartElementNsHandler(void* closure, const xmlChar* xmlLocalName, const xmlChar* /*xmlPrefix*/, const xmlChar* /*xmlURI*/, int /*numNamespaces*/, const xmlChar** /*namespaces*/, int numAttributes, int /*numDefaulted*/, const xmlChar** libxmlAttributes)
{
    if (strcmp(reinterpret_cast<const char*>(xmlLocalName), "attrs") != 0)
        return;

    auto& state = *static_cast<AttributeParseState*>(static_cast<xmlParserCtxtPtr>(closure)->_private);

    state = HashMap<String, String> { };

    xmlSAX2Attributes* attributes = reinterpret_cast<xmlSAX2Attributes*>(libxmlAttributes);
    for (int i = 0; i < numAttributes; i++) {
        String attrLocalName = toString(attributes[i].localname);
        int valueLength = (int) (attributes[i].end - attributes[i].value);
        String attrValue = toString(attributes[i].value, valueLength);
        String attrPrefix = toString(attributes[i].prefix);
        String attrQName = attrPrefix.isEmpty() ? attrLocalName : attrPrefix + ":" + attrLocalName;

        state->set(attrQName, attrValue);
    }
}

Optional<HashMap<String, String>> parseAttributes(const String& string)
{
    String parseString = "<?xml version=\"1.0\"?><attrs " + string + " />";

    AttributeParseState attributes;

    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));
    sax.startElementNs = attributesStartElementNsHandler;
    sax.initialized = XML_SAX2_MAGIC;

    auto parser = XMLParserContext::createStringParser(&sax, &attributes);

    // FIXME: Can we parse 8-bit strings directly as Latin-1 instead of upconverting to UTF-16?
    xmlParseChunk(parser->context(), reinterpret_cast<const char*>(StringView(parseString).upconvertedCharacters().get()), parseString.length() * sizeof(UChar), 1);

    return attributes;
}

}
