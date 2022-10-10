/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "WebProcess.h"
#include "WebURLSchemeHandlerProxy.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <wtf/CompletionHandler.h>

#define WEBURLSCHEMETASKPROXY_RELEASE_LOG_STANDARD_TEMPLATE "[schemeHandler=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", taskID=%" PRIu64 "] WebURLSchemeTaskProxy::"
#define WEBURLSCHEMETASKPROXY_RELEASE_LOG_STANDARD_PARAMETERS m_urlSchemeHandler.identifier().toUInt64(), pageIDFromWebFrame(m_frame), frameIDFromWebFrame(m_frame), m_identifier.toUInt64()
#define WEBURLSCHEMETASKPROXY_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, WEBURLSCHEMETASKPROXY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBURLSCHEMETASKPROXY_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

#if !RELEASE_LOG_DISABLED
static uint64_t pageIDFromWebFrame(const RefPtr<WebFrame>& frame)
{
    if (frame) {
        if (auto* page = frame->page())
            return page->identifier().toUInt64();
    }
    return 0;
}

static uint64_t frameIDFromWebFrame(const RefPtr<WebFrame>& frame)
{
    if (frame)
        return frame->frameID().object().toUInt64();
    return 0;
}
#endif

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
    WEBURLSCHEMETASKPROXY_RELEASE_LOG("startLoading");
    m_urlSchemeHandler.page().send(Messages::WebPageProxy::StartURLSchemeTask(URLSchemeTaskParameters { m_urlSchemeHandler.identifier(), m_coreLoader->identifier(), m_request, m_frame->info() }));
}

void WebURLSchemeTaskProxy::stopLoading()
{
    ASSERT(m_coreLoader);
    WEBURLSCHEMETASKPROXY_RELEASE_LOG("stopLoading");
    m_urlSchemeHandler.page().send(Messages::WebPageProxy::StopURLSchemeTask(m_urlSchemeHandler.identifier(), m_coreLoader->identifier()));
    m_coreLoader = nullptr;
    m_frame = nullptr;

    // This line will result in this being deleted.
    m_urlSchemeHandler.taskDidStopLoading(*this);
}
    
void WebURLSchemeTaskProxy::didPerformRedirection(WebCore::ResourceResponse&& redirectResponse, WebCore::ResourceRequest&& request, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    if (!hasLoader()) {
        completionHandler({ });
        return;
    }

    if (m_waitingForCompletionHandler) {
        WEBURLSCHEMETASKPROXY_RELEASE_LOG("didPerformRedirection: Received redirect during previous redirect processing, queuing it.");
        queueTask([this, protectedThis = Ref { *this }, redirectResponse = WTFMove(redirectResponse), request = WTFMove(request), completionHandler = WTFMove(completionHandler)]() mutable {
            didPerformRedirection(WTFMove(redirectResponse), WTFMove(request), WTFMove(completionHandler));
        });
        return;
    }
    m_waitingForCompletionHandler = true;

    auto innerCompletionHandler = [this, protectedThis = Ref { *this }, originalRequest = request, completionHandler = WTFMove(completionHandler)] (ResourceRequest&& request) mutable {
        m_waitingForCompletionHandler = false;

        completionHandler(WTFMove(request));

        processNextPendingTask();
    };

    m_coreLoader->willSendRequest(WTFMove(request), redirectResponse, WTFMove(innerCompletionHandler));
}

void WebURLSchemeTaskProxy::didReceiveResponse(const ResourceResponse& response)
{
    if (m_waitingForCompletionHandler) {
        WEBURLSCHEMETASKPROXY_RELEASE_LOG("didReceiveResponse: Received response during redirect processing, queuing it.");
        queueTask([this, protectedThis = Ref { *this }, response] {
            didReceiveResponse(response);
        });
        return;
    }
    
    if (!hasLoader())
        return;

    m_waitingForCompletionHandler = true;
    m_coreLoader->didReceiveResponse(response, [this, protectedThis = Ref { *this }] {
        m_waitingForCompletionHandler = false;
        processNextPendingTask();
    });
}

void WebURLSchemeTaskProxy::didReceiveData(const WebCore::SharedBuffer& data)
{
    if (!hasLoader())
        return;

    if (m_waitingForCompletionHandler) {
        WEBURLSCHEMETASKPROXY_RELEASE_LOG("didReceiveData: Received data during response processing, queuing it.");
        queueTask([this, protectedThis = Ref { *this }, data = Ref { data }] {
            didReceiveData(data);
        });
        return;
    }

    Ref protectedThis { *this };
    m_coreLoader->didReceiveData(data, 0, DataPayloadType::DataPayloadBytes);
    processNextPendingTask();
}

void WebURLSchemeTaskProxy::didComplete(const ResourceError& error)
{
    WEBURLSCHEMETASKPROXY_RELEASE_LOG("didComplete");
    if (!hasLoader())
        return;

    if (m_waitingForCompletionHandler) {
        queueTask([this, protectedThis = Ref { *this }, error] {
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

#undef WEBURLSCHEMETASKPROXY_RELEASE_LOG_STANDARD_TEMPLATE
#undef WEBURLSCHEMETASKPROXY_RELEASE_LOG_STANDARD_PARAMETERS
#undef WEBURLSCHEMETASKPROXY_RELEASE_LOG
