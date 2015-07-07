/*
 * Copyright (C) 2008, 2009, 2013, 2014 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#ifndef JSStack_h
#define JSStack_h

#include "ExecutableAllocator.h"
#include "Register.h"
#include <wtf/Noncopyable.h>
#include <wtf/PageReservation.h>
#include <wtf/VMTags.h>

namespace JSC {

    class CodeBlockSet;
    class ConservativeRoots;
    class ExecState;
    class JITStubRoutineSet;
    class VM;
    class LLIntOffsetsExtractor;

    struct Instruction;
    typedef ExecState CallFrame;

    struct CallerFrameAndPC {
        CallFrame* callerFrame;
        Instruction* pc;
    };

    class JSStack {
        WTF_MAKE_NONCOPYABLE(JSStack);
    public:
        enum CallFrameHeaderEntry {
            CallerFrameAndPCSize = sizeof(CallerFrameAndPC) / sizeof(Register),
            CodeBlock = CallerFrameAndPCSize,
            ScopeChain,
            Callee,
            ArgumentCount,
            CallFrameHeaderSize,

            // The following entries are not part of the CallFrameHeader but are provided here as a convenience:
            ThisArgument = CallFrameHeaderSize,
            FirstArgument,
        };

        static const size_t commitSize = 16 * 1024;
        // Allow 8k of excess registers before we start trying to reap the stack
        static const ptrdiff_t maxExcessCapacity = 8 * 1024;

        JSStack(VM&);
        
        bool ensureCapacityFor(Register* newTopOfStack);

        bool containsAddress(Register* address) { return (lowAddress() <= address && address < highAddress()); }
        static size_t committedByteCount();

#if ENABLE(JIT)
        void gatherConservativeRoots(ConservativeRoots&) { }
        void gatherConservativeRoots(ConservativeRoots&, JITStubRoutineSet&, CodeBlockSet&) { }
        void sanitizeStack() { }
        static void initializeThreading() { }
#else
        ~JSStack();

        void gatherConservativeRoots(ConservativeRoots&);
        void gatherConservativeRoots(ConservativeRoots&, JITStubRoutineSet&, CodeBlockSet&);
        void sanitizeStack();

        Register* baseOfStack() const
        {
            return highAddress() - 1;
        }

        size_t size() const { return highAddress() - lowAddress(); }

        static void initializeThreading();

        void setReservedZoneSize(size_t);

        inline Register* topOfStack();
#endif // ENABLE(JIT)

    private:

#if !ENABLE(JIT)
        Register* lowAddress() const
        {
            return m_end + 1;
        }

        Register* highAddress() const
        {
            return reinterpret_cast_ptr<Register*>(static_cast<char*>(m_reservation.base()) + m_reservation.size());
        }
#else
        Register* lowAddress() const;
        Register* highAddress() const;
#endif // !ENABLE(JIT)

#if !ENABLE(JIT)
        inline Register* topOfFrameFor(CallFrame*);

        Register* reservationTop() const
        {
            char* reservationTop = static_cast<char*>(m_reservation.base());
            return reinterpret_cast_ptr<Register*>(reservationTop);
        }

        bool grow(Register* newTopOfStack);
        bool growSlowCase(Register* newTopOfStack);
        void shrink(Register* newTopOfStack);
        void releaseExcessCapacity();
        void addToCommittedByteCount(long);

        void setStackLimit(Register* newTopOfStack);
#endif // !ENABLE(JIT)

        VM& m_vm;
        CallFrame*& m_topCallFrame;
#if !ENABLE(JIT)
        Register* m_end;
        Register* m_commitTop;
        PageReservation m_reservation;
        Register* m_lastStackTop;
        ptrdiff_t m_reservedZoneSizeInRegisters;
#endif // !ENABLE(JIT)

        friend class LLIntOffsetsExtractor;
    };

} // namespace JSC

#endif // JSStack_h
