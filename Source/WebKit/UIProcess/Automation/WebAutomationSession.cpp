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
#include "APIOpenPanelParameters.h"
#include "AutomationProtocolObjects.h"
#include "CoordinateSystem.h"
#include "WebAutomationSessionMacros.h"
#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebCookieManagerProxy.h"
#include "WebInspectorProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebProcessPool.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/URL.h>
#include <algorithm>
#include <wtf/HashMap.h>
#include <wtf/Optional.h>
#include <wtf/UUID.h>
#include <wtf/text/StringConcatenate.h>

using namespace Inspector;

namespace WebKit {

// §8. Sessions
// https://www.w3.org/TR/webdriver/#dfn-session-page-load-timeout
static const Seconds defaultPageLoadTimeout = 300_s;
// https://www.w3.org/TR/webdriver/#dfn-page-loading-strategy
static const Inspector::Protocol::Automation::PageLoadStrategy defaultPageLoadStrategy = Inspector::Protocol::Automation::PageLoadStrategy::Normal;

WebAutomationSession::WebAutomationSession()
    : m_client(std::make_unique<API::AutomationSessionClient>())
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_domainDispatcher(AutomationBackendDispatcher::create(m_backendDispatcher, this))
    , m_domainNotifier(std::make_unique<AutomationFrontendDispatcher>(m_frontendRouter))
    , m_loadTimer(RunLoop::main(), this, &WebAutomationSession::loadTimerFired)
{
}

WebAutomationSession::~WebAutomationSession()
{
    ASSERT(!m_client);

    if (m_processPool)
        m_processPool->removeMessageReceiver(Messages::WebAutomationSession::messageReceiverName());
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

void WebAutomationSession::connect(Inspector::FrontendChannel* channel, bool isAutomaticConnection, bool immediatelyPause)
{
    UNUSED_PARAM(isAutomaticConnection);
    UNUSED_PARAM(immediatelyPause);

    m_remoteChannel = channel;
    m_frontendRouter->connectFrontend(channel);

    setIsPaired(true);
}

void WebAutomationSession::disconnect(Inspector::FrontendChannel* channel)
{
    ASSERT(channel == m_remoteChannel);
    terminate();
}

#endif // ENABLE(REMOTE_INSPECTOR)

void WebAutomationSession::terminate()
{
#if ENABLE(REMOTE_INSPECTOR)
    if (Inspector::FrontendChannel* channel = m_remoteChannel) {
        m_remoteChannel = nullptr;
        m_frontendRouter->disconnectFrontend(channel);
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

std::optional<uint64_t> WebAutomationSession::webFrameIDForHandle(const String& handle)
{
    if (handle.isEmpty())
        return 0;

    auto iter = m_handleWebFrameMap.find(handle);
    if (iter == m_handleWebFrameMap.end())
        return std::nullopt;

    return iter->value;
}

String WebAutomationSession::handleForWebFrameID(uint64_t frameID)
{
    if (!frameID)
        return emptyString();

    for (auto& process : m_processPool->processes()) {
        if (WebFrameProxy* frame = process->webFrame(frameID)) {
            if (frame->isMainFrame())
                return emptyString();
            break;
        }
    }

    auto iter = m_webFrameHandleMap.find(frameID);
    if (iter != m_webFrameHandleMap.end())
        return iter->value;

    String handle = "frame-" + createCanonicalUUIDString().convertToASCIIUppercase();

    auto firstAddResult = m_webFrameHandleMap.add(frameID, handle);
    RELEASE_ASSERT(firstAddResult.isNewEntry);

    auto secondAddResult = m_handleWebFrameMap.add(handle, frameID);
    RELEASE_ASSERT(secondAddResult.isNewEntry);

    return handle;
}

String WebAutomationSession::handleForWebFrameProxy(const WebFrameProxy& webFrameProxy)
{
    return handleForWebFrameID(webFrameProxy.frameID());
}

RefPtr<Inspector::Protocol::Automation::BrowsingContext> WebAutomationSession::buildBrowsingContextForPage(WebPageProxy& page)
{
    WebCore::FloatRect windowFrame;
    page.getWindowFrame(windowFrame);

    auto originObject = Inspector::Protocol::Automation::Point::create()
        .setX(windowFrame.x())
        .setY(windowFrame.y())
        .release();

    auto sizeObject = Inspector::Protocol::Automation::Size::create()
        .setWidth(windowFrame.width())
        .setHeight(windowFrame.height())
        .release();

    String handle = handleForWebPageProxy(page);

    return Inspector::Protocol::Automation::BrowsingContext::create()
        .setHandle(handle)
        .setActive(m_activeBrowsingContextHandle == handle)
        .setUrl(page.pageLoadState().activeURL())
        .setWindowOrigin(WTFMove(originObject))
        .setWindowSize(WTFMove(sizeObject))
        .release();
}

// Platform-independent Commands.

void WebAutomationSession::getBrowsingContexts(Inspector::ErrorString& errorString, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Automation::BrowsingContext>>& contexts)
{
    contexts = Inspector::Protocol::Array<Inspector::Protocol::Automation::BrowsingContext>::create();

    for (auto& process : m_processPool->processes()) {
        for (auto* page : process->pages()) {
            ASSERT(page);
            if (!page->isControlledByAutomation())
                continue;

            contexts->addItem(buildBrowsingContextForPage(*page));
        }
    }
}

void WebAutomationSession::getBrowsingContext(Inspector::ErrorString& errorString, const String& handle, RefPtr<Inspector::Protocol::Automation::BrowsingContext>& context)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    context = buildBrowsingContextForPage(*page);
}

void WebAutomationSession::createBrowsingContext(Inspector::ErrorString& errorString, String* handle)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote session could not request a new browsing context.");

    WebPageProxy* page = m_client->didRequestNewWindow(*this);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The remote session failed to create a new browsing context.");

    m_activeBrowsingContextHandle = *handle = handleForWebPageProxy(*page);
}

void WebAutomationSession::closeBrowsingContext(Inspector::ErrorString& errorString, const String& handle)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (handle == m_activeBrowsingContextHandle)
        m_activeBrowsingContextHandle = emptyString();

    page->closePage(false);
}

void WebAutomationSession::switchToBrowsingContext(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String* optionalFrameHandle)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    std::optional<uint64_t> frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString());
    if (!frameID)
        FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    // FIXME: We don't need to track this in WK2. Remove in a follow up.
    m_activeBrowsingContextHandle = browsingContextHandle;

    page->setFocus(true);
    page->process().send(Messages::WebAutomationSessionProxy::FocusFrame(page->pageID(), frameID.value()), 0);
}

