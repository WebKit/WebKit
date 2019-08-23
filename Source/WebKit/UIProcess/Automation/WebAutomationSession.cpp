
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
#include "APINavigation.h"
#include "APIOpenPanelParameters.h"
#include "AutomationProtocolObjects.h"
#include "CoordinateSystem.h"
#include "WebAutomationSessionMacros.h"
#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebCookieManagerProxy.h"
#include "WebFullScreenManagerProxy.h"
#include "WebInspectorProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <WebCore/MIMETypeRegistry.h>
#include <algorithm>
#include <wtf/HashMap.h>
#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/StringConcatenate.h>

namespace WebKit {

using namespace Inspector;
using namespace WebCore;

String AutomationCommandError::toProtocolString()
{
    String protocolErrorName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(type);
    if (!message.hasValue())
        return protocolErrorName;

    return makeString(protocolErrorName, errorNameAndDetailsSeparator, message.value());
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
#if ENABLE(WEBDRIVER_ACTIONS_API)
    // Set up canonical input sources to be used for 'performInteractionSequence' and 'cancelInteractionSequence'.
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    m_inputSources.add(SimulatedInputSource::create(SimulatedInputSourceType::Touch));
#endif
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    m_inputSources.add(SimulatedInputSource::create(SimulatedInputSourceType::Mouse));
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    m_inputSources.add(SimulatedInputSource::create(SimulatedInputSourceType::Keyboard));
#endif
    m_inputSources.add(SimulatedInputSource::create(SimulatedInputSourceType::Null));
#endif // ENABLE(WEBDRIVER_ACTIONS_API)
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

void WebAutomationSession::dispatchMessageFromRemote(const String& message)
{
    m_backendDispatcher->dispatch(message);
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
    auto iter = m_webPageHandleMap.find(webPageProxy.pageID());
    if (iter != m_webPageHandleMap.end())
        return iter->value;

    String handle = "page-" + createCanonicalUUIDString().convertToASCIIUppercase();

    auto firstAddResult = m_webPageHandleMap.add(webPageProxy.pageID(), handle);
    RELEASE_ASSERT(firstAddResult.isNewEntry);

    auto secondAddResult = m_handleWebPageMap.add(handle, webPageProxy.pageID());
    RELEASE_ASSERT(secondAddResult.isNewEntry);

    return handle;
}

Optional<FrameIdentifier> WebAutomationSession::webFrameIDForHandle(const String& handle, bool& frameNotFound)
{
    if (handle.isEmpty())
        return WTF::nullopt;

    auto iter = m_handleWebFrameMap.find(handle);
    if (iter == m_handleWebFrameMap.end()) {
        frameNotFound = true;
        return WTF::nullopt;
    }

    return iter->value;
}

String WebAutomationSession::handleForWebFrameID(Optional<FrameIdentifier> frameID)
{
    if (!frameID || !*frameID)
        return emptyString();

    for (auto& process : m_processPool->processes()) {
        if (WebFrameProxy* frame = process->webFrame(*frameID)) {
            if (frame->isMainFrame())
                return emptyString();
            break;
        }
    }

    auto iter = m_webFrameHandleMap.find(*frameID);
    if (iter != m_webFrameHandleMap.end())
        return iter->value;

    String handle = "frame-" + createCanonicalUUIDString().convertToASCIIUppercase();

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
    
    getNextContext(makeRef(*this), WTFMove(pages), JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>::create(), WTFMove(callback));
}

void WebAutomationSession::getBrowsingContext(const String& handle, Ref<GetBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    page->getWindowFrameWithCallback([protectedThis = makeRef(*this), page = makeRef(*page), callback = WTFMove(callback)](WebCore::FloatRect windowFrame) mutable {
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

void WebAutomationSession::createBrowsingContext(const String* optionalPresentationHint, Ref<CreateBrowsingContextCallback>&& callback)
{
    ASSERT(m_client);
    if (!m_client)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote session could not request a new browsing context.");

    uint16_t options = 0;

    if (optionalPresentationHint) {
        auto parsedPresentationHint = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::BrowsingContextPresentation>(*optionalPresentationHint);
        if (parsedPresentationHint.hasValue() && parsedPresentationHint.value() == Inspector::Protocol::Automation::BrowsingContextPresentation::Tab)
            options |= API::AutomationSessionBrowsingContextOptionsPreferNewTab;
    }

    m_client->requestNewPageWithOptions(*this, static_cast<API::AutomationSessionBrowsingContextOptions>(options), [protectedThis = makeRef(*this), callback = WTFMove(callback)](WebPageProxy* page) {
        if (!page)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote session failed to create a new browsing context.");

        callback->sendSuccess(protectedThis->handleForWebPageProxy(*page), toProtocol(protectedThis->m_client->currentPresentationOfPage(protectedThis.get(), *page)));
    });
}

void WebAutomationSession::closeBrowsingContext(Inspector::ErrorString& errorString, const String& handle)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    page->closePage(false);
}

void WebAutomationSession::switchToBrowsingContext(const String& browsingContextHandle, const String* optionalFrameHandle, Ref<SwitchToBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);


    m_client->requestSwitchToPage(*this, *page, [frameID, page = makeRef(*page), callback = WTFMove(callback)]() {
        page->setFocus(true);
        page->process().send(Messages::WebAutomationSessionProxy::FocusFrame(page->pageID(), frameID), 0);

        callback->sendSuccess();
    });
}

void WebAutomationSession::setWindowFrameOfBrowsingContext(const String& handle, const JSON::Object* optionalOriginObject, const JSON::Object* optionalSizeObject, Ref<SetWindowFrameOfBrowsingContextCallback>&& callback)
{
    Optional<float> x;
    Optional<float> y;
    if (optionalOriginObject) {
        if (!(x = optionalOriginObject->getNumber<float>("x"_s)))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'x' parameter was not found or invalid.");

        if (!(y = optionalOriginObject->getNumber<float>("y"_s)))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'y' parameter was not found or invalid.");

        if (x.value() < 0)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'x' parameter had an invalid value.");

        if (y.value() < 0)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'y' parameter had an invalid value.");
    }

    Optional<float> width;
    Optional<float> height;
    if (optionalSizeObject) {
        if (!(width = optionalSizeObject->getNumber<float>("width"_s)))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'width' parameter was not found or invalid.");

        if (!(height = optionalSizeObject->getNumber<float>("height"_s)))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'height' parameter was not found or invalid.");

        if (width.value() < 0)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'width' parameter had an invalid value.");

        if (height.value() < 0)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'height' parameter had an invalid value.");
    }

    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    exitFullscreenWindowForPage(*page, [this, protectedThis = makeRef(*this), callback = WTFMove(callback), page = makeRefPtr(page), width, height, x, y]() mutable {
        auto& webPage = *page;
        this->restoreWindowForPage(webPage, [callback = WTFMove(callback), page = WTFMove(page), width, height, x, y]() mutable {
            auto& webPage = *page;
            webPage.getWindowFrameWithCallback([callback = WTFMove(callback), page = WTFMove(page), width, height, x, y](WebCore::FloatRect originalFrame) mutable {
                WebCore::FloatRect newFrame = WebCore::FloatRect(WebCore::FloatPoint(x.valueOr(originalFrame.location().x()), y.valueOr(originalFrame.location().y())), WebCore::FloatSize(width.valueOr(originalFrame.size().width()), height.valueOr(originalFrame.size().height())));
                if (newFrame != originalFrame)
                    page->setWindowFrame(newFrame);
                
                callback->sendSuccess();
            });
        });
    });
}

static Optional<Inspector::Protocol::Automation::PageLoadStrategy> pageLoadStrategyFromStringParameter(const String* optionalPageLoadStrategyString)
{
    if (!optionalPageLoadStrategyString)
        return defaultPageLoadStrategy;

    auto parsedPageLoadStrategy = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::PageLoadStrategy>(*optionalPageLoadStrategyString);
    if (!parsedPageLoadStrategy)
        return WTF::nullopt;
    return parsedPageLoadStrategy;
}

void WebAutomationSession::waitForNavigationToComplete(const String& browsingContextHandle, const String* optionalFrameHandle, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<WaitForNavigationToCompleteCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    // If page is loading and there's an active JavaScript dialog is probably because the
    // dialog was started in an onload handler, so in case of normal page load strategy the
    // load will not finish until the dialog is dismissed. Instead of waiting for the timeout,
    // we return without waiting since we know it will timeout for sure. We want to check
    // arguments first, though.
    bool shouldTimeoutDueToUnexpectedAlert = pageLoadStrategy.value() == Inspector::Protocol::Automation::PageLoadStrategy::Normal
        && page->pageLoadState().isLoading() && m_client->isShowingJavaScriptDialogOnPage(*this, *page);

    if (optionalFrameHandle && !optionalFrameHandle->isEmpty()) {
        bool frameNotFound = false;
        auto frameID = webFrameIDForHandle(*optionalFrameHandle, frameNotFound);
        if (frameNotFound)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);
        WebFrameProxy* frame = page->process().webFrame(frameID.value());
        if (!frame)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);
        if (!shouldTimeoutDueToUnexpectedAlert)
            waitForNavigationToCompleteOnFrame(*frame, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
    } else {
        if (!shouldTimeoutDueToUnexpectedAlert)
            waitForNavigationToCompleteOnPage(*page, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
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
        m_pendingNormalNavigationInBrowsingContextCallbacksPerPage.set(page.pageID(), WTFMove(callback));
        break;
    case Inspector::Protocol::Automation::PageLoadStrategy::Eager:
        m_pendingEagerNavigationInBrowsingContextCallbacksPerPage.set(page.pageID(), WTFMove(callback));
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

void WebAutomationSession::respondToPendingPageNavigationCallbacksWithTimeout(HashMap<PageIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>>& map)
{
    Inspector::ErrorString timeoutError = STRING_FOR_PREDEFINED_ERROR_NAME(Timeout);
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
    for (auto& process : processPool.processes()) {
        if (auto* frame = process->webFrame(frameID))
            return frame->page();
    }
    return nullptr;
}

void WebAutomationSession::respondToPendingFrameNavigationCallbacksWithTimeout(HashMap<FrameIdentifier, RefPtr<Inspector::BackendDispatcher::CallbackBase>>& map)
{
    Inspector::ErrorString timeoutError = STRING_FOR_PREDEFINED_ERROR_NAME(Timeout);
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

void WebAutomationSession::maximizeWindowOfBrowsingContext(const String& browsingContextHandle, Ref<MaximizeWindowOfBrowsingContextCallback>&& callback)
{
    auto* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    exitFullscreenWindowForPage(*page, [this, protectedThis = makeRef(*this), callback = WTFMove(callback), page = makeRefPtr(page)]() mutable {
        auto& webPage = *page;
        restoreWindowForPage(webPage, [this, callback = WTFMove(callback), page = WTFMove(page)]() mutable {
            maximizeWindowForPage(*page, [callback = WTFMove(callback)]() {
                callback->sendSuccess();
            });
        });
    });
}

void WebAutomationSession::hideWindowOfBrowsingContext(const String& browsingContextHandle, Ref<HideWindowOfBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);
    
    exitFullscreenWindowForPage(*page, [protectedThis = makeRef(*this), callback = WTFMove(callback), page = makeRefPtr(page)]() mutable {
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
    
    m_windowStateTransitionCallback = WTF::Function<void(WindowTransitionedToState)> { [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](WindowTransitionedToState state) mutable {
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
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), page = makeRef(page)] {
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
                callback(WTF::nullopt);
            }
        }
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        if (!m_pendingKeyboardEventsFlushedCallbacksPerPage.isEmpty()) {
            for (auto key : copyToVector(m_pendingKeyboardEventsFlushedCallbacksPerPage.keys())) {
                auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(key);
                callback(WTF::nullopt);
            }
        }
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    });
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

void WebAutomationSession::navigateBrowsingContext(const String& handle, const String& url, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<NavigateBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->loadRequest(URL(URL(), url));
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::goBackInBrowsingContext(const String& handle, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<GoBackInBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->goBack();
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::goForwardInBrowsingContext(const String& handle, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<GoForwardInBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->goForward();
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::reloadBrowsingContext(const String& handle, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<ReloadBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->reload({ });
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::navigationOccurredForFrame(const WebFrameProxy& frame)
{
    if (frame.isMainFrame()) {
        // New page loaded, clear frame handles previously cached.
        m_handleWebFrameMap.clear();
        m_webFrameHandleMap.clear();
        if (auto callback = m_pendingNormalNavigationInBrowsingContextCallbacksPerPage.take(frame.page()->pageID())) {
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
        if (auto callback = m_pendingEagerNavigationInBrowsingContextCallbacksPerPage.take(frame.page()->pageID())) {
            m_loadTimer.stop();
            callback->sendSuccess(JSON::Object::create());
        }
    } else {
        if (auto callback = m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame.take(frame.frameID())) {
            m_loadTimer.stop();
            callback->sendSuccess(JSON::Object::create());
        }
    }
}

void WebAutomationSession::inspectorFrontendLoaded(const WebPageProxy& page)
{
    if (auto callback = m_pendingInspectorCallbacksPerPage.take(page.pageID()))
        callback->sendSuccess(JSON::Object::create());
}

void WebAutomationSession::mouseEventsFlushedForPage(const WebPageProxy& page)
{
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    if (auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(page.pageID()))
        callback(WTF::nullopt);
#else
    UNUSED_PARAM(page);
#endif
}

void WebAutomationSession::keyboardEventsFlushedForPage(const WebPageProxy& page)
{
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    if (auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.pageID()))
        callback(WTF::nullopt);
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
    if (auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(page.pageID()))
        callback(AUTOMATION_COMMAND_ERROR_WITH_NAME(WindowNotFound));
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    if (auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.pageID()))
        callback(AUTOMATION_COMMAND_ERROR_WITH_NAME(WindowNotFound));
#endif

#if ENABLE(WEBDRIVER_ACTIONS_API)
    // Then tell the input dispatcher to cancel so timers are stopped, and let it go out of scope.
    Optional<Ref<SimulatedInputDispatcher>> inputDispatcher = m_inputDispatchersByPage.take(page.pageID());
    if (inputDispatcher.hasValue())
        inputDispatcher.value()->cancel();
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

    String mappedMIMEType = WebCore::MIMETypeRegistry::getMIMETypeForExtension(extension).convertToASCIILowercase();
    if (mappedMIMEType.isEmpty())
        return false;
    
    if (allowedMIMETypes.contains(mappedMIMEType))
        return true;

    // Fall back to checking for a MIME type wildcard if an exact match is not found.
    Vector<String> components = mappedMIMEType.split('/');
    if (components.size() != 2)
        return false;

    String wildcardedMIMEType = makeString(components[0], "/*");
    if (allowedMIMETypes.contains(wildcardedMIMEType))
        return true;

    return false;
}

void WebAutomationSession::handleRunOpenPanel(const WebPageProxy& page, const WebFrameProxy&, const API::OpenPanelParameters& parameters, WebOpenPanelResultListenerProxy& resultListener)
{
    String browsingContextHandle = handleForWebPageProxy(page);
    if (!m_filesToSelectForFileUpload.size()) {
        resultListener.cancel();
        m_domainNotifier->fileChooserDismissed(browsingContextHandle, true);
        return;
    }

    if (m_filesToSelectForFileUpload.size() > 1 && !parameters.allowMultipleFiles()) {
        resultListener.cancel();
        m_domainNotifier->fileChooserDismissed(browsingContextHandle, true);
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
    // The REST API service can tell that this failed by checking the "files" attribute of the input element.
    for (const String& filename : m_filesToSelectForFileUpload) {
        if (!fileCanBeAcceptedForUpload(filename, allowedMIMETypes, allowedFileExtensions)) {
            resultListener.cancel();
            m_domainNotifier->fileChooserDismissed(browsingContextHandle, true);
            return;
        }
    }

    resultListener.chooseFiles(m_filesToSelectForFileUpload);
    m_domainNotifier->fileChooserDismissed(browsingContextHandle, false);
}

void WebAutomationSession::evaluateJavaScriptFunction(const String& browsingContextHandle, const String* optionalFrameHandle, const String& function, const JSON::Array& arguments, const bool* optionalExpectsImplicitCallbackArgument, const int* optionalCallbackTimeout, Ref<EvaluateJavaScriptFunctionCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    Vector<String> argumentsVector;
    argumentsVector.reserveCapacity(arguments.length());

    for (auto& argument : arguments) {
        String argumentString;
        argument->asString(argumentString);
        argumentsVector.uncheckedAppend(argumentString);
    }

    bool expectsImplicitCallbackArgument = optionalExpectsImplicitCallbackArgument ? *optionalExpectsImplicitCallbackArgument : false;
    int callbackTimeout = optionalCallbackTimeout ? *optionalCallbackTimeout : 0;

    uint64_t callbackID = m_nextEvaluateJavaScriptCallbackID++;
    m_evaluateJavaScriptFunctionCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::EvaluateJavaScriptFunction(page->pageID(), frameID, function, argumentsVector, expectsImplicitCallbackArgument, callbackTimeout, callbackID), 0);
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

void WebAutomationSession::resolveChildFrameHandle(const String& browsingContextHandle, const String* optionalFrameHandle, const int* optionalOrdinal, const String* optionalName, const String* optionalNodeHandle, Ref<ResolveChildFrameHandleCallback>&& callback)
{
    if (!optionalOrdinal && !optionalName && !optionalNodeHandle)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "Command must specify a child frame by ordinal, name, or element handle.");

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    WTF::CompletionHandler<void(Optional<String>, Optional<FrameIdentifier>)> completionHandler = [this, protectedThis = makeRef(*this), callback = callback.copyRef()](Optional<String> errorType, Optional<FrameIdentifier> frameID) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess(handleForWebFrameID(frameID));
    };

    if (optionalNodeHandle) {
        page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ResolveChildFrameWithNodeHandle(page->pageID(), frameID, *optionalNodeHandle), WTFMove(completionHandler));
        return;
    }

    if (optionalName) {
        page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ResolveChildFrameWithName(page->pageID(), frameID, *optionalName), WTFMove(completionHandler));
        return;
    }

    if (optionalOrdinal) {
        page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ResolveChildFrameWithOrdinal(page->pageID(), frameID, *optionalOrdinal), WTFMove(completionHandler));
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebAutomationSession::resolveParentFrameHandle(const String& browsingContextHandle, const String& frameHandle, Ref<ResolveParentFrameHandleCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    WTF::CompletionHandler<void(Optional<String>, Optional<FrameIdentifier>)> completionHandler = [this, protectedThis = makeRef(*this), callback = callback.copyRef()](Optional<String> errorType, Optional<FrameIdentifier> frameID) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess(handleForWebFrameID(frameID));
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ResolveParentFrame(page->pageID(), frameID), WTFMove(completionHandler));
}

static Optional<CoordinateSystem> protocolStringToCoordinateSystem(const String& coordinateSystemString)
{
    if (coordinateSystemString == "Page")
        return CoordinateSystem::Page;
    if (coordinateSystemString == "LayoutViewport")
        return CoordinateSystem::LayoutViewport;
    return WTF::nullopt;
}

void WebAutomationSession::computeElementLayout(const String& browsingContextHandle, const String& frameHandle, const String& nodeHandle, const bool* optionalScrollIntoViewIfNeeded, const String& coordinateSystemString, Ref<ComputeElementLayoutCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    Optional<CoordinateSystem> coordinateSystem = protocolStringToCoordinateSystem(coordinateSystemString);
    if (!coordinateSystem)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'coordinateSystem' is invalid.");

    WTF::CompletionHandler<void(Optional<String>, WebCore::IntRect, Optional<WebCore::IntPoint>, bool)> completionHandler = [callback = callback.copyRef()](Optional<String> errorType, WebCore::IntRect rect, Optional<WebCore::IntPoint> inViewCenterPoint, bool isObscured) mutable {
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
    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ComputeElementLayout(page->pageID(), frameID, nodeHandle, scrollIntoViewIfNeeded, coordinateSystem.value()), WTFMove(completionHandler));
}

void WebAutomationSession::selectOptionElement(const String& browsingContextHandle, const String& frameHandle, const String& nodeHandle, Ref<SelectOptionElementCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    WTF::CompletionHandler<void(Optional<String>)> completionHandler = [callback = callback.copyRef()](Optional<String> errorType) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }
        
        callback->sendSuccess();
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::SelectOptionElement(page->pageID(), frameID, nodeHandle), WTFMove(completionHandler));
}

void WebAutomationSession::isShowingJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle, bool* result)
{
    ASSERT(m_client);
    if (!m_client)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    *result = m_client->isShowingJavaScriptDialogOnPage(*this, *page);
}

void WebAutomationSession::dismissCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle)
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
}

