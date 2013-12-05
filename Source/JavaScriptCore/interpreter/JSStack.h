/*
 * Copyright (C) 2008, 2009, 2013 Apple Inc. All rights reserved.
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

#ifndef JSStack_h
#define JSStack_h

#include "ExecutableAllocator.h"
#include "Register.h"
#include <wtf/Noncopyable.h>
#include <wtf/PageReservation.h>
#include <wtf/VMTags.h>

#define ENABLE_DEBUG_JSSTACK 0
#if !defined(NDEBUG) && !defined(ENABLE_DEBUG_JSSTACK)
#define ENABLE_DEBUG_JSSTACK 1
#endif

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
            CodeBlock = sizeof(CallerFrameAndPC) / sizeof(Register),
            ScopeChain,
            Callee,
            ArgumentCount,
            CallFrameHeaderSize,

            // The following entries are not part of the CallFrameHeader but are provided here as a convenience:
            ThisArgument = CallFrameHeaderSize,
            FirstArgument,
        };

        static const size_t defaultCapacity = 512 * 1024;
        static const size_t commitSize = 16 * 1024;
        // Allow 8k of excess registers before we start trying to reap the stack
        static const ptrdiff_t maxExcessCapacity = 8 * 1024;

        JSStack(VM&, size_t capacity = defaultCapacity);
        ~JSStack();
        
        void gatherConservativeRoots(ConservativeRoots&);
        void gatherConservativeRoots(ConservativeRoots&, JITStubRoutineSet&, CodeBlockSet&);
        void sanitizeStack();

        Register* getBaseOfStack() const
        {
            return highAddress() - 1;
        }

        size_t size() const { return highAddress() - lowAddress(); }

        bool grow(Register*);
        
        static size_t committedByteCount();
        static void initializeThreading();

        Register* getTopOfFrame(CallFrame*);
        Register* getStartOfFrame(CallFrame*);
        Register* getTopOfStack();

        bool entryCheck(class CodeBlock*, int);

        CallFrame* pushFrame(class CodeBlock*, JSScope*, int argsCount, JSObject* callee);

        void popFrame(CallFrame*);

        bool containsAddress(Register* address) { return (lowAddress() <= address && address <= highAddress()); }

        void enableErrorStackReserve();
        void disableErrorStackReserve();

#if ENABLE(DEBUG_JSSTACK)
        void installFence(CallFrame*, const char *function = "", int lineNo = 0);
        void validateFence(CallFrame*, const char *function = "", int lineNo = 0);
        static const int FenceSize = 4;
#else // !ENABLE(DEBUG_JSSTACK)
        void installFence(CallFrame*, const char* = "", int = 0) { }
        void validateFence(CallFrame*, const char* = "", int = 0) { }
#endif // !ENABLE(DEBUG_JSSTACK)

    private:
        Register* lowAddress() const
        {
            return m_end;
        }

        Register* highAddress() const
        {
            return reinterpret_cast_ptr<Register*>(static_cast<char*>(m_reservation.base()) + m_reservation.size());
        }

        Register* reservationEnd() const
        {
            char* reservationEnd = static_cast<char*>(m_reservation.base());
            return reinterpret_cast_ptr<Register*>(reservationEnd);
        }

#if ENABLE(DEBUG_JSSTACK)
        static JSValue generateFenceValue(size_t argIndex);
        void installTrapsAfterFrame(CallFrame*);
#else
        void installTrapsAfterFrame(CallFrame*) { }
#endif

        bool growSlowCase(Register*);
        void shrink(Register*);
        void releaseExcessCapacity();
        void addToCommittedByteCount(long);

        void updateStackLimit(Register* newEnd);

        VM& m_vm;
        Register* m_end;
        Register* m_commitEnd;
        Register* m_useableEnd;
        PageReservation m_reservation;
        CallFrame*& m_topCallFrame;
        Register* m_lastStackTop;

        friend class LLIntOffsetsExtractor;
    };

} // namespace JSC

#endif // JSStack_h
