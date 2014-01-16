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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSPromiseReaction.h"

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "JSPromiseDeferred.h"
#include "Microtask.h"
#include "SlotVisitorInlines.h"
#include "StrongInlines.h"

namespace JSC {

class ExecutePromiseReactionMicrotask FINAL : public Microtask {
public:
    ExecutePromiseReactionMicrotask(VM& vm, JSPromiseReaction* reaction, JSValue argument)
    {
        m_reaction.set(vm, reaction);
        m_argument.set(vm, argument);
    }

    virtual ~ExecutePromiseReactionMicrotask()
    {
    }

private:
    virtual void run(ExecState*) override;

    Strong<JSPromiseReaction> m_reaction;
    Strong<Unknown> m_argument;
};

PassRefPtr<Microtask> createExecutePromiseReactionMicrotask(VM& vm, JSPromiseReaction* reaction, JSValue argument)
{
    return adoptRef(new ExecutePromiseReactionMicrotask(vm, reaction, argument));
}

void ExecutePromiseReactionMicrotask::run(ExecState* exec)
{
    // 1. Let 'deferred' be reaction.[[Deferred]].
    JSPromiseDeferred* deferred = m_reaction->deferred();
    
    // 2. Let 'handler' be reaction.[[Handler]].
    JSValue handler = m_reaction->handler();

    // 3. Let 'handlerResult' be the result of calling the [[Call]] internal method of
    //    handler passing undefined as thisArgument and a List containing argument as
    //    argumentsList.

    CallData handlerCallData;
    CallType handlerCallType = getCallData(handler, handlerCallData);
    ASSERT(handlerCallType != CallTypeNone);

    MarkedArgumentBuffer handlerArguments;
    handlerArguments.append(m_argument.get());

    JSValue handlerResult = call(exec, handler, handlerCallType, handlerCallData, jsUndefined(), handlerArguments);

    // 4. If handlerResult is an abrupt completion, return the result of calling the
    //    [[Call]] internal method of deferred.[[Reject]] passing undefined as thisArgument
    //    and a List containing handlerResult.[[value]] as argumentsList.
    if (exec->hadException()) {
        JSValue exception = exec->exception();
        exec->clearException();

        performDeferredReject(exec, deferred, exception);
    }
    
    // 5. Let 'handlerResult' be handlerResult.[[value]].
    // Note: Nothing to do.

    // 6. If SameValue(handlerResult, deferred.[[Promise]]) is true,
    if (sameValue(exec, handlerResult, deferred->promise())) {
        // i. Let 'selfResolutionError' be a newly-created TypeError object.
        JSObject* selfResolutionError = createTypeError(exec, ASCIILiteral("Resolve a promise with itself"));

        // ii. Return the result of calling the [[Call]] internal method of deferred.[[Reject]] passing
        //     undefined as thisArgument and a List containing selfResolutionError as argumentsList.
        performDeferredReject(exec, deferred, selfResolutionError);
    }

    // 7. Let 'updateResult' be the result of calling UpdateDeferredFromPotentialThenable(handlerResult, deferred).
    ThenableStatus updateResult = updateDeferredFromPotentialThenable(exec, handlerResult, deferred);
    
    // 8. ReturnIfAbrupt(updateResult).
    if (exec->hadException())
        return;

    // 9. If 'updateResult' is "not a thenable",
    if (updateResult == NotAThenable) {
        // i. Return the result of calling the [[Call]] internal method of deferred.[[Resolve]]
        //    passing undefined as thisArgument and a List containing handlerResult as argumentsList.
        performDeferredResolve(exec, deferred, handlerResult);
    }
}


const ClassInfo JSPromiseReaction::s_info = { "JSPromiseReaction", 0, 0, 0, CREATE_METHOD_TABLE(JSPromiseReaction) };

JSPromiseReaction* JSPromiseReaction::create(VM& vm, JSPromiseDeferred* deferred, JSValue handler)
{
    JSPromiseReaction* promiseReaction = new (NotNull, allocateCell<JSPromiseReaction>(vm.heap)) JSPromiseReaction(vm);
    promiseReaction->finishCreation(vm, deferred, handler);
    return promiseReaction;
}

JSPromiseReaction::JSPromiseReaction(VM& vm)
    : Base(vm, vm.promiseReactionStructure.get())
{
}

void JSPromiseReaction::finishCreation(VM& vm, JSPromiseDeferred* deferred, JSValue handler)
{
    Base::finishCreation(vm);
    m_deferred.set(vm, this, deferred);
    m_handler.set(vm, this, handler);
}

void JSPromiseReaction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSPromiseReaction* thisObject = jsCast<JSPromiseReaction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_deferred);
    visitor.append(&thisObject->m_handler);
}

} // namespace JSC
