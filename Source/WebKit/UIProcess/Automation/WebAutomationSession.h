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
#include "FrameTreeNodeData.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "SimulatedInputDispatcher.h"
#include "WebEvent.h"
#include "WebPageProxyIdentifier.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WallTime.h>
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
class WebFrameProxy;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
class WebProcessPool;

#if ENABLE(WEBDRIVER_BIDI)
class WebDriverBidiProcessor;
#endif

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

using PageAndFrameHandle = std::pair<Inspector::Protocol::Automation::BrowsingContextHandle, Inspector::Protocol::Automation::FrameHandle>;

class WebAutomationSession final : public API::ObjectImpl<API::Object::Type::AutomationSession>
    , public IPC::MessageReceiver
    , public Inspector::AutomationBackendDispatcherHandler
#if ENABLE(WEBDRIVER_ACTIONS_API)
    , public SimulatedInputDispatcher::Client
#endif
{

#if ENABLE(WEBDRIVER_BIDI)
friend class WebDriverBidiProcessor;
#endif

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
    void documentLoadedForFrame(const WebFrameProxy&, std::optional<WebCore::NavigationIdentifier>, WallTime timestamp);
    void loadCompletedForFrame(const WebFrameProxy&, std::optional<WebCore::NavigationIdentifier>, WallTime timestamp);
    void inspectorFrontendLoaded(const WebPageProxy&);
    void keyboardEventsFlushedForPage(const WebPageProxy&);
    void mouseEventsFlushedForPage(const WebPageProxy&);
    void wheelEventsFlushedForPage(const WebPageProxy&);
#if ENABLE(WEBDRIVER_BIDI)
    void didCreatePage(WebPageProxy&);
    void navigationStartedForFrame(const WebFrameProxy&, std::optional<WebCore::NavigationIdentifier>);
    void navigationCommittedForFrame(const WebFrameProxy&, std::optional<WebCore::NavigationIdentifier>);
    void navigationFailedForFrame(const WebFrameProxy&, std::optional<WebCore::NavigationIdentifier>);
    void navigationAbortedForFrame(const WebFrameProxy&, std::optional<WebCore::NavigationIdentifier>);
    void fragmentNavigatedForFrame(const WebFrameProxy&, std::optional<WebCore::NavigationIdentifier>);
    void emitContextCreatedEvent(const WebPageProxy&);
    void didCreateFrame(const WebFrameProxy&);
    void willDestroyFrame(const WebFrameProxy&);
    void contextCreatedForFrame(const WebFrameProxy&);
    void contextDestroyedForPage(const WebPageProxy&);
    void contextDestroyedForFrame(const WebFrameProxy&);
#endif
    void willClosePage(const WebPageProxy&);
    void handleRunOpenPanel(const WebPageProxy&, const WebFrameProxy&, const API::OpenPanelParameters&, WebOpenPanelResultListenerProxy&);
    void willShowJavaScriptDialog(WebPageProxy&, const String& message, std::optional<String>&& defaultValue);
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
    void clearDoubleClicks() override;
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    void simulateTouchInteraction(WebPageProxy&, TouchInteraction, const WebCore::IntPoint& locationInView, std::optional<Seconds> duration, AutomationCompletionHandler&&) override;
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    void simulateKeyboardInteraction(WebPageProxy&, KeyboardInteraction, Variant<VirtualKey, CharKey>&&, AutomationCompletionHandler&&) override;
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
    void getBrowsingContexts(Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>>&&) override;
    void getBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Inspector::CommandCallback<Ref<Inspector::Protocol::Automation::BrowsingContext>>&&) override;
    void createBrowsingContext(std::optional<Inspector::Protocol::Automation::BrowsingContextPresentation>&&, Inspector::CommandCallbackOf<String, Inspector::Protocol::Automation::BrowsingContextPresentation>&&) override;
    void closeBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Inspector::CommandCallback<void>&&) override;
    Inspector::CommandResult<void> deleteSession() override;
    void resolveBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Inspector::CommandCallback<void>&&) override;
    void switchToBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Inspector::CommandCallback<void>&&) override;
    void setWindowFrameOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, RefPtr<JSON::Object>&& origin, RefPtr<JSON::Object>&& size, Inspector::CommandCallback<void>&&) override;
    void maximizeWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Inspector::CommandCallback<void>&&) override;
    void hideWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, Inspector::CommandCallback<void>&&) override;
    void navigateBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& url, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void goBackInBrowsingContext(const String&, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void goForwardInBrowsingContext(const String&, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void traverseHistoryInBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, int delta, Inspector::CommandCallback<void>&&);
    void reloadBrowsingContext(const String&, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void waitForNavigationToComplete(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void evaluateJavaScriptFunction(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const String& function, Ref<JSON::Array>&& arguments, std::optional<bool>&& expectsImplicitCallbackArgument, std::optional<bool>&& forceUserGesture, std::optional<double>&& callbackTimeout, Inspector::CommandCallback<String>&&) override;
    void performMouseInteraction(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Object>&& requestedPosition, Inspector::Protocol::Automation::MouseButton, Inspector::Protocol::Automation::MouseInteraction, Ref<JSON::Array>&& keyModifiers, Inspector::CommandCallback<Ref<Inspector::Protocol::Automation::Point>>&&) override;
    void performKeyboardInteractions(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Array>&& interactions, Inspector::CommandCallback<void>&&) override;
    void performInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Ref<JSON::Array>&& sources, Ref<JSON::Array>&& steps, Inspector::CommandCallback<void>&&) override;
    void cancelInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Inspector::CommandCallback<void>&&) override;
    void takeScreenshot(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, std::optional<bool>&& scrollIntoViewIfNeeded, std::optional<bool>&& clipToViewport, Inspector::CommandCallback<String>&&) override;
    void resolveChildFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, std::optional<int>&& ordinal, const String& name, const Inspector::Protocol::Automation::NodeHandle&, Inspector::CommandCallback<String>&&) override;
    void resolveParentFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Inspector::CommandCallback<String>&&) override;
    void computeElementLayout(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, std::optional<bool>&& scrollIntoViewIfNeeded, Inspector::Protocol::Automation::CoordinateSystem, Inspector::CommandCallbackOf<Ref<Inspector::Protocol::Automation::Rect>, RefPtr<Inspector::Protocol::Automation::Point>, bool>&&) override;
    void getComputedRole(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Inspector::CommandCallback<String>&&) override;
    void getComputedLabel(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Inspector::CommandCallback<String>&&) override;
    void selectOptionElement(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Inspector::CommandCallback<void>&&) override;
    Inspector::CommandResult<bool> isShowingJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::CommandResult<void> dismissCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::CommandResult<void> acceptCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::CommandResult<String> messageOfCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::CommandResult<void> setUserInputForCurrentJavaScriptPrompt(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& text) override;
    Inspector::CommandResult<void> setFilesToSelectForFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Array>&& filenames, RefPtr<JSON::Array>&& fileContents) override;
    void setFilesForInputFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, const Inspector::Protocol::Automation::NodeHandle&, Ref<JSON::Array>&& filenames, Inspector::CommandCallback<void>&&) override;
    void getAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle&, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::Cookie>>>&&) override;
    void deleteSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle&, const String& cookieName, Inspector::CommandCallback<void>&&) override;
    void addSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle&, Ref<JSON::Object>&& cookie, Inspector::CommandCallback<void>&&) override;
    Inspector::CommandResult<void> deleteAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle&) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>>> getSessionPermissions() override;
    Inspector::CommandResult<void> setSessionPermissions(Ref<JSON::Array>&&) override;

    Inspector::CommandResult<String /* authenticatorId */> addVirtualAuthenticator(const String& browsingContextHandle, Ref<JSON::Object>&& authenticator) override;
    Inspector::CommandResult<void> removeVirtualAuthenticator(const String& browsingContextHandle, const String& authenticatorId) override;
    Inspector::CommandResult<void> addVirtualAuthenticatorCredential(const String& browsingContextHandle, const String& authenticatorId, Ref<JSON::Object>&& credential) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::VirtualAuthenticatorCredential>> /* credentials */> getVirtualAuthenticatorCredentials(const String& browsingContextHandle, const String& authenticatorId) override;
    Inspector::CommandResult<void> removeVirtualAuthenticatorCredential(const String& browsingContextHandle, const String& authenticatorId, const String& credentialId) override;
    Inspector::CommandResult<void> removeAllVirtualAuthenticatorCredentials(const String& browsingContextHandle, const String& authenticatorId) override;
    Inspector::CommandResult<void> setVirtualAuthenticatorUserVerified(const String& browsingContextHandle, const String& authenticatorId, bool isUserVerified) override;
    Inspector::CommandResult<void> generateTestReport(const String& browsingContextHandle, const String& message, const String& group) override;

