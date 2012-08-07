/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CCThreadImpl.h"

#include "cc/CCCompletionEvent.h"
#include <public/Platform.h>
#include <public/WebThread.h>

using WebCore::CCThread;
using WebCore::CCCompletionEvent;

namespace WebKit {

// Task that, when runs, places the current thread ID into the provided
// pointer and signals a completion event.
//
// Does not provide a PassOwnPtr<GetThreadIDTask>::create method because
// the resulting object is always handed into a WebThread, which does not understand
// PassOwnPtrs.
class GetThreadIDTask : public WebThread::Task {
public:
    GetThreadIDTask(ThreadIdentifier* result, CCCompletionEvent* completion)
         : m_completion(completion)
         , m_result(result) { }

    virtual ~GetThreadIDTask() { }

    virtual void run()
    {
        *m_result = currentThread();
        m_completion->signal();
    }

private:
    CCCompletionEvent* m_completion;
    ThreadIdentifier* m_result;
};

// General adapter from a CCThread::Task to a WebThread::Task.
class CCThreadTaskAdapter : public WebThread::Task {
public:
    CCThreadTaskAdapter(PassOwnPtr<CCThread::Task> task) : m_task(task) { }

    virtual ~CCThreadTaskAdapter() { }

    virtual void run()
    {
        m_task->performTask();
    }

private:
    OwnPtr<CCThread::Task> m_task;
};

PassOwnPtr<CCThread> CCThreadImpl::create(WebThread* thread)
{
    return adoptPtr(new CCThreadImpl(thread));
}

CCThreadImpl::~CCThreadImpl()
{
}

void CCThreadImpl::postTask(PassOwnPtr<CCThread::Task> task)
{
    m_thread->postTask(new CCThreadTaskAdapter(task));
}

void CCThreadImpl::postDelayedTask(PassOwnPtr<CCThread::Task> task, long long delayMs)
{
    m_thread->postDelayedTask(new CCThreadTaskAdapter(task), delayMs);
}

ThreadIdentifier CCThreadImpl::threadID() const
{
    return m_threadID;
}

CCThreadImpl::CCThreadImpl(WebThread* thread)
    : m_thread(thread)
{
    if (thread == WebKit::Platform::current()->currentThread()) {
        m_threadID = currentThread();
        return;
    }

    // Get the threadId for the newly-created thread by running a task
    // on that thread, blocking on the result.
    m_threadID = currentThread();
    CCCompletionEvent completion;
    m_thread->postTask(new GetThreadIDTask(&m_threadID, &completion));
    completion.wait();
}

} // namespace WebKit