void WebAutomationSession::acceptCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle)
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
}

void WebAutomationSession::messageOfCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle, String* text)
{
    ASSERT(m_client);
    if (!m_client)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        SYNC_FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    *text = m_client->messageOfCurrentJavaScriptDialogOnPage(*this, *page);
}

void WebAutomationSession::setUserInputForCurrentJavaScriptPrompt(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& promptValue)
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
}

void WebAutomationSession::setFilesToSelectForFileUpload(ErrorString& errorString, const String& browsingContextHandle, const JSON::Array& filenames, const JSON::Array* fileContents)
{
    Vector<String> newFileList;
    newFileList.reserveInitialCapacity(filenames.length());

    if (fileContents && fileContents->length() != filenames.length())
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The parameters 'filenames' and 'fileContents' must have equal length.");

    for (size_t i = 0; i < filenames.length(); ++i) {
        String filename;
        if (!filenames.get(i)->asString(filename))
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The parameter 'filenames' contains a non-string value.");

        if (!fileContents) {
            newFileList.append(filename);
            continue;
        }

        String fileData;
        if (!fileContents->get(i)->asString(fileData))
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The parameter 'fileContents' contains a non-string value.");

        Optional<String> localFilePath = platformGenerateLocalFilePathForRemoteFile(filename, fileData);
        if (!localFilePath)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote file could not be saved to a local temporary directory.");

        newFileList.append(localFilePath.value());
    }

    m_filesToSelectForFileUpload.swap(newFileList);
}

