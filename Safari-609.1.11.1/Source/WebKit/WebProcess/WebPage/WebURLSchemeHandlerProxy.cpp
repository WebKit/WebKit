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
#include "WebURLSchemeHandlerProxy.h"

#include "DataReference.h"
#include "URLSchemeTaskParameters.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebLoaderStrategy.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

namespace WebKit {
using namespace WebCore;

WebURLSchemeHandlerProxy::WebURLSchemeHandlerProxy(WebPage& page, uint64_t identifier)
    : m_webPage(page)
    , m_identifier(identifier)
{
}

WebURLSchemeHandlerProxy::~WebURLSchemeHandlerProxy()
{
    ASSERT(m_tasks.isEmpty());
}

void WebURLSchemeHandlerProxy::startNewTask(ResourceLoader& loader)
{
    auto result = m_tasks.add(loader.identifier(), WebURLSchemeTaskProxy::create(*this, loader));
    ASSERT(result.isNewEntry);

    WebProcess::singleton().webLoaderStrategy().addURLSchemeTaskProxy(*result.iterator->value);
    result.iterator->value->startLoading();
}

void WebURLSchemeHandlerProxy::loadSynchronously(ResourceLoadIdentifier loadIdentifier, const ResourceRequest& request, ResourceResponse& response, ResourceError& error, Vector<char>& data)
{
    data.shrink(0);
    if (!m_webPage.sendSync(Messages::WebPageProxy::LoadSynchronousURLSchemeTask(URLSchemeTaskParameters { m_identifier, loadIdentifier, request }), Messages::WebPageProxy::LoadSynchronousURLSchemeTask::Reply(response, error, data))) {
        error = failedCustomProtocolSyncLoad(request);
        return;
    }
}

void WebURLSchemeHandlerProxy::stopAllTasks()
{
    while (!m_tasks.isEmpty())
        m_tasks.begin()->value->stopLoading();
}

void WebURLSchemeHandlerProxy::taskDidPerformRedirection(uint64_t taskIdentifier, WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& newRequest)
{
    auto* task = m_tasks.get(taskIdentifier);
    if (!task)
        return;
    
    task->didPerformRedirection(WTFMove(redirectResponse), WTFMove(newRequest));
}

void WebURLSchemeHandlerProxy::taskDidReceiveResponse(uint64_t taskIdentifier, const ResourceResponse& response)
{
    auto* task = m_tasks.get(taskIdentifier);
    if (!task)
        return;

    task->didReceiveResponse(response);
}

void WebURLSchemeHandlerProxy::taskDidReceiveData(uint64_t taskIdentifier, size_t size, const uint8_t* data)
{
    auto* task = m_tasks.get(taskIdentifier);
    if (!task)
        return;

    task->didReceiveData(size, data);
}

void WebURLSchemeHandlerProxy::taskDidComplete(uint64_t taskIdentifier, const ResourceError& error)
{
    if (auto task = removeTask(taskIdentifier))
        task->didComplete(error);
}

void WebURLSchemeHandlerProxy::taskDidStopLoading(WebURLSchemeTaskProxy& task)
{
    ASSERT(m_tasks.get(task.identifier()) == &task);
    removeTask(task.identifier());
}

RefPtr<WebURLSchemeTaskProxy> WebURLSchemeHandlerProxy::removeTask(uint64_t identifier)
{
    auto task = m_tasks.take(identifier);
    if (!task)
        return nullptr;

    WebProcess::singleton().webLoaderStrategy().removeURLSchemeTaskProxy(*task);

    return task;
}

} // namespace WebKit
