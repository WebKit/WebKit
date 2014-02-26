/*
 * Copyright (C) 2005, 2008, 2012, 2014 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the NU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA 
 *
 */

#include "config.h"
#include "JSLock.h"

#include "Heap.h"
#include "CallFrame.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSCInlines.h"
#include <thread>

namespace JSC {

std::mutex* GlobalJSLock::s_sharedInstanceMutex;

GlobalJSLock::GlobalJSLock()
{
    s_sharedInstanceMutex->lock();
}

GlobalJSLock::~GlobalJSLock()
{
    s_sharedInstanceMutex->unlock();
}

void GlobalJSLock::initialize()
{
    s_sharedInstanceMutex = new std::mutex();
}

JSLockHolder::JSLockHolder(ExecState* exec)
    : m_vm(&exec->vm())
{
    init();
}

JSLockHolder::JSLockHolder(VM* vm)
    : m_vm(vm)
{
    init();
}

JSLockHolder::JSLockHolder(VM& vm)
    : m_vm(&vm)
{
    init();
}

void JSLockHolder::init()
{
    m_vm->apiLock().lock();
}

JSLockHolder::~JSLockHolder()
{
    RefPtr<JSLock> apiLock(&m_vm->apiLock());
    m_vm.clear();
    apiLock->unlock();
}

JSLock::JSLock(VM* vm)
    : m_ownerThreadID(std::thread::id())
    , m_lockCount(0)
    , m_lockDropDepth(0)
    , m_hasExclusiveThread(false)
    , m_vm(vm)
{
}

JSLock::~JSLock()
{
}

void JSLock::willDestroyVM(VM* vm)
{
    ASSERT_UNUSED(vm, m_vm == vm);
    m_vm = 0;
}

void JSLock::setExclusiveThread(std::thread::id threadId)
{
    RELEASE_ASSERT(!m_lockCount && m_ownerThreadID == std::thread::id());
    m_hasExclusiveThread = (threadId != std::thread::id());
    m_ownerThreadID = threadId;
}

void JSLock::lock()
{
    lock(1);
}

void JSLock::lock(intptr_t lockCount)
{
    ASSERT(lockCount > 0);
    if (currentThreadIsHoldingLock()) {
        m_lockCount += lockCount;
        return;
    }

    if (!m_hasExclusiveThread) {
        m_lock.lock();
        m_ownerThreadID = std::this_thread::get_id();
    }
    ASSERT(!m_lockCount);
    m_lockCount = lockCount;

    if (!m_vm)
        return;

    RELEASE_ASSERT(!m_vm->stackPointerAtVMEntry());
    void* p = &p; // A proxy for the current stack pointer.
    m_vm->setStackPointerAtVMEntry(p);

    WTFThreadData& threadData = wtfThreadData();
    m_vm->setLastStackTop(threadData.savedLastStackTop());
}

void JSLock::unlock()
{
    unlock(1);
}

void JSLock::unlock(intptr_t unlockCount)
{
    RELEASE_ASSERT(currentThreadIsHoldingLock());
    ASSERT(m_lockCount >= unlockCount);

    m_lockCount -= unlockCount;

    if (!m_lockCount) {
        if (m_vm)
            m_vm->setStackPointerAtVMEntry(nullptr);

        if (!m_hasExclusiveThread) {
            m_ownerThreadID = std::thread::id();
            m_lock.unlock();
        }
    }
}

void JSLock::lock(ExecState* exec)
{
    exec->vm().apiLock().lock();
}

void JSLock::unlock(ExecState* exec)
{
    exec->vm().apiLock().unlock();
}

bool JSLock::currentThreadIsHoldingLock()
{
    ASSERT(!m_hasExclusiveThread || (exclusiveThread() == std::this_thread::get_id()));
    if (m_hasExclusiveThread)
        return !!m_lockCount;
    return m_ownerThreadID == std::this_thread::get_id();
}

// This function returns the number of locks that were dropped.
unsigned JSLock::dropAllLocks(DropAllLocks* dropper)
{
    if (m_hasExclusiveThread) {
        ASSERT(exclusiveThread() == std::this_thread::get_id());
        return 0;
    }

    // Check if this thread is currently holding the lock.
    // FIXME: Maybe we want to require this, guard with an ASSERT?
    if (!currentThreadIsHoldingLock())
        return 0;

    ++m_lockDropDepth;

    dropper->setDropDepth(m_lockDropDepth);

    WTFThreadData& threadData = wtfThreadData();
    threadData.setSavedStackPointerAtVMEntry(m_vm->stackPointerAtVMEntry());
    threadData.setSavedLastStackTop(m_vm->lastStackTop());

    unsigned droppedLockCount = m_lockCount;
    unlock(droppedLockCount);

    return droppedLockCount;
}

void JSLock::grabAllLocks(DropAllLocks* dropper, unsigned droppedLockCount)
{
    ASSERT(!m_hasExclusiveThread || !droppedLockCount);

    // If no locks were dropped, nothing to do!
    if (!droppedLockCount)
        return;

    ASSERT(!currentThreadIsHoldingLock());
    lock(droppedLockCount);

    while (dropper->dropDepth() != m_lockDropDepth) {
        unlock(droppedLockCount);
        std::this_thread::yield();
        lock(droppedLockCount);
    }

    --m_lockDropDepth;

    WTFThreadData& threadData = wtfThreadData();
    m_vm->setStackPointerAtVMEntry(threadData.savedStackPointerAtVMEntry());
    m_vm->setLastStackTop(threadData.savedLastStackTop());
}

JSLock::DropAllLocks::DropAllLocks(ExecState* exec)
    : m_droppedLockCount(0)
    , m_vm(exec ? &exec->vm() : nullptr)
{
    if (!m_vm)
        return;
    RELEASE_ASSERT(!m_vm->isCollectorBusy());
    m_droppedLockCount = m_vm->apiLock().dropAllLocks(this);
}

JSLock::DropAllLocks::DropAllLocks(VM* vm)
    : m_droppedLockCount(0)
    , m_vm(vm)
{
    if (!m_vm)
        return;
    RELEASE_ASSERT(!m_vm->isCollectorBusy());
    m_droppedLockCount = m_vm->apiLock().dropAllLocks(this);
}

JSLock::DropAllLocks::~DropAllLocks()
{
    if (!m_vm)
        return;
    m_vm->apiLock().grabAllLocks(this, m_droppedLockCount);
}

} // namespace JSC