static Ref<Inspector::Protocol::Automation::Cookie> buildObjectForCookie(const WebCore::Cookie& cookie)
{
    return Inspector::Protocol::Automation::Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires ? cookie.expires / 1000 : 0)
        .setSize((cookie.name.length() + cookie.value.length()))
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSession(cookie.session)
        .release();
}

static Ref<JSON::ArrayOf<Inspector::Protocol::Automation::Cookie>> buildArrayForCookies(Vector<WebCore::Cookie>& cookiesList)
{
    auto cookies = JSON::ArrayOf<Inspector::Protocol::Automation::Cookie>::create();

    for (const auto& cookie : cookiesList)
        cookies->addItem(buildObjectForCookie(cookie));

    return cookies;
}

void WebAutomationSession::getAllCookies(const String& browsingContextHandle, Ref<GetAllCookiesCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WTF::CompletionHandler<void(Optional<String>, Vector<WebCore::Cookie>)> completionHandler = [callback = callback.copyRef()](Optional<String> errorType, Vector<WebCore::Cookie> cookies) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess(buildArrayForCookies(cookies));
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::GetCookiesForFrame(page->pageID(), WTF::nullopt), WTFMove(completionHandler));
}

void WebAutomationSession::deleteSingleCookie(const String& browsingContextHandle, const String& cookieName, Ref<DeleteSingleCookieCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WTF::CompletionHandler<void(Optional<String>)> completionHandler = [callback = callback.copyRef()](Optional<String> errorType) mutable {
        if (errorType) {
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(*errorType));
            return;
        }

        callback->sendSuccess();
    };

    page->process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::DeleteCookie(page->pageID(), WTF::nullopt, cookieName), WTFMove(completionHandler));
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
    