void WebAutomationSession::resizeWindowOfBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const JSON::Object& sizeObject)
{
#if PLATFORM(IOS)
    FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    float width;
    if (!sizeObject.getDouble(WTF::ASCIILiteral("width"), width))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'width' parameter was not found or invalid.");

    float height;
    if (!sizeObject.getDouble(WTF::ASCIILiteral("height"), height))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'height' parameter was not found or invalid.");

    if (width < 0)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'width' parameter had an invalid value.");

    if (height < 0)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'height' parameter had an invalid value.");

    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WebCore::FloatRect originalFrame;
    page->getWindowFrame(originalFrame);

    WebCore::FloatRect newFrame = WebCore::FloatRect(originalFrame.location(), WebCore::FloatSize(width, height));
    if (newFrame == originalFrame)
        return;

    page->setWindowFrame(newFrame);

#if !PLATFORM(GTK)
    // If nothing changed at all, it's probably fair to report that something went wrong.
    // (We can't assume that the requested frame size will be honored exactly, however.)
    WebCore::FloatRect updatedFrame;
    page->getWindowFrame(updatedFrame);
    if (originalFrame == updatedFrame)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The window size was expected to have changed, but did not.");
#endif
#endif
}

void WebAutomationSession::moveWindowOfBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const JSON::Object& positionObject)
{
#if PLATFORM(IOS)
    FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    float x;
    if (!positionObject.getDouble(WTF::ASCIILiteral("x"), x))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'x' parameter was not found or invalid.");

    float y;
    if (!positionObject.getDouble(WTF::ASCIILiteral("y"), y))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The 'y' parameter was not found or invalid.");

    if (x < 0)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'x' parameter had an invalid value.");

    if (y < 0)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The 'y' parameter had an invalid value.");

    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WebCore::FloatRect originalFrame;
    page->getWindowFrame(originalFrame);

    WebCore::FloatRect newFrame = WebCore::FloatRect(WebCore::FloatPoint(x, y), originalFrame.size());
    if (newFrame == originalFrame)
        return;

    page->setWindowFrame(newFrame);

#if !PLATFORM(GTK)
    // If nothing changed at all, it's probably fair to report that something went wrong.
    // (We can't assume that the requested frame size will be honored exactly, however.)
    WebCore::FloatRect updatedFrame;
    page->getWindowFrame(updatedFrame);
    if (originalFrame == updatedFrame)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "The window position was expected to have changed, but did not.");
