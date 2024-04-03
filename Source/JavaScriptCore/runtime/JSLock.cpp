/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
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

#include "HeapInlines.h"
#include "JSGlobalObject.h"
#include "MachineStackMarker.h"
#include "SamplingProfiler.h"
#include <wtf/StackPointer.h>
#include <wtf/Threading.h>
#include <wtf/threads/Signals.h>

#if USE(WEB_THREAD)
#include <wtf/ios/WebCoreThread.h>
#endif

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace JSC {

Lock GlobalJSLock::s_sharedInstanceMutex;

GlobalJSLock::GlobalJSLock()
{
    s_sharedInstanceMutex.lock();
}

GlobalJSLock::~GlobalJSLock()
{
    s_sharedInstanceMutex.unlock();
}

JSLockHolder::JSLockHolder(JSGlobalObject* globalObject)
    : JSLockHolder(globalObject->vm())
{
}

JSLockHolder::JSLockHolder(VM* vm)
    : JSLockHolder(*vm)
{
}

JSLockHolder::JSLockHolder(VM& vm)
    : m_vm(&vm)
{
    m_vm->apiLock().lock();
}

JSLockHolder::~JSLockHolder()
{
    RefPtr<JSLock> apiLock(&m_vm->apiLock());
    m_vm = nullptr;
    apiLock->unlock();
}

JSLock::JSLock(VM* vm)
    : m_lockCount(0)
    , m_lockDropDepth(0)
    , m_vm(vm)
    , m_entryAtomStringTable(nullptr)
{
}

JSLock::~JSLock()
{
}

void JSLock::willDestroyVM(VM* vm)
{
    ASSERT_UNUSED(vm, m_vm == vm);
    m_vm = nullptr;
}

void JSLock::lock()
{
    lock(1);
}

// Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS because this function conditionally unlocks m_lock, which
// is not supported by analysis.
void JSLock::lock(intptr_t lockCount) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(lockCount > 0);
#if USE(WEB_THREAD)
    if (m_isWebThreadAware) {
        ASSERT(WebCoreWebThreadIsEnabled && WebCoreWebThreadIsEnabled());
        WebCoreWebThreadLock();
    }
#endif

    bool success = m_lock.tryLock();
    if (UNLIKELY(!success)) {
        if (currentThreadIsHoldingLock()) {
            m_lockCount += lockCount;
            return;
        }
        m_lock.lock();
    }

    m_ownerThread = &Thread::current();
    WTF::storeStoreFence();
    m_hasOwnerThread = true;
    ASSERT(!m_lockCount);
    m_lockCount = lockCount;

    didAcquireLock();
}

void JSLock::didAcquireLock()
{  
    // FIXME: What should happen to the per-thread identifier table if we don't have a VM?
    if (!m_vm)
        return;
    
    Thread& thread = Thread::current();
    ASSERT(!m_entryAtomStringTable);
    m_entryAtomStringTable = thread.setCurrentAtomStringTable(m_vm->atomStringTable());
    ASSERT(m_entryAtomStringTable);

    m_vm->setLastStackTop(thread);

    if (m_vm->heap.hasAccess())
        m_shouldReleaseHeapAccess = false;
    else {
        m_vm->heap.acquireAccess();
        m_shouldReleaseHeapAccess = true;
    }

    RELEASE_ASSERT(!m_vm->stackPointerAtVMEntry());
    void* p = currentStackPointer();
    m_vm->setStackPointerAtVMEntry(p);

    if (thread.uid() != m_lastOwnerThread) {
        m_lastOwnerThread = thread.uid();
        if (m_vm->heap.machineThreads().addCurrentThread()) {
            if (isKernTCSMAvailable())
                enableKernTCSM();
        }
    }

    // Note: everything below must come after addCurrentThread().
    m_vm->traps().notifyGrabAllLocks();

#if ENABLE(SAMPLING_PROFILER)
    {
        SamplingProfiler* samplingProfiler = m_vm->samplingProfiler();
        if (UNLIKELY(samplingProfiler))
            samplingProfiler->noticeJSLockAcquisition();
    }
#endif
}

void JSLock::unlock()
{
    unlock(1);
}

