
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

#include "config.h"
#include "WebAutomationSession.h"

#include "APIArray.h"
#include "APIAutomationSessionClient.h"
#include "APIHTTPCookieStore.h"
#include "APINavigation.h"
#include "APIOpenPanelParameters.h"
#include "AutomationProtocolObjects.h"
#include "CoordinateSystem.h"
#include "WebAutomationSessionMacros.h"
#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebFullScreenManagerProxy.h"
#include "WebInspectorUIProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/PointerEventTypeNames.h>
#include <algorithm>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/StringConcatenate.h>

#if ENABLE(WEB_AUTHN)
#include "VirtualAuthenticatorManager.h"
#include <WebCore/AuthenticatorTransport.h>
#endif // ENABLE(WEB_AUTHN)

namespace WebKit {

using namespace Inspector;
using namespace WebCore;

String AutomationCommandError::toProtocolString()
{
    String protocolErrorName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(type);
    if (!message)
        return protocolErrorName;
    return makeString(protocolErrorName, errorNameAndDetailsSeparator, *message);
}
    
// §8. Sessions
// https://www.w3.org/TR/webdriver/#dfn-session-page-load-timeout
static const Seconds defaultPageLoadTimeout = 300_s;
// https://www.w3.org/TR/webdriver/#dfn-page-loading-strategy
static const Inspector::Protocol::Automation::PageLoadStrategy defaultPageLoadStrategy = Inspector::Protocol::Automation::PageLoadStrategy::Normal;

WebAutomationSession::WebAutomationSession()
    : m_client(makeUnique<API::AutomationSessionClient>())
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_domainDispatcher(AutomationBackendDispatcher::create(m_backendDispatcher, this))
    , m_domainNotifier(makeUnique<AutomationFrontendDispatcher>(m_frontendRouter))
    , m_loadTimer(RunLoop::main(), this, &WebAutomationSession::loadTimerFired)
{
}

WebAutomationSession::~WebAutomationSession()
{
    ASSERT(!m_client);
    ASSERT(!m_processPool);
}

void WebAutomationSession::setClient(std::unique_ptr<API::AutomationSessionClient>&& client)
{
    m_client = WTFMove(client);
}

void WebAutomationSession::setProcessPool(WebKit::WebProcessPool* processPool)
{
    if (m_processPool)
        m_processPool->removeMessageReceiver(Messages::WebAutomationSession::messageReceiverName());

    m_processPool = processPool;

    if (m_processPool)
        m_processPool->addMessageReceiver(Messages::WebAutomationSession::messageReceiverName(), *this);
}

// NOTE: this class could be split at some point to support local and remote automation sessions.
// For now, it only works with a remote automation driver over a RemoteInspector connection.

#if ENABLE(REMOTE_INSPECTOR)

// Inspector::RemoteAutomationTarget API

void WebAutomationSession::dispatchMessageFromRemote(String&& message)
{
    m_backendDispatcher->dispatch(WTFMove(message));
}

void WebAutomationSession::connect(Inspector::FrontendChannel& channel, bool isAutomaticConnection, bool immediatelyPause)
{
    UNUSED_PARAM(isAutomaticConnection);
    UNUSED_PARAM(immediatelyPause);

    m_remoteChannel = &channel;
    m_frontendRouter->connectFrontend(channel);

    setIsPaired(true);
}

void WebAutomationSession::disconnect(Inspector::FrontendChannel& channel)
{
    ASSERT(&channel == m_remoteChannel);
    terminate();
}

#endif // ENABLE(REMOTE_INSPECTOR)

void WebAutomationSession::terminate()
{
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    for (auto& identifier : copyToVector(m_pendingKeyboardEventsFlushedCallbacksPerPage.keys())) {
        auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(identifier);
        callback(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
    }
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    for (auto& identifier : copyToVector(m_pendingMouseEventsFlushedCallbacksPerPage.keys())) {
        auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(identifier);
        callback(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
    }
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    for (auto& identifier : copyToVector(m_pendingWheelEventsFlushedCallbacksPerPage.keys())) {
        auto callback = m_pendingWheelEventsFlushedCallbacksPerPage.take(identifier);
        callback(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
    }
#endif // ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

#if ENABLE(REMOTE_INSPECTOR)
    if (Inspector::FrontendChannel* channel = m_remoteChannel) {
        m_remoteChannel = nullptr;
        m_frontendRouter->disconnectFrontend(*channel);
    }

    setIsPaired(false);
#endif

    if (m_client)
        m_client->didDisconnectFromRemote(*this);
}

WebPageProxy* WebAutomationSession::webPageProxyForHandle(const String& handle)
{
    auto iter = m_handleWebPageMap.find(handle);
    if (iter == m_handleWebPageMap.end())
        return nullptr;
    return WebProcessProxy::webPage(iter->value);
}

String WebAutomationSession::handleForWebPageProxy(const WebPageProxy& webPageProxy)
{
    auto iter = m_webPageHandleMap.find(webPageProxy.identifier());
    if (iter != m_webPageHandleMap.end())
        return iter->value;

    String handle = makeString("page-"_s, asASCIIUppercase(createVersion4UUIDString()));

    auto firstAddResult = m_webPageHandleMap.add(webPageProxy.identifier(), handle);
    RELEASE_ASSERT(firstAddResult.isNewEntry);

    auto secondAddResult = m_handleWebPageMap.add(handle, webPageProxy.identifier());
    RELEASE_ASSERT(secondAddResult.isNewEntry);

    return handle;
}

void WebAutomationSession::didDestroyFrame(FrameIdentifier frameID)
{
    auto handle = m_webFrameHandleMap.take(frameID);
    if (!handle.isEmpty())
        m_handleWebFrameMap.remove(handle);
}

std::optional<FrameIdentifier> WebAutomationSession::webFrameIDForHandle(const String& handle, bool& frameNotFound)
{
    if (handle.isEmpty())
        return std::nullopt;

    auto iter = m_handleWebFrameMap.find(handle);
    if (iter == m_handleWebFrameMap.end()) {
        frameNotFound = true;
        return std::nullopt;
    }

    return iter->value;
}

String WebAutomationSession::handleForWebFrameID(std::optional<FrameIdentifier> frameID)
{
    if (!frameID || !*frameID)
        return emptyString();

    if (auto* frame = WebFrameProxy::webFrame(*frameID); frame && frame->isMainFrame())
        return emptyString();

    auto iter = m_webFrameHandleMap.find(*frameID);
    if (iter != m_webFrameHandleMap.end())
        return iter->value;

    String handle = makeString("frame-"_s, asASCIIUppercase(createVersion4UUIDString()));

    auto firstAddResult = m_webFrameHandleMap.add(*frameID, handle);
    RELEASE_ASSERT(firstAddResult.isNewEntry);

    auto secondAddResult = m_handleWebFrameMap.add(handle, *frameID);
    RELEASE_ASSERT(secondAddResult.isNewEntry);

    return handle;
}

String WebAutomationSession::handleForWebFrameProxy(const WebFrameProxy& webFrameProxy)
{
    return handleForWebFrameID(webFrameProxy.frameID());
}

Ref<Inspector::Protocol::Automation::BrowsingContext> WebAutomationSession::buildBrowsingContextForPage(WebPageProxy& page, WebCore::FloatRect windowFrame)
{
    auto originObject = Inspector::Protocol::Automation::Point::create()
        .setX(windowFrame.x())
        .setY(windowFrame.y())
        .release();

    auto sizeObject = Inspector::Protocol::Automation::Size::create()
        .setWidth(windowFrame.width())
        .setHeight(windowFrame.height())
        .release();

    bool isActive = page.isViewVisible() && page.isViewFocused() && page.isViewWindowActive();
    String handle = handleForWebPageProxy(page);

    return Inspector::Protocol::Automation::BrowsingContext::create()
        .setHandle(handle)
        .setActive(isActive)
        .setUrl(page.pageLoadState().activeURL())
        .setWindowOrigin(WTFMove(originObject))
        .setWindowSize(WTFMove(sizeObject))
        .release();
}

// Platform-independent Commands.

void WebAutomationSession::getNextContext(Ref<WebAutomationSession>&& protectedThis, Vector<Ref<WebPageProxy>>&& pages, Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>> contexts, Ref<WebAutomationSession::GetBrowsingContextsCallback>&& callback)
{
    if (pages.isEmpty()) {
        callback->sendSuccess(WTFMove(contexts));
        return;
    }
    auto page = pages.takeLast();
    auto& webPageProxy = page.get();
    webPageProxy.getWindowFrameWithCallback([this, protectedThis = WTFMove(protectedThis), callback = WTFMove(callback), pages = WTFMove(pages), contexts = WTFMove(contexts), page = WTFMove(page)](WebCore::FloatRect windowFrame) mutable {
        contexts->addItem(protectedThis->buildBrowsingContextForPage(page.get(), windowFrame));
        getNextContext(WTFMove(protectedThis), WTFMove(pages), WTFMove(contexts), WTFMove(callback));
    });
}
    
void WebAutomationSession::getBrowsingContexts(Ref<GetBrowsingContextsCallback>&& callback)
{
    Vector<Ref<WebPageProxy>> pages;
    for (auto& process : m_processPool->processes()) {
        for (auto* page : process->pages()) {
            ASSERT(page);
            if (!page->isControlledByAutomation())
                continue;
            pages.append(*page);
        }
    }
    
    getNextContext(Ref { *this }, WTFMove(pages), JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>::create(), WTFMove(callback));
}

void WebAutomationSession::getBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, Ref<GetBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    page->getWindowFrameWithCallback([protectedThis = Ref { *this }, page = Ref { *page }, callback = WTFMove(callback)](WebCore::FloatRect windowFrame) mutable {
        callback->sendSuccess(protectedThis->buildBrowsingContextForPage(page.get(), windowFrame));
    });
}

static Inspector::Protocol::Automation::BrowsingContextPresentation toProtocol(API::AutomationSessionClient::BrowsingContextPresentation value)
{
    switch (value) {
    case API::AutomationSessionClient::BrowsingContextPresentation::Tab:
        return Inspector::Protocol::Automation::BrowsingContextPresentation::Tab;
    case API::AutomationSessionClient::BrowsingContextPresentation::Window:
        return Inspector::Protocol::Automation::BrowsingContextPresentation::Window;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void WebAutomationSession::createBrowsingContext(std::optional<Inspector::Protocol::Automation::BrowsingContextPresentation>&& presentationHint, Ref<CreateBrowsingContextCallback>&& callback)
{
    ASSERT(m_client);
    if (!m_client)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote session could not request a new browsing context.");

    uint16_t options = 0;

    if (presentationHint == Inspector::Protocol::Automation::BrowsingContextPresentation::Tab)
        options |= API::AutomationSessionBrowsingContextOptionsPreferNewTab;

    m_client->requestNewPageWithOptions(*this, static_cast<API::AutomationSessionBrowsingContextOptions>(options), [protectedThis = Ref { *this }, callback = WTFMove(callback)](WebPageProxy* page) {
        if (!page)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote session failed to create a new browsing context.");

        // WebDriver allows running commands in a browsing context which has not done any loads yet. Force WebProcess to be created so it can receive messages.
        page->launchInitialProcessIfNecessary();
        callback->sendSuccess(protectedThis->handleForWebPageProxy(*page), toProtocol(protectedThis->m_client->currentPresentationOfPage(protectedThis.get(), *page)));
    });
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::closeBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    page->closePage();

    return { };
}

void WebAutomationSession::switchToBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, Ref<SwitchToBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    // FIXME: Should we also switch to the frame that frameHandle points to?
    m_client->requestSwitchToPage(*this, *page, [page = Ref { *page }, callback = WTFMove(callback)]() {
        page->setFocus(true);

        callback->sendSuccess();
    });
}

void WebAutomationSession::setWindowFrameOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, RefPtr<JSON::Object>&& origin, RefPtr<JSON::Object>&& size, Ref<SetWindowFrameOfBrowsingContextCallback>&& callback)
{
    std::optional<double> x;
    std::optional<double> y;
    if (origin) {
        x = origin->getDouble("x"_s);
        if (!x)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'x' parameter was not found or invalid.");

        y = origin->getDouble("y"_s);
        if (!y)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'y' parameter was not found or invalid.");
    }

    std::optional<double> width;
    std::optional<double> height;
    if (size) {
        width = size->getDouble("width"_s);
        if (!width)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'width' parameter was not found or invalid.");

        height = size->getDouble("height"_s);
        if (!height)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'height' parameter was not found or invalid.");

        if (width.value() < 0)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'width' parameter had an invalid value.");

        if (height.value() < 0)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'height' parameter had an invalid value.");
    }

    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    exitFullscreenWindowForPage(*page, [this, protectedThis = Ref { *this }, callback = WTFMove(callback), page = RefPtr { page }, width, height, x, y]() mutable {
        auto& webPage = *page;
        this->restoreWindowForPage(webPage, [callback = WTFMove(callback), page = WTFMove(page), width, height, x, y]() mutable {
            auto& webPage = *page;
            webPage.getWindowFrameWithCallback([callback = WTFMove(callback), page = WTFMove(page), width, height, x, y](WebCore::FloatRect originalFrame) mutable {
                WebCore::FloatRect newFrame = WebCore::FloatRect(WebCore::FloatPoint(x.value_or(originalFrame.location().x()), y.value_or(originalFrame.location().y())), WebCore::FloatSize(width.value_or(originalFrame.size().width()), height.value_or(originalFrame.size().height())));
                if (newFrame != originalFrame)
                    page->setWindowFrame(newFrame);
                
                callback->sendSuccess();
            });
        });
    });
}

void WebAutomationSession::waitForNavigationToComplete(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& optionalFrameHandle, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, Ref<WaitForNavigationToCompleteCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    // If page is loading and there's an active JavaScript dialog is probably because the
    // dialog was started in an onload handler, so in case of normal page load strategy the
    // load will not finish until the dialog is dismissed. Instead of waiting for the timeout,
    // we return without waiting since we know it will timeout for sure. We want to check
    // arguments first, though.
    bool shouldTimeoutDueToUnexpectedAlert = pageLoadStrategy == Inspector::Protocol::Automation::PageLoadStrategy::Normal
        && page->pageLoadState().isLoading() && m_client->isShowingJavaScriptDialogOnPage(*this, *page);

    if (!optionalFrameHandle.isEmpty()) {
        bool frameNotFound = false;
        auto frameID = webFrameIDForHandle(optionalFrameHandle, frameNotFound);
        if (frameNotFound)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);
        WebFrameProxy* frame = WebFrameProxy::webFrame(frameID.value());
        if (!frame)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);
        if (!shouldTimeoutDueToUnexpectedAlert)
            waitForNavigationToCompleteOnFrame(*frame, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
    } else {
        if (!shouldTimeoutDueToUnexpectedAlert)
            waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
    }

    if (shouldTimeoutDueToUnexpectedAlert) {
        // §9 Navigation.
        // 7. If the previous step completed by the session page load timeout being reached and the browser does not
        // have an active user prompt, return error with error code timeout.
        // 8. Return success with data null.
        // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-wait-for-navigation-to-complete
        callback->sendSuccess();
    }
}

void WebAutomationSession::waitForNavigationToCompleteOnPage(WebPageProxy& page, Inspector::Protocol::Automation::PageLoadStrategy loadStrategy, Seconds timeout, Ref<Inspector::BackendDispatcher::CallbackBase>&& callback)
{
    ASSERT(!m_loadTimer.isActive());
    if (loadStrategy == Inspector::Protocol::Automation::PageLoadStrategy::None || (!page.pageLoadState().isLoading() && !page.pageLoadState().hasUncommittedLoad())) {
        callback->sendSuccess(JSON::Object::create());
        return;
    }

    m_loadTimer.startOneShot(timeout);
    switch (loadStrategy) {
    case Inspector::Protocol::Automation::PageLoadStrategy::Normal:
        m_pendingNormalNavigationInBrowsingContextCallbacksPerPage.set(page.identifier(), WTFMove(callback));
        break;
    case Inspector::Protocol::Automation::PageLoadStrategy::Eager:
        m_pendingEagerNavigationInBrowsingContextCallbacksPerPage.set(page.identifier(), WTFMove(callback));
        break;
    case Inspector::Protocol::Automation::PageLoadStrategy::None:
        ASSERT_NOT_REACHED();
    }
}

void WebAutomationSession::waitForNavigationToCompleteOnFrame(WebFrameProxy& frame, Inspector::Protocol::Automation::PageLoadStrategy loadStrategy, Seconds timeout, Ref<Inspector::BackendDispatcher::CallbackBase>&& callback)
{
    ASSERT(!m_loadTimer.isActive());
    if (loadStrategy == Inspector::Protocol::Automation::PageLoadStrategy::None || frame.frameLoadState().state() == FrameLoadState::State::Finished) {
        callback->sendSuccess(JSON::Object::create());
        return;
    }

    m_loadTimer.startOneShot(timeout);
    switch (loadStrategy) {
    case Inspector::Protocol::Automation::PageLoadStrategy::Normal:
        m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame.set(frame.frameID(), WTFMove(callback));
        break;
    case Inspector::Protocol::Automation::PageLoadStrategy::Eager:
        m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame.set(frame.frameID(), WTFMove(callback));
        break;
    case Inspector::Protocol::Automation::PageLoadStrategy::None:
        ASSERT_NOT_REACHED();
    }
}

void WebAutomationSession::respondToPendingPageNavigationCallbacksWithTimeout(HashMap<WebPageProxyIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>>& map)
{
    auto timeoutError = STRING_FOR_PREDEFINED_ERROR_NAME(Timeout);
    for (auto id : copyToVector(map.keys())) {
        auto page = WebProcessProxy::webPage(id);
        auto callback = map.take(id);
        if (page && m_client->isShowingJavaScriptDialogOnPage(*this, *page))
            callback->sendSuccess(JSON::Object::create());
        else
            callback->sendFailure(timeoutError);
    }
}

static WebPageProxy* findPageForFrameID(const WebProcessPool& processPool, FrameIdentifier frameID)
{
    if (auto* frame = WebFrameProxy::webFrame(frameID))
        return frame->page();
    return nullptr;
}

void WebAutomationSession::respondToPendingFrameNavigationCallbacksWithTimeout(HashMap<FrameIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>>& map)
{
    auto timeoutError = STRING_FOR_PREDEFINED_ERROR_NAME(Timeout);
    for (auto id : copyToVector(map.keys())) {
        auto* page = findPageForFrameID(*m_processPool, id);
        auto callback = map.take(id);
        if (page && m_client->isShowingJavaScriptDialogOnPage(*this, *page))
            callback->sendSuccess(JSON::Object::create());
        else
            callback->sendFailure(timeoutError);
    }
}

void WebAutomationSession::loadTimerFired()
{
    respondToPendingFrameNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame);
    respondToPendingFrameNavigationCallbacksWithTimeout(m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame);
    respondToPendingPageNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerPage);
    respondToPendingPageNavigationCallbacksWithTimeout(m_pendingEagerNavigationInBrowsingContextCallbacksPerPage);
}

