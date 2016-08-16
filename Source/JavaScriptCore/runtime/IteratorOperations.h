/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#ifndef IteratorOperations_h
#define IteratorOperations_h

#include "JSCJSValue.h"
#include "JSObject.h"

namespace JSC {

JSValue iteratorNext(ExecState*, JSValue iterator, JSValue);
JSValue iteratorNext(ExecState*, JSValue iterator);
JS_EXPORT_PRIVATE JSValue iteratorValue(ExecState*, JSValue iterator);
bool iteratorComplete(ExecState*, JSValue iterator);
JS_EXPORT_PRIVATE JSValue iteratorStep(ExecState*, JSValue iterator);
JS_EXPORT_PRIVATE void iteratorClose(ExecState*, JSValue iterator);
JS_EXPORT_PRIVATE JSObject* createIteratorResultObject(ExecState*, JSValue, bool done);

Structure* createIteratorResultObjectStructure(VM&, JSGlobalObject&);

JS_EXPORT_PRIVATE JSValue iteratorForIterable(ExecState*, JSValue iterable);

template <typename CallBackType>
void forEachInIterable(ExecState* state, JSValue iterable, const CallBackType& callback)
{
    auto& vm = state->vm();
    JSValue iterator = iteratorForIterable(state, iterable);
    if (vm.exception())
        return;
    while (true) {
        JSValue next = iteratorStep(state, iterator);
        if (next.isFalse() || vm.exception())
            return;

        JSValue nextValue = iteratorValue(state, next);
        if (vm.exception())
            return;

        callback(vm, state, nextValue);
        if (vm.exception()) {
            iteratorClose(state, iterator);
            return;
        }
    }
}

}

#endif // !defined(IteratorOperations_h)
