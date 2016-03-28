/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebProcessPool.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <WebCore/URL.h>
#include <WebCore/UUID.h>
#include <wtf/HashMap.h>

using namespace Inspector;

#define FAIL_WITH_PREDEFINED_ERROR_MESSAGE(messageName) \
do { \
    auto enumValue = Inspector::Protocol::Automation::ErrorMessage::messageName; \
    errorString = Inspector::Protocol::getEnumConstantValue(enumValue); \
    return; \
} while (false)

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

    m_remoteChannel = nullptr;
    m_frontendRouter->disconnectFrontend(channel);

    setIsPaired(false);

    if (m_client)
        m_client->didDisconnectFromRemote(this);
}

#endif // ENABLE(REMOTE_INSPECTOR)

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

WebFrameProxy* WebAutomationSession::webFrameProxyForHandle(const String& handle, WebPageProxy& page)
{
    if (handle.isEmpty())
        return page.mainFrame();

    auto iter = m_handleWebFrameMap.find(handle);
    if (iter == m_handleWebFrameMap.end())
        return nullptr;

    if (WebFrameProxy* frame = page.process().webFrame(iter->value))
        return frame;

    return nullptr;
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

void WebAutomationSession::getBrowsingContexts(Inspector::ErrorString& errorString, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Automation::BrowsingContext>>& contexts)
{
    contexts = Inspector::Protocol::Array<Inspector::Protocol::Automation::BrowsingContext>::create();

    for (auto& process : m_processPool->processes()) {
        for (auto& page : process->pages()) {
            if (!page->isControlledByAutomation())
                continue;

            String handle = handleForWebPageProxy(*page);

            auto browsingContext = Inspector::Protocol::Automation::BrowsingContext::create()
                .setHandle(handleForWebPageProxy(*page))
                .setActive(m_activeBrowsingContextHandle == handle)
                .setUrl(page->pageLoadState().activeURL())
                .release();

            contexts->addItem(browsingContext.copyRef());
        }
    }
}

void WebAutomationSession::getBrowsingContext(Inspector::ErrorString& errorString, const String& handle, RefPtr<Inspector::Protocol::Automation::BrowsingContext>& context)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    context = Inspector::Protocol::Automation::BrowsingContext::create()
        .setHandle(handleForWebPageProxy(*page))
        .setActive(m_activeBrowsingContextHandle == handle)
        .setUrl(page->pageLoadState().activeURL())
        .release();
}

void WebAutomationSession::createBrowsingContext(Inspector::ErrorString& errorString, String* handle)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(InternalError);

    WebPageProxy* page = m_client->didRequestNewWindow(this);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(InternalError);

    m_activeBrowsingContextHandle = *handle = handleForWebPageProxy(*page);
}

void WebAutomationSession::closeBrowsingContext(Inspector::ErrorString& errorString, const String& handle)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    if (handle == m_activeBrowsingContextHandle)
        m_activeBrowsingContextHandle = emptyString();

    page->closePage(false);
}

void WebAutomationSession::switchToBrowsingContext(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String* optionalFrameHandle)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    WebFrameProxy* frame = webFrameProxyForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), *page);
    if (!frame)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(FrameNotFound);

    // FIXME: We don't need to track this in WK2. Remove in a follow up.
    m_activeBrowsingContextHandle = browsingContextHandle;

    page->setFocus(true);
    page->process().send(Messages::WebAutomationSessionProxy::FocusFrame(frame->frameID()), 0);
}

void WebAutomationSession::navigateBrowsingContext(Inspector::ErrorString& errorString, const String& handle, const String& url)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    page->loadRequest(WebCore::URL(WebCore::URL(), url));
}

void WebAutomationSession::goBackInBrowsingContext(Inspector::ErrorString& errorString, const String& handle)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    page->goBack();
}

void WebAutomationSession::goForwardInBrowsingContext(Inspector::ErrorString& errorString, const String& handle)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    page->goForward();
}

void WebAutomationSession::reloadBrowsingContext(Inspector::ErrorString& errorString, const String& handle)
{
    WebPageProxy* page = webPageProxyForHandle(handle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    const bool reloadFromOrigin = false;
    const bool contentBlockersEnabled = true;
    page->reload(reloadFromOrigin, contentBlockersEnabled);
}

void WebAutomationSession::evaluateJavaScriptFunction(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String* optionalFrameHandle, const String& function, const Inspector::InspectorArray& arguments, bool expectsImplicitCallbackArgument, Ref<EvaluateJavaScriptFunctionCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    WebFrameProxy* frame = webFrameProxyForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), *page);
    if (!frame)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(FrameNotFound);

    Vector<String> argumentsVector;
    argumentsVector.reserveCapacity(arguments.length());

    for (auto& argument : arguments) {
        String argumentString;
        argument->asString(argumentString);
        argumentsVector.uncheckedAppend(argumentString);
    }

    uint64_t callbackID = m_nextEvaluateJavaScriptCallbackID++;
    m_evaluateJavaScriptFunctionCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::EvaluateJavaScriptFunction(frame->frameID(), function, argumentsVector, expectsImplicitCallbackArgument, callbackID), 0);
}

