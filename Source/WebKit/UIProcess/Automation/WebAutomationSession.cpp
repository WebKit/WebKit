/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "APIString.h"
#include "AutomationProtocolObjects.h"
#include "CoordinateSystem.h"
#include "PageLoadState.h"
#include "WebAutomationSessionMacros.h"
#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebBackForwardList.h"
#include "WebDriverBidiFrontendDispatchers.h"
#include "WebFrameProxy.h"
#include "WebFullScreenManagerProxy.h"
#include "WebInspectorUIProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/PointerEventTypeNames.h>
#include <algorithm>
#include <wtf/CallbackAggregator.h>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>

#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
#include <wtf/text/TextBreakIterator.h>
#endif

#if ENABLE(WEB_AUTHN)
#include "VirtualAuthenticatorManager.h"
#include <WebCore/AuthenticatorTransport.h>
#endif

#if ENABLE(WEBDRIVER_BIDI)
#include "BidiBrowserAgent.h"
#include "WebDriverBidiProcessor.h"
#endif

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

#if ENABLE(REMOTE_INSPECTOR)
auto WebAutomationSession::Debuggable::create(WebAutomationSession& session) -> Ref<Debuggable>
{
    return adoptRef(*new Debuggable(session));
}

WebAutomationSession::Debuggable::Debuggable(WebAutomationSession& session)
    : m_session(&session)
{
}

void WebAutomationSession::Debuggable::sessionDestroyed()
{
    m_session = nullptr;
}

String WebAutomationSession::Debuggable::name() const
{
    String name;
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, &name] {
        if (RefPtr session = m_session.get())
            name = session->name().isolatedCopy();
    });
    return name;
}

void WebAutomationSession::Debuggable::dispatchMessageFromRemote(String&& message)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, message = WTFMove(message).isolatedCopy()]() mutable {
        if (RefPtr session = m_session.get())
            session->dispatchMessageFromRemote(WTFMove(message));
    });
}

void WebAutomationSession::Debuggable::connect(Inspector::FrontendChannel& channel, bool isAutomaticConnection, bool immediatelyPause)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, &channel, isAutomaticConnection, immediatelyPause] {
        if (RefPtr session = m_session.get())
            session->connect(channel, isAutomaticConnection, immediatelyPause);
    });
}

void WebAutomationSession::Debuggable::disconnect(Inspector::FrontendChannel& channel)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, &channel] {
        if (RefPtr session = m_session.get())
            session->disconnect(channel);
    });
}
#endif // ENABLE(REMOTE_INSPECTOR)

WebAutomationSession::WebAutomationSession()
    : m_client(makeUnique<API::AutomationSessionClient>())
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_domainDispatcher(AutomationBackendDispatcher::create(m_backendDispatcher, this))
    , m_domainNotifier(makeUniqueRef<AutomationFrontendDispatcher>(m_frontendRouter))
#if ENABLE(WEBDRIVER_BIDI)
    , m_bidiProcessor(makeUniqueRef<WebDriverBidiProcessor>(*this))
#endif
    , m_loadTimer(RunLoop::mainSingleton(), "WebAutomationSession::LoadTimer"_s, this, &WebAutomationSession::loadTimerFired)
#if ENABLE(REMOTE_INSPECTOR)
    , m_debuggable(Debuggable::create(*this))
#endif
{
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    m_mouseButtonsCurrentlyDown.reserveInitialCapacity(3);
#endif
}

WebAutomationSession::~WebAutomationSession()
{
    ASSERT(!m_client);
    ASSERT(!m_processPool);
#if ENABLE(REMOTE_INSPECTOR)
    m_debuggable->sessionDestroyed();
#endif
}

void WebAutomationSession::setClient(std::unique_ptr<API::AutomationSessionClient>&& client)
{
    m_client = WTFMove(client);
}

void WebAutomationSession::setProcessPool(WebKit::WebProcessPool* processPool)
{
    if (RefPtr pool = m_processPool.get())
        pool->removeMessageReceiver(Messages::WebAutomationSession::messageReceiverName());

    m_processPool = processPool;

    if (RefPtr pool = m_processPool.get())
        pool->addMessageReceiver(Messages::WebAutomationSession::messageReceiverName(), *this);
}

RefPtr<WebProcessPool> WebAutomationSession::protectedProcessPool() const
{
    return const_cast<WebProcessPool*>(m_processPool.get());
}

// NOTE: this class could be split at some point to support local and remote automation sessions.
// For now, it only works with a remote automation driver over a RemoteInspector connection.

#if ENABLE(REMOTE_INSPECTOR)

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

    m_debuggable->setIsPaired(true);
}

void WebAutomationSession::disconnect(Inspector::FrontendChannel& channel)
{
    ASSERT(&channel == m_remoteChannel);
    terminate();
}

void WebAutomationSession::init()
{
    m_debuggable->init();
}

bool WebAutomationSession::isPaired() const
{
    return m_debuggable->isPaired();
}

bool WebAutomationSession::isPendingTermination() const
{
    return m_debuggable->isPendingTermination();
}

#endif // ENABLE(REMOTE_INSPECTOR)

void WebAutomationSession::terminate()
{
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    for (auto& identifier : copyToVector(m_pendingKeyboardEventsFlushedCallbacksPerPage.keys())) {
        auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(identifier);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);
    }
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    for (auto& identifier : copyToVector(m_pendingMouseEventsFlushedCallbacksPerPage.keys())) {
        auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(identifier);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);
    }
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    for (auto& identifier : copyToVector(m_pendingWheelEventsFlushedCallbacksPerPage.keys())) {
        auto callback = m_pendingWheelEventsFlushedCallbacksPerPage.take(identifier);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(InternalError);
    }
#endif // ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)

#if ENABLE(REMOTE_INSPECTOR)
    if (Inspector::FrontendChannel* channel = m_remoteChannel) {
        m_remoteChannel = nullptr;
        m_frontendRouter->disconnectFrontend(*channel);
    }

    m_debuggable->setIsPaired(false);
#endif

    if (m_client)
        m_client->didDisconnectFromRemote(*this);
}

RefPtr<WebPageProxy> WebAutomationSession::webPageProxyForHandle(const String& handle)
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
    if (!frameID)
        return emptyString();

    if (RefPtr frame = WebFrameProxy::webFrame(*frameID); frame && frame->isMainFrame())
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

String WebAutomationSession::effectiveHandleForWebFrameProxy(const WebFrameProxy& webFrameProxy)
{
    if (webFrameProxy.isMainFrame()) {
        if (RefPtr page = webFrameProxy.page())
            return handleForWebPageProxy(*page);
    }

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
        .setUrl(page.protectedPageLoadState()->activeURL())
        .setWindowOrigin(WTFMove(originObject))
        .setWindowSize(WTFMove(sizeObject))
        .release();
}

Expected<PageAndFrameHandle, AutomationCommandError> WebAutomationSession::extractBrowsingContextHandles(const String& handle)
{
    if (handle.isEmpty())
        return makeUnexpected(AUTOMATION_COMMAND_ERROR_WITH_NAME_AND_MESSAGE(InvalidParameter, "Browsing context handles cannot be empty"_s));

    if (m_handleWebPageMap.contains(handle))
        return { { handle, emptyString() } };

    bool notFound = false;
    auto frameID = webFrameIDForHandle(handle, notFound);
    if (!notFound && frameID) {
        if (RefPtr frame = WebFrameProxy::webFrame(frameID)) {
            // Main frames are expected to be handled by "page-" handles
            ASSERT(!frame->isMainFrame());
            if (RefPtr page = frame->page()) {
                auto topLevelHandle = handleForWebPageProxy(*page);
                if (!topLevelHandle.isEmpty())
                    return { { topLevelHandle, handle } };
            }
        }
    }

    // WebDriver-BiDi's "get a navigable" algorithm uses just FrameNotFound for both top level and child contexts
    // https://w3c.github.io/webdriver-bidi/#get-a-navigable
    return makeUnexpected(AUTOMATION_COMMAND_ERROR_WITH_NAME(FrameNotFound));
}

// Platform-independent Commands.

void WebAutomationSession::getNextContext(Vector<Ref<WebPageProxy>>&& pages, Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>> contexts, CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>>&& callback)
{
    if (pages.isEmpty()) {
        callback(WTFMove(contexts));
        return;
    }
    auto page = pages.takeLast();
    Ref webPageProxy = page.get();
    webPageProxy->getWindowFrameWithCallback([this, protectedThis = Ref { *this }, callback = WTFMove(callback), pages = WTFMove(pages), contexts = WTFMove(contexts), page = WTFMove(page)](WebCore::FloatRect windowFrame) mutable {
        contexts->addItem(protectedThis->buildBrowsingContextForPage(page.get(), windowFrame));
        getNextContext(WTFMove(pages), WTFMove(contexts), WTFMove(callback));
    });
}

void WebAutomationSession::getBrowsingContexts(CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>>>&& callback)
{
    Vector<Ref<WebPageProxy>> pages;
    for (Ref process : protectedProcessPool()->processes()) {
        for (Ref page : process->pages()) {
            if (!page->isControlledByAutomation())
                continue;
            pages.append(WTFMove(page));
        }
    }

    getNextContext(WTFMove(pages), JSON::ArrayOf<Inspector::Protocol::Automation::BrowsingContext>::create(), WTFMove(callback));
}

void WebAutomationSession::getBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, CommandCallback<Ref<Inspector::Protocol::Automation::BrowsingContext>>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    page->getWindowFrameWithCallback([protectedThis = Ref { *this }, page = Ref { *page }, callback = WTFMove(callback)](WebCore::FloatRect windowFrame) mutable {
        callback(protectedThis->buildBrowsingContextForPage(page.get(), windowFrame));
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

void WebAutomationSession::createBrowsingContext(std::optional<Inspector::Protocol::Automation::BrowsingContextPresentation>&& presentationHint, CommandCallbackOf<String, Inspector::Protocol::Automation::BrowsingContextPresentation>&& callback)
{
    ASSERT(m_client);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!m_client, InternalError, "The remote session could not request a new browsing context."_s);

    uint16_t options = 0;

    if (presentationHint == Inspector::Protocol::Automation::BrowsingContextPresentation::Tab)
        options |= API::AutomationSessionBrowsingContextOptionsPreferNewTab;

    m_client->requestNewPageWithOptions(*this, static_cast<API::AutomationSessionBrowsingContextOptions>(options), [protectedThis = Ref { *this }, callback = WTFMove(callback)](WebPageProxy* page) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!page, InternalError, "The remote session failed to create a new browsing context."_s);

        // WebDriver allows running commands in a browsing context which has not done any loads yet. Force WebProcess to be created so it can receive messages.
        page->launchInitialProcessIfNecessary();
        callback({ { protectedThis->handleForWebPageProxy(*page), toProtocol(protectedThis->m_client->currentPresentationOfPage(protectedThis.get(), *page)) } });
    });
}

