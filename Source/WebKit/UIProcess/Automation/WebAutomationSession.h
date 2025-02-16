/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "SimulatedInputDispatcher.h"
#include "WebEvent.h"
#include "WebPageProxyIdentifier.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>

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

enum class ForceSoftwareCapturingViewportSnapshot : bool;

class AutomationCommandError {
public:
    Inspector::Protocol::Automation::ErrorMessage type;
    std::optional<String> message { std::nullopt };
    
    AutomationCommandError(Inspector::Protocol::Automation::ErrorMessage type)
        : type(type) { }

    AutomationCommandError(Inspector::Protocol::Automation::ErrorMessage type, const String& message)
        : type(type)
        , message(message) { }
    
    String toProtocolString();
};

using AutomationCompletionHandler = WTF::CompletionHandler<void(std::optional<AutomationCommandError>)>;

class WebAutomationSession final : public API::ObjectImpl<API::Object::Type::AutomationSession>
    , public IPC::MessageReceiver
    , public Inspector::AutomationBackendDispatcherHandler
#if ENABLE(WEBDRIVER_ACTIONS_API)
    , public SimulatedInputDispatcher::Client
#endif
{
public:
    WebAutomationSession();
    ~WebAutomationSession() override;

    void ref() const final { API::Object::ref(); }
    void deref() const final { API::Object::deref(); }


#if ENABLE(REMOTE_INSPECTOR)
    class Debuggable : public Inspector::RemoteAutomationTarget {
    public:
        static Ref<Debuggable> create(WebAutomationSession&);

        void sessionDestroyed();

    // Inspector::RemoteAutomationTarget API
    String name() const;
    void dispatchMessageFromRemote(String&& message);
    void connect(Inspector::FrontendChannel&, bool isAutomaticConnection = false, bool immediatelyPause = false);
    void disconnect(Inspector::FrontendChannel&);

    private:
        explicit Debuggable(WebAutomationSession&);

        WeakPtr<WebAutomationSession> m_session;
    };
#endif // ENABLE(REMOTE_INSPECTOR)

    void setClient(std::unique_ptr<API::AutomationSessionClient>&&);

    void setSessionIdentifier(const String& sessionIdentifier) { m_sessionIdentifier = sessionIdentifier; }
    String sessionIdentifier() const { return m_sessionIdentifier; }

    RefPtr<WebProcessPool> protectedProcessPool() const;
    void setProcessPool(WebProcessPool*);

    void navigationOccurredForFrame(const WebFrameProxy&);
    void documentLoadedForFrame(const WebFrameProxy&);
    void inspectorFrontendLoaded(const WebPageProxy&);
    void keyboardEventsFlushedForPage(const WebPageProxy&);
    void mouseEventsFlushedForPage(const WebPageProxy&);
    void wheelEventsFlushedForPage(const WebPageProxy&);
    void willClosePage(const WebPageProxy&);
    void handleRunOpenPanel(const WebPageProxy&, const WebFrameProxy&, const API::OpenPanelParameters&, WebOpenPanelResultListenerProxy&);
    void willShowJavaScriptDialog(WebPageProxy&);
    void didEnterFullScreenForPage(const WebPageProxy&);
    void didExitFullScreenForPage(const WebPageProxy&);

    bool shouldAllowGetUserMediaForPage(const WebPageProxy&) const;

#if ENABLE(REMOTE_INSPECTOR)
    String name() const { return m_sessionIdentifier; }
    void dispatchMessageFromRemote(String&& message);
    void connect(Inspector::FrontendChannel&, bool isAutomaticConnection = false, bool immediatelyPause = false);
    void disconnect(Inspector::FrontendChannel&);

    void init();
    bool isPaired() const;
    bool isPendingTermination() const;
#endif

    void terminate();

#if ENABLE(WEBDRIVER_ACTIONS_API)

    // SimulatedInputDispatcher::Client API
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    void simulateMouseInteraction(WebPageProxy&, MouseInteraction, MouseButton, const WebCore::IntPoint& locationInView, const String& pointerType, AutomationCompletionHandler&&) override;
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    void simulateTouchInteraction(WebPageProxy&, TouchInteraction, const WebCore::IntPoint& locationInView, std::optional<Seconds> duration, AutomationCompletionHandler&&) override;
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    void simulateKeyboardInteraction(WebPageProxy&, KeyboardInteraction, std::variant<VirtualKey, CharKey>&&, AutomationCompletionHandler&&) override;
#endif
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    void simulateWheelInteraction(WebPageProxy&, const WebCore::IntPoint& locationInView, const WebCore::IntSize& delta, AutomationCompletionHandler&&) override;
#endif
    void viewportInViewCenterPointOfElement(WebPageProxy&, std::optional<WebCore::FrameIdentifier>, const Inspector::Protocol::Automation::NodeHandle&, Function<void(std::optional<WebCore::IntPoint>, std::optional<AutomationCommandError>)>&&) override;

#endif // ENABLE(WEBDRIVER_ACTIONS_API)

    // Inspector::AutomationBackendDispatcherHandler API
    // NOTE: the set of declarations included in this interface depend on the "platform" property in Automation.json
    // and the --platform argument passed to the protocol bindings generator.

    // Platform: Generic
    void getBrowsingContexts(Ref<GetBrowsingContextsCallback>&&) override;
    void getBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<GetBrowsingContextCallback>&&) override;
    void createBrowsingContext(std::optional<Inspector::Protocol::Automation::BrowsingContextPresentation>&&, Ref<CreateBrowsingContextCallback>&&) override;
    Inspector::Protocol::ErrorStringOr<void> closeBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::Protocol::ErrorStringOr<void> deleteSession() override;
    void switchToBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<SwitchToBrowsingContextCallback>&&) override;
    void setWindowFrameOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, RefPtr<JSON::Object>&& origin, RefPtr<JSON::Object>&& size, Ref<SetWindowFrameOfBrowsingContextCallback>&&) override;
    void maximizeWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<MaximizeWindowOfBrowsingContextCallback>&&) override;
    void hideWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<HideWindowOfBrowsingContextCallback>&&) override;
    void navigateBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& url, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Ref<NavigateBrowsingContextCallback>&&) override;
    void goBackInBrowsingContext(const String&, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Ref<GoBackInBrowsingContextCallback>&&) override;
    void goForwardInBrowsingContext(const String&, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Ref<GoForwardInBrowsingContextCallback>&&) override;
    void reloadBrowsingContext(const String&, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Ref<ReloadBrowsingContextCallback>&&) override;
    void waitForNavigationToComplete(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Ref<WaitForNavigationToCompleteCallback>&&) override;
    void evaluateJavaScriptFunction(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const String& function, Ref<JSON::Array>&& arguments, std::optional<bool>&& expectsImplicitCallbackArgument, std::optional<bool>&& forceUserGesture, std::optional<double>&& callbackTimeout, Ref<EvaluateJavaScriptFunctionCallback>&&) override;
    void performMouseInteraction(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Object>&& requestedPosition, Inspector::Protocol::Automation::MouseButton, Inspector::Protocol::Automation::MouseInteraction, Ref<JSON::Array>&& keyModifiers, Ref<PerformMouseInteractionCallback>&&) override;
    void performKeyboardInteractions(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Array>&& interactions, Ref<PerformKeyboardInteractionsCallback>&&) override;
    void performInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<JSON::Array>&& sources, Ref<JSON::Array>&& steps, Ref<PerformInteractionSequenceCallback>&&) override;
    void cancelInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<CancelInteractionSequenceCallback>&&) override;
    void takeScreenshot(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, std::optional<bool>&& scrollIntoViewIfNeeded, std::optional<bool>&& clipToViewport, Ref<TakeScreenshotCallback>&&) override;
    void resolveChildFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, std::optional<int>&& ordinal, const String& name, const Inspector::Protocol::Automation::NodeHandle&, Ref<ResolveChildFrameHandleCallback>&&) override;
    void resolveParentFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<ResolveParentFrameHandleCallback>&&) override;
    void computeElementLayout(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, std::optional<bool>&& scrollIntoViewIfNeeded, Inspector::Protocol::Automation::CoordinateSystem, Ref<ComputeElementLayoutCallback>&&) override;
    void getComputedRole(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Ref<GetComputedRoleCallback>&&) override;
    void getComputedLabel(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Ref<GetComputedLabelCallback>&&) override;
    void selectOptionElement(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Ref<SelectOptionElementCallback>&&) override;
    Inspector::Protocol::ErrorStringOr<bool> isShowingJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::Protocol::ErrorStringOr<void> dismissCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::Protocol::ErrorStringOr<void> acceptCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::Protocol::ErrorStringOr<String> messageOfCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::Protocol::ErrorStringOr<void> setUserInputForCurrentJavaScriptPrompt(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& text) override;
    Inspector::Protocol::ErrorStringOr<void> setFilesToSelectForFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Array>&& filenames, RefPtr<JSON::Array>&& fileContents) override;
    void setFilesForInputFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Ref<JSON::Array>&& filenames, Ref<SetFilesForInputFileUploadCallback>&&) override;
    void getAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<GetAllCookiesCallback>&&) override;
    void deleteSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& cookieName, Ref<DeleteSingleCookieCallback>&&) override;
    void addSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Object>&& cookie, Ref<AddSingleCookieCallback>&&) override;
    Inspector::Protocol::ErrorStringOr<void> deleteAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>>> getSessionPermissions() override;
    Inspector::Protocol::ErrorStringOr<void> setSessionPermissions(Ref<JSON::Array>&&) override;

    Inspector::Protocol::ErrorStringOr<String /* authenticatorId */> addVirtualAuthenticator(const String& browsingContextHandle, Ref<JSON::Object>&& authenticator) override;
    Inspector::Protocol::ErrorStringOr<void> removeVirtualAuthenticator(const String& browsingContextHandle, const String& authenticatorId) override;
    Inspector::Protocol::ErrorStringOr<void> addVirtualAuthenticatorCredential(const String& browsingContextHandle, const String& authenticatorId, Ref<JSON::Object>&& credential) override;
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::VirtualAuthenticatorCredential>> /* credentials */> getVirtualAuthenticatorCredentials(const String& browsingContextHandle, const String& authenticatorId) override;
    Inspector::Protocol::ErrorStringOr<void> removeVirtualAuthenticatorCredential(const String& browsingContextHandle, const String& authenticatorId, const String& credentialId) override;
    Inspector::Protocol::ErrorStringOr<void> removeAllVirtualAuthenticatorCredentials(const String& browsingContextHandle, const String& authenticatorId) override;
    Inspector::Protocol::ErrorStringOr<void> setVirtualAuthenticatorUserVerified(const String& browsingContextHandle, const String& authenticatorId, bool isUserVerified) override;
    Inspector::Protocol::ErrorStringOr<void> generateTestReport(const String& browsingContextHandle, const String& message, const String& group) override;

