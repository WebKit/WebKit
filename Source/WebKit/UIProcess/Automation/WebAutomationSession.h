/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "AutomationFrontendDispatchers.h"
#include "Connection.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "ShareableBitmap.h"
#include "SimulatedInputDispatcher.h"
#include "WebEvent.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>

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

#if PLATFORM(COCOA)
OBJC_CLASS NSArray;
typedef unsigned short unichar;
#endif

#if USE(APPKIT)
OBJC_CLASS NSEvent;
#endif

namespace API {
class OpenPanelParameters;
}

namespace WebKit {

class ViewSnapshot;
class WebAutomationSessionClient;
class WebFrameProxy;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
class WebProcessPool;

class AutomationCommandError {
public:
    Inspector::Protocol::Automation::ErrorMessage type;
    Optional<String> message { WTF::nullopt };
    
    AutomationCommandError(Inspector::Protocol::Automation::ErrorMessage type)
        : type(type) { }

    AutomationCommandError(Inspector::Protocol::Automation::ErrorMessage type, const String& message)
        : type(type)
        , message(message) { }
    
    String toProtocolString();
};

using AutomationCompletionHandler = WTF::CompletionHandler<void(Optional<AutomationCommandError>)>;

class WebAutomationSession final : public API::ObjectImpl<API::Object::Type::AutomationSession>, public IPC::MessageReceiver
#if ENABLE(REMOTE_INSPECTOR)
    , public Inspector::RemoteAutomationTarget
#endif
    , public Inspector::AutomationBackendDispatcherHandler
#if ENABLE(WEBDRIVER_ACTIONS_API)
    , public SimulatedInputDispatcher::Client
#endif
{
public:
    WebAutomationSession();
    ~WebAutomationSession();

    void setClient(std::unique_ptr<API::AutomationSessionClient>&&);

    void setSessionIdentifier(const String& sessionIdentifier) { m_sessionIdentifier = sessionIdentifier; }
    String sessionIdentifier() const { return m_sessionIdentifier; }

    WebProcessPool* processPool() const { return m_processPool; }
    void setProcessPool(WebProcessPool*);

    void navigationOccurredForFrame(const WebFrameProxy&);
    void documentLoadedForFrame(const WebFrameProxy&);
    void inspectorFrontendLoaded(const WebPageProxy&);
    void keyboardEventsFlushedForPage(const WebPageProxy&);
    void mouseEventsFlushedForPage(const WebPageProxy&);
    void willClosePage(const WebPageProxy&);
    void handleRunOpenPanel(const WebPageProxy&, const WebFrameProxy&, const API::OpenPanelParameters&, WebOpenPanelResultListenerProxy&);
    void willShowJavaScriptDialog(WebPageProxy&);
    void didEnterFullScreenForPage(const WebPageProxy&);
    void didExitFullScreenForPage(const WebPageProxy&);

    bool shouldAllowGetUserMediaForPage(const WebPageProxy&) const;

#if ENABLE(REMOTE_INSPECTOR)
    // Inspector::RemoteAutomationTarget API
    String name() const { return m_sessionIdentifier; }
    void dispatchMessageFromRemote(const String& message);
    void connect(Inspector::FrontendChannel&, bool isAutomaticConnection = false, bool immediatelyPause = false);
    void disconnect(Inspector::FrontendChannel&);
#endif
    void terminate();

#if ENABLE(WEBDRIVER_ACTIONS_API)

    // SimulatedInputDispatcher::Client API
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    void simulateMouseInteraction(WebPageProxy&, MouseInteraction, MouseButton, const WebCore::IntPoint& locationInView, AutomationCompletionHandler&&);
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    void simulateTouchInteraction(WebPageProxy&, TouchInteraction, const WebCore::IntPoint& locationInView, Optional<Seconds> duration, AutomationCompletionHandler&&);
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    void simulateKeyboardInteraction(WebPageProxy&, KeyboardInteraction, WTF::Variant<VirtualKey, CharKey>&&, AutomationCompletionHandler&&);
#endif
    void viewportInViewCenterPointOfElement(WebPageProxy&, Optional<WebCore::FrameIdentifier>, const Inspector::Protocol::Automation::NodeHandle&, Function<void(Optional<WebCore::IntPoint>, Optional<AutomationCommandError>)>&&);

#endif // ENABLE(WEBDRIVER_ACTIONS_API)

    // Inspector::AutomationBackendDispatcherHandler API
    // NOTE: the set of declarations included in this interface depend on the "platform" property in Automation.json
    // and the --platform argument passed to the protocol bindings generator.

    // Platform: Generic
    void getBrowsingContexts(Ref<GetBrowsingContextsCallback>&&);
    void getBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<GetBrowsingContextCallback>&&);
    void createBrowsingContext(Optional<Inspector::Protocol::Automation::BrowsingContextPresentation>&&, Ref<CreateBrowsingContextCallback>&&);
    Inspector::Protocol::ErrorStringOr<void> closeBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&);
    void switchToBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<SwitchToBrowsingContextCallback>&&);
    void setWindowFrameOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, RefPtr<JSON::Object>&& origin, RefPtr<JSON::Object>&& size, Ref<SetWindowFrameOfBrowsingContextCallback>&&);
    void maximizeWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<MaximizeWindowOfBrowsingContextCallback>&&);
    void hideWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<HideWindowOfBrowsingContextCallback>&&);
    void navigateBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& url, Optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, Optional<double>&& pageLoadTimeout, Ref<NavigateBrowsingContextCallback>&&);
    void goBackInBrowsingContext(const String&, Optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, Optional<double>&& pageLoadTimeout, Ref<GoBackInBrowsingContextCallback>&&);
    void goForwardInBrowsingContext(const String&, Optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, Optional<double>&& pageLoadTimeout, Ref<GoForwardInBrowsingContextCallback>&&);
    void reloadBrowsingContext(const String&, Optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, Optional<double>&& pageLoadTimeout, Ref<ReloadBrowsingContextCallback>&&);
    void waitForNavigationToComplete(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, Optional<double>&& pageLoadTimeout, Ref<WaitForNavigationToCompleteCallback>&&);
    void evaluateJavaScriptFunction(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const String& function, Ref<JSON::Array>&& arguments, Optional<bool>&& expectsImplicitCallbackArgument, Optional<double>&& callbackTimeout, Ref<EvaluateJavaScriptFunctionCallback>&&);
    void performMouseInteraction(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Object>&& requestedPosition, Inspector::Protocol::Automation::MouseButton, Inspector::Protocol::Automation::MouseInteraction, Ref<JSON::Array>&& keyModifiers, Ref<PerformMouseInteractionCallback>&&);
    void performKeyboardInteractions(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Array>&& interactions, Ref<PerformKeyboardInteractionsCallback>&&);
    void performInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<JSON::Array>&& sources, Ref<JSON::Array>&& steps, Ref<PerformInteractionSequenceCallback>&&);
    void cancelInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<CancelInteractionSequenceCallback>&&);
    void takeScreenshot(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Optional<bool>&& scrollIntoViewIfNeeded, Optional<bool>&& clipToViewport, Ref<TakeScreenshotCallback>&&);
    void resolveChildFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Optional<int>&& ordinal, const String& name, const Inspector::Protocol::Automation::NodeHandle&, Ref<ResolveChildFrameHandleCallback>&&);
    void resolveParentFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<ResolveParentFrameHandleCallback>&&);
    void computeElementLayout(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Optional<bool>&& scrollIntoViewIfNeeded, Inspector::Protocol::Automation::CoordinateSystem, Ref<ComputeElementLayoutCallback>&&);
    void selectOptionElement(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Ref<SelectOptionElementCallback>&&);
    Inspector::Protocol::ErrorStringOr<bool> isShowingJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&);
    Inspector::Protocol::ErrorStringOr<void> dismissCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&);
    Inspector::Protocol::ErrorStringOr<void> acceptCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&);
    Inspector::Protocol::ErrorStringOr<String> messageOfCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&);
    Inspector::Protocol::ErrorStringOr<void> setUserInputForCurrentJavaScriptPrompt(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& text);
    Inspector::Protocol::ErrorStringOr<void> setFilesToSelectForFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Array>&& filenames, RefPtr<JSON::Array>&& fileContents);
    void setFilesForInputFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Ref<JSON::Array>&& filenames, Ref<SetFilesForInputFileUploadCallback>&&);
    void getAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<GetAllCookiesCallback>&&);
    void deleteSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& cookieName, Ref<DeleteSingleCookieCallback>&&);
    void addSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Object>&& cookie, Ref<AddSingleCookieCallback>&&);
    Inspector::Protocol::ErrorStringOr<void> deleteAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle&);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>>> getSessionPermissions();
    Inspector::Protocol::ErrorStringOr<void> setSessionPermissions(Ref<JSON::Array>&&);

