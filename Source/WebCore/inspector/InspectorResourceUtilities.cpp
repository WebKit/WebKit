/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorResourceUtilities.h"

#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "CachedScript.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentResourceLoader.h"
#include "FrameLoader.h"
#include "InspectorResourceType.h"
#include "LocalFrame.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "SharedBuffer.h"

namespace Inspector {

namespace ResourceUtilities {

using namespace WebCore;

Inspector::Protocol::Page::ResourceType resourceTypeToProtocol(Inspector::ResourceType resourceType)
{
    switch (resourceType) {
    case ResourceType::Document:
        return Inspector::Protocol::Page::ResourceType::Document;
    case ResourceType::Image:
        return Inspector::Protocol::Page::ResourceType::Image;
    case ResourceType::Font:
        return Inspector::Protocol::Page::ResourceType::Font;
    case ResourceType::StyleSheet:
        return Inspector::Protocol::Page::ResourceType::StyleSheet;
    case ResourceType::Script:
        return Inspector::Protocol::Page::ResourceType::Script;
    case ResourceType::XHR:
        return Inspector::Protocol::Page::ResourceType::XHR;
    case ResourceType::Fetch:
        return Inspector::Protocol::Page::ResourceType::Fetch;
    case ResourceType::Ping:
        return Inspector::Protocol::Page::ResourceType::Ping;
    case ResourceType::Beacon:
        return Inspector::Protocol::Page::ResourceType::Beacon;
    case ResourceType::WebSocket:
        return Inspector::Protocol::Page::ResourceType::WebSocket;
    case ResourceType::EventSource:
        return Inspector::Protocol::Page::ResourceType::EventSource;
    case ResourceType::Other:
        return Inspector::Protocol::Page::ResourceType::Other;
#if ENABLE(APPLICATION_MANIFEST)
    case ResourceType::ApplicationManifest:
        break;
#endif
    }
    return Inspector::Protocol::Page::ResourceType::Other;
}

[[nodiscard]] static bool decodeBuffer(std::span<const uint8_t> buffer, const String& textEncodingName, String* result)
{
    if (buffer.data()) {
        PAL::TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = PAL::WindowsLatin1Encoding();
        *result = encoding.decode(buffer);
        return true;
    }
    return false;
}

static bool dataContent(std::span<const uint8_t> data, const String& textEncodingName, bool withBase64Encode, String* result)
{
    if (withBase64Encode) {
        *result = base64EncodeToString(data);
        return true;
    }

    return decodeBuffer(data, textEncodingName, result);
}

bool sharedBufferContent(RefPtr<FragmentedSharedBuffer>&& buffer, const String& textEncodingName, bool withBase64Encode, String* result)
{
    return dataContent(buffer ? buffer->makeContiguous()->span() : std::span<const uint8_t> { }, textEncodingName, withBase64Encode, result);
}

Vector<CachedResource*> cachedResourcesForFrame(LocalFrame* frame)
{
    Vector<CachedResource*> result;

    for (auto& cachedResourceHandle : frame->document()->cachedResourceLoader().allCachedResources().values()) {
        auto* cachedResource = cachedResourceHandle.get();
        if (cachedResource->resourceRequest().hiddenFromInspector())
            continue;

        switch (cachedResource->type()) {
        case CachedResource::Type::ImageResource:
            // Skip images that were not auto loaded (images disabled in the user agent).
        case CachedResource::Type::SVGFontResource:
        case CachedResource::Type::FontResource:
            // Skip fonts that were referenced in CSS but never used/downloaded.
            if (cachedResource->stillNeedsLoad())
                continue;
            break;
        default:
            // All other CachedResource types download immediately.
            break;
        }

        result.append(cachedResource);
    }

    return result;
}

bool mainResourceContent(LocalFrame* frame, bool withBase64Encode, String* result)
{
    RefPtr<FragmentedSharedBuffer> buffer = frame->loader().documentLoader()->mainResourceData();
    if (!buffer)
        return false;
    return dataContent(buffer->makeContiguous()->span(), frame->document()->encoding(), withBase64Encode, result);
}

void resourceContent(Inspector::Protocol::ErrorString& errorString, LocalFrame* frame, const URL& url, String* result, bool* base64Encoded)
{
    RefPtr<DocumentLoader> loader = assertDocumentLoader(errorString, frame);
    if (!loader)
        return;

    RefPtr<FragmentedSharedBuffer> buffer;
    bool success = false;
    if (equalIgnoringFragmentIdentifier(url, loader->url())) {
        *base64Encoded = false;
        success = mainResourceContent(frame, *base64Encoded, result);
    }

    if (!success) {
        if (auto* resource = cachedResource(frame, url))
            success = cachedResourceContent(*resource, result, base64Encoded);
    }

    if (!success)
        errorString = "Missing resource for given url"_s;
}

String sourceMapURLForResource(CachedResource* cachedResource)
{
    if (!cachedResource)
        return String();

    // Scripts are handled in a separate path.
    if (cachedResource->type() != CachedResource::Type::CSSStyleSheet)
        return String();

    String sourceMapHeader = cachedResource->response().httpHeaderField(HTTPHeaderName::SourceMap);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    sourceMapHeader = cachedResource->response().httpHeaderField(HTTPHeaderName::XSourceMap);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    String content;
    bool base64Encoded;
    if (cachedResourceContent(*cachedResource, &content, &base64Encoded) && !base64Encoded)
        return ContentSearchUtilities::findStylesheetSourceMapURL(content);

    return String();
}

CachedResource* cachedResource(const LocalFrame* frame, const URL& url)
{
    if (url.isNull())
        return nullptr;

    CachedResource* cachedResource = frame->document()->cachedResourceLoader().cachedResource(MemoryCache::removeFragmentIdentifierIfNeeded(url));
    if (!cachedResource) {
        ResourceRequest request(URL { url });
        request.setDomainForCachePartition(frame->document()->domainForCachePartition());
        cachedResource = MemoryCache::singleton().resourceForRequest(request, frame->page()->sessionID());
    }

    return cachedResource;
}

Inspector::ResourceType inspectorResourceType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::ImageResource:
        return ResourceType::Image;
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::FontResource:
        return ResourceType::Font;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::CSSStyleSheet:
        return ResourceType::StyleSheet;
    case CachedResource::Type::JSON: // FIXME: Add ResourceType::JSON.
    case CachedResource::Type::Script:
        return ResourceType::Script;
    case CachedResource::Type::MainResource:
        return ResourceType::Document;
    case CachedResource::Type::Beacon:
        return ResourceType::Beacon;
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return ResourceType::ApplicationManifest;
#endif
    case CachedResource::Type::Ping:
        return ResourceType::Ping;
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::RawResource:
    default:
        return ResourceType::Other;
    }
}

