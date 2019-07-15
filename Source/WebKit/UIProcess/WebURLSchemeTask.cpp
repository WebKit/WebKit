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

#include "DataReference.h"
#include "SharedBufferDataReference.h"
#include "WebErrors.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebURLSchemeHandler.h"

namespace WebKit {
using namespace WebCore;

Ref<WebURLSchemeTask> WebURLSchemeTask::create(WebURLSchemeHandler& handler, WebPageProxy& page, WebProcessProxy& process, uint64_t resourceIdentifier, ResourceRequest&& request, SyncLoadCompletionHandler&& syncCompletionHandler)
{
    return adoptRef(*new WebURLSchemeTask(handler, page, process, resourceIdentifier, WTFMove(request), WTFMove(syncCompletionHandler)));
}

WebURLSchemeTask::WebURLSchemeTask(WebURLSchemeHandler& handler, WebPageProxy& page, WebProcessProxy& process, uint64_t resourceIdentifier, ResourceRequest&& request, SyncLoadCompletionHandler&& syncCompletionHandler)
    : m_urlSchemeHandler(handler)
    , m_page(&page)
    , m_process(makeRef(process))
    , m_identifier(resourceIdentifier)
    , m_pageIdentifier(page.pageID())
    , m_request(WTFMove(request))
    , m_syncCompletionHandler(WTFMove(syncCompletionHandler))
{
    ASSERT(RunLoop::isMain());
}

WebURLSchemeTask::~WebURLSchemeTask()
{
    ASSERT(RunLoop::isMain());
}

auto WebURLSchemeTask::didPerformRedirection(WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return ExceptionType::TaskAlreadyStopped;
    
    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;
    
    if (m_dataSent)
        return ExceptionType::DataAlreadySent;
    
    if (m_responseSent)
        return ExceptionType::RedirectAfterResponse;
    
    if (isSync())
        m_syncResponse = response;

    {
        LockHolder locker(m_requestLock);
        m_request = request;
    }

    m_process->send(Messages::WebPage::URLSchemeTaskDidPerformRedirection(m_urlSchemeHandler->identifier(), m_identifier, response, request), m_page->pageID());

    return ExceptionType::None;
}

auto WebURLSchemeTask::didReceiveResponse(const ResourceResponse& response) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;

    if (m_dataSent)
        return ExceptionType::DataAlreadySent;

    m_responseSent = true;

    response.includeCertificateInfo();

    if (isSync())
        m_syncResponse = response;

    m_process->send(Messages::WebPage::URLSchemeTaskDidReceiveResponse(m_urlSchemeHandler->identifier(), m_identifier, response), m_page->pageID());
    return ExceptionType::None;
}

auto WebURLSchemeTask::didReceiveData(Ref<SharedBuffer>&& buffer) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;

    if (!m_responseSent)
        return ExceptionType::NoResponseSent;

    m_dataSent = true;

    if (isSync()) {
        if (m_syncData)
            m_syncData->append(WTFMove(buffer));
        else
            m_syncData = WTFMove(buffer);
        return ExceptionType::None;
    }

    m_process->send(Messages::WebPage::URLSchemeTaskDidReceiveData(m_urlSchemeHandler->identifier(), m_identifier, { buffer }), m_page->pageID());
    return ExceptionType::None;
}

auto WebURLSchemeTask::didComplete(const ResourceError& error) -> ExceptionType
{
    ASSERT(RunLoop::isMain());

    if (m_stopped)
        return ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return ExceptionType::CompleteAlreadyCalled;

    if (!m_responseSent && error.isNull())
        return ExceptionType::NoResponseSent;

    m_completed = true;
    
    if (isSync()) {
        Vector<char> data;
        if (m_syncData) {
            data.resize(m_syncData->size());
            memcpy(data.data(), reinterpret_cast<const char*>(m_syncData->data()), m_syncData->size());
        }

        m_syncCompletionHandler(m_syncResponse, error, WTFMove(data));
        m_syncData = nullptr;
    }

    m_process->send(Messages::WebPage::URLSchemeTaskDidComplete(m_urlSchemeHandler->identifier(), m_identifier, error), m_page->pageID());
    m_urlSchemeHandler->taskCompleted(*this);

    return ExceptionType::None;
}

void WebURLSchemeTask::pageDestroyed()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_page);

    m_page = nullptr;
    m_process = nullptr;
    m_stopped = true;
    
    if (isSync()) {
        LockHolder locker(m_requestLock);
        m_syncCompletionHandler({ }, failedCustomProtocolSyncLoad(m_request), { });
    }
}

void WebURLSchemeTask::stop()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_stopped);

    m_stopped = true;

    if (isSync()) {
        LockHolder locker(m_requestLock);
        m_syncCompletionHandler({ }, failedCustomProtocolSyncLoad(m_request), { });
    }
}

#if PLATFORM(COCOA)
NSURLRequest *WebURLSchemeTask::nsRequest() const
{
    LockHolder locker(m_requestLock);
    return m_request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
}
#endif

} // namespace WebKit
