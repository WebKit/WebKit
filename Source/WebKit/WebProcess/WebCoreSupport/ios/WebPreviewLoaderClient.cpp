/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebPreviewLoaderClient.h"

#if USE(QUICK_LOOK)

#include "Logging.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

WebPreviewLoaderClient::WebPreviewLoaderClient(const String& fileName, const String& uti, PageIdentifier pageID)
    : m_fileName { fileName }
    , m_uti { uti }
    , m_pageID { pageID }
    , m_buffer { SharedBuffer::create() }
{
}

WebPreviewLoaderClient::~WebPreviewLoaderClient() = default;

void WebPreviewLoaderClient::didReceiveBuffer(const SharedBuffer& buffer)
{
    auto webPage = WebProcess::singleton().webPage(m_pageID);
    if (!webPage)
        return;

    if (m_buffer->isEmpty())
        webPage->didStartLoadForQuickLookDocumentInMainFrame(m_fileName, m_uti);

    m_buffer->append(buffer);
}

void WebPreviewLoaderClient::didFinishLoading()
{
    auto webPage = WebProcess::singleton().webPage(m_pageID);
    if (!webPage)
        return;

    webPage->didFinishLoadForQuickLookDocumentInMainFrame(m_buffer.get());
    m_buffer->clear();
}

void WebPreviewLoaderClient::didFail()
{
    m_buffer->clear();
}

void WebPreviewLoaderClient::didRequestPassword(Function<void(const String&)>&& completionHandler)
{
    auto webPage = WebProcess::singleton().webPage(m_pageID);
    if (!webPage) {
        completionHandler({ });
        return;
    }

    webPage->requestPasswordForQuickLookDocumentInMainFrame(m_fileName, WTFMove(completionHandler));
}

} // namespace WebKit

#endif // USE(QUICK_LOOK)