void WebAutomationSession::closeBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    page->closePage();

    RunLoop::mainSingleton().dispatch([callback = WTFMove(callback)] {
        callback({ });
    });
}

CommandResult<void> WebAutomationSession::deleteSession()
{
#if ENABLE(REMOTE_INSPECTOR)
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!isPaired(), InternalError);
#endif

    terminate();
    return { };
}

void WebAutomationSession::resolveBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameID && !WebFrameProxy::webFrame(frameID.value()), FrameNotFound);

    callback({ });
}

void WebAutomationSession::switchToBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    m_client->requestSwitchToPage(*this, *page, [this, protectedThis = RefPtr { *this }, frameID, page = Ref { *page }, callback = WTFMove(callback)]() mutable {
        page->setFocus(true);

        if (!frameID) {
            callback({ });
            return;
        }

        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!WebFrameProxy::webFrame(frameID.value()), FrameNotFound);

        if (m_client->isShowingJavaScriptDialogOnPage(*this, page)) {
            callback({ });
            return;
        }

        page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::FocusFrame(page->webPageIDInMainFrameProcess(), frameID.value()), WTF::CompletionHandler<void(Inspector::CommandResult<void>&&)> { [callback = WTFMove(callback)] (auto result) mutable {
            callback(WTFMove(result));
        } });
    });
}

void WebAutomationSession::setWindowFrameOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, RefPtr<JSON::Object>&& origin, RefPtr<JSON::Object>&& size, CommandCallback<void>&& callback)
{
    std::optional<double> x;
    std::optional<double> y;
    if (origin) {
        x = origin->getDouble("x"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!x, MissingParameter, "The 'x' parameter was not found or invalid."_s);
        y = origin->getDouble("y"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!y, MissingParameter, "The 'y' parameter was not found or invalid."_s);
    }

    std::optional<double> width;
    std::optional<double> height;
    if (size) {
        width = size->getDouble("width"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!width, MissingParameter, "The 'width' parameter was not found or invalid."_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(width.value() < 0, InvalidParameter, "The 'width' parameter had an invalid value."_s);
        height = size->getDouble("height"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!height, MissingParameter, "The 'height' parameter was not found or invalid."_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(height.value() < 0, InvalidParameter, "The 'height' parameter had an invalid value."_s);
    }

    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    exitFullscreenWindowForPage(*page, [this, protectedThis = Ref { *this }, callback = WTFMove(callback), page = RefPtr { page }, width, height, x, y]() mutable {
        auto& webPage = *page;
        this->restoreWindowForPage(webPage, [callback = WTFMove(callback), page = RefPtr { page }, width, height, x, y]() mutable {
            auto& webPage = *page;
            webPage.getWindowFrameWithCallback([callback = WTFMove(callback), page = RefPtr { page }, width, height, x, y](WebCore::FloatRect originalFrame) mutable {
                WebCore::FloatRect newFrame = WebCore::FloatRect(WebCore::FloatPoint(x.value_or(originalFrame.location().x()), y.value_or(originalFrame.location().y())), WebCore::FloatSize(width.value_or(originalFrame.size().width()), height.value_or(originalFrame.size().height())));
                if (newFrame != originalFrame)
                    page->setWindowFrame(newFrame);
                
                callback({ });
            });
        });
    });
}

void WebAutomationSession::waitForNavigationToComplete(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& optionalFrameHandle, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    // If page is loading and there's an active JavaScript dialog is probably because the
    // dialog was started in an onload handler, so in case of normal page load strategy the
    // load will not finish until the dialog is dismissed. Instead of waiting for the timeout,
    // we return without waiting since we know it will timeout for sure. We want to check
    // arguments first, though.
    bool shouldTimeoutDueToUnexpectedAlert = pageLoadStrategy == Inspector::Protocol::Automation::PageLoadStrategy::Normal
        && page->protectedPageLoadState()->isLoading() && m_client->isShowingJavaScriptDialogOnPage(*this, *page);

    if (!optionalFrameHandle.isEmpty()) {
        bool frameNotFound = false;
        auto frameID = webFrameIDForHandle(optionalFrameHandle, frameNotFound);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

        RefPtr frame = WebFrameProxy::webFrame(frameID.value());
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!frame, FrameNotFound);

        if (!shouldTimeoutDueToUnexpectedAlert) {
            waitForNavigationToCompleteOnFrame(*frame, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
            return;
        }
    } else {
        if (!shouldTimeoutDueToUnexpectedAlert) {
            waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
            return;
        }
    }

    // §9 Navigation.
    // 7. If the previous step completed by the session page load timeout being reached and the browser does not
    // have an active user prompt, return error with error code timeout.
    // 8. Return success with data null.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-wait-for-navigation-to-complete
    callback({ });
}

void WebAutomationSession::waitForNavigationToCompleteOnPage(WebPageProxy& page, Inspector::Protocol::Automation::PageLoadStrategy loadStrategy, Seconds timeout, Inspector::CommandCallback<void>&& callback)
{
    ASSERT(!m_loadTimer.isActive());
    Ref pageLoadState = page.pageLoadState();

    if (loadStrategy == Inspector::Protocol::Automation::PageLoadStrategy::None || (!pageLoadState->isLoading() && !pageLoadState->hasUncommittedLoad())) {
        callback({ });
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

void WebAutomationSession::waitForNavigationToCompleteOnFrame(WebFrameProxy& frame, Inspector::Protocol::Automation::PageLoadStrategy loadStrategy, Seconds timeout, Inspector::CommandCallback<void>&& callback)
{
    ASSERT(!m_loadTimer.isActive());
    if (loadStrategy == Inspector::Protocol::Automation::PageLoadStrategy::None || frame.frameLoadState().state() == FrameLoadState::State::Finished) {
        callback({ });
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

void WebAutomationSession::respondToPendingPageNavigationCallbacksWithTimeout(HashMap<WebPageProxyIdentifier, Inspector::CommandCallback<void>>& map)
{
    for (auto id : copyToVector(map.keys())) {
        auto page = WebProcessProxy::webPage(id);
        auto callback = map.take(id);
        if (page && m_client->isShowingJavaScriptDialogOnPage(*this, *page))
            callback({ });
        else
            ASYNC_FAIL_WITH_PREDEFINED_ERROR(Timeout);
    }
}

static WebPageProxy* findPageForFrameID(const WebProcessPool& processPool, FrameIdentifier frameID)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        return frame->page();
    return nullptr;
}

void WebAutomationSession::respondToPendingFrameNavigationCallbacksWithTimeout(HashMap<FrameIdentifier, Inspector::CommandCallback<void>>& map)
{
    for (auto id : copyToVector(map.keys())) {
        RefPtr page = findPageForFrameID(*protectedProcessPool(), id);
        auto callback = map.take(id);
        if (page && m_client->isShowingJavaScriptDialogOnPage(*this, *page))
            callback({ });
        else
            ASYNC_FAIL_WITH_PREDEFINED_ERROR(Timeout);
    }
}

void WebAutomationSession::loadTimerFired()
{
    respondToPendingFrameNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame);
    respondToPendingFrameNavigationCallbacksWithTimeout(m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame);
    respondToPendingPageNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerPage);
    respondToPendingPageNavigationCallbacksWithTimeout(m_pendingEagerNavigationInBrowsingContextCallbacksPerPage);
}

void WebAutomationSession::maximizeWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    exitFullscreenWindowForPage(*page, [this, protectedThis = Ref { *this }, callback = WTFMove(callback), page = RefPtr { page }]() mutable {
        auto& webPage = *page;
        restoreWindowForPage(webPage, [this, protectedThis, callback = WTFMove(callback), page = RefPtr { page }]() mutable {
            maximizeWindowForPage(*page, [callback = WTFMove(callback)]() {
                callback({ });
            });
        });
    });
}

void WebAutomationSession::hideWindowOfBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);
    
    exitFullscreenWindowForPage(*page, [protectedThis = Ref { *this }, callback = WTFMove(callback), page = RefPtr { page }]() mutable {
        protectedThis->hideWindowForPage(*page, [callback = WTFMove(callback)]() mutable {
            callback({ });
        });
    });
}

void WebAutomationSession::exitFullscreenWindowForPage(WebPageProxy& page, WTF::CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(FULLSCREEN_API)
    ASSERT(!m_windowStateTransitionCallback);
    RefPtr fullScreenManager = page.fullScreenManager();
    if (!fullScreenManager || !fullScreenManager->isFullScreen()) {
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
    
    fullScreenManager->requestExitFullScreen();
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

#if ENABLE(WEBDRIVER_BIDI)
static Inspector::Protocol::BidiBrowsingContext::UserPromptType toProtocolUserPromptType(API::AutomationSessionClient::JavaScriptDialogType dialogType)
{
    switch (dialogType) {
    case API::AutomationSessionClient::JavaScriptDialogType::Alert:
        return Inspector::Protocol::BidiBrowsingContext::UserPromptType::Alert;
    case API::AutomationSessionClient::JavaScriptDialogType::Confirm:
        return Inspector::Protocol::BidiBrowsingContext::UserPromptType::Confirm;
    case API::AutomationSessionClient::JavaScriptDialogType::Prompt:
        return Inspector::Protocol::BidiBrowsingContext::UserPromptType::Prompt;
    case API::AutomationSessionClient::JavaScriptDialogType::BeforeUnloadConfirm:
        return Inspector::Protocol::BidiBrowsingContext::UserPromptType::Beforeunload;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return Inspector::Protocol::BidiBrowsingContext::UserPromptType::Alert;
}
#endif

void WebAutomationSession::willShowJavaScriptDialog(WebPageProxy& page, const String& message, std::optional<String>&& defaultText)
{
    // Wait until the next run loop iteration to give time for the client to show the dialog,
    // then check if the dialog is still present. If the page is loading, the dialog will block
    // the load in case of normal strategy, so we want to dispatch all pending navigation callbacks.
    // If the dialog was shown during a script execution, we want to finish the evaluateJavaScriptFunction
    // operation with an unexpected alert open error.
    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, page = Ref { page }, message, defaultText] {
        if (!page->hasRunningProcess() || !m_client || !m_client->isShowingJavaScriptDialogOnPage(*this, page))
            return;

#if ENABLE(WEBDRIVER_BIDI)
        std::optional<API::AutomationSessionClient::JavaScriptDialogType> apiDialogType = m_client->typeOfCurrentJavaScriptDialogOnPage(*this, page);
        auto userPromptType = toProtocolUserPromptType(apiDialogType.value_or(API::AutomationSessionClient::JavaScriptDialogType::Prompt));

        // FIXME: propagate the 'userPromptHandler' from session capabilities.
        auto userPromptHandlerType = Inspector::Protocol::BidiSession::UserPromptHandlerType::Accept;
        m_bidiProcessor->browsingContextDomainNotifier().userPromptOpened(handleForWebPageProxy(page), userPromptType, userPromptHandlerType, message, m_client->defaultTextOfCurrentJavaScriptDialogOnPage(*this, page).value_or(defaultText.value_or(emptyString())));
#endif

        if (page->protectedPageLoadState()->isLoading()) {
            m_loadTimer.stop();
            respondToPendingFrameNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame);
            respondToPendingPageNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerPage);
        }

        if (!m_evaluateJavaScriptFunctionCallbacks.isEmpty()) {
            for (auto key : copyToVector(m_evaluateJavaScriptFunctionCallbacks.keys())) {
                auto callback = m_evaluateJavaScriptFunctionCallbacks.take(key);
                callback(String("null"_s));
            }
        }

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        if (!m_pendingMouseEventsFlushedCallbacksPerPage.isEmpty()) {
            for (auto key : copyToVector(m_pendingMouseEventsFlushedCallbacksPerPage.keys())) {
                auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(key);
                callback({ });
            }
        }
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        if (!m_pendingKeyboardEventsFlushedCallbacksPerPage.isEmpty()) {
            for (auto key : copyToVector(m_pendingKeyboardEventsFlushedCallbacksPerPage.keys())) {
                auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(key);
                callback({ });
            }
        }
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    });

