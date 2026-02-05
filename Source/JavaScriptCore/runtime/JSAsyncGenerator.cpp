/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "JSAsyncGenerator.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSPromise.h"
#include "JSPromiseReaction.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo JSAsyncGenerator::s_info = { "AsyncGenerator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAsyncGenerator) };

JSAsyncGenerator* JSAsyncGenerator::create(VM& vm, Structure* structure)
{
    JSAsyncGenerator* generator = new (NotNull, allocateCell<JSAsyncGenerator>(vm)) JSAsyncGenerator(vm, structure);
    generator->finishCreation(vm);
    return generator;
}

JSAsyncGenerator* JSAsyncGenerator::createWithInitialValues(VM& vm, Structure* structure)
{
    JSAsyncGenerator* generator = new (NotNull, allocateCell<JSAsyncGenerator>(vm)) JSAsyncGenerator(vm, structure);
    generator->finishCreation(vm);
    return generator;
}

Structure* JSAsyncGenerator::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSAsyncGeneratorType, StructureFlags), info());
}

JSAsyncGenerator::JSAsyncGenerator(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSAsyncGenerator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    ASSERT(values.size() == numberOfInternalFields);
    for (unsigned index = 0; index < values.size(); ++index)
        internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSAsyncGenerator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSAsyncGenerator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSAsyncGenerator);

void JSAsyncGenerator::enqueue(VM& vm, JSValue value, int32_t mode, JSPromise* promise)
{
    if (isQueueEmpty()) {
        setResumeValue(vm, value);
        setResumeMode(mode);
        setResumePromise(vm, promise);
    } else {
        JSValue last = queue();
        if (last.isNull()) {
            JSPromiseReaction* item = JSPromiseReaction::create(
                vm,
                promise,
                value,
                jsNumber(mode),
                jsUndefined(), // Will be set to self (prev)
                nullptr // Will be set to self (next)
            );
            item->setNext(vm, item);
            item->setContext(vm, item);
            setQueue(vm, item);
        } else {
            JSPromiseReaction* tail = jsCast<JSPromiseReaction*>(last);
            JSPromiseReaction* head = tail->next();
            JSPromiseReaction* item = JSPromiseReaction::create(
                vm,
                promise,
                value,
                jsNumber(mode),
                tail, // prev = old tail
                head // next = head (to maintain circular)
            );
            tail->setNext(vm, item);
            head->setContext(vm, item);
            setQueue(vm, item);
        }
    }
}

std::tuple<JSValue, int32_t, JSPromise*> JSAsyncGenerator::dequeue(VM& vm)
{
    ASSERT(!isQueueEmpty());

    JSValue value = resumeValue();
    int32_t mode = resumeMode();
    JSPromise* promise = jsCast<JSPromise*>(resumePromise());

    JSValue last = queue();
    if (last.isNull()) {
        setResumeMode(static_cast<int32_t>(AsyncGeneratorResumeMode::Empty));
        setResumeValue(vm, jsUndefined());
        setResumePromise(vm, jsUndefined());
    } else {
        JSPromiseReaction* tail = jsCast<JSPromiseReaction*>(last);
        JSPromiseReaction* head = tail->next();

        setResumePromise(vm, head->promise());
        setResumeValue(vm, head->onFulfilled());
        setResumeMode(head->onRejected().asInt32());

        if (head == tail)
            setQueue(vm, jsNull());
        else {
            JSPromiseReaction* newHead = head->next();
            newHead->setContext(vm, tail); // newHead.prev = tail
            tail->setNext(vm, newHead);
        }
    }

    return { value, mode, promise };
}

} // namespace JSC