#endif
#endif
}

static std::optional<Inspector::Protocol::Automation::PageLoadStrategy> pageLoadStrategyFromStringParameter(const String* optionalPageLoadStrategyString)
{
    if (!optionalPageLoadStrategyString)
        return defaultPageLoadStrategy;

    auto parsedPageLoadStrategy = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::PageLoadStrategy>(*optionalPageLoadStrategyString);
    if (!parsedPageLoadStrategy)
        return std::nullopt;
    return parsedPageLoadStrategy;
}

void WebAutomationSession::waitForNavigationToComplete(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String* optionalFrameHandle, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<WaitForNavigationToCompleteCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    // If page is loading and there's an active JavaScript dialog is probably because the
    // dialog was started in an onload handler, so in case of normal page load strategy the
    // load will not finish until the dialog is dismissed. Instead of waiting for the timeout,
    // we return without waiting since we know it will timeout for sure. We want to check
    // arguments first, though.
    bool shouldTimeoutDueToUnexpectedAlert = pageLoadStrategy.value() == Inspector::Protocol::Automation::PageLoadStrategy::Normal
        && page->pageLoadState().isLoading() && m_client->isShowingJavaScriptDialogOnPage(*this, *page);

    if (optionalFrameHandle && !optionalFrameHandle->isEmpty()) {
        std::optional<uint64_t> frameID = webFrameIDForHandle(*optionalFrameHandle);
        if (!frameID)
            FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);
        WebFrameProxy* frame = page->process().webFrame(frameID.value());
        if (!frame)
            FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);
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
    if (loadStrategy == Inspector::Protocol::Automation::PageLoadStrategy::None || !page.pageLoadState().isLoading()) {
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

void WebAutomationSession::respondToPendingPageNavigationCallbacksWithTimeout(HashMap<uint64_t, RefPtr<Inspector::BackendDispatcher::CallbackBase>>& map)
{
    Inspector::ErrorString timeoutError = STRING_FOR_PREDEFINED_ERROR_NAME(Timeout);
    for (auto id : map.keys()) {
        auto page = WebProcessProxy::webPage(id);
        auto callback = map.take(id);
        if (page && m_client->isShowingJavaScriptDialogOnPage(*this, *page))
            callback->sendSuccess(JSON::Object::create());
        else
            callback->sendFailure(timeoutError);
    }
}

static WebPageProxy* findPageForFrameID(const WebProcessPool& processPool, uint64_t frameID)
{
    for (auto& process : processPool.processes()) {
        if (auto* frame = process->webFrame(frameID))
            return frame->page();
    }
    return nullptr;
}

void WebAutomationSession::respondToPendingFrameNavigationCallbacksWithTimeout(HashMap<uint64_t, RefPtr<Inspector::BackendDispatcher::CallbackBase>>& map)
{
    Inspector::ErrorString timeoutError = STRING_FOR_PREDEFINED_ERROR_NAME(Timeout);
    for (auto id : map.keys()) {
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

void WebAutomationSession::willShowJavaScriptDialog(WebPageProxy& page)
{
    // Wait until the next run loop iteration to give time for the client to show the dialog,
    // then check if the dialog is still present. If the page is loading, the dialog will block
    // the load in case of normal strategy, so we want to dispatch all pending navigation callbacks.
    // If the dialog was shown during a script execution, we want to finish the evaluateJavaScriptFunction
    // operation with an unexpected alert open error.
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), page = makeRef(page)] {
        if (!page->isValid() || !m_client || !m_client->isShowingJavaScriptDialogOnPage(*this, page))
            return;

        if (page->pageLoadState().isLoading()) {
            respondToPendingFrameNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerFrame);
            respondToPendingPageNavigationCallbacksWithTimeout(m_pendingNormalNavigationInBrowsingContextCallbacksPerPage);
        }

        if (!m_evaluateJavaScriptFunctionCallbacks.isEmpty()) {
            Inspector::ErrorString unexpectedAlertOpenError = STRING_FOR_PREDEFINED_ERROR_NAME(UnexpectedAlertOpen);
            for (auto key : m_evaluateJavaScriptFunctionCallbacks.keys()) {
                auto callback = m_evaluateJavaScriptFunctionCallbacks.take(key);
                callback->sendFailure(unexpectedAlertOpenError);
            }
        }
    });
}

