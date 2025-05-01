/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "AsyncRenderObjectDeletionQueue.h"

#include "RenderElement.h"

namespace WebCore {

AsyncRenderObjectDeletionQueue::AsyncRenderObjectDeletionQueue() = default;

AsyncRenderObjectDeletionQueue::~AsyncRenderObjectDeletionQueue() = default;

void AsyncRenderObjectDeletionQueue::append(RenderPtr<RenderObject>&& toDestroy)
{
    if (m_renderObjectCount == s_maxSizeAsyncRenderObjectDeletionQueue)
        return;
    toDestroy->willBeDestroyedAsync();

    if (!head) {
        head = toDestroy.release();
        tail = head;
    } else {
        tail->setNextSiblingForAsyncDeletionQueue(toDestroy.get());
        tail = toDestroy.release();
    }
    m_renderObjectCount++;
}

int AsyncRenderObjectDeletionQueue::size()
{
    return m_renderObjectCount;
}

void AsyncRenderObjectDeletionQueue::deleteRenderObjectsNow()
{
    RenderObject* curr = head;
    RenderObject* next;
#ifndef NDEBUG
    unsigned numRenderObjectsDeleted = 0;
#endif
    while (curr) {
        next = curr->nextSibling();
        curr->deleteRenderObject();
        curr = next;
#ifndef NDEBUG
        numRenderObjectsDeleted++;
#endif
    }
    head = nullptr;
    tail = nullptr;
#ifndef NDEBUG
    ASSERT(numRenderObjectsDeleted == m_renderObjectCount);
#endif
    m_renderObjectCount = 0;
}

};
