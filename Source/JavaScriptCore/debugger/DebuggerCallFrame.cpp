/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
    LineAndColumnFunctor functor;
    m_callFrame->iterate(functor);
    m_line = functor.line();
    m_column = functor.column();
}

intptr_t DebuggerCallFrame::sourceId() const
{
    return m_callFrame->codeBlock()->ownerExecutable()->sourceID();
}

String DebuggerCallFrame::functionName() const
{
    if (!m_callFrame->codeBlock())
        return String();

    if (!m_callFrame->callee())
        return String();

    JSObject* function = m_callFrame->callee();
    if (!function || !function->inherits(JSFunction::info()))
        return String();
    return jsCast<JSFunction*>(function)->name(m_callFrame);
}
    
String DebuggerCallFrame::calculatedFunctionName() const
{
    if (!m_callFrame->codeBlock())
        return String();

    JSObject* function = m_callFrame->callee();

    if (!function)
        return String();

    return getCalculatedDisplayName(m_callFrame, function);
}

DebuggerCallFrame::Type DebuggerCallFrame::type() const
{
    if (m_callFrame->callee())
        return FunctionType;

    return ProgramType;
}

JSObject* DebuggerCallFrame::thisObject() const
{
    CodeBlock* codeBlock = m_callFrame->codeBlock();
    if (!codeBlock)
        return 0;

    JSValue thisValue = m_callFrame->uncheckedR(codeBlock->thisRegister().offset()).jsValue();
    if (!thisValue.isObject())
        return 0;

    return asObject(thisValue);
}

JSValue DebuggerCallFrame::evaluate(const String& script, JSValue& exception) const
{
    if (!m_callFrame->codeBlock())
        return JSValue();
    
    VM& vm = m_callFrame->vm();
    EvalExecutable* eval = EvalExecutable::create(m_callFrame, makeSource(script), m_callFrame->codeBlock()->isStrictMode());
    if (vm.exception()) {
        exception = vm.exception();
        vm.clearException();
    }

    JSValue result = vm.interpreter->execute(eval, m_callFrame, thisObject(), m_callFrame->scope());
    if (vm.exception()) {
        exception = vm.exception();
        vm.clearException();
    }
    ASSERT(result);
    return result;
}

void DebuggerCallFrame::clear()
{
    m_callFrame = 0;
}

} // namespace JSC
