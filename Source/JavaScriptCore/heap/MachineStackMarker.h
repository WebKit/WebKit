/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef MachineThreads_h
#define MachineThreads_h

#include <setjmp.h>
#include <wtf/Noncopyable.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/ThreadingPrimitives.h>

namespace JSC {

    class CodeBlockSet;
    class ConservativeRoots;
    class Heap;
    class JITStubRoutineSet;

    class MachineThreads {
        WTF_MAKE_NONCOPYABLE(MachineThreads);
    public:
        typedef jmp_buf RegisterState;

        MachineThreads(Heap*);
        ~MachineThreads();

        void gatherConservativeRoots(ConservativeRoots&, JITStubRoutineSet&, CodeBlockSet&, void* stackCurrent, RegisterState& registers);

        JS_EXPORT_PRIVATE void makeUsableFromMultipleThreads();
        JS_EXPORT_PRIVATE void addCurrentThread(); // Only needs to be called by clients that can use the same heap from multiple threads.

    private:
        void gatherFromCurrentThread(ConservativeRoots&, JITStubRoutineSet&, CodeBlockSet&, void* stackCurrent, RegisterState& registers);

        class Thread;

        static void removeThread(void*);
        void removeCurrentThread();

        void gatherFromOtherThread(ConservativeRoots&, Thread*, JITStubRoutineSet&, CodeBlockSet&);

        Mutex m_registeredThreadsMutex;
        Thread* m_registeredThreads;
        WTF::ThreadSpecificKey m_threadSpecific;
#if !ASSERT_DISABLED
        Heap* m_heap;
#endif
    };

} // namespace JSC

#if COMPILER(GCC)
#define REGISTER_BUFFER_ALIGNMENT __attribute__ ((aligned (sizeof(void*))))
#else
#define REGISTER_BUFFER_ALIGNMENT
#endif

// ALLOCATE_AND_GET_REGISTER_STATE() is a macro so that it is always "inlined" even in debug builds.
#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4611)
#define ALLOCATE_AND_GET_REGISTER_STATE(registers) \
    MachineThreads::RegisterState registers REGISTER_BUFFER_ALIGNMENT; \
    setjmp(registers)
#pragma warning(pop)
#else
#define ALLOCATE_AND_GET_REGISTER_STATE(registers) \
    MachineThreads::RegisterState registers REGISTER_BUFFER_ALIGNMENT; \
    setjmp(registers)
#endif

#endif // MachineThreads_h
