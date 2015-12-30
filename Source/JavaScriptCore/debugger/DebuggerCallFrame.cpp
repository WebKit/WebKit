/*
 * Copyright (C) 2008, 2013, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DebuggerCallFrame.h"

#include "CodeBlock.h"
#include "DebuggerEvalEnabler.h"
#include "DebuggerScope.h"
#include "Interpreter.h"
#include "JSFunction.h"
#include "JSLexicalEnvironment.h"
#include "JSCInlines.h"
#include "Parser.h"
#include "StackVisitor.h"
#include "StrongInlines.h"

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

class FindCallerMidStackFunctor {
public:
    FindCallerMidStackFunctor(CallFrame* callFrame)
        : m_callFrame(callFrame)
        , m_callerFrame(nullptr)
    { }

    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        if (visitor->callFrame() == m_callFrame) {
            m_callerFrame = visitor->callerFrame();
            return StackVisitor::Done;
        }
        return StackVisitor::Continue;
    }

    CallFrame* getCallerFrame() const { return m_callerFrame; }

private:
    CallFrame* m_callFrame;
    CallFrame* m_callerFrame;
};

DebuggerCallFrame::DebuggerCallFrame(CallFrame* callFrame)
    : m_callFrame(callFrame)
{
    m_position = positionForCallFrame(m_callFrame);
}

RefPtr<DebuggerCallFrame> DebuggerCallFrame::callerFrame()
{
    ASSERT(isValid());
    if (!isValid())
        return 0;

    if (m_caller)
        return m_caller;

    FindCallerMidStackFunctor functor(m_callFrame);
    m_callFrame->vm().topCallFrame->iterate(functor);

    CallFrame* callerFrame = functor.getCallerFrame();
    if (!callerFrame)
        return nullptr;

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
    return m_callFrame->friendlyFunctionName();
}

DebuggerScope* DebuggerCallFrame::scope()
{
    ASSERT(isValid());
    if (!isValid())
        return 0;

    if (!m_scope) {
        VM& vm = m_callFrame->vm();
        JSScope* scope;
        CodeBlock* codeBlock = m_callFrame->codeBlock();
        if (codeBlock && codeBlock->scopeRegister().isValid())
            scope = m_callFrame->scope(codeBlock->scopeRegister().offset());
        else if (JSCallee* callee = jsDynamicCast<JSCallee*>(m_callFrame->callee()))
            scope = callee->scope();
        else
            scope = m_callFrame->lexicalGlobalObject();

        m_scope.set(vm, DebuggerScope::create(vm, scope));
    }
    return m_scope.get();
}

DebuggerCallFrame::Type DebuggerCallFrame::type() const
{
    ASSERT(isValid());
    if (!isValid())
        return ProgramType;

    if (jsDynamicCast<JSFunction*>(m_callFrame->callee()))
        return FunctionType;

    return ProgramType;
}

JSValue DebuggerCallFrame::thisValue() const
{
    ASSERT(isValid());
    return thisValueForCallFrame(m_callFrame);
}

// Evaluate some JavaScript code in the scope of this frame.
JSValue DebuggerCallFrame::evaluate(const String& script, NakedPtr<Exception>& exception)
{
    ASSERT(isValid());
    CallFrame* callFrame = m_callFrame;
    if (!callFrame)
        return jsNull();

    JSLockHolder lock(callFrame);

    if (!callFrame->codeBlock())
        return JSValue();
    
    DebuggerEvalEnabler evalEnabler(callFrame);
    VM& vm = callFrame->vm();
    auto& codeBlock = *callFrame->codeBlock();
    ThisTDZMode thisTDZMode = codeBlock.unlinkedCodeBlock()->constructorKind() == ConstructorKind::Derived ? ThisTDZMode::AlwaysCheck : ThisTDZMode::CheckIfNeeded;

    VariableEnvironment variablesUnderTDZ;
    JSScope::collectVariablesUnderTDZ(scope()->jsScope(), variablesUnderTDZ);

    EvalExecutable* eval = EvalExecutable::create(callFrame, makeSource(script), codeBlock.isStrictMode(), thisTDZMode, codeBlock.unlinkedCodeBlock()->derivedContextType(), codeBlock.unlinkedCodeBlock()->isArrowFunction(), &variablesUnderTDZ);
    if (vm.exception()) {
        exception = vm.exception();
        vm.clearException();
        return jsUndefined();
    }

    JSValue thisValue = thisValueForCallFrame(callFrame);
    JSValue result = vm.interpreter->execute(eval, callFrame, thisValue, scope()->jsScope());
    if (vm.exception()) {
        exception = vm.exception();
        vm.clearException();
    }
    ASSERT(result);
    return result;
}

void DebuggerCallFrame::invalidate()
{
    RefPtr<DebuggerCallFrame> frame = this;
    while (frame) {
        frame->m_callFrame = nullptr;
        if (frame->m_scope) {
            frame->m_scope->invalidateChain();
            frame->m_scope.clear();
        }
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
    return codeBlock->ownerScriptExecutable()->sourceID();
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
