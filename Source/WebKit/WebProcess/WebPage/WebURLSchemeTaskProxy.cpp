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
#include "WebURLSchemeTaskProxy.h"

#include "Logging.h"
#include "URLSchemeTaskParameters.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebURLSchemeHandlerProxy.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <wtf/CompletionHandler.h>

namespace WebKit {
using namespace WebCore;

WebURLSchemeTaskProxy::WebURLSchemeTaskProxy(WebURLSchemeHandlerProxy& handler, ResourceLoader& loader, WebFrame& frame)
    : m_urlSchemeHandler(handler)
    , m_coreLoader(&loader)
    , m_frame(&frame)
    , m_request(loader.request())
    , m_identifier(loader.identifier())
{
}

void WebURLSchemeTaskProxy::startLoading()
{
    ASSERT(m_coreLoader);
    ASSERT(m_frame);
    m_urlSchemeHandler.page().send(Messages::WebPageProxy::StartURLSchemeTask(URLSchemeTaskParameters { m_urlSchemeHandler.identifier(), m_coreLoader->identifier(), m_request, m_frame->info() }));
}

void WebURLSchemeTaskProxy::stopLoading()
{
    ASSERT(m_coreLoader);
    m_urlSchemeHandler.page().send(Messages::WebPageProxy::StopURLSchemeTask(m_urlSchemeHandler.identifier(), m_coreLoader->identifier()));
    m_coreLoader = nullptr;
    m_frame = nullptr;

    // This line will result in this being deleted.
    m_urlSchemeHandler.taskDidStopLoading(*this);
}
    
void WebURLSchemeTaskProxy::didPerformRedirection(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& request)
{
    if (!hasLoader())
        return;
    
    auto completionHandler = [this, protectedThis = makeRef(*this), originalRequest = request] (ResourceRequest&& request) {
        m_waitingForCompletionHandler = false;
        // We do not inform the UIProcess of WebKit's new request with the given suggested request.
        // We do want to know if WebKit would have generated a request that differs from the suggested request, though.
        if (request.url() != originalRequest.url())
            RELEASE_LOG(Loading, "Redirected scheme task would have been sent to a different URL.");

        processNextPendingTask();
    };
    
    if (m_waitingForCompletionHandler) {
        RELEASE_LOG(Loading, "Received redirect during previous redirect processing, queuing it.");
        queueTask([this, protectedThis = makeRef(*this), redirectResponse = WTFMove(redirectResponse), request = WTFMove(request)]() mutable {
            didPerformRedirection(WTFMove(redirectResponse), WTFMove(request));
        });
        return;
    }
    m_waitingForCompletionHandler = true;

    m_coreLoader->willSendRequest(WTFMove(request), redirectResponse, WTFMove(completionHandler));
}

void WebURLSchemeTaskProxy::didReceiveResponse(const ResourceResponse& response)
{
    if (m_waitingForCompletionHandler) {
        RELEASE_LOG(Loading, "Received response during redirect processing, queuing it.");
        queueTask([this, protectedThis = makeRef(*this), response] {
            didReceiveResponse(response);
        });
        return;
    }
    
    if (!hasLoader())
        return;

    m_waitingForCompletionHandler = true;
    m_coreLoader->didReceiveResponse(response, [this, protectedThis = makeRef(*this)] {
        m_waitingForCompletionHandler = false;
        processNextPendingTask();
    });
}

void WebURLSchemeTaskProxy::didReceiveData(size_t size, const uint8_t* data)
{
    if (!hasLoader())
        return;

    if (m_waitingForCompletionHandler) {
        RELEASE_LOG(Loading, "Received data during response processing, queuing it.");
        Vector<uint8_t> dataVector;
        dataVector.append(data, size);
        queueTask([this, protectedThis = makeRef(*this), dataVector = WTFMove(dataVector)] {
            didReceiveData(dataVector.size(), dataVector.data());
        });
        return;
    }

    auto protectedThis = makeRef(*this);
    m_coreLoader->didReceiveData(reinterpret_cast<const char*>(data), size, 0, DataPayloadType::DataPayloadBytes);
    processNextPendingTask();
}

void WebURLSchemeTaskProxy::didComplete(const ResourceError& error)
{
    if (!hasLoader())
        return;

    if (m_waitingForCompletionHandler) {
        queueTask([this, protectedThis = makeRef(*this), error] {
            didComplete(error);
        });
        return;
    }

    if (error.isNull())
        m_coreLoader->didFinishLoading(NetworkLoadMetrics());
    else
        m_coreLoader->didFail(error);

    m_coreLoader = nullptr;
    m_frame = nullptr;
}

bool WebURLSchemeTaskProxy::hasLoader()
{
    if (m_coreLoader && m_coreLoader->reachedTerminalState()) {
        m_coreLoader = nullptr;
        m_frame = nullptr;
    }

    return m_coreLoader;
}

void WebURLSchemeTaskProxy::processNextPendingTask()
{
    if (!m_queuedTasks.isEmpty())
        m_queuedTasks.takeFirst()();
}

} // namespace WebKit
