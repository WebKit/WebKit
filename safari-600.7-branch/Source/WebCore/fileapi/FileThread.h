/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FileThread_h
#define FileThread_h

#include <functional>
#include <wtf/MessageQueue.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

class FileStream;

class FileThread : public ThreadSafeRefCounted<FileThread> {
public:
    static PassRefPtr<FileThread> create()
    {
        return adoptRef(new FileThread());
    }

    ~FileThread();

    bool start();
    void stop();

    class Task {
        WTF_MAKE_NONCOPYABLE(Task);
    public:
        template<typename T, typename U, typename = typename std::enable_if<!std::is_base_of<Task, U>::value && std::is_convertible<U, std::function<void ()>>::value>::type>
        Task(T* instance, U method)
            : m_task(WTF::move(method))
            , m_instance(instance)
        {
        }

        Task(Task&& other)
            : m_task(WTF::move(other.m_task))
            , m_instance(other.m_instance)
        {
        }

        void performTask()
        {
            m_task();
        }
        void* instance() const { return m_instance; }

    private:
        std::function<void ()> m_task;
        void* m_instance;
    };

    void postTask(Task);

    void unscheduleTasks(const void* instance);

private:
    FileThread();

    static void fileThreadStart(void*);
    void runLoop();

    ThreadIdentifier m_threadID;
    RefPtr<FileThread> m_selfRef;
    MessageQueue<Task> m_queue;

    Mutex m_threadCreationMutex;
};

} // namespace WebCore

#endif // FileThread_h