void WebAutomationSession::navigateBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const String& url, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<NavigateBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->loadRequest(WebCore::URL(WebCore::URL(), url));
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::goBackInBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<GoBackInBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->goBack();
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::goForwardInBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<GoForwardInBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
    auto pageLoadTimeout = optionalPageLoadTimeout ? Seconds::fromMilliseconds(*optionalPageLoadTimeout) : defaultPageLoadTimeout;

    page->goForward();
    waitForNavigationToCompleteOnPage(*page, pageLoadStrategy.value(), pageLoadTimeout, WTFMove(callback));
}

void WebAutomationSession::reloadBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const String* optionalPageLoadStrategyString, const int* optionalPageLoadTimeout, Ref<ReloadBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    auto pageLoadStrategy = pageLoadStrategyFromStringParameter(optionalPageLoadStrategyString);
    if (!pageLoadStrategy)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'pageLoadStrategy' is invalid.");
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

void WebAutomationSession::keyboardEventsFlushedForPage(const WebPageProxy& page)
{
    if (auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.pageID()))
        callback->sendSuccess(JSON::Object::create());
}

void WebAutomationSession::willClosePage(const WebPageProxy& page)
{
    String handle = handleForWebPageProxy(page);
    m_domainNotifier->browsingContextCleared(handle);
}

static bool fileCanBeAcceptedForUpload(const String& filename, const HashSet<String>& allowedMIMETypes, const HashSet<String>& allowedFileExtensions)
{
    if (!WebCore::fileExists(filename))
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
    Vector<String> components;
    mappedMIMEType.split('/', false, components);
    if (components.size() != 2)
        return false;

    String wildcardedMIMEType = makeString(components[0], "/*");
    if (allowedMIMETypes.contains(wildcardedMIMEType))
        return true;

    return false;
}

void WebAutomationSession::handleRunOpenPanel(const WebPageProxy& page, const WebFrameProxy&, const API::OpenPanelParameters& parameters, WebOpenPanelResultListenerProxy& resultListener)
{
    if (!m_filesToSelectForFileUpload.size()) {
        resultListener.cancel();
        m_domainNotifier->fileChooserDismissed(m_activeBrowsingContextHandle, true);
        return;
    }

    if (m_filesToSelectForFileUpload.size() > 1 && !parameters.allowMultipleFiles()) {
        resultListener.cancel();
        m_domainNotifier->fileChooserDismissed(m_activeBrowsingContextHandle, true);
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
            m_domainNotifier->fileChooserDismissed(m_activeBrowsingContextHandle, true);
            return;
        }
    }

    resultListener.chooseFiles(m_filesToSelectForFileUpload);
    m_domainNotifier->fileChooserDismissed(m_activeBrowsingContextHandle, false);
}

void WebAutomationSession::evaluateJavaScriptFunction(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String* optionalFrameHandle, const String& function, const JSON::Array& arguments, const bool* optionalExpectsImplicitCallbackArgument, const int* optionalCallbackTimeout, Ref<EvaluateJavaScriptFunctionCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    std::optional<uint64_t> frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString());
    if (!frameID)
        FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

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

    page->process().send(Messages::WebAutomationSessionProxy::EvaluateJavaScriptFunction(page->pageID(), frameID.value(), function, argumentsVector, expectsImplicitCallbackArgument, callbackTimeout, callbackID), 0);
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

void WebAutomationSession::resolveChildFrameHandle(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String* optionalFrameHandle, const int* optionalOrdinal, const String* optionalName, const String* optionalNodeHandle, Ref<ResolveChildFrameHandleCallback>&& callback)
{
    if (!optionalOrdinal && !optionalName && !optionalNodeHandle)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "Command must specify a child frame by ordinal, name, or element handle.");

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    std::optional<uint64_t> frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString());
    if (!frameID)
        FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    uint64_t callbackID = m_nextResolveFrameCallbackID++;
    m_resolveChildFrameHandleCallbacks.set(callbackID, WTFMove(callback));

    if (optionalNodeHandle) {
        page->process().send(Messages::WebAutomationSessionProxy::ResolveChildFrameWithNodeHandle(page->pageID(), frameID.value(), *optionalNodeHandle, callbackID), 0);
        return;
    }

    if (optionalName) {
        page->process().send(Messages::WebAutomationSessionProxy::ResolveChildFrameWithName(page->pageID(), frameID.value(), *optionalName, callbackID), 0);
        return;
    }

    if (optionalOrdinal) {
        page->process().send(Messages::WebAutomationSessionProxy::ResolveChildFrameWithOrdinal(page->pageID(), frameID.value(), *optionalOrdinal, callbackID), 0);
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebAutomationSession::didResolveChildFrame(uint64_t callbackID, uint64_t frameID, const String& errorType)
{
    auto callback = m_resolveChildFrameHandleCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty())
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType));
    else
        callback->sendSuccess(handleForWebFrameID(frameID));
}