#if PLATFORM(MAC)
    void inspectBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Optional<bool>&& enableAutoCapturing, Ref<InspectBrowsingContextCallback>&&);
#endif

    // Event Simulation Support.
    bool isSimulatingUserInteraction() const;
#if ENABLE(WEBDRIVER_ACTIONS_API)
    SimulatedInputDispatcher& inputDispatcherForPage(WebPageProxy&);
    SimulatedInputSource* inputSourceForType(SimulatedInputSourceType) const;
#endif

#if PLATFORM(MAC)
    bool wasEventSynthesizedForAutomation(NSEvent *);
    void markEventAsSynthesizedForAutomation(NSEvent *);
#endif

private:
    WebPageProxy* webPageProxyForHandle(const String&);
    String handleForWebPageProxy(const WebPageProxy&);
    Ref<Inspector::Protocol::Automation::BrowsingContext> buildBrowsingContextForPage(WebPageProxy&, WebCore::FloatRect windowFrame);
    void getNextContext(Ref<WebAutomationSession>&&, Vector<Ref<WebPageProxy>>&&, Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>, Ref<WebAutomationSession::GetBrowsingContextsCallback>&&);

    Optional<WebCore::FrameIdentifier> webFrameIDForHandle(const String&, bool& frameNotFound);
    String handleForWebFrameID(Optional<WebCore::FrameIdentifier>);
    String handleForWebFrameProxy(const WebFrameProxy&);

    void waitForNavigationToCompleteOnPage(WebPageProxy&, Inspector::Protocol::Automation::PageLoadStrategy, Seconds, Ref<Inspector::BackendDispatcher::CallbackBase>&&);
    void waitForNavigationToCompleteOnFrame(WebFrameProxy&, Inspector::Protocol::Automation::PageLoadStrategy, Seconds, Ref<Inspector::BackendDispatcher::CallbackBase>&&);
    void respondToPendingPageNavigationCallbacksWithTimeout(HashMap<WebPageProxyIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>>&);
    void respondToPendingFrameNavigationCallbacksWithTimeout(HashMap<WebCore::FrameIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>>&);
    void loadTimerFired();

    void exitFullscreenWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void restoreWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void maximizeWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void hideWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);

    // IPC::MessageReceiver (Implemented by generated code in WebAutomationSessionMessageReceiver.cpp).
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    // Called by WebAutomationSession messages.
    void didEvaluateJavaScriptFunction(uint64_t callbackID, const String& result, const String& errorType);
    void didTakeScreenshot(uint64_t callbackID, const ShareableBitmap::Handle&, const String& errorType);

    // Platform-dependent implementations.
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    void updateClickCount(MouseButton, const WebCore::IntPoint&, Seconds maxTime, int maxDistance);
    void resetClickCount();
    void platformSimulateMouseInteraction(WebPageProxy&, MouseInteraction, MouseButton, const WebCore::IntPoint& locationInViewport, OptionSet<WebEvent::Modifier>);
    static OptionSet<WebEvent::Modifier> platformWebModifiersFromRaw(unsigned modifiers);
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    // Simulates a single touch point being pressed, moved, and released.
    void platformSimulateTouchInteraction(WebPageProxy&, TouchInteraction, const WebCore::IntPoint& locationInViewport, Optional<Seconds> duration, AutomationCompletionHandler&&);
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    // Simulates a single virtual or char key being pressed/released, such as 'a', Control, F-keys, Numpad keys, etc. as allowed by the protocol.
    void platformSimulateKeyboardInteraction(WebPageProxy&, KeyboardInteraction, WTF::Variant<VirtualKey, CharKey>&&);
    // Simulates key presses to produce the codepoints in a string. One or more code points are delivered atomically at grapheme cluster boundaries.
    void platformSimulateKeySequence(WebPageProxy&, const String&);
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

    // Get base64-encoded PNG data from a bitmap.
    static Optional<String> platformGetBase64EncodedPNGData(const ShareableBitmap::Handle&);
    static Optional<String> platformGetBase64EncodedPNGData(const ViewSnapshot&);

    // Save base64-encoded file contents to a local file path and return the path.
    // This reuses the basename of the remote file path so that the filename exposed to DOM API remains the same.
    Optional<String> platformGenerateLocalFilePathForRemoteFile(const String& remoteFilePath, const String& base64EncodedFileContents);

