/*
 * Copyright (C) 2013-2019 Apple, Inc. All rights reserved.
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

#include "config.h"
#include "StringIteratorPrototype.h"

#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSStringIteratorInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(stringIteratorProtoFuncNext);

const ClassInfo StringIteratorPrototype::s_info = { "String Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(StringIteratorPrototype) };

void StringIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->next, stringIteratorProtoFuncNext, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, JSStringIteratorNextIntrinsic);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

JSC_DEFINE_HOST_FUNCTION(stringIteratorProtoFuncNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* iterator = dynamicDowncast<JSStringIterator>(callFrame->thisValue());
    if (!iterator) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "%StringIteratorPrototype%.next requires that |this| be a String Iterator instance"_s);

    JSString* value = iterator->nextWithAdvance(globalObject, vm);
    RETURN_IF_EXCEPTION(scope, { });
    if (!value)
        return JSValue::encode(createIteratorResultObject(globalObject, jsUndefined(), true));
    return JSValue::encode(createIteratorResultObject(globalObject, value, false));
}

} // namespace JSC
