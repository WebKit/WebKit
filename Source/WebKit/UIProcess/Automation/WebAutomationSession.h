/*
 * Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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
#include "Untrusted.h"
#include "WebEvent.h"
#include "WebPageProxyIdentifier.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/NavigationIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WallTime.h>
#include <wtf/WeakPtr.h>

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteAutomationTarget.h>
#endif

#if ENABLE(WEBDRIVER_BIDI)
#include "IdentifierTypes.h"
#endif

namespace API {
class AutomationSessionClient;
class Navigation;
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
typedef struct CGImage* CGImageRef;
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
class WebPageInspectorController;
class WebProcessPool;

#if ENABLE(REMOTE_INSPECTOR)
class InspectorPassthroughChannel;
#endif

#if ENABLE(WEBDRIVER_BIDI)
class WebDriverBidiProcessor;
#endif

enum class ForceSoftwareCapturingViewportSnapshot : bool;
enum class SameDocumentNavigationType : uint8_t;

class AutomationCommandError {
public:
    Inspector::Protocol::Automation::ErrorMessage type;
    std::optional<RefPtr<StringImpl>> message { std::nullopt };
    
    AutomationCommandError(Inspector::Protocol::Automation::ErrorMessage type)
        : type(type) { }

    AutomationCommandError(Inspector::Protocol::Automation::ErrorMessage type, StringImpl* message)
        : type(type)
        , message(message) { }
    
    RefPtr<StringImpl> toProtocolString();
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

#if ENABLE(REMOTE_INSPECTOR)
friend class InspectorPassthroughChannel;
#endif

public:
    WebAutomationSession(bool siteIsolationEnabled = false);
    ~WebAutomationSession() override;

    void ref() const final { API::Object::ref(); }
    void deref() const final { API::Object::deref(); }


#if ENABLE(REMOTE_INSPECTOR)
    class Debuggable : public Inspector::RemoteAutomationTarget {
    public:
        static Ref<Debuggable> create(WebAutomationSession&);

        NODELETE void sessionDestroyed();

    // Inspector::RemoteAutomationTarget API
    RefPtr<StringImpl> name() const;
    void dispatchMessageFromRemote(RefPtr<StringImpl>&& message);
    void connect(Inspector::FrontendChannel&, bool isAutomaticConnection = false, bool immediatelyPause = false);
    void disconnect(Inspector::FrontendChannel&);

    private:
        explicit Debuggable(WebAutomationSession&);

        WeakPtr<WebAutomationSession> m_session;
    };
#endif // ENABLE(REMOTE_INSPECTOR)

    void setClient(std::unique_ptr<API::AutomationSessionClient>&&);

    void setSessionIdentifier(StringImpl* sessionIdentifier) { m_sessionIdentifier = sessionIdentifier; }
    RefPtr<StringImpl> sessionIdentifier() const { return m_sessionIdentifier; }

    bool siteIsolationEnabled() const { return m_siteIsolationEnabled; }

    NODELETE WebProcessPool* processPool() const;
    void setProcessPool(WebProcessPool*);

    // A same-document navigation is reported as such: it does not end a wait whose navigation was destroyed.
    enum class NavigationNotification : bool { LoadEnded, SameDocumentNavigation };
    void navigationOccurredForFrame(const WebFrameProxy&, API::Navigation* = nullptr, NavigationNotification = NavigationNotification::LoadEnded);
    void navigationDestroyed(const WebPageProxy&, const API::Navigation&);
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
    void historyUpdatedForFrame(const WebFrameProxy&);
    // Returns false for a same-document navigation that is not reported because another report follows it, which
    // should then be the one to complete pending navigation commands.
    bool sameDocumentNavigationOccurredForFrame(const WebFrameProxy&, SameDocumentNavigationType, std::optional<WebCore::NavigationIdentifier>, const URL& previousURL);
    void emitContextCreatedEvent(const WebPageProxy&);
    void didCreateFrame(const WebFrameProxy&);
    void willDestroyFrame(const WebFrameProxy&);
    void contextCreatedForFrame(const WebFrameProxy&);
    void contextDestroyedForPage(const WebPageProxy&);
    void contextDestroyedForFrame(const WebFrameProxy&);
    void setViewportForPage(WebPageProxy&, std::optional<int> width, std::optional<int> height, std::optional<double> devicePixelRatio, Inspector::CommandCallback<void>&&);
#endif
    void willClosePage(const WebPageProxy&);
    void handleRunOpenPanel(const WebPageProxy&, const WebFrameProxy&, const API::OpenPanelParameters&, WebOpenPanelResultListenerProxy&);
    void willShowJavaScriptDialog(WebPageProxy&, StringImpl* message, std::optional<RefPtr<StringImpl>>&& defaultValue);
    void didEnterFullScreenForPage(const WebPageProxy&);
    void didExitFullScreenForPage(const WebPageProxy&);

    NODELETE bool shouldAllowGetUserMediaForPage(const WebPageProxy&) const;

#if ENABLE(REMOTE_INSPECTOR)
    RefPtr<StringImpl> name() const { return m_sessionIdentifier; }
    void dispatchMessageFromRemote(RefPtr<StringImpl>&& message);
    void connect(Inspector::FrontendChannel&, bool isAutomaticConnection = false, bool immediatelyPause = false);
    void disconnect(Inspector::FrontendChannel&);

    void init();
    NODELETE bool isPaired() const;
    NODELETE bool isPendingTermination() const;
#endif

    void terminate();

#if ENABLE(WEBDRIVER_ACTIONS_API)

    // SimulatedInputDispatcher::Client API
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    void simulateMouseInteraction(WebPageProxy&, MouseInteraction, MouseButton, const WebCore::IntPoint& locationInView, StringImpl* pointerType, AutomationCompletionHandler&&) override;
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
    void viewportInViewCenterPointOfElement(WebPageProxy&, std::optional<WebCore::FrameIdentifier>, StringImpl*, Function<void(std::optional<WebCore::IntPoint>, std::optional<AutomationCommandError>)>&&) override;

#endif // ENABLE(WEBDRIVER_ACTIONS_API)

    // Inspector::AutomationBackendDispatcherHandler API
    // NOTE: the set of declarations included in this interface depend on the "platform" property in Automation.json
    // and the --platform argument passed to the protocol bindings generator.

    // Platform: Generic
    void getBrowsingContexts(Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>>&&) override;
    void getBrowsingContext(StringImpl*, Inspector::CommandCallback<Ref<Inspector::Protocol::Automation::BrowsingContext>>&&) override;
    void createBrowsingContext(std::optional<Inspector::Protocol::Automation::BrowsingContextPresentation>&&, Inspector::CommandCallbackOf<RefPtr<StringImpl>, Inspector::Protocol::Automation::BrowsingContextPresentation>&&) override;
    void closeBrowsingContext(StringImpl*, Inspector::CommandCallback<void>&&) override;
    Inspector::CommandResult<void> deleteSession() override;
    void resolveBrowsingContext(StringImpl*, StringImpl*, Inspector::CommandCallback<void>&&) override;
    void switchToBrowsingContext(StringImpl*, StringImpl*, Inspector::CommandCallback<void>&&) override;
    void setWindowFrameOfBrowsingContext(StringImpl*, RefPtr<JSON::Object>&& origin, RefPtr<JSON::Object>&& size, Inspector::CommandCallback<void>&&) override;
    void setPageZoomFactorOfBrowsingContext(StringImpl*, double zoomFactor, Inspector::CommandCallback<void>&&) override;
    void maximizeWindowOfBrowsingContext(StringImpl*, Inspector::CommandCallback<void>&&) override;
    void hideWindowOfBrowsingContext(StringImpl*, Inspector::CommandCallback<void>&&) override;
    void navigateBrowsingContext(StringImpl*, StringImpl* url, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void goBackInBrowsingContext(StringImpl*, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void goForwardInBrowsingContext(StringImpl*, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void traverseHistoryInBrowsingContext(StringImpl*, int delta, Inspector::CommandCallback<void>&&);
    void reloadBrowsingContext(StringImpl*, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void waitForNavigationToComplete(StringImpl*, StringImpl*, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&&, std::optional<double>&& pageLoadTimeout, Inspector::CommandCallback<void>&&) override;
    void evaluateJavaScriptFunction(StringImpl*, StringImpl*, StringImpl* function, Ref<JSON::Array>&& arguments, std::optional<bool>&& expectsImplicitCallbackArgument, std::optional<bool>&& forceUserGesture, std::optional<double>&& callbackTimeout, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;
    // Unlike the classic command, a user prompt does not end the evaluation: it completes when the script does.
    void evaluateJavaScriptFunctionForBidi(StringImpl*, StringImpl*, StringImpl* function, Ref<JSON::Array>&& arguments, std::optional<bool>&& forceUserGesture, Inspector::CommandCallback<RefPtr<StringImpl>>&&);
    void evaluateBidiScript(StringImpl*, StringImpl*, StringImpl* expression, bool awaitPromise, int maxObjectDepth, std::optional<double>&& callbackTimeout, Inspector::CommandCallback<RefPtr<StringImpl>>&&);
    void performMouseInteraction(StringImpl*, Ref<JSON::Object>&& requestedPosition, Inspector::Protocol::Automation::MouseButton, Inspector::Protocol::Automation::MouseInteraction, Ref<JSON::Array>&& keyModifiers, Inspector::CommandCallback<Ref<Inspector::Protocol::Automation::Point>>&&) override;
    void performKeyboardInteractions(StringImpl*, Ref<JSON::Array>&& interactions, Inspector::CommandCallback<void>&&) override;
    void performInteractionSequence(StringImpl*, StringImpl*, Ref<JSON::Array>&& sources, Ref<JSON::Array>&& steps, Inspector::CommandCallback<void>&&) override;
    void cancelInteractionSequence(StringImpl*, StringImpl*, Inspector::CommandCallback<void>&&) override;
    void takeScreenshot(StringImpl*, StringImpl*, StringImpl*, std::optional<bool>&& scrollIntoViewIfNeeded, std::optional<bool>&& clipToViewport, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;
    void resolveChildFrameHandle(StringImpl*, StringImpl*, std::optional<int>&& ordinal, StringImpl* name, StringImpl*, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;
    void resolveParentFrameHandle(StringImpl*, StringImpl*, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;
    void computeElementLayout(StringImpl*, StringImpl*, StringImpl*, std::optional<bool>&& scrollIntoViewIfNeeded, Inspector::Protocol::Automation::CoordinateSystem, Inspector::CommandCallbackOf<Ref<Inspector::Protocol::Automation::Rect>, RefPtr<Inspector::Protocol::Automation::Point>, bool>&&) override;
    void getComputedRole(StringImpl*, StringImpl*, StringImpl*, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;
    void getComputedLabel(StringImpl*, StringImpl*, StringImpl*, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;
    void consumeUserActivation(StringImpl*, StringImpl*, Inspector::CommandCallback<bool>&&) override;
    void selectOptionElement(StringImpl*, StringImpl*, StringImpl*, Inspector::CommandCallback<void>&&) override;
    Inspector::CommandResult<bool> isShowingJavaScriptDialog(StringImpl*) override;
    Inspector::CommandResult<Inspector::Protocol::Automation::JavaScriptDialogType> typeOfCurrentJavaScriptDialog(StringImpl*) override;
    Inspector::CommandResult<void> dismissCurrentJavaScriptDialog(StringImpl*) override;
    Inspector::CommandResult<void> acceptCurrentJavaScriptDialog(StringImpl*) override;
    Inspector::CommandResult<RefPtr<StringImpl>> messageOfCurrentJavaScriptDialog(StringImpl*) override;
    Inspector::CommandResult<void> setUserInputForCurrentJavaScriptPrompt(StringImpl*, StringImpl* text) override;
    Inspector::CommandResult<void> setFilesToSelectForFileUpload(StringImpl*, Ref<JSON::Array>&& filenames, RefPtr<JSON::Array>&& fileContents) override;
    void setFilesForInputFileUpload(StringImpl*, StringImpl*, StringImpl*, Ref<JSON::Array>&& filenames, Inspector::CommandCallback<void>&&) override;
    void getAllCookies(StringImpl*, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::Cookie>>>&&) override;
    void deleteSingleCookie(StringImpl*, StringImpl* cookieName, Inspector::CommandCallback<void>&&) override;
    void addSingleCookie(StringImpl*, Ref<JSON::Object>&& cookie, Inspector::CommandCallback<void>&&) override;
    void deleteAllCookies(StringImpl*, Inspector::CommandCallback<void>&&) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>>> getSessionPermissions() override;
    Inspector::CommandResult<void> setSessionPermissions(Ref<JSON::Array>&&) override;

    Inspector::CommandResult<RefPtr<StringImpl> /* authenticatorId */> addVirtualAuthenticator(StringImpl* browsingContextHandle, Ref<JSON::Object>&& authenticator) override;
    Inspector::CommandResult<void> removeVirtualAuthenticator(StringImpl* browsingContextHandle, StringImpl* authenticatorId) override;
    Inspector::CommandResult<void> addVirtualAuthenticatorCredential(StringImpl* browsingContextHandle, StringImpl* authenticatorId, Ref<JSON::Object>&& credential) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::VirtualAuthenticatorCredential>> /* credentials */> getVirtualAuthenticatorCredentials(StringImpl* browsingContextHandle, StringImpl* authenticatorId) override;
    Inspector::CommandResult<void> removeVirtualAuthenticatorCredential(StringImpl* browsingContextHandle, StringImpl* authenticatorId, StringImpl* credentialId) override;
    Inspector::CommandResult<void> removeAllVirtualAuthenticatorCredentials(StringImpl* browsingContextHandle, StringImpl* authenticatorId) override;
    Inspector::CommandResult<void> setVirtualAuthenticatorUserVerified(StringImpl* browsingContextHandle, StringImpl* authenticatorId, bool isUserVerified) override;
    Inspector::CommandResult<void> generateTestReport(StringImpl* browsingContextHandle, StringImpl* message, StringImpl* group) override;

