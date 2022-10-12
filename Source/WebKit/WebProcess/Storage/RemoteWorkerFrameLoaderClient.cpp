/*
 * Copyright (C) 20217-2022 Apple Inc. All rights reserved.
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
#include "RemoteWorkerFrameLoaderClient.h"

#include "Logging.h"
#include "WebDocumentLoader.h"

namespace WebKit {

RemoteWorkerFrameLoaderClient::RemoteWorkerFrameLoaderClient(WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier pageID, const String& userAgent)
    : m_webPageProxyID(webPageProxyID)
    , m_pageID(pageID)
    , m_userAgent(userAgent)
{
    RELEASE_LOG(Worker, "RemoteWorkerFrameLoaderClient::RemoteWorkerFrameLoaderClient webPageProxyID %" PRIu64 ", pageID %" PRIu64, webPageProxyID.toUInt64(), pageID.toUInt64());
}

Ref<WebCore::DocumentLoader> RemoteWorkerFrameLoaderClient::createDocumentLoader(const WebCore::ResourceRequest& request, const WebCore::SubstituteData& substituteData)
{
    return WebDocumentLoader::create(request, substituteData);
}

} // namespace WebKit