#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
        if (!m_pendingWheelEventsFlushedCallbacksPerPage.isEmpty()) {
            for (auto key : copyToVector(m_pendingWheelEventsFlushedCallbacksPerPage.keys())) {
                auto callback = m_pendingWheelEventsFlushedCallbacksPerPage.take(key);
                callback({ });
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

void WebAutomationSession::navigateBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, const String& url, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->loadRequest(URL { url });
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::goBackInBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->goBack();
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::goForwardInBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    auto pageLoadStrategy = optionalPageLoadStrategy.value_or(defaultPageLoadStrategy);
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->goForward();
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy, pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::traverseHistoryInBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, int delta, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    RefPtr mainFrame = page->mainFrame();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!mainFrame, InvalidParameter);

    if (!delta) {
        callback({ });
        return;
    }

    Ref backForwardList = page->backForwardList();
    unsigned backCount = backForwardList->backListCount();
    unsigned forwardCount = backForwardList->forwardListCount();
    int currentIndex = static_cast<int>(backCount);
    int targetIndex = currentIndex + delta;
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(targetIndex < 0, InvalidParameter);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(targetIndex >= static_cast<int>(backCount + forwardCount + 1), InvalidParameter);

    RefPtr targetItem = backForwardList->itemAtIndex(targetIndex);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!targetItem, InternalError);

    page->goToBackForwardItem(*targetItem);
    waitForNavigationToCompleteOnPage(*page, defaultPageLoadStrategy, defaultPageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::reloadBrowsingContext(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, std::optional<Inspector::Protocol::Automation::PageLoadStrategy>&& optionalPageLoadStrategy, std::optional<double>&& optionalPageLoadTimeout, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

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
            callback({ });
        }
        m_domainNotifier->browsingContextCleared(handleForWebPageProxy(*frame.protectedPage()));
    } else {
        if (auto callback = m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame.take(frame.frameID())) {
            m_loadTimer.stop();
            callback({ });
        }
    }
}

#if ENABLE(WEBDRIVER_BIDI)
static String navigationIDToProtocolString(std::optional<WebCore::NavigationIdentifier> navigationID)
{
    if (!navigationID)
        return nullString();

    uint64_t id = navigationID->toUInt64();
    return WTF::UUID(id, id).toString();
}
#endif

void WebAutomationSession::documentLoadedForFrame(const WebFrameProxy& frame, std::optional<WebCore::NavigationIdentifier> navigationID, WallTime timestamp)
{
#if ENABLE(WEBDRIVER_BIDI)
    m_bidiProcessor->browsingContextDomainNotifier().domContentLoaded(effectiveHandleForWebFrameProxy(frame), navigationIDToProtocolString(navigationID), std::trunc(timestamp.secondsSinceEpoch().milliseconds()), frame.url().string());
#endif

    if (frame.isMainFrame()) {
        if (auto callback = m_pendingEagerNavigationInBrowsingContextCallbacksPerPage.take(frame.page()->identifier())) {
            m_loadTimer.stop();
            callback({ });
        }

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        resetMouseState();
#endif
    } else {
        if (auto callback = m_pendingEagerNavigationInBrowsingContextCallbacksPerFrame.take(frame.frameID())) {
            m_loadTimer.stop();
            callback({ });
        }
    }
}

void WebAutomationSession::loadCompletedForFrame(const WebFrameProxy& frame, std::optional<WebCore::NavigationIdentifier> navigationID, WallTime timestamp)
{
#if ENABLE(WEBDRIVER_BIDI)
    m_bidiProcessor->browsingContextDomainNotifier().load(effectiveHandleForWebFrameProxy(frame), navigationIDToProtocolString(navigationID), std::trunc(timestamp.secondsSinceEpoch().milliseconds()), frame.url().string());
#endif
}

void WebAutomationSession::inspectorFrontendLoaded(const WebPageProxy& page)
{
    if (auto callback = m_pendingInspectorCallbacksPerPage.take(page.identifier()))
        callback({ });
}