#if ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER)
    void loadWebExtension(const Inspector::Protocol::Automation::WebExtensionResourceOptions, const String& resource, Ref<LoadWebExtensionCallback>&&) override;
    void unloadWebExtension(const String& identifier, Ref<UnloadWebExtensionCallback>&&) override;
#endif

#if PLATFORM(MAC)
    void inspectBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, std::optional<bool>&& enableAutoCapturing, Ref<InspectBrowsingContextCallback>&&) override;
#endif

    // Event Simulation Support.
    bool isSimulatingUserInteraction() const;
#if ENABLE(WEBDRIVER_ACTIONS_API)
    SimulatedInputDispatcher& inputDispatcherForPage(WebPageProxy&);
#endif

#if PLATFORM(MAC)
    bool wasEventSynthesizedForAutomation(NSEvent *);
    void markEventAsSynthesizedForAutomation(NSEvent *);
#endif

    void didDestroyFrame(WebCore::FrameIdentifier);

private:
    RefPtr<WebPageProxy> webPageProxyForHandle(const String&);
    String handleForWebPageProxy(const WebPageProxy&);
    Ref<Inspector::Protocol::Automation::BrowsingContext> buildBrowsingContextForPage(WebPageProxy&, WebCore::FloatRect windowFrame);
    void getNextContext(Ref<WebAutomationSession>&&, Vector<Ref<WebPageProxy>>&&, Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>, Ref<WebAutomationSession::GetBrowsingContextsCallback>&&);

    std::optional<WebCore::FrameIdentifier> webFrameIDForHandle(const String&, bool& frameNotFound);
    String handleForWebFrameID(std::optional<WebCore::FrameIdentifier>);
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
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by WebAutomationSession messages.
    void logEntryAdded(const JSC::MessageSource&, const JSC::MessageLevel&, const String& messageText, const JSC::MessageType&, const WallTime&);

    // Platform-dependent implementations.
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    void updateClickCount(MouseButton, const WebCore::IntPoint&, Seconds maxTime = 1_s, int maxDistance = 0);
    void resetClickCount();
    void platformSimulateMouseInteraction(WebPageProxy&, MouseInteraction, MouseButton, const WebCore::IntPoint& locationInViewport, OptionSet<WebEventModifier>, const String& pointerType);
    static OptionSet<WebEventModifier> platformWebModifiersFromRaw(WebPageProxy&, unsigned modifiers);
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    // Simulates a single touch point being pressed, moved, and released.
    void platformSimulateTouchInteraction(WebPageProxy&, TouchInteraction, const WebCore::IntPoint& locationInViewport, std::optional<Seconds> duration, AutomationCompletionHandler&&);
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    // Simulates a single virtual or char key being pressed/released, such as 'a', Control, F-keys, Numpad keys, etc. as allowed by the protocol.
    void platformSimulateKeyboardInteraction(WebPageProxy&, KeyboardInteraction, std::variant<VirtualKey, CharKey>&&);
    // Simulates key presses to produce the codepoints in a string. One or more code points are delivered atomically at grapheme cluster boundaries.
    void platformSimulateKeySequence(WebPageProxy&, const String&);
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    void platformSimulateWheelInteraction(WebPageProxy&, const WebCore::IntPoint& locationInViewport, const WebCore::IntSize& delta);
#endif // ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

    // Get base64-encoded PNG data from a bitmap.
    static std::optional<String> platformGetBase64EncodedPNGData(WebCore::ShareableBitmap::Handle&&);
    static std::optional<String> platformGetBase64EncodedPNGData(const ViewSnapshot&);

    // Save base64-encoded file contents to a local file path and return the path.
    // This reuses the basename of the remote file path so that the filename exposed to DOM API remains the same.
    std::optional<String> platformGenerateLocalFilePathForRemoteFile(const String& remoteFilePath, const String& base64EncodedFileContents);

