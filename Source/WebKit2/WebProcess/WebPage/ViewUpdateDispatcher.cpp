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

#if PLATFORM(IOS)

#include "ViewUpdateDispatcherMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <wtf/RunLoop.h>

namespace WebKit {

PassRef<ViewUpdateDispatcher> ViewUpdateDispatcher::create()
{
    return adoptRef(*new ViewUpdateDispatcher);
}

ViewUpdateDispatcher::ViewUpdateDispatcher()
    : m_queue(WorkQueue::create("com.apple.WebKit.ViewUpdateDispatcher"))
    , m_dataMutex(SPINLOCK_INITIALIZER)
{
}

ViewUpdateDispatcher::~ViewUpdateDispatcher()
{
}

void ViewUpdateDispatcher::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::ViewUpdateDispatcher::messageReceiverName(), m_queue.get(), this);
}

void ViewUpdateDispatcher::visibleContentRectUpdate(uint64_t pageID, const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo)
{
    bool updateListWasEmpty;
    {
        SpinLockHolder locker(&m_dataMutex);
        updateListWasEmpty = m_latestUpdate.isEmpty();
        m_latestUpdate.set(pageID, visibleContentRectUpdateInfo);
    }
    if (updateListWasEmpty)
        RunLoop::main().dispatch(bind(&ViewUpdateDispatcher::dispatchVisibleContentRectUpdate, this));
}

void ViewUpdateDispatcher::dispatchVisibleContentRectUpdate()
{
    HashMap<uint64_t, VisibleContentRectUpdateInfo> localCopy;
    {
        SpinLockHolder locker(&m_dataMutex);
        localCopy.swap(m_latestUpdate);
    }

    for (auto& slot : localCopy) {
        if (WebPage* webPage = WebProcess::shared().webPage(slot.key))
            webPage->updateVisibleContentRects(slot.value);
    }
}

} // namespace WebKit

#endif // PLATFORM(IOS)