void WebAutomationSession::maximizeWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, Ref<MaximizeWindowOfBrowsingContextCallback>&& callback)
{
    auto* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    exitFullscreenWindowForPage(*page, [this, protectedThis = Ref { *this }, callback = WTFMove(callback), page = RefPtr { page }]() mutable {
        auto& webPage = *page;
        restoreWindowForPage(webPage, [this, callback = WTFMove(callback), page = WTFMove(page)]() mutable {
            maximizeWindowForPage(*page, [callback = WTFMove(callback)]() {
                callback->sendSuccess();
            });
        });
    });
}

void WebAutomationSession::hideWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, Ref<HideWindowOfBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);
    
    exitFullscreenWindowForPage(*page, [protectedThis = Ref { *this }, callback = WTFMove(callback), page = RefPtr { page }]() mutable {
        protectedThis->hideWindowForPage(*page, [callback = WTFMove(callback)]() mutable {
            callback->sendSuccess();
        });
    });
}

void WebAutomationSession::exitFullscreenWindowForPage(WebPageProxy& page, WTF::CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(FULLSCREEN_API)
    ASSERT(!m_windowStateTransitionCallback);
    if (!page.fullScreenManager() || !page.fullScreenManager()->isFullScreen()) {
        completionHandler();
        return;
    }
    
    m_windowStateTransitionCallback = WTF::Function<void(WindowTransitionedToState)> { [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](WindowTransitionedToState state) mutable {
        // If fullscreen exited and we didn't request that, just ignore it.
        if (state != WindowTransitionedToState::Unfullscreen)
            return;

        // Keep this callback in scope so completionHandler does not get destroyed before we call it.
        auto protectedCallback = WTFMove(m_windowStateTransitionCallback);
        completionHandler();
    } };
    
    page.fullScreenManager()->requestExitFullScreen();
#else
    completionHandler();
#endif
}