#if PLATFORM(COCOA)
    // The type parameter of the NSArray argument is platform-dependent.
    void sendSynthesizedEventsToPage(WebPageProxy&, NSArray *eventsToSend);

    std::optional<unichar> charCodeForVirtualKey(Inspector::Protocol::Automation::VirtualKey) const;
    std::optional<unichar> charCodeIgnoringModifiersForVirtualKey(Inspector::Protocol::Automation::VirtualKey) const;
#endif

    Ref<Inspector::FrontendRouter> protectedFrontendRouter() const;
    Ref<Inspector::BackendDispatcher> protectedBackendDispatcher() const;
#if ENABLE(REMOTE_INSPECTOR)
    Ref<Debuggable> protectedDebuggable() const;
#endif

    WeakPtr<WebProcessPool> m_processPool;

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
    HashMap<WebPageProxyIdentifier, Function<void(std::optional<AutomationCommandError>)>> m_pendingKeyboardEventsFlushedCallbacksPerPage;
#endif
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    HashMap<WebPageProxyIdentifier, Function<void(std::optional<AutomationCommandError>)>> m_pendingMouseEventsFlushedCallbacksPerPage;
#endif
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    HashMap<WebPageProxyIdentifier, Function<void(std::optional<AutomationCommandError>)>> m_pendingWheelEventsFlushedCallbacksPerPage;
#endif

    uint64_t m_nextEvaluateJavaScriptCallbackID { 1 };
    HashMap<uint64_t, RefPtr<Inspector::AutomationBackendDispatcherHandler::EvaluateJavaScriptFunctionCallback>> m_evaluateJavaScriptFunctionCallbacks;

    enum class WindowTransitionedToState {
        Fullscreen,
        Unfullscreen,
    };
    Function<void(WindowTransitionedToState)> m_windowStateTransitionCallback { };

    RunLoop::Timer m_loadTimer;
    Vector<String> m_filesToSelectForFileUpload;

    bool m_permissionForGetUserMedia { true };

#if ENABLE(WEBDRIVER_ACTIONS_API)
    // SimulatedInputDispatcher APIs take a set of input sources. We also intern these
    // so that previous input source state is used as initial state for later commands.
    HashMap<String, Ref<SimulatedInputSource>> m_inputSources;
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
    HashMap<MouseButton, bool, WTF::IntHash<MouseButton>, WTF::StrongEnumHashTraits<MouseButton>> m_mouseButtonsCurrentlyDown;
    WebCore::IntPoint m_lastClickPosition;
    unsigned m_clickCount { 1 };
#endif

#if ENABLE(REMOTE_INSPECTOR)
    Inspector::FrontendChannel* m_remoteChannel { nullptr };
    Ref<Debuggable> m_debuggable;
#endif

};

} // namespace WebKit