#if ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER)
    void loadWebExtension(const Inspector::Protocol::Automation::WebExtensionResourceOptions, StringImpl* resource, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;
    void unloadWebExtension(StringImpl* identifier, Inspector::CommandCallback<void>&&) override;
#endif
    void setStorageAccessPermissionState(StringImpl*, StringImpl*, Inspector::Protocol::Automation::PermissionState, Inspector::CommandCallback<void>&&) override;
    void setStorageAccessPolicy(StringImpl*, bool blocked, Inspector::CommandCallback<void>&&) override;

#if ENABLE(WEBDRIVER_BIDI)
    Inspector::CommandResult<void> processBidiMessage(StringImpl*) override;
    Inspector::CommandResult<void> emitActiveBidiScriptRealmCreatedEvents() override;
    void sendBidiMessage(StringImpl*);
    WebDriverBidiProcessor& bidiProcessor() const { return m_bidiProcessor; }
#endif

#if ENABLE(REMOTE_INSPECTOR)
    Inspector::CommandResult<void> sendInspectorMessage(StringImpl*, StringImpl* message) override;
#endif

    void performApplicationCommand(StringImpl*, StringImpl* commandName, StringImpl* arguments, Inspector::CommandCallback<RefPtr<StringImpl>>&&) override;

#if PLATFORM(MAC)
    void inspectBrowsingContext(StringImpl*, std::optional<bool>&& enableAutoCapturing, Inspector::CommandCallback<void>&&) override;
#endif

    // Event Simulation Support.
    NODELETE bool isSimulatingUserInteraction() const;
#if ENABLE(WEBDRIVER_ACTIONS_API)
    SimulatedInputDispatcher& inputDispatcherForPage(WebPageProxy&);
    void runInteractionSequence(WebPageProxy&, std::optional<WebCore::FrameIdentifier>, Vector<SimulatedInputKeyFrame>&&, const HashMap<RefPtr<StringImpl>, Ref<SimulatedInputSource>>& inputSources, SimulatedInputRunOptions, AutomationCompletionHandler&&);
#if ENABLE(WEBDRIVER_BIDI)
    void runBidiInteractionSequence(WebPageProxy&, std::optional<WebCore::FrameIdentifier>, Vector<SimulatedInputKeyFrame>&&, const HashMap<RefPtr<StringImpl>, Ref<SimulatedInputSource>>& inputSources, AutomationCompletionHandler&&);
#endif
#endif

#if PLATFORM(MAC)
    bool wasEventSynthesizedForAutomation(NSEvent *);
    void markEventAsSynthesizedForAutomation(NSEvent *);
#endif

    void didDestroyFrame(WebCore::FrameIdentifier);

    // A process swap replaces the main frame's proxy, and the replacement is given a
    // freshly generated identifier, so references issued under the old one have to be
    // re-keyed or they read as never having existed.
    void transferKnownNodeReferences(WebCore::FrameIdentifier oldFrameID, WebCore::FrameIdentifier newFrameID);

    RefPtr<WebPageProxy> webPageProxyForHandle(StringImpl*);
    std::optional<WebCore::FrameIdentifier> webFrameIDForHandle(StringImpl*, bool& frameNotFound);
    RefPtr<StringImpl> effectiveHandleForWebFrameProxy(const WebFrameProxy&);
#if ENABLE(WEBDRIVER_BIDI)
    HashSet<RefPtr<StringImpl>> bidiSubscriptionScopeForFrame(const WebFrameProxy&) const;
#endif
    RefPtr<StringImpl> handleForWebFrameID(std::optional<WebCore::FrameIdentifier>);
    RefPtr<StringImpl> handleForWebPageProxy(const WebPageProxy&);

    std::expected<PageAndFrameHandle, AutomationCommandError> extractBrowsingContextHandles(StringImpl*);

#if ENABLE(WEBDRIVER_BIDI)
    bool isValidUserContext(StringImpl* userContextID) const;
#endif

private:
    Ref<Inspector::Protocol::Automation::BrowsingContext> buildBrowsingContextForPage(WebPageProxy&, WebCore::FloatRect windowFrame);
    void getNextContext(Vector<Ref<WebPageProxy>>&&, Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>>&&);

    // A wait registered by a command that started a navigation completes for that navigation, or for one the
    // UI process learned of later, which replaced it; a load that was already in flight (e.g. the initial
    // about:blank of a new tab) cannot end it early. Once that navigation is destroyed without having reported
    // its end, any load that is not an earlier one completes the wait. A wait without a navigation completes
    // with the first navigation notification when it is for a same-document target, and otherwise with the
    // notification its load strategy specifies.
    struct PendingPageNavigationCallback {
        RefPtr<API::Navigation> navigation;
        bool isForSameDocumentNavigation { false };
        bool navigationWasDestroyed { false };
        Inspector::CommandCallback<void> callback;
    };
    enum class UnboundWaits : bool { Keep, Complete };
    Vector<Inspector::CommandCallback<void>> takePendingPageNavigationCallbacks(HashMap<WebPageProxyIdentifier, Vector<PendingPageNavigationCallback>>&, const WebPageProxy&, API::Navigation* finishedNavigation, UnboundWaits, NavigationNotification = NavigationNotification::LoadEnded);
    enum class SameDocumentNavigation : bool { No, Yes };
    enum class UserPromptEndsEvaluation : bool { No, Yes };
    // The callbacks a user prompt ends early (Yes) are kept apart from those it leaves running (No).
    HashMap<uint64_t, Inspector::CommandCallback<RefPtr<StringImpl>>>& evaluateJavaScriptFunctionCallbacks(UserPromptEndsEvaluation);
    void evaluateJavaScriptFunction(StringImpl*, StringImpl*, StringImpl* function, Ref<JSON::Array>&& arguments, std::optional<bool>&& expectsImplicitCallbackArgument, std::optional<bool>&& forceUserGesture, std::optional<double>&& callbackTimeout, UserPromptEndsEvaluation, Inspector::CommandCallback<RefPtr<StringImpl>>&&);
    void waitForNavigationToCompleteOnPage(WebPageProxy&, Inspector::Protocol::Automation::PageLoadStrategy, Seconds, Inspector::CommandCallback<void>&&, RefPtr<API::Navigation>&& = nullptr, SameDocumentNavigation = SameDocumentNavigation::No);
    void waitForHistoryTraversalToComplete(WebPageProxy&, Inspector::Protocol::Automation::PageLoadStrategy, Seconds, Inspector::CommandCallback<void>&&, RefPtr<API::Navigation>&&);
    bool hasPendingNavigationCallbacks() const;
    void waitForNavigationToCompleteOnFrame(WebFrameProxy&, Inspector::Protocol::Automation::PageLoadStrategy, Seconds, Inspector::CommandCallback<void>&&);
    void respondToPendingPageNavigationCallbacksWithTimeout(HashMap<WebPageProxyIdentifier, Vector<PendingPageNavigationCallback>>&);
    void respondToPendingFrameNavigationCallbacksWithTimeout(HashMap<WebCore::FrameIdentifier, Vector<Inspector::CommandCallback<void>>>&);
    void respondToPendingNavigationCallbacksWithSuccess(Vector<Inspector::CommandCallback<void>>&&);
    void loadTimerFired();

    void exitFullscreenWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void restoreWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void maximizeWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);
    void hideWindowForPage(WebPageProxy&, WTF::CompletionHandler<void()>&&);

