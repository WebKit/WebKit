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
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DocumentLoader;
class Frame;
class InspectorClient;
class InspectorOverlay;
class Page;
class RenderObject;
class SharedBuffer;
class URL;

typedef String ErrorString;

class InspectorPageAgent final : public InspectorAgentBase, public Inspector::PageBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorPageAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorPageAgent(PageAgentContext&, InspectorClient*, InspectorOverlay*);

    enum ResourceType {
        DocumentResource,
        StylesheetResource,
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
        OtherResource,
    };

    static bool sharedBufferContent(RefPtr<SharedBuffer>&&, const String& textEncodingName, bool withBase64Encode, String* result);
    static void resourceContent(ErrorString&, Frame*, const URL&, String* result, bool* base64Encoded);
    static String sourceMapURLForResource(CachedResource*);

    static CachedResource* cachedResource(Frame*, const URL&);
    static Inspector::Protocol::Page::ResourceType resourceTypeJSON(ResourceType);
    static ResourceType inspectorResourceType(CachedResource::Type);
    static ResourceType inspectorResourceType(const CachedResource&);
    static Inspector::Protocol::Page::ResourceType cachedResourceTypeJSON(const CachedResource&);

    // Page API for InspectorFrontend
    void enable(ErrorString&) final;
    void disable(ErrorString&) final;
    void reload(ErrorString&, const bool* optionalReloadFromOrigin, const bool* optionalRevalidateAllResources) final;
    void navigate(ErrorString&, const String& url) final;
    void getCookies(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::Cookie>>& cookies) final;
    void deleteCookie(ErrorString&, const String& cookieName, const String& url) final;
    void getResourceTree(ErrorString&, RefPtr<Inspector::Protocol::Page::FrameResourceTree>&) final;
    void getResourceContent(ErrorString&, const String& frameId, const String& url, String* content, bool* base64Encoded) final;
    void searchInResource(ErrorString&, const String& frameId, const String& url, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, const String* optionalRequestId, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>&) final;
    void searchInResources(ErrorString&, const String&, const bool* caseSensitive, const bool* isRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>&) final;
    void setShowRulers(ErrorString&, bool) final;
    void setShowPaintRects(ErrorString&, bool show) final;
    void setEmulatedMedia(ErrorString&, const String&) final;
    void setForcedAppearance(ErrorString&, const String&) final;
    void getCompositingBordersVisible(ErrorString&, bool* out_param) final;
    void setCompositingBordersVisible(ErrorString&, bool) final;
    void snapshotNode(ErrorString&, int nodeId, String* outDataURL) final;
    void snapshotRect(ErrorString&, int x, int y, int width, int height, const String& coordinateSystem, String* outDataURL) final;
    void archive(ErrorString&, String* data) final;

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
    void defaultAppearanceDidChange(bool useDarkAppearance);
    void applyEmulatedMedia(String&);
    void didPaint(RenderObject&, const LayoutRect&);
    void didLayout();
    void didScroll();
    void didRecalculateStyle();

    // Inspector Controller API
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) final;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) final;

    // Cross-agents API
    Page& page() { return m_page; }
    Frame& mainFrame();
    Frame* frameForId(const String& frameId);
    WEBCORE_EXPORT String frameId(Frame*);
    bool hasIdForFrame(Frame*) const;
    String loaderId(DocumentLoader*);
    Frame* findFrameWithSecurityOrigin(const String& originRawString);
    Frame* assertFrame(ErrorString&, const String& frameId);
    static DocumentLoader* assertDocumentLoader(ErrorString&, Frame*);

private:
    double timestamp();

    static bool mainResourceContent(Frame*, bool withBase64Encode, String* result);
    static bool dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result);

    Ref<Inspector::Protocol::Page::Frame> buildObjectForFrame(Frame*);
    Ref<Inspector::Protocol::Page::FrameResourceTree> buildObjectForFrameTree(Frame*);

    std::unique_ptr<Inspector::PageFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::PageBackendDispatcher> m_backendDispatcher;

    Page& m_page;
    InspectorClient* m_client { nullptr };
    InspectorOverlay* m_overlay { nullptr };

    HashMap<Frame*, String> m_frameToIdentifier;
    HashMap<String, Frame*> m_identifierToFrame;
    HashMap<DocumentLoader*, String> m_loaderToIdentifier;
    bool m_enabled { false };
    bool m_isFirstLayoutAfterOnLoad { false };
    bool m_showPaintRects { false };
    String m_emulatedMedia;
    String m_forcedAppearance;
};

} // namespace WebCore
