/*
 * Copyright (C) 2018 Igalia, S.L.
 * Copyright (C) 2018 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

/* AbortableTaskQueue is a high-level synchronization object for cases where abortable work is done in
 * background thread(s) that sometimes needs to post tasks to the main thread.
 *
 * The tasks posted by the background thread(s) to the main thread may be asynchronous, using enqueueTask(),
 * which returns immediately; or synchronous, using enqueueTaskAndWait(), which blocks the calling
 * background thread until the task is run by the main thread (possibly returning a value).
 *
 * What makes AbortableTaskQueue different from other task queueing mechanisms is that it provides a two-phase
 * protocol for aborting the work in the background thread in presence of queued tasks without deadlocks or
 * late notification bugs.
 *
 * Without a two-phase design deadlocks would occur when attempting an abort if a background thread was
 * blocked in a synchronous task and needed to return from there for the abort to be handled. Also, without
 * a design like this, tasks already enqueued at that point or soon thereafter until the abort is complete
 * would still be handled by the main thread, even though we don't want to anymore.
 *
 * Aborting background processing with AbortableTaskQueue is a several step process:
 *
 *  1. Call abortableTaskQueue.startAborting() -- This will make any current or future (until further notice)
 *     synchronous tasks fail immediately, so that we don't deadlock in the next step. Also, tasks of any kind
 *     already enqueued will not be run.
 *
 *  2. Send the abort signal to the background threads. This is completely application specific. For instance,
 *     in the AppendPipeline case you would flush or reset the GStreamer pipeline here. Wait until all the
 *     background threads have finished aborting.
 *
 *  3. Call abortableTaskQueue.finishAborting() -- This will allow new tasks queued from this point on to be
 *     handled just as before the abort was made.
 *
 *  4. After this, the background thread(s) can be put to work again safely.
 *
 * This class is used for handling demuxer events in AppendPipeline, taking into account demuxing can be
 * aborted at any moment if SourceBuffer.abort() is called or the SourceBuffer is destroyed. */
class AbortableTaskQueue final {
    WTF_MAKE_NONCOPYABLE(AbortableTaskQueue);
public:
    AbortableTaskQueue()
    {
        ASSERT(isMainThread());
    }

    ~AbortableTaskQueue()
    {
        ASSERT(isMainThread());
        ASSERT(!m_mutex.isHeld());
        ASSERT(m_channel.isEmpty());
    }

    // ===========================
    // Methods for the main thread
    // ===========================

    // Starts an abort process.
    //
    // Tasks already queued will be discarded.
    //
    // Until finishAborting is called, all present and future calls to enqueueTaskAndWait() will immediately
    // return an empty optional.
    //
    // This method is idempotent.
    void startAborting()
    {
        ASSERT(isMainThread());

        {
            LockHolder lockHolder(m_mutex);
            m_aborting = true;
            cancelAllTasks();
        }
        m_abortedOrResponseSet.notifyAll();
    }

    // Declares the previous abort finished.
    //
    // In order to avoid race conditions the background threads must be unable to post tasks at this point.
    void finishAborting()
    {
        ASSERT(isMainThread());

        LockHolder lockHolder(m_mutex);
        ASSERT(m_aborting);
        m_aborting = false;
    }

    // ==================================
    // Methods for the background threads
    // ==================================

    // Enqueue a task to be run on the main thread. The task may be cancelled if an abort starts before it's
    // handled.
    void enqueueTask(WTF::Function<void()>&& mainThreadTaskHandler)
    {
        ASSERT(!isMainThread());

        LockHolder lockHolder(m_mutex);
        if (m_aborting)
            return;

        postTask(WTFMove(mainThreadTaskHandler));
    }

    // Enqueue a task to be run on the main thread and wait for it to return. The return value of the task is
    // forwarded to the background thread, wrapped in an optional.
    //
    // If we are aborting, the call finishes immediately, returning an empty optional.
    //
    // It is allowed for the main thread task handler to abort the AbortableTaskQueue. In that case, the return
    // value is discarded and the caller receives an empty optional.
    template<typename R>
    std::optional<R> enqueueTaskAndWait(WTF::Function<R()>&& mainThreadTaskHandler)
    {
        // Don't deadlock the main thread with itself.
        ASSERT(!isMainThread());

        LockHolder lockHolder(m_mutex);
        if (m_aborting)
            return std::nullopt;

        std::optional<R> response = std::nullopt;
        postTask([this, &response, &mainThreadTaskHandler]() {
            R responseValue = mainThreadTaskHandler();
            LockHolder lockHolder(m_mutex);
            if (!m_aborting)
                response = WTFMove(responseValue);
            m_abortedOrResponseSet.notifyAll();
        });
        m_abortedOrResponseSet.wait(m_mutex, [this, &response]() {
            return m_aborting || response;
        });
        return response;
    }

    // This is class is provided for convenience when you want to use enqueueTaskAndWait() but
    // you don't need any particular data from the main thread in return and just knowing that it finished
    // running the handler function is enough.
    class Void { };

private:
    // Protected state:
    //   Main thread: read-write. Writes must be made with the lock.
    //   Background threads: read only. Reads must be made with the lock.
    class Task : public ThreadSafeRefCounted<Task> {
        WTF_MAKE_NONCOPYABLE(Task);
        WTF_MAKE_FAST_ALLOCATED(Task);
    public:
        static Ref<Task> create(AbortableTaskQueue* taskQueue, WTF::Function<void()>&& taskCallback)
        {
            return adoptRef(*new Task(taskQueue, WTFMove(taskCallback)));
        }

        bool isCancelled() const
        {
            return !m_taskQueue;
        }

        void cancel()
        {
            ASSERT(!isCancelled());
            m_taskCallback = nullptr;
            m_taskQueue = nullptr;
        }

        void dispatch()
        {
            ASSERT(isMainThread());
            if (isCancelled())
                return;

            LockHolder lock(m_taskQueue->m_mutex);
            ASSERT(this == m_taskQueue->m_channel.first().ptr());
            m_taskQueue->m_channel.removeFirst();
            lock.unlockEarly();
            m_taskCallback();
        }

    private:
        AbortableTaskQueue* m_taskQueue;
        WTF::Function<void()> m_taskCallback;

        Task(AbortableTaskQueue* taskQueue, WTF::Function<void()>&& taskCallback)
            : m_taskQueue(taskQueue), m_taskCallback(WTFMove(taskCallback))
        { }
    };

    void postTask(WTF::Function<void()>&& callback)
    {
        ASSERT(m_mutex.isHeld());
        Ref<Task> task = Task::create(this, WTFMove(callback));
        m_channel.append(task.copyRef());
        RunLoop::main().dispatch([task = WTFMove(task)]() { task->dispatch(); });
    }

    void cancelAllTasks()
    {
        ASSERT(isMainThread());
        ASSERT(m_mutex.isHeld());
        for (Ref<Task>& task : m_channel)
            task->cancel();
        m_channel.clear();
    }

    bool m_aborting { false };
    Lock m_mutex;
    Condition m_abortedOrResponseSet;
    WTF::Deque<Ref<Task>> m_channel;
};

} // namespace WebCore
