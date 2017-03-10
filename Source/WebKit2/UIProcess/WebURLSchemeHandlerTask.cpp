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
#include "WebURLSchemeHandlerTask.h"

#include "DataReference.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebURLSchemeHandler.h"

using namespace WebCore;

namespace WebKit {

Ref<WebURLSchemeHandlerTask> WebURLSchemeHandlerTask::create(WebURLSchemeHandler& handler, WebPageProxy& page, uint64_t resourceIdentifier, const ResourceRequest& request)
{
    return adoptRef(*new WebURLSchemeHandlerTask(handler, page, resourceIdentifier, request));
}

WebURLSchemeHandlerTask::WebURLSchemeHandlerTask(WebURLSchemeHandler& handler, WebPageProxy& page, uint64_t resourceIdentifier, const ResourceRequest& request)
    : m_urlSchemeHandler(handler)
    , m_page(&page)
    , m_identifier(resourceIdentifier)
    , m_request(request)
{
}

WebURLSchemeHandlerTask::ExceptionType WebURLSchemeHandlerTask::didReceiveResponse(const ResourceResponse& response)
{
    if (m_stopped)
        return WebURLSchemeHandlerTask::ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return WebURLSchemeHandlerTask::ExceptionType::CompleteAlreadyCalled;

    if (m_dataSent)
        return WebURLSchemeHandlerTask::ExceptionType::DataAlreadySent;

    m_responseSent = true;
    m_page->send(Messages::WebPage::URLSchemeHandlerTaskDidReceiveResponse(m_urlSchemeHandler->identifier(), m_identifier, response));
    return WebURLSchemeHandlerTask::ExceptionType::None;
}

WebURLSchemeHandlerTask::ExceptionType WebURLSchemeHandlerTask::didReceiveData(Ref<SharedBuffer> buffer)
{
    if (m_stopped)
        return WebURLSchemeHandlerTask::ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return WebURLSchemeHandlerTask::ExceptionType::CompleteAlreadyCalled;

    if (!m_responseSent)
        return WebURLSchemeHandlerTask::ExceptionType::NoResponseSent;

    m_dataSent = true;
    m_page->send(Messages::WebPage::URLSchemeHandlerTaskDidReceiveData(m_urlSchemeHandler->identifier(), m_identifier, IPC::SharedBufferDataReference(buffer.ptr())));
    return WebURLSchemeHandlerTask::ExceptionType::None;
}

WebURLSchemeHandlerTask::ExceptionType WebURLSchemeHandlerTask::didComplete(const ResourceError& error)
{
    if (m_stopped)
        return WebURLSchemeHandlerTask::ExceptionType::TaskAlreadyStopped;

    if (m_completed)
        return WebURLSchemeHandlerTask::ExceptionType::CompleteAlreadyCalled;

    if (!m_responseSent && error.isNull())
        return WebURLSchemeHandlerTask::ExceptionType::NoResponseSent;

    m_completed = true;
    m_page->send(Messages::WebPage::URLSchemeHandlerTaskDidComplete(m_urlSchemeHandler->identifier(), m_identifier, error));
    return WebURLSchemeHandlerTask::ExceptionType::None;
}

void WebURLSchemeHandlerTask::pageDestroyed()
{
    ASSERT(m_page);
    m_page = nullptr;
    m_stopped = true;
}

void WebURLSchemeHandlerTask::stop()
{
    ASSERT(!m_stopped);
    m_stopped = true;
}

} // namespace WebKit
