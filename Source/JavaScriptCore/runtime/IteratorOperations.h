/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

#pragma once

#include "JSCJSValue.h"
#include "JSObject.h"
#include "ThrowScope.h"

namespace JSC {

struct IterationRecord {
    JSValue iterator;
    JSValue nextMethod;
};

JSValue iteratorNext(ExecState*, IterationRecord, JSValue argument = JSValue());
JS_EXPORT_PRIVATE JSValue iteratorValue(ExecState*, JSValue iterResult);
bool iteratorComplete(ExecState*, JSValue iterResult);
JS_EXPORT_PRIVATE JSValue iteratorStep(ExecState*, IterationRecord);
JS_EXPORT_PRIVATE void iteratorClose(ExecState*, IterationRecord);
JS_EXPORT_PRIVATE JSObject* createIteratorResultObject(ExecState*, JSValue, bool done);

Structure* createIteratorResultObjectStructure(VM&, JSGlobalObject&);

JS_EXPORT_PRIVATE JSValue iteratorMethod(ExecState&, JSObject*);
JS_EXPORT_PRIVATE IterationRecord iteratorForIterable(ExecState&, JSObject*, JSValue iteratorMethod);
JS_EXPORT_PRIVATE IterationRecord iteratorForIterable(ExecState*, JSValue iterable);

JS_EXPORT_PRIVATE JSValue iteratorMethod(ExecState&, JSObject*);
JS_EXPORT_PRIVATE bool hasIteratorMethod(ExecState&, JSValue);

template<typename CallBackType>
void forEachInIterable(ExecState* exec, JSValue iterable, const CallBackType& callback)
{
    auto& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    IterationRecord iterationRecord = iteratorForIterable(exec, iterable);
    RETURN_IF_EXCEPTION(scope, void());
    while (true) {
        JSValue next = iteratorStep(exec, iterationRecord);
        if (UNLIKELY(scope.exception()) || next.isFalse())
            return;

        JSValue nextValue = iteratorValue(exec, next);
        RETURN_IF_EXCEPTION(scope, void());

        callback(vm, exec, nextValue);
        if (UNLIKELY(scope.exception())) {
            scope.release();
            iteratorClose(exec, iterationRecord);
            return;
        }
    }
}

template<typename CallBackType>
void forEachInIterable(ExecState& state, JSObject* iterable, JSValue iteratorMethod, const CallBackType& callback)
{
    auto& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto iterationRecord = iteratorForIterable(state, iterable, iteratorMethod);
    RETURN_IF_EXCEPTION(scope, void());
    while (true) {
        JSValue next = iteratorStep(&state, iterationRecord);
        if (UNLIKELY(scope.exception()) || next.isFalse())
            return;

        JSValue nextValue = iteratorValue(&state, next);
        RETURN_IF_EXCEPTION(scope, void());

        callback(vm, state, nextValue);
        if (UNLIKELY(scope.exception())) {
            scope.release();
            iteratorClose(&state, iterationRecord);
            return;
        }
    }
}

} // namespace JSC
