/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CachedResource.h"
#include "InspectorWebAgentBase.h"
#include "LayoutRect.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DOMWrapperWorld;
class DocumentLoader;
class Frame;
class InspectorClient;
class InspectorOverlay;
class Page;
class RenderObject;
class FragmentedSharedBuffer;

class InspectorPageAgent final : public InspectorAgentBase, public Inspector::PageBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorPageAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorPageAgent(PageAgentContext&, InspectorClient*, InspectorOverlay*);
    ~InspectorPageAgent();

    enum ResourceType {
        DocumentResource,
        StyleSheetResource,
        ImageResource,
        FontResource,
        ScriptResource,
        XHRResource,
        FetchResource,
        PingResource,
        BeaconResource,
        WebSocketResource,
#if ENABLE(APPLICATION_MANIFEST)
        ApplicationManifestResource,
#endif
        EventSourceResource,
        OtherResource,
    };

    static bool sharedBufferContent(RefPtr<FragmentedSharedBuffer>&&, const String& textEncodingName, bool withBase64Encode, String* result);
    static Vector<CachedResource*> cachedResourcesForFrame(Frame*);
    static void resourceContent(Inspector::Protocol::ErrorString&, Frame*, const URL&, String* result, bool* base64Encoded);
    static String sourceMapURLForResource(CachedResource*);
    static CachedResource* cachedResource(const Frame*, const URL&);
    static Inspector::Protocol::Page::ResourceType resourceTypeJSON(ResourceType);
    static ResourceType inspectorResourceType(CachedResource::Type);
    static ResourceType inspectorResourceType(const CachedResource&);
    static Inspector::Protocol::Page::ResourceType cachedResourceTypeJSON(const CachedResource&);
    static Frame* findFrameWithSecurityOrigin(Page&, const String& originRawString);
    static DocumentLoader* assertDocumentLoader(Inspector::Protocol::ErrorString&, Frame*);

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // PageBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<void> reload(std::optional<bool>&& ignoreCache, std::optional<bool>&& revalidateAllResources);
    Inspector::Protocol::ErrorStringOr<void> navigate(const String& url);
    Inspector::Protocol::ErrorStringOr<void> overrideUserAgent(const String&);
    Inspector::Protocol::ErrorStringOr<void> overrideSetting(Inspector::Protocol::Page::Setting, std::optional<bool>&& value);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Page::Cookie>>> getCookies();
    Inspector::Protocol::ErrorStringOr<void> setCookie(Ref<JSON::Object>&&);
    Inspector::Protocol::ErrorStringOr<void> deleteCookie(const String& cookieName, const String& url);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Page::FrameResourceTree>> getResourceTree();
    Inspector::Protocol::ErrorStringOr<std::tuple<String, bool /* base64Encoded */>> getResourceContent(const Inspector::Protocol::Network::FrameId&, const String& url);
    Inspector::Protocol::ErrorStringOr<void> setBootstrapScript(const String& source);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>> searchInResource(const Inspector::Protocol::Network::FrameId&, const String& url, const String& query, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex, const Inspector::Protocol::Network::RequestId&);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>> searchInResources(const String&, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex);
#if !PLATFORM(IOS_FAMILY)
    Inspector::Protocol::ErrorStringOr<void> setShowRulers(bool);
#endif
    Inspector::Protocol::ErrorStringOr<void> setShowPaintRects(bool);
    Inspector::Protocol::ErrorStringOr<void> setEmulatedMedia(const String&);
#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    Inspector::Protocol::ErrorStringOr<void> setForcedAppearance(std::optional<Inspector::Protocol::Page::Appearance>&&);
#endif
    Inspector::Protocol::ErrorStringOr<String> snapshotNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<String> snapshotRect(int x, int y, int width, int height, Inspector::Protocol::Page::CoordinateSystem);
#if ENABLE(WEB_ARCHIVE) && USE(CF)
    Inspector::Protocol::ErrorStringOr<String> archive();
#endif
#if !PLATFORM(COCOA)
    Inspector::Protocol::ErrorStringOr<void> setScreenSizeOverride(std::optional<int>&& width, std::optional<int>&& height);
#endif

    // InspectorInstrumentation
    void domContentEventFired();
    void loadEventFired();
    void frameNavigated(Frame&);
    void frameDetached(Frame&);
    void loaderDetachedFromFrame(DocumentLoader&);
    void frameStartedLoading(Frame&);
    void frameStoppedLoading(Frame&);
    void frameScheduledNavigation(Frame&, Seconds delay);
    void frameClearedScheduledNavigation(Frame&);
#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    void defaultAppearanceDidChange(bool useDarkAppearance);
#endif
    void applyUserAgentOverride(String&);
    void applyEmulatedMedia(AtomString&);
    void didClearWindowObjectInWorld(Frame&, DOMWrapperWorld&);
    void didPaint(RenderObject&, const LayoutRect&);
    void didLayout();
    void didScroll();
    void didRecalculateStyle();

    Frame* frameForId(const Inspector::Protocol::Network::FrameId&);
    WEBCORE_EXPORT String frameId(Frame*);
    String loaderId(DocumentLoader*);
    Frame* assertFrame(Inspector::Protocol::ErrorString&, const Inspector::Protocol::Network::FrameId&);

private:
    double timestamp();

    static bool mainResourceContent(Frame*, bool withBase64Encode, String* result);
    static bool dataContent(const uint8_t* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result);

    Ref<Inspector::Protocol::Page::Frame> buildObjectForFrame(Frame*);
    Ref<Inspector::Protocol::Page::FrameResourceTree> buildObjectForFrameTree(Frame*);

    std::unique_ptr<Inspector::PageFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::PageBackendDispatcher> m_backendDispatcher;

    Page& m_inspectedPage;
    InspectorClient* m_client { nullptr };
    InspectorOverlay* m_overlay { nullptr };

    // FIXME: Make a WeakHashMap and use it for m_frameToIdentifier and m_loaderToIdentifier.
    HashMap<Frame*, String> m_frameToIdentifier;
    MemoryCompactRobinHoodHashMap<String, WeakPtr<Frame>> m_identifierToFrame;
    HashMap<DocumentLoader*, String> m_loaderToIdentifier;
    String m_userAgentOverride;
    AtomString m_emulatedMedia;
    String m_bootstrapScript;
    bool m_isFirstLayoutAfterOnLoad { false };
    bool m_showPaintRects { false };
};

} // namespace WebCore