void WebAutomationSession::restoreWindowForPage(WebPageProxy& page, WTF::CompletionHandler<void()>&& completionHandler)
{
    m_client->requestRestoreWindowOfPage(*this, page, WTFMove(completionHandler));
}

void WebAutomationSession::maximizeWindowForPage(WebPageProxy& page, WTF::CompletionHandler<void()>&& completionHandler)
{
    m_client->requestMaximizeWindowOfPage(*this, page, WTFMove(completionHandler));
}

void WebAutomationSession::hideWindowForPage(WebPageProxy& page, WTF::CompletionHandler<void()>&& completionHandler)
{
    m_client->requestHideWindowOfPage(*this, page, WTFMove(completionHandler));
}

void WebAutomationSession::willShowJavaScriptDialog(WebPageProxy& page)
{
    // Wait until the next run loop iteration to give time for the client to show the dialog,
    // then check if the dialog is still present. If the page is loading, the dialog will block
    // the load in case of normal strategy, so we want to dispatch all pending navigation callbacks.
    // If the dialog was shown during a script execution, we want to finish the evaluateJavaScriptFunction
    // operation with an unexpected alert open error.
    RunLoop::main().dispatch([this, protectedThis = Ref { *this }, page = Ref { page }] {
        if (!page->hasRunningProcess() || !m_client || !m_client->isShowingJavaScriptDialogOnPage(*this, page))
            return;

        if (page->pageLoadState().isLoading()) {
            m_loadTimer.stop();
            respondToPendingFrameNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame);
            respondToPendingPageNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerPage);
        }

        if (!m_evaluateJavaScriptFunctionCallbacks.isEmpty()) {
            for (auto key : copyToVector(m_evaluateJavaScriptFunctionCallbacks.keys())) {
                auto callback = m_evaluateJavaScriptFunctionCallbacks.take(key);
                callback->sendSuccess("null"_s);
            }
        }

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        if (!m_pendingMouseEventsFlushedCallbacksPerPage.isEmpty()) {
            for (auto key : copyToVector(m_pendingMouseEventsFlushedCallbacksPerPage.keys())) {
                auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(key);
                callback(std::nullopt);
            }
        }
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        if (!m_pendingKeyboardEventsFlushedCallbacksPerPage.isEmpty()) {
            for (auto key : copyToVector(m_pendingKeyboardEventsFlushedCallbacksPerPage.keys())) {
                auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(key);
                callback(std::nullopt);
            }
        }
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    });

#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
        if (!m_pendingWheelEventsFlushedCallbacksPerPage.isEmpty()) {
            for (auto key : copyToVector(m_pendingWheelEventsFlushedCallbacksPerPage.keys())) {
                auto callback = m_pendingWheelEventsFlushedCallbacksPerPage.take(key);
                callback(std::nullopt);
            }
        }
#endif // ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
}
    
void WebAutomationSession::didEnterFullScreenForPage(const WebPageProxy&)
{
    if (m_windowStateTransitionCallback)
        m_windowStateTransitionCallback(WindowTransitionedToState::Fullscreen);
}

void WebAutomationSession::didExitFullScreenForPage(const WebPageProxy&)
{
    if (m_windowStateTransitionCallback)
        m_windowStateTransitionCallback(WindowTransitionedToState::Unfullscreen);
}

void WebAutomationSession::navigateBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, const String& url, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, Ref<NavigateBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->loadRequest(URL { url });
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::goBackInBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, Ref<GoBackInBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->goBack();
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::goForwardInBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, Ref<GoForwardInBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->goForward();
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::reloadBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, Ref<ReloadBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->reload({ });
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::navigationOccurredForFrame(const WebFrameProxy& frame)
{
    if (frame.isMainFrame()) {
        // New page loaded, clear frame handles previously cached for frame's page.
        HashSet<String> handlesToRemove;
        for (const auto& iter : m_handleWebFrameMap) {
            RefPtr webFrame = WebFrameProxy::webFrame(iter.value);
            if (webFrame && webFrame->page() == frame.page()) {
                handlesToRemove.add(iter.key);
                m_webFrameHandleMap.remove(iter.value);
            }
        }
        m_handleWebFrameMap.removeIf([&](auto& iter) {
            return handlesToRemove.contains(iter.key);
        });

        if (auto callback = m_pendingNormalNavigationInBrowsingContextCallbacksPerPage.take(frame.page()->identifier())) {
            m_loadTimer.stop();
            callback->sendSuccess(JSON::Object::create());
        }
        m_domainNotifier->browsingContextCleared(handleForWebPageProxy(*frame.page()));
    } else {
        if (auto callback = m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame.take(frame.frameID())) {
            m_loadTimer.stop();
            callback->sendSuccess(JSON::Object::create());
        }
    }
}

void WebAutomationSession::documentLoadedForFrame(const WebFrameProxy& frame)
{
    if (frame.isMainFrame()) {
        if (auto callback = m_pendingEagerNavigationInBrowsingContextCallbacksPerPage.take(frame.page()->identifier())) {
            m_loadTimer.stop();
            callback->sendSuccess(JSON::Object::create());
        }

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        resetClickCount();
#endif
    } else {
        if (auto callback = m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame.take(frame.frameID())) {
            m_loadTimer.stop();
            callback->sendSuccess(JSON::Object::create());
        }
    }
}

void WebAutomationSession::inspectorFrontendLoaded(const WebPageProxy& page)
{
    if (auto callback = m_pendingInspectorCallbacksPerPage.take(page.identifier()))
        callback->sendSuccess(JSON::Object::create());
}

void WebAutomationSession::mouseEventsFlushedForPage(const WebPageProxy& page)
{
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    if (auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback(std::nullopt);
#else
    UNUSED_PARAM(page);
#endif
}

void WebAutomationSession::keyboardEventsFlushedForPage(const WebPageProxy& page)
{
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    if (auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback(std::nullopt);
#else
    UNUSED_PARAM(page);
#endif
}

void WebAutomationSession::wheelEventsFlushedForPage(const WebPageProxy& page)
{
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    if (auto callback = m_pendingWheelEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback(std::nullopt);
#else
    UNUSED_PARAM(page);
#endif
}

void WebAutomationSession::willClosePage(const WebPageProxy& page)
{
    String handle = handleForWebPageProxy(page);
    m_domainNotifier->browsingContextCleared(handle);

    // Cancel pending interactions on this page. By providing an error, this will cause subsequent
    // actions to be aborted and the SimulatedInputDispatcher::run() call will unwind and fail.
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    if (auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback(AUTOMATION_COMMAND_ERROR_WITH_NAME(WindowNotFound));
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    if (auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback(AUTOMATION_COMMAND_ERROR_WITH_NAME(WindowNotFound));
#endif
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    if (auto callback = m_pendingWheelEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback(AUTOMATION_COMMAND_ERROR_WITH_NAME(WindowNotFound));
#endif

#if ENABLE(WEBDRIVER_ACTIONS_API)
    // Then tell the input dispatcher to cancel so timers are stopped, and let it go out of scope.
    if (auto inputDispatcher = m_inputDispatchersByPage.take(page.identifier()))
        inputDispatcher->cancel();
#endif
}

static bool fileCanBeAcceptedForUpload(const String& filename, const HashSet<String>& allowedMIMETypes, const HashSet<String>& allowedFileExtensions)
{
    if (!FileSystem::fileExists(filename))
        return false;

    if (allowedMIMETypes.isEmpty() && allowedFileExtensions.isEmpty())
        return true;

    // We can't infer a MIME type from a file without an extension, just give up.
    auto dotOffset = filename.reverseFind('.');
    if (dotOffset == notFound)
        return false;

    String extension = filename.substring(dotOffset + 1).convertToASCIILowercase();
    if (extension.isEmpty())
        return false;

    if (allowedFileExtensions.contains(extension))
        return true;

    String mappedMIMEType = WebCore::MIMETypeRegistry::mimeTypeForExtension(extension).convertToASCIILowercase();
    if (mappedMIMEType.isEmpty())
        return false;
    
    if (allowedMIMETypes.contains(mappedMIMEType))
        return true;

    // Fall back to checking for a MIME type wildcard if an exact match is not found.
    Vector<String> components = mappedMIMEType.split('/');
    if (components.size() != 2)
        return false;

    auto wildcardedMIMEType = makeString(components[0], "/*");
    if (allowedMIMETypes.contains(wildcardedMIMEType))
        return true;

    return false;
}

void WebAutomationSession::handleRunOpenPanel(const WebPageProxy& page, const WebFrameProxy&, const API::OpenPanelParameters& parameters, WebOpenPanelResultListenerProxy& resultListener)
{
    String browsingContextHandle = handleForWebPageProxy(page);
    if (!m_filesToSelectForFileUpload.size()) {
        resultListener.cancel();
        m_domainNotifier->fileChooserDismissed(browsingContextHandle, true, { });
        return;
    }

    if (m_filesToSelectForFileUpload.size() > 1 && !parameters.allowMultipleFiles()) {
        resultListener.cancel();
        m_domainNotifier->fileChooserDismissed(browsingContextHandle, true, { });
        return;
    }

    HashSet<String> allowedMIMETypes;
    auto acceptMIMETypes = parameters.acceptMIMETypes();
    for (auto type : acceptMIMETypes->elementsOfType<API::String>())
        allowedMIMETypes.add(type->string());

    HashSet<String> allowedFileExtensions;
    auto acceptFileExtensions = parameters.acceptFileExtensions();
    for (auto type : acceptFileExtensions->elementsOfType<API::String>()) {
        // WebCore vends extensions with leading periods. Strip these to simplify matching later.
        String extension = type->string();
        ASSERT(extension.characterAt(0) == '.');
        allowedFileExtensions.add(extension.substring(1));
    }

    // Per §14.3.10.5 in the W3C spec, if at least one file cannot be accepted, the command should fail.
    for (const String& filename : m_filesToSelectForFileUpload) {
        if (!fileCanBeAcceptedForUpload(filename, allowedMIMETypes, allowedFileExtensions)) {
            resultListener.cancel();
            m_domainNotifier->fileChooserDismissed(browsingContextHandle, true, { });
            return;
        }
    }

    // Copy the file list we used before calling out to the open panel listener.
    Ref<JSON::ArrayOf<String>> selectedFiles = JSON::ArrayOf<String>::create();
    for (const String& filename : m_filesToSelectForFileUpload)
        selectedFiles->addItem(filename);

    resultListener.chooseFiles(m_filesToSelectForFileUpload);

    m_domainNotifier->fileChooserDismissed(browsingContextHandle, false, WTFMove(selectedFiles));
}

void WebAutomationSession::evaluateJavaScriptFunction(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const String& function, Ref<JSON::Array>&& arguments, std::optional<bool>&& expectsImplicitCallbackArgument, std::optional<double>&& callbackTimeout, Ref<EvaluateJavaScriptFunctionCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    Vector<String> argumentsVector;
    argumentsVector.reserveCapacity(arguments->length());

    for (const auto& argument : arguments.get())
        argumentsVector.uncheckedAppend(argument->asString());

    uint64_t callbackID = m_nextEvaluateJavaScriptCallbackID++;
    m_evaluateJavaScriptFunctionCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::EvaluateJavaScriptFunction(page->webPageID(), frameID, function, argumentsVector, expectsImplicitCallbackArgument.value_or(false), WTFMove(callbackTimeout), callbackID), 0);
}

void WebAutomationSession::didEvaluateJavaScriptFunction(uint64_t callbackID, const String& result, const String& errorType)
{
    auto callback = m_evaluateJavaScriptFunctionCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty())
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE_AND_DETAILS(errorType, result));
    else
        callback->sendSuccess(result);
}

void WebAutomationSession::resolveChildFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, std::optional<int>&& optionalOrdinal, const String& optionalName, const Inspector::Protocol::Automation::NodeHandle& optionalNodeHandle, Ref<ResolveChildFrameHandleCallback>&& callback)
{
    if (!optionalOrdinal && !optionalName && !optionalNodeHandle)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "Command must specify a child frame by ordinal, name, or element handle.");

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>, std::optional<FrameIdentifier>)> completionHandler = [this, protectedThis = Ref { *this }, callback](std::optional<String> errorType, std::optional<FrameIdentifier> frameID) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess(handleForWebFrameID(frameID));
    };

    if (!!optionalNodeHandle) {
        page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ResolveChildFrameWithNodeHandle(page->webPageID(), frameID, optionalNodeHandle), WTFMove(completionHandler));
        return;
    }

    if (!!optionalName) {
        page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ResolveChildFrameWithName(page->webPageID(), frameID, optionalName), WTFMove(completionHandler));
        return;
    }

    if (optionalOrdinal) {
        page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ResolveChildFrameWithOrdinal(page->webPageID(), frameID, *optionalOrdinal), WTFMove(completionHandler));
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebAutomationSession::resolveParentFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, Ref<ResolveParentFrameHandleCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>, std::optional<FrameIdentifier>)> completionHandler = [this, protectedThis = Ref { *this }, callback](std::optional<String> errorType, std::optional<FrameIdentifier> frameID) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess(handleForWebFrameID(frameID));
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ResolveParentFrame(page->webPageID(), frameID), WTFMove(completionHandler));
}

