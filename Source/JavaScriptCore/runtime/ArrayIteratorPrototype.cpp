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
#include "ArrayIteratorPrototype.h"

#include "ImplementationVisibility.h"
#include "IterationKind.h"
#include "IteratorOperations.h"
#include "JSArray.h"
#include "JSArrayBufferView.h"
#include "JSArrayIterator.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSObject.h"
#include "JSType.h"
#include "TypedArrayType.h"
#include <concepts>

namespace JSC {

const ClassInfo ArrayIteratorPrototype::s_info = { "Array Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ArrayIteratorPrototype) };

void ArrayIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->next, arrayIteratorProtoNext, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

JSC_DEFINE_HOST_FUNCTION(arrayIteratorProtoNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    if (!thisValue.isCell() || thisValue.asCell()->type() != JSArrayIteratorType) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "%ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance"_s);

    JSArrayIterator* iterator = jsCast<JSArrayIterator*>(thisValue);
    JSObject* array = iterator->iteratedObject();
    ASSERT(array);

    bool isTypedArray = isTypedView(array->type());
    if (isTypedArray && jsCast<JSArrayBufferView*>(array)->isDetached()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Underlying ArrayBuffer has been detached from the view or out-of-bounds"_s);

    int64_t index = iterator->index();

    // This `unlikely` annotation assumes that the closed iterator's `next()` would not be called frequently.
    if (index == JSArrayIterator::doneIndex) [[unlikely]] {
        JSObject* result = createIteratorResultObject(globalObject, jsUndefined(), true);
        RELEASE_AND_RETURN(scope, JSValue::encode(result));
    }

    uint64_t length = 0;
    if (isTypedArray) {
        auto view = jsCast<JSArrayBufferView*>(array);
        validateTypedArray(globalObject, view);
        RETURN_IF_EXCEPTION(scope, { });
        length = static_cast<uint64_t>(view->length());
    } else {
        length = toLength(globalObject, array);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // This `unlikely` annotation assumes that it's not likely relatively the current seeking is the end of the source for collection.
    if (static_cast<uint64_t>(index) >= length) [[unlikely]] {
        iterator->putIndex(JSArrayIterator::doneIndex);
        JSObject* result = createIteratorResultObject(globalObject, jsUndefined(), true);
        RELEASE_AND_RETURN(scope, JSValue::encode(result));
    }

    iterator->putIndex(index + 1);

    switch (iterator->kind()) {
    case IterationKind::Keys: {
        JSObject* result = createIteratorResultObject(globalObject, jsNumber(index), false);
        RELEASE_AND_RETURN(scope, JSValue::encode(result));
    }
    case IterationKind::Values: {
        JSValue value = array->getIndex(globalObject, index);
        RETURN_IF_EXCEPTION(scope, { });

        JSObject* result = createIteratorResultObject(globalObject, value, false);
        RELEASE_AND_RETURN(scope, JSValue::encode(result));
    }
    case IterationKind::Entries: {
        JSValue key = jsNumber(index);
        JSValue value = array->getIndex(globalObject, index);
        RETURN_IF_EXCEPTION(scope, { });

        JSArray* entry = constructArrayPair(globalObject, key, value);
        JSObject* result = createIteratorResultObject(globalObject, entry, false);
        RELEASE_AND_RETURN(scope, JSValue::encode(result));
    }
    default: {
        ASSERT_NOT_REACHED();
        return { };
    }
    }
}

} // namespace JSC