void WebAutomationSession::addSingleCookie(const String& browsingContextHandle, const JSON::Object& cookieObject, Ref<AddSingleCookieCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    URL activeURL = URL(URL(), page->pageLoadState().activeURL());
    ASSERT(activeURL.isValid());

    WebCore::Cookie cookie;

    if (!cookieObject.getString("name"_s, cookie.name))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'name' was not found.");

    if (!cookieObject.getString("value"_s, cookie.value))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'value' was not found.");

    String domain;
    if (!cookieObject.getString("domain"_s, domain))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'domain' was not found.");

    // Inherit the domain/host from the main frame's URL if it is not explicitly set.
    if (domain.isEmpty())
        cookie.domain = activeURL.host().toString();
    else
        cookie.domain = domainByAddingDotPrefixIfNeeded(domain);

    if (!cookieObject.getString("path"_s, cookie.path))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'path' was not found.");

    double expires;
    if (!cookieObject.getDouble("expires"_s, expires))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'expires' was not found.");

    cookie.expires = expires * 1000.0;

    if (!cookieObject.getBoolean("secure"_s, cookie.secure))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'secure' was not found.");

    if (!cookieObject.getBoolean("session"_s, cookie.session))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'session' was not found.");

    if (!cookieObject.getBoolean("httpOnly"_s, cookie.httpOnly))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'httpOnly' was not found.");

    WebCookieManagerProxy* cookieManager = m_processPool->supplement<WebCookieManagerProxy>();
    cookieManager->setCookies(page->websiteDataStore().sessionID(), { cookie }, [callback = callback.copyRef()](CallbackBase::Error error) {
        if (error == CallbackBase::Error::None)
            callback->sendSuccess();
        else
            callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError));
    });
}