void WebAutomationSession::didEvaluateJavaScriptFunction(uint64_t callbackID, const String& result, const String& errorType)
{
    auto callback = m_evaluateJavaScriptFunctionCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty()) {
        // FIXME: We should send both the errorType and result, since result has the specific exception message.
        callback->sendFailure(errorType);
    } else
        callback->sendSuccess(result);
}

void WebAutomationSession::resolveChildFrameHandle(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String* optionalFrameHandle, const int* optionalOrdinal, const String* optionalName, const String* optionalNodeHandle, Ref<ResolveChildFrameHandleCallback>&& callback)
{
    if (!optionalOrdinal && !optionalName && !optionalNodeHandle)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(MissingParameter);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    WebFrameProxy* frame = webFrameProxyForHandle(optionalFrameHandle ? *optionalFrameHandle : emptyString(), *page);
    if (!frame)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(FrameNotFound);

    uint64_t callbackID = m_nextResolveFrameCallbackID++;
    m_resolveChildFrameHandleCallbacks.set(callbackID, WTFMove(callback));

    if (optionalNodeHandle) {
        page->process().send(Messages::WebAutomationSessionProxy::ResolveChildFrameWithNodeHandle(frame->frameID(), *optionalNodeHandle, callbackID), 0);
        return;
    }

    if (optionalName) {
        page->process().send(Messages::WebAutomationSessionProxy::ResolveChildFrameWithName(frame->frameID(), *optionalName, callbackID), 0);
        return;
    }

    if (optionalOrdinal) {
        page->process().send(Messages::WebAutomationSessionProxy::ResolveChildFrameWithOrdinal(frame->frameID(), *optionalOrdinal, callbackID), 0);
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
        callback->sendFailure(errorType);
    else
        callback->sendSuccess(handleForWebFrameID(frameID));
}

void WebAutomationSession::resolveParentFrameHandle(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& frameHandle, Ref<ResolveParentFrameHandleCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    WebFrameProxy* frame = webFrameProxyForHandle(frameHandle, *page);
    if (!frame)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(FrameNotFound);

    uint64_t callbackID = m_nextResolveParentFrameCallbackID++;
    m_resolveParentFrameHandleCallbacks.set(callbackID, WTFMove(callback));

    page->process().send(Messages::WebAutomationSessionProxy::ResolveParentFrame(frame->frameID(), callbackID), 0);
}

void WebAutomationSession::didResolveParentFrame(uint64_t callbackID, uint64_t frameID, const String& errorType)
{
    auto callback = m_resolveParentFrameHandleCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty())
        callback->sendFailure(errorType);
    else
        callback->sendSuccess(handleForWebFrameID(frameID));
}

void WebAutomationSession::computeElementLayout(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& frameHandle, const String& nodeHandle, const bool* optionalScrollIntoViewIfNeeded, const bool* optionalUseViewportCoordinates, Ref<ComputeElementLayoutCallback>&& callback)
{
    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    WebFrameProxy* frame = webFrameProxyForHandle(frameHandle, *page);
    if (!frame)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(FrameNotFound);

    uint64_t callbackID = m_nextComputeElementLayoutCallbackID++;
    m_computeElementLayoutCallbacks.set(callbackID, WTFMove(callback));

    bool scrollIntoViewIfNeeded = optionalScrollIntoViewIfNeeded ? *optionalScrollIntoViewIfNeeded : false;
    bool useViewportCoordinates = optionalUseViewportCoordinates ? *optionalUseViewportCoordinates : false;

    page->process().send(Messages::WebAutomationSessionProxy::ComputeElementLayout(frame->frameID(), nodeHandle, scrollIntoViewIfNeeded, useViewportCoordinates, callbackID), 0);
}

void WebAutomationSession::didComputeElementLayout(uint64_t callbackID, WebCore::IntRect rect, const String& errorType)
{
    auto callback = m_computeElementLayoutCallbacks.take(callbackID);
    if (!callback)
        return;

    if (!errorType.isEmpty()) {
        callback->sendFailure(errorType);
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
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    *result = m_client->isShowingJavaScriptDialogOnPage(this, page);
}

void WebAutomationSession::dismissCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(this, page))
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(NoJavaScriptDialog);

    m_client->dismissCurrentJavaScriptDialogOnPage(this, page);
}

void WebAutomationSession::acceptCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(this, page))
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(NoJavaScriptDialog);

    m_client->acceptCurrentJavaScriptDialogOnPage(this, page);
}

void WebAutomationSession::messageOfCurrentJavaScriptDialog(Inspector::ErrorString& errorString, const String& browsingContextHandle, String* text)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(this, page))
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(NoJavaScriptDialog);

    *text = m_client->messageOfCurrentJavaScriptDialogOnPage(this, page);
}

void WebAutomationSession::setUserInputForCurrentJavaScriptPrompt(Inspector::ErrorString& errorString, const String& browsingContextHandle, const String& promptValue)
{
    ASSERT(m_client);
    if (!m_client)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(InternalError);

    WebPageProxy* page = webPageProxyForHandle(browsingContextHandle);
    if (!page)
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(WindowNotFound);

    if (!m_client->isShowingJavaScriptDialogOnPage(this, page))
        FAIL_WITH_PREDEFINED_ERROR_MESSAGE(NoJavaScriptDialog);

    m_client->setUserInputForCurrentJavaScriptPromptOnPage(this, page, promptValue);
}

} // namespace WebKit