void WebAutomationSession::resolveParentFrameHandle(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& frameHandle, Ref<ResolveParentFrameHandleCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    std::optional<uint64_t> frameID = webFrameIDForHandle(frameHandle);
    if (!frameID)
        FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    uint64_t callbackID = m_nextResolveParentFrameCallbackID++;
    m_resolveParentFrameHandleCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::ResolveParentFrame(page->pageID(), frameID.value(), callbackID), 0);
}

void WebAutomationSession::didResolveParentFrame(uint64_t callbackID, uint64_t frameID, const String& errorType)
{
    auto callback = m_resolveParentFrameHandleCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty())
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType));
    else
        callback->sendSuccess(handleForWebFrameID(frameID));
}

static std::optional<CoordinateSystem> protocolStringToCoordinateSystem(const String& coordinateSystemString)
{
    if (coordinateSystemString == "Page")
        return CoordinateSystem::Page;
    if (coordinateSystemString == "LayoutViewport")
        return CoordinateSystem::LayoutViewport;
    if (coordinateSystemString == "VisualViewport")
        return CoordinateSystem::VisualViewport;
    return std::nullopt;
}

void WebAutomationSession::computeElementLayout(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& frameHandle, const String& nodeHandle, const bool* optionalScrollIntoViewIfNeeded, const String& coordinateSystemString, Ref<ComputeElementLayoutCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    std::optional<uint64_t> frameID = webFrameIDForHandle(frameHandle);
    if (!frameID)
        FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    std::optional<CoordinateSystem> coordinateSystem = protocolStringToCoordinateSystem(coordinateSystemString);
    if (!coordinateSystem)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'coordinateSystem' is invalid.");

    uint64_t callbackID = m_nextComputeElementLayoutCallbackID++;
    m_computeElementLayoutCallbacks.set(callbackID, WTFMove(callback));

    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    page->process().send(Messages::WebAutomationSessionProxy::ComputeElementLayout(page->pageID(), frameID.value(), nodeHandle, scrollIntoViewIfNeeded, coordinateSystem.value(), callbackID), 0);
}

void WebAutomationSession::didComputeElementLayout(uint64_t callbackID, WebCore::IntRect rect, std::optional<WebCore::IntPoint> inViewCenterPoint, bool isObscured, const String& errorType)
{
    auto callback = m_computeElementLayoutCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty()) {
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType));
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
}

void WebAutomationSession::selectOptionElement(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& frameHandle, const String& nodeHandle, Ref<SelectOptionElementCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    std::optional<uint64_t> frameID = webFrameIDForHandle(frameHandle);
    if (!frameID)
        FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    uint64_t callbackID = m_nextSelectOptionElementCallbackID++;
    m_selectOptionElementCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::SelectOptionElement(page->pageID(), frameID.value(), nodeHandle, callbackID), 0);
}

void WebAutomationSession::didSelectOptionElement(uint64_t callbackID, const String& errorType)
{
    auto callback = m_selectOptionElementCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty()) {
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType));
        return;
    }

    callback->sendSuccess();
}


void WebAutomationSession::isShowingJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle, bool* result)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    *result = m_client->isShowingJavaScriptDialogOnPage(*this, *page);
}

void WebAutomationSession::dismissCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    m_client->dismissCurrentJavaScriptDialogOnPage(*this, *page);
}

void WebAutomationSession::acceptCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    m_client->acceptCurrentJavaScriptDialogOnPage(*this, *page);
}

void WebAutomationSession::messageOfCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle, String* text)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    *text = m_client->messageOfCurrentJavaScriptDialogOnPage(*this, *page);
}

void WebAutomationSession::setUserInputForCurrentJavaScriptPrompt(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& promptValue)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(*this, *page))
        FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    // §18.4 Send Alert Text.
    // https://w3c.github.io/webdriver/webdriver-spec.html#send-alert-text
    // 3. Run the substeps of the first matching current user prompt:
    auto scriptDialogType = m_client->typeOfCurrentJavaScriptDialogOnPage(*this, *page);
    ASSERT(scriptDialogType);
    switch (scriptDialogType.value()) {
    case API::AutomationSessionClient::JavaScriptDialogType::Alert:
    case API::AutomationSessionClient::JavaScriptDialogType::Confirm:
        // Return error with error code element not interactable.
        FAIL_WITH_PREDEFINED_ERROR(ElementNotInteractable);
    case API::AutomationSessionClient::JavaScriptDialogType::Prompt:
        // Do nothing.
        break;
    case API::AutomationSessionClient::JavaScriptDialogType::BeforeUnloadConfirm:
        // Return error with error code unsupported operation.
        FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
    }

    m_client->setUserInputForCurrentJavaScriptPromptOnPage(*this, *page, promptValue);
}