#if ENABLE(WEBDRIVER_BIDI)
    void recursivelyEmitContextCreatedEvent(const FrameTreeNodeData&, std::optional<RefPtr<StringImpl>>&& parentContext);
    WebPageProxy* getOpenerPage(const WebPageProxy&);
#endif

    // IPC::MessageReceiver (Implemented by generated code in WebAutomationSessionMessageReceiver.cpp).
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    // Called by WebAutomationSession messages.
    void logEntryAdded(const JSC::MessageSource&, const JSC::MessageLevel&, StringImpl* messageText, const JSC::MessageType&, const WallTime&);
    void addKnownNodeReference(WebCore::FrameIdentifier, StringImpl* nodeHandle);
    void isKnownNodeReference(WebCore::FrameIdentifier, StringImpl* nodeHandle, CompletionHandler<void(bool)>&&);
#if ENABLE(WEBDRIVER_BIDI)
    void scriptRealmCreated(WebCore::FrameIdentifier, RealmIdentifier, IPC::Untrusted<WebCore::SecurityOriginData>&&);
    void scriptRealmDestroyed(WebCore::FrameIdentifier, RealmIdentifier);
#endif

    // Platform-dependent implementations.
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    NODELETE void resetMouseState();
    void updateClickCount(MouseButton, const WebCore::IntPoint&, Seconds maxTime = 0.5_s, int maxDistance = 0);
    NODELETE void updateLastPosition(const WebCore::IntPoint&, int maxDistance = 0);
    void platformSimulateMouseInteraction(WebPageProxy&, MouseInteraction, MouseButton, const WebCore::IntPoint& locationInViewport, OptionSet<WebEventModifier>, StringImpl* pointerType);
    NODELETE static OptionSet<WebEventModifier> platformWebModifiersFromRaw(WebPageProxy&, unsigned modifiers);
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    // Simulates a single touch point being pressed, moved, and released.
    void platformSimulateTouchInteraction(WebPageProxy&, TouchInteraction, const WebCore::IntPoint& locationInViewport, std::optional<Seconds> duration, AutomationCompletionHandler&&);
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    // Simulates a single virtual or char key being pressed/released, such as 'a', Control, F-keys, Numpad keys, etc. as allowed by the protocol.
    void platformSimulateKeyboardInteraction(WebPageProxy&, KeyboardInteraction, Variant<VirtualKey, CharKey>&&);
    // Simulates key presses to produce the codepoints in a string. One or more code points are delivered atomically at grapheme cluster boundaries.
    void platformSimulateKeySequence(WebPageProxy&, StringImpl*);
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    void platformSimulateWheelInteraction(WebPageProxy&, const WebCore::IntPoint& locationInViewport, const WebCore::IntSize& delta);
#endif // ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

    // Get base64-encoded PNG data from a bitmap.
    static std::optional<RefPtr<StringImpl>> platformGetBase64EncodedPNGData(WebCore::ShareableBitmap::Handle&&);
    static std::optional<RefPtr<StringImpl>> platformGetBase64EncodedPNGData(const ViewSnapshot&);