static std::optional<CoordinateSystem> protocolStringToCoordinateSystem(Inspector::Protocol::Automation::CoordinateSystem coordinateSystem)
{
    switch (coordinateSystem) {
    case Inspector::Protocol::Automation::CoordinateSystem::Page:
        return CoordinateSystem::Page;

    case Inspector::Protocol::Automation::CoordinateSystem::Viewport:
    case Inspector::Protocol::Automation::CoordinateSystem::LayoutViewport:
        return CoordinateSystem::LayoutViewport;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

void WebAutomationSession::computeElementLayout(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, std::optional<bool>&& optionalScrollIntoViewIfNeeded, Inspector::Protocol::Automation::CoordinateSystem coordinateSystemValue, Ref<ComputeElementLayoutCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    std::optional<CoordinateSystem> coordinateSystem = protocolStringToCoordinateSystem(coordinateSystemValue);
    if (!coordinateSystem)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'coordinateSystem' is invalid.");

    WTF::CompletionHandler<void(std::optional<String>, WebCore::IntRect, std::optional<WebCore::IntPoint>, bool)> completionHandler = [callback](std::optional<String> errorType, WebCore::IntRect rect, std::optional<WebCore::IntPoint> inViewCenterPoint, bool isObscured) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        auto originObject = Inspector::Protocol::Automation::Point::create()
            .setX(rect.x())
            .setY(rect.y())
            .release();

        auto sizeObject = Inspector::Protocol::Automation::Size::create()
            .setWidth(rect.width())
            .setHeight(rect.height())
            .release();

        auto rectObject = Inspector::Protocol::Automation::Rect::create()
            .setOrigin(WTFMove(originObject))
            .setSize(WTFMove(sizeObject))
            .release();

        if (!inViewCenterPoint) {
            callback->sendSuccess(WTFMove(rectObject), nullptr, isObscured);
            return;
        }

        auto inViewCenterPointObject = Inspector::Protocol::Automation::Point::create()
            .setX(inViewCenterPoint.value().x())
            .setY(inViewCenterPoint.value().y())
            .release();

        callback->sendSuccess(WTFMove(rectObject), WTFMove(inViewCenterPointObject), isObscured);
    };

    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ComputeElementLayout(page->webPageID(), frameID, nodeHandle, scrollIntoViewIfNeeded, coordinateSystem.value()), WTFMove(completionHandler));
}

void WebAutomationSession::getComputedRole(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, Ref<GetComputedRoleCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>, std::optional<String>)> completionHandler = [callback](std::optional<String> errorType, std::optional<String> role) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess(*role);
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::GetComputedRole(page->webPageID(), frameID, nodeHandle), WTFMove(completionHandler));
}

void WebAutomationSession::getComputedLabel(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, Ref<GetComputedLabelCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>, std::optional<String>)> completionHandler = [callback](std::optional<String> errorType, std::optional<String> label) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess(*label);
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::GetComputedLabel(page->webPageID(), frameID, nodeHandle), WTFMove(completionHandler));
}

void WebAutomationSession::selectOptionElement(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, Ref<SelectOptionElementCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>)> completionHandler = [callback](std::optional<String> errorType) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }
        
        callback->sendSuccess();
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::SelectOptionElement(page->webPageID(), frameID, nodeHandle), WTFMove(completionHandler));
}

Inspector::Protocol::ErrorStringOr<bool> WebAutomationSession::isShowingJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    return m_client->isShowingJavaScriptDialogOnPage(*this, *page);
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::dismissCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        SYNC_FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    m_client->dismissCurrentJavaScriptDialogOnPage(*this, *page);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::acceptCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        SYNC_FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    m_client->acceptCurrentJavaScriptDialogOnPage(*this, *page);

    return { };
}

Inspector::Protocol::ErrorStringOr<String> WebAutomationSession::messageOfCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        SYNC_FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    return m_client->messageOfCurrentJavaScriptDialogOnPage(*this, *page);
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::setUserInputForCurrentJavaScriptPrompt(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const String& promptValue)
{
    ASSERT(m_client);
    if (!m_client)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        SYNC_FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    // §18.4 Send Alert Text.
    // https://w3c.github.io/webdriver/webdriver-spec.html#send-alert-text
    // 3. Run the substeps of the first matching current user prompt:
    auto scriptDialogType = m_client->typeOfCurrentJavaScriptDialogOnPage(*this, *page);
    ASSERT(scriptDialogType);
    switch (scriptDialogType.value()) {
    case API::AutomationSessionClient::JavaScriptDialogType::Alert:
    case API::AutomationSessionClient::JavaScriptDialogType::Confirm:
        // Return error with error code element not interactable.
        SYNC_FAIL_WITH_PREDEFINED_ERROR(ElementNotInteractable);
    case API::AutomationSessionClient::JavaScriptDialogType::Prompt:
        // Do nothing.
        break;
    case API::AutomationSessionClient::JavaScriptDialogType::BeforeUnloadConfirm:
        // Return error with error code unsupported operation.
        SYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
    }

    m_client->setUserInputForCurrentJavaScriptPromptOnPage(*this, *page, promptValue);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::setFilesToSelectForFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, Ref<JSON::Array>&& filenames, RefPtr<JSON::Array>&& fileContents)
{
    Vector<String> newFileList;
    newFileList.reserveInitialCapacity(filenames->length());

    if (fileContents && fileContents->length() != filenames->length())
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The parameters 'filenames' and 'fileContents' must have equal length.");

    for (size_t i = 0; i < filenames->length(); ++i) {
        auto filename = filenames->get(i)->asString();
        if (!filename)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The parameter 'filenames' contains a non-string value.");

        if (!fileContents) {
            newFileList.append(filename);
            continue;
        }

        auto fileData = fileContents->get(i)->asString();
        if (!fileData)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The parameter 'fileContents' contains a non-string value.");

        std::optional<String> localFilePath = platformGenerateLocalFilePathForRemoteFile(filename, fileData);
        if (!localFilePath)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote file could not be saved to a local temporary directory.");

        newFileList.append(localFilePath.value());
    }

    m_filesToSelectForFileUpload.swap(newFileList);

    return { };
}

void WebAutomationSession::setFilesForInputFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, Ref<JSON::Array>&& filenames, Ref<SetFilesForInputFileUploadCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    Vector<String> newFileList;
    newFileList.reserveInitialCapacity(filenames->length());
    for (size_t i = 0; i < filenames->length(); ++i) {
        auto filename = filenames->get(i)->asString();
        if (!filename)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The parameter 'filenames' contains a non-string value.");

        newFileList.append(filename);
    }

    CompletionHandler<void(std::optional<String>)> completionHandler = [callback](std::optional<String> errorType) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess();
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::SetFilesForInputFileUpload(page->webPageID(), frameID, nodeHandle, WTFMove(newFileList)), WTFMove(completionHandler));
}

