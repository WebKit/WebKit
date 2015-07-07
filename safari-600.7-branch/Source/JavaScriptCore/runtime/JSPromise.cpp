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
#include "JSPromise.h"

#if ENABLE(PROMISES)

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromiseConstructor.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

static void triggerPromiseReactions(VM&, JSGlobalObject*, Vector<WriteBarrier<JSPromiseReaction>>&, JSValue);

const ClassInfo JSPromise::s_info = { "Promise", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSPromise) };

JSPromise* JSPromise::create(VM& vm, JSGlobalObject* globalObject, JSPromiseConstructor* constructor)
{
    JSPromise* promise = new (NotNull, allocateCell<JSPromise>(vm.heap)) JSPromise(vm, globalObject->promiseStructure());
    promise->finishCreation(vm, constructor);
    return promise;
}

Structure* JSPromise::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromise::JSPromise(VM& vm, Structure* structure)
    : JSDestructibleObject(vm, structure)
    , m_status(Status::Unresolved)
{
}

void JSPromise::finishCreation(VM& vm, JSPromiseConstructor* constructor)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    
    m_constructor.set(vm, this, constructor);
}

void JSPromise::destroy(JSCell* cell)
{
    static_cast<JSPromise*>(cell)->JSPromise::~JSPromise();
}

void JSPromise::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSPromise* thisObject = jsCast<JSPromise*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_result);
    visitor.append(&thisObject->m_constructor);
    visitor.append(thisObject->m_resolveReactions.begin(), thisObject->m_resolveReactions.end());
    visitor.append(thisObject->m_rejectReactions.begin(), thisObject->m_rejectReactions.end());
}

void JSPromise::reject(VM& vm, JSValue reason)
{
    // 1. If the value of promise's internal slot [[PromiseStatus]] is not "unresolved", return.
    if (m_status != Status::Unresolved)
        return;

    DeferGC deferGC(vm.heap);

    // 2. Let 'reactions' be the value of promise's [[RejectReactions]] internal slot.
    Vector<WriteBarrier<JSPromiseReaction>> reactions;
    reactions.swap(m_rejectReactions);

    // 3. Set the value of promise's [[Result]] internal slot to reason.
    m_result.set(vm, this, reason);

    // 4. Set the value of promise's [[ResolveReactions]] internal slot to undefined.
    m_resolveReactions.clear();

    // 5. Set the value of promise's [[RejectReactions]] internal slot to undefined.
    // NOTE: Handled by the swap above.

    // 6. Set the value of promise's [[PromiseStatus]] internal slot to "has-rejection".
    m_status = Status::HasRejection;

    // 7. Return the result of calling TriggerPromiseReactions(reactions, reason).
    triggerPromiseReactions(vm, globalObject(), reactions, reason);
}

void JSPromise::resolve(VM& vm, JSValue resolution)
{
    // 1. If the value of promise's internal slot [[PromiseStatus]] is not "unresolved", return.
    if (m_status != Status::Unresolved)
        return;

    DeferGC deferGC(vm.heap);

    // 2. Let 'reactions' be the value of promise's [[ResolveReactions]] internal slot.
    Vector<WriteBarrier<JSPromiseReaction>> reactions;
    reactions.swap(m_resolveReactions);
    
    // 3. Set the value of promise's [[Result]] internal slot to resolution.
    m_result.set(vm, this, resolution);

    // 4. Set the value of promise's [[ResolveReactions]] internal slot to undefined.
    // NOTE: Handled by the swap above.

    // 5. Set the value of promise's [[RejectReactions]] internal slot to undefined.
    m_rejectReactions.clear();

    // 6. Set the value of promise's [[PromiseStatus]] internal slot to "has-resolution".
    m_status = Status::HasResolution;

    // 7. Return the result of calling TriggerPromiseReactions(reactions, resolution).
    triggerPromiseReactions(vm, globalObject(), reactions, resolution);
}

void JSPromise::appendResolveReaction(VM& vm, JSPromiseReaction* reaction)
{
    m_resolveReactions.append(WriteBarrier<JSPromiseReaction>(vm, this, reaction));
}

void JSPromise::appendRejectReaction(VM& vm, JSPromiseReaction* reaction)
{
    m_rejectReactions.append(WriteBarrier<JSPromiseReaction>(vm, this, reaction));
}

void triggerPromiseReactions(VM& vm, JSGlobalObject* globalObject, Vector<WriteBarrier<JSPromiseReaction>>& reactions, JSValue argument)
{
    // 1. Repeat for each reaction in reactions, in original insertion order
    for (auto& reaction : reactions) {
        // i. Call QueueMicrotask(ExecutePromiseReaction, (reaction, argument)).
        globalObject->queueMicrotask(createExecutePromiseReactionMicrotask(vm, reaction.get(), argument));
    }
    
    // 2. Return.
}

} // namespace JSC

#endif // ENABLE(PROMISES)
