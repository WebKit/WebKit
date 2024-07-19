/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
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
    IterationStatus operator()(StackVisitor& visitor) const
    {
        m_lineColumn = visitor->computeLineAndColumn();
        return IterationStatus::Done;
    }

    unsigned line() const { return m_lineColumn.line; }
    unsigned column() const { return m_lineColumn.column; }

private:
    mutable LineColumn m_lineColumn;
};

Ref<DebuggerCallFrame> DebuggerCallFrame::create(VM& vm, CallFrame* callFrame)
{
    if (UNLIKELY(!callFrame)) {
        ShadowChicken::Frame emptyFrame;
        RELEASE_ASSERT(!emptyFrame.isTailDeleted);
        return adoptRef(*new DebuggerCallFrame(vm, callFrame, emptyFrame));
    }

    if (callFrame->isEmptyTopLevelCallFrameForDebugger()) {
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

JSGlobalObject* DebuggerCallFrame::globalObject(VM& vm)
{
    return scope(vm)->globalObject();
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

String DebuggerCallFrame::functionName(VM& vm) const
{
    ASSERT(isValid());
    if (!isValid())
        return String();

    if (isTailDeleted()) {
        if (JSFunction* func = jsDynamicCast<JSFunction*>(m_shadowChickenFrame.callee))
            return func->calculatedDisplayName(vm);
        return String::fromLatin1(m_shadowChickenFrame.codeBlock->inferredName().data());
    }

    return m_validMachineFrame->friendlyFunctionName();
}

DebuggerScope* DebuggerCallFrame::scope(VM& vm)
{
    ASSERT(isValid());
    if (!isValid())
        return nullptr;

    if (!m_scope) {
        JSScope* scope;
        CodeBlock* codeBlock = m_validMachineFrame->isNativeCalleeFrame() ? nullptr : m_validMachineFrame->codeBlock();
        if (isTailDeleted())
            scope = m_shadowChickenFrame.scope;
        else if (codeBlock && codeBlock->scopeRegister().isValid())
            scope = m_validMachineFrame->scope(codeBlock->scopeRegister().offset());
        else if (JSCallee* callee = jsDynamicCast<JSCallee*>(m_validMachineFrame->jsCallee()))
            scope = callee->scope();
        else
            scope = m_validMachineFrame->lexicalGlobalObject(vm)->globalLexicalEnvironment();

        m_scope.set(vm, DebuggerScope::create(vm, scope));
    }
    return m_scope.get();
}

DebuggerCallFrame::Type DebuggerCallFrame::type(VM&) const
{
    ASSERT(isValid());
    if (!isValid())
        return ProgramType;

    if (isTailDeleted())
        return FunctionType;

    if (jsDynamicCast<JSFunction*>(m_validMachineFrame->jsCallee()))
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
        codeBlock = m_validMachineFrame->isNativeCalleeFrame() ? nullptr : m_validMachineFrame->codeBlock();
    }

    if (!thisValue)
        return jsUndefined();

    ECMAMode ecmaMode = ECMAMode::sloppy();
    if (codeBlock && codeBlock->ownerExecutable()->isInStrictContext())
        ecmaMode = ECMAMode::strict();
    return thisValue.toThis(m_validMachineFrame->lexicalGlobalObject(vm), ecmaMode);
}

// Evaluate some JavaScript code in the scope of this frame.
JSValue DebuggerCallFrame::evaluateWithScopeExtension(VM& vm, const String& script, JSObject* scopeExtensionObject, NakedPtr<Exception>& exception)
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
                codeBlock = callFrame->isNativeCalleeFrame() ? nullptr : callFrame->codeBlock();
        }

        if (callFrame && codeBlock)
            break;

        debuggerCallFrame = debuggerCallFrame->m_caller.get();
    }

    if (!callFrame || !codeBlock)
        return jsUndefined();

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

    TDZEnvironment variablesUnderTDZ;
    PrivateNameEnvironment privateNameEnvironment;
    JSScope::collectClosureVariablesUnderTDZ(scope(vm)->jsScope(), variablesUnderTDZ, privateNameEnvironment);

    auto* eval = DirectEvalExecutable::create(globalObject, makeSource(script, callFrame->callerSourceOrigin(vm), SourceTaintedOrigin::Untainted), codeBlock->ownerExecutable()->lexicallyScopedFeatures(), codeBlock->unlinkedCodeBlock()->derivedContextType(), codeBlock->unlinkedCodeBlock()->needsClassFieldInitializer(), codeBlock->unlinkedCodeBlock()->privateBrandRequirement(), codeBlock->unlinkedCodeBlock()->isArrowFunction(), codeBlock->ownerExecutable()->isInsideOrdinaryFunction(), evalContextType, &variablesUnderTDZ, &privateNameEnvironment);
    if (UNLIKELY(catchScope.exception())) {
        exception = catchScope.exception();
        catchScope.clearException();
        return jsUndefined();
    }

    if (scopeExtensionObject) {
        JSScope* ignoredPreviousScope = globalObject->globalScope();
        globalObject->setGlobalScopeExtension(JSWithScope::create(vm, globalObject, ignoredPreviousScope, scopeExtensionObject));
        eval->setTaintedByWithScope();
    }

    JSValue result = vm.interpreter.executeEval(eval, debuggerCallFrame->thisValue(vm), debuggerCallFrame->scope(vm)->jsScope());
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
        if (std::optional<BytecodeIndex> bytecodeIndex = codeBlock->bytecodeIndexFromCallSiteIndex(m_shadowChickenFrame.callSiteIndex)) {
            auto lineColumn = codeBlock->lineColumnForBytecodeIndex(*bytecodeIndex);
            return TextPosition(OrdinalNumber::fromOneBasedInt(lineColumn.line),
                OrdinalNumber::fromOneBasedInt(lineColumn.column));
        }
    }

    return positionForCallFrame(vm, m_validMachineFrame);
}

TextPosition DebuggerCallFrame::positionForCallFrame(VM& vm, CallFrame* callFrame)
{
    LineAndColumnFunctor functor;
    if (!callFrame)
        return TextPosition(OrdinalNumber::fromOneBasedInt(0), OrdinalNumber::fromOneBasedInt(0));
    StackVisitor::visit(callFrame, vm, functor);
    return TextPosition(OrdinalNumber::fromOneBasedInt(functor.line()), OrdinalNumber::fromOneBasedInt(functor.column()));
}

SourceID DebuggerCallFrame::sourceIDForCallFrame(CallFrame* callFrame)
{
    if (!callFrame)
        return noSourceID;
    if (callFrame->isNativeCalleeFrame())
        return noSourceID;
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (!codeBlock)
        return noSourceID;
    return codeBlock->ownerExecutable()->sourceID();
}

} // namespace JSC
