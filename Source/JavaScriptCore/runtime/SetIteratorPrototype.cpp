/*
 * Copyright (C) 2013-2019 Apple, Inc. All rights reserved.
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
#include "SetIteratorPrototype.h"

#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSSetIteratorInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(setIteratorProtoFuncNext);

const ClassInfo SetIteratorPrototype::s_info = { "Set Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(SetIteratorPrototype) };

void SetIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->next, setIteratorProtoFuncNext, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, JSSetIteratorNextIntrinsic);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

JSC_DEFINE_HOST_FUNCTION(setIteratorProtoFuncNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* iterator = dynamicDowncast<JSSetIterator>(callFrame->thisValue());
    if (!iterator) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "%SetIteratorPrototype%.next requires that |this| be a Set Iterator instance"_s);

    JSValue value;
    bool hasNext = iterator->next(globalObject, value);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(createIteratorResultObject(globalObject, hasNext ? value : jsUndefined(), !hasNext));
}

}