void WebAutomationSession::setFilesToSelectForFileUpload(ErrorString& errorString, const String& browsingContextHandle, const JSON::Array& filenames)
{
    Vector<String> newFileList;
    newFileList.reserveInitialCapacity(filenames.length());

    for (auto item : filenames) {
        String filename;
        if (!item->asString(filename))
            FAIL_WITH_PREDEFINED_ERROR(InternalError);

        newFileList.append(filename);
    }

    m_filesToSelectForFileUpload.swap(newFileList);
}

void WebAutomationSession::getAllCookies(ErrorString& errorString, const String& browsingContextHandle, Ref<GetAllCookiesCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    // Always send the main frame ID as 0 so it is resolved on the WebProcess side. This avoids a race when page->mainFrame() is null still.
    const uint64_t mainFrameID = 0;

    uint64_t callbackID = m_nextGetCookiesCallbackID++;
    m_getCookieCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::GetCookiesForFrame(page->pageID(), mainFrameID, callbackID), 0);
}

static Ref<Inspector::Protocol::Automation::Cookie> buildObjectForCookie(const WebCore::Cookie& cookie)
{
    return Inspector::Protocol::Automation::Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires)
        .setSize((cookie.name.length() + cookie.value.length()))
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSession(cookie.session)
        .release();
}

static Ref<Inspector::Protocol::Array<Inspector::Protocol::Automation::Cookie>> buildArrayForCookies(Vector<WebCore::Cookie>& cookiesList)
{
    auto cookies = Inspector::Protocol::Array<Inspector::Protocol::Automation::Cookie>::create();

    for (const auto& cookie : cookiesList)
        cookies->addItem(buildObjectForCookie(cookie));

    return cookies;
}

void WebAutomationSession::didGetCookiesForFrame(uint64_t callbackID, Vector<WebCore::Cookie> cookies, const String& errorType)
{
    auto callback = m_getCookieCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty()) {
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType));
        return;
    }

    callback->sendSuccess(buildArrayForCookies(cookies));
}

void WebAutomationSession::deleteSingleCookie(ErrorString& errorString, const String& browsingContextHandle, const String& cookieName, Ref<DeleteSingleCookieCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    // Always send the main frame ID as 0 so it is resolved on the WebProcess side. This avoids a race when page->mainFrame() is null still.
    const uint64_t mainFrameID = 0;

    uint64_t callbackID = m_nextDeleteCookieCallbackID++;
    m_deleteCookieCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::DeleteCookie(page->pageID(), mainFrameID, cookieName, callbackID), 0);
}

void WebAutomationSession::didDeleteCookie(uint64_t callbackID, const String& errorType)
{
    auto callback = m_deleteCookieCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty()) {
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorType));
        return;
    }

    callback->sendSuccess();
}

