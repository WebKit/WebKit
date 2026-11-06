/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "InspectorTargetProxy.h"
#include "ProvisionalPageProxy.h"
#include "UIProcess/WebFrameProxy.h"
#include <JavaScriptCore/InspectorAgentRegistry.h>
#include <JavaScriptCore/InspectorTargetAgent.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include <WebCore/InspectorOverlay.h>
#endif

namespace Inspector {
class BackendDispatcher;
class FrontendChannel;
class FrontendRouter;
class ProxyingNetworkAgent;
class ProxyingPageAgent;
}

namespace WebKit {

class BrowsingContextGroup;
class DebuggerPauseCoordinator;
class InspectorBrowserAgent;
class ProvisionalPageProxy;
enum class ProvisionalLoadIsReplaced : bool;
struct WebPageAgentContext;

class WebPageInspectorController {
    WTF_MAKE_TZONE_ALLOCATED(WebPageInspectorController);
    WTF_MAKE_NONCOPYABLE(WebPageInspectorController);
public:
    WebPageInspectorController(WebPageProxy&);
    ~WebPageInspectorController();

    void init();
    void pageClosed();

    bool hasLocalFrontend() const;
    bool hasFrontends() const;

    void frameDidPauseForDebugger(WebProcessProxy&, WebCore::FrameIdentifier);
    void frameDidContinueForDebugger(WebProcessProxy&, WebCore::FrameIdentifier);
    static void pageWasAddedToProcess(BrowsingContextGroup&, WebProcessProxy&, WebCore::PageIdentifier);
    void frameDidChangeProcess(WebCore::FrameIdentifier, WebCore::ProcessIdentifier newProcessID);
    void mainFrameDidStartProvisionalLoad();
    void mainFrameDidCommitLoad();
    void mainFrameProvisionalLoadDidEnd(ProvisionalLoadIsReplaced);
    bool isMainFrameNavigating() const { return m_isMainFrameNavigating; }
    void processDidTerminate(WebProcessProxy&);

    void connectFrontend(Inspector::FrontendChannel&, bool isAutomaticInspection = false, bool immediatelyPause = false);
    void disconnectFrontend(Inspector::FrontendChannel&);
    void disconnectAllFrontends();

    void dispatchMessageFromFrontend(const String& message);

#if ENABLE(REMOTE_INSPECTOR)
    void setIndicating(bool);
#endif

    void sendMessageToInspectorFrontend(const String& targetId, const String& message);

    bool shouldPauseLoadingForPage(const ProvisionalPageProxy&) const;
    void setContinueLoadingCallbackForPage(const ProvisionalPageProxy&, WTF::Function<void()>&&);
    bool shouldPauseLoadingForFrame(const ProvisionalFrameProxy&) const;
    void setContinueLoadingCallbackForFrame(const ProvisionalFrameProxy&, WTF::Function<void()>&&);

    void didCreateProvisionalPage(ProvisionalPageProxy&, WebCore::FrameIdentifier mainFrameID, WebProcessProxy& mainFrameProcess);
    void willDestroyProvisionalPage(const ProvisionalPageProxy&, WebCore::FrameIdentifier mainFrameID, WebCore::ProcessIdentifier mainFrameProcessID);
    void didCommitProvisionalPage(std::optional<WebCore::FrameIdentifier> oldMainFrameID, WebCore::ProcessIdentifier oldProcessID, WebCore::PageIdentifier oldWebPageID, WebCore::PageIdentifier newWebPageID);
    void didCreateFrame(WebFrameProxy&);
    void willDestroyFrame(const WebFrameProxy&);
    void didCreateProvisionalFrame(ProvisionalFrameProxy&);
    void willDestroyProvisionalFrame(const ProvisionalFrameProxy&);
    void didCommitProvisionalFrame(WebFrameProxy&, WebCore::ProcessIdentifier oldProcessID, std::optional<WebCore::PageIdentifier> oldPageID, WebCore::ProcessIdentifier newProcessID);

    NODELETE InspectorBrowserAgent* enabledBrowserAgent() const;
    void setEnabledBrowserAgent(InspectorBrowserAgent*);

