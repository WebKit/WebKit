/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "CalleeBits.h"
#include "WasmIndexOrName.h"
#include <wtf/Function.h>
#include <wtf/Indenter.h>
#include <wtf/text/WTFString.h>

namespace JSC {

struct EntryFrame;
struct InlineCallFrame;

class CodeBlock;
class CodeOrigin;
class ExecState;
class JSCell;
class JSFunction;
class ClonedArguments;
class Register;
class RegisterAtOffsetList;

typedef ExecState CallFrame;

class StackVisitor {
public:
    class Frame {
    public:
        enum CodeType {
            Global,
            Eval,
            Function,
            Module,
            Native,
            Wasm
        };

        size_t index() const { return m_index; }
        size_t argumentCountIncludingThis() const { return m_argumentCountIncludingThis; }
        bool callerIsEntryFrame() const { return m_callerIsEntryFrame; }
        CallFrame* callerFrame() const { return m_callerFrame; }
        CalleeBits callee() const { return m_callee; }
        CodeBlock* codeBlock() const { return m_codeBlock; }
        unsigned bytecodeOffset() const { return m_bytecodeOffset; }
        InlineCallFrame* inlineCallFrame() const {
#if ENABLE(DFG_JIT)
            return m_inlineCallFrame;
#else
            return nullptr;
#endif
        }

        bool isNativeFrame() const { return !codeBlock() && !isWasmFrame(); }
        bool isInlinedFrame() const { return !!inlineCallFrame(); }
        bool isWasmFrame() const { return m_isWasmFrame; }
        Wasm::IndexOrName const wasmFunctionIndexOrName()
        {
            ASSERT(isWasmFrame());
            return m_wasmFunctionIndexOrName;
        }

        JS_EXPORT_PRIVATE String functionName() const;
        JS_EXPORT_PRIVATE String sourceURL() const;
        JS_EXPORT_PRIVATE String toString() const;

        JS_EXPORT_PRIVATE intptr_t sourceID();

        CodeType codeType() const;
        bool hasLineAndColumnInfo() const;
        JS_EXPORT_PRIVATE void computeLineAndColumn(unsigned& line, unsigned& column) const;

#if ENABLE(ASSEMBLER)
        Optional<RegisterAtOffsetList> calleeSaveRegistersForUnwinding();
#endif

        ClonedArguments* createArguments();
        CallFrame* callFrame() const { return m_callFrame; }
        
        void dump(PrintStream&, Indenter = Indenter()) const;
        void dump(PrintStream&, Indenter, WTF::Function<void(PrintStream&)> prefix) const;

    private:
        Frame() { }
        ~Frame() { }

        void retrieveExpressionInfo(int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column) const;
        void setToEnd();

#if ENABLE(DFG_JIT)
        InlineCallFrame* m_inlineCallFrame;
#endif
        CallFrame* m_callFrame;
        EntryFrame* m_entryFrame;
        EntryFrame* m_callerEntryFrame;
        CallFrame* m_callerFrame;
        CalleeBits m_callee;
        CodeBlock* m_codeBlock;
        size_t m_index;
        size_t m_argumentCountIncludingThis;
        unsigned m_bytecodeOffset;
        bool m_callerIsEntryFrame : 1;
        bool m_isWasmFrame : 1;
        Wasm::IndexOrName m_wasmFunctionIndexOrName;

        friend class StackVisitor;
    };

    enum Status {
        Continue = 0,
        Done = 1
    };

    // StackVisitor::visit() expects a Functor that implements the following method:
    //     Status operator()(StackVisitor&) const;

    enum EmptyEntryFrameAction {
        ContinueIfTopEntryFrameIsEmpty,
        TerminateIfTopEntryFrameIsEmpty,
    };

    template <EmptyEntryFrameAction action = ContinueIfTopEntryFrameIsEmpty, typename Functor>
    static void visit(CallFrame* startFrame, VM* vm, const Functor& functor)
    {
        StackVisitor visitor(startFrame, vm);
        if (action == TerminateIfTopEntryFrameIsEmpty && visitor.topEntryFrameIsEmpty())
            return;
        while (visitor->callFrame()) {
            Status status = functor(visitor);
            if (status != Continue)
                break;
            visitor.gotoNextFrame();
        }
    }

    Frame& operator*() { return m_frame; }
    ALWAYS_INLINE Frame* operator->() { return &m_frame; }
    void unwindToMachineCodeBlockFrame();

    bool topEntryFrameIsEmpty() const { return m_topEntryFrameIsEmpty; }

private:
    JS_EXPORT_PRIVATE StackVisitor(CallFrame* startFrame, VM*);

    JS_EXPORT_PRIVATE void gotoNextFrame();

    void readFrame(CallFrame*);
    void readNonInlinedFrame(CallFrame*, CodeOrigin* = 0);
#if ENABLE(DFG_JIT)
    void readInlinedFrame(CallFrame*, CodeOrigin*);
#endif

    Frame m_frame;
    bool m_topEntryFrameIsEmpty { false };
};

class CallerFunctor {
public:
    CallerFunctor()
        : m_hasSkippedFirstFrame(false)
        , m_callerFrame(0)
    {
    }

    CallFrame* callerFrame() const { return m_callerFrame; }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (!m_hasSkippedFirstFrame) {
            m_hasSkippedFirstFrame = true;
            return StackVisitor::Continue;
        }

        m_callerFrame = visitor->callFrame();
        return StackVisitor::Done;
    }
    
private:
    mutable bool m_hasSkippedFirstFrame;
    mutable CallFrame* m_callerFrame;
};

} // namespace JSC