void WebAutomationSession::deleteAllCookies(ErrorString& errorString, const String& browsingContextHandle)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        SYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    URL activeURL = URL(URL(), page->pageLoadState().activeURL());
    ASSERT(activeURL.isValid());

    String host = activeURL.host().toString();

    WebCookieManagerProxy* cookieManager = m_processPool->supplement<WebCookieManagerProxy>();
    cookieManager->deleteCookiesForHostnames(page->websiteDataStore().sessionID(), { host, domainByAddingDotPrefixIfNeeded(host) });
}

void WebAutomationSession::getSessionPermissions(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>>& out_permissions)
{
    auto permissionsObjectArray = JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>::create();
    auto getUserMediaPermissionObject = Inspector::Protocol::Automation::SessionPermissionData::create()
        .setPermission(Inspector::Protocol::Automation::SessionPermission::GetUserMedia)
        .setValue(m_permissionForGetUserMedia)
        .release();

    permissionsObjectArray->addItem(WTFMove(getUserMediaPermissionObject));
    out_permissions = WTFMove(permissionsObjectArray);
}

void WebAutomationSession::setSessionPermissions(ErrorString& errorString, const JSON::Array& permissions)
{
    for (auto it = permissions.begin(); it != permissions.end(); ++it) {
        RefPtr<JSON::Object> permission;
        if (!it->get()->asObject(permission))
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'permissions' is invalid.");

        String permissionName;
        if (!permission->getString("permission"_s, permissionName))
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'permission' is missing or invalid.");

        auto parsedPermissionName = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::SessionPermission>(permissionName);
        if (!parsedPermissionName)
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'permission' has an unknown value.");

        bool permissionValue;
        if (!permission->getBoolean("value"_s, permissionValue))
            SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'value' is missing or invalid.");

        switch (parsedPermissionName.value()) {
        case Inspector::Protocol::Automation::SessionPermission::GetUserMedia:
            m_permissionForGetUserMedia = permissionValue;
            break;
        }
    }
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
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
    if (m_simulatingTouchInteraction)
        return true;
#endif
    return false;
}

#if ENABLE(WEBDRIVER_ACTIONS_API)
SimulatedInputDispatcher& WebAutomationSession::inputDispatcherForPage(WebPageProxy& page)
{
    return m_inputDispatchersByPage.ensure(page.pageID(), [&] {
        return SimulatedInputDispatcher::create(page, *this);
    }).iterator->value;
}

SimulatedInputSource* WebAutomationSession::inputSourceForType(SimulatedInputSourceType type) const
{
    // FIXME: this should use something like Vector's findMatching().
    for (auto& inputSource : m_inputSources) {
        if (inputSource->type == type)
            return &inputSource.get();
    }

    return nullptr;
}