#if ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER)
    void loadWebExtension(const Inspector::Protocol::Automation::WebExtensionResourceOptions, const String& resource, Inspector::CommandCallback<String>&&) override;
    void unloadWebExtension(const String& identifier, Inspector::CommandCallback<void>&&) override;
#endif
    void setStorageAccessPermissionState(const Inspector::Protocol::Automation::BrowsingContextHandle&, const Inspector::Protocol::Automation::FrameHandle&, Inspector::Protocol::Automation::PermissionState, Inspector::CommandCallback<void>&&) override;
    void setStorageAccessPolicy(const Inspector::Protocol::Automation::BrowsingContextHandle&, bool blocked, Inspector::CommandCallback<void>&&) override;

#if ENABLE(WEBDRIVER_BIDI)
    Inspector::CommandResult<void> processBidiMessage(const String&) override;
    void sendBidiMessage(const String&);
#endif

#if PLATFORM(MAC)
    void inspectBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle&, std::optional<bool>&& enableAutoCapturing, Inspector::CommandCallback<void>&&) override;
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

    RefPtr<WebPageProxy> webPageProxyForHandle(const String&);
    String effectiveHandleForWebFrameProxy(const WebFrameProxy&);
    String handleForWebFrameID(std::optional<WebCore::FrameIdentifier>);
    String handleForWebPageProxy(const WebPageProxy&);

    Expected<PageAndFrameHandle, AutomationCommandError> extractBrowsingContextHandles(const String&);

