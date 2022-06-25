/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebURLSchemeTask.h"

#include "APIFrameInfo.h"
#include "URLSchemeTaskParameters.h"
#include "WebErrors.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebURLSchemeHandler.h"

namespace WebKit {
using namespace WebCore;

Ref<WebURLSchemeTask> WebURLSchemeTask::create(WebURLSchemeHandler& handler, WebPageProxy& page, WebProcessProxy& process, PageIdentifier webPageID, URLSchemeTaskParameters&& parameters, SyncLoadCompletionHandler&& syncCompletionHandler)
{
    return adoptRef(*new WebURLSchemeTask(handler, page, process, webPageID, WTFMove(parameters), WTFMove(syncCompletionHandler)));
}

WebURLSchemeTask::WebURLSchemeTask(WebURLSchemeHandler& handler, WebPageProxy& page, WebProcessProxy& process, PageIdentifier webPageID, URLSchemeTaskParameters&& parameters, SyncLoadCompletionHandler&& syncCompletionHandler)
    : m_urlSchemeHandler(handler)
    , m_process(&process)
    , m_resourceLoaderID(parameters.taskIdentifier)
    , m_pageProxyID(page.identifier())
    , m_webPageID(webPageID)
    , m_request(WTFMove(parameters.request))
    , m_frameInfo(API::FrameInfo::create(WTFMove(parameters.frameInfo), &page))
    , m_syncCompletionHandler(WTFMove(syncCompletionHandler))
{
    ASSERT(RunLoop::isMain());
}

WebURLSchemeTask::~WebURLSchemeTask()
{
    ASSERT(RunLoop::isMain());
}

ResourceRequest WebURLSchemeTask::request() const
{
    ASSERT(RunLoop::isMain());
    Locker locker { m_requestLock };
    return m_request;
}

auto WebURLSchemeTask::willPerformRedirection(ResourceResponse&& response, ResourceRequest&& request,  Function<void(ResourceRequest&&)>&& completionHandler) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return m_shouldSuppressTaskStoppedExceptions ? ExceptionType::None : ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;

    if (m_dataSent)
        return ExceptionType::DataAlreadySent;

    if (m_responseSent)
        return ExceptionType::RedirectAfterResponse;

    if (m_waitingForRedirectCompletionHandlerCallback)
        return ExceptionType::WaitingForRedirectCompletionHandler;

    if (isSync())
        m_syncResponse = response;

    {
        Locker locker { m_requestLock };
        m_request = request;
    }

    auto* page = WebProcessProxy::webPage(m_pageProxyID);
    if (!page)
        return ExceptionType::None;

    m_waitingForRedirectCompletionHandlerCallback = true;

    Function<void(ResourceRequest&&)> innerCompletionHandler = [protectedThis = Ref { *this }, this, completionHandler = WTFMove(completionHandler)] (ResourceRequest&& request) mutable {
        m_waitingForRedirectCompletionHandlerCallback = false;
        if (!m_stopped && !m_completed)
            completionHandler(WTFMove(request));
    };

    page->sendWithAsyncReply(Messages::WebPage::URLSchemeTaskWillPerformRedirection(m_urlSchemeHandler->identifier(), m_resourceLoaderID, response, request), WTFMove(innerCompletionHandler));

    return ExceptionType::None;
}

auto WebURLSchemeTask::didPerformRedirection(WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return m_shouldSuppressTaskStoppedExceptions ? ExceptionType::None : ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;

    if (m_dataSent)
        return ExceptionType::DataAlreadySent;

    if (m_responseSent)
        return ExceptionType::RedirectAfterResponse;

    if (m_waitingForRedirectCompletionHandlerCallback)
        return ExceptionType::WaitingForRedirectCompletionHandler;

    if (isSync())
        m_syncResponse = response;

    {
        Locker locker { m_requestLock };
        m_request = request;
    }

    m_process->send(Messages::WebPage::URLSchemeTaskDidPerformRedirection(m_urlSchemeHandler->identifier(), m_resourceLoaderID, response, request), m_webPageID);

    return ExceptionType::None;
}

auto WebURLSchemeTask::didReceiveResponse(const ResourceResponse& response) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return m_shouldSuppressTaskStoppedExceptions ? ExceptionType::None : ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;

    if (m_dataSent)
        return ExceptionType::DataAlreadySent;

    if (m_waitingForRedirectCompletionHandlerCallback)
        return ExceptionType::WaitingForRedirectCompletionHandler;

    m_responseSent = true;

    if (isSync())
        m_syncResponse = response;

    m_process->send(Messages::WebPage::URLSchemeTaskDidReceiveResponse(m_urlSchemeHandler->identifier(), m_resourceLoaderID, response), m_webPageID);
    return ExceptionType::None;
}

auto WebURLSchemeTask::didReceiveData(const SharedBuffer& buffer) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return m_shouldSuppressTaskStoppedExceptions ? ExceptionType::None : ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;

    if (!m_responseSent)
        return ExceptionType::NoResponseSent;

    if (m_waitingForRedirectCompletionHandlerCallback)
        return ExceptionType::WaitingForRedirectCompletionHandler;

    m_dataSent = true;

    if (isSync()) {
        m_syncData.append(buffer);
        return ExceptionType::None;
    }

    m_process->send(Messages::WebPage::URLSchemeTaskDidReceiveData(m_urlSchemeHandler->identifier(), m_resourceLoaderID, IPC::SharedBufferReference(buffer)), m_webPageID);
    return ExceptionType::None;
}

auto WebURLSchemeTask::didComplete(const ResourceError& error) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return m_shouldSuppressTaskStoppedExceptions ? ExceptionType::None : ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;

    if (!m_responseSent && error.isNull())
        return ExceptionType::NoResponseSent;

    if (m_waitingForRedirectCompletionHandlerCallback && error.isNull())
        return ExceptionType::WaitingForRedirectCompletionHandler;

    m_completed = true;

    if (isSync()) {
        size_t size = m_syncData.size();
        Vector<uint8_t> data = { m_syncData.takeAsContiguous()->data(), size };
        m_syncCompletionHandler(m_syncResponse, error, WTFMove(data));
    }

    m_process->send(Messages::WebPage::URLSchemeTaskDidComplete(m_urlSchemeHandler->identifier(), m_resourceLoaderID, error), m_webPageID);
    m_urlSchemeHandler->taskCompleted(pageProxyID(), *this);

    return ExceptionType::None;
}

void WebURLSchemeTask::pageDestroyed()
{
    ASSERT(RunLoop::isMain());

    m_pageProxyID = { };
    m_webPageID = { };
    m_process = nullptr;
    m_stopped = true;
    
    if (isSync()) {
        Locker locker { m_requestLock };
        m_syncCompletionHandler({ }, failedCustomProtocolSyncLoad(m_request), { });
    }
}

void WebURLSchemeTask::stop()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_stopped);

    m_stopped = true;

    if (isSync()) {
        Locker locker { m_requestLock };
        m_syncCompletionHandler({ }, failedCustomProtocolSyncLoad(m_request), { });
    }
}

#if PLATFORM(COCOA)
NSURLRequest *WebURLSchemeTask::nsRequest() const
{
    Locker locker { m_requestLock };
    return m_request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
}
#endif

} // namespace WebKit
