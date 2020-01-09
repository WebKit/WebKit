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

#include "ViewUpdateDispatcherMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/PageIdentifier.h>
#include <wtf/RunLoop.h>

namespace WebKit {

Ref<ViewUpdateDispatcher> ViewUpdateDispatcher::create()
{
    return adoptRef(*new ViewUpdateDispatcher);
}

ViewUpdateDispatcher::ViewUpdateDispatcher()
    : m_queue(WorkQueue::create("com.apple.WebKit.ViewUpdateDispatcher"))
{
}

ViewUpdateDispatcher::~ViewUpdateDispatcher()
{
}

void ViewUpdateDispatcher::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::ViewUpdateDispatcher::messageReceiverName(), m_queue.get(), this);
}

void ViewUpdateDispatcher::visibleContentRectUpdate(WebCore::PageIdentifier pageID, const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo)
{
    bool updateListWasEmpty;
    {
        LockHolder locker(&m_dataMutex);
        updateListWasEmpty = m_latestUpdate.isEmpty();
        auto iterator = m_latestUpdate.find(pageID);
        if (iterator == m_latestUpdate.end())
            m_latestUpdate.set<UpdateData>(pageID, { visibleContentRectUpdateInfo, visibleContentRectUpdateInfo.timestamp() });
        else
            iterator->value.visibleContentRectUpdateInfo = visibleContentRectUpdateInfo;
    }
    if (updateListWasEmpty) {
        RunLoop::main().dispatch([protectedThis = makeRef(*this)]() mutable {
            protectedThis->dispatchVisibleContentRectUpdate();
        });
    }
}

void ViewUpdateDispatcher::dispatchVisibleContentRectUpdate()
{
    HashMap<WebCore::PageIdentifier, UpdateData> update;
    {
        LockHolder locker(&m_dataMutex);
        update = WTFMove(m_latestUpdate);
    }

    for (auto& slot : update) {
        if (WebPage* webPage = WebProcess::singleton().webPage(slot.key))
            webPage->updateVisibleContentRects(slot.value.visibleContentRectUpdateInfo, slot.value.oldestTimestamp);
    }
}

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
