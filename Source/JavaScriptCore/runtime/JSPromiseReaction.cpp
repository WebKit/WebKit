/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSPromiseReaction.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSPromiseReaction::s_info = { "PromiseReaction"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromiseReaction) };

template<typename Visitor>
void JSPromiseReaction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<JSPromiseReaction>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_promise);
    visitor.appendUnbarriered(thisObject->m_next.pointer());
}

DEFINE_VISIT_CHILDREN(JSPromiseReaction);

JSValue JSPromiseReaction::tryGetContext(JSValue reactionsValue)
{
    if (auto* slim = dynamicDowncast<JSSlimPromiseReaction>(reactionsValue)) {
        if (slim->internalMicrotask() != InternalMicrotask::None)
            return slim->handlerOrContext();
        return { };
    }
    if (auto* full = dynamicDowncast<JSFullPromiseReaction>(reactionsValue))
        return full->context();
    return { };
}

// JSSlimPromiseReaction

const ClassInfo JSSlimPromiseReaction::s_info = { "SlimPromiseReaction"_s, &JSPromiseReaction::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSSlimPromiseReaction) };

JSSlimPromiseReaction* JSSlimPromiseReaction::create(VM& vm, JSValue promise, JSValue handler, bool isFulfill, JSPromiseReaction* next)
{
    JSSlimPromiseReaction* result = new (NotNull, allocateCell<JSSlimPromiseReaction>(vm)) JSSlimPromiseReaction(vm, vm.slimPromiseReactionStructure.get(), promise, handler, next, InternalMicrotask::None, isFulfill);
    result->finishCreation(vm);
    return result;
}

JSSlimPromiseReaction* JSSlimPromiseReaction::create(VM& vm, JSValue promise, InternalMicrotask task, JSValue context, JSPromiseReaction* next)
{
    ASSERT(task != InternalMicrotask::None);
    JSSlimPromiseReaction* result = new (NotNull, allocateCell<JSSlimPromiseReaction>(vm)) JSSlimPromiseReaction(vm, vm.slimPromiseReactionStructure.get(), promise, context, next, task, false);
    result->finishCreation(vm);
    return result;
}

template<typename Visitor>
void JSSlimPromiseReaction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<JSSlimPromiseReaction>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_handlerOrContext);
}

Structure* JSSlimPromiseReaction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSSlimPromiseReactionType, StructureFlags), info());
}

DEFINE_VISIT_CHILDREN(JSSlimPromiseReaction);

// JSFullPromiseReaction

const ClassInfo JSFullPromiseReaction::s_info = { "FullPromiseReaction"_s, &JSPromiseReaction::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSFullPromiseReaction) };

JSFullPromiseReaction* JSFullPromiseReaction::create(VM& vm, JSValue promise, JSValue onFulfilled, JSValue onRejected, JSValue context, JSPromiseReaction* next)
{
    JSFullPromiseReaction* result = new (NotNull, allocateCell<JSFullPromiseReaction>(vm)) JSFullPromiseReaction(vm, vm.fullPromiseReactionStructure.get(), promise, onFulfilled, onRejected, context, next);
    result->finishCreation(vm);
    return result;
}

template<typename Visitor>
void JSFullPromiseReaction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<JSFullPromiseReaction>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_onFulfilled);
    visitor.append(thisObject->m_onRejected);
    visitor.append(thisObject->m_context);
}

Structure* JSFullPromiseReaction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFullPromiseReactionType, StructureFlags), info());
}

DEFINE_VISIT_CHILDREN(JSFullPromiseReaction);

} // namespace JSC
