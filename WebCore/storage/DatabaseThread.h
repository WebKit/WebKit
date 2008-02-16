/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef DatabaseThread_h
#define DatabaseThread_h

#include "MessageQueue.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Deque.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Database;
class DatabaseTask;
class Document;

class DatabaseThread : public ThreadSafeShared<DatabaseThread>
{
public:
    DatabaseThread(Document*);
    ~DatabaseThread();

    bool start();
    void requestTermination();
    bool terminationRequested() const;

    void scheduleTask(DatabaseTask*);
    void scheduleImmediateTask(DatabaseTask*); // This just adds the task to the front of the queue - the caller needs to be extremely careful not to create deadlocks when waiting for completion.
    void unscheduleDatabaseTasks(Database*);

private:
    static void* databaseThreadStart(void*);
    void* databaseThread();

    ThreadIdentifier m_threadID;
    RefPtr<DatabaseThread> m_selfRef;

    MessageQueue<RefPtr<DatabaseTask> > m_queue;
};

} // namespace WebCore

#endif // DatabaseThread_h
