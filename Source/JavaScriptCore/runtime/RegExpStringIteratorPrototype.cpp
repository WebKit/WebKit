/*
 * Copyright (C) 2019 Alexey Shvayka <shvaikalesh@gmail.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "RegExpStringIteratorPrototype.h"

#include "JSCInlines.h"
#include "JSRegExpStringIterator.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(regExpStringIteratorPrototypeFuncNext);

const ClassInfo RegExpStringIteratorPrototype::s_info = { "RegExp String Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RegExpStringIteratorPrototype) };

void RegExpStringIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->next, regExpStringIteratorPrototypeFuncNext, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, RegExpStringIteratorNextIntrinsic);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/ecma262/#sec-%regexpstringiteratorprototype%.next
JSC_DEFINE_HOST_FUNCTION(regExpStringIteratorPrototypeFuncNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    // 3. If O does not have all of the internal slots of a RegExp String Iterator Object Instance, throw a TypeError exception.
    JSValue thisValue = callFrame->thisValue();
    auto* iterator = dynamicDowncast<JSRegExpStringIterator>(thisValue);
    if (!iterator) [[unlikely]] {
        if (!thisValue.isObject())
            return throwVMTypeError(globalObject, scope, "%RegExpStringIteratorPrototype%.next requires |this| to be an Object"_s);
        return throwVMTypeError(globalObject, scope, "%RegExpStringIteratorPrototype%.next requires |this| to be a RegExp String Iterator instance"_s);
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(iterator->next(globalObject)));
}

} // namespace JSC
