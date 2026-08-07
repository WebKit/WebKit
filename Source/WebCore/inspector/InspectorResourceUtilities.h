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
#include <WebCore/FrameIdentifier.h>
#include <WebCore/InspectorResourceType.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <cstdint>
#include <memory>
#include <tuple>
#include <wtf/Compiler.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class DocumentLoader;
class FragmentedSharedBuffer;
class HTTPHeaderMap;
class InstrumentingAgents;
class LocalFrame;
class NetworkLoadMetrics;
class Page;
class ScriptExecutionContext;
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
// protocol loaderId string (computed by that process so it matches the live Network events) and
// the frame's cached subresources.
struct FrameResourceData {
    String loaderId;
    Vector<FrameResource> resources;
};

// Plain, IPC-serializable mirror of Protocol::GenericTypes::SearchMatch; the UIProcess turns these
// back into protocol objects after gathering matches from a hosting WebContent process.
struct SearchMatch {
    int lineNumber { 0 };
    String lineContent;
};

// Plain, IPC-serializable mirror of Protocol::Page::SearchResult, carried unqualified across IPC;
// the UIProcess qualifies frameID and resourceID into protocol IDs at the agent boundary. resourceID
// is set only for hits from this process's BackendResourceDataStore (XHR/Fetch response bodies).
// frameID is optional only to keep the struct default-constructible for IPC decoding; every emitted
// result sets it.
struct SearchResult {
    String url;
    std::optional<WebCore::FrameIdentifier> frameID;
    int matchesCount { 0 };
    std::optional<WebCore::ResourceLoaderIdentifier> resourceID;
};

// Completion for Network.loadResource: {content, mimeType, status} on success, or an error string
// on failure.
using LoadResourceCompletionHandler = CompletionHandler<void(Expected<std::tuple<String /* content */, String /* mimeType */, int /* status */>, String /* error */>&&)>;

enum class InitiatorType : uint8_t {
    Parser,
    Script,
    Other,
};

// Plain, IPC-serializable mirror of Protocol::Console::CallFrame.
struct InitiatorCallFrame {
    String functionName;
    String sourceURL;
    uint32_t sourceID { 0 }; // A JSC::SourceID; the protocol reports it as a Debugger.ScriptId string.
    unsigned lineNumber { 0 };
    unsigned columnNumber { 0 };
};

// Plain, IPC-serializable mirror of Protocol::Console::StackTrace, including its async parent chain.
// The parent link is linear (each level has at most one parent), so the chain is a singly linked
// list terminated by a null parentStackTrace.
struct InitiatorStackTrace {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED_EXPORT(InitiatorStackTrace, WEBCORE_EXPORT);

    Vector<InitiatorCallFrame> callFrames;
    bool truncated { false };

    // Marks an async boundary frame. Set only on async parent levels, never on the synchronous top
    // level, mirroring AsyncStackTrace::buildInspectorObject.
    bool topCallFrameIsBoundary { false };

    std::unique_ptr<InitiatorStackTrace> parentStackTrace;
};

// Plain, IPC-serializable mirror of Protocol::Network::Initiator, describing what caused a load.
struct InitiatorData {
    InitiatorType type { InitiatorType::Other };

    // Set only for InitiatorType::Script.
    std::unique_ptr<InitiatorStackTrace> stackTrace;

    // Set only for InitiatorType::Parser.
    String parserURL;
    std::optional<int> parserLineNumber;

    // May accompany any type.
    std::optional<int> nodeId;

    // False when the load could not be attributed to anything, which lets a caller substitute its
    // own last-resort initiator before settling for a bare "other".
    bool isAttributed() const { return type != InitiatorType::Other || nodeId.has_value(); }
};