    void browserExtensionsEnabled(HashMap<String, String>&&);
    void browserExtensionsDisabled(HashSet<String>&&);

    bool isNetworkInstrumentationEnabled() const;
    bool isPageInstrumentationEnabled() const;

    void setShowPaintRects(bool);

    void disconnectFrameTargetForTesting(WebCore::FrameIdentifier);

#if PLATFORM(MAC)
    using FrameHighlights = HashMap<WebCore::FrameIdentifier, std::pair<WebCore::ProcessIdentifier, WebCore::InspectorOverlayHighlight>>;
    void setFrameHighlight(WebCore::FrameIdentifier, WebCore::ProcessIdentifier, WebCore::InspectorOverlayHighlight&&);
    void clearFrameHighlight(WebCore::FrameIdentifier);
    const FrameHighlights& frameHighlights() const LIFETIME_BOUND { return m_frameHighlights; }
    WebCore::InspectorOverlay::PageGeometry frameHighlightPageGeometry() const;

    // The page-wide ruler toggle from Page.setShowRulers, forwarded by the main frame's process.
    void setShowRulers(bool);
    // Called when the inspector detaches from the main frame's process, before the new process's
    // WebInspectorBackendProxy receiver is registered, so no forwarded toggle can arrive out of order.
    void resetPageRulers();
    bool showRulers() const { return m_showRulers; }

    struct FrameHighlightDrawing {
        const WebCore::InspectorOverlayHighlight* highlight { nullptr };
        WebCore::InspectorOverlay::FrameHighlightGeometry geometry;
    };
    // Draws every frame highlight with its guide lines, then one set of ruler bars over the union of
    // their exclusions. Reports the bars on the first highlight that gets rulers.
    Vector<FrameHighlightDrawing> drawFrameHighlights(WebCore::GraphicsContext&, const WebCore::InspectorOverlay::PageGeometry&) const;
#endif

private:
    void browsingContextGroupPageWasAddedToProcess(WebProcessProxy&, WebCore::PageIdentifier);

    NODELETE WebPageAgentContext webPageAgentContext();
    void createLazyAgents();

    void addTarget(std::unique_ptr<InspectorTargetProxy>&&);
    void removeTarget(const String& targetId);

    bool shouldManageFrameTargets() const;

#if PLATFORM(MAC)
    void clearFrameHighlightNotFromProcess(WebCore::FrameIdentifier, WebCore::ProcessIdentifier);
    void clearAllFrameHighlights();
    void frameHighlightsDidChange();
    // Whether a frame highlight's nodes get rulers, guide lines and the viewport size label. Grid and
    // flex overlays alone have no node bounds to mark and never draw rulers here.
    bool frameHighlightShowRulers(const WebCore::InspectorOverlayHighlight&) const;
    // Whether this process draws ruler bars for a frame highlight, so the page's own bars stay off.
    bool pageRulersSuppressed() const;
    void updatePageRulersSuppression();
#endif

    const Ref<Inspector::FrontendRouter> m_frontendRouter;
    const Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    Inspector::AgentRegistry m_agents;

    WeakRef<WebPageProxy> m_inspectedPage;

    CheckedPtr<Inspector::InspectorTargetAgent> m_targetAgent;
    HashMap<String, std::unique_ptr<InspectorTargetProxy>> m_targets;

    CheckedPtr<InspectorBrowserAgent> m_enabledBrowserAgent;
    const RefPtr<Inspector::ProxyingNetworkAgent> m_networkAgent;
    const RefPtr<Inspector::ProxyingPageAgent> m_pageAgent;

    bool m_didCreateLazyAgents { false };
    std::unique_ptr<DebuggerPauseCoordinator> m_debuggerPauseCoordinator;
    bool m_isMainFrameNavigating { false };

#if PLATFORM(MAC)
    FrameHighlights m_frameHighlights;
    bool m_showRulers { false };
    // std::nullopt after a main frame process change, so the next update resends the current value.
    std::optional<bool> m_lastSentPageRulersSuppression;
#endif
};

} // namespace WebKit
