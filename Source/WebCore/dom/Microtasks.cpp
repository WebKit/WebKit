/*
 * Copyright (C) 2014 Yoav Weiss (yoav@yoav.ws)
 * Copyright (C) 2015 Akamai Technologies Inc. All rights reserved.
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Microtasks.h"

#include "CommonVM.h"
#include "WorkerGlobalScope.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>

namespace WebCore {

void Microtask::removeSelfFromQueue(MicrotaskQueue& queue)
{
    queue.remove(*this);
}

MicrotaskQueue::MicrotaskQueue(JSC::VM& vm)
    : m_vm(makeRef(vm))
    , m_timer(*this, &MicrotaskQueue::timerFired)
{
}

MicrotaskQueue::~MicrotaskQueue() = default;

MicrotaskQueue& MicrotaskQueue::mainThreadQueue()
{
    ASSERT(isMainThread());
    static NeverDestroyed<MicrotaskQueue> queue(commonVM());
    return queue;
}

MicrotaskQueue& MicrotaskQueue::contextQueue(ScriptExecutionContext& context)
{
    // While main thread has many ScriptExecutionContexts, WorkerGlobalScope and worker thread have
    // one on one correspondence. The lifetime of MicrotaskQueue is aligned to this semantics.
    // While main thread MicrotaskQueue is persistently held, worker's MicrotaskQueue is held by
    // WorkerGlobalScope.
    if (isMainThread())
        return mainThreadQueue();
    return downcast<WorkerGlobalScope>(context).microtaskQueue();
}

void MicrotaskQueue::append(std::unique_ptr<Microtask>&& task)
{
    m_microtaskQueue.append(WTFMove(task));

    m_timer.startOneShot(0_s);
}

void MicrotaskQueue::remove(const Microtask& task)
{
    for (size_t i = 0; i < m_microtaskQueue.size(); ++i) {
        if (m_microtaskQueue[i].get() == &task) {
            m_microtaskQueue.remove(i);
            return;
        }
    }
}

void MicrotaskQueue::timerFired()
{
    performMicrotaskCheckpoint();
}

void MicrotaskQueue::performMicrotaskCheckpoint()
{
    if (m_performingMicrotaskCheckpoint)
        return;

    SetForScope<bool> change(m_performingMicrotaskCheckpoint, true);
    JSC::JSLockHolder locker(vm());

    Vector<std::unique_ptr<Microtask>> toKeep;
    while (!m_microtaskQueue.isEmpty()) {
        Vector<std::unique_ptr<Microtask>> queue = WTFMove(m_microtaskQueue);
        for (auto& task : queue) {
            auto result = task->run();
            switch (result) {
            case Microtask::Result::Done:
                break;
            case Microtask::Result::KeepInQueue:
                toKeep.append(WTFMove(task));
                break;
            }
        }
    }

    vm().finalizeSynchronousJSExecution();
    m_microtaskQueue = WTFMove(toKeep);
}

} // namespace WebCore