void WebAutomationSession::mouseEventsFlushedForPage(const WebPageProxy& page)
{
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    if (auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback({ });
#else
    UNUSED_PARAM(page);
#endif
}

void WebAutomationSession::keyboardEventsFlushedForPage(const WebPageProxy& page)
{
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    if (auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback({ });
#else
    UNUSED_PARAM(page);
#endif
}

void WebAutomationSession::wheelEventsFlushedForPage(const WebPageProxy& page)
{
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    if (auto callback = m_pendingWheelEventsFlushedCallbacksPerPage.take(page.identifier()))
        callback({ });
#else
    UNUSED_PARAM(page);
#endif
}

#if ENABLE(WEBDRIVER_BIDI)
void WebAutomationSession::didCreatePage(WebPageProxy& page)
{
    m_bidiProcessor->browserAgent().didCreatePage(page);
    emitContextCreatedEvent(page);
}

void WebAutomationSession::navigationStartedForFrame(const WebFrameProxy& frame, std::optional<WebCore::NavigationIdentifier> navigationID)
{
    m_bidiProcessor->browsingContextDomainNotifier().navigationStarted(effectiveHandleForWebFrameProxy(frame), navigationIDToProtocolString(navigationID), WallTime::now().secondsSinceEpoch().milliseconds(), frame.url().string());
}

void WebAutomationSession::navigationCommittedForFrame(const WebFrameProxy& frame, std::optional<WebCore::NavigationIdentifier> navigationID)
{
    m_bidiProcessor->browsingContextDomainNotifier().navigationCommitted(effectiveHandleForWebFrameProxy(frame), navigationIDToProtocolString(navigationID), WallTime::now().secondsSinceEpoch().milliseconds(), frame.url().string());
}

void WebAutomationSession::navigationFailedForFrame(const WebFrameProxy& frame, std::optional<WebCore::NavigationIdentifier> navigationID)
{
    m_bidiProcessor->browsingContextDomainNotifier().navigationFailed(effectiveHandleForWebFrameProxy(frame), navigationIDToProtocolString(navigationID), WallTime::now().secondsSinceEpoch().milliseconds(), frame.url().string());
}

void WebAutomationSession::navigationAbortedForFrame(const WebFrameProxy& frame, std::optional<WebCore::NavigationIdentifier> navigationID)
{
    m_bidiProcessor->browsingContextDomainNotifier().navigationAborted(effectiveHandleForWebFrameProxy(frame), navigationIDToProtocolString(navigationID), WallTime::now().secondsSinceEpoch().milliseconds(), frame.url().string());
}

void WebAutomationSession::fragmentNavigatedForFrame(const WebFrameProxy& frame, std::optional<WebCore::NavigationIdentifier> navigationID)
{
    m_bidiProcessor->browsingContextDomainNotifier().fragmentNavigated(effectiveHandleForWebFrameProxy(frame), navigationIDToProtocolString(navigationID), WallTime::now().secondsSinceEpoch().milliseconds(), frame.url().string());
}

void WebAutomationSession::emitContextCreatedEvent(const WebPageProxy& page)
{
    if (RefPtr mutablePage = WebProcessProxy::webPage(page.identifier())) {
        mutablePage->getAllFrameTrees([this, protectedThis = Ref { *this }, pageID = page.identifier()](Vector<FrameTreeNodeData>&& trees) {
            RefPtr page = WebProcessProxy::webPage(pageID);
            if (!page)
                return;

            for (auto& tree : trees)
                recursivelyEmitContextCreatedEvent(tree, std::nullopt);
        });
    }
}

static std::pair<String, String> getClientWindowAndUserContext(const WebPageProxy& page)
{
    String clientWindow = makeString(page.identifier().toUInt64());
    String userContext = "default"_s;
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=281941 - Add support for reporting user context
    return { clientWindow, userContext };
}

WebPageProxy* WebAutomationSession::getOpenerPage(const WebPageProxy& page)
{
    if (auto openerFrameID = page.openerFrameIdentifier()) {
        if (RefPtr openerFrame = WebFrameProxy::webFrame(*openerFrameID))
            return openerFrame->page();
    }
    return nullptr;
}

void WebAutomationSession::didCreateFrame(const WebFrameProxy& frame)
{
    effectiveHandleForWebFrameProxy(frame);
#if ENABLE(WEBDRIVER_BIDI)
    contextCreatedForFrame(frame);
#endif
}

void WebAutomationSession::willDestroyFrame(const WebFrameProxy& frame)
{
#if ENABLE(WEBDRIVER_BIDI)
    contextDestroyedForFrame(frame);
#endif
}

void WebAutomationSession::contextCreatedForFrame(const WebFrameProxy& frame)
{
    auto contextHandle = effectiveHandleForWebFrameProxy(frame);
    auto url = frame.url().string();
    String parentHandle = "null"_s;
    if (RefPtr parentFrame = frame.parentFrame())
        parentHandle = effectiveHandleForWebFrameProxy(*parentFrame);

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=281941 - Add support for reporting clientWindow for frames
    String clientWindow = "unknown-window"_s;
    String userContext = "default"_s;

    if (RefPtr page = frame.page()) {
        auto [windowId, contextId] = getClientWindowAndUserContext(*page);
        clientWindow = windowId;
        userContext = contextId;
    }

    m_bidiProcessor->browsingContextDomainNotifier().contextCreated(contextHandle, url, "null"_s, parentHandle, JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>::create(), clientWindow, userContext);
}

void WebAutomationSession::recursivelyEmitContextCreatedEvent(const FrameTreeNodeData& tree, std::optional<String>&& parentContext)
{
    RefPtr frame = WebFrameProxy::webFrame(tree.info.frameID);
    if (!frame)
        return;

    RefPtr page = frame->page();
    if (!page)
        return;

    String contextHandle;
    String originalOpenerHandle = "null"_s;

    if (tree.info.isMainFrame) {
        contextHandle = handleForWebPageProxy(*page);
        if (RefPtr openerPage = this->getOpenerPage(*page))
            originalOpenerHandle = handleForWebPageProxy(*openerPage);
    } else
        contextHandle = handleForWebFrameID(tree.info.frameID);

    String url = tree.info.request.url().string();
    auto [clientWindow, userContext] = getClientWindowAndUserContext(*page);
    auto children = JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>::create();
    // FIXME: Use JSON null instead of string "null" when Inspector::Protocol supports RefPtr<String> or std::optional<String>
    String parentContextHandle = parentContext.value_or("null"_s);

    m_bidiProcessor->browsingContextDomainNotifier().contextCreated(contextHandle, url, originalOpenerHandle, parentContextHandle, WTFMove(children), clientWindow, userContext);

    for (const auto& child : tree.children)
        recursivelyEmitContextCreatedEvent(child, contextHandle);
}

void WebAutomationSession::contextDestroyedForPage(const WebPageProxy& page)
{
    auto contextHandle = handleForWebPageProxy(page);
    auto url = page.currentURL();

    String parentContext = "null"_s;
    if (RefPtr mainFrame = page.mainFrame()) {
        if (RefPtr parentFrame = mainFrame->parentFrame())
            parentContext = effectiveHandleForWebFrameProxy(*parentFrame);
    }

    String originalOpenerHandle = "null"_s;
    if (RefPtr openerPage = this->getOpenerPage(page))
        originalOpenerHandle = handleForWebPageProxy(*openerPage);

    auto [clientWindow, userContext] = getClientWindowAndUserContext(page);

    m_bidiProcessor->browsingContextDomainNotifier().contextDestroyed(contextHandle, url, originalOpenerHandle, parentContext, JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>::create(), clientWindow, userContext);

    m_handleWebPageMap.remove(contextHandle);
    m_webPageHandleMap.remove(page.identifier());
}

void WebAutomationSession::contextDestroyedForFrame(const WebFrameProxy& frame)
{
    auto contextHandle = effectiveHandleForWebFrameProxy(frame);
    auto url = frame.url().string();
    String parentHandle = "null"_s;
    if (RefPtr parentFrame = frame.parentFrame())
        parentHandle = effectiveHandleForWebFrameProxy(*parentFrame);

    String clientWindow = "unknown-window"_s;
    String userContext = "default"_s;

    if (RefPtr page = frame.page()) {
        auto [windowId, contextId] = getClientWindowAndUserContext(*page);
        clientWindow = windowId;
        userContext = contextId;
    }

    m_bidiProcessor->browsingContextDomainNotifier().contextDestroyed(contextHandle, url, "null"_s, parentHandle, JSON::ArrayOf<Inspector::Protocol::BidiBrowsingContext::Info>::create(), clientWindow, userContext);

    // Note: Frame handle cleanup is done by didDestroyFrame(), so we don't duplicate that here
}

#endif

void WebAutomationSession::willClosePage(const WebPageProxy& page)
{
    String handle = handleForWebPageProxy(page);
    m_domainNotifier->browsingContextCleared(handle);

#if ENABLE(WEBDRIVER_BIDI)
    contextDestroyedForPage(page);
#endif

    // Cancel pending interactions on this page. By providing an error, this will cause subsequent
    // actions to be aborted and the SimulatedInputDispatcher::run() call will unwind and fail.
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    if (auto callback = m_pendingMouseEventsFlushedCallbacksPerPage.take(page.identifier()))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);
#endif
#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    if (auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.identifier()))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);
#endif
#if ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
    if (auto callback = m_pendingWheelEventsFlushedCallbacksPerPage.take(page.identifier()))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);
#endif

#if ENABLE(WEBDRIVER_ACTIONS_API)
    // Then tell the input dispatcher to cancel so timers are stopped, and let it go out of scope.
    if (auto inputDispatcher = m_inputDispatchersByPage.take(page.identifier()))
        inputDispatcher->cancel();
#endif

#if ENABLE(WEBDRIVER_BIDI)
    m_bidiProcessor->browserAgent().willClosePage(page);
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

    auto wildcardedMIMEType = makeString(components[0], "/*"_s);
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
    for (RefPtr type : acceptMIMETypes->elementsOfType<API::String>())
        allowedMIMETypes.add(type->string());

    HashSet<String> allowedFileExtensions;
    auto acceptFileExtensions = parameters.acceptFileExtensions();
    for (RefPtr type : acceptFileExtensions->elementsOfType<API::String>()) {
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

void WebAutomationSession::evaluateJavaScriptFunction(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const String& function, Ref<JSON::Array>&& arguments, std::optional<bool>&& expectsImplicitCallbackArgument, std::optional<bool>&& forceUserGesture, std::optional<double>&& callbackTimeout, Inspector::CommandCallback<String>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    auto argumentsVector = WTF::map(arguments.get(), [](auto& argument) {
        return argument->asString();
    });

    uint64_t callbackID = m_nextEvaluateJavaScriptCallbackID++;
    m_evaluateJavaScriptFunctionCallbacks.set(callbackID, WTFMove(callback));

    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::EvaluateJavaScriptFunction(page->webPageIDInMainFrameProcess(), frameID, function, argumentsVector, expectsImplicitCallbackArgument.value_or(false), forceUserGesture.value_or(false), WTFMove(callbackTimeout)), CompletionHandler<void(String&&, String&&)> { [protectedThis = Ref { *this }, callbackID] (String&& result, String&& errorType) {
        auto callback = protectedThis->m_evaluateJavaScriptFunctionCallbacks.take(callbackID);
        if (!callback)
            return;

        if (!errorType.isEmpty()) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_MESSAGE_AND_DETAILS(errorType, result)));
            return;
        }

        callback(result);
    } });
}

void WebAutomationSession::resolveChildFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, std::optional<int>&& optionalOrdinal, const String& optionalName, const Inspector::Protocol::Automation::NodeHandle& optionalNodeHandle, CommandCallback<String>&& callback)
{
    bool hasNoChildFrameSpecifier = !optionalOrdinal && !optionalName && !optionalNodeHandle;
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(hasNoChildFrameSpecifier, MissingParameter, "Command must specify a child frame by ordinal, name, or element handle."_s);

    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>&&, std::optional<FrameIdentifier>&&)> completionHandler = [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String>&& optionalError, std::optional<FrameIdentifier>&& frameID) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        callback(handleForWebFrameID(frameID));
    };

    if (!!optionalNodeHandle) {
        page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::ResolveChildFrameWithNodeHandle(page->webPageIDInMainFrameProcess(), frameID, optionalNodeHandle), WTFMove(completionHandler));
        return;
    }

    if (!!optionalName) {
        page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::ResolveChildFrameWithName(page->webPageIDInMainFrameProcess(), frameID, optionalName), WTFMove(completionHandler));
        return;
    }

    if (optionalOrdinal) {
        page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::ResolveChildFrameWithOrdinal(page->webPageIDInMainFrameProcess(), frameID, *optionalOrdinal), WTFMove(completionHandler));
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebAutomationSession::resolveParentFrameHandle(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, CommandCallback<String>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>&&, std::optional<FrameIdentifier>&&)> completionHandler = [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String>&& optionalError, std::optional<FrameIdentifier>&& frameID) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        callback(handleForWebFrameID(frameID));
    };

    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::ResolveParentFrame(page->webPageIDInMainFrameProcess(), frameID), WTFMove(completionHandler));
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

void WebAutomationSession::computeElementLayout(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, std::optional<bool>&& optionalScrollIntoViewIfNeeded, Inspector::Protocol::Automation::CoordinateSystem coordinateSystemValue, CommandCallbackOf<Ref<Inspector::Protocol::Automation::Rect>, RefPtr<Inspector::Protocol::Automation::Point>, bool>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    std::optional<CoordinateSystem> coordinateSystem = protocolStringToCoordinateSystem(coordinateSystemValue);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!coordinateSystem, InvalidParameter, "The parameter 'coordinateSystem' is invalid."_s);

    WTF::CompletionHandler<void(std::optional<String>&&, WebCore::FloatRect&&, std::optional<WebCore::IntPoint>&&, bool)> completionHandler = [callback = WTFMove(callback)](std::optional<String> optionalError, WebCore::FloatRect rect, std::optional<WebCore::IntPoint> inViewCenterPoint, bool isObscured) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

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
            callback({ { WTFMove(rectObject), nullptr, isObscured } });
            return;
        }

        auto inViewCenterPointObject = Inspector::Protocol::Automation::Point::create()
            .setX(inViewCenterPoint.value().x())
            .setY(inViewCenterPoint.value().y())
            .release();

        callback({ { WTFMove(rectObject), WTFMove(inViewCenterPointObject), isObscured } });
    };

    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::ComputeElementLayout(page->webPageIDInMainFrameProcess(), frameID, nodeHandle, scrollIntoViewIfNeeded, coordinateSystem.value()), WTFMove(completionHandler));
}

void WebAutomationSession::getComputedRole(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, CommandCallback<String>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>&&, std::optional<String>&&)> completionHandler = [callback = WTFMove(callback)](std::optional<String>&& optionalError, std::optional<String>&& role) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        callback(*role);
    };

    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::GetComputedRole(page->webPageIDInMainFrameProcess(), frameID, nodeHandle), WTFMove(completionHandler));
}

void WebAutomationSession::getComputedLabel(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, CommandCallback<String>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>&&, std::optional<String>&&)> completionHandler = [callback = WTFMove(callback)](std::optional<String>&& optionalError, std::optional<String>&& label) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        callback(*label);
    };

    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::GetComputedLabel(page->webPageIDInMainFrameProcess(), frameID, nodeHandle), WTFMove(completionHandler));
}

void WebAutomationSession::selectOptionElement(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    WTF::CompletionHandler<void(std::optional<String>&&)> completionHandler = [callback = WTFMove(callback)](std::optional<String>&& optionalError) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        callback({ });
    };

    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::SelectOptionElement(page->webPageIDInMainFrameProcess(), frameID, nodeHandle), WTFMove(completionHandler));
}

CommandResult<bool> WebAutomationSession::isShowingJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    ASSERT(m_client);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!m_client, InternalError);

    auto page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    return m_client->isShowingJavaScriptDialogOnPage(*this, *page);
}

