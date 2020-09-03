/*
 * Copyright (C) 2008, 2013-2014, 2016 Apple Inc. All rights reserved.
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

#include "CatchScope.h"
#include "CodeBlock.h"
#include "DebuggerEvalEnabler.h"
#include "DebuggerScope.h"
#include "Interpreter.h"
#include "JSFunction.h"
#include "JSWithScope.h"
#include "ShadowChickenInlines.h"
#include "StackVisitor.h"
#include "StrongInlines.h"

namespace JSC {

class LineAndColumnFunctor {
public:
    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        visitor->computeLineAndColumn(m_line, m_column);
        return StackVisitor::Done;
    }

    unsigned line() const { return m_line; }
    unsigned column() const { return m_column; }

private:
    mutable unsigned m_line { 0 };
    mutable unsigned m_column { 0 };
};

Ref<DebuggerCallFrame> DebuggerCallFrame::create(VM& vm, CallFrame* callFrame)
{
    if (UNLIKELY(!callFrame)) {
        ShadowChicken::Frame emptyFrame;
        RELEASE_ASSERT(!emptyFrame.isTailDeleted);
        return adoptRef(*new DebuggerCallFrame(vm, callFrame, emptyFrame));
    }

    if (callFrame->isDeprecatedCallFrameForDebugger()) {
        ShadowChicken::Frame emptyFrame;
        RELEASE_ASSERT(!emptyFrame.isTailDeleted);
        return adoptRef(*new DebuggerCallFrame(vm, callFrame, emptyFrame));
    }

    Vector<ShadowChicken::Frame> frames;
    vm.ensureShadowChicken();
    vm.shadowChicken()->iterate(vm, callFrame, [&] (const ShadowChicken::Frame& frame) -> bool {
        frames.append(frame);
        return true;
    });

    RELEASE_ASSERT(frames.size());
    ASSERT(!frames[0].isTailDeleted); // The top frame should never be tail deleted.

    RefPtr<DebuggerCallFrame> currentParent = nullptr;
    // This walks the stack from the entry stack frame to the top of the stack.
    for (unsigned i = frames.size(); i--; ) {
        const ShadowChicken::Frame& frame = frames[i];
        if (!frame.isTailDeleted)
            callFrame = frame.frame;
        Ref<DebuggerCallFrame> currentFrame = adoptRef(*new DebuggerCallFrame(vm, callFrame, frame));
        currentFrame->m_caller = currentParent;
        currentParent = WTFMove(currentFrame);
    }
    return *currentParent;
}

DebuggerCallFrame::DebuggerCallFrame(VM& vm, CallFrame* callFrame, const ShadowChicken::Frame& frame)
    : m_validMachineFrame(callFrame)
    , m_shadowChickenFrame(frame)
{
    m_position = currentPosition(vm);
}

RefPtr<DebuggerCallFrame> DebuggerCallFrame::callerFrame()
{
    ASSERT(isValid());
    if (!isValid())
        return nullptr;

    return m_caller;
}

JSGlobalObject* DebuggerCallFrame::globalObject()
{
    return scope()->globalObject();
}

JSC::JSGlobalObject* DebuggerCallFrame::deprecatedVMEntryGlobalObject() const
{
    ASSERT(isValid());
    if (!isValid())
        return nullptr;
    VM& vm = m_validMachineFrame->deprecatedVM();
    return vm.deprecatedVMEntryGlobalObject(m_validMachineFrame->lexicalGlobalObject(vm));
}

SourceID DebuggerCallFrame::sourceID() const
{
    ASSERT(isValid());
    if (!isValid())
        return noSourceID;
    if (isTailDeleted())
        return m_shadowChickenFrame.codeBlock->ownerExecutable()->sourceID();
    return sourceIDForCallFrame(m_validMachineFrame);
}

String DebuggerCallFrame::functionName() const
{
    ASSERT(isValid());
    if (!isValid())
        return String();

    VM& vm = m_validMachineFrame->deprecatedVM();
    if (isTailDeleted()) {
        if (JSFunction* func = jsDynamicCast<JSFunction*>(vm, m_shadowChickenFrame.callee))
            return func->calculatedDisplayName(vm);
        return m_shadowChickenFrame.codeBlock->inferredName().data();
    }

    return m_validMachineFrame->friendlyFunctionName();
}

DebuggerScope* DebuggerCallFrame::scope()
{
    ASSERT(isValid());
    if (!isValid())
        return nullptr;

    if (!m_scope) {
        VM& vm = m_validMachineFrame->deprecatedVM();
        JSScope* scope;
        CodeBlock* codeBlock = m_validMachineFrame->codeBlock();
        if (isTailDeleted())
            scope = m_shadowChickenFrame.scope;
        else if (codeBlock && codeBlock->scopeRegister().isValid())
            scope = m_validMachineFrame->scope(codeBlock->scopeRegister().offset());
        else if (JSCallee* callee = jsDynamicCast<JSCallee*>(vm, m_validMachineFrame->jsCallee()))
            scope = callee->scope();
        else
            scope = m_validMachineFrame->lexicalGlobalObject(vm)->globalLexicalEnvironment();

        m_scope.set(vm, DebuggerScope::create(vm, scope));
    }
    return m_scope.get();
}

DebuggerCallFrame::Type DebuggerCallFrame::type() const
{
    ASSERT(isValid());
    if (!isValid())
        return ProgramType;

    if (isTailDeleted())
        return FunctionType;

    if (jsDynamicCast<JSFunction*>(m_validMachineFrame->deprecatedVM(), m_validMachineFrame->jsCallee()))
        return FunctionType;

    return ProgramType;
}

JSValue DebuggerCallFrame::thisValue(VM& vm) const
{
    ASSERT(isValid());
    if (!isValid())
        return jsUndefined();

    CodeBlock* codeBlock = nullptr;
    JSValue thisValue;
    if (isTailDeleted()) {
        thisValue = m_shadowChickenFrame.thisValue;
        codeBlock = m_shadowChickenFrame.codeBlock;
    } else {
        thisValue = m_validMachineFrame->thisValue();
        codeBlock = m_validMachineFrame->codeBlock();
    }

    if (!thisValue)
        return jsUndefined();

    ECMAMode ecmaMode = ECMAMode::sloppy();
    if (codeBlock && codeBlock->ownerExecutable()->isInStrictContext())
        ecmaMode = ECMAMode::strict();
    return thisValue.toThis(m_validMachineFrame->lexicalGlobalObject(vm), ecmaMode);
}

// Evaluate some JavaScript code in the scope of this frame.
JSValue DebuggerCallFrame::evaluateWithScopeExtension(const String& script, JSObject* scopeExtensionObject, NakedPtr<Exception>& exception)
{
    CallFrame* callFrame = nullptr;
    CodeBlock* codeBlock = nullptr;

    auto* debuggerCallFrame = this;
    while (debuggerCallFrame) {
        ASSERT(debuggerCallFrame->isValid());

        callFrame = debuggerCallFrame->m_validMachineFrame;
        if (callFrame) {
            if (debuggerCallFrame->isTailDeleted())
                codeBlock = debuggerCallFrame->m_shadowChickenFrame.codeBlock;
            else
                codeBlock = callFrame->codeBlock();
        }

        if (callFrame && codeBlock)
            break;

        debuggerCallFrame = debuggerCallFrame->m_caller.get();
    }

    if (!callFrame || !codeBlock)
        return jsUndefined();

    VM& vm = callFrame->deprecatedVM();
    JSLockHolder lock(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    
    JSGlobalObject* globalObject = codeBlock->globalObject();
    DebuggerEvalEnabler evalEnabler(globalObject, DebuggerEvalEnabler::Mode::EvalOnGlobalObjectAtDebuggerEntry);

    EvalContextType evalContextType;
    
    if (isFunctionParseMode(codeBlock->unlinkedCodeBlock()->parseMode()))
        evalContextType = EvalContextType::FunctionEvalContext;
    else if (codeBlock->unlinkedCodeBlock()->codeType() == EvalCode)
        evalContextType = codeBlock->unlinkedCodeBlock()->evalContextType();
    else 
        evalContextType = EvalContextType::None;

    VariableEnvironment variablesUnderTDZ;
    JSScope::collectClosureVariablesUnderTDZ(scope()->jsScope(), variablesUnderTDZ);

    ECMAMode ecmaMode = codeBlock->ownerExecutable()->isInStrictContext() ? ECMAMode::strict() : ECMAMode::sloppy();
    auto* eval = DirectEvalExecutable::create(globalObject, makeSource(script, callFrame->callerSourceOrigin(vm)), codeBlock->unlinkedCodeBlock()->derivedContextType(), codeBlock->unlinkedCodeBlock()->needsClassFieldInitializer(), codeBlock->unlinkedCodeBlock()->isArrowFunction(), codeBlock->ownerExecutable()->isInsideOrdinaryFunction(), evalContextType, &variablesUnderTDZ, ecmaMode);
    if (UNLIKELY(catchScope.exception())) {
        exception = catchScope.exception();
        catchScope.clearException();
        return jsUndefined();
    }

    if (scopeExtensionObject) {
        JSScope* ignoredPreviousScope = globalObject->globalScope();
        globalObject->setGlobalScopeExtension(JSWithScope::create(vm, globalObject, ignoredPreviousScope, scopeExtensionObject));
    }

    JSValue result = vm.interpreter->execute(eval, globalObject, debuggerCallFrame->thisValue(vm), debuggerCallFrame->scope()->jsScope());
    if (UNLIKELY(catchScope.exception())) {
        exception = catchScope.exception();
        catchScope.clearException();
    }

    if (scopeExtensionObject)
        globalObject->clearGlobalScopeExtension();

    ASSERT(result);
    return result;
}

void DebuggerCallFrame::invalidate()
{
    RefPtr<DebuggerCallFrame> frame = this;
    while (frame) {
        frame->m_validMachineFrame = nullptr;
        if (frame->m_scope) {
            frame->m_scope->invalidateChain();
            frame->m_scope.clear();
        }
        frame = WTFMove(frame->m_caller);
    }
}

TextPosition DebuggerCallFrame::currentPosition(VM& vm)
{
    if (!m_validMachineFrame)
        return TextPosition();

    if (isTailDeleted()) {
        CodeBlock* codeBlock = m_shadowChickenFrame.codeBlock;
        if (Optional<BytecodeIndex> bytecodeIndex = codeBlock->bytecodeIndexFromCallSiteIndex(m_shadowChickenFrame.callSiteIndex)) {
            return TextPosition(OrdinalNumber::fromOneBasedInt(codeBlock->lineNumberForBytecodeIndex(*bytecodeIndex)),
                OrdinalNumber::fromOneBasedInt(codeBlock->columnNumberForBytecodeIndex(*bytecodeIndex)));
        }
    }

    return positionForCallFrame(vm, m_validMachineFrame);
}

TextPosition DebuggerCallFrame::positionForCallFrame(VM& vm, CallFrame* callFrame)
{
    LineAndColumnFunctor functor;
    StackVisitor::visit(callFrame, vm, functor);
    return TextPosition(OrdinalNumber::fromOneBasedInt(functor.line()), OrdinalNumber::fromOneBasedInt(functor.column()));
}

SourceID DebuggerCallFrame::sourceIDForCallFrame(CallFrame* callFrame)
{
    if (!callFrame)
        return noSourceID;
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (!codeBlock || callFrame->callee().isWasm())
        return noSourceID;
    return codeBlock->ownerExecutable()->sourceID();
}

} // namespace JSC