// MARK: SimulatedInputDispatcher::Client API
void WebAutomationSession::viewportInViewCenterPointOfElement(WebPageProxy& page, Optional<FrameIdentifier> frameID, const String& nodeHandle, Function<void (Optional<WebCore::IntPoint>, Optional<AutomationCommandError>)>&& completionHandler)
{
    WTF::CompletionHandler<void(Optional<String>, WebCore::IntRect, Optional<WebCore::IntPoint>, bool)> didComputeElementLayoutHandler = [completionHandler = WTFMove(completionHandler)](Optional<String> errorType, WebCore::IntRect, Optional<WebCore::IntPoint> inViewCenterPoint, bool) mutable {
        if (errorType) {
            completionHandler(WTF::nullopt, AUTOMATION_COMMAND_ERROR_WITH_MESSAGE(*errorType));
            return;
        }

        if (!inViewCenterPoint) {
            completionHandler(WTF::nullopt, AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
            return;
        }

        completionHandler(inViewCenterPoint, WTF::nullopt);
    };

    page.process().sendWithAsyncReply(Messages::WebAutomationSessionProxy::ComputeElementLayout(page.pageID(), frameID, nodeHandle, false, CoordinateSystem::LayoutViewport), WTFMove(didComputeElementLayoutHandler));
}

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

void WebAutomationSession::simulateMouseInteraction(WebPageProxy& page, MouseInteraction interaction, MouseButton mouseButton, const WebCore::IntPoint& locationInViewport, CompletionHandler<void(Optional<AutomationCommandError>)>&& completionHandler)
{
    page.getWindowFrameWithCallback([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), page = makeRef(page), interaction, mouseButton, locationInViewport](WebCore::FloatRect windowFrame) mutable {
        auto clippedX = std::min(std::max(0.0f, (float)locationInViewport.x()), windowFrame.size().width());
        auto clippedY = std::min(std::max(0.0f, (float)locationInViewport.y()), windowFrame.size().height());
        if (clippedX != locationInViewport.x() || clippedY != locationInViewport.y()) {
            completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
            return;
        }

        // Bridge the flushed callback to our command's completion handler.
        auto mouseEventsFlushedCallback = [completionHandler = WTFMove(completionHandler)](Optional<AutomationCommandError> error) mutable {
            completionHandler(error);
        };

        auto& callbackInMap = m_pendingMouseEventsFlushedCallbacksPerPage.add(page->pageID(), nullptr).iterator->value;
        if (callbackInMap)
            callbackInMap(AUTOMATION_COMMAND_ERROR_WITH_NAME(Timeout));
        callbackInMap = WTFMove(mouseEventsFlushedCallback);

        platformSimulateMouseInteraction(page, interaction, mouseButton, locationInViewport, OptionSet<WebEvent::Modifier>::fromRaw(m_currentModifiers));

        // If the event does not hit test anything in the window, then it may not have been delivered.
        if (callbackInMap && !page->isProcessingMouseEvents()) {
            auto callbackToCancel = m_pendingMouseEventsFlushedCallbacksPerPage.take(page->pageID());
            callbackToCancel(WTF::nullopt);
        }

        // Otherwise, wait for mouseEventsFlushedCallback to run when all events are handled.
    });
}
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
void WebAutomationSession::simulateTouchInteraction(WebPageProxy& page, TouchInteraction interaction, const WebCore::IntPoint& locationInViewport, Optional<Seconds> duration, CompletionHandler<void(Optional<AutomationCommandError>)>&& completionHandler)
{
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect visualViewportBounds = WebCore::FloatRect({ }, page.unobscuredContentRect().size());
    if (!visualViewportBounds.contains(locationInViewport)) {
        completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
        return;
    }
#endif

    m_simulatingTouchInteraction = true;
    platformSimulateTouchInteraction(page, interaction, locationInViewport, duration, [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](Optional<AutomationCommandError> error) mutable {
        m_simulatingTouchInteraction = false;
        completionHandler(error);
    });
}
#endif // ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
void WebAutomationSession::simulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, WTF::Variant<VirtualKey, CharKey>&& key, CompletionHandler<void(Optional<AutomationCommandError>)>&& completionHandler)
{
    // Bridge the flushed callback to our command's completion handler.
    auto keyboardEventsFlushedCallback = [completionHandler = WTFMove(completionHandler)](Optional<AutomationCommandError> error) mutable {
        completionHandler(error);
    };

    auto& callbackInMap = m_pendingKeyboardEventsFlushedCallbacksPerPage.add(page.pageID(), nullptr).iterator->value;
    if (callbackInMap)
        callbackInMap(AUTOMATION_COMMAND_ERROR_WITH_NAME(Timeout));
    callbackInMap = WTFMove(keyboardEventsFlushedCallback);

    platformSimulateKeyboardInteraction(page, interaction, WTFMove(key));

    // If the interaction does not generate any events, then do not wait for events to be flushed.
    // This happens in some corner cases on macOS, such as releasing a key while Command is pressed.
    if (callbackInMap && !page.isProcessingKeyboardEvents()) {
        auto callbackToCancel = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.pageID());
        callbackToCancel(WTF::nullopt);
    }

    // Otherwise, wait for keyboardEventsFlushedCallback to run when all events are handled.
}
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
#endif // ENABLE(WEBDRIVER_ACTIONS_API)

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
static WebEvent::Modifier protocolModifierToWebEventModifier(Inspector::Protocol::Automation::KeyModifier modifier)
{
    switch (modifier) {
    case Inspector::Protocol::Automation::KeyModifier::Alt:
        return WebEvent::Modifier::AltKey;
    case Inspector::Protocol::Automation::KeyModifier::Meta:
        return WebEvent::Modifier::MetaKey;
    case Inspector::Protocol::Automation::KeyModifier::Control:
        return WebEvent::Modifier::ControlKey;
    case Inspector::Protocol::Automation::KeyModifier::Shift:
        return WebEvent::Modifier::ShiftKey;
    case Inspector::Protocol::Automation::KeyModifier::CapsLock:
        return WebEvent::Modifier::CapsLockKey;
    }

    RELEASE_ASSERT_NOT_REACHED();
}
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

void WebAutomationSession::performMouseInteraction(const String& handle, const JSON::Object& requestedPositionObject, const String& mouseButtonString, const String& mouseInteractionString, const JSON::Array& keyModifierStrings, Ref<PerformMouseInteractionCallback>&& callback)
{
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    float x;
    if (!requestedPositionObject.getDouble("x"_s, x))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'x' was not found.");

    float y;
    if (!requestedPositionObject.getDouble("y"_s, y))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'y' was not found.");

    OptionSet<WebEvent::Modifier> keyModifiers;
    for (auto it = keyModifierStrings.begin(); it != keyModifierStrings.end(); ++it) {
        String modifierString;
        if (!it->get()->asString(modifierString))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'modifiers' is invalid.");

        auto parsedModifier = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::KeyModifier>(modifierString);
        if (!parsedModifier)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "A modifier in the 'modifiers' array is invalid.");
        keyModifiers.add(protocolModifierToWebEventModifier(parsedModifier.value()));
    }

    page->getWindowFrameWithCallback([this, protectedThis = makeRef(*this), callback = WTFMove(callback), page = makeRef(*page), x, y, mouseInteractionString, mouseButtonString, keyModifiers](WebCore::FloatRect windowFrame) mutable {

        x = std::min(std::max(0.0f, x), windowFrame.size().width());
        y = std::min(std::max(0.0f, y), windowFrame.size().height());

        WebCore::IntPoint locationInViewport = WebCore::IntPoint(static_cast<int>(x), static_cast<int>(y));

        auto parsedInteraction = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseInteraction>(mouseInteractionString);
        if (!parsedInteraction)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'interaction' is invalid.");

        auto parsedButton = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseButton>(mouseButtonString);
        if (!parsedButton)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'button' is invalid.");

        auto mouseEventsFlushedCallback = [protectedThis = WTFMove(protectedThis), callback = WTFMove(callback), page = page.copyRef(), x, y](Optional<AutomationCommandError> error) {
            if (error)
                callback->sendFailure(error.value().toProtocolString());
            else {
                callback->sendSuccess(Inspector::Protocol::Automation::Point::create()
                    .setX(x)
                    .setY(y - page->topContentInset())
                    .release());
            }
        };

        auto& callbackInMap = m_pendingMouseEventsFlushedCallbacksPerPage.add(page->pageID(), nullptr).iterator->value;
        if (callbackInMap)
            callbackInMap(AUTOMATION_COMMAND_ERROR_WITH_NAME(Timeout));
        callbackInMap = WTFMove(mouseEventsFlushedCallback);

        platformSimulateMouseInteraction(page, parsedInteraction.value(), parsedButton.value(), locationInViewport, keyModifiers);

        // If the event location was previously clipped and does not hit test anything in the window, then it will not be processed.
        // For compatibility with pre-W3C driver implementations, don't make this a hard error; just do nothing silently.
        // In W3C-only code paths, we can reject any pointer actions whose coordinates are outside the viewport rect.
        if (callbackInMap && !page->isProcessingMouseEvents()) {
            auto callbackToCancel = m_pendingMouseEventsFlushedCallbacksPerPage.take(page->pageID());
            callbackToCancel(WTF::nullopt);
        }
    });
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
}

