/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebDownloadClient.h"

#include "WKAPICast.h"

namespace WebKit {

void WebDownloadClient::didStart(WebContext* webContext, DownloadProxy* downloadProxy)
{
    if (!m_client.didStart)
        return;

    m_client.didStart(toAPI(webContext), toAPI(downloadProxy), m_client.clientInfo);
}

void WebDownloadClient::didCreateDestination(WebContext* webContext, DownloadProxy* downloadProxy, const String& path)
{
    if (!m_client.didCreateDestination)
        return;

    m_client.didCreateDestination(toAPI(webContext), toAPI(downloadProxy), toAPI(path.impl()), m_client.clientInfo);
}

void WebDownloadClient::didFinish(WebContext* webContext, DownloadProxy* downloadProxy)
{
    if (!m_client.didFinish)
        return;
    
    m_client.didFinish(toAPI(webContext), toAPI(downloadProxy), m_client.clientInfo);
}    

} // namespace WebKit
