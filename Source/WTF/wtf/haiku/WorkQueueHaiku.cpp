/*
 * Copyright (C) 2016 Haiku, Inc. All rights reserved.
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
#include "WorkQueue.h"

#include <Application.h>
#include <Message.h>
#include <MessageRunner.h>

namespace WTF {

static const uint32 kWorkQueueDispatch = 'wqdp';


struct WorkQueue::WorkItemHaiku
{
    ~WorkItemHaiku()
    {
        delete runner;
    }

    BMessageRunner* runner;
};

void WorkQueue::dispatch(std::function<void ()> function)
{
    ref();

    std::function<void ()>* storage = new std::function<void ()>(function);
    BMessage message(kWorkQueueDispatch);
    message.AddPointer("function", storage);
    message.AddPointer("queue", this);

    m_looper->PostMessage(&message);
}


class WorkQueueLooper: public BLooper
{
public:
    WorkQueueLooper(const char* name)
        : BLooper(name)
    {
        Run();
    }

protected:
    void MessageReceived(BMessage* message)
    {
        if (message->what == kWorkQueueDispatch) {
            WorkQueue* wq = nullptr;
            message->FindPointer("queue", (void**)&wq);
            wq->performWork(message);
            return;
        }

        BLooper::MessageReceived(message);
    }
};


void WorkQueue::dispatchAfter(std::chrono::nanoseconds duration,
    std::function<void ()> function)
{
    ref();

    std::function<void ()>* storage = new std::function<void ()>(function);
    BMessage message(kWorkQueueDispatch);
    message.AddPointer("function", storage);
    message.AddPointer("queue", this);

    WorkItemHaiku* item = new WorkItemHaiku();
    message.AddPointer("item", item);
    m_workItems.insert(item);

    item->runner = new BMessageRunner(m_looper, message,
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(),
        1);
}


void WorkQueue::platformInitialize(const char* name, Type, QOS)
{
    m_looper = new WorkQueueLooper(name);
}


void WorkQueue::platformInvalidate()
{
    thread_id thread = m_looper->Thread();
    status_t ret;

    m_looper->PostMessage(B_QUIT_REQUESTED);

    // Wait for the pending BMessages to be handled
    wait_for_thread(thread, &ret);

    // Delete all remaining work items
    for (auto item: m_workItems)
    {
        delete item;
    }
    m_workItems.clear();
}


void WorkQueue::performWork(BMessage* message)
{
    std::function<void ()>* function = nullptr;
    if (message->FindPointer("function", (void**)&function) == B_OK) {
        (*function)();
        delete function;
    }

    WorkItemHaiku* item = nullptr;
    if (message->FindPointer("item", (void**)&item) == B_OK) {
        m_workItems.erase(item);
        delete item;
    }

    deref();
}

};