CommandResult<void> WebAutomationSession::dismissCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    ASSERT(m_client);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!m_client, InternalError);

    auto page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool isShowingJavaScriptDialog = m_client->isShowingJavaScriptDialogOnPage(*this, *page);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!isShowingJavaScriptDialog, NoJavaScriptDialog);

#if ENABLE(WEBDRIVER_BIDI)
    auto apiDialogType = m_client->typeOfCurrentJavaScriptDialogOnPage(*this, *page);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!apiDialogType, InternalError);

    m_bidiProcessor->browsingContextDomainNotifier().userPromptClosed(handleForWebPageProxy(*page), toProtocolUserPromptType(apiDialogType.value()), false, m_client->userInputOfCurrentJavaScriptDialogOnPage(*this, *page).value_or(emptyString()));
#endif
    m_client->dismissCurrentJavaScriptDialogOnPage(*this, *page);

    return { };
}

CommandResult<void> WebAutomationSession::acceptCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    ASSERT(m_client);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!m_client, InternalError);

    auto page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool isShowingJavaScriptDialog = m_client->isShowingJavaScriptDialogOnPage(*this, *page);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!isShowingJavaScriptDialog, NoJavaScriptDialog);

#if ENABLE(WEBDRIVER_BIDI)
    auto apiDialogType = m_client->typeOfCurrentJavaScriptDialogOnPage(*this, *page);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!apiDialogType, InternalError);

    m_bidiProcessor->browsingContextDomainNotifier().userPromptClosed(handleForWebPageProxy(*page), toProtocolUserPromptType(apiDialogType.value()), true, m_client->userInputOfCurrentJavaScriptDialogOnPage(*this, *page).value_or(emptyString()));
#endif

    m_client->acceptCurrentJavaScriptDialogOnPage(*this, *page);

    return { };
}

CommandResult<String> WebAutomationSession::messageOfCurrentJavaScriptDialog(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    ASSERT(m_client);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!m_client, InternalError);

    auto page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool isShowingJavaScriptDialog = m_client->isShowingJavaScriptDialogOnPage(*this, *page);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!isShowingJavaScriptDialog, NoJavaScriptDialog);

    return m_client->messageOfCurrentJavaScriptDialogOnPage(*this, *page).value_or(emptyString());
}

CommandResult<void> WebAutomationSession::setUserInputForCurrentJavaScriptPrompt(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const String& promptValue)
{
    ASSERT(m_client);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!m_client, InternalError);

    auto page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool isShowingJavaScriptDialog = m_client->isShowingJavaScriptDialogOnPage(*this, *page);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!isShowingJavaScriptDialog, NoJavaScriptDialog);

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

CommandResult<void> WebAutomationSession::setFilesToSelectForFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, Ref<JSON::Array>&& filenames, RefPtr<JSON::Array>&& fileContents)
{
    Vector<String> newFileList;
    newFileList.reserveInitialCapacity(filenames->length());

    bool mismatchedFileLengthsDetected = fileContents && fileContents->length() != filenames->length();
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(mismatchedFileLengthsDetected, InternalError, "The parameters 'filenames' and 'fileContents' must have equal length."_s);

    for (size_t i = 0; i < filenames->length(); ++i) {
        auto filename = filenames->get(i)->asString();
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!filename, InternalError, "The parameter 'filenames' contains a non-string value."_s);

        if (!fileContents) {
            newFileList.append(filename);
            continue;
        }

        auto fileData = fileContents->get(i)->asString();
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!fileData, InternalError, "The parameter 'fileContents' contains a non-string value."_s);

        std::optional<String> localFilePath = platformGenerateLocalFilePathForRemoteFile(filename, fileData);
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!localFilePath, InternalError, "The remote file could not be saved to a local temporary directory."_s);

        newFileList.append(localFilePath.value());
    }

    m_filesToSelectForFileUpload.swap(newFileList);

    return { };
}

void WebAutomationSession::setFilesForInputFileUpload(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, Ref<JSON::Array>&& filenames, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);
    Vector<String> newFileList;
    newFileList.reserveInitialCapacity(filenames->length());
    for (size_t i = 0; i < filenames->length(); ++i) {
        auto filename = filenames->get(i)->asString();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!filename, InternalError, "The parameter 'filenames' contains a non-string value."_s);

        newFileList.append(filename);
    }

    CompletionHandler<void(std::optional<String>&&)> completionHandler = [callback = WTFMove(callback)](std::optional<String>&& optionalError) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        callback({ });
    };

    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::SetFilesForInputFileUpload(page->webPageIDInMainFrameProcess(), frameID, nodeHandle, WTFMove(newFileList)), WTFMove(completionHandler));
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

void WebAutomationSession::getAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::Cookie>>>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    WTF::CompletionHandler<void(std::optional<String>, Vector<WebCore::Cookie>)> completionHandler = [callback = WTFMove(callback)](std::optional<String> optionalError, Vector<WebCore::Cookie> cookies) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        callback(buildArrayForCookies(cookies));
    };

    page->protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebAutomationSessionProxy::GetCookiesForFrame(page->webPageIDInMainFrameProcess(), std::nullopt), WTFMove(completionHandler));
}

void WebAutomationSession::deleteSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const String& cookieName, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    WTF::CompletionHandler<void(std::optional<String>)> completionHandler = [callback = WTFMove(callback)](std::optional<String> optionalError) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        callback({ });
    };

    page->protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebAutomationSessionProxy::DeleteCookie(page->webPageIDInMainFrameProcess(), std::nullopt, cookieName), WTFMove(completionHandler));
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
    
void WebAutomationSession::addSingleCookie(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, Ref<JSON::Object>&& cookieObject, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    URL activeURL { page->protectedPageLoadState()->activeURL() };
    ASSERT(activeURL.isValid());

    WebCore::Cookie cookie;

    cookie.name = cookieObject->getString("name"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!cookie.name, MissingParameter, "The parameter 'name' was not found."_s);
    cookie.value = cookieObject->getString("value"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!cookie.value, MissingParameter, "The parameter 'value' was not found."_s);

    auto domain = cookieObject->getString("domain"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!domain, MissingParameter, "The parameter 'domain' was not found."_s);

    // Inherit the domain/host from the main frame's URL if it is not explicitly set.
    if (domain.isEmpty())
        cookie.domain = activeURL.host().toString();
    else
        cookie.domain = domainByAddingDotPrefixIfNeeded(domain);

    cookie.path = cookieObject->getString("path"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!cookie.path, MissingParameter, "The parameter 'path' was not found."_s);

    auto expires = cookieObject->getDouble("expires"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!expires, MissingParameter, "The parameter 'expires' was not found."_s);
    cookie.expires = *expires * 1000.0;

    auto secure = cookieObject->getBoolean("secure"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!secure, MissingParameter, "The parameter 'secure' was not found."_s);
    cookie.secure = *secure;

    auto session = cookieObject->getBoolean("session"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!session, MissingParameter, "The parameter 'session' was not found."_s);
    cookie.session = *session;

    auto httpOnly = cookieObject->getBoolean("httpOnly"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!httpOnly, MissingParameter, "The parameter 'httpOnly' was not found."_s);

    cookie.httpOnly = *httpOnly;

    auto sameSite = cookieObject->getString("sameSite"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!sameSite, MissingParameter, "The parameter 'sameSite' was not found."_s);

    auto parsedSameSite = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::CookieSameSitePolicy>(sameSite);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!parsedSameSite, InvalidParameter, "The parameter 'sameSite' has an unknown value."_s);
    cookie.sameSite = toWebCoreSameSitePolicy(*parsedSameSite);

    Ref cookieStore = page->protectedWebsiteDataStore()->cookieStore();
    cookieStore->setCookies({ cookie }, [callback = WTFMove(callback)]() {
        callback({ });
    });
}

CommandResult<void> WebAutomationSession::deleteAllCookies(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle)
{
    RefPtr page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    URL activeURL { page->protectedPageLoadState()->activeURL() };
    ASSERT(activeURL.isValid());

    String host = activeURL.host().toString();
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(host.isNull(), WindowNotFound);

    Ref cookieStore = page->protectedWebsiteDataStore()->cookieStore();
    cookieStore->deleteCookiesForHostnames({ host, domainByAddingDotPrefixIfNeeded(host) }, [] { });

    return { };
}

CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>>> WebAutomationSession::getSessionPermissions()
{
    auto permissionsObjectArray = JSON::ArrayOf<Inspector::Protocol::Automation::SessionPermissionData>::create();
    auto getUserMediaPermissionObject = Inspector::Protocol::Automation::SessionPermissionData::create()
        .setPermission(Inspector::Protocol::Automation::SessionPermission::GetUserMedia)
        .setValue(m_permissionForGetUserMedia)
        .release();

    permissionsObjectArray->addItem(WTFMove(getUserMediaPermissionObject));
    return permissionsObjectArray;
}

CommandResult<void> WebAutomationSession::setSessionPermissions(Ref<JSON::Array>&& permissions)
{
    for (auto& value : permissions.get()) {
        auto permission = value->asObject();
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!permission, InvalidParameter, "The parameter 'permissions' is invalid."_s);

        auto permissionName = permission->getString("permission"_s);
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!permissionName, InvalidParameter, "The parameter 'permission' is missing or invalid."_s);

        auto parsedPermissionName = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::SessionPermission>(permissionName);
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!parsedPermissionName, InvalidParameter, "The parameter 'permission' has an unknown value."_s);

        auto permissionValue = permission->getBoolean("value"_s);
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!permissionValue, InvalidParameter, "The parameter 'value' is missing or invalid."_s);

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

CommandResult<String /* authenticatorId */> WebAutomationSession::addVirtualAuthenticator(const String& browsingContextHandle, Ref<JSON::Object>&& authenticator)
{
#if ENABLE(WEB_AUTHN)
    auto protocol = authenticator->getString("protocol"_s);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!protocol, InvalidParameter, "The parameter 'protocol' is missing or invalid."_s);

    auto transport = authenticator->getString("transport"_s);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!transport, InvalidParameter, "The parameter 'transport' is missing or invalid."_s);
    auto parsedTransport = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::AuthenticatorTransport>(transport);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!parsedTransport, InvalidParameter, "The parameter 'transport' has an unknown value."_s);

    auto hasResidentKey = authenticator->getBoolean("hasResidentKey"_s);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!hasResidentKey, InvalidParameter, "The parameter 'hasResidentKey' is missing or invalid."_s);

    auto hasUserVerification = authenticator->getBoolean("hasUserVerification"_s);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!hasUserVerification, InvalidParameter, "The parameter 'hasUserVerification' is missing or invalid."_s);

    auto isUserConsenting = authenticator->getBoolean("isUserConsenting"_s);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!isUserConsenting, InvalidParameter, "The parameter 'isUserConsenting' is missing or invalid."_s);

    auto isUserVerified = authenticator->getBoolean("isUserVerified"_s);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!isUserVerified, InvalidParameter, "The parameter 'isUserVerified' is missing or invalid."_s);

    auto page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    return page->protectedWebsiteDataStore()->protectedVirtualAuthenticatorManager()->createAuthenticator({
        .protocol = protocol,
        .transport = toAuthenticatorTransport(parsedTransport.value()),
        .hasResidentKey = *hasResidentKey,
        .hasUserVerification = *hasUserVerification,
        .isUserConsenting = *isUserConsenting,
        .isUserVerified = *isUserVerified,
    });
