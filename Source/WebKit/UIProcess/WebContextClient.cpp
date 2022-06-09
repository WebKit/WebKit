/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "WebContextClient.h"

#include "WebProcessPool.h"

namespace WebKit {

void WebContextClient::plugInAutoStartOriginHashesChanged(WebProcessPool* processPool)
{
    if (!m_client.plugInAutoStartOriginHashesChanged)
        return;

    m_client.plugInAutoStartOriginHashesChanged(toAPI(processPool), m_client.base.clientInfo);
}

void WebContextClient::networkProcessDidCrash(WebProcessPool* processPool)
{
    if (!m_client.networkProcessDidCrash)
        return;

    m_client.networkProcessDidCrash(toAPI(processPool), m_client.base.clientInfo);
}

void WebContextClient::serviceWorkerProcessDidCrash(WebProcessPool* processPool, ProcessID processID)
{
    if (!m_client.serviceWorkerProcessDidCrash)
        return;

    m_client.serviceWorkerProcessDidCrash(toAPI(processPool), processID, m_client.base.clientInfo);
}

void WebContextClient::gpuProcessDidCrash(WebProcessPool* processPool, ProcessID processID)
{
    if (!m_client.gpuProcessDidCrash)
        return;

    m_client.gpuProcessDidCrash(toAPI(processPool), processID, m_client.base.clientInfo);
}

} // namespace WebKit
