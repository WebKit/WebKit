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

#include "APIAutomationSessionClient.h"
#include "AutomationProtocolObjects.h"
#include "WebAutomationSessionMacros.h"
#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebCookieManagerProxy.h"
#include "WebInspectorProxy.h"
#include "WebProcessPool.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <WebCore/URL.h>
#include <WebCore/UUID.h>
#include <algorithm>
#include <wtf/HashMap.h>
#include <wtf/Optional.h>
#include <wtf/text/StringConcatenate.h>

using namespace Inspector;

namespace WebKit {

WebAutomationSession::WebAutomationSession()
    : m_client(std::make_unique<API::AutomationSessionClient>())
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_domainDispatcher(AutomationBackendDispatcher::create(m_backendDispatcher, this))
{
}

WebAutomationSession::~WebAutomationSession()
{
    ASSERT(!m_client);

    if (m_processPool)
        m_processPool->removeMessageReceiver(Messages::WebAutomationSession::messageReceiverName());
}

void WebAutomationSession::setClient(std::unique_ptr<API::AutomationSessionClient> client)
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

void WebAutomationSession::connect(Inspector::FrontendChannel* channel, bool isAutomaticConnection)
{
    UNUSED_PARAM(isAutomaticConnection);

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
        m_client->didDisconnectFromRemote(this);
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

    String handle = "page-" + WebCore::createCanonicalUUIDString().convertToASCIIUppercase();

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

    String handle = "frame-" + WebCore::createCanonicalUUIDString().convertToASCIIUppercase();

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

    WebPageProxy* page = m_client->didRequestNewWindow(this);
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

void WebAutomationSession::navigateBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const String& url, Ref<NavigateBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (auto callback = m_pendingNavigationInBrowsingContextCallbacksPerPage.take(page->pageID()))
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout));
    m_pendingNavigationInBrowsingContextCallbacksPerPage.set(page->pageID(), WTFMove(callback));

    page->loadRequest(WebCore::URL(WebCore::URL(), url));
}

void WebAutomationSession::goBackInBrowsingContext(Inspector::ErrorString& errorString, const String& handle, Ref<GoBackInBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (auto callback = m_pendingNavigationInBrowsingContextCallbacksPerPage.take(page->pageID()))
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout));
    m_pendingNavigationInBrowsingContextCallbacksPerPage.set(page->pageID(), WTFMove(callback));

    page->goBack();
}

void WebAutomationSession::goForwardInBrowsingContext(Inspector::ErrorString& errorString, const String& handle, Ref<GoForwardInBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (auto callback = m_pendingNavigationInBrowsingContextCallbacksPerPage.take(page->pageID()))
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout));
    m_pendingNavigationInBrowsingContextCallbacksPerPage.set(page->pageID(), WTFMove(callback));

    page->goForward();
}

void WebAutomationSession::reloadBrowsingContext(Inspector::ErrorString& errorString, const String& handle, Ref<ReloadBrowsingContextCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (auto callback = m_pendingNavigationInBrowsingContextCallbacksPerPage.take(page->pageID()))
        callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(Timeout));
    m_pendingNavigationInBrowsingContextCallbacksPerPage.set(page->pageID(), WTFMove(callback));

    const bool reloadFromOrigin = false;
    page->reload(reloadFromOrigin, { });
}

void WebAutomationSession::navigationOccurredForPage(const WebPageProxy& page)
{
    if (auto callback = m_pendingNavigationInBrowsingContextCallbacksPerPage.take(page.pageID()))
        callback->sendSuccess(InspectorObject::create());
}

void WebAutomationSession::inspectorFrontendLoaded(const WebPageProxy& page)
{
    if (auto callback = m_pendingInspectorCallbacksPerPage.take(page.pageID()))
        callback->sendSuccess(InspectorObject::create());
}

void WebAutomationSession::keyboardEventsFlushedForPage(const WebPageProxy& page)
{
    if (auto callback = m_pendingKeyboardEventsFlushedCallbacksPerPage.take(page.pageID()))
        callback->sendSuccess(InspectorObject::create());
}

void WebAutomationSession::evaluateJavaScriptFunction(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String* optionalFrameHandle, const String& function, const Inspector::InspectorArray& arguments, const bool* optionalExpectsImplicitCallbackArgument, const int* optionalCallbackTimeout, Ref<EvaluateJavaScriptFunctionCallback>&& callback)
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

