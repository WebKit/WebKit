/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebProcessSyncClient.h"

#include "MessageSenderInlines.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Page.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebProcessSyncClient);

WebProcessSyncClient::WebProcessSyncClient(WebPage& webPage)
    : m_page(webPage)
{
}

Ref<WebPage> WebProcessSyncClient::protectedPage() const
{
    return m_page.get();
}

bool WebProcessSyncClient::siteIsolationEnabled()
{
    RefPtr<WebCore::Page> corePage = protectedPage()->protectedCorePage();
    return corePage ? corePage->settings().siteIsolationEnabled() : false;
}

void WebProcessSyncClient::broadcastProcessSyncDataToOtherProcesses(const WebCore::ProcessSyncData& data)
{
    if (!siteIsolationEnabled())
        return;

    protectedPage()->send(Messages::WebPageProxy::BroadcastProcessSyncData(data));
}

void WebProcessSyncClient::broadcastTopDocumentSyncDataToOtherProcesses(WebCore::DocumentSyncData& data)
{
    if (!siteIsolationEnabled())
        return;

    protectedPage()->send(Messages::WebPageProxy::BroadcastTopDocumentSyncData(data));
}

} // namespace WebKit
