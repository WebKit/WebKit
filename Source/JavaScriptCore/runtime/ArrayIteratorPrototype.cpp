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

#include "IteratorOperations.h"
#include "JSArrayInlines.h"
#include "JSArrayIterator.h"
#include "JSCInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(arrayIteratorProtoFuncNext);

const ClassInfo ArrayIteratorPrototype::s_info = { "Array Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ArrayIteratorPrototype) };

void ArrayIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->next, arrayIteratorProtoFuncNext, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, JSArrayIteratorNextIntrinsic);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/ecma262/#sec-%arrayiteratorprototype%.next
JSC_DEFINE_HOST_FUNCTION(arrayIteratorProtoFuncNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* iterator = dynamicDowncast<JSArrayIterator>(callFrame->thisValue());
    if (!iterator) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "%ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance"_s);

    int64_t index = iterator->index();
    if (index == JSArrayIterator::doneIndex)
        return JSValue::encode(createIteratorResultObject(globalObject, jsUndefined(), true));

    JSObject* iteratedObject = iterator->iteratedObject();
    Structure* structure = iteratedObject->structure();
    ArrayProfile& profile = globalObject->arrayIteratorNextArrayProfile();
    profile.computeUpdatedPrediction(globalObject, structure);
    globalObject->arrayIteratorNextIteratedObjectSpeculation() |= speculationFromStructure(structure);

    uint64_t length;
    if (isTypedArrayType(structure->typeInfo().type())) {
        auto* view = uncheckedDowncast<JSArrayBufferView>(iteratedObject);
        if (view->isOutOfBounds()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        length = view->length();
    } else if (isJSArray(iteratedObject))
        length = uncheckedDowncast<JSArray>(iteratedObject)->length();
    else {
        length = toLength(globalObject, iteratedObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (static_cast<uint64_t>(index) >= length) {
        iterator->setIndex(JSArrayIterator::doneIndex);
        return JSValue::encode(createIteratorResultObject(globalObject, jsUndefined(), true));
    }

    // A length getter can finish the iterator through a nested next(), which must stay finished.
    if (iterator->index() != JSArrayIterator::doneIndex)
        iterator->setIndex(index + 1);

    IterationKind kind = iterator->kind();
    if (kind == IterationKind::Keys)
        return JSValue::encode(createIteratorResultObject(globalObject, jsNumber(index), false));

    if (isJSArray(iteratedObject) && !iteratedObject->canGetIndexQuickly(static_cast<uint64_t>(index)))
        profile.setOutOfBounds();

    JSValue element = iteratedObject->get(globalObject, static_cast<uint64_t>(index));
    RETURN_IF_EXCEPTION(scope, { });

    if (kind == IterationKind::Values)
        return JSValue::encode(createIteratorResultObject(globalObject, element, false));

    JSArray* pair = constructArrayPair(globalObject, jsNumber(index), element);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(createIteratorResultObject(globalObject, pair, false));
}

} // namespace JSC