static inline Inspector::Protocol::Automation::CookieSameSitePolicy toProtocolSameSitePolicy(WebCore::Cookie::SameSitePolicy policy)
{
    switch (policy) {
    case WebCore::Cookie::SameSitePolicy::None:
        return Inspector::Protocol::Automation::CookieSameSitePolicy::None;
    case WebCore::Cookie::SameSitePolicy::Lax:
        return Inspector::Protocol::Automation::CookieSameSitePolicy::Lax;
    case WebCore::Cookie::SameSitePolicy::Strict:
        return Inspector::Protocol::Automation::CookieSameSitePolicy::Strict;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static inline WebCore::Cookie::SameSitePolicy toWebCoreSameSitePolicy(Inspector::Protocol::Automation::CookieSameSitePolicy policy)
{
    switch (policy) {
    case Inspector::Protocol::Automation::CookieSameSitePolicy::None:
        return WebCore::Cookie::SameSitePolicy::None;
    case Inspector::Protocol::Automation::CookieSameSitePolicy::Lax:
        return WebCore::Cookie::SameSitePolicy::Lax;
    case Inspector::Protocol::Automation::CookieSameSitePolicy::Strict:
        return WebCore::Cookie::SameSitePolicy::Strict;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<Inspector::Protocol::Automation::Cookie> buildObjectForCookie(const WebCore::Cookie& cookie)
{
    return Inspector::Protocol::Automation::Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires ? *cookie.expires / 1000 : 0)
        .setSize((cookie.name.length() + cookie.value.length()))
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSession(cookie.session)
        .setSameSite(toProtocolSameSitePolicy(cookie.sameSite))
        .release();
}

static Ref<JSON::ArrayOf<Inspector::Protocol::Automation::Cookie>> buildArrayForCookies(Vector<WebCore::Cookie>& cookiesList)
{
    auto cookies = JSON::ArrayOf<Inspector::Protocol::Automation::Cookie>::create();

    for (const auto& cookie : cookiesList)
        cookies->addItem(buildObjectForCookie(cookie));

    return cookies;
}

void WebAutomationSession::getAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, Ref<GetAllCookiesCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WTF::CompletionHandler<void(std::optional<String>, Vector<WebCore::Cookie>)> completionHandler = [callback](std::optional<String> errorType, Vector<WebCore::Cookie> cookies) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess(buildArrayForCookies(cookies));
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::GetCookiesForFrame(page->webPageID(), std::nullopt), WTFMove(completionHandler));
}

void WebAutomationSession::deleteSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const String& cookieName, Ref<DeleteSingleCookieCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WTF::CompletionHandler<void(std::optional<String>)> completionHandler = [callback](std::optional<String> errorType) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess();
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::DeleteCookie(page->webPageID(), std::nullopt, cookieName), WTFMove(completionHandler));
}

static String domainByAddingDotPrefixIfNeeded(String domain)
{
    if (domain[0] != '.') {
        // RFC 2965: If an explicitly specified value does not start with a dot, the user agent supplies a leading dot.
        // Assume that any host that ends with a digit is trying to be an IP address.
        if (!URL::hostIsIPAddress(domain))
            return makeString('.', domain);
    }
    
    return domain;
}
    
void WebAutomationSession::addSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, Ref<JSON::Object>&& cookieObject, Ref<AddSingleCookieCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    URL activeURL { page->pageLoadState().activeURL() };
    ASSERT(activeURL.isValid());

    WebCore::Cookie cookie;

    cookie.name = cookieObject->getString("name"_s);
    if (!cookie.name)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'name' was not found.");

    cookie.value = cookieObject->getString("value"_s);
    if (!cookie.value)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'value' was not found.");

    auto domain = cookieObject->getString("domain"_s);
    if (!domain)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'domain' was not found.");

    // Inherit the domain/host from the main frame's URL if it is not explicitly set.
    if (domain.isEmpty())
        cookie.domain = activeURL.host().toString();
    else
        cookie.domain = domainByAddingDotPrefixIfNeeded(domain);

    cookie.path = cookieObject->getString("path"_s);
    if (!cookie.path)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'path' was not found.");

    auto expires = cookieObject->getDouble("expires"_s);
    if (!expires)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'expires' was not found.");

    cookie.expires = *expires * 1000.0;

    auto secure = cookieObject->getBoolean("secure"_s);
    if (!secure)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'secure' was not found.");

    cookie.secure = *secure;

    auto session = cookieObject->getBoolean("session"_s);
    if (!session)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'session' was not found.");

    cookie.session = *session;

    auto httpOnly = cookieObject->getBoolean("httpOnly"_s);
    if (!httpOnly)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'httpOnly' was not found.");

    cookie.httpOnly = *httpOnly;

    auto sameSite = cookieObject->getString("sameSite"_s);
    if (!sameSite)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'sameSite' was not found.");

    auto parsedSameSite = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::CookieSameSitePolicy>(sameSite);
    if (!parsedSameSite)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'sameSite' has an unknown value.");

    cookie.sameSite = toWebCoreSameSitePolicy(*parsedSameSite);

    auto& cookieStore = page->websiteDataStore().cookieStore();
    cookieStore.setCookies({ cookie }, [callback]() {
        callback->sendSuccess();
    });
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::deleteAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    URL activeURL { page->pageLoadState().activeURL() };
    ASSERT(activeURL.isValid());

    String host = activeURL.host().toString();

    auto& cookieStore = page->websiteDataStore().cookieStore();
    cookieStore.deleteCookiesForHostnames({ host, domainByAddingDotPrefixIfNeeded(host) }, [] { });

    return { };
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>>> WebAutomationSession::getSessionPermissions()
{
    auto permissionsObjectArray = JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>::create();
    auto getUserMediaPermissionObject = Inspector::Protocol::Automation::SessionPermissionData::create()
        .setPermission(Inspector::Protocol::Automation::SessionPermission::GetUserMedia)
        .setValue(m_permissionForGetUserMedia)
        .release();

    permissionsObjectArray->addItem(WTFMove(getUserMediaPermissionObject));
    return permissionsObjectArray;
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::setSessionPermissions(Ref<JSON::Array>&& permissions)
{
    for (auto it = permissions->begin(); it != permissions->end(); ++it) {
        auto permission = it->get().asObject();
        if (!permission)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'permissions' is invalid.");

        auto permissionName = permission->getString("permission"_s);
        if (!permissionName)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'permission' is missing or invalid.");

        auto parsedPermissionName = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::SessionPermission>(permissionName);
        if (!parsedPermissionName)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'permission' has an unknown value.");

        auto permissionValue = permission->getBoolean("value"_s);
        if (!permissionValue)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'value' is missing or invalid.");

        switch (parsedPermissionName.value()) {
        case Inspector::Protocol::Automation::SessionPermission::GetUserMedia:
            m_permissionForGetUserMedia = *permissionValue;
            break;
        }
    }

    return { };
}

#if ENABLE(WEB_AUTHN)
static WebCore::AuthenticatorTransport toAuthenticatorTransport(Inspector::Protocol::Automation::AuthenticatorTransport transport)
{
    switch (transport) {
    case Inspector::Protocol::Automation::AuthenticatorTransport::Usb:
        return WebCore::AuthenticatorTransport::Usb;
    case Inspector::Protocol::Automation::AuthenticatorTransport::Nfc:
        return WebCore::AuthenticatorTransport::Nfc;
    case Inspector::Protocol::Automation::AuthenticatorTransport::Ble:
        return WebCore::AuthenticatorTransport::Ble;
    case Inspector::Protocol::Automation::AuthenticatorTransport::Internal:
        return WebCore::AuthenticatorTransport::Internal;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::AuthenticatorTransport::Internal;
    }
}
#endif // ENABLE(WEB_AUTHN)

Inspector::Protocol::ErrorStringOr<String /* authenticatorId */> WebAutomationSession::addVirtualAuthenticator(const String& browsingContextHandle, Ref<JSON::Object>&& authenticator)
{
#if ENABLE(WEB_AUTHN)
    auto protocol = authenticator->getString("protocol"_s);
    if (!protocol)
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'protocol' is missing or invalid.");
    auto transport = authenticator->getString("transport"_s);
    if (!transport)
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'transport' is missing or invalid.");
    auto parsedTransport = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::AuthenticatorTransport>(transport);
    if (!parsedTransport)
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'transport' has an unknown value.");
    auto hasResidentKey = authenticator->getBoolean("hasResidentKey"_s);
    if (!hasResidentKey)
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'hasResidentKey' is missing or invalid.");
    auto hasUserVerification = authenticator->getBoolean("hasUserVerification"_s);
    if (!hasUserVerification)
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'hasUserVerification' is missing or invalid.");
    auto isUserConsenting = authenticator->getBoolean("isUserConsenting"_s);
    if (!isUserConsenting)
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'isUserConsenting' is missing or invalid.");
    auto isUserVerified = authenticator->getBoolean("isUserVerified"_s);
    if (!isUserVerified)
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'isUserVerified' is missing or invalid.");

    auto page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);
    return page->websiteDataStore().virtualAuthenticatorManager().createAuthenticator({
        .protocol = protocol,
        .transport = toAuthenticatorTransport(parsedTransport.value()),
        .hasResidentKey = *hasResidentKey,
        .hasUserVerification = *hasUserVerification,
        .isUserConsenting = *isUserConsenting,
        .isUserVerified = *isUserVerified,
    });
#else
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented.");
#endif // ENABLE(WEB_AUTHN)
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::removeVirtualAuthenticator(const String& browsingContextHandle, const String& authenticatorId)
{
#if ENABLE(WEB_AUTHN)
    auto page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);
    if (!page->websiteDataStore().virtualAuthenticatorManager().removeAuthenticator(authenticatorId))
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "No such authenticator exists.");
    return { };
#else
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented.");
#endif // ENABLE(WEB_AUTHN)
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::addVirtualAuthenticatorCredential(const String& browsingContextHandle, const String& authenticatorId, Ref<JSON::Object>&& credential)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented.");
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::VirtualAuthenticatorCredential>> /* credentials */> WebAutomationSession::getVirtualAuthenticatorCredentials(const String& browsingContextHandle, const String& authenticatorId)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented.");
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::removeVirtualAuthenticatorCredential(const String& browsingContextHandle, const String& authenticatorId, const String& credentialId)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented.");
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::removeAllVirtualAuthenticatorCredentials(const String& browsingContextHandle, const String& authenticatorId)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented.");
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::setVirtualAuthenticatorUserVerified(const String& browsingContextHandle, const String& authenticatorId, bool isUserVerified)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented.");
}

Inspector::Protocol::ErrorStringOr<void> WebAutomationSession::generateTestReport(const String& browsingContextHandle, const String& message, const String& group)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    page->generateTestReport(message, group);

    return { };
}

bool WebAutomationSession::shouldAllowGetUserMediaForPage(const WebPageProxy&) const
{
    return m_permissionForGetUserMedia;
}