// Use WTF_IGNORES_THREAD_SAFETY_ANALYSIS because this function conditionally unlocks m_lock, which
// is not supported by analysis.
void JSLock::unlock(intptr_t unlockCount) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    RELEASE_ASSERT(currentThreadIsHoldingLock());
    ASSERT(m_lockCount >= unlockCount);

    // Maintain m_lockCount while calling willReleaseLock() so that its callees know that
    // they still have the lock.
    if (unlockCount == m_lockCount)
        willReleaseLock();

    m_lockCount -= unlockCount;

    if (!m_lockCount) {
        m_hasOwnerThread = false;
        m_lock.unlock();
    }
}

void JSLock::willReleaseLock()
{
    {
        RefPtr protectedVM { m_vm };
        if (protectedVM) {
            static bool useLegacyDrain = false;
#if PLATFORM(COCOA)
            static std::once_flag once;
            std::call_once(once, [] {
                useLegacyDrain = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DoesNotDrainTheMicrotaskQueueWhenCallingObjC);
            });
#endif

            if (!m_lockDropDepth || useLegacyDrain)
                protectedVM->drainMicrotasks();

            if (!protectedVM->topCallFrame)
                protectedVM->clearLastException();

            protectedVM->heap.releaseDelayedReleasedObjects();
            protectedVM->setStackPointerAtVMEntry(nullptr);

            if (m_shouldReleaseHeapAccess)
                protectedVM->heap.releaseAccess();
        }
    }

    if (m_entryAtomStringTable) {
        Thread::current().setCurrentAtomStringTable(m_entryAtomStringTable);
        m_entryAtomStringTable = nullptr;
    }
}

void JSLock::lock(JSGlobalObject* globalObject)
{
    globalObject->vm().apiLock().lock();
}

void JSLock::unlock(JSGlobalObject* globalObject)
{
    globalObject->vm().apiLock().unlock();
}

// This function returns the number of locks that were dropped.
unsigned JSLock::dropAllLocks(DropAllLocks* dropper)
{
    if (!currentThreadIsHoldingLock())
        return 0;

    ++m_lockDropDepth;

    dropper->setDropDepth(m_lockDropDepth);

    Thread& thread = Thread::current();
    thread.setSavedStackPointerAtVMEntry(m_vm->stackPointerAtVMEntry());
    thread.setSavedLastStackTop(m_vm->lastStackTop());

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

    while (dropper->dropDepth() != m_lockDropDepth) {
        unlock(droppedLockCount);
        Thread::yield();
        lock(droppedLockCount);
    }

    --m_lockDropDepth;

    Thread& thread = Thread::current();
    m_vm->setStackPointerAtVMEntry(thread.savedStackPointerAtVMEntry());
    m_vm->setLastStackTop(thread);
}

JSLock::DropAllLocks::DropAllLocks(VM* vm)
    : m_droppedLockCount(0)
    // If the VM is in the middle of being destroyed then we don't want to resurrect it
    // by allowing DropAllLocks to ref it. By this point the JSLock has already been 
    // released anyways, so it doesn't matter that DropAllLocks is a no-op.
    , m_vm(vm->heap.isShuttingDown() ? nullptr : vm)
{
    if (!m_vm)
        return;

    // Contrary to intuition, DropAllLocks does not require that we are actually holding
    // the JSLock before getting here. Its goal is to release the lock if it is held. So,
    // if the lock isn't already held, there's nothing to do, and that's fine.
    // See https://bugs.webkit.org/show_bug.cgi?id=139654#c11.
    RELEASE_ASSERT(!m_vm->apiLock().currentThreadIsHoldingLock() || !m_vm->isCollectorBusyOnCurrentThread(), m_vm->apiLock().currentThreadIsHoldingLock(), m_vm->isCollectorBusyOnCurrentThread());
    m_droppedLockCount = m_vm->apiLock().dropAllLocks(this);
}

JSLock::DropAllLocks::DropAllLocks(JSGlobalObject* globalObject)
    : DropAllLocks(globalObject ? &globalObject->vm() : nullptr)
{
}

JSLock::DropAllLocks::DropAllLocks(VM& vm)
    : DropAllLocks(&vm)
{
}

JSLock::DropAllLocks::~DropAllLocks()
{
    if (!m_vm)
        return;
    m_vm->apiLock().grabAllLocks(this, m_droppedLockCount);
}

} // namespace JSC
