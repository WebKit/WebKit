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
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DocumentLoader;
class Frame;
class InspectorClient;
class InspectorOverlay;
class MainFrame;
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
    void enable(ErrorString&) override;
    void disable(ErrorString&) override;
    void reload(ErrorString&, const bool* const optionalReloadFromOrigin, const bool* const optionalRevalidateAllResources) override;
    void navigate(ErrorString&, const String& url) override;
    void getCookies(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::Cookie>>& cookies) override;
    void deleteCookie(ErrorString&, const String& cookieName, const String& url) override;
    void getResourceTree(ErrorString&, RefPtr<Inspector::Protocol::Page::FrameResourceTree>&) override;
    void getResourceContent(ErrorString&, const String& frameId, const String& url, String* content, bool* base64Encoded) override;
    void searchInResource(ErrorString&, const String& frameId, const String& url, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, const String* const optionalRequestId, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>&) override;
    void searchInResources(ErrorString&, const String&, const bool* const caseSensitive, const bool* const isRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>&) override;
    void setShowPaintRects(ErrorString&, bool show) override;
    void setEmulatedMedia(ErrorString&, const String&) override;
    void getCompositingBordersVisible(ErrorString&, bool* out_param) override;
    void setCompositingBordersVisible(ErrorString&, bool) override;
    void snapshotNode(ErrorString&, int nodeId, String* outDataURL) override;
    void snapshotRect(ErrorString&, int x, int y, int width, int height, const String& coordinateSystem, String* outDataURL) override;
    void archive(ErrorString&, String* data) override;

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
    void applyEmulatedMedia(String&);
    void didPaint(RenderObject&, const LayoutRect&);
    void didLayout();
    void didScroll();
    void didRecalculateStyle();

    // Inspector Controller API
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // Cross-agents API
    Page& page() { return m_page; }
    MainFrame& mainFrame();
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
};

} // namespace WebCore