private:
    Ref<Inspector::Protocol::Automation::BrowsingContext> buildBrowsingContextForPage(WebPageProxy&, WebCore::FloatRect windowFrame);
    void getNextContext(Vector<Ref<WebPageProxy>>&&, Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>>&&);

    std::optional<WebCore::FrameIdentifier> webFrameIDForHandle(const String&, bool& frameNotFound);

    void waitForNavigationToCompleteOnPage(WebPageProxy&, Inspector::Protocol::Automation::PageLoadStrategy, Seconds, Inspector::CommandCallback<void>&&);
    void waitForNavigationToCompleteOnFrame(WebFrameProxy&, Inspector::Protocol::Automation::PageLoadStrategy, Seconds, Inspector::CommandCallback<void>&&);
    void respondToPendingPageNavigationCallbacksWithTimeout(HashMap<WebPageProxyIdentifier, Inspector::CommandCallback<void>>&);
    void respondToPendingFrameNavigationCallbacksWithTimeout(HashMap<WebCore::FrameIdentifier, Inspector::CommandCallback<void>>&);
    void loadTimerFired();

    void exitFullscreenWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void restoreWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void maximizeWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void hideWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);

#if ENABLE(WEBDRIVER_BIDI)
    void recursivelyEmitContextCreatedEvent(const FrameTreeNodeData&, std::optional<String>&& parentContext);
    WebPageProxy* getOpenerPage(const WebPageProxy&);
