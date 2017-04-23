/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "MachineContext.h"
#include "RegisterState.h"
#include <wtf/DoublyLinkedList.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/ScopedLambda.h>
#include <wtf/ThreadSpecific.h>

namespace JSC {

class CodeBlockSet;
class ConservativeRoots;
class Heap;
class JITStubRoutineSet;

struct CurrentThreadState {
    void* stackOrigin { nullptr };
    void* stackTop { nullptr };
    RegisterState* registerState { nullptr };
};
    
class MachineThreads {
    WTF_MAKE_NONCOPYABLE(MachineThreads);
public:
    MachineThreads();
    ~MachineThreads();

    void gatherConservativeRoots(ConservativeRoots&, JITStubRoutineSet&, CodeBlockSet&, CurrentThreadState*);

    JS_EXPORT_PRIVATE void addCurrentThread(); // Only needs to be called by clients that can use the same heap from multiple threads.

    class MachineThread : public DoublyLinkedListNode<MachineThread> {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        MachineThread();

        struct Registers {
            void* stackPointer() const;
#if ENABLE(SAMPLING_PROFILER)
            void* framePointer() const;
            void* instructionPointer() const;
            void* llintPC() const;
#endif // ENABLE(SAMPLING_PROFILER)
            PlatformRegisters regs;
        };

        Expected<void, Thread::PlatformSuspendError> suspend() { return m_thread->suspend(); }
        void resume() { m_thread->resume(); }
        size_t getRegisters(Registers& regs);
        std::pair<void*, size_t> captureStack(void* stackTop);

        WTF::ThreadIdentifier threadID() const { return m_thread->id(); }
        void* stackBase() const { return m_stackBase; }
        void* stackEnd() const { return m_stackEnd; }

        Ref<WTF::Thread> m_thread;
        void* m_stackBase;
        void* m_stackEnd;
        MachineThread* m_next { nullptr };
        MachineThread* m_prev { nullptr };
    };

    Lock& getLock() { return m_registeredThreadsMutex; }
    const DoublyLinkedList<MachineThread>& threadsListHead(const AbstractLocker&) const { ASSERT(m_registeredThreadsMutex.isLocked()); return m_registeredThreads; }
    MachineThread* machineThreadForCurrentThread();

private:
    void gatherFromCurrentThread(ConservativeRoots&, JITStubRoutineSet&, CodeBlockSet&, CurrentThreadState&);

    void tryCopyOtherThreadStack(MachineThread*, void*, size_t capacity, size_t*);
    bool tryCopyOtherThreadStacks(const AbstractLocker&, void*, size_t capacity, size_t*);

    static void THREAD_SPECIFIC_CALL removeThread(void*);

    void removeThreadIfFound(ThreadIdentifier);

    Lock m_registeredThreadsMutex;
    DoublyLinkedList<MachineThread> m_registeredThreads;
    WTF::ThreadSpecificKey m_threadSpecificForMachineThreads;
};

#define DECLARE_AND_COMPUTE_CURRENT_THREAD_STATE(stateName) \
    CurrentThreadState stateName; \
    stateName.stackTop = &stateName; \
    stateName.stackOrigin = wtfThreadData().stack().origin(); \
    ALLOCATE_AND_GET_REGISTER_STATE(stateName ## _registerState); \
    stateName.registerState = &stateName ## _registerState

// The return value is meaningless. We just use it to suppress tail call optimization.
int callWithCurrentThreadState(const ScopedLambda<void(CurrentThreadState&)>&);

} // namespace JSC

