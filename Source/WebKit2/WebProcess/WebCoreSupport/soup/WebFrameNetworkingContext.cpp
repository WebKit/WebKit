/*
 * Copyright (C) 2012 Igalia S.L.
 * Copyright (C) 2013 Company 100 Inc.
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
#include "WebFrameNetworkingContext.h"

#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/Settings.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

static std::unique_ptr<NetworkStorageSession>& privateSession()
{
    static NeverDestroyed<std::unique_ptr<NetworkStorageSession>> session;
    return session;
}

void WebFrameNetworkingContext::ensurePrivateBrowsingSession()
{
    ASSERT(isMainThread());

    if (privateSession())
        return;

    privateSession() = NetworkStorageSession::createPrivateBrowsingSession();
}

void WebFrameNetworkingContext::destroyPrivateBrowsingSession()
{
    ASSERT(isMainThread());

    privateSession() = nullptr;
}

WebFrameNetworkingContext::WebFrameNetworkingContext(WebFrame* frame)
    : FrameNetworkingContext(frame->coreFrame())
    , m_initiatingPageID(0)
{
    if (WebPage* page = frame->page())
        m_initiatingPageID = page->pageID();
}

NetworkStorageSession& WebFrameNetworkingContext::storageSession() const
{
    if (frame() && frame()->settings().privateBrowsingEnabled())
        return *privateSession();

    return NetworkStorageSession::defaultStorageSession();
}

uint64_t WebFrameNetworkingContext::initiatingPageID() const
{
    return m_initiatingPageID;
}

WebFrameLoaderClient* WebFrameNetworkingContext::webFrameLoaderClient() const
{
    if (!frame())
        return nullptr;

    return toWebFrameLoaderClient(frame()->loader().client());
}

}
