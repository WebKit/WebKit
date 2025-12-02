/*
 * Copyright (C) 2000 Peter Kelly <pmk@post.com>
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
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
#include "CommonAtomStrings.h"
#include "CustomElementReactionQueue.h"
#include "CustomElementRegistry.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentResourceLoader.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentType.h"
#include "EventLoop.h"
#include "FrameConsoleClient.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLEntityParser.h"
#include "HTMLHtmlElement.h"
#include "HTMLTemplateElement.h"
#include "HTTPParsers.h"
#include "InlineClassicScript.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "MIMETypeRegistry.h"
#include "NodeDocument.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
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
#include "TextResourceDecoder.h"
#include "ThrowOnDynamicMarkupInsertionCountIncrementer.h"
#include "TransformSource.h"
#include "UserScriptTypes.h"
#include "XMLDocumentParserScope.h"
#include "XMLNSNames.h"
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <wtf/MallocSpan.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
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

    RefPtr frame = document.frame();
    if (!frame)
        return false;

    if (!frame->settings().developerExtrasEnabled())
        return false;

    if (frame->tree().parent())
        return false; // This document is not in a top frame

    return true;
}

#endif

// xmlMalloc() and xmlFree() are macros that call malloc() and free(), respectively. Thus, they
// cannot be called directly from XMLMalloc::malloc() and XMLMalloc::free() or they would cause
// infinite recusion.

#if HAVE(TYPE_AWARE_MALLOC)
using XMLMalloc = WTF::SystemMalloc;
#else
static void* xmlMallocHelper(size_t size)
{
    return xmlMalloc(size);
}

static void xmlFreeHelper(void* p)
{
    xmlFree(p);
}

struct XMLMalloc {
    static void* malloc(size_t size)
    {
        return xmlMallocHelper(size);
    }
    static void free(void* p) { xmlFreeHelper(p); }
};
#endif

static std::span<xmlChar> unsafeSpanIncludingNullTerminator(xmlChar* string)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return unsafeMakeSpan(string, string ? strlen(byteCast<char>(string)) + 1 : 0);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

class PendingCallbacks {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PendingCallbacks);
public:
    void appendStartElementNSCallback(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int numNamespaces, const xmlChar** rawNamespaces, int numAttributes, int numDefaulted, const xmlChar** rawAttributes)
    {
        auto namespaces = unsafeMakeSpan(rawNamespaces, numNamespaces * 2);
        auto attributes = unsafeMakeSpan(rawAttributes, numAttributes * 5);
        auto callback = makeUnique<PendingStartElementNSCallback>();

        callback->xmlLocalName = xmlStrdup(xmlLocalName);
        callback->xmlPrefix = xmlStrdup(xmlPrefix);
        callback->xmlURI = xmlStrdup(xmlURI);
        callback->numNamespaces = numNamespaces;
        callback->namespaces = MallocSpan<xmlChar*, XMLMalloc>::malloc(sizeof(xmlChar*) * numNamespaces * 2);
        for (int i = 0; i < numNamespaces * 2 ; ++i)
            callback->namespaces[i] = xmlStrdup(namespaces[i]);
        callback->numAttributes = numAttributes;
        callback->numDefaulted = numDefaulted;
        callback->attributes = MallocSpan<xmlChar*, XMLMalloc>::malloc(sizeof(xmlChar*) * numAttributes * 5);
        for (int i = 0; i < numAttributes; ++i) {
            // Each attribute has 5 elements in the array:
            // name, prefix, uri, value and an end pointer.

            for (int j = 0; j < 3; ++j)
                callback->attributes[i * 5 + j] = xmlStrdup(attributes[i * 5 + j]);

            int len = attributes[i * 5 + 4] - attributes[i * 5 + 3];

            callback->attributes[i * 5 + 3] = xmlStrndup(attributes[i * 5 + 3], len);
            callback->attributes[i * 5 + 4] = unsafeSpanIncludingNullTerminator(callback->attributes[i * 5 + 3]).subspan(len).data();
        }

        m_callbacks.append(WTFMove(callback));
    }

    void appendEndElementNSCallback()
    {
        m_callbacks.append(makeUnique<PendingEndElementNSCallback>());
    }

    void appendCharactersCallback(std::span<const xmlChar> s)
    {
        auto callback = makeUnique<PendingCharactersCallback>();

        callback->s = MallocSpan<xmlChar, XMLMalloc>::malloc(s.size());
        memcpySpan(callback->s.mutableSpan(), s);

        m_callbacks.append(WTFMove(callback));
    }

    void appendProcessingInstructionCallback(const xmlChar* target, const xmlChar* data)
    {
        auto callback = makeUnique<PendingProcessingInstructionCallback>();

        callback->target = xmlStrdup(target);
        callback->data = xmlStrdup(data);

        m_callbacks.append(WTFMove(callback));
    }

    void appendCDATABlockCallback(std::span<const xmlChar> s)
    {
        auto callback = makeUnique<PendingCDATABlockCallback>();
        callback->s = MallocSpan<xmlChar, XMLMalloc>::malloc(s.size());
        memcpySpan(callback->s.mutableSpan(), s);

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

    void appendErrorCallback(XMLErrors::Type type, const xmlChar* message, OrdinalNumber lineNumber, OrdinalNumber columnNumber)
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
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(PendingCallback);
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
            for (int i = 0; i < numAttributes; i++) {
                for (int j = 0; j < 4; j++)
                    xmlFree(attributes[i * 5 + j]);
            }
        }

        void call(XMLDocumentParser* parser) override
        {
            parser->startElementNs(xmlLocalName, xmlPrefix, xmlURI, numNamespaces, const_cast<const xmlChar**>(namespaces.span().data()), numAttributes, numDefaulted, const_cast<const xmlChar**>(attributes.span().data()));
        }

        xmlChar* xmlLocalName;
        xmlChar* xmlPrefix;
        xmlChar* xmlURI;
        int numNamespaces;
        MallocSpan<xmlChar*, XMLMalloc> namespaces;
        int numAttributes;
        int numDefaulted;
        MallocSpan<xmlChar*, XMLMalloc> attributes;
    };

    struct PendingEndElementNSCallback : public PendingCallback {
        void call(XMLDocumentParser* parser) override
        {
            parser->endElementNs();
        }
    };

    struct PendingCharactersCallback : public PendingCallback {
        void call(XMLDocumentParser* parser) override
        {
            parser->characters(s.span());
        }

        MallocSpan<xmlChar, XMLMalloc> s;
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
        void call(XMLDocumentParser* parser) override
        {
            parser->cdataBlock(s.span());
        }

        MallocSpan<xmlChar, XMLMalloc> s;
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

        XMLErrors::Type type;
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
    return XMLDocumentParserScope::currentCachedResourceLoader() && libxmlLoaderThread == &Thread::currentSingleton();
}

class OffsetBuffer {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(OffsetBuffer);
public:
    OffsetBuffer(Vector<uint8_t>&& buffer)
        : m_buffer(WTFMove(buffer))
    {
    }

    int readOutBytes(std::span<char> outputBuffer)
    {
        size_t bytesLeft = m_buffer.size() - m_currentOffset;
        size_t lengthToCopy = std::min(outputBuffer.size(), bytesLeft);
        if (lengthToCopy) {
            memcpySpan(outputBuffer, m_buffer.subspan(m_currentOffset, lengthToCopy));
            m_currentOffset += lengthToCopy;
        }
        return lengthToCopy;
    }

private:
    Vector<uint8_t> m_buffer;
    unsigned m_currentOffset { 0 };
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

static inline void setAttributes(Element* element, Vector<Attribute>& attributeVector, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    if (!scriptingContentIsAllowed(parserContentPolicy))
        element->stripScriptingAttributes(attributeVector);
    element->parserSetAttributes(attributeVector.span());
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
    if (urlString == "file:///etc/xml/catalog"_s)
        return false;

    // On Windows, libxml computes a URL relative to where its DLL resides.
    if (startsWithLettersIgnoringASCIICase(urlString, "file:///"_s) && urlString.endsWithIgnoringASCIICase("/etc/catalog"_s))
        return false;

    // The most common DTD. There isn't much point in hammering www.w3c.org by requesting this for every XHTML document.
    if (startsWithLettersIgnoringASCIICase(urlString, "http://www.w3.org/tr/xhtml"_s))
        return false;

    // Similarly, there isn't much point in requesting the SVG DTD.
    if (startsWithLettersIgnoringASCIICase(urlString, "http://www.w3.org/graphics/svg"_s))
        return false;

    // This will crash due a missing XMLDocumentParserScope object in WebKit, or when
    // a non-WebKit, in-process framework/library uses libxml2 off the main thread.
    ASSERT(!!XMLDocumentParserScope::currentCachedResourceLoader());

    // The libxml doesn't give us a lot of context for deciding whether to
    // allow this request.  In the worst case, this load could be for an
    // external entity and the resulting document could simply read the
    // retrieved content.  If we had more context, we could potentially allow
    // the parser to load a DTD.  As things stand, we take the conservative
    // route and allow same-origin requests only.
    RefPtr currentCachedResourceLoader = XMLDocumentParserScope::currentCachedResourceLoader().get();
    if (!currentCachedResourceLoader || !currentCachedResourceLoader->document())
        return false;
    if (!currentCachedResourceLoader->document()->protectedSecurityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton())) {
        currentCachedResourceLoader->printAccessDeniedMessage(url);
        return false;
    }

    return true;
}

static void* openFunc(const char* uri)
{
    ASSERT(XMLDocumentParserScope::currentCachedResourceLoader());
    ASSERT(libxmlLoaderThread == &Thread::currentSingleton());

    RefPtr cachedResourceLoader = XMLDocumentParserScope::currentCachedResourceLoader().get();
    if (!cachedResourceLoader)
        return &globalDescriptor;

    RefPtr document = cachedResourceLoader->document();
    // Same logic as Document::completeURL(). Keep them in sync.
    auto* encoding = (document && document->decoder()) ? document->decoder()->encodingForURLParsing() : nullptr;
    URL url(document ? document->fallbackBaseURL() : URL(), String::fromLatin1(uri), encoding);

    if (!shouldAllowExternalLoad(url))
        return &globalDescriptor;

    ResourceResponse response;
    RefPtr<SharedBuffer> data;

    {
        ResourceError error;
        XMLDocumentParserScope scope(nullptr);
        // FIXME: We should restore the original global error handler as well.

        if (cachedResourceLoader->frame()) {
            FetchOptions options;
            options.mode = FetchOptions::Mode::SameOrigin;
            options.credentials = FetchOptions::Credentials::Include;
            cachedResourceLoader->frame()->loader().loadResourceSynchronously(URL { url }, ClientCredentialPolicy::MayAskClientForCredentials, options, { }, error, response, data);

            if (response.url().isEmpty()) {
                if (RefPtr frame = document ? document->frame() : nullptr)
                    frame->console().addMessage(MessageSource::Security, MessageLevel::Error, makeString("Did not parse external entity resource at '"_s, url.stringCenterEllipsizedToLength(), "' because cross-origin loads are not allowed."_s));
                return &globalDescriptor;
            }
            if (!externalEntityMimeTypeAllowed(response)) {
                if (RefPtr frame = document ? document->frame() : nullptr)
                    frame->console().addMessage(MessageSource::Security, MessageLevel::Error, makeString("Did not parse external entity resource at '"_s, url.stringCenterEllipsizedToLength(), "' because only XML MIME types are allowed."_s));
                return &globalDescriptor;
            }
        }
    }

    if (!data)
        return &globalDescriptor;

    return new OffsetBuffer(Vector(data->span()));
}

static int readFunc(void* context, char* buffer, int len)
{
    // Do 0-byte reads in case of a null descriptor
    if (context == &globalDescriptor)
        return 0;

    OffsetBuffer* data = static_cast<OffsetBuffer*>(context);
    return data->readOutBytes(unsafeMakeSpan(buffer, len));
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

static xmlExternalEntityLoader defaultEntityLoader { nullptr };

xmlParserInputPtr externalEntityLoader(const char* url, const char* id, xmlParserCtxtPtr context)
{
    if (!shouldAllowExternalLoad(URL(String::fromUTF8(url))))
        return nullptr;
    RELEASE_ASSERT_WITH_MESSAGE(!!defaultEntityLoader, "Missing call to initializeXMLParser()");
    return defaultEntityLoader(url, id, context);
}

void initializeXMLParser()
{
    static std::once_flag flag;
    std::call_once(flag, [&] {
        xmlInitParser();
        xmlRegisterInputCallbacks(matchFunc, openFunc, readFunc, closeFunc);
        xmlRegisterOutputCallbacks(matchFunc, openFunc, writeFunc, closeFunc);
        defaultEntityLoader = xmlGetExternalEntityLoader();
        RELEASE_ASSERT_WITH_MESSAGE(defaultEntityLoader != WebCore::externalEntityLoader, "XMLDocumentParserScope was created too early");
        libxmlLoaderThread = &Thread::currentSingleton();
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
        return nullptr;

    memcpySpan(asMutableByteSpan(*parser->sax), asByteSpan(*handlers));

    // Substitute entities.
    // FIXME: Why is XML_PARSE_NODICT needed? This is different from what createStringParser does.
    xmlCtxtUseOptions(parser, XML_PARSE_NODICT | XML_PARSE_NOENT | XML_PARSE_HUGE);

#if LIBXML_VERSION < 21300
    // Internal initialization required before libxml2 2.13.
    // Fixed with https://gitlab.gnome.org/GNOME/libxml2/-/commit/8c5848bd
    parser->sax2 = 1;
    parser->instate = XML_PARSER_CONTENT; // We are parsing a CONTENT
    parser->depth = 0;
    parser->str_xml = xmlDictLookup(parser->dict, byteCast<xmlChar>(const_cast<char*>("xml")), 3);
    parser->str_xmlns = xmlDictLookup(parser->dict, byteCast<xmlChar>(const_cast<char*>("xmlns")), 5);
    parser->str_xml_ns = xmlDictLookup(parser->dict, XML_XML_NAMESPACE, 36);
#endif
    parser->_private = userData;

    return adoptRef(*new XMLParserContext(parser));
}

// --------------------------------

bool XMLDocumentParser::supportsXMLVersion(const String& version)
{
    return version == "1.0"_s;
}

XMLDocumentParser::XMLDocumentParser(Document& document, IsInFrameView isInFrameView, OptionSet<ParserContentPolicy> policy)
    : ScriptableDocumentParser(document, policy)
    , m_isInFrameView(isInFrameView)
    , m_pendingCallbacks(makeUniqueRef<PendingCallbacks>())
    , m_currentNode(&document)
    , m_scriptStartPosition(TextPosition::belowRangePosition())
{
}

XMLDocumentParser::XMLDocumentParser(DocumentFragment& fragment, HashMap<AtomString, AtomString>&& prefixToNamespaceMap, const AtomString& defaultNamespaceURI, OptionSet<ParserContentPolicy> parserContentPolicy)
    : ScriptableDocumentParser(fragment.document(), parserContentPolicy)
    , m_pendingCallbacks(makeUniqueRef<PendingCallbacks>())
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
        xmlParseChunk(context->context(), reinterpret_cast<const char*>(StringView(parseString).upconvertedCharacters().get()), sizeof(char16_t) * parseString.length(), 0);

        // JavaScript (which may be run under the xmlParseChunk callstack) may
        // cause the parser to be stopped or detached.
        if (isStopped())
            return;
    }

    // FIXME: Why is this here?  And why is it after we process the passed source?
    if (document()->decoder() && document()->decoder()->sawError()) {
        // If the decoder saw an error, report it as fatal (stops parsing)
        TextPosition position(OrdinalNumber::fromOneBasedInt(context->context()->input->line), OrdinalNumber::fromOneBasedInt(context->context()->input->col));
        handleError(XMLErrors::Type::Fatal, "Encoding error", position);
    }
}

static inline String toString(std::span<const xmlChar> string)
{
    return String::fromUTF8(byteCast<char>(string));
}

static inline String toString(const xmlChar* string)
{
    return String::fromUTF8(byteCast<char>(string));
}

static inline AtomString toAtomString(std::span<const xmlChar> string)
{
    return AtomString::fromUTF8(byteCast<char>(string));
}

static inline AtomString toAtomString(const xmlChar* string)
{
    return AtomString::fromUTF8(byteCast<char>(string));
}

struct _xmlSAX2Namespace {
    const xmlChar* prefix;
    const xmlChar* uri;
};
typedef struct _xmlSAX2Namespace xmlSAX2Namespace;

static inline bool handleNamespaceAttributes(Vector<Attribute>& prefixedAttributes, const xmlChar** libxmlNamespaces, int numNamespaces, bool& shouldUseNullCustomElementRegistry)
{
    auto namespaces = unsafeMakeSpan(reinterpret_cast<xmlSAX2Namespace*>(libxmlNamespaces), numNamespaces);
    for (auto& xmlNamespace : namespaces) {
        AtomString namespaceQName = xmlnsAtom();
        AtomString namespaceURI = toAtomString(xmlNamespace.uri);
        if (xmlNamespace.prefix)
            namespaceQName = makeAtomString("xmlns:"_s, toString(xmlNamespace.prefix));

        auto result = Element::parseAttributeName(XMLNSNames::xmlnsNamespaceURI, namespaceQName);
        if (result.hasException())
            return false;

        QualifiedName attributeName = result.releaseReturnValue();
        if (attributeName == HTMLNames::customelementregistryAttr)
            shouldUseNullCustomElementRegistry = true;

        prefixedAttributes.append(Attribute(attributeName, namespaceURI));
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

static inline bool handleElementAttributes(Vector<Attribute>& prefixedAttributes, const xmlChar** libxmlAttributes, int numAttributes, bool& shouldUseNullCustomElementRegistry)
{
    auto attributes = unsafeMakeSpan(reinterpret_cast<xmlSAX2Attributes*>(libxmlAttributes), numAttributes);
    for (auto& attribute : attributes) {
        size_t valueLength = static_cast<size_t>(attribute.end - attribute.value);
        AtomString attrValue = toAtomString(unsafeMakeSpan(attribute.value, valueLength));
        String attrPrefix = toString(attribute.prefix);
        AtomString attrURI = attrPrefix.isEmpty() ? nullAtom() : toAtomString(attribute.uri);
        AtomString attrQName = attrPrefix.isEmpty() ? toAtomString(attribute.localname) : makeAtomString(attrPrefix, ':', toString(attribute.localname));

        auto result = Element::parseAttributeName(attrURI, attrQName);
        if (result.hasException())
            return false;

        QualifiedName attributeName = result.releaseReturnValue();
        if (attributeName == HTMLNames::customelementregistryAttr)
            shouldUseNullCustomElementRegistry = true;

        prefixedAttributes.append(Attribute(attributeName, attrValue));
    }
    return true;
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
        else if (is<SVGElement>(m_currentNode.get()) || localName == SVGNames::svgTag->localName())
            uri = SVGNames::svgNamespaceURI;
        else
            uri = m_defaultNamespaceURI;
    }

    bool isFirstElement = !m_sawFirstElement;
    m_sawFirstElement = true;

    QualifiedName qName(prefix, localName, uri);

    bool willConstructCustomElement = false;
    if (!m_parsingFragment) {
        if (RefPtr window = m_currentNode->document().window()) {
            if (RefPtr registry = window->customElementRegistry(); registry) [[unlikely]]
                willConstructCustomElement = registry->findInterface(qName);
        }
    }

    std::optional<ThrowOnDynamicMarkupInsertionCountIncrementer> markupInsertionCountIncrementer;
    std::optional<CustomElementReactionStack> customElementReactionStack;
    if (willConstructCustomElement) [[unlikely]] {
        markupInsertionCountIncrementer.emplace(m_currentNode->document());
        m_currentNode->document().eventLoop().performMicrotaskCheckpoint();
        customElementReactionStack.emplace(m_currentNode->document().globalObject());
    }

    Vector<Attribute> prefixedAttributes;
    bool shouldUseNullCustomElementRegistry = false;
    bool handledAttributes = handleNamespaceAttributes(prefixedAttributes, libxmlNamespaces, numNamespaces, shouldUseNullCustomElementRegistry);
    bool success = handledAttributes ? handleElementAttributes(prefixedAttributes, libxmlAttributes, numAttributes, shouldUseNullCustomElementRegistry) : false;

    RefPtr registry = shouldUseNullCustomElementRegistry ? nullptr : CustomElementRegistry::registryForNodeOrTreeScope(*m_currentNode, m_currentNode->treeScope());
    auto newElement = m_currentNode->document().createElement(qName, true, registry.get());

    if (!handledAttributes) {
        setAttributes(newElement.ptr(), prefixedAttributes, parserContentPolicy());
        stopParsing();
        return;
    }

    setAttributes(newElement.ptr(), prefixedAttributes, parserContentPolicy());
    if (!success) {
        stopParsing();
        return;
    }

    if (willConstructCustomElement) [[unlikely]] {
        customElementReactionStack.reset();
        markupInsertionCountIncrementer.reset();
    }

    newElement->beginParsingChildren();

    if (isScriptElement(newElement.get()))
        m_scriptStartPosition = textPosition();

    m_currentNode->parserAppendChild(newElement);
    if (!m_currentNode) // Synchronous DOM events may have removed the current node.
        return;

    if (RefPtr templateElement = dynamicDowncast<HTMLTemplateElement>(newElement))
        pushCurrentNode(&templateElement->content());
    else
        pushCurrentNode(newElement.ptr());

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

    Ref node = *m_currentNode;
    RefPtr element = dynamicDowncast<Element>(node);

    if (element)
        element->finishParsingChildren();

    if (!scriptingContentIsAllowed(parserContentPolicy()) && element && isScriptElement(*element)) {
        popCurrentNode();
        node->remove();
        return;
    }

    if (!element || m_isInFrameView == IsInFrameView::No) {
        popCurrentNode();
        return;
    }

    // The element's parent may have already been removed from document.
    // Parsing continues in this case, but scripts aren't executed.
    if (!element->isConnected()) {
        popCurrentNode();
        return;
    }

    RefPtr scriptElement = dynamicDowncastScriptElement(*element);
    if (!scriptElement) {
        popCurrentNode();
        return;
    }

    // Don't load external scripts for standalone documents (for now).
    ASSERT(!m_pendingScript);
    m_requestingScript = true;

    if (scriptElement->prepareScript(m_scriptStartPosition)) {
        if (scriptElement->readyToBeParserExecuted()) {
            if (scriptElement->scriptType() == ScriptType::Classic)
                scriptElement->executeClassicScript(ScriptSourceCode(scriptElement->scriptContent(), scriptElement->sourceTaintedOrigin(), URL(document()->url()), m_scriptStartPosition, JSC::SourceProviderSourceType::Program, InlineClassicScript::create(*scriptElement)));
            else
                scriptElement->registerImportMap(ScriptSourceCode(scriptElement->scriptContent(), scriptElement->sourceTaintedOrigin(), URL(document()->url()), m_scriptStartPosition, JSC::SourceProviderSourceType::ImportMap));
        } else if (scriptElement->willBeParserExecuted() && scriptElement->loadableScript()) {
            m_pendingScript = PendingScript::create(*scriptElement, *scriptElement->loadableScript());
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

void XMLDocumentParser::characters(std::span<const xmlChar> characters)
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCharactersCallback(characters);
        return;
    }

    if (!m_leafTextNode)
        createLeafTextNode();
    m_bufferedText.append(characters);
}

void XMLDocumentParser::error(XMLErrors::Type type, const char* message, va_list args)
{
    if (isStopped())
        return;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    va_list preflightArgs;
    va_copy(preflightArgs, args);
    size_t stringLength = vsnprintf(nullptr, 0, message, preflightArgs);
    va_end(preflightArgs);

    Vector<char, 1024> buffer(stringLength + 1);
    vsnprintf(buffer.mutableSpan().data(), stringLength + 1, message, args);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    TextPosition position = textPosition();
    if (m_parserPaused)
        m_pendingCallbacks->appendErrorCallback(type, reinterpret_cast<const xmlChar*>(buffer.span().data()), position.m_line, position.m_column);
    else
        handleError(type, buffer.span().data(), textPosition());
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

    pi->setCreatedByParser(false);

    if (pi->isCSS())
        m_sawCSS = true;

#if ENABLE(XSLT)
    m_sawXSLTransform = !m_sawFirstElement && pi->isXSL();
    if (m_sawXSLTransform && !document()->transformSourceDocument())
        stopParsing();
#endif
}

void XMLDocumentParser::cdataBlock(std::span<const xmlChar> s)
{
    if (isStopped())
        return;

    if (m_parserPaused) {
        m_pendingCallbacks->appendCDATABlockCallback(s);
        return;
    }

    if (!updateLeafTextNode())
        return;

    m_currentNode->parserAppendChild(CDATASection::create(m_currentNode->document(), toString(s)));
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

static void startElementNsHandler(void* closure, const xmlChar* localname, const xmlChar* prefix, const xmlChar* uri, int numNamespaces, const xmlChar** namespaces, int numAttributes, int numDefaulted, const xmlChar** libxmlAttributes)
{
    getParser(closure)->startElementNs(localname, prefix, uri, numNamespaces, namespaces, numAttributes, numDefaulted, libxmlAttributes);
}

static void endElementNsHandler(void* closure, const xmlChar*, const xmlChar*, const xmlChar*)
{
    getParser(closure)->endElementNs();
}

static void charactersHandler(void* closure, const xmlChar* s, int len)
{
    getParser(closure)->characters(unsafeMakeSpan(s, len));
}

static void processingInstructionHandler(void* closure, const xmlChar* target, const xmlChar* data)
{
    getParser(closure)->processingInstruction(target, data);
}

static void cdataBlockHandler(void* closure, const xmlChar* s, int len)
{
    getParser(closure)->cdataBlock(unsafeMakeSpan(s, len));
}

static void commentHandler(void* closure, const xmlChar* comment)
{
    getParser(closure)->comment(comment);
}

WTF_ATTRIBUTE_PRINTF(2, 3)
static void warningHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getParser(closure)->error(XMLErrors::Type::Warning, message, args);
    va_end(args);
}

WTF_ATTRIBUTE_PRINTF(2, 3)
static void fatalErrorHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getParser(closure)->error(XMLErrors::Type::Fatal, message, args);
    va_end(args);
}

WTF_ATTRIBUTE_PRINTF(2, 3)
static void normalErrorHandler(void* closure, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    getParser(closure)->error(XMLErrors::Type::NonFatal, message, args);
    va_end(args);
}

// Using a static entity and marking it XML_INTERNAL_PREDEFINED_ENTITY is
// a hack to avoid malloc/free. Using a global variable like this could cause trouble
// if libxml implementation details were to change
static std::array<xmlChar, 9> sharedXHTMLEntityResult = { };

static xmlEntityPtr sharedXHTMLEntity()
{
    static xmlEntity entity;
    if (!entity.type) {
        entity.type = XML_ENTITY_DECL;
        entity.orig = sharedXHTMLEntityResult.data();
        entity.content = sharedXHTMLEntityResult.data();
        entity.etype = XML_INTERNAL_PREDEFINED_ENTITY;
    }
    return &entity;
}

static size_t convertUTF16EntityToUTF8(std::span<const char16_t> utf16Entity, std::span<char8_t> target)
{
    auto result = WTF::Unicode::convert(utf16Entity, target);
    if (result.code != WTF::Unicode::ConversionResultCode::Success)
        return 0;

    // Even though we must pass the length, libxml expects the entity string to be null terminated.
    ASSERT(!result.buffer.empty());
    target[result.buffer.size()] = '\0';
    return result.buffer.size();
}

static xmlEntityPtr getXHTMLEntity(const xmlChar* name)
{
    auto decodedEntity = decodeNamedHTMLEntityForXMLParser(byteCast<char>(name));
    if (decodedEntity.failed())
        return nullptr;

    auto utf16DecodedEntity = decodedEntity.span();

    size_t entityLengthInUTF8;
    // Unlike HTML parser, XML parser parses the content of named
    // entities. So we need to escape '&' and '<'.
    if (utf16DecodedEntity.size() == 1 && utf16DecodedEntity[0] == '&') {
        sharedXHTMLEntityResult[0] = '&';
        sharedXHTMLEntityResult[1] = '#';
        sharedXHTMLEntityResult[2] = '3';
        sharedXHTMLEntityResult[3] = '8';
        sharedXHTMLEntityResult[4] = ';';
        entityLengthInUTF8 = 5;
    } else if (utf16DecodedEntity.size() == 1 && utf16DecodedEntity[0] == '<') {
        sharedXHTMLEntityResult[0] = '&';
        sharedXHTMLEntityResult[1] = '#';
        sharedXHTMLEntityResult[2] = 'x';
        sharedXHTMLEntityResult[3] = '3';
        sharedXHTMLEntityResult[4] = 'C';
        sharedXHTMLEntityResult[5] = ';';
        entityLengthInUTF8 = 6;
    } else if (utf16DecodedEntity.size() == 2 && utf16DecodedEntity[0] == '<' && utf16DecodedEntity[1] == 0x20D2) {
        sharedXHTMLEntityResult[0] = '&';
        sharedXHTMLEntityResult[1] = '#';
        sharedXHTMLEntityResult[2] = '6';
        sharedXHTMLEntityResult[3] = '0';
        sharedXHTMLEntityResult[4] = ';';
        sharedXHTMLEntityResult[5] = 0xE2;
        sharedXHTMLEntityResult[6] = 0x83;
        sharedXHTMLEntityResult[7] = 0x92;
        entityLengthInUTF8 = 8;
    } else {
        ASSERT(utf16DecodedEntity.size() <= 4);
        entityLengthInUTF8 = convertUTF16EntityToUTF8(utf16DecodedEntity, byteCast<char8_t>(std::span { sharedXHTMLEntityResult }));
        if (!entityLengthInUTF8)
            return 0;
    }
    ASSERT(entityLengthInUTF8 <= sharedXHTMLEntityResult.size());

    xmlEntityPtr entity = sharedXHTMLEntity();
    entity->length = entityLengthInUTF8;
    entity->name = name;
    return entity;
}

static xmlEntityPtr getEntityHandler(void* closure, const xmlChar* name)
{
    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure);

    xmlEntityPtr ent = xmlGetPredefinedEntity(name);
    if (ent) {
        RELEASE_ASSERT(ent->etype == XML_INTERNAL_PREDEFINED_ENTITY);
        return ent;
    }

    ent = xmlGetDocEntity(ctxt->myDoc, name);
    if (!ent && getParser(closure)->isXHTMLDocument()) {
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
    if ((extId == "-//W3C//DTD XHTML 1.0 Transitional//EN"_s)
        || (extId == "-//W3C//DTD XHTML 1.1//EN"_s)
        || (extId == "-//W3C//DTD XHTML 1.0 Strict//EN"_s)
        || (extId == "-//W3C//DTD XHTML 1.0 Frameset//EN"_s)
        || (extId == "-//W3C//DTD XHTML Basic 1.0//EN"_s)
        || (extId == "-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN"_s)
        || (extId == "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN"_s)
        || (extId == "-//W3C//DTD MathML 2.0//EN"_s)
        || (extId == "-//WAPFORUM//DTD XHTML Mobile 1.0//EN"_s)
        || (extId == "-//WAPFORUM//DTD XHTML Mobile 1.1//EN"_s)
        || (extId == "-//WAPFORUM//DTD XHTML Mobile 1.2//EN"_s))
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
    zeroBytes(sax);

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
    DocumentParser::startParsing();
    m_sawError = false;
    m_sawCSS = false;
    m_sawXSLTransform = false;
    m_sawFirstElement = false;

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
                xmlParseChunk(context(), nullptr, 0, 1);
            }

            m_context = nullptr;
        }
    }

#if ENABLE(XSLT)
    if (isDetached())
        return;

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
    auto characters = is8Bit ? byteCast<char>(source.span8()) : spanReinterpretCast<const char>(source.span16());
    size_t sizeInBytes = source.length() * (is8Bit ? sizeof(Latin1Character) : sizeof(char16_t));
    const char* encoding = is8Bit ? "iso-8859-1" : nativeEndianUTF16Encoding();

    XMLDocumentParserScope scope(&cachedResourceLoader, errorFunc);
    return xmlReadMemory(characters.data(), sizeInBytes, url.latin1().data(), encoding, XSLT_PARSE_OPTIONS);
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

    CString chunkAsUTF8 = chunk.utf8();
    
    // libxml2 takes an int for a length, and therefore can't handle XML chunks larger than 2 GiB.
    if (chunkAsUTF8.length() > INT_MAX)
        return false;

    initializeParserContext(chunkAsUTF8);
    XMLDocumentParserScope scope(&document()->cachedResourceLoader());
    xmlParseContent(context());
    endDocument(); // Close any open text nodes.

#if LIBXML_VERSION < 21400
    // FIXME: If this code is actually needed, it should probably move to finish()
    // XMLDocumentParserQt has a similar check (m_stream.error() == QXmlStreamReader::PrematureEndOfDocumentError) in doEnd().
    // Check if all the chunk has been processed.
    long bytesProcessed = xmlByteConsumed(context());
    if (bytesProcessed == -1 || ((unsigned long)bytesProcessed) != chunkAsUTF8.length()) {
        // FIXME: I don't believe we can hit this case without also having seen an error or a null byte.
        // If we hit this ASSERT, we've found a test case which demonstrates the need for this code.
        ASSERT(m_sawError || (bytesProcessed >= 0 && !chunkAsUTF8.span()[bytesProcessed]));
        return false;
    }
#endif

    // No error if the chunk is well formed or it is not but we have no error.
    return context()->wellFormed || !xmlCtxtGetLastError(context());
}

// --------------------------------

using AttributeParseState = std::optional<HashMap<String, String>>;

static void attributesStartElementNsHandler(void* closure, const xmlChar* xmlLocalName, const xmlChar* /*xmlPrefix*/, const xmlChar* /*xmlURI*/, int /*numNamespaces*/, const xmlChar** /*namespaces*/, int numAttributes, int /*numDefaulted*/, const xmlChar** libxmlAttributes)
{
    if (!equalSpans(unsafeSpan(byteCast<char>(xmlLocalName)), "attrs"_span))
        return;

    auto& state = *static_cast<AttributeParseState*>(static_cast<xmlParserCtxtPtr>(closure)->_private);

    state = HashMap<String, String> { };

    auto attributes = unsafeMakeSpan(reinterpret_cast<xmlSAX2Attributes*>(libxmlAttributes), numAttributes);
    for (auto& attribute : attributes) {
        String attrLocalName = toString(attribute.localname);
        size_t valueLength = static_cast<size_t>(attribute.end - attribute.value);
        String attrValue = toString(unsafeMakeSpan(attribute.value, valueLength));
        String attrPrefix = toString(attribute.prefix);
        String attrQName = attrPrefix.isEmpty() ? attrLocalName : makeString(attrPrefix, ':', attrLocalName);

        state->set(attrQName, attrValue);
    }
}

std::optional<HashMap<String, String>> parseAttributes(CachedResourceLoader& cachedResourceLoader, const String& string)
{
    auto parseString = makeString("<?xml version=\"1.0\"?><attrs "_s, string, " />"_s);

    AttributeParseState attributes;

    xmlSAXHandler sax;
    zeroBytes(sax);
    sax.startElementNs = attributesStartElementNsHandler;
    sax.initialized = XML_SAX2_MAGIC;

    auto parser = XMLParserContext::createStringParser(&sax, &attributes);

    XMLDocumentParserScope scope(&cachedResourceLoader);
    // FIXME: Can we parse 8-bit strings directly as Latin-1 instead of upconverting to UTF-16?
    xmlParseChunk(parser->context(), reinterpret_cast<const char*>(StringView(parseString).upconvertedCharacters().get()), parseString.length() * sizeof(char16_t), 1);

    return attributes;
}

}
