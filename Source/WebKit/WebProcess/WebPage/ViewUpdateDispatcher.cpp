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
#include "ViewUpdateDispatcher.h"

#if ENABLE(UI_SIDE_COMPOSITING)

#include "Connection.h"
#include "ViewUpdateDispatcherMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/PageIdentifier.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

ViewUpdateDispatcher::ViewUpdateDispatcher()
    : m_queue(WorkQueue::create("com.apple.WebKit.ViewUpdateDispatcher"))
{
}

ViewUpdateDispatcher::~ViewUpdateDispatcher()
{
    ASSERT_NOT_REACHED();
}

void ViewUpdateDispatcher::initializeConnection(IPC::Connection& connection)
{
    connection.addMessageReceiver(m_queue.get(), *this, Messages::ViewUpdateDispatcher::messageReceiverName());
}

void ViewUpdateDispatcher::visibleContentRectUpdate(WebCore::PageIdentifier pageID, const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo)
{
    bool updateListWasEmpty;
    {
        Locker locker { m_latestUpdateLock };
        updateListWasEmpty = m_latestUpdate.isEmpty();
        auto iterator = m_latestUpdate.find(pageID);
        if (iterator == m_latestUpdate.end())
            m_latestUpdate.set(pageID, makeUniqueRef<UpdateData>(visibleContentRectUpdateInfo, visibleContentRectUpdateInfo.timestamp()));
        else
            iterator->value.get().visibleContentRectUpdateInfo = visibleContentRectUpdateInfo;
    }
    if (updateListWasEmpty) {
        RunLoop::main().dispatch([this] {
            dispatchVisibleContentRectUpdate();
        });
    }
}

void ViewUpdateDispatcher::dispatchVisibleContentRectUpdate()
{
    HashMap<WebCore::PageIdentifier, UniqueRef<UpdateData>> update;
    {
        Locker locker { m_latestUpdateLock };
        update = std::exchange(m_latestUpdate, { });
    }

    for (auto& slot : update) {
        if (WebPage* webPage = WebProcess::singleton().webPage(slot.key))
            webPage->updateVisibleContentRects(slot.value.get().visibleContentRectUpdateInfo, slot.value.get().oldestTimestamp);
    }
}

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