#else
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented."_s);
#endif // ENABLE(WEB_AUTHN)
}

CommandResult<void> WebAutomationSession::removeVirtualAuthenticator(const String& browsingContextHandle, const String& authenticatorId)
{
#if ENABLE(WEB_AUTHN)
    auto page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool success = page->protectedWebsiteDataStore()->protectedVirtualAuthenticatorManager()->removeAuthenticator(authenticatorId);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!success, InvalidParameter, "No such authenticator exists."_s);

    return { };
#else
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented."_s);
#endif // ENABLE(WEB_AUTHN)
}

CommandResult<void> WebAutomationSession::addVirtualAuthenticatorCredential(const String& browsingContextHandle, const String& authenticatorId, Ref<JSON::Object>&& credential)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented."_s);
}

CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::Automation::VirtualAuthenticatorCredential>> /* credentials */> WebAutomationSession::getVirtualAuthenticatorCredentials(const String& browsingContextHandle, const String& authenticatorId)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented."_s);
}

CommandResult<void> WebAutomationSession::removeVirtualAuthenticatorCredential(const String& browsingContextHandle, const String& authenticatorId, const String& credentialId)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented."_s);
}

CommandResult<void> WebAutomationSession::removeAllVirtualAuthenticatorCredentials(const String& browsingContextHandle, const String& authenticatorId)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented."_s);
}

CommandResult<void> WebAutomationSession::setVirtualAuthenticatorUserVerified(const String& browsingContextHandle, const String& authenticatorId, bool isUserVerified)
{
    SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(NotImplemented, "This method is not yet implemented."_s);
}

#if ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER)
void WebAutomationSession::loadWebExtension(const Inspector::Protocol::Automation::WebExtensionResourceOptions resourceHint, const String& resource, CommandCallback<String>&& callback)
{
    ASSERT(m_client);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!m_client, InternalError, "The remote session could not load the web extension."_s);

    uint16_t options = 0;
    if (resourceHint == Inspector::Protocol::Automation::WebExtensionResourceOptions::Path)
        options |= API::AutomationSessionWebExtensionResourceOptionsPath;
    else if (resourceHint == Inspector::Protocol::Automation::WebExtensionResourceOptions::ArchivePath)
        options |= API::AutomationSessionWebExtensionResourceOptionsArchivePath;
    else
        options |= API::AutomationSessionWebExtensionResourceOptionsBase64;

    m_client->loadWebExtensionWithOptions(*this, static_cast<API::AutomationSessionWebExtensionResourceOptions>(options), resource, [protectedThis = Ref { *this }, callback = WTFMove(callback)](const String& extensionId) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!extensionId, UnableToLoadExtension, "Failed to load web extension."_s);

        callback(extensionId);
    });
}

void WebAutomationSession::unloadWebExtension(const String& identifier, CommandCallback<void>&& callback)
{
    ASSERT(m_client);
    if (!m_client)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote session could not unload the web extension."_s);

    m_client->unloadWebExtension(*this, identifier, [protectedThis = Ref { *this }, callback = WTFMove(callback)](const bool success) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!success, NoSuchExtension, "Failed to unload web extension because it could not be found."_s);

        callback({ });
    });
}
#endif

CommandResult<void> WebAutomationSession::generateTestReport(const String& browsingContextHandle, const String& message, const String& group)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    page->generateTestReport(message, group);

    return { };
}

void WebAutomationSession::setStorageAccessPermissionState(const Inspector::Protocol::Automation::BrowsingContextHandle& browsingContextHandle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, Inspector::Protocol::Automation::PermissionState state, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    RefPtr frame = frameID ? WebFrameProxy::webFrame(*frameID) : page->mainFrame();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!frame, FrameNotFound);

    Ref callbackAggregator = CallbackAggregator::create([callback = WTFMove(callback)] {
        callback({ });
    });

    Ref store = page->websiteDataStore();
    bool granted = state == Inspector::Protocol::Automation::PermissionState::Granted;
    if (!granted)
        store->clearResourceLoadStatisticsInWebProcesses([callbackAggregator] { });
    store->setStorageAccessPermissionForTesting(granted, page->identifier(), page->currentURL(), frame->url().string(), [callbackAggregator] { });
}

void WebAutomationSession::setStorageAccessPolicy(const String& browsingContextHandle, bool blocked, CommandCallback<void>&& callback)
{
    auto page = webPageProxyForHandle(browsingContextHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    Ref callbackAggregator = CallbackAggregator::create([callback = WTFMove(callback)] {
        callback({ });
    });

    Ref store = page->websiteDataStore();
    if (blocked)
        store->clearStorageAccessForTesting([callbackAggregator] { });
    store->setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(blocked, WebCore::ThirdPartyCookieBlockingMode::All, [callbackAggregator] { });
}

#if ENABLE(WEBDRIVER_BIDI)
CommandResult<void> WebAutomationSession::processBidiMessage(const String& message)
{
    m_bidiProcessor->processBidiMessage(message);

    return { };
}

void WebAutomationSession::sendBidiMessage(const String& message)
{
    m_domainNotifier->bidiMessageSent(message);
}
#endif // ENABLE(WEBDRIVER_BIDI)

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
    WTF::CompletionHandler<void(std::optional<String>&&, WebCore::FloatRect&&, std::optional<WebCore::IntPoint>&&, bool)> didComputeElementLayoutHandler = [completionHandler = WTFMove(completionHandler)](std::optional<String>&& optionalError, WebCore::FloatRect&&, std::optional<WebCore::IntPoint>&& inViewCenterPoint, bool) mutable {
        if (optionalError) {
            completionHandler(std::nullopt, AUTOMATION_COMMAND_ERROR_WITH_MESSAGE(*optionalError));
            return;
        }

        if (!inViewCenterPoint) {
            completionHandler(std::nullopt, AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
            return;
        }

        completionHandler(inViewCenterPoint, std::nullopt);
    };

    page.sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::ComputeElementLayout(page.webPageIDInMainFrameProcess(), frameID, nodeHandle, false, CoordinateSystem::LayoutViewport), WTFMove(didComputeElementLayoutHandler));
}

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
void WebAutomationSession::updateClickCount(MouseButton button, const WebCore::IntPoint& position, Seconds maxTime, int maxDistance)
{
    if (button != MouseButton::Left) {
        m_lastClickPosition.reset();
        m_clickCount = 1;
        return;
    }

    auto now = MonotonicTime::now();
    if (!m_lastClickPosition || m_lastClickPosition->distanceSquaredToPoint(position) > maxDistance || now - m_lastClickTime > maxTime) {
        m_lastClickPosition = position;
        m_lastClickTime = now;
        m_clickCount = 1;
        return;
    }

    m_lastClickTime = now;
    ++m_clickCount;
}

void WebAutomationSession::updateLastPosition(const WebCore::IntPoint& position, int maxDistance)
{
    // Cancel multiple clicks if mouse strays too far:
    if (m_lastClickPosition && m_lastClickPosition->distanceSquaredToPoint(position) > maxDistance)
        m_lastClickPosition.reset();

    m_lastPosition = position;
}

void WebAutomationSession::resetMouseState()
{
    m_clickCount = 1;
    m_lastPosition.reset();
    m_lastClickPosition.reset();
}

