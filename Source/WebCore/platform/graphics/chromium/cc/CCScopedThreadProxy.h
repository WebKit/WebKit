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

#ifndef CCScopedThreadProxy_h
#define CCScopedThreadProxy_h

#include "CCThreadTask.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

// This class is a proxy used to post tasks to an target thread from any other thread. The proxy may be shut down at
// any point from the target thread after which no more tasks posted to the proxy will run. In other words, all
// tasks posted via a proxy are scoped to the lifecycle of the proxy. Use this when posting tasks to an object that
// might die with tasks in flight.
//
// The proxy must be created and shut down from the target thread, tasks may be posted from any thread.
//
// Implementation note: Unlike ScopedRunnableMethodFactory in Chromium, pending tasks are not cancelled by actually
// destroying the proxy. Instead each pending task holds a reference to the proxy to avoid maintaining an explicit
// list of outstanding tasks.
class CCScopedThreadProxy : public ThreadSafeRefCounted<CCScopedThreadProxy> {
public:
    static PassRefPtr<CCScopedThreadProxy> create(CCThread* targetThread)
    {
        ASSERT(currentThread() == targetThread->threadID());
        return adoptRef(new CCScopedThreadProxy(targetThread));
    }

    // Can be called from any thread. Posts a task to the target thread that runs unless
    // shutdown() is called before it runs.
    void postTask(PassOwnPtr<CCThread::Task> task)
    {
        ref();
        m_targetThread->postTask(createCCThreadTask(this, &CCScopedThreadProxy::runTaskIfNotShutdown, task));
    }

    void shutdown()
    {
        ASSERT(currentThread() == m_targetThread->threadID());
        ASSERT(!m_shutdown);
        m_shutdown = true;
    }

private:
    explicit CCScopedThreadProxy(CCThread* targetThread)
        : m_targetThread(targetThread)
        , m_shutdown(false) { }

    void runTaskIfNotShutdown(PassOwnPtr<CCThread::Task> popTask)
    {
        OwnPtr<CCThread::Task> task = popTask;
        // If our shutdown flag is set, it's possible that m_targetThread has already been destroyed so don't
        // touch it.
        if (m_shutdown) {
            deref();
            return;
        }
        ASSERT(currentThread() == m_targetThread->threadID());
        task->performTask();
        deref();
    }

    CCThread* m_targetThread;
    bool m_shutdown; // Only accessed on the target thread
};

}

#endif
