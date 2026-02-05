/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "MessageSenderInlines.h"
#include "URLSchemeTaskParameters.h"
#include "WebErrors.h"
#include "WebFrame.h"
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

WebURLSchemeHandlerProxy::WebURLSchemeHandlerProxy(WebPage& page, WebURLSchemeHandlerIdentifier identifier)
    : m_webPage(page)
    , m_identifier(identifier)
{
}

WebURLSchemeHandlerProxy::~WebURLSchemeHandlerProxy()
{
    ASSERT(m_tasks.isEmpty());
}

void WebURLSchemeHandlerProxy::startNewTask(ResourceLoader& loader, WebFrame& webFrame)
{
    Ref task = WebURLSchemeTaskProxy::create(*this, loader, webFrame);
    auto result = m_tasks.add(*loader.identifier(), task);
    ASSERT_UNUSED(result, result.isNewEntry);

    protect(WebProcess::singleton().webLoaderStrategy())->addURLSchemeTaskProxy(task);
    task->startLoading();
}

void WebURLSchemeHandlerProxy::loadSynchronously(WebCore::ResourceLoaderIdentifier loadIdentifier, WebFrame& webFrame, const ResourceRequest& request, ResourceResponse& response, ResourceError& error, Vector<uint8_t>& data)
{
    data.shrink(0);
    auto sendResult = protect(page())->sendSync(Messages::WebPageProxy::LoadSynchronousURLSchemeTask(URLSchemeTaskParameters { m_identifier, loadIdentifier, request, webFrame.info() }));
    if (sendResult.succeeded())
        std::tie(response, error, data) = sendResult.takeReply();
    else
        error = failedCustomProtocolSyncLoad(request);
}

void WebURLSchemeHandlerProxy::stopAllTasks()
{
    while (!m_tasks.isEmpty())
        Ref { m_tasks.begin()->value }->stopLoading();
}

void WebURLSchemeHandlerProxy::taskDidPerformRedirection(WebCore::ResourceLoaderIdentifier taskIdentifier, WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& newRequest, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    if (RefPtr task = m_tasks.get(taskIdentifier))
        task->didPerformRedirection(WTF::move(redirectResponse), WTF::move(newRequest), WTF::move(completionHandler));
}

void WebURLSchemeHandlerProxy::taskDidReceiveResponse(WebCore::ResourceLoaderIdentifier taskIdentifier, ResourceResponse&& response)
{
    if (RefPtr task = m_tasks.get(taskIdentifier))
        task->didReceiveResponse(WTF::move(response));
}

void WebURLSchemeHandlerProxy::taskDidReceiveData(WebCore::ResourceLoaderIdentifier taskIdentifier, Ref<WebCore::SharedBuffer>&& data)
{
    if (RefPtr task = m_tasks.get(taskIdentifier))
        task->didReceiveData(WTF::move(data));
}

void WebURLSchemeHandlerProxy::taskDidComplete(WebCore::ResourceLoaderIdentifier taskIdentifier, const ResourceError& error)
{
    if (RefPtr task = removeTask(taskIdentifier))
        task->didComplete(error);
}

void WebURLSchemeHandlerProxy::taskDidStopLoading(WebURLSchemeTaskProxy& task)
{
    ASSERT(m_tasks.get(task.identifier()) == &task);
    removeTask(task.identifier());
}

RefPtr<WebURLSchemeTaskProxy> WebURLSchemeHandlerProxy::removeTask(WebCore::ResourceLoaderIdentifier identifier)
{
    RefPtr task = m_tasks.take(identifier);
    if (!task)
        return nullptr;

    protect(WebProcess::singleton().webLoaderStrategy())->removeURLSchemeTaskProxy(*task);

    return task;
}

} // namespace WebKit
