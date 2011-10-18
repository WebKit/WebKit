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

#ifndef CCScopedMainThreadProxy_h
#define CCScopedMainThreadProxy_h

#include "cc/CCMainThreadTask.h"
#include "cc/CCProxy.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

// This class is a proxy used to post tasks to the main thread from any other thread. The proxy may be shut down at
// any point from the main thread after which no more tasks posted to the proxy will run. In other words, all
// tasks posted via a proxy are scoped to the lifecycle of the proxy. Use this when posting tasks to an object that
// might die with tasks in flight.
//
// The proxy must be created and shut down from the main thread, tasks may be posted from any thread.
//
// Implementation note: Unlike ScopedRunnableMethodFactory in Chromium, pending tasks are not cancelled by actually
// destroying the proxy. Instead each pending task holds a reference to the proxy to avoid maintaining an explicit
// list of outstanding tasks.
class CCScopedMainThreadProxy : public ThreadSafeRefCounted<CCScopedMainThreadProxy> {
public:
    static PassRefPtr<CCScopedMainThreadProxy> create()
    {
        ASSERT(CCProxy::isMainThread());
        return adoptRef(new CCScopedMainThreadProxy);
    }

    // Can be called from any thread. Posts a task to the main thread that runs unless
    // shutdown() is called before it runs.
    void postTask(PassOwnPtr<CCMainThread::Task> task)
    {
        ref();
        CCMainThread::postTask(createMainThreadTask(this, &CCScopedMainThreadProxy::runTaskIfNotShutdown, task));
    }

    void shutdown()
    {
        ASSERT(CCProxy::isMainThread());
        ASSERT(!m_shutdown);
        m_shutdown = true;
    }

private:
    CCScopedMainThreadProxy()
        : m_shutdown(false)
    {
    }

    void runTaskIfNotShutdown(PassOwnPtr<CCMainThread::Task> popTask)
    {
        ASSERT(CCProxy::isMainThread());
        OwnPtr<CCMainThread::Task> task = popTask;
        if (!m_shutdown)
            task->performTask();
        deref();
    }

    bool m_shutdown; // Only accessed on the main thread
};

}

#endif