void WebAutomationSession::performKeyboardInteractions(const String& handle, const JSON::Array& interactions, Ref<PerformKeyboardInteractionsCallback>&& callback)
{
#if !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!interactions.length())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'interactions' was not found or empty.");

    // Validate all of the parameters before performing any interactions with the browsing context under test.
    Vector<WTF::Function<void()>> actionsToPerform;
    actionsToPerform.reserveCapacity(interactions.length());

    for (const auto& interaction : interactions) {
        RefPtr<JSON::Object> interactionObject;
        if (!interaction->asObject(interactionObject))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter was invalid.");

        String interactionTypeString;
        if (!interactionObject->getString("type"_s, interactionTypeString))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter is missing the 'type' key.");
        auto interactionType = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::KeyboardInteractionType>(interactionTypeString);
        if (!interactionType)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'type' key.");

        String virtualKeyString;
        bool foundVirtualKey = interactionObject->getString("key"_s, virtualKeyString);
        if (foundVirtualKey) {
            Optional<VirtualKey> virtualKey = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::VirtualKey>(virtualKeyString);
            if (!virtualKey)
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'key' value.");

            actionsToPerform.uncheckedAppend([this, page, interactionType, virtualKey] {
                platformSimulateKeyboardInteraction(*page, interactionType.value(), virtualKey.value());
            });
        }

        String keySequence;
        bool foundKeySequence = interactionObject->getString("text"_s, keySequence);
        if (foundKeySequence) {
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

        if (!foundVirtualKey && !foundKeySequence)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "An interaction in the 'interactions' parameter is missing both 'key' and 'text'. One must be provided.");
    }

    ASSERT(actionsToPerform.size());
    if (!actionsToPerform.size())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "No actions to perform.");

    auto keyboardEventsFlushedCallback = [protectedThis = makeRef(*this), callback = WTFMove(callback), page = makeRef(*page)](Optional<AutomationCommandError> error) {
        if (error)
            callback->sendFailure(error.value().toProtocolString());
        else
            callback->sendSuccess();
    };

    auto& callbackInMap = m_pendingKeyboardEventsFlushedCallbacksPerPage.add(page->pageID(), nullptr).iterator->value;
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
    }

    RELEASE_ASSERT_NOT_REACHED();
}
#endif // ENABLE(WEBDRIVER_ACTIONS_API)

void WebAutomationSession::performInteractionSequence(const String& handle, const String* optionalFrameHandle, const JSON::Array& inputSources, const JSON::Array& steps, Ref<WebAutomationSession::PerformInteractionSequenceCallback>&& callback)
{
    // This command implements WebKit support for §17.5 Perform Actions.

#if !ENABLE(WEBDRIVER_ACTIONS_API)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    HashMap<String, Ref<SimulatedInputSource>> sourceIdToInputSourceMap;
    HashMap<SimulatedInputSourceType, String, WTF::IntHash<SimulatedInputSourceType>, WTF::StrongEnumHashTraits<SimulatedInputSourceType>> typeToSourceIdMap;

    // Parse and validate Automation protocol arguments. By this point, the driver has
    // already performed the steps in §17.3 Processing Actions Requests.
    if (!inputSources.length())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'inputSources' was not found or empty.");

    for (const auto& inputSource : inputSources) {
        RefPtr<JSON::Object> inputSourceObject;
        if (!inputSource->asObject(inputSourceObject))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An input source in the 'inputSources' parameter was invalid.");
        
        String sourceId;
        if (!inputSourceObject->getString("sourceId"_s, sourceId))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An input source in the 'inputSources' parameter is missing a 'sourceId'.");

        String sourceType;
        if (!inputSourceObject->getString("sourceType"_s, sourceType))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An input source in the 'inputSources' parameter is missing a 'sourceType'.");

        auto parsedInputSourceType = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::InputSourceType>(sourceType);
        if (!parsedInputSourceType)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An input source in the 'inputSources' parameter has an invalid 'sourceType'.");

        SimulatedInputSourceType inputSourceType = simulatedInputSourceTypeFromProtocolSourceType(*parsedInputSourceType);

        // Note: iOS does not support mouse input sources, and other platforms do not support touch input sources.
        // If a mismatch happens, alias to the supported input source. This works because both Mouse and Touch input sources
        // use a MouseButton to indicate the result of interacting (down/up/move), which can be interpreted for touch or mouse.
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) && ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Mouse)
            inputSourceType = SimulatedInputSourceType::Touch;