#if PLATFORM(COCOA)
    static std::optional<RefPtr<StringImpl>> platformGetBase64EncodedPNGData(CGImageRef);
#endif

    // Save base64-encoded file contents to a local file path and return the path.
    // This reuses the basename of the remote file path so that the filename exposed to DOM API remains the same.
    std::optional<RefPtr<StringImpl>> platformGenerateLocalFilePathForRemoteFile(StringImpl* remoteFilePath, StringImpl* base64EncodedFileContents);

#if PLATFORM(COCOA)
    // The type parameter of the NSArray argument is platform-dependent.
    void sendSynthesizedEventsToPage(WebPageProxy&, NSArray *eventsToSend);

    NODELETE std::optional<unichar> charCodeForVirtualKey(Inspector::Protocol::Automation::VirtualKey) const;
    NODELETE std::optional<unichar> charCodeIgnoringModifiersForVirtualKey(Inspector::Protocol::Automation::VirtualKey) const;

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    // Where the WebDriver key table states a DOM identity for the key, attaches it to each of the
    // synthesized events (see AutomationKeyIdentity.h). The type parameter of the NSArray argument
    // is platform-dependent.
    static void setKeyIdentityForVirtualKeyIfNeeded(NSArray *events, const Variant<VirtualKey, CharKey>&);
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
#endif

    WeakPtr<WebProcessPool> m_processPool;

    std::unique_ptr<API::AutomationSessionClient> m_client;
    RefPtr<StringImpl> m_sessionIdentifier { "Untitled Session"_si };
    const bool m_siteIsolationEnabled;
    const Ref<Inspector::FrontendRouter> m_frontendRouter;
    const Ref<Inspector::BackendDispatcher> m_backendDispatcher;
    const Ref<Inspector::AutomationBackendDispatcher> m_domainDispatcher;
    const UniqueRef<Inspector::AutomationFrontendDispatcher> m_domainNotifier;

