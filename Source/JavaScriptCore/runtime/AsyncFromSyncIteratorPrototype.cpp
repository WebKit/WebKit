/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
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
#include "AsyncFromSyncIteratorPrototype.h"

#include "BuiltinNames.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSObject.h"

namespace JSC {

const ClassInfo AsyncFromSyncIteratorPrototype::s_info = { "AsyncFromSyncIterator", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AsyncFromSyncIteratorPrototype) };

AsyncFromSyncIteratorPrototype::AsyncFromSyncIteratorPrototype(VM& vm, Structure* structure)
    : JSC::JSNonFinalObject(vm, structure)
{
}

void AsyncFromSyncIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    didBecomePrototype();

    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("next", asyncFromSyncIteratorPrototypeNextCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("return", asyncFromSyncIteratorPrototypeReturnCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("throw", asyncFromSyncIteratorPrototypeThrowCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

AsyncFromSyncIteratorPrototype* AsyncFromSyncIteratorPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    AsyncFromSyncIteratorPrototype* prototype = new (NotNull, allocateCell<AsyncFromSyncIteratorPrototype>(vm.heap)) AsyncFromSyncIteratorPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    vm.heap.addFinalizer(prototype, destroy);

    return prototype;
}

} // namespace JSC
