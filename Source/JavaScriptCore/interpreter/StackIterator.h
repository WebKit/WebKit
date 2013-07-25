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

#include "CallFrame.h"
#include "Interpreter.h"
#include "StackIteratorPrivate.h"

namespace JSC {

class Arguments;

class StackIterator::Frame : public CallFrame {
public:
    enum CodeType {
        Global = StackFrameGlobalCode,
        Eval = StackFrameEvalCode,
        Function = StackFrameFunctionCode,
        Native = StackFrameNativeCode
    };

    static Frame* create(CallFrame* f) { return reinterpret_cast<Frame*>(f); }

    bool isJSFrame() const { return !!codeBlock(); }

    JS_EXPORT_PRIVATE String functionName();
    JS_EXPORT_PRIVATE String sourceURL();
    JS_EXPORT_PRIVATE String toString();

    CodeType codeType() const;
    unsigned bytecodeOffset();
    JS_EXPORT_PRIVATE void computeLineAndColumn(unsigned& line, unsigned& column);

    Arguments* arguments();
    CallFrame* callFrame() { return reinterpret_cast<CallFrame*>(this); }
    
    Frame* logicalFrame();
    Frame* logicalCallerFrame();

#ifndef NDEBUG
    JS_EXPORT_PRIVATE void print(int indentLevel);
#endif

private:
    Frame();
    ~Frame();

    void retrieveExpressionInfo(int& divot, int& startOffset, int& endOffset, unsigned& line, unsigned& column);
};

} // namespace JSC

#endif // StackIterator_h

