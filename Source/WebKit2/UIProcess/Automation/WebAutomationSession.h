/*
 * Copyright (C) 2016, 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "APIObject.h"
#include "AutomationBackendDispatchers.h"
#include "Connection.h"
#include "ShareableBitmap.h"
#include "WebEvent.h"
#include <wtf/Forward.h>

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteAutomationTarget.h>
#endif

namespace API {
class AutomationSessionClient;
}

namespace Inspector {
class BackendDispatcher;
class FrontendRouter;
}

namespace WebCore {
class IntPoint;
class IntRect;

struct Cookie;
}

#if USE(APPKIT)
OBJC_CLASS NSArray;
OBJC_CLASS NSEvent;
#endif

namespace WebKit {

class WebAutomationSessionClient;
class WebFrameProxy;
class WebPageProxy;
class WebProcessPool;

class WebAutomationSession final : public API::ObjectImpl<API::Object::Type::AutomationSession>, public IPC::MessageReceiver
#if ENABLE(REMOTE_INSPECTOR)
    , public Inspector::RemoteAutomationTarget
#endif
    , public Inspector::AutomationBackendDispatcherHandler
{
public:
    WebAutomationSession();
    ~WebAutomationSession();

    void setClient(std::unique_ptr<API::AutomationSessionClient>);

    void setSessionIdentifier(const String& sessionIdentifier) { m_sessionIdentifier = sessionIdentifier; }
    String sessionIdentifier() const { return m_sessionIdentifier; }

    WebProcessPool* processPool() const { return m_processPool; }
    void setProcessPool(WebProcessPool*);

    void navigationOccurredForPage(const WebPageProxy&);
    void inspectorFrontendLoaded(const WebPageProxy&);
    void keyboardEventsFlushedForPage(const WebPageProxy&);

#if ENABLE(REMOTE_INSPECTOR)
    // Inspector::RemoteAutomationTarget API
    String name() const override { return m_sessionIdentifier; }
    void dispatchMessageFromRemote(const String& message) override;
    void connect(Inspector::FrontendChannel*, bool isAutomaticConnection = false) override;
    void disconnect(Inspector::FrontendChannel*) override;
#endif
    void terminate();

    // Inspector::AutomationBackendDispatcherHandler API
    // NOTE: the set of declarations included in this interface depend on the "platform" property in Automation.json
    // and the --platform argument passed to the protocol bindings generator.

    // Platform: Generic
    void getBrowsingContexts(Inspector::ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Automation::BrowsingContext>>&) override;
    void getBrowsingContext(Inspector::ErrorString&, const String&, RefPtr<Inspector::Protocol::Automation::BrowsingContext>&) override;
    void createBrowsingContext(Inspector::ErrorString&, String*) override;
    void closeBrowsingContext(Inspector::ErrorString&, const String&) override;
    void switchToBrowsingContext(Inspector::ErrorString&, const String& browsingContextHandle, const String* optionalFrameHandle) override;
    void navigateBrowsingContext(Inspector::ErrorString&, const String& handle, const String& url, Ref<NavigateBrowsingContextCallback>&&) override;
    void goBackInBrowsingContext(Inspector::ErrorString&, const String&, Ref<GoBackInBrowsingContextCallback>&&) override;
    void goForwardInBrowsingContext(Inspector::ErrorString&, const String&, Ref<GoForwardInBrowsingContextCallback>&&) override;
    void reloadBrowsingContext(Inspector::ErrorString&, const String&, Ref<ReloadBrowsingContextCallback>&&) override;
    void evaluateJavaScriptFunction(Inspector::ErrorString&, const String& browsingContextHandle, const String* optionalFrameHandle, const String& function, const Inspector::InspectorArray& arguments, const bool* optionalExpectsImplicitCallbackArgument, const int* optionalCallbackTimeout, Ref<Inspector::AutomationBackendDispatcherHandler::EvaluateJavaScriptFunctionCallback>&&) override;
    void performMouseInteraction(Inspector::ErrorString&, const String& handle, const Inspector::InspectorObject& requestedPosition, const String& mouseButton, const String& mouseInteraction, const Inspector::InspectorArray& keyModifiers, RefPtr<Inspector::Protocol::Automation::Point>& updatedPosition) override;
    void performKeyboardInteractions(Inspector::ErrorString&, const String& handle, const Inspector::InspectorArray& interactions, Ref<PerformKeyboardInteractionsCallback>&&) override;
    void takeScreenshot(Inspector::ErrorString&, const String& handle, Ref<Inspector::AutomationBackendDispatcherHandler::TakeScreenshotCallback>&&) override;
    void resolveChildFrameHandle(Inspector::ErrorString&, const String& browsingContextHandle, const String* optionalFrameHandle, const int* optionalOrdinal, const String* optionalName, const String* optionalNodeHandle, Ref<ResolveChildFrameHandleCallback>&&) override;
    void resolveParentFrameHandle(Inspector::ErrorString&, const String& browsingContextHandle, const String& frameHandle, Ref<ResolveParentFrameHandleCallback>&&) override;
    void computeElementLayout(Inspector::ErrorString&, const String& browsingContextHandle, const String& frameHandle, const String& nodeHandle, const bool* optionalScrollIntoViewIfNeeded, const bool* useViewportCoordinates, Ref<Inspector::AutomationBackendDispatcherHandler::ComputeElementLayoutCallback>&&) override;
    void isShowingJavaScriptDialog(Inspector::ErrorString&, const String& browsingContextHandle, bool* result) override;
    void dismissCurrentJavaScriptDialog(Inspector::ErrorString&, const String& browsingContextHandle) override;
    void acceptCurrentJavaScriptDialog(Inspector::ErrorString&, const String& browsingContextHandle) override;
    void messageOfCurrentJavaScriptDialog(Inspector::ErrorString&, const String& browsingContextHandle, String* text) override;
    void setUserInputForCurrentJavaScriptPrompt(Inspector::ErrorString&, const String& browsingContextHandle, const String& text) override;
    void getAllCookies(Inspector::ErrorString&, const String& browsingContextHandle, Ref<GetAllCookiesCallback>&&) override;
    void deleteSingleCookie(Inspector::ErrorString&, const String& browsingContextHandle, const String& cookieName, Ref<DeleteSingleCookieCallback>&&) override;
    void addSingleCookie(Inspector::ErrorString&, const String& browsingContextHandle, const Inspector::InspectorObject& cookie, Ref<AddSingleCookieCallback>&&) override;
    void deleteAllCookies(Inspector::ErrorString&, const String& browsingContextHandle) override;

    // Platform: macOS
#if PLATFORM(MAC)
    void resizeWindowOfBrowsingContext(Inspector::ErrorString&, const String& handle, const Inspector::InspectorObject& size) override;
    void moveWindowOfBrowsingContext(Inspector::ErrorString&, const String& handle, const Inspector::InspectorObject& position) override;
    void inspectBrowsingContext(Inspector::ErrorString&, const String&, const bool* optionalEnableAutoCapturing, Ref<InspectBrowsingContextCallback>&&) override;
#endif

    // Event Simulation Support.
#if PLATFORM(MAC)
    bool wasEventSynthesizedForAutomation(NSEvent *);
    void markEventAsSynthesizedForAutomation(NSEvent *);
#endif

private:
    WebPageProxy* webPageProxyForHandle(const String&);
    String handleForWebPageProxy(const WebPageProxy&);
    RefPtr<Inspector::Protocol::Automation::BrowsingContext> buildBrowsingContextForPage(WebPageProxy&);

    std::optional<uint64_t> webFrameIDForHandle(const String&);
    String handleForWebFrameID(uint64_t frameID);
    String handleForWebFrameProxy(const WebFrameProxy&);

    // Implemented in generated WebAutomationSessionMessageReceiver.cpp.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by WebAutomationSession messages.
    void didEvaluateJavaScriptFunction(uint64_t callbackID, const String& result, const String& errorType);
    void didResolveChildFrame(uint64_t callbackID, uint64_t frameID, const String& errorType);
    void didResolveParentFrame(uint64_t callbackID, uint64_t frameID, const String& errorType);
    void didComputeElementLayout(uint64_t callbackID, WebCore::IntRect, const String& errorType);
    void didTakeScreenshot(uint64_t callbackID, const ShareableBitmap::Handle&, const String& errorType);
    void didGetCookiesForFrame(uint64_t callbackID, Vector<WebCore::Cookie>, const String& errorType);
    void didDeleteCookie(uint64_t callbackID, const String& errorType);

    // Platform-dependent implementations.
    void platformSimulateMouseInteraction(WebPageProxy&, const WebCore::IntPoint& viewPosition, Inspector::Protocol::Automation::MouseInteraction, Inspector::Protocol::Automation::MouseButton, WebEvent::Modifiers);
    // Simulates a single virtual key being pressed, such as Control, F-keys, Numpad keys, etc. as allowed by the protocol.
    void platformSimulateKeyStroke(WebPageProxy&, Inspector::Protocol::Automation::KeyboardInteractionType, Inspector::Protocol::Automation::VirtualKey);
    // Simulates key presses to produce the codepoints in a string. One or more code points are delivered atomically at grapheme cluster boundaries.
    void platformSimulateKeySequence(WebPageProxy&, const String&);
    // Get base64 encoded PNG data from a bitmap.
    std::optional<String> platformGetBase64EncodedPNGData(const ShareableBitmap::Handle&);

#if PLATFORM(MAC)
    void sendSynthesizedEventsToPage(WebPageProxy&, NSArray *eventsToSend);
#endif

    WebProcessPool* m_processPool { nullptr };

    std::unique_ptr<API::AutomationSessionClient> m_client;
    String m_sessionIdentifier { ASCIILiteral("Untitled Session") };
    Ref<Inspector::FrontendRouter> m_frontendRouter;
    Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    Ref<Inspector::AutomationBackendDispatcher> m_domainDispatcher;

    HashMap<uint64_t, String> m_webPageHandleMap;
    HashMap<String, uint64_t> m_handleWebPageMap;
    String m_activeBrowsingContextHandle;

    HashMap<uint64_t, String> m_webFrameHandleMap;
    HashMap<String, uint64_t> m_handleWebFrameMap;

    HashMap<uint64_t, RefPtr<Inspector::BackendDispatcher::CallbackBase>> m_pendingNavigationInBrowsingContextCallbacksPerPage;
    HashMap<uint64_t, RefPtr<Inspector::BackendDispatcher::CallbackBase>> m_pendingInspectorCallbacksPerPage;
    HashMap<uint64_t, RefPtr<Inspector::BackendDispatcher::CallbackBase>> m_pendingKeyboardEventsFlushedCallbacksPerPage;

    uint64_t m_nextEvaluateJavaScriptCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::EvaluateJavaScriptFunctionCallback>> m_evaluateJavaScriptFunctionCallbacks;

    uint64_t m_nextResolveFrameCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::ResolveChildFrameHandleCallback>> m_resolveChildFrameHandleCallbacks;

    uint64_t m_nextResolveParentFrameCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::ResolveParentFrameHandleCallback>> m_resolveParentFrameHandleCallbacks;

    uint64_t m_nextComputeElementLayoutCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::ComputeElementLayoutCallback>> m_computeElementLayoutCallbacks;

    uint64_t m_nextScreenshotCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::TakeScreenshotCallback>> m_screenshotCallbacks;

    uint64_t m_nextGetCookiesCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::GetAllCookiesCallback>> m_getCookieCallbacks;

    uint64_t m_nextDeleteCookieCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::DeleteSingleCookieCallback>> m_deleteCookieCallbacks;

#if ENABLE(REMOTE_INSPECTOR)
    Inspector::FrontendChannel* m_remoteChannel { nullptr };
#endif
};

} // namespace WebKit