void WebAutomationSession::addSingleCookie(ErrorString& errorString, const String& browsingContextHandle, const JSON::Object& cookieObject, Ref<AddSingleCookieCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WebCore::URL activeURL = WebCore::URL(WebCore::URL(), page->pageLoadState().activeURL());
    ASSERT(activeURL.isValid());

    WebCore::Cookie cookie;

    if (!cookieObject.getString(WTF::ASCIILiteral("name"), cookie.name))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'name' was not found.");

    if (!cookieObject.getString(WTF::ASCIILiteral("value"), cookie.value))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'value' was not found.");

    String domain;
    if (!cookieObject.getString(WTF::ASCIILiteral("domain"), domain))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'domain' was not found.");

    // Inherit the domain/host from the main frame's URL if it is not explicitly set.
    if (domain.isEmpty())
        domain = activeURL.host();

    cookie.domain = domain;

    if (!cookieObject.getString(WTF::ASCIILiteral("path"), cookie.path))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'path' was not found.");

    double expires;
    if (!cookieObject.getDouble(WTF::ASCIILiteral("expires"), expires))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'expires' was not found.");

    cookie.expires = expires * 1000.0;

    if (!cookieObject.getBoolean(WTF::ASCIILiteral("secure"), cookie.secure))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'secure' was not found.");

    if (!cookieObject.getBoolean(WTF::ASCIILiteral("session"), cookie.session))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'session' was not found.");

    if (!cookieObject.getBoolean(WTF::ASCIILiteral("httpOnly"), cookie.httpOnly))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'httpOnly' was not found.");

    WebCookieManagerProxy* cookieManager = m_processPool->supplement<WebCookieManagerProxy>();

    // FIXME: Using activeURL here twice is basically saying "this is always in the context of the main document"
    // which probably isn't accurate.
    cookieManager->setCookies(page->websiteDataStore().sessionID(), { cookie }, activeURL, activeURL, [callback = callback.copyRef()](CallbackBase::Error error) {
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
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WebCore::URL activeURL = WebCore::URL(WebCore::URL(), page->pageLoadState().activeURL());
    ASSERT(activeURL.isValid());

    WebCookieManagerProxy* cookieManager = m_processPool->supplement<WebCookieManagerProxy>();
    cookieManager->deleteCookiesForHostname(page->websiteDataStore().sessionID(), activeURL.host());
}

#if USE(APPKIT) || PLATFORM(GTK)
static WebEvent::Modifiers protocolModifierToWebEventModifier(Inspector::Protocol::Automation::KeyModifier modifier)
{
    switch (modifier) {
    case Inspector::Protocol::Automation::KeyModifier::Alt:
        return WebEvent::AltKey;
    case Inspector::Protocol::Automation::KeyModifier::Meta:
        return WebEvent::MetaKey;
    case Inspector::Protocol::Automation::KeyModifier::Control:
        return WebEvent::ControlKey;
    case Inspector::Protocol::Automation::KeyModifier::Shift:
        return WebEvent::ShiftKey;
    case Inspector::Protocol::Automation::KeyModifier::CapsLock:
        return WebEvent::CapsLockKey;
    }

    RELEASE_ASSERT_NOT_REACHED();
}
#endif // USE(APPKIT)

void WebAutomationSession::performMouseInteraction(Inspector::ErrorString& errorString, const String& handle, const JSON::Object& requestedPositionObject, const String& mouseButtonString, const String& mouseInteractionString, const JSON::Array& keyModifierStrings, RefPtr<Inspector::Protocol::Automation::Point>& updatedPositionObject)
{
#if !USE(APPKIT) && !PLATFORM(GTK)
    FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    float x;
    if (!requestedPositionObject.getDouble(WTF::ASCIILiteral("x"), x))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'x' was not found.");

    float y;
    if (!requestedPositionObject.getDouble(WTF::ASCIILiteral("y"), y))
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "The parameter 'y' was not found.");

    WebCore::FloatRect windowFrame;
    page->getWindowFrame(windowFrame);

    x = std::min(std::max(0.0f, x), windowFrame.size().width());
    y = std::min(std::max(0.0f, y + page->topContentInset()), windowFrame.size().height());

    WebCore::IntPoint viewPosition = WebCore::IntPoint(static_cast<int>(x), static_cast<int>(y));

    auto parsedInteraction = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseInteraction>(mouseInteractionString);
    if (!parsedInteraction)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'interaction' is invalid.");

    auto parsedButton = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::MouseButton>(mouseButtonString);
    if (!parsedButton)
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'button' is invalid.");

    WebEvent::Modifiers keyModifiers = (WebEvent::Modifiers)0;
    for (auto it = keyModifierStrings.begin(); it != keyModifierStrings.end(); ++it) {
        String modifierString;
        if (!it->get()->asString(modifierString))
            FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'modifiers' is invalid.");

        auto parsedModifier = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::KeyModifier>(modifierString);
        if (!parsedModifier)
            FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "A modifier in the 'modifiers' array is invalid.");
        WebEvent::Modifiers enumValue = protocolModifierToWebEventModifier(parsedModifier.value());
        keyModifiers = (WebEvent::Modifiers)(enumValue | keyModifiers);
    }

    platformSimulateMouseInteraction(*page, viewPosition, parsedInteraction.value(), parsedButton.value(), keyModifiers);

    updatedPositionObject = Inspector::Protocol::Automation::Point::create()
        .setX(x)
        .setY(y - page->topContentInset())
        .release();
#endif // USE(APPKIT)
}

