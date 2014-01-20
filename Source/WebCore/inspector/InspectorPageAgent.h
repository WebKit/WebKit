/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorPageAgent_h
#define InspectorPageAgent_h

#if ENABLE(INSPECTOR)

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include "IntSize.h"
#include "LayoutRect.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InspectorArray;
class InspectorObject;
}

namespace WebCore {

class CachedResource;
class DOMWrapperWorld;
class DocumentLoader;
class Frame;
class Frontend;
class GraphicsContext;
class InspectorClient;
class InspectorOverlay;
class InstrumentingAgents;
class URL;
class Page;
class RegularExpression;
class SharedBuffer;
class TextResourceDecoder;

typedef String ErrorString;

class InspectorPageAgent : public InspectorAgentBase, public Inspector::InspectorPageBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorPageAgent);
public:
    InspectorPageAgent(InstrumentingAgents*, Page*, InspectorClient*, InspectorOverlay*);

    enum ResourceType {
        DocumentResource,
        StylesheetResource,
        ImageResource,
        FontResource,
        ScriptResource,
        XHRResource,
        WebSocketResource,
        OtherResource
    };

    static bool cachedResourceContent(CachedResource*, String* result, bool* base64Encoded);
    static bool sharedBufferContent(PassRefPtr<SharedBuffer>, const String& textEncodingName, bool withBase64Encode, String* result);
    static void resourceContent(ErrorString*, Frame*, const URL&, String* result, bool* base64Encoded);
    static String sourceMapURLForResource(CachedResource*);

    static PassRefPtr<SharedBuffer> resourceData(Frame*, const URL&, String* textEncodingName);
    static CachedResource* cachedResource(Frame*, const URL&);
    static Inspector::TypeBuilder::Page::ResourceType::Enum resourceTypeJson(ResourceType);
    static ResourceType cachedResourceType(const CachedResource&);
    static Inspector::TypeBuilder::Page::ResourceType::Enum cachedResourceTypeJson(const CachedResource&);

    // Page API for InspectorFrontend
    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void addScriptToEvaluateOnLoad(ErrorString*, const String& source, String* result) override;
    virtual void removeScriptToEvaluateOnLoad(ErrorString*, const String& identifier) override;
    virtual void reload(ErrorString*, const bool* optionalIgnoreCache, const String* optionalScriptToEvaluateOnLoad) override;
    virtual void navigate(ErrorString*, const String& url) override;
    virtual void getCookies(ErrorString*, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Page::Cookie>>& cookies) override;
    virtual void deleteCookie(ErrorString*, const String& cookieName, const String& url) override;
    virtual void getResourceTree(ErrorString*, RefPtr<Inspector::TypeBuilder::Page::FrameResourceTree>&) override;
    virtual void getResourceContent(ErrorString*, const String& frameId, const String& url, String* content, bool* base64Encoded) override;
    virtual void searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::GenericTypes::SearchMatch>>&) override;
    virtual void searchInResources(ErrorString*, const String&, const bool* caseSensitive, const bool* isRegex, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Page::SearchResult>>&) override;
    virtual void setDocumentContent(ErrorString*, const String& frameId, const String& html) override;
    virtual void setShowPaintRects(ErrorString*, bool show) override;
    virtual void canShowDebugBorders(ErrorString*, bool*) override;
    virtual void setShowDebugBorders(ErrorString*, bool show) override;
    virtual void canShowFPSCounter(ErrorString*, bool*) override;
    virtual void setShowFPSCounter(ErrorString*, bool show) override;
    virtual void canContinuouslyPaint(ErrorString*, bool*) override;
    virtual void setContinuousPaintingEnabled(ErrorString*, bool enabled) override;
    virtual void getScriptExecutionStatus(ErrorString*, Inspector::InspectorPageBackendDispatcherHandler::Result::Enum*) override;
    virtual void setScriptExecutionDisabled(ErrorString*, bool) override;
    virtual void setTouchEmulationEnabled(ErrorString*, bool) override;
    virtual void setEmulatedMedia(ErrorString*, const String&) override;
    virtual void getCompositingBordersVisible(ErrorString*, bool* out_param) override;
    virtual void setCompositingBordersVisible(ErrorString*, bool) override;
    virtual void snapshotNode(ErrorString*, int nodeId, String* outDataURL) override;
    virtual void snapshotRect(ErrorString*, int x, int y, int width, int height, const String& coordinateSystem, String* outDataURL) override;
    virtual void handleJavaScriptDialog(ErrorString*, bool accept, const String* promptText) override;
    virtual void archive(ErrorString*, String* data) override;

    // InspectorInstrumentation API
    void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld&);
    void domContentEventFired();
    void loadEventFired();
    void frameNavigated(DocumentLoader*);
    void frameDetached(Frame*);
    void loaderDetachedFromFrame(DocumentLoader*);
    void frameStartedLoading(Frame&);
    void frameStoppedLoading(Frame&);
    void frameScheduledNavigation(Frame&, double delay);
    void frameClearedScheduledNavigation(Frame&);
    void willRunJavaScriptDialog(const String& message);
    void didRunJavaScriptDialog();
    void applyEmulatedMedia(String*);
    void didPaint(GraphicsContext*, const LayoutRect&);
    void didLayout();
    void didScroll();
    void didRecalculateStyle();
    void scriptsEnabled(bool isEnabled);

    // Inspector Controller API
    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend() override;

    // Cross-agents API
    Page* page() { return m_page; }
    Frame* mainFrame();
    String createIdentifier();
    Frame* frameForId(const String& frameId);
    String frameId(Frame*);
    bool hasIdForFrame(Frame*) const;
    String loaderId(DocumentLoader*);
    Frame* findFrameWithSecurityOrigin(const String& originRawString);
    Frame* assertFrame(ErrorString*, const String& frameId);
    static DocumentLoader* assertDocumentLoader(ErrorString*, Frame*);

private:
#if ENABLE(TOUCH_EVENTS)
    void updateTouchEventEmulationInPage(bool);
#endif

    static bool mainResourceContent(Frame*, bool withBase64Encode, String* result);
    static bool dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result);

    PassRefPtr<Inspector::TypeBuilder::Page::Frame> buildObjectForFrame(Frame*);
    PassRefPtr<Inspector::TypeBuilder::Page::FrameResourceTree> buildObjectForFrameTree(Frame*);
    Page* m_page;
    InspectorClient* m_client;
    std::unique_ptr<Inspector::InspectorPageFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorPageBackendDispatcher> m_backendDispatcher;
    InspectorOverlay* m_overlay;
    long m_lastScriptIdentifier;
    String m_pendingScriptToEvaluateOnLoadOnce;
    String m_scriptToEvaluateOnLoadOnce;
    HashMap<Frame*, String> m_frameToIdentifier;
    HashMap<String, Frame*> m_identifierToFrame;
    HashMap<DocumentLoader*, String> m_loaderToIdentifier;
    bool m_enabled;
    bool m_isFirstLayoutAfterOnLoad;
    bool m_originalScriptExecutionDisabled;
    bool m_ignoreScriptsEnabledNotification;
    bool m_showPaintRects;
    String m_emulatedMedia;
    RefPtr<Inspector::InspectorObject> m_scriptsToEvaluateOnLoad;
};


} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorPagerAgent_h)