void WebAutomationSession::clearDoubleClicks()
{
    m_clickCount = 1;
    m_lastClickPosition.reset();
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
        auto mouseEventsFlushedCallback = [completionHandler = WTFMove(completionHandler)](CommandResult<void> result) mutable {
            if (result) {
                completionHandler(std::nullopt);
                return;
            }

            completionHandler(AutomationCommandError(VALIDATED_ERROR_MESSAGE(result.error())));
        };

        auto& callbackInMap = m_pendingMouseEventsFlushedCallbacksPerPage.add(page->identifier(), nullptr).iterator->value;
        if (callbackInMap)
            callbackInMap(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout)));
        callbackInMap = WTFMove(mouseEventsFlushedCallback);

        platformSimulateMouseInteraction(page, interaction, mouseButton, locationInViewport, platformWebModifiersFromRaw(page, m_currentModifiers), pointerType);

        // If the event does not hit test anything in the window, then it may not have been delivered.
        if (callbackInMap && !page->isProcessingMouseEvents()) {
            auto callbackToCancel = m_pendingMouseEventsFlushedCallbacksPerPage.take(page->identifier());
            callbackToCancel({ });
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
void WebAutomationSession::simulateKeyboardInteraction(WebPageProxy& page, KeyboardInteraction interaction, Variant<VirtualKey, CharKey>&& key, CompletionHandler<void(std::optional<AutomationCommandError>)>&& completionHandler)
{
    // Bridge the flushed callback to our command's completion handler.
    auto keyboardEventsFlushedCallback = [completionHandler = WTFMove(completionHandler)](CommandResult<void>&& result) mutable {
        if (result) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(AutomationCommandError(VALIDATED_ERROR_MESSAGE(result.error())));
    };

    auto& callbackInMap = m_pendingKeyboardEventsFlushedCallbacksPerPage.add(page.identifier(), nullptr).iterator->value;
    if (callbackInMap)
        callbackInMap(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout)));
    callbackInMap = WTFMove(keyboardEventsFlushedCallback);

    platformSimulateKeyboardInteraction(page, interaction, WTFMove(key));

    // If the interaction does not generate any events, then do not wait for events to be flushed.
    // This happens in some corner cases on macOS, such as releasing a key while Command is pressed.
    if (callbackInMap && !page.isProcessingKeyboardEvents()) {
        auto callbackToCancel = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.identifier());
        callbackToCancel({ });
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
        auto wheelEventsFlushedCallback = [completionHandler = WTFMove(completionHandler)](CommandResult<void>&& result) mutable {
            if (result) {
                completionHandler(std::nullopt);
                return;
            }

            completionHandler(AutomationCommandError(VALIDATED_ERROR_MESSAGE(result.error())));
        };

        auto& callbackInMap = m_pendingWheelEventsFlushedCallbacksPerPage.add(page->identifier(), nullptr).iterator->value;
        if (callbackInMap)
            callbackInMap(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout)));
        callbackInMap = WTFMove(wheelEventsFlushedCallback);

        platformSimulateWheelInteraction(page, locationInViewport, delta);

        // If the event does not hit test anything in the window, then it may not have been delivered.
        if (callbackInMap && !page->isProcessingWheelEvents()) {
            auto callbackToCancel = m_pendingWheelEventsFlushedCallbacksPerPage.take(page->identifier());
            callbackToCancel({ });
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

void WebAutomationSession::performMouseInteraction(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, Ref<JSON::Object>&& requestedPosition, Inspector::Protocol::Automation::MouseButton mouseButton, Inspector::Protocol::Automation::MouseInteraction mouseInteraction, Ref<JSON::Array>&& keyModifierStrings, CommandCallback<Ref<Inspector::Protocol::Automation::Point>>&& callback)
{
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    auto x = requestedPosition->getDouble("x"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!x, MissingParameter, "The parameter 'x' was not found."_s);
    auto floatX = static_cast<float>(*x);

    auto y = requestedPosition->getDouble("y"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!y, MissingParameter, "The parameter 'y' was not found."_s);
    auto floatY = static_cast<float>(*y);

    OptionSet<WebEventModifier> keyModifiers;
    for (auto& value : keyModifierStrings.get()) {
        auto modifierString = value->asString();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!modifierString, InvalidParameter, "The parameter 'modifiers' is invalid."_s);

        auto parsedModifier = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::KeyModifier>(modifierString);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!parsedModifier, InvalidParameter, "A modifier in the 'modifiers' array is invalid."_s);

        keyModifiers.add(protocolModifierToWebEventModifier(parsedModifier.value()));
    }

    page->getWindowFrameWithCallback([this, protectedThis = Ref { *this }, callback = WTFMove(callback), page = Ref { *page }, floatX, floatY, mouseInteraction, mouseButton, keyModifiers](WebCore::FloatRect windowFrame) mutable {
        floatX = std::min(std::max(0.0f, floatX), windowFrame.size().width());
        floatY = std::min(std::max(0.0f, floatY), windowFrame.size().height());

        WebCore::IntPoint locationInViewport = WebCore::IntPoint(static_cast<int>(floatX), static_cast<int>(floatY));

        auto mouseEventsFlushedCallback = [protectedThis = WTFMove(protectedThis), callback = WTFMove(callback), page, floatX, floatY](CommandResult<void> result) {
            if (!result) {
                callback(makeUnexpected(result.error()));
                return;
            }
            auto obscuredContentInsets = page->obscuredContentInsets();
            callback(Inspector::Protocol::Automation::Point::create()
                .setX(floatX - obscuredContentInsets.left())
                .setY(floatY - obscuredContentInsets.top())
                .release());
        };

        auto& callbackInMap = m_pendingMouseEventsFlushedCallbacksPerPage.add(page->identifier(), nullptr).iterator->value;
        if (callbackInMap)
            callbackInMap(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout)));
        callbackInMap = WTFMove(mouseEventsFlushedCallback);

        platformSimulateMouseInteraction(page, mouseInteraction, mouseButton, locationInViewport, keyModifiers, WebCore::mousePointerEventType());

        // If the event location was previously clipped and does not hit test anything in the window, then it will not be processed.
        // For compatibility with pre-W3C driver implementations, don't make this a hard error; just do nothing silently.
        // In W3C-only code paths, we can reject any pointer actions whose coordinates are outside the viewport rect.
        if (callbackInMap && !page->isProcessingMouseEvents()) {
            auto callbackToCancel = m_pendingMouseEventsFlushedCallbacksPerPage.take(page->identifier());
            callbackToCancel({ });
        }
    });
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
}

void WebAutomationSession::performKeyboardInteractions(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, Ref<JSON::Array>&& interactions, CommandCallback<void>&& callback)
{
#if !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!interactions->length(), InvalidParameter, "The parameter 'interactions' was not found or empty."_s);

    // Validate all of the parameters before performing any interactions with the browsing context under test.
    Vector<WTF::Function<void()>> actionsToPerform;
    actionsToPerform.reserveCapacity(interactions->length());

    for (const auto& interactionValue : interactions.get()) {
        auto interactionObject = interactionValue->asObject();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!interactionObject, InvalidParameter, "An interaction in the 'interactions' parameter was invalid."_s);

        auto interactionTypeString = interactionObject->getString("type"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!interactionTypeString, InvalidParameter, "An interaction in the 'interactions' parameter is missing the 'type' key."_s);
        auto interactionType = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::KeyboardInteractionType>(interactionTypeString);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!interactionType, InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'type' key."_s);

        auto virtualKeyString = interactionObject->getString("key"_s);
        if (!!virtualKeyString) {
            std::optional<VirtualKey> virtualKey = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::VirtualKey>(virtualKeyString);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!virtualKey, InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'key' value."_s);

            actionsToPerform.append([this, protectedThis = Ref { *this }, page, interactionType, virtualKey] {
                platformSimulateKeyboardInteraction(*page, interactionType.value(), virtualKey.value());
            });
        }

        auto keySequence = interactionObject->getString("text"_s);
        if (!!keySequence) {
            switch (interactionType.value()) {
            case Inspector::Protocol::Automation::KeyboardInteractionType::KeyPress:
            case Inspector::Protocol::Automation::KeyboardInteractionType::KeyRelease:
                // 'KeyPress' and 'KeyRelease' are meant for a virtual key and are not supported for a string (sequence of codepoints).
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'key' value."_s);

            case Inspector::Protocol::Automation::KeyboardInteractionType::InsertByKey:
                actionsToPerform.append([this, protectedThis = Ref { *this }, page, keySequence] {
                    platformSimulateKeySequence(*page, keySequence);
                });
                break;
            }
        }

        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!virtualKeyString && !keySequence, MissingParameter, "An interaction in the 'interactions' parameter is missing both 'key' and 'text'. One must be provided."_s);
    }

    ASSERT(actionsToPerform.size());
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!actionsToPerform.size(), InternalError, "No actions to perform."_s);

    auto keyboardEventsFlushedCallback = [protectedThis = Ref { *this }, callback = WTFMove(callback), page = Ref { *page }](Inspector::CommandResult<void> result) {
        if (!result) {
            callback(makeUnexpected(result.error()));
            return;
        }

        callback({ });
    };

    auto& callbackInMap = m_pendingKeyboardEventsFlushedCallbacksPerPage.add(page->identifier(), nullptr).iterator->value;
    if (callbackInMap)
        callbackInMap(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout)));

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
    case Inspector::Protocol::Automation::VirtualKey::CommandRight:
        return Inspector::Protocol::Automation::VirtualKey::Command;
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

#if !ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
static std::optional<char32_t> pressedCharKey(const String& pressedCharKeyString)
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
#endif // !ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
#endif // ENABLE(WEBDRIVER_ACTIONS_API)

void WebAutomationSession::performInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, Ref<JSON::Array>&& inputSources, Ref<JSON::Array>&& steps, CommandCallback<void>&& callback)
{
    // This command implements WebKit support for §17.5 Perform Actions.

#if !ENABLE(WEBDRIVER_ACTIONS_API)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    // Parse and validate Automation protocol arguments. By this point, the driver has
    // already performed the steps in §17.3 Processing Actions Requests.
    if (!inputSources->length())
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'inputSources' was not found or empty."_s);

    HashSet<String> sourceIdSet;
    for (const auto& inputSourceValue : inputSources.get()) {
        auto inputSourceObject = inputSourceValue->asObject();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!inputSourceObject, InvalidParameter, "An input source in the 'inputSources' parameter was invalid."_s);

        auto sourceId = inputSourceObject->getString("sourceId"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!sourceId, InvalidParameter, "An input source in the 'inputSources' parameter is missing a 'sourceId'."_s);

        auto sourceType = inputSourceObject->getString("sourceType"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!sourceType, InvalidParameter, "An input source in the 'inputSources' parameter is missing a 'sourceType'."_s);

        auto parsedInputSourceType = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::InputSourceType>(sourceType);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!parsedInputSourceType, InvalidParameter, "An input source in the 'inputSources' parameter has an invalid 'sourceType'."_s);

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
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(inputSourceType == SimulatedInputSourceType::Mouse, NotImplemented, "Mouse input sources are not yet supported."_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(inputSourceType == SimulatedInputSourceType::Pen, NotImplemented, "Pen input sources are not yet supported."_s);
#endif
#if !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(inputSourceType == SimulatedInputSourceType::Touch, NotImplemented, "Touch input sources are not yet supported."_s);
#endif
#if !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(inputSourceType == SimulatedInputSourceType::Keyboard, NotImplemented, "Keyboard input sources are not yet supported."_s);
#endif
#if !ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(inputSourceType == SimulatedInputSourceType::Wheel, NotImplemented, "Wheel input sources are not yet supported."_s);
#endif
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(sourceIdSet.contains(sourceId), InvalidParameter, "Two input sources with the same sourceId were specified."_s);

        sourceIdSet.add(sourceId);
        m_inputSources.ensure(sourceId, [inputSourceType] {
            return SimulatedInputSource::create(inputSourceType);
        });
    }

    Vector<SimulatedInputKeyFrame> keyFrames;

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!steps->length(), InvalidParameter, "The parameter 'steps' was not found or empty."_s);

    for (const auto& stepValue : steps.get()) {
        auto stepObject = stepValue->asObject();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!stepObject, InvalidParameter, "A step in the 'steps' parameter was not an object."_s);

        auto stepStates = stepObject->getArray("states"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!stepStates, InvalidParameter, "A step is missing the 'states' property."_s);

        Vector<SimulatedInputKeyFrame::StateEntry> entries;
        entries.reserveCapacity(stepStates->length());

        for (const auto& stateValue : *stepStates) {
            auto stateObject = stateValue->asObject();
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!stateObject, InvalidParameter, "Encountered a non-object step state."_s);

            auto sourceId = stateObject->getString("sourceId"_s);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!sourceId, InvalidParameter, "Step state lacks required 'sourceId' property."_s);

            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!m_inputSources.contains(sourceId), InvalidParameter, "Unknown 'sourceId' specified."_s);

            Ref inputSource = *m_inputSources.get(sourceId);
            SimulatedInputSourceState sourceState { };

            auto pressedCharKeyString = stateObject->getString("pressedCharKey"_s);
            if (!!pressedCharKeyString) {
#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(WTF::numGraphemeClusters(pressedCharKeyString) != 1, InvalidParameter, "Invalid 'pressedCharKey'."_s);
                sourceState.pressedCharKeys.add(pressedCharKeyString);
#else
                auto charKey = pressedCharKey(pressedCharKeyString);
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!charKey, InvalidParameter, "Invalid 'pressedCharKey'."_s);
                sourceState.pressedCharKeys.add(*charKey);
