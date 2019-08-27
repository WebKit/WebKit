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

typedef String ErrorString;

class InspectorPageAgent final : public InspectorAgentBase, public Inspector::PageBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorPageAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorPageAgent(PageAgentContext&, InspectorClient*, InspectorOverlay*);
    virtual ~InspectorPageAgent();

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
        OtherResource,
    };

    static bool sharedBufferContent(RefPtr<SharedBuffer>&&, const String& textEncodingName, bool withBase64Encode, String* result);
    static Vector<CachedResource*> cachedResourcesForFrame(Frame*);
    static void resourceContent(ErrorString&, Frame*, const URL&, String* result, bool* base64Encoded);
    static String sourceMapURLForResource(CachedResource*);
    static CachedResource* cachedResource(Frame*, const URL&);
    static Inspector::Protocol::Page::ResourceType resourceTypeJSON(ResourceType);
    static ResourceType inspectorResourceType(CachedResource::Type);
    static ResourceType inspectorResourceType(const CachedResource&);
    static Inspector::Protocol::Page::ResourceType cachedResourceTypeJSON(const CachedResource&);
    static Frame* findFrameWithSecurityOrigin(Page&, const String& originRawString);
    static DocumentLoader* assertDocumentLoader(ErrorString&, Frame*);

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // PageBackendDispatcherHandler
    void enable(ErrorString&);
    void disable(ErrorString&);
    void reload(ErrorString&, const bool* optionalReloadFromOrigin, const bool* optionalRevalidateAllResources);
    void navigate(ErrorString&, const String& url);
    void overrideUserAgent(ErrorString&, const String* value);
    void overrideSetting(ErrorString&, const String& setting, const bool* value);
    void getCookies(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::Cookie>>& cookies);
    void deleteCookie(ErrorString&, const String& cookieName, const String& url);
    void getResourceTree(ErrorString&, RefPtr<Inspector::Protocol::Page::FrameResourceTree>&);
    void getResourceContent(ErrorString&, const String& frameId, const String& url, String* content, bool* base64Encoded);
    void searchInResource(ErrorString&, const String& frameId, const String& url, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, const String* optionalRequestId, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>&);
    void searchInResources(ErrorString&, const String&, const bool* caseSensitive, const bool* isRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>&);
    void setShowRulers(ErrorString&, bool);
    void setShowPaintRects(ErrorString&, bool show);
    void setEmulatedMedia(ErrorString&, const String&);
    void setForcedAppearance(ErrorString&, const String&);
    void getCompositingBordersVisible(ErrorString&, bool* out_param);
    void setCompositingBordersVisible(ErrorString&, bool);
    void snapshotNode(ErrorString&, int nodeId, String* outDataURL);
    void snapshotRect(ErrorString&, int x, int y, int width, int height, const String& coordinateSystem, String* outDataURL);
    void archive(ErrorString&, String* data);

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
    void applyUserAgentOverride(String&);
    void applyEmulatedMedia(String&);
    void didPaint(RenderObject&, const LayoutRect&);
    void didLayout();
    void didScroll();
    void didRecalculateStyle();

    Frame* frameForId(const String& frameId);
    WEBCORE_EXPORT String frameId(Frame*);
    String loaderId(DocumentLoader*);
    Frame* assertFrame(ErrorString&, const String& frameId);

private:
    double timestamp();

    static bool mainResourceContent(Frame*, bool withBase64Encode, String* result);
    static bool dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result);

    Ref<Inspector::Protocol::Page::Frame> buildObjectForFrame(Frame*);
    Ref<Inspector::Protocol::Page::FrameResourceTree> buildObjectForFrameTree(Frame*);

    std::unique_ptr<Inspector::PageFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::PageBackendDispatcher> m_backendDispatcher;

    Page& m_inspectedPage;
    InspectorClient* m_client { nullptr };
    InspectorOverlay* m_overlay { nullptr };

    HashMap<Frame*, String> m_frameToIdentifier;
    HashMap<String, Frame*> m_identifierToFrame;
    HashMap<DocumentLoader*, String> m_loaderToIdentifier;
    String m_userAgentOverride;
    String m_emulatedMedia;
    String m_forcedAppearance;
    bool m_isFirstLayoutAfterOnLoad { false };
    bool m_showPaintRects { false };
};

} // namespace WebCore
