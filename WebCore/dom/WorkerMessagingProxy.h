/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#ifndef WorkerMessagingProxy_h
#define WorkerMessagingProxy_h

#if ENABLE(WORKERS)

#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

    class ScriptExecutionContext;
    class String;
    class Worker;
    class WorkerTask;
    class WorkerThread;

    class WorkerMessagingProxy : Noncopyable {
    public:
        WorkerMessagingProxy(PassRefPtr<ScriptExecutionContext>, Worker*);

        void postMessageToWorkerObject(const String& message);
        void postMessageToWorkerContext(const String& message);

        void workerThreadCreated(PassRefPtr<WorkerThread>);
        void workerObjectDestroyed();
        void workerContextDestroyed();

        void confirmWorkerThreadMessage(bool hasPendingActivity);
        void reportWorkerThreadActivity(bool hasPendingActivity);
        bool workerThreadHasPendingActivity() const;

    private:
        friend class MessageWorkerTask;
        friend class WorkerContextDestroyedTask;
        friend class WorkerThreadActivityReportTask;

        ~WorkerMessagingProxy();

        void workerContextDestroyedInternal();
        void reportWorkerThreadActivityInternal(bool confirmingMessage, bool hasPendingActivity);
        Worker* workerObject() const { return m_workerObject; }

        RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
        Worker* m_workerObject;
        RefPtr<WorkerThread> m_workerThread;

        unsigned m_unconfirmedMessageCount; // Unconfirmed messages from worker object to worker thread.
        bool m_workerThreadHadPendingActivity; // The latest confirmation from worker thread reported that it was still active.

        Vector<RefPtr<WorkerTask> > m_queuedEarlyTasks; // Tasks are queued here until there's a thread object created.
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // WorkerMessagingProxy_h