#endif
            }

            if (auto pressedVirtualKeysArray = stateObject->getArray("pressedVirtualKeys"_s)) {
                VirtualKeyMap pressedVirtualKeys;

                for (auto& value : *pressedVirtualKeysArray) {
                    auto pressedVirtualKeyString = value->asString();
                    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!pressedVirtualKeyString, InvalidParameter, "Encountered a non-string virtual key value."_s);

                    std::optional<VirtualKey> parsedVirtualKey = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::VirtualKey>(pressedVirtualKeyString);
                    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!parsedVirtualKey, InvalidParameter, "Encountered an unknown virtual key value."_s);

                    pressedVirtualKeys.add(normalizedVirtualKey(parsedVirtualKey.value()), parsedVirtualKey.value());
                }

                sourceState.pressedVirtualKeys = pressedVirtualKeys;
            }

            auto pressedButtonString = stateObject->getString("pressedButton"_s);
            if (!!pressedButtonString) {
                auto protocolButton = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseButton>(pressedButtonString);
                sourceState.pressedMouseButton = protocolButton.value_or(MouseButton::None);
            }

            auto mouseInteractionString = stateObject->getString("mouseInteraction"_s);
            if (!!mouseInteractionString)
                sourceState.mouseInteraction = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseInteraction>(mouseInteractionString);

            auto originString = stateObject->getString("origin"_s);
            if (!!originString)
                sourceState.origin = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseMoveOrigin>(originString);

            if (sourceState.origin && sourceState.origin.value() == Inspector::Protocol::Automation::MouseMoveOrigin::Element) {
                auto nodeHandleString = stateObject->getString("nodeHandle"_s);
                ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!nodeHandleString, InvalidParameter, "Node handle not provided for 'Element' origin"_s);
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

            entries.append(std::pair<SimulatedInputSource&, SimulatedInputSourceState> { inputSource, sourceState });
        }

        keyFrames.append(SimulatedInputKeyFrame(WTFMove(entries)));
    }

    Ref inputDispatcher = inputDispatcherForPage(*page);
    if (inputDispatcher->isActive()) {
        ASSERT_NOT_REACHED();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "A previous interaction is still underway."_s);
    }

    // Delegate the rest of §17.4 Dispatching Actions to the dispatcher.
    inputDispatcher->run(frameID, WTFMove(keyFrames), m_inputSources, [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<AutomationCommandError> error) {
        if (error)
            callback(makeUnexpected(error.value().toProtocolString()));
        else
            callback({ });
    });
#endif // ENABLE(WEBDRIVER_ACTIONS_API)
}

void WebAutomationSession::cancelInteractionSequence(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, CommandCallback<void>&& callback)
{
    // This command implements WebKit support for §17.6 Release Actions.

#if !ENABLE(WEBDRIVER_ACTIONS_API)
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);

    Vector<SimulatedInputKeyFrame> keyFrames({ SimulatedInputKeyFrame::keyFrameToResetInputSources(m_inputSources) });
    Ref inputDispatcher = inputDispatcherForPage(*page);
    inputDispatcher->cancel();
    
    inputDispatcher->run(frameID, WTFMove(keyFrames), m_inputSources, [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<AutomationCommandError> error) {
        if (error)
            callback(makeUnexpected(error.value().toProtocolString()));
        else
            callback({ });
        m_inputSources.clear();
    });
#endif // ENABLE(WEBDRIVER_ACTIONS_API)
}

void WebAutomationSession::takeScreenshot(const Inspector::Protocol::Automation::BrowsingContextHandle& handle, const Inspector::Protocol::Automation::FrameHandle& frameHandle, const Inspector::Protocol::Automation::NodeHandle& nodeHandle, std::optional<bool>&& optionalScrollIntoViewIfNeeded, std::optional<bool>&& optionalClipToViewport, CommandCallback<String>&& callback)
{
    auto page = webPageProxyForHandle(handle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, WindowNotFound);

    bool frameNotFound = false;
    auto frameID = webFrameIDForHandle(frameHandle, frameNotFound);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(frameNotFound, FrameNotFound);
    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    bool clipToViewport = optionalClipToViewport ? *optionalClipToViewport : false;

#if PLATFORM(COCOA) || (!PLATFORM(GTK) && !(PLATFORM(WPE) && USE(SKIA)))
    auto ipcCompletionHandler = [] (CommandCallback<String>&& callback) mutable {
        return CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&, String&&)> { [callback = WTFMove(callback)] (std::optional<ShareableBitmap::Handle>&& imageDataHandle, String&& errorType) mutable {
            if (!errorType.isEmpty())
                return callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType)));

            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!imageDataHandle, InternalError);
            std::optional<String> base64EncodedData = platformGetBase64EncodedPNGData(WTFMove(*imageDataHandle));
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!base64EncodedData, InternalError);

            callback(base64EncodedData.value());
        } };
    };
#endif
#if PLATFORM(COCOA)
    // FIXME: <webkit.org/b/242215> We can currently only get viewport snapshots from the UIProcess, so fall back to
    // taking a snapshot in the WebProcess if we need to snapshot a specific element. This has the side effect of not
    // accurately showing all CSS transforms in the snapshot.
    //
    // There is still a tradeoff by going to the UIProcess for viewport snapshots on macOS. We can not currently get a
    // snapshot of the entire viewport without the window's rounded corners being excluded. So we trade off those corner
    // pixels for accurate pixels in the rest of the viewport which help us verify features like CSS transforms are
    // actually behaving correctly.
    if (!nodeHandle.isEmpty())
        return page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::TakeScreenshot(page->webPageIDInMainFrameProcess(), frameID, nodeHandle, scrollIntoViewIfNeeded, clipToViewport), ipcCompletionHandler(WTFMove(callback)));
#endif
#if PLATFORM(GTK) || PLATFORM(COCOA) || (PLATFORM(WPE) && USE(SKIA))
    Function<void(WebPageProxy&, std::optional<WebCore::IntRect>&&, CommandCallback<String>&&)> takeViewSnapshot = [](WebPageProxy& page, std::optional<WebCore::IntRect>&& rect, CommandCallback<String>&& callback) {
        page.callAfterNextPresentationUpdate([page = Ref { page }, rect = WTFMove(rect), callback = WTFMove(callback)] () mutable {
            RefPtr snapshot = page->takeViewSnapshot(WTFMove(rect), ForceSoftwareCapturingViewportSnapshot::Yes);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!snapshot, InternalError);

            std::optional<String> base64EncodedData = platformGetBase64EncodedPNGData(*snapshot);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!base64EncodedData, InternalError);

            callback(base64EncodedData.value());
        });
    };

    if (nodeHandle.isEmpty()) {
        takeViewSnapshot(*page, std::nullopt, WTFMove(callback));
        return;
    }

    CompletionHandler<void(std::optional<String>&&, WebCore::IntRect&&)> completionHandler = [page = Ref { *page }, callback = WTFMove(callback), takeViewSnapshot = WTFMove(takeViewSnapshot)](std::optional<String>&& optionalError, WebCore::IntRect&& rect) mutable {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF_SET(optionalError);

        takeViewSnapshot(page.get(), WTFMove(rect), WTFMove(callback));
    };

    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::SnapshotRectForScreenshot(page->webPageIDInMainFrameProcess(), frameID, nodeHandle, scrollIntoViewIfNeeded, clipToViewport), WTFMove(completionHandler));
#else
    page->sendWithAsyncReplyToProcessContainingFrameWithoutDestinationIdentifier(frameID, Messages::WebAutomationSessionProxy::TakeScreenshot(page->webPageIDInMainFrameProcess(), frameID, nodeHandle, scrollIntoViewIfNeeded, clipToViewport), ipcCompletionHandler(WTFMove(callback)));
#endif
}

#if ENABLE(WEBDRIVER_BIDI)
static String logEntryLevelForMessage(const JSC::MessageType& messageType, const MessageLevel& messageLevel)
{
    if (messageType == JSC::MessageType::Assert || messageLevel == JSC::MessageLevel::Error)
        return "error"_s;
    if (messageType == JSC::MessageType::Trace || messageLevel == JSC::MessageLevel::Debug)
        return "debug"_s;
    if (messageLevel == MessageLevel::Warning)
        return "warn"_s;
    return "info"_s;
}

static String logEntryMethodNameForMessage(const JSC::MessageType& messageType, const MessageLevel& messageLevel)
{
    if (messageType == JSC::MessageType::Assert)
        return "assert"_s;
    if (messageType == JSC::MessageType::Trace)
        return "trace"_s;
    if (messageLevel == JSC::MessageLevel::Warning)
        return "warn"_s;
    if (messageLevel == JSC::MessageLevel::Error)
        return "error"_s;
    if (messageLevel == JSC::MessageLevel::Debug)
        return "debug"_s;
    return "log"_s;
}

static String logEntryTypeForMessage(const JSC::MessageSource& messageSource)
{
    if (messageSource == JSC::MessageSource::JS)
        return "javascript"_s;
    return "console"_s;
}
#endif // ENABLE(WEBDRIVER_BIDI)

void WebAutomationSession::logEntryAdded(const JSC::MessageSource& messageSource, const JSC::MessageLevel& messageLevel, const String& messageText, const JSC::MessageType& messageType, const WallTime& timestamp)
{
#if ENABLE(WEBDRIVER_BIDI)
    // FIXME Support getting source information
    // https://bugs.webkit.org/show_bug.cgi?id=282978
    String sourceString;

    auto level = logEntryLevelForMessage(messageType, messageLevel);
    auto method = logEntryMethodNameForMessage(messageType, messageLevel);
    auto type = logEntryTypeForMessage(messageSource);
    auto milliseconds =  timestamp.secondsSinceEpoch().milliseconds();

    // FIXME Get browsing context handle and source info
    // https://bugs.webkit.org/show_bug.cgi?id=282981
    m_bidiProcessor->logDomainNotifier().entryAdded(level, sourceString, messageText, milliseconds, type, method);
#else
    UNUSED_PARAM(messageSource);
    UNUSED_PARAM(messageLevel);
    UNUSED_PARAM(messageText);
    UNUSED_PARAM(messageType);
    UNUSED_PARAM(timestamp);
#endif
}

#if !PLATFORM(COCOA) && !USE(CAIRO) && !USE(SKIA)
std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(ShareableBitmap::Handle&&)
{
    return std::nullopt;
}

std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ViewSnapshot&)
{
    return std::nullopt;
}
#endif // !PLATFORM(COCOA) && !USE(CAIRO) && !USE(SKIA)

#if !PLATFORM(COCOA)
std::optional<String> WebAutomationSession::platformGenerateLocalFilePathForRemoteFile(const String&, const String&)
{
    return std::nullopt;
}
#endif // !PLATFORM(COCOA)

} // namespace WebKit
