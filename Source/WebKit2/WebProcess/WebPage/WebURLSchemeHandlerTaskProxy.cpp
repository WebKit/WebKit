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
#include "WebURLSchemeHandlerTaskProxy.h"

#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebURLSchemeHandlerProxy.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <wtf/CurrentTime.h>

using namespace WebCore;

namespace WebKit {

WebURLSchemeHandlerTaskProxy::WebURLSchemeHandlerTaskProxy(WebURLSchemeHandlerProxy& handler, ResourceLoader& loader)
    : m_urlSchemeHandler(handler)
    , m_coreLoader(&loader)
    , m_request(loader.request())
{
}

void WebURLSchemeHandlerTaskProxy::startLoading()
{
    ASSERT(m_coreLoader);
    m_urlSchemeHandler.page().send(Messages::WebPageProxy::StartURLSchemeHandlerTask(m_urlSchemeHandler.identifier(), m_coreLoader->identifier(), m_request));
}

void WebURLSchemeHandlerTaskProxy::stopLoading()
{
    if (!m_coreLoader)
        return;

    m_urlSchemeHandler.page().send(Messages::WebPageProxy::StopURLSchemeHandlerTask(m_urlSchemeHandler.identifier(), m_coreLoader->identifier()));
    m_coreLoader = nullptr;
}

void WebURLSchemeHandlerTaskProxy::didReceiveResponse(const ResourceResponse& response)
{
    if (!m_coreLoader)
        return;

    m_coreLoader->didReceiveResponse(response);
}

void WebURLSchemeHandlerTaskProxy::didReceiveData(size_t size, const uint8_t* data)
{
    if (!m_coreLoader)
        return;

    m_coreLoader->didReceiveData(reinterpret_cast<const char*>(data), size, 0, DataPayloadType::DataPayloadBytes);
}

void WebURLSchemeHandlerTaskProxy::didComplete(const ResourceError& error)
{
    if (!m_coreLoader)
        return;

    if (error.isNull())
        m_coreLoader->didFinishLoading(NetworkLoadMetrics());
    else
        m_coreLoader->didFail(error);

    m_coreLoader = nullptr;
}

} // namespace WebKit