bool WebAutomationSession::isSimulatingUserInteraction() const
{
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    if (!m_pendingMouseEventsFlushedCallbacksPerPage.isEmpty())
        return true;
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    if (!m_pendingKeyboardEventsFlushedCallbacksPerPage.isEmpty())
        return true;
#endif
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    if (!m_pendingWheelEventsFlushedCallbacksPerPage.isEmpty())
        return true;
#endif
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    if (m_simulatingTouchInteraction)
        return true;
#endif
    return false;
}

#if ENABLE(WEBDRIVER_ACTIONS_API)
SimulatedInputDispatcher& WebAutomationSession::inputDispatcherForPage(WebPageProxy& page)
{
    return m_inputDispatchersByPage.ensure(page.identifier(), [&] {
        return SimulatedInputDispatcher::create(page, *this);
    }).iterator->value;
}

// MARK: SimulatedInputDispatcher::Client API
void WebAutomationSession::viewportInViewCenterPointOfElement(WebPageProxy& page, std::optional<FrameIdentifier> frameID, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, Function<void(std::optional<WebCore::IntPoint>, std::optional<AutomationCommandError>)>&& completionHandler)
{
    WTF::CompletionHandler<void(std::optional<String>, WebCore::IntRect, std::optional<WebCore::IntPoint>, bool)> didComputeElementLayoutHandler = [completionHandler = WTFMove(completionHandler)](std::optional<String> errorType, WebCore::IntRect, std::optional<WebCore::IntPoint> inViewCenterPoint, bool) mutable {
        if (errorType) {
            completionHandler(std::nullopt, AUTOMATION_COMMAND_ERROR_WITH_MESSAGE(*errorType));
            return;
        }

        if (!inViewCenterPoint) {
            completionHandler(std::nullopt, AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
            return;
        }

        completionHandler(inViewCenterPoint, std::nullopt);
    };

    page.process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ComputeElementLayout(page.webPageID(), frameID, nodeHandle, false, CoordinateSystem::LayoutViewport), WTFMove(didComputeElementLayoutHandler));
}

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
void WebAutomationSession::updateClickCount(MouseButton button, const WebCore::IntPoint& position, Seconds maxTime, int maxDistance)
{
    auto now = MonotonicTime::now();
    if (now - m_lastClickTime < maxTime && button == m_lastClickButton && m_lastClickPosition.distanceSquaredToPoint(position) < maxDistance) {
        m_clickCount++;
        m_lastClickTime = now;
        return;
    }

    m_clickCount = 1;
    m_lastClickTime = now;
    m_lastClickButton = button;
    m_lastClickPosition = position;
}

void WebAutomationSession::resetClickCount()
{
    m_clickCount = 1;
    m_lastClickButton = MouseButton::None;
    m_lastClickPosition = { };
}

void WebAutomationSession::simulateMouseInteraction(WebPageProxy& page, MouseInteraction interaction, MouseButton mouseButton, const WebCore::IntPoint& locationInViewport, const String& pointerType, CompletionHandler<void(std::optional<AutomationCommandError>)>&& completionHandler)
{
    page.getWindowFrameWithCallback([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), page = Ref { page }, interaction, mouseButton, locationInViewport, pointerType](WebCore::FloatRect windowFrame) mutable {
        auto clippedX = std::min(std::max(0.0f, (float)locationInViewport.x()), windowFrame.size().width());
        auto clippedY = std::min(std::max(0.0f, (float)locationInViewport.y()), windowFrame.size().height());
        if (clippedX != locationInViewport.x() || clippedY != locationInViewport.y()) {
            completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
            return;
        }

        // Bridge the flushed callback to our command's completion handler.
        auto mouseEventsFlushedCallback = [completionHandler = WTFMove(completionHandler)](std::optional<AutomationCommandError> error) mutable {
            completionHandler(error);
        };

        auto& callbackInMap = m_pendingMouseEventsFlushedCallbacksPerPage.add(page->identifier(), nullptr).iterator->value;
        if (callbackInMap)
            callbackInMap(AUTOMATION_COMMAND_ERROR_WITH_NAME(Timeout));
        callbackInMap = WTFMove(mouseEventsFlushedCallback);

        platformSimulateMouseInteraction(page, interaction, mouseButton, locationInViewport, platformWebModifiersFromRaw(m_currentModifiers), pointerType);

        // If the event does not hit test anything in the window, then it may not have been delivered.
        if (callbackInMap && !page->isProcessingMouseEvents()) {
            auto callbackToCancel = m_pendingMouseEventsFlushedCallbacksPerPage.take(page->identifier());
            callbackToCancel(std::nullopt);
        }

        // Otherwise, wait for mouseEventsFlushedCallback to run when all events are handled.
    });
}
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
void WebAutomationSession::simulateTouchInteraction(WebPageProxy& page, TouchInteraction interaction, const WebCore::IntPoint& locationInViewport, std::optional<Seconds> duration, CompletionHandler<void(std::optional<AutomationCommandError>)>&& completionHandler)
{
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect visualViewportBounds = WebCore::FloatRect({ }, page.unobscuredContentRect().size());
    if (!visualViewportBounds.contains(locationInViewport)) {
        completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
        return;
    }
#endif

    m_simulatingTouchInteraction = true;
    platformSimulateTouchInteraction(page, interaction, locationInViewport, duration, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](std::optional<AutomationCommandError> error) mutable {
        m_simulatingTouchInteraction = false;
        completionHandler(error);
    });
}
#endif // ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
void WebAutomationSession::simulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, std::variant<VirtualKey, CharKey>&& key, CompletionHandler<void(std::optional<AutomationCommandError>)>&& completionHandler)
{
    // Bridge the flushed callback to our command's completion handler.
    auto keyboardEventsFlushedCallback = [completionHandler = WTFMove(completionHandler)](std::optional<AutomationCommandError> error) mutable {
        completionHandler(error);
    };

    auto& callbackInMap = m_pendingKeyboardEventsFlushedCallbacksPerPage.add(page.identifier(), nullptr).iterator->value;
    if (callbackInMap)
        callbackInMap(AUTOMATION_COMMAND_ERROR_WITH_NAME(Timeout));
    callbackInMap = WTFMove(keyboardEventsFlushedCallback);

    platformSimulateKeyboardInteraction(page, interaction, WTFMove(key));

    // If the interaction does not generate any events, then do not wait for events to be flushed.
    // This happens in some corner cases on macOS, such as releasing a key while Command is pressed.
    if (callbackInMap && !page.isProcessingKeyboardEvents()) {
        auto callbackToCancel = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.identifier());
        callbackToCancel(std::nullopt);
    }

    // Otherwise, wait for keyboardEventsFlushedCallback to run when all events are handled.
}
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
void WebAutomationSession::simulateWheelInteraction(WebPageProxy& page, const WebCore::IntPoint& locationInViewport, const WebCore::IntSize& delta, AutomationCompletionHandler&& completionHandler)
{
    page.getWindowFrameWithCallback([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), page = Ref { page }, locationInViewport, delta](WebCore::FloatRect windowFrame) mutable {
        auto clippedX = std::min(std::max(0.0f, static_cast<float>(locationInViewport.x())), windowFrame.size().width());
        auto clippedY = std::min(std::max(0.0f, static_cast<float>(locationInViewport.y())), windowFrame.size().height());
        if (clippedX != locationInViewport.x() || clippedY != locationInViewport.y()) {
            completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
            return;
        }

        // Bridge the flushed callback to our command's completion handler.
        auto wheelEventsFlushedCallback = [completionHandler = WTFMove(completionHandler)](std::optional<AutomationCommandError> error) mutable {
            completionHandler(error);
        };

        auto& callbackInMap = m_pendingWheelEventsFlushedCallbacksPerPage.add(page->identifier(), nullptr).iterator->value;
        if (callbackInMap)
            callbackInMap(AUTOMATION_COMMAND_ERROR_WITH_NAME(Timeout));
        callbackInMap = WTFMove(wheelEventsFlushedCallback);

        platformSimulateWheelInteraction(page, locationInViewport, delta);

        // If the event does not hit test anything in the window, then it may not have been delivered.
        if (callbackInMap && !page->isProcessingWheelEvents()) {
            auto callbackToCancel = m_pendingWheelEventsFlushedCallbacksPerPage.take(page->identifier());
            callbackToCancel(std::nullopt);
        }

        // Otherwise, wait for wheelEventsFlushedCallback to run when all events are handled.
    });
}
#endif
#endif // ENABLE(WEBDRIVER_ACTIONS_API)

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
static WebEventModifier protocolModifierToWebEventModifier(Inspector::Protocol::Automation::KeyModifier modifier)
{
    switch (modifier) {
    case Inspector::Protocol::Automation::KeyModifier::Alt:
        return WebEventModifier::AltKey;
    case Inspector::Protocol::Automation::KeyModifier::Meta:
        return WebEventModifier::MetaKey;
    case Inspector::Protocol::Automation::KeyModifier::Control:
        return WebEventModifier::ControlKey;
    case Inspector::Protocol::Automation::KeyModifier::Shift:
        return WebEventModifier::ShiftKey;
    case Inspector::Protocol::Automation::KeyModifier::CapsLock:
        return WebEventModifier::CapsLockKey;
    }

    RELEASE_ASSERT_NOT_REACHED();
}
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

