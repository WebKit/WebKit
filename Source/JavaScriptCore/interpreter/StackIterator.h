/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef StackIterator_h
#define StackIterator_h

#include <wtf/text/WTFString.h>

namespace JSC {

struct CodeOrigin;
struct InlineCallFrame;

class Arguments;
class CodeBlock;
class ExecState;
class JSFunction;
class JSObject;
class JSScope;

typedef ExecState CallFrame;

class StackIterator {
public:
    class Frame {
    public:
        enum CodeType {
            Global,
            Eval,
            Function,
            Native
        };

        size_t index() const { return m_index; }
        size_t argumentCountIncludingThis() const { return m_argumentCountIncludingThis; }
        CallFrame* callerFrame() const { return m_callerFrame; }
        JSObject* callee() const { return m_callee; }
        JSScope* scope() const { return m_scope; }
        CodeBlock* codeBlock() const { return m_codeBlock; }
        unsigned bytecodeOffset() const { return m_bytecodeOffset; }
#if ENABLE(DFG_JIT)
        InlineCallFrame* inlineCallFrame() const { return m_inlineCallFrame; }
#endif

        bool isJSFrame() const { return !!codeBlock(); }
#if ENABLE(DFG_JIT)
        bool isInlinedFrame() const { return !!m_inlineCallFrame; }
#endif

        JS_EXPORT_PRIVATE String functionName();
        JS_EXPORT_PRIVATE String sourceURL();
        JS_EXPORT_PRIVATE String toString();

        CodeType codeType() const;
        JS_EXPORT_PRIVATE void computeLineAndColumn(unsigned& line, unsigned& column);

        Arguments* arguments();
        CallFrame* callFrame() const { return m_callFrame; }
    
#ifndef NDEBUG
        JS_EXPORT_PRIVATE void print(int indentLevel);
#endif

    private:
        Frame() { }
        ~Frame() { }

        void retrieveExpressionInfo(int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column);
        void setToEnd();

        size_t m_index;
        size_t m_argumentCountIncludingThis;
        CallFrame* m_callerFrame;
        JSObject* m_callee;
        JSScope* m_scope;
        CodeBlock* m_codeBlock;
        unsigned m_bytecodeOffset;
#if ENABLE(DFG_JIT)
        InlineCallFrame* m_inlineCallFrame;
#endif

        CallFrame* m_callFrame;

        friend class StackIterator;
    };

    typedef bool (*FrameFilter)(Frame*);

    enum Status {
        Continue = 0,
        Done = 1
    };

    // StackIterator::iterate() expects a Functor that implements the following method:
    //     Status operator()(StackIterator&);

    template <typename Functor> void iterate(Functor& functor)
    {
        while (m_frame.callFrame()) {
            Status status = functor(*this);
            if (status != Continue)
                break;
            gotoNextFrameWithFilter();
        }
    }

    JS_EXPORT_PRIVATE size_t numberOfFrames();

    Frame& operator*() { return m_frame; }
    ALWAYS_INLINE Frame* operator->() { return &m_frame; }

private:
    JS_EXPORT_PRIVATE StackIterator(CallFrame* startFrame, FrameFilter = 0);

    void gotoFrameAtIndex(size_t frameIndex);
    void gotoNextFrame();
    JS_EXPORT_PRIVATE void gotoNextFrameWithFilter();
    void resetIterator();

    void readFrame(CallFrame*);
    void readNonInlinedFrame(CallFrame*, CodeOrigin* = 0);
#if ENABLE(DFG_JIT)
    void readInlinedFrame(CallFrame*, CodeOrigin*);
#endif

    CallFrame* m_startFrame;
    FrameFilter m_filter;
    Frame m_frame;

    friend class ExecState;
};

} // namespace JSC

#endif // StackIterator_h