#if ENABLE(WEBDRIVER_BIDI)
    const UniqueRef<WebDriverBidiProcessor> m_bidiProcessor;
#endif

    HashMap<WebPageProxyIdentifier, RefPtr<StringImpl>> m_webPageHandleMap;
    HashMap<RefPtr<StringImpl>, WebPageProxyIdentifier> m_handleWebPageMap;

    HashMap<WebCore::FrameIdentifier, RefPtr<StringImpl>> m_webFrameHandleMap;
    HashMap<RefPtr<StringImpl>, WebCore::FrameIdentifier> m_handleWebFrameMap;

    // The authoritative record of every node reference this session has issued, per
    // navigable. It lives here rather than in the web process because a
    // browsing-context-group swap replaces the web process, and a reference minted
    // before the swap must still be reported stale after it.
    //
    // Insertion-ordered so the oldest entries can be dropped once a frame passes
    // maxKnownNodeReferencesPerFrame. Evicting one costs accuracy, not safety: that
    // reference reports "no such element" again instead of "stale element reference".
    HashMap<WebCore::FrameIdentifier, ListHashSet<RefPtr<StringImpl>>> m_knownNodeReferences;

    HashMap<WebPageProxyIdentifier, Vector<Inspector::CommandCallback<void>>> m_pendingCloseBrowsingContextCallbacksPerPage;
    HashMap<WebPageProxyIdentifier, Vector<PendingPageNavigationCallback>> m_pendingNormalNavigationInBrowsingContextCallbacksPerPage;
    HashMap<WebPageProxyIdentifier, Vector<PendingPageNavigationCallback>> m_pendingEagerNavigationInBrowsingContextCallbacksPerPage;
    HashMap<WebCore::FrameIdentifier, Vector<Inspector::CommandCallback<void>>> m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame;
    HashMap<WebCore::FrameIdentifier, Vector<Inspector::CommandCallback<void>>> m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame;
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
    HashMap<uint64_t, Inspector::CommandCallback<RefPtr<StringImpl>>> m_evaluateJavaScriptFunctionCallbacks;
    // Kept apart from m_evaluateJavaScriptFunctionCallbacks because a user prompt must not end a BiDi script.evaluate
    // or script.callFunction early; it keeps running until the script finishes after the prompt is handled.
    HashMap<uint64_t, Inspector::CommandCallback<RefPtr<StringImpl>>> m_evaluateBidiScriptCallbacks;

    enum class WindowTransitionedToState {
        Fullscreen,
        Unfullscreen,
    };
    Function<void(WindowTransitionedToState)> m_windowStateTransitionCallback { };

    RunLoop::Timer m_loadTimer;
    Vector<RefPtr<StringImpl>> m_filesToSelectForFileUpload;

    bool m_permissionForGetUserMedia { true };

#if ENABLE(WEBDRIVER_ACTIONS_API)
    // SimulatedInputDispatcher APIs take a set of input sources. We also intern these
    // so that previous input source state is used as initial state for later commands.
    HashMap<RefPtr<StringImpl>, Ref<SimulatedInputSource>> m_inputSources;
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

    // Web Inspector backend passthrough: clients with the
    // shouldEnableInspectorTesting capability can issue
    // Automation.sendInspectorMessage to forward an Inspector backend
    // command into a page's WebPageInspectorController; the controller's
    // responses and events come back as Automation.receiveInspectorMessage.
    // Each connected page owns its own InspectorPassthroughChannel so the
    // outgoing event can name its source browsing context.
#if ENABLE(REMOTE_INSPECTOR)
    void sendInspectorMessageToClient(StringImpl* browsingContextHandle, StringImpl* message);
    void disconnectInspectorPassthroughChannel(WebPageProxyIdentifier);
    void disconnectAllInspectorPassthroughChannels();

    HashMap<WebPageProxyIdentifier, std::unique_ptr<InspectorPassthroughChannel>> m_inspectorPassthroughChannels;
#endif

};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebAutomationSession)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::AutomationSession; }
SPECIALIZE_TYPE_TRAITS_END()