void WebAutomationSession::performMouseInteraction(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, Ref<JSON::Object>&& requestedPosition, Inspector::Protocol::Automation::MouseButton mouseButton, Inspector::Protocol::Automation::MouseInteraction mouseInteraction, Ref<JSON::Array>&& keyModifierStrings, Ref<PerformMouseInteractionCallback>&& callback)
{
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto x = requestedPosition->getDouble("x"_s);
    if (!x)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'x' was not found.");

    auto floatX = static_cast<float>(*x);

    auto y = requestedPosition->getDouble("y"_s);
    if (!y)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'y' was not found.");

    auto floatY = static_cast<float>(*y);

    OptionSet<WebEventModifier> keyModifiers;
    for (auto it = keyModifierStrings->begin(); it != keyModifierStrings->end(); ++it) {
        auto modifierString = it->get().asString();
        if (!modifierString)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'modifiers' is invalid.");

        auto parsedModifier = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::KeyModifier>(modifierString);
        if (!parsedModifier)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "A modifier in the 'modifiers' array is invalid.");

        keyModifiers.add(protocolModifierToWebEventModifier(parsedModifier.value()));
    }

    page->getWindowFrameWithCallback([this, protectedThis = Ref { *this }, callback = WTFMove(callback), page = Ref { *page }, floatX, floatY, mouseInteraction, mouseButton, keyModifiers](WebCore::FloatRect windowFrame) mutable {
        floatX = std::min(std::max(0.0f, floatX), windowFrame.size().width());
        floatY = std::min(std::max(0.0f, floatY), windowFrame.size().height());

        WebCore::IntPoint locationInViewport = WebCore::IntPoint(static_cast<int>(floatX), static_cast<int>(floatY));

        auto mouseEventsFlushedCallback = [protectedThis = WTFMove(protectedThis), callback = WTFMove(callback), page, floatX, floatY](std::optional<AutomationCommandError> error) {
            if (error)
                callback->sendFailure(error.value().toProtocolString());
            else {
                callback->sendSuccess(Inspector::Protocol::Automation::Point::create()
                    .setX(floatX)
                    .setY(floatY - page->topContentInset())
                    .release());
            }
        };

        auto& callbackInMap = m_pendingMouseEventsFlushedCallbacksPerPage.add(page->identifier(), nullptr).iterator->value;
        if (callbackInMap)
            callbackInMap(AUTOMATION_COMMAND_ERROR_WITH_NAME(Timeout));
        callbackInMap = WTFMove(mouseEventsFlushedCallback);

        platformSimulateMouseInteraction(page, mouseInteraction, mouseButton, locationInViewport, keyModifiers, WebCore::mousePointerEventType());

        // If the event location was previously clipped and does not hit test anything in the window, then it will not be processed.
        // For compatibility with pre-W3C driver implementations, don't make this a hard error; just do nothing silently.
        // In W3C-only code paths, we can reject any pointer actions whose coordinates are outside the viewport rect.
        if (callbackInMap && !page->isProcessingMouseEvents()) {
            auto callbackToCancel = m_pendingMouseEventsFlushedCallbacksPerPage.take(page->identifier());
            callbackToCancel(std::nullopt);
        }
    });
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
}

void WebAutomationSession::performKeyboardInteractions(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, Ref<JSON::Array>&& interactions, Ref<PerformKeyboardInteractionsCallback>&& callback)
{
#if !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!interactions->length())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'interactions' was not found or empty.");

    // Validate all of the parameters before performing any interactions with the browsing context under test.
    Vector<WTF::Function<void()>> actionsToPerform;
    actionsToPerform.reserveCapacity(interactions->length());

    for (const auto& interactionValue : interactions.get()) {
        auto interactionObject = interactionValue->asObject();
        if (!interactionObject)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter was invalid.");

        auto interactionTypeString = interactionObject->getString("type"_s);
        if (!interactionTypeString)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter is missing the 'type' key.");
        auto interactionType = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::KeyboardInteractionType>(interactionTypeString);
        if (!interactionType)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'type' key.");

        auto virtualKeyString = interactionObject->getString("key"_s);
        if (!!virtualKeyString) {
            std::optional<VirtualKey> virtualKey = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::VirtualKey>(virtualKeyString);
            if (!virtualKey)
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'key' value.");

            actionsToPerform.uncheckedAppend([this, page, interactionType, virtualKey] {
                platformSimulateKeyboardInteraction(*page, interactionType.value(), virtualKey.value());
            });
        }

        auto keySequence = interactionObject->getString("text"_s);
        if (!!keySequence) {
            switch (interactionType.value()) {
            case Inspector::Protocol::Automation::KeyboardInteractionType::KeyPress:
            case Inspector::Protocol::Automation::KeyboardInteractionType::KeyRelease:
                // 'KeyPress' and 'KeyRelease' are meant for a virtual key and are not supported for a string (sequence of codepoints).
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'key' value.");

            case Inspector::Protocol::Automation::KeyboardInteractionType::InsertByKey:
                actionsToPerform.uncheckedAppend([this, page, keySequence] {
                    platformSimulateKeySequence(*page, keySequence);
                });
                break;
            }
        }

        if (!virtualKeyString && !keySequence)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "An interaction in the 'interactions' parameter is missing both 'key' and 'text'. One must be provided.");
    }

    ASSERT(actionsToPerform.size());
    if (!actionsToPerform.size())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "No actions to perform.");

    auto keyboardEventsFlushedCallback = [protectedThis = Ref { *this }, callback = WTFMove(callback), page = Ref { *page }](std::optional<AutomationCommandError> error) {
        if (error)
            callback->sendFailure(error.value().toProtocolString());
        else
            callback->sendSuccess();
    };

    auto& callbackInMap = m_pendingKeyboardEventsFlushedCallbacksPerPage.add(page->identifier(), nullptr).iterator->value;
    if (callbackInMap)
        callbackInMap(AUTOMATION_COMMAND_ERROR_WITH_NAME(Timeout));
    callbackInMap = WTFMove(keyboardEventsFlushedCallback);

    for (auto& action : actionsToPerform)
        action();
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
}

#if ENABLE(WEBDRIVER_ACTIONS_API)
static SimulatedInputSourceType simulatedInputSourceTypeFromProtocolSourceType(Inspector::Protocol::Automation::InputSourceType protocolType)
{
    switch (protocolType) {
    case Inspector::Protocol::Automation::InputSourceType::Null:
        return SimulatedInputSourceType::Null;
    case Inspector::Protocol::Automation::InputSourceType::Keyboard:
        return SimulatedInputSourceType::Keyboard;
    case Inspector::Protocol::Automation::InputSourceType::Mouse:
        return SimulatedInputSourceType::Mouse;
    case Inspector::Protocol::Automation::InputSourceType::Touch:
        return SimulatedInputSourceType::Touch;
    case Inspector::Protocol::Automation::InputSourceType::Wheel:
        return SimulatedInputSourceType::Wheel;
    case Inspector::Protocol::Automation::InputSourceType::Pen:
        return SimulatedInputSourceType::Pen;
    }

    RELEASE_ASSERT_NOT_REACHED();
}
#endif // ENABLE(WEBDRIVER_ACTIONS_API)

#if ENABLE(WEBDRIVER_ACTIONS_API)
// §15.4.2 Keyboard actions
// https://w3c.github.io/webdriver/#dfn-normalised-key-value
static VirtualKey normalizedVirtualKey(VirtualKey key)
{
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::ControlRight:
        return Inspector::Protocol::Automation::VirtualKey::Control;
    case Inspector::Protocol::Automation::VirtualKey::ShiftRight:
        return Inspector::Protocol::Automation::VirtualKey::Shift;
    case Inspector::Protocol::Automation::VirtualKey::AlternateRight:
        return Inspector::Protocol::Automation::VirtualKey::Alternate;
    case Inspector::Protocol::Automation::VirtualKey::MetaRight:
        return Inspector::Protocol::Automation::VirtualKey::Meta;
    case Inspector::Protocol::Automation::VirtualKey::DownArrowRight:
        return Inspector::Protocol::Automation::VirtualKey::DownArrow;
    case Inspector::Protocol::Automation::VirtualKey::UpArrowRight:
        return Inspector::Protocol::Automation::VirtualKey::UpArrow;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrowRight:
        return Inspector::Protocol::Automation::VirtualKey::LeftArrow;
    case Inspector::Protocol::Automation::VirtualKey::RightArrowRight:
        return Inspector::Protocol::Automation::VirtualKey::RightArrow;
    case Inspector::Protocol::Automation::VirtualKey::PageUpRight:
        return Inspector::Protocol::Automation::VirtualKey::PageUp;
    case Inspector::Protocol::Automation::VirtualKey::PageDownRight:
        return Inspector::Protocol::Automation::VirtualKey::PageDown;
    case Inspector::Protocol::Automation::VirtualKey::EndRight:
        return Inspector::Protocol::Automation::VirtualKey::End;
    case Inspector::Protocol::Automation::VirtualKey::HomeRight:
        return Inspector::Protocol::Automation::VirtualKey::Home;
    case Inspector::Protocol::Automation::VirtualKey::DeleteRight:
        return Inspector::Protocol::Automation::VirtualKey::Delete;
    case Inspector::Protocol::Automation::VirtualKey::InsertRight:
        return Inspector::Protocol::Automation::VirtualKey::Insert;
    default:
        return key;
    }
}

static std::optional<UChar32> pressedCharKey(const String& pressedCharKeyString)
{
    switch (pressedCharKeyString.length()) {
    case 1:
        return pressedCharKeyString.characterAt(0);
    case 2: {
        auto lead = pressedCharKeyString.characterAt(0);
        auto trail = pressedCharKeyString.characterAt(1);
        if (U16_IS_LEAD(lead) && U16_IS_TRAIL(trail))
            return U16_GET_SUPPLEMENTARY(lead, trail);
    }
    }

    return std::nullopt;
}
#endif // ENABLE(WEBDRIVER_ACTIONS_API)

void WebAutomationSession::performInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, Ref<JSON::Array>&& inputSources, Ref<JSON::Array>&& steps, Ref<WebAutomationSession::PerformInteractionSequenceCallback>&& callback)
{
    // This command implements WebKit support for §17.5 Perform Actions.

#if !ENABLE(WEBDRIVER_ACTIONS_API)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    // Parse and validate Automation protocol arguments. By this point, the driver has
    // already performed the steps in §17.3 Processing Actions Requests.
    if (!inputSources->length())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'inputSources' was not found or empty.");

    HashSet<String> sourceIdSet;
    for (const auto& inputSourceValue : inputSources.get()) {
        auto inputSourceObject = inputSourceValue->asObject();
        if (!inputSourceObject)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An input source in the 'inputSources' parameter was invalid.");

        auto sourceId = inputSourceObject->getString("sourceId"_s);
        if (!sourceId)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An input source in the 'inputSources' parameter is missing a 'sourceId'.");

        auto sourceType = inputSourceObject->getString("sourceType"_s);
        if (!sourceType)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An input source in the 'inputSources' parameter is missing a 'sourceType'.");

        auto parsedInputSourceType = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::InputSourceType>(sourceType);
        if (!parsedInputSourceType)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An input source in the 'inputSources' parameter has an invalid 'sourceType'.");

        SimulatedInputSourceType inputSourceType = simulatedInputSourceTypeFromProtocolSourceType(*parsedInputSourceType);

        // Note: iOS does not support mouse input sources, and other platforms do not support touch input sources.
        // If a mismatch happens, alias to the supported input source. This works because both Mouse and Touch input sources
        // use a MouseButton to indicate the result of interacting (down/up/move), which can be interpreted for touch or mouse.
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) && ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Mouse || inputSourceType == SimulatedInputSourceType::Pen)
            inputSourceType = SimulatedInputSourceType::Touch;
