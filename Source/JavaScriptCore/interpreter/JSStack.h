/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

    class JSStack {
        WTF_MAKE_NONCOPYABLE(JSStack);
    public:
        enum CallFrameHeaderEntry {
            CallFrameHeaderSize = 6,

            ArgumentCount = 6,
            CallerFrame = 5,
            Callee = 4,
            ScopeChain = 3,
            ReturnPC = 2, // This is either an Instruction* or a pointer into JIT generated code stored as an Instruction*.
            CodeBlock = 1,
        };

        static const size_t defaultCapacity = 512 * 1024;
        static const size_t commitSize = 16 * 1024;
        // Allow 8k of excess registers before we start trying to reap the stack
        static const ptrdiff_t maxExcessCapacity = 8 * 1024;

        JSStack(VM&, size_t capacity = defaultCapacity);
        ~JSStack();
        
        void gatherConservativeRoots(ConservativeRoots&);
        void gatherConservativeRoots(ConservativeRoots&, JITStubRoutineSet&, CodeBlockSet&);

        Register* getBaseOfStack() const
        {
            return highAddress() - 1;
        }

        Register* getLimitOfStack() const { return m_end; }
        size_t size() const { return highAddress() - lowAddress(); }

        bool grow(Register*);
        
        static size_t committedByteCount();
        static void initializeThreading();

        Register* const * addressOfEnd() const
        {
            return &m_end;
        }

        Register* getTopOfFrame(CallFrame*);
        Register* getStartOfFrame(CallFrame*);
        Register* getTopOfStack();
        Register* end() const { return m_end; }

        CallFrame* pushFrame(CallFrame* callerFrame, class CodeBlock*,
            JSScope*, int argsCount, JSObject* callee);

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

        Register* m_end;
        Register* m_commitEnd;
        Register* m_useableEnd;
        PageReservation m_reservation;
        CallFrame*& m_topCallFrame;

        friend class LLIntOffsetsExtractor;
    };

    inline void JSStack::shrink(Register* newEnd)
    {
        if (newEnd >= m_end)
            return;
        m_end = newEnd;
        if (m_end == getBaseOfStack() && (m_commitEnd - getBaseOfStack()) >= maxExcessCapacity)
            releaseExcessCapacity();
    }

    inline bool JSStack::grow(Register* newEnd)
    {
        if (newEnd >= m_end)
            return true;
        return growSlowCase(newEnd);
    }

} // namespace JSC

#endif // JSStack_h
