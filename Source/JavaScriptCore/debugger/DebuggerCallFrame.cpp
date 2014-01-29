/*
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DebuggerCallFrame.h"

#include "JSFunction.h"
#include "CodeBlock.h"
#include "Interpreter.h"
#include "Operations.h"
#include "Parser.h"
#include "StackVisitor.h"

namespace JSC {

class LineAndColumnFunctor {
public:
    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        visitor->computeLineAndColumn(m_line, m_column);
        return StackVisitor::Done;
    }

    unsigned line() const { return m_line; }
    unsigned column() const { return m_column; }

private:
    unsigned m_line;
    unsigned m_column;
};

DebuggerCallFrame::DebuggerCallFrame(CallFrame* callFrame)
    : m_callFrame(callFrame)
{
    m_position = positionForCallFrame(m_callFrame);
}

PassRefPtr<DebuggerCallFrame> DebuggerCallFrame::callerFrame()
{
    ASSERT(isValid());
    if (!isValid())
        return 0;

    if (m_caller)
        return m_caller;

    CallFrame* callerFrame = m_callFrame->callerFrameSkippingVMEntrySentinel();
    if (!callerFrame)
        return 0;

    m_caller = DebuggerCallFrame::create(callerFrame);
    return m_caller;
}

JSC::JSGlobalObject* DebuggerCallFrame::vmEntryGlobalObject() const
{
    ASSERT(isValid());
    if (!isValid())
        return 0;
    return m_callFrame->vmEntryGlobalObject();
}

SourceID DebuggerCallFrame::sourceID() const
{
    ASSERT(isValid());
    if (!isValid())
        return noSourceID;
    return sourceIDForCallFrame(m_callFrame);
}

String DebuggerCallFrame::functionName() const
{
    ASSERT(isValid());
    if (!isValid())
        return String();
    JSObject* function = m_callFrame->callee();
    if (!function)
        return String();

    return getCalculatedDisplayName(m_callFrame, function);
}

JSScope* DebuggerCallFrame::scope() const
{
    ASSERT(isValid());
    if (!isValid())
        return 0;
    return m_callFrame->scope();
}

DebuggerCallFrame::Type DebuggerCallFrame::type() const
{
    ASSERT(isValid());
    if (!isValid())
        return ProgramType;

    if (m_callFrame->callee())
        return FunctionType;

    return ProgramType;
}

JSValue DebuggerCallFrame::thisValue() const
{
    ASSERT(isValid());
    return thisValueForCallFrame(m_callFrame);
}

// Evaluate some JavaScript code in the scope of this frame.
JSValue DebuggerCallFrame::evaluate(const String& script, JSValue& exception) const
{
    ASSERT(isValid());
    return evaluateWithCallFrame(m_callFrame, script, exception);
}

JSValue DebuggerCallFrame::evaluateWithCallFrame(CallFrame* callFrame, const String& script, JSValue& exception)
{
    if (!callFrame)
        return jsNull();

    JSLockHolder lock(callFrame);

    if (!callFrame->codeBlock())
        return JSValue();
    
    VM& vm = callFrame->vm();
    EvalExecutable* eval = EvalExecutable::create(callFrame, makeSource(script), callFrame->codeBlock()->isStrictMode());
    if (vm.exception()) {
        exception = vm.exception();
        vm.clearException();
        return jsUndefined();
    }

    JSValue thisValue = thisValueForCallFrame(callFrame);
    JSValue result = vm.interpreter->execute(eval, callFrame, thisValue, callFrame->scope());
    if (vm.exception()) {
        exception = vm.exception();
        vm.clearException();
    }
    ASSERT(result);
    return result;
}

void DebuggerCallFrame::invalidate()
{
    m_callFrame = nullptr;
    RefPtr<DebuggerCallFrame> frame = m_caller.release();
    while (frame) {
        frame->m_callFrame = nullptr;
        frame = frame->m_caller.release();
    }
}

TextPosition DebuggerCallFrame::positionForCallFrame(CallFrame* callFrame)
{
    if (!callFrame)
        return TextPosition();

    LineAndColumnFunctor functor;
    callFrame->iterate(functor);
    return TextPosition(OrdinalNumber::fromOneBasedInt(functor.line()), OrdinalNumber::fromOneBasedInt(functor.column()));
}

SourceID DebuggerCallFrame::sourceIDForCallFrame(CallFrame* callFrame)
{
    ASSERT(callFrame);
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (!codeBlock)
        return noSourceID;
    return codeBlock->ownerExecutable()->sourceID();
}

JSValue DebuggerCallFrame::thisValueForCallFrame(CallFrame* callFrame)
{
    if (!callFrame)
        return jsNull();

    ECMAMode ecmaMode = NotStrictMode;
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (codeBlock && codeBlock->isStrictMode())
        ecmaMode = StrictMode;
    JSValue thisValue = callFrame->thisValue().toThis(callFrame, ecmaMode);
    return thisValue;
}

} // namespace JSC
