/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "JSIteratorHelper.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSIteratorHelper::s_info = { "Iterator Helper"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSIteratorHelper) };

JSIteratorHelper* JSIteratorHelper::createWithInitialValues(VM& vm, Structure* structure)
{
    auto values = initialValues();
    JSIteratorHelper* result = new (NotNull, allocateCell<JSIteratorHelper>(vm)) JSIteratorHelper(vm, structure);
    result->finishCreation(vm);
    result->internalField(Field::Generator).set(vm, result, values[0]);
    result->internalField(Field::UnderlyingIterator).set(vm, result, values[1]);
    return result;
}

JSIteratorHelper* JSIteratorHelper::create(VM& vm, Structure* structure, JSObject* generator, JSObject* underlyingIterator)
{
    JSIteratorHelper* result = new (NotNull, allocateCell<JSIteratorHelper>(vm)) JSIteratorHelper(vm, structure);
    result->finishCreation(vm);
    result->internalField(Field::Generator).set(vm, result, generator);
    result->internalField(Field::UnderlyingIterator).set(vm, result, underlyingIterator);
    return result;
}

Structure* JSIteratorHelper::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSIteratorHelperType, StructureFlags), info());
}

JSIteratorHelper::JSIteratorHelper(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

template<typename Visitor>
void JSIteratorHelper::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSIteratorHelper*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSIteratorHelper);

JSC_DEFINE_HOST_FUNCTION(iteratorHelperPrivateFuncCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(JSIteratorHelper::create(globalObject->vm(), globalObject->iteratorHelperStructure(), jsCast<JSObject*>(callFrame->uncheckedArgument(0)), jsCast<JSObject*>(callFrame->uncheckedArgument(1))));
}

} // namespace JSC
