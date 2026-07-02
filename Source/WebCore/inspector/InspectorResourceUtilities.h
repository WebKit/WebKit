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

#pragma once

#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <WebCore/CachedResource.h>
#include <WebCore/InspectorResourceType.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class DocumentLoader;
class FragmentedSharedBuffer;
class LocalFrame;
class Page;
class TextResourceDecoder;
}

namespace Inspector {
namespace Protocol {
using ErrorString = String;
namespace Page {
enum class ResourceType : int;
}
}

enum class ResourceType : uint8_t;

// Plain, IPC-serializable description of a frame's cached subresource. Mirrors the fields of
// Protocol::Page::FrameResource so a frame's resources can be gathered from its hosting
// WebContent process and the Protocol object built later in the UIProcess (rather than shipping
// protocol JSON across the wire). See webkit.org/b/308896.
struct FrameResource {
    String url;
    ResourceType type { ResourceType::Other };
    String mimeType;
    bool canceled { false };
    bool failed { false };
    String sourceMapURL;
    String targetId;
};

// Per-frame data gathered from a frame's hosting WebContent process: the committed document's
// loaderId (carried as a ScriptExecutionContextIdentifier and converted to the protocol loaderId
// string at the UIProcess boundary) and the frame's cached subresources.
struct FrameResourceData {
    std::optional<WebCore::ScriptExecutionContextIdentifier> loaderId;
    Vector<FrameResource> resources;
};

namespace ResourceUtilities {

WEBCORE_EXPORT bool sharedBufferContent(RefPtr<WebCore::FragmentedSharedBuffer>&&, const String& textEncodingName, bool withBase64Encode, String* result);
Vector<WebCore::CachedResource*> cachedResourcesForFrame(WebCore::LocalFrame*);
WEBCORE_EXPORT Ref<JSON::ArrayOf<Inspector::Protocol::Page::FrameResource>> buildResourceObjectsForFrame(WebCore::LocalFrame&);
WEBCORE_EXPORT Vector<Inspector::FrameResource> buildResourceDataForFrame(WebCore::LocalFrame&);
WEBCORE_EXPORT Ref<Inspector::Protocol::Page::FrameResource> buildResourceObject(const Inspector::FrameResource&);
void resourceContent(Inspector::Protocol::ErrorString&, WebCore::LocalFrame*, const URL&, String* result, bool* base64Encoded);
bool mainResourceContent(WebCore::LocalFrame*, bool withBase64Encode, String* result);

String sourceMapURLForResource(WebCore::CachedResource*);
RefPtr<WebCore::CachedResource> WEBCORE_EXPORT cachedResource(const WebCore::LocalFrame*, const URL&);
Inspector::ResourceType WEBCORE_EXPORT inspectorResourceType(WebCore::CachedResource::Type);
Inspector::ResourceType WEBCORE_EXPORT inspectorResourceType(const WebCore::CachedResource&);

Inspector::Protocol::Page::ResourceType NODELETE resourceTypeToProtocol(Inspector::ResourceType);
Inspector::Protocol::Page::ResourceType cachedResourceTypeToProtocol(const WebCore::CachedResource&);
WebCore::LocalFrame* findFrameWithSecurityOrigin(WebCore::Page&, const String& originRawString);
WebCore::DocumentLoader* assertDocumentLoader(Inspector::Protocol::ErrorString&, WebCore::LocalFrame*);

WEBCORE_EXPORT bool shouldTreatAsText(const String& mimeType);
WEBCORE_EXPORT Ref<WebCore::TextResourceDecoder> createTextDecoder(const String& mimeType, const String& textEncodingName);
std::optional<String> textContentForCachedResource(WebCore::CachedResource&);
WEBCORE_EXPORT bool cachedResourceContent(WebCore::CachedResource&, String* result, bool* base64Encoded);

} // namespace ResourceUtilities

} // namespace Inspector