#if PLATFORM(COCOA)
    // The type parameter of the NSArray argument is platform-dependent.
    void sendSynthesizedEventsToPage(WebPageProxy&, NSArray *eventsToSend);

    Optional<unichar> charCodeForVirtualKey(Inspector::Protocol::Automation::VirtualKey) const;
    Optional<unichar> charCodeIgnoringModifiersForVirtualKey(Inspector::Protocol::Automation::VirtualKey) const;
#endif

    WebProcessPool* m_processPool { nullptr };

    std::unique_ptr<API::AutomationSessionClient> m_client;
    String m_sessionIdentifier { "Untitled Session"_s };
    Ref<Inspector::FrontendRouter> m_frontendRouter;
    Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    Ref<Inspector::AutomationBackendDispatcher> m_domainDispatcher;
    std::unique_ptr<Inspector::AutomationFrontendDispatcher> m_domainNotifier;

    HashMap<WebPageProxyIdentifier, String> m_webPageHandleMap;
    HashMap<String, WebPageProxyIdentifier> m_handleWebPageMap;

    HashMap<WebCore::FrameIdentifier, String> m_webFrameHandleMap;
    HashMap<String, WebCore::FrameIdentifier> m_handleWebFrameMap;

    HashMap<WebPageProxyIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>> m_pendingNormalNavigationInBrowsingContextCallbacksPerPage;
    HashMap<WebPageProxyIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>> m_pendingEagerNavigationInBrowsingContextCallbacksPerPage;
    HashMap<WebCore::FrameIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>> m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame;
    HashMap<WebCore::FrameIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>> m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame;
    HashMap<WebPageProxyIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>> m_pendingInspectorCallbacksPerPage;
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    HashMap<WebPageProxyIdentifier, Function<void(Optional<AutomationCommandError>)>> m_pendingKeyboardEventsFlushedCallbacksPerPage;
#endif
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    HashMap<WebPageProxyIdentifier, Function<void(Optional<AutomationCommandError>)>> m_pendingMouseEventsFlushedCallbacksPerPage;
#endif

    uint64_t m_nextEvaluateJavaScriptCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::EvaluateJavaScriptFunctionCallback>> m_evaluateJavaScriptFunctionCallbacks;

    uint64_t m_nextScreenshotCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::TakeScreenshotCallback>> m_screenshotCallbacks;

    enum class WindowTransitionedToState {
        Fullscreen,
        Unfullscreen,
    };
    Function<void(WindowTransitionedToState)> m_windowStateTransitionCallback { };

    RunLoop::Timer<WebAutomationSession> m_loadTimer;
    Vector<String> m_filesToSelectForFileUpload;

    bool m_permissionForGetUserMedia { true };

#if ENABLE(WEBDRIVER_ACTIONS_API)
    // SimulatedInputDispatcher APIs take a set of input sources. We also intern these
    // so that previous input source state is used as initial state for later commands.
    HashSet<Ref<SimulatedInputSource>> m_inputSources;
    HashMap<WebPageProxyIdentifier, Ref<SimulatedInputDispatcher>> m_inputDispatchersByPage;
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    // Keep track of currently active modifiers across multiple keystrokes.
    // Most platforms do not track current modifiers from synthesized events.
    unsigned m_currentModifiers { 0 };
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    bool m_simulatingTouchInteraction { false };
#endif
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    MonotonicTime m_lastClickTime;
    MouseButton m_lastClickButton { MouseButton::None };
    WebCore::IntPoint m_lastClickPosition;
    unsigned m_clickCount { 1 };
#endif

#if ENABLE(REMOTE_INSPECTOR)
    Inspector::FrontendChannel* m_remoteChannel { nullptr };
#endif

};

} // namespace WebKit