ResourceType inspectorResourceType(const CachedResource& cachedResource)
{
    if (cachedResource.type() == CachedResource::Type::MainResource && MIMETypeRegistry::isSupportedImageMIMEType(cachedResource.mimeType()))
        return ResourceType::Image;

    if (cachedResource.type() == CachedResource::Type::RawResource) {
        switch (cachedResource.resourceRequest().requester()) {
        case ResourceRequestRequester::Fetch:
            return ResourceType::Fetch;
        case ResourceRequestRequester::Main:
            return ResourceType::Document;
        case ResourceRequestRequester::EventSource:
            return ResourceType::EventSource;
        default:
            return ResourceType::XHR;
        }
    }

    return inspectorResourceType(cachedResource.type());
}

Inspector::Protocol::Page::ResourceType cachedResourceTypeToProtocol(const CachedResource& cachedResource)
{
    return resourceTypeToProtocol(inspectorResourceType(cachedResource));
}

LocalFrame* findFrameWithSecurityOrigin(Page& page, const String& originRawString)
{
    // FIXME: this frame tree traversal needs to be redesigned for Site Isolation.
    for (SUPPRESS_UNCOUNTED_LOCAL auto* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        SUPPRESS_UNCOUNTED_LOCAL auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (localFrame->document()->securityOrigin().toRawString() == originRawString)
            return localFrame;
    }
    return nullptr;
}

DocumentLoader* assertDocumentLoader(Inspector::Protocol::ErrorString& errorString, LocalFrame* frame)
{
    FrameLoader& frameLoader = frame->loader();
    SUPPRESS_UNCOUNTED_LOCAL auto* documentLoader = frameLoader.documentLoader();
    if (!documentLoader)
        errorString = "Missing document loader for given frame"_s;
    return documentLoader;
}

bool shouldTreatAsText(const String& mimeType)
{
    return startsWithLettersIgnoringASCIICase(mimeType, "text/"_s)
        || MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType)
        || MIMETypeRegistry::isSupportedJSONMIMEType(mimeType)
        || MIMETypeRegistry::isXMLMIMEType(mimeType)
        || MIMETypeRegistry::isTextMediaPlaylistMIMEType(mimeType);
}

Ref<TextResourceDecoder> createTextDecoder(const String& mimeType, const String& textEncodingName)
{
    if (!textEncodingName.isEmpty())
        return TextResourceDecoder::create("text/plain"_s, textEncodingName);

    if (MIMETypeRegistry::isTextMIMEType(mimeType))
        return TextResourceDecoder::create(mimeType, "UTF-8"_s);

    if (MIMETypeRegistry::isXMLMIMEType(mimeType)) {
        auto decoder = TextResourceDecoder::create("application/xml"_s);
        decoder->useLenientXMLDecoding();
        return decoder;
    }

    return TextResourceDecoder::create("text/plain"_s, "UTF-8"_s);
}

std::optional<String> textContentForCachedResource(CachedResource& cachedResource)
{
    if (!shouldTreatAsText(cachedResource.mimeType()))
        return std::nullopt;

    String result;
    bool base64Encoded;
    if (cachedResourceContent(cachedResource, &result, &base64Encoded)) {
        ASSERT(!base64Encoded);
        return result;
    }

    return std::nullopt;
}

bool cachedResourceContent(CachedResource& resource, String* result, bool* base64Encoded)
{
    ASSERT(result);
    ASSERT(base64Encoded);

    if (!resource.encodedSize()) {
        *base64Encoded = false;
        *result = String();
        return true;
    }

    switch (resource.type()) {
    case CachedResource::Type::CSSStyleSheet:
        *base64Encoded = false;
        *result = downcast<CachedCSSStyleSheet>(resource).sheetText();
        // The above can return a null String if the MIME type is invalid.
        return !result->isNull();
    case CachedResource::Type::JSON:
    case CachedResource::Type::Script:
        *base64Encoded = false;
        *result = downcast<CachedScript>(resource).script().toString();
        return true;
    default:
        RefPtr buffer = resource.resourceBuffer();
        if (!buffer)
            return false;

        if (shouldTreatAsText(resource.mimeType())) {
            auto decoder = createTextDecoder(resource.mimeType(), resource.response().textEncodingName());
            *base64Encoded = false;
            *result = decoder->decodeAndFlush(buffer->makeContiguous()->span());
            return true;
        }

        *base64Encoded = true;
        *result = base64EncodeToString(buffer->makeContiguous()->span());
        return true;
    }
}

} // namespace ResourceUtilities

} // namespace Inspector
