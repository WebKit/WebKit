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
#if ENABLE(LLINT_C_LOOP)
#include <thread>
#endif

namespace JSC {

Mutex* GlobalJSLock::s_sharedInstanceLock = 0;

GlobalJSLock::GlobalJSLock()
{
    s_sharedInstanceLock->lock();
}

GlobalJSLock::~GlobalJSLock()
{
    s_sharedInstanceLock->unlock();
}

void GlobalJSLock::initialize()
{
    s_sharedInstanceLock = new Mutex();
}

JSLockHolder::JSLockHolder(ExecState* exec)
    : m_vm(exec ? &exec->vm() : 0)
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
    if (m_vm)
        m_vm->apiLock().lock();
}

JSLockHolder::~JSLockHolder()
{
    if (!m_vm)
        return;

    RefPtr<JSLock> apiLock(&m_vm->apiLock());
    m_vm.clear();
    apiLock->unlock();
}

JSLock::JSLock(VM* vm)
    : m_ownerThread(0)
    , m_lockCount(0)
    , m_lockDropDepth(0)
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

void JSLock::lock()
{
    lock(1);
}

void JSLock::lock(intptr_t lockCount)
{
    ASSERT(lockCount > 0);
    ThreadIdentifier currentThread = WTF::currentThread();
    if (currentThreadIsHoldingLock()) {
        m_lockCount += lockCount;
        return;
    }

    m_lock.lock();

    setOwnerThread(currentThread);
    ASSERT(!m_lockCount);
    m_lockCount = lockCount;

    WTFThreadData& threadData = wtfThreadData();

    if (!m_vm->stackPointerAtVMEntry) {
        void* p = &p;
        m_vm->stackPointerAtVMEntry = p; // A proxy for the current stack pointer.
        threadData.setSavedReservedZoneSize(m_vm->updateReservedZoneSize(Options::reservedZoneSize()));
    }

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
        if (m_vm) {
            m_vm->stackPointerAtVMEntry = nullptr;
            m_vm->updateReservedZoneSize(wtfThreadData().savedReservedZoneSize());
        }
        setOwnerThread(0);
        m_lock.unlock();
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
    return m_ownerThread == WTF::currentThread();
}

// This function returns the number of locks that were dropped.
unsigned JSLock::dropAllLocks(DropAllLocks* dropper)
{
    // Check if this thread is currently holding the lock.
    // FIXME: Maybe we want to require this, guard with an ASSERT?
    if (!currentThreadIsHoldingLock())
        return 0;

    ++m_lockDropDepth;

    UNUSED_PARAM(dropper);
#if ENABLE(LLINT_C_LOOP)
    dropper->setDropDepth(m_lockDropDepth);
#endif

    WTFThreadData& threadData = wtfThreadData();
    threadData.setSavedStackPointerAtVMEntry(m_vm->stackPointerAtVMEntry);
    threadData.setSavedLastStackTop(m_vm->lastStackTop());
    threadData.setSavedReservedZoneSize(m_vm->reservedZoneSize());

    unsigned droppedLockCount = m_lockCount;
    unlock(droppedLockCount);

    return droppedLockCount;
}

void JSLock::grabAllLocks(DropAllLocks* dropper, unsigned droppedLockCount)
{
    // If no locks were dropped, nothing to do!
    if (!droppedLockCount)
        return;

    ASSERT(!currentThreadIsHoldingLock());
    lock(droppedLockCount);

    UNUSED_PARAM(dropper);
#if ENABLE(LLINT_C_LOOP)
    while (dropper->dropDepth() != m_lockDropDepth) {
        unlock(droppedLockCount);
        std::this_thread::yield();
        lock(droppedLockCount);
    }
#endif

    --m_lockDropDepth;

    WTFThreadData& threadData = wtfThreadData();
    m_vm->stackPointerAtVMEntry = threadData.savedStackPointerAtVMEntry();
    m_vm->setLastStackTop(threadData.savedLastStackTop());
    m_vm->updateReservedZoneSize(threadData.savedReservedZoneSize());
}

JSLock::DropAllLocks::DropAllLocks(ExecState* exec)
    : m_droppedLockCount(0)
    , m_vm(exec ? &exec->vm() : nullptr)
{
    if (!m_vm)
        return;
    m_droppedLockCount = m_vm->apiLock().dropAllLocks(this);
}

JSLock::DropAllLocks::DropAllLocks(VM* vm)
    : m_droppedLockCount(0)
    , m_vm(vm)
{
    if (!m_vm)
        return;
    m_droppedLockCount = m_vm->apiLock().dropAllLocks(this);
}

JSLock::DropAllLocks::~DropAllLocks()
{
    if (!m_vm)
        return;
    m_vm->apiLock().grabAllLocks(this, m_droppedLockCount);
}

} // namespace JSC