namespace ResourceUtilities {

WEBCORE_EXPORT bool sharedBufferContent(RefPtr<WebCore::FragmentedSharedBuffer>&&, const String& textEncodingName, bool withBase64Encode, String* result);
WEBCORE_EXPORT Vector<WebCore::CachedResource*> cachedResourcesForFrame(WebCore::LocalFrame*);
WEBCORE_EXPORT Ref<JSON::ArrayOf<Inspector::Protocol::Page::FrameResource>> buildResourceObjectsForFrame(WebCore::LocalFrame&);
WEBCORE_EXPORT Vector<Inspector::FrameResource> buildResourceDataForFrame(WebCore::LocalFrame&);
WEBCORE_EXPORT Ref<Inspector::Protocol::Page::FrameResource> buildResourceObject(const Inspector::FrameResource&);
void resourceContent(Inspector::Protocol::ErrorString&, WebCore::LocalFrame*, const URL&, String* result, bool* base64Encoded);
WEBCORE_EXPORT bool mainResourceContent(WebCore::LocalFrame*, bool withBase64Encode, String* result);

WEBCORE_EXPORT String sourceMapURLForResource(WebCore::CachedResource*);
RefPtr<WebCore::CachedResource> WEBCORE_EXPORT cachedResource(const WebCore::LocalFrame*, const URL&);
Inspector::ResourceType WEBCORE_EXPORT inspectorResourceType(WebCore::CachedResource::Type);
Inspector::ResourceType WEBCORE_EXPORT inspectorResourceType(const WebCore::CachedResource&);

Inspector::Protocol::Page::ResourceType NODELETE resourceTypeToProtocol(Inspector::ResourceType);
Inspector::Protocol::Page::ResourceType cachedResourceTypeToProtocol(const WebCore::CachedResource&);
WebCore::LocalFrame* findFrameWithSecurityOrigin(WebCore::Page&, const String& originRawString);
WebCore::DocumentLoader* assertDocumentLoader(Inspector::Protocol::ErrorString&, WebCore::LocalFrame*);

WEBCORE_EXPORT bool shouldTreatAsText(const String& mimeType);
WEBCORE_EXPORT Ref<WebCore::TextResourceDecoder> createTextDecoder(const String& mimeType, const String& textEncodingName);
WEBCORE_EXPORT std::optional<String> textContentForCachedResource(WebCore::CachedResource&);
WEBCORE_EXPORT bool cachedResourceContent(WebCore::CachedResource&, String* result, bool* base64Encoded);

WEBCORE_EXPORT Ref<Inspector::Protocol::Network::Headers> buildObjectForHeaders(const WebCore::HTTPHeaderMap&);

// Timebase-independent: every field is either a plain scalar or relative to the load itself.
WEBCORE_EXPORT Ref<Inspector::Protocol::Network::Metrics> buildObjectForMetrics(const WebCore::NetworkLoadMetrics&);

// ResourceTiming's first four fields are absolute protocol timestamps, so the caller supplies
// monotonicToProtocolSeconds to express them in whichever timebase its target reports on. The
// remaining fields are milliseconds relative to fetchStart and need no conversion.
WEBCORE_EXPORT Ref<Inspector::Protocol::Network::ResourceTiming> buildObjectForTiming(const WebCore::NetworkLoadMetrics&, MonotonicTime loadStartTime, NOESCAPE const Function<double(MonotonicTime)>& monotonicToProtocolSeconds);

// Captures what caused the load being started right now. Must be called synchronously from the
// load-initiating instrumentation hook, since it reads the live JS stack to decide whether a script
// is responsible.
//
// A node identifier is reported only when a page-target DOM agent is registered, because that agent
// mints the identifiers the frontend resolves. A process hosting only cross-origin iframes has none
// — those nodes belong to a per-target FrameDOMAgent — so reporting an id here could match it
// against an unrelated node in another target.
WEBCORE_EXPORT InitiatorData gatherInitiatorData(WebCore::Document*, const WebCore::ResourceRequest*, const WebCore::InstrumentingAgents&);

WEBCORE_EXPORT Ref<Inspector::Protocol::Network::Initiator> buildInitiatorObject(const InitiatorData&);

// Loads url in the given context on behalf of the inspector, bypassing cross-origin checks (Network.loadResource).
WEBCORE_EXPORT void loadResource(WebCore::ScriptExecutionContext&, const String& url, LoadResourceCompletionHandler&&);

} // namespace ResourceUtilities

} // namespace Inspector