#endif
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Mouse)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "Mouse input sources are not yet supported.");
#endif
#if !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Touch)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "Touch input sources are not yet supported.");
#endif
#if !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        if (inputSourceType == SimulatedInputSourceType::Keyboard)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "Keyboard input sources are not yet supported.");
#endif
        if (typeToSourceIdMap.contains(inputSourceType))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Two input sources with the same type were specified.");
        if (sourceIdToInputSourceMap.contains(sourceId))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Two input sources with the same sourceId were specified.");

        typeToSourceIdMap.add(inputSourceType, sourceId);
        sourceIdToInputSourceMap.add(sourceId, *inputSourceForType(inputSourceType));
    }

    Vector<SimulatedInputKeyFrame> keyFrames;

    if (!steps.length())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'steps' was not found or empty.");

    for (const auto& step : steps) {
        RefPtr<JSON::Object> stepObject;
        if (!step->asObject(stepObject))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "A step in the 'steps' parameter was not an object.");

        RefPtr<JSON::Array> stepStates;
        if (!stepObject->getArray("states"_s, stepStates))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "A step is missing the 'states' property.");

        Vector<SimulatedInputKeyFrame::StateEntry> entries;
        entries.reserveCapacity(stepStates->length());

        for (const auto& state : *stepStates) {
            RefPtr<JSON::Object> stateObject;
            if (!state->asObject(stateObject))
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Encountered a non-object step state.");

            String sourceId;
            if (!stateObject->getString("sourceId"_s, sourceId))
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Step state lacks required 'sourceId' property.");

            if (!sourceIdToInputSourceMap.contains(sourceId))
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Unknown 'sourceId' specified.");

            SimulatedInputSource& inputSource = *sourceIdToInputSourceMap.get(sourceId);
            SimulatedInputSourceState sourceState { };

            String pressedCharKeyString;
            if (stateObject->getString("pressedCharKey"_s, pressedCharKeyString))
                sourceState.pressedCharKey = pressedCharKeyString.characterAt(0);

            RefPtr<JSON::Array> pressedVirtualKeysArray;
            if (stateObject->getArray("pressedVirtualKeys"_s, pressedVirtualKeysArray)) {
                VirtualKeySet pressedVirtualKeys { };

                for (auto it = pressedVirtualKeysArray->begin(); it != pressedVirtualKeysArray->end(); ++it) {
                    String pressedVirtualKeyString;
                    if (!(*it)->asString(pressedVirtualKeyString))
                        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Encountered a non-string virtual key value.");

                    Optional<VirtualKey> parsedVirtualKey = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::VirtualKey>(pressedVirtualKeyString);
                    if (!parsedVirtualKey)
                        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Encountered an unknown virtual key value.");
                    else
                        pressedVirtualKeys.add(parsedVirtualKey.value());
                }

                sourceState.pressedVirtualKeys = pressedVirtualKeys;
            }

            String pressedButtonString;
            if (stateObject->getString("pressedButton"_s, pressedButtonString)) {
                auto protocolButton = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseButton>(pressedButtonString);
                sourceState.pressedMouseButton = protocolButton.valueOr(MouseButton::None);
            }

            String originString;
            if (stateObject->getString("origin"_s, originString))
                sourceState.origin = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseMoveOrigin>(originString);

            if (sourceState.origin && sourceState.origin.value() == Inspector::Protocol::Automation::MouseMoveOrigin::Element) {
                String nodeHandleString;
                if (!stateObject->getString("nodeHandle"_s, nodeHandleString))
                    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "Node handle not provided for 'Element' origin");
                sourceState.nodeHandle = nodeHandleString;
            }

            RefPtr<JSON::Object> locationObject;
            if (stateObject->getObject("location"_s, locationObject)) {
                int x, y;
                if (locationObject->getInteger("x"_s, x) && locationObject->getInteger("y"_s, y))
                    sourceState.location = WebCore::IntPoint(x, y);
            }

            int parsedDuration;
            if (stateObject->getInteger("duration"_s, parsedDuration))
                sourceState.duration = Seconds::fromMilliseconds(parsedDuration);

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
    inputDispatcher.run(frameID, WTFMove(keyFrames), m_inputSources, [protectedThis = makeRef(*this), callback = WTFMove(callback)](Optional<AutomationCommandError> error) {
        if (error)
            callback->sendFailure(error.value().toProtocolString());
        else
            callback->sendSuccess();
    });
#endif // ENABLE(WEBDRIVER_ACTIONS_API)
}

void WebAutomationSession::cancelInteractionSequence(const String& handle, const String* optionalFrameHandle, Ref<CancelInteractionSequenceCallback>&& callback)
{
    // This command implements WebKit support for §17.6 Release Actions.

#if !ENABLE(WEBDRIVER_ACTIONS_API)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    Vector<SimulatedInputKeyFrame> keyFrames({ SimulatedInputKeyFrame::keyFrameToResetInputSources(m_inputSources) });
    SimulatedInputDispatcher& inputDispatcher = inputDispatcherForPage(*page);
    inputDispatcher.cancel();
    
    inputDispatcher.run(frameID, WTFMove(keyFrames), m_inputSources, [protectedThis = makeRef(*this), callback = WTFMove(callback)](Optional<AutomationCommandError> error) {
        if (error)
            callback->sendFailure(error.value().toProtocolString());
        else
            callback->sendSuccess();
    });
#endif // ENABLE(WEBDRIVER_ACTIONS_API)
}

void WebAutomationSession::takeScreenshot(const String& handle, const String* optionalFrameHandle, const String* optionalNodeHandle, const bool* optionalScrollIntoViewIfNeeded, const bool* optionalClipToViewport, Ref<TakeScreenshotCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), frameNotFound);
    if (frameNotFound)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    String nodeHandle = optionalNodeHandle ? *optionalNodeHandle : emptyString();
    bool clipToViewport = optionalClipToViewport ? *optionalClipToViewport : false;

    uint64_t callbackID = m_nextScreenshotCallbackID++;
    m_screenshotCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::TakeScreenshot(page->pageID(), frameID, nodeHandle, scrollIntoViewIfNeeded, clipToViewport, callbackID), 0);
}

void WebAutomationSession::didTakeScreenshot(uint64_t callbackID, const ShareableBitmap::Handle& imageDataHandle, const String& errorType)
{
    auto callback = m_screenshotCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty()) {
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType));
        return;
    }

    Optional<String> base64EncodedData = platformGetBase64EncodedPNGData(imageDataHandle);
    if (!base64EncodedData)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);

    callback->sendSuccess(base64EncodedData.value());
}

#if !PLATFORM(COCOA) && !USE(CAIRO)
Optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ShareableBitmap::Handle&)
{
    return WTF::nullopt;
}
#endif // !PLATFORM(COCOA) && !USE(CAIRO)

#if !PLATFORM(COCOA)
Optional<String> WebAutomationSession::platformGenerateLocalFilePathForRemoteFile(const String&, const String&)
{
    return WTF::nullopt;
}
#endif // !PLATFORM(COCOA)

} // namespace WebKit