#elif ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) && !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Touch)
            inputSourceType = SimulatedInputSourceType::Mouse;
#endif
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Mouse)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "Mouse input sources are not yet supported.");
        if (inputSourceType == SimulatedInputSourceType::Pen)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "Pen input sources are not yet supported.");
#endif
#if !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Touch)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "Touch input sources are not yet supported.");
#endif
#if !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Keyboard)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "Keyboard input sources are not yet supported.");
#endif
#if !ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Wheel)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "Wheel input sources are not yet supported.");
#endif
        if (sourceIdSet.contains(sourceId))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Two input sources with the same sourceId were specified.");

        sourceIdSet.add(sourceId);
        m_inputSources.ensure(sourceId, [inputSourceType] {
            return SimulatedInputSource::create(inputSourceType);
        });
    }

    Vector<SimulatedInputKeyFrame> keyFrames;

    if (!steps->length())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'steps' was not found or empty.");

    for (const auto& stepValue : steps.get()) {
        auto stepObject = stepValue->asObject();
        if (!stepObject)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "A step in the 'steps' parameter was not an object.");

        auto stepStates = stepObject->getArray("states"_s);
        if (!stepStates)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "A step is missing the 'states' property.");

        Vector<SimulatedInputKeyFrame::StateEntry> entries;
        entries.reserveCapacity(stepStates->length());

        for (const auto& stateValue : *stepStates) {
            auto stateObject = stateValue->asObject();
            if (!stateObject)
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Encountered a non-object step state.");

            auto sourceId = stateObject->getString("sourceId"_s);
            if (!sourceId)
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Step state lacks required 'sourceId' property.");

            if (!m_inputSources.contains(sourceId))
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Unknown 'sourceId' specified.");

            SimulatedInputSource& inputSource = *m_inputSources.get(sourceId);
            SimulatedInputSourceState sourceState { };

            auto pressedCharKeyString = stateObject->getString("pressedCharKey"_s);
            if (!!pressedCharKeyString) {
                auto charKey = pressedCharKey(pressedCharKeyString);
                if (!charKey)
                    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Invalid 'pressedCharKey'.");
                sourceState.pressedCharKeys.add(*charKey);
            }

            if (auto pressedVirtualKeysArray = stateObject->getArray("pressedVirtualKeys"_s)) {
                VirtualKeyMap pressedVirtualKeys;

                for (auto it = pressedVirtualKeysArray->begin(); it != pressedVirtualKeysArray->end(); ++it) {
                    auto pressedVirtualKeyString = (*it)->asString();
                    if (!pressedVirtualKeyString)
                        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Encountered a non-string virtual key value.");

                    std::optional<VirtualKey> parsedVirtualKey = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::VirtualKey>(pressedVirtualKeyString);
                    if (!parsedVirtualKey)
                        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Encountered an unknown virtual key value.");
                    else
                        pressedVirtualKeys.add(normalizedVirtualKey(parsedVirtualKey.value()), parsedVirtualKey.value());
                }

                sourceState.pressedVirtualKeys = pressedVirtualKeys;
            }

            auto pressedButtonString = stateObject->getString("pressedButton"_s);
            if (!!pressedButtonString) {
                auto protocolButton = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseButton>(pressedButtonString);
                sourceState.pressedMouseButton = protocolButton.value_or(MouseButton::None);
            }

            auto originString = stateObject->getString("origin"_s);
            if (!!originString)
                sourceState.origin = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseMoveOrigin>(originString);

            if (sourceState.origin && sourceState.origin.value() == Inspector::Protocol::Automation::MouseMoveOrigin::Element) {
                auto nodeHandleString = stateObject->getString("nodeHandle"_s);
                if (!nodeHandleString)
                    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Node handle not provided for 'Element' origin");
                sourceState.nodeHandle = nodeHandleString;
            }

            if (auto locationObject = stateObject->getObject("location"_s)) {
                auto x = locationObject->getInteger("x"_s);
                auto y = locationObject->getInteger("y"_s);
                if (x && y)
                    sourceState.location = WebCore::IntPoint(*x, *y);
            }

            if (auto deltaObject = stateObject->getObject("delta"_s)) {
                auto deltaX = deltaObject->getInteger("width"_s);
                auto deltaY = deltaObject->getInteger("height"_s);
                if (deltaX && deltaY)
                    sourceState.scrollDelta = WebCore::IntSize(*deltaX, *deltaY);
            }

            if (auto duration = stateObject->getInteger("duration"_s))
                sourceState.duration = Seconds::fromMilliseconds(*duration);

            entries.uncheckedAppend(std::pair<SimulatedInputSource&, SimulatedInputSourceState> { inputSource, sourceState });
        }
        
        keyFrames.append(SimulatedInputKeyFrame(WTFMove(entries)));
    }

    SimulatedInputDispatcher& inputDispatcher = inputDispatcherForPage(*page);
    if (inputDispatcher.isActive()) {
        ASSERT_NOT_REACHED();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "A previous interaction is still underway.");
    }

    // Delegate the rest of §17.4 Dispatching Actions to the dispatcher.
    inputDispatcher.run(frameID, WTFMove(keyFrames), m_inputSources, [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<AutomationCommandError> error) {
        if (error)
            callback->sendFailure(error.value().toProtocolString());
        else
            callback->sendSuccess();
    });
#endif // ENABLE(WEBDRIVER_ACTIONS_API)
}

void WebAutomationSession::cancelInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, Ref<CancelInteractionSequenceCallback>&& callback)
{
    // This command implements WebKit support for §17.6 Release Actions.

#if !ENABLE(WEBDRIVER_ACTIONS_API)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    Vector<SimulatedInputKeyFrame> keyFrames({ SimulatedInputKeyFrame::keyFrameToResetInputSources(m_inputSources) });
    SimulatedInputDispatcher& inputDispatcher = inputDispatcherForPage(*page);
    inputDispatcher.cancel();
    
    inputDispatcher.run(frameID, WTFMove(keyFrames), m_inputSources, [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<AutomationCommandError> error) {
        if (error)
            callback->sendFailure(error.value().toProtocolString());
        else
            callback->sendSuccess();
        m_inputSources.clear();
    });
#endif // ENABLE(WEBDRIVER_ACTIONS_API)
}

void WebAutomationSession::takeScreenshot(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, std::optional<bool>&& optionalScrollIntoViewIfNeeded, std::optional<bool>&& optionalClipToViewport, Ref<TakeScreenshotCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    bool clipToViewport = optionalClipToViewport ? *optionalClipToViewport : false;

#if PLATFORM(COCOA)
    // FIXME: <webkit.org/b/242215> We can currently only get viewport snapshots from the UIProcess, so fall back to
    // taking a snapshot in the WebProcess if we need to snapshot a specific element. This has the side effect of not
    // accurately showing all CSS transforms in the snapshot.
    //
    // There is still a tradeoff by going to the UIProcess for viewport snapshots on macOS. We can not currently get a
    // snapshot of the entire viewport without the window's rounded corners being excluded. So we trade off those corner
    // pixels for accurate pixels in the rest of the viewport which help us verify features like CSS transforms are
    // actually behaving correctly.
    if (!nodeHandle.isEmpty()) {
        uint64_t callbackID = m_nextScreenshotCallbackID++;
        m_screenshotCallbacks.set(callbackID, WTFMove(callback));

        page->process().send(Messages::WebAutomationSessionProxy::TakeScreenshot(page->webPageID(), frameID, nodeHandle, scrollIntoViewIfNeeded, clipToViewport, callbackID), 0);
        return;
    }
#endif
#if PLATFORM(GTK) || PLATFORM(COCOA)
    Function<void(WebPageProxy&, std::optional<WebCore::IntRect>&&, Ref<TakeScreenshotCallback>&&)> takeViewSnapsot = [](WebPageProxy& page, std::optional<WebCore::IntRect>&& rect, Ref<TakeScreenshotCallback>&& callback) {
        page.callAfterNextPresentationUpdate([page = Ref { page }, rect = WTFMove(rect), callback = WTFMove(callback)](CallbackBase::Error error) mutable {
            if (error != CallbackBase::Error::None)
                ASYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

            auto snapshot = page->takeViewSnapshot(WTFMove(rect));
            if (!snapshot)
                ASYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

            std::optional<String> base64EncodedData = platformGetBase64EncodedPNGData(*snapshot);
            if (!base64EncodedData)
                ASYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

            callback->sendSuccess(base64EncodedData.value());
        });
    };

    if (nodeHandle.isEmpty()) {
        takeViewSnapsot(*page, std::nullopt, WTFMove(callback));
        return;
    }

    CompletionHandler<void(std::optional<String>, WebCore::IntRect&&)> completionHandler = [page = Ref { *page }, callback, takeViewSnapsot = WTFMove(takeViewSnapsot)](std::optional<String> errorType, WebCore::IntRect&& rect) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        takeViewSnapsot(page.get(), WTFMove(rect), WTFMove(callback));
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::SnapshotRectForScreenshot(page->webPageID(), frameID, nodeHandle, scrollIntoViewIfNeeded, clipToViewport), WTFMove(completionHandler));
#else
    uint64_t callbackID = m_nextScreenshotCallbackID++;
    m_screenshotCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::TakeScreenshot(page->webPageID(), frameID, nodeHandle, scrollIntoViewIfNeeded, clipToViewport, callbackID), 0);
#endif
}

void WebAutomationSession::didTakeScreenshot(uint64_t callbackID, const ShareableBitmapHandle& imageDataHandle, const String& errorType)
{
    auto callback = m_screenshotCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty()) {
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType));
        return;
    }

    std::optional<String> base64EncodedData = platformGetBase64EncodedPNGData(imageDataHandle);
    if (!base64EncodedData)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    callback->sendSuccess(base64EncodedData.value());
}

#if !PLATFORM(COCOA) && !USE(CAIRO)
std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ShareableBitmapHandle&)
{
    return std::nullopt;
}

std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ViewSnapshot&)
{
    return std::nullopt;
}
#endif // !PLATFORM(COCOA) && !USE(CAIRO)

#if !PLATFORM(COCOA)
std::optional<String> WebAutomationSession::platformGenerateLocalFilePathForRemoteFile(const String&, const String&)
{
    return std::nullopt;
}
#endif // !PLATFORM(COCOA)

} // namespace WebKit
