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
#include "LocalFrameLoaderClient.h"

#include "Document.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "ResourceTiming.h"

namespace WebCore {

LocalFrameLoaderClient::LocalFrameLoaderClient(FrameLoader& loader)
    : m_loader(loader)
{ }

LocalFrameLoaderClient::~LocalFrameLoaderClient() = default;

void LocalFrameLoaderClient::ref() const
{
    m_loader->ref();
}

void LocalFrameLoaderClient::deref() const
{
    m_loader->deref();
}

#if ENABLE(CONTENT_EXTENSIONS)
void LocalFrameLoaderClient::didExceedNetworkUsageThreshold()
{
}
#endif

void LocalFrameLoaderClient::applyMonitorUnloadToOwnerFrame(IFrameUnloadReason)
{
}

// The three notifications below are entry points for the multi-process BFCache
// coordination on top of WebKit's UIProcess. WebKitLegacy's WebFrameLoaderClient
// does not run a UIProcess and tracks BFCache locally, so the base class
// default is a silent no-op rather than an assert.
void LocalFrameLoaderClient::didCacheBackForwardItem(BackForwardItemIdentifier, BackForwardFrameItemIdentifier)
{
}

void LocalFrameLoaderClient::didEvictBackForwardItem(BackForwardItemIdentifier)
{
}

void LocalFrameLoaderClient::didTakeBackForwardItemForRestoration(BackForwardItemIdentifier)
{
}

RefPtr<Frame> LocalFrameLoaderClient::provisionalParentFrame() const
{
    return nullptr;
}

void LocalFrameLoaderClient::dispatchBackForwardItemLoading(const URL& url, const String& referer, LocalFrame& childFrame)
{
    Ref loader = m_loader;
    ASSERT(isBackForwardLoadType(loader->loadType()));
    ASSERT(!loader->frame().document()->loadEventFinished());

    if (loader->loadChildHistoryItemIntoFrame(childFrame))
        return;

    loader->continueLoadURLIntoChildFrame(url, referer, childFrame);
}

} // namespace WebCore
