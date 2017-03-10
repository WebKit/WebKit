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

#include "WebErrors.h"
#include <WebCore/ResourceLoader.h>

using namespace WebCore;

namespace WebKit {

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
    auto result = m_tasks.add(loader.identifier(), std::make_unique<WebURLSchemeHandlerTaskProxy>(*this, loader));
    ASSERT(result.isNewEntry);

    result.iterator->value->startLoading();
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
    auto task = m_tasks.take(taskIdentifier);
    if (!task)
        return;

    task->didComplete(error);
}

} // namespace WebKit