void WebAutomationSession::performKeyboardInteractions(ErrorString& errorString, const String& handle, const JSON::Array& interactions, Ref<PerformKeyboardInteractionsCallback>&& callback)
{
#if !PLATFORM(COCOA) && !PLATFORM(GTK)
    FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!interactions.length())
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'interactions' was not found or empty.");

    // Validate all of the parameters before performing any interactions with the browsing context under test.
    Vector<WTF::Function<void()>> actionsToPerform;
    actionsToPerform.reserveCapacity(interactions.length());

    for (auto interaction : interactions) {
        RefPtr<JSON::Object> interactionObject;
        if (!interaction->asObject(interactionObject))
            FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter was invalid.");

        String interactionTypeString;
        if (!interactionObject->getString(ASCIILiteral("type"), interactionTypeString))
            FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter is missing the 'type' key.");
        auto interactionType = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::KeyboardInteractionType>(interactionTypeString);
        if (!interactionType)
            FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'type' key.");

        String virtualKeyString;
        bool foundVirtualKey = interactionObject->getString(ASCIILiteral("key"), virtualKeyString);
        if (foundVirtualKey) {
            auto virtualKey = Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::VirtualKey>(virtualKeyString);
            if (!virtualKey)
                FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'key' value.");

            actionsToPerform.uncheckedAppend([this, page, interactionType, virtualKey] {
                platformSimulateKeyStroke(*page, interactionType.value(), virtualKey.value());
            });
        }

        String keySequence;
        bool foundKeySequence = interactionObject->getString(ASCIILiteral("text"), keySequence);
        if (foundKeySequence) {
            switch (interactionType.value()) {
            case Inspector::Protocol::Automation::KeyboardInteractionType::KeyPress:
            case Inspector::Protocol::Automation::KeyboardInteractionType::KeyRelease:
                // 'KeyPress' and 'KeyRelease' are meant for a virtual key and are not supported for a string (sequence of codepoints).
                FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "An interaction in the 'interactions' parameter has an invalid 'key' value.");

            case Inspector::Protocol::Automation::KeyboardInteractionType::InsertByKey:
                actionsToPerform.uncheckedAppend([this, page, keySequence] {
                    platformSimulateKeySequence(*page, keySequence);
                });
                break;
            }
        }

        if (!foundVirtualKey && !foundKeySequence)
            FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(MissingParameter, "An interaction in the 'interactions' parameter is missing both 'key' and 'text'. One must be provided.");
    }

    ASSERT(actionsToPerform.size());
    if (!actionsToPerform.size())
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InternalError, "No actions to perform.");

    auto& callbackInMap = m_pendingKeyboardEventsFlushedCallbacksPerPage.add(page->pageID(), nullptr).iterator->value;
    if (callbackInMap)
        callbackInMap->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout));
    callbackInMap = WTFMove(callback);

    for (auto& action : actionsToPerform)
        action();
#endif // PLATFORM(COCOA)
}

void WebAutomationSession::takeScreenshot(ErrorString& errorString, const String& handle, const String* optionalFrameHandle, const String* optionalNodeHandle, const bool* optionalScrollIntoViewIfNeeded, Ref<TakeScreenshotCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    std::optional<uint64_t> frameID = webFrameIDForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString());
    if (!frameID)
        FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    String nodeHandle = optionalNodeHandle ? *optionalNodeHandle : emptyString();

    uint64_t callbackID = m_nextScreenshotCallbackID++;
    m_screenshotCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::TakeScreenshot(page->pageID(), frameID.value(), nodeHandle, scrollIntoViewIfNeeded, callbackID), 0);
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

    std::optional<String> base64EncodedData = platformGetBase64EncodedPNGData(imageDataHandle);
    if (!base64EncodedData) {
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError));
        return;
    }

    callback->sendSuccess(base64EncodedData.value());
}

// Platform-dependent Implementation Stubs.

#if !PLATFORM(MAC) && !PLATFORM(GTK)
void WebAutomationSession::platformSimulateMouseInteraction(WebKit::WebPageProxy&, const WebCore::IntPoint&, Inspector::Protocol::Automation::MouseInteraction, Inspector::Protocol::Automation::MouseButton, WebEvent::Modifiers)
{
}
#endif // !PLATFORM(MAC) && !PLATFORM(GTK)

#if !PLATFORM(COCOA) && !PLATFORM(GTK)
void WebAutomationSession::platformSimulateKeyStroke(WebPageProxy&, Inspector::Protocol::Automation::KeyboardInteractionType, Inspector::Protocol::Automation::VirtualKey)
{
}

void WebAutomationSession::platformSimulateKeySequence(WebPageProxy&, const String&)
{
}
#endif // !PLATFORM(COCOA) && !PLATFORM(GTK)

#if !PLATFORM(COCOA) && !USE(CAIRO)
std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ShareableBitmap::Handle&)
{
    return std::nullopt;
}
#endif // !PLATFORM(COCOA) && !USE(CAIRO)

} // namespace WebKit