#endif

    // IPC::MessageReceiver (Implemented by generated code in WebAutomationSessionMessageReceiver.cpp).
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by WebAutomationSession messages.
    void logEntryAdded(const JSC::MessageSource&, const JSC::MessageLevel&, const String& messageText, const JSC::MessageType&, const WallTime&);

    // Platform-dependent implementations.
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    void resetMouseState();
    void updateClickCount(MouseButton, const WebCore::IntPoint&, Seconds maxTime = 0.5_s, int maxDistance = 0);
    void updateLastPosition(const WebCore::IntPoint&, int maxDistance = 0);
    void platformSimulateMouseInteraction(WebPageProxy&, MouseInteraction, MouseButton, const WebCore::IntPoint& locationInViewport, OptionSet<WebEventModifier>, const String& pointerType);
    static OptionSet<WebEventModifier> platformWebModifiersFromRaw(WebPageProxy&, unsigned modifiers);
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    // Simulates a single touch point being pressed, moved, and released.
    void platformSimulateTouchInteraction(WebPageProxy&, TouchInteraction, const WebCore::IntPoint& locationInViewport, std::optional<Seconds> duration, AutomationCompletionHandler&&);
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    // Simulates a single virtual or char key being pressed/released, such as 'a', Control, F-keys, Numpad keys, etc. as allowed by the protocol.
    void platformSimulateKeyboardInteraction(WebPageProxy&, KeyboardInteraction, Variant<VirtualKey, CharKey>&&);
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

    WeakPtr<WebProcessPool> m_processPool;

    std::unique_ptr<API::AutomationSessionClient> m_client;
    String m_sessionIdentifier { "Untitled Session"_s };
    const Ref<Inspector::FrontendRouter> m_frontendRouter;
    const Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    const Ref<Inspector::AutomationBackendDispatcher> m_domainDispatcher;
    const UniqueRef<Inspector::AutomationFrontendDispatcher> m_domainNotifier;

#if ENABLE(WEBDRIVER_BIDI)
    const UniqueRef<WebDriverBidiProcessor> m_bidiProcessor;
#endif

    HashMap<WebPageProxyIdentifier, String> m_webPageHandleMap;
    HashMap<String, WebPageProxyIdentifier> m_handleWebPageMap;

    HashMap<WebCore::FrameIdentifier, String> m_webFrameHandleMap;
    HashMap<String, WebCore::FrameIdentifier> m_handleWebFrameMap;

    HashMap<WebPageProxyIdentifier, Inspector::CommandCallback<void>> m_pendingNormalNavigationInBrowsingContextCallbacksPerPage;
    HashMap<WebPageProxyIdentifier, Inspector::CommandCallback<void>> m_pendingEagerNavigationInBrowsingContextCallbacksPerPage;
    HashMap<WebCore::FrameIdentifier, Inspector::CommandCallback<void>> m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame;
    HashMap<WebCore::FrameIdentifier, Inspector::CommandCallback<void>> m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame;
    HashMap<WebPageProxyIdentifier, Inspector::CommandCallback<void>> m_pendingInspectorCallbacksPerPage;
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    HashMap<WebPageProxyIdentifier, Inspector::CommandCallback<void>> m_pendingKeyboardEventsFlushedCallbacksPerPage;
#endif
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    HashMap<WebPageProxyIdentifier, Inspector::CommandCallback<void>> m_pendingMouseEventsFlushedCallbacksPerPage;
#endif
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    HashMap<WebPageProxyIdentifier, Inspector::CommandCallback<void>> m_pendingWheelEventsFlushedCallbacksPerPage;
#endif

    uint64_t m_nextEvaluateJavaScriptCallbackID { 1 };
    HashMap<uint64_t, Inspector::CommandCallback<String>> m_evaluateJavaScriptFunctionCallbacks;

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
    HashMap<MouseButton, bool, WTF::IntHash<MouseButton>, WTF::StrongEnumHashTraits<MouseButton>> m_mouseButtonsCurrentlyDown;
    std::optional<WebCore::IntPoint> m_lastPosition;
    std::optional<WebCore::IntPoint> m_lastClickPosition;
    unsigned m_clickCount { 1 };
#endif

#if ENABLE(REMOTE_INSPECTOR)
    Inspector::FrontendChannel* m_remoteChannel { nullptr };
    const Ref<Debuggable> m_debuggable;
#endif

};

} // namespace WebKit