void WebAutomationSession::computeElementLayout(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& frameHandle, const String& nodeHandle, const bool* optionalScrollIntoViewIfNeeded, const bool* optionalUseViewportCoordinates, Ref<ComputeElementLayoutCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    std::optional<uint64_t> frameID = webFrameIDForHandle(frameHandle);
    if (!frameID)
        FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

    uint64_t callbackID = m_nextComputeElementLayoutCallbackID++;
    m_computeElementLayoutCallbacks.set(callbackID, WTFMove(callback));

    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    bool useViewportCoordinates = optionalUseViewportCoordinates ? *optionalUseViewportCoordinates : false;

    page->process().send(Messages::WebAutomationSessionProxy::ComputeElementLayout(page->pageID(), frameID.value(), nodeHandle, scrollIntoViewIfNeeded, useViewportCoordinates, callbackID), 0);
}

void WebAutomationSession::didComputeElementLayout(uint64_t callbackID, WebCore::IntRect rect, const String& errorType)
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

    callback->sendSuccess(WTFMove(rectObject));
}

void WebAutomationSession::isShowingJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle, bool* result)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    *result = m_client->isShowingJavaScriptDialogOnPage(this, page);
}

void WebAutomationSession::dismissCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(this, page))
        FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    m_client->dismissCurrentJavaScriptDialogOnPage(this, page);
}

void WebAutomationSession::acceptCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(this, page))
        FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    m_client->acceptCurrentJavaScriptDialogOnPage(this, page);
}

void WebAutomationSession::messageOfCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle, String* text)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(this, page))
        FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    *text = m_client->messageOfCurrentJavaScriptDialogOnPage(this, page);
}

void WebAutomationSession::setUserInputForCurrentJavaScriptPrompt(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& promptValue)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(this, page))
        FAIL_WITH_PREDEFINED_ERROR(NoJavaScriptDialog);

    m_client->setUserInputForCurrentJavaScriptPromptOnPage(this, page, promptValue);
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

void WebAutomationSession::addSingleCookie(ErrorString& errorString, const String& browsingContextHandle, const Inspector::InspectorObject& cookieObject, Ref<AddSingleCookieCallback>&& callback)
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
    cookieManager->addCookie(WebCore::SessionID::defaultSessionID(), cookie, activeURL.host());

    callback->sendSuccess();
}

void WebAutomationSession::deleteAllCookies(ErrorString& errorString, const String& browsingContextHandle)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    WebCore::URL activeURL = WebCore::URL(WebCore::URL(), page->pageLoadState().activeURL());
    ASSERT(activeURL.isValid());

    WebCookieManagerProxy* cookieManager = m_processPool->supplement<WebCookieManagerProxy>();
    cookieManager->deleteCookiesForHostname(WebCore::SessionID::defaultSessionID(), activeURL.host());
}

#if USE(APPKIT)
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
}
#endif // USE(APPKIT)

void WebAutomationSession::performMouseInteraction(Inspector::ErrorString& errorString, const String& handle, const Inspector::InspectorObject& requestedPositionObject, const String& mouseButtonString, const String& mouseInteractionString, const Inspector::InspectorArray& keyModifierStrings, RefPtr<Inspector::Protocol::Automation::Point>& updatedPositionObject)
{
#if !USE(APPKIT)
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

void WebAutomationSession::performKeyboardInteractions(ErrorString& errorString, const String& handle, const Inspector::InspectorArray& interactions, Ref<PerformKeyboardInteractionsCallback>&& callback)
{
#if !USE(APPKIT)
    FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
#else
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    if (!interactions.length())
        FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, "The parameter 'interactions' was not found or empty.");

    // Validate all of the parameters before performing any interactions with the browsing context under test.
    Vector<std::function<void()>> actionsToPerform;
    actionsToPerform.reserveCapacity(interactions.length());

    for (auto interaction : interactions) {
        RefPtr<InspectorObject> interactionObject;
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
#endif // USE(APPKIT)
}

void WebAutomationSession::takeScreenshot(ErrorString& errorString, const String& handle, Ref<TakeScreenshotCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR(WindowNotFound);

    uint64_t callbackID = m_nextScreenshotCallbackID++;
    m_screenshotCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::TakeScreenshot(page->pageID(), callbackID), 0);
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

#if !PLATFORM(MAC)
void WebAutomationSession::platformSimulateMouseInteraction(WebKit::WebPageProxy&, const WebCore::IntPoint&, Inspector::Protocol::Automation::MouseInteraction, Inspector::Protocol::Automation::MouseButton, WebEvent::Modifiers)
{
}

void WebAutomationSession::platformSimulateKeyStroke(WebPageProxy&, Inspector::Protocol::Automation::KeyboardInteractionType, Inspector::Protocol::Automation::VirtualKey)
{
}

void WebAutomationSession::platformSimulateKeySequence(WebPageProxy&, const String&)
{
}
#endif // !PLATFORM(MAC)

#if !PLATFORM(COCOA)
std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ShareableBitmap::Handle&)
{
    return String();
}
#endif // !PLATFORM(COCOA)

} // namespace WebKit
