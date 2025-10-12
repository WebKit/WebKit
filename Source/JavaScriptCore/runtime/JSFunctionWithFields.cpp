/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "JSFunctionWithFields.h"

#include "JSCellInlines.h"
#include "JSFunctionInlines.h"
#include "SlotVisitorInlines.h"
#include "VM.h"

namespace JSC {

const ClassInfo JSFunctionWithFields::s_info = { "Function"_s,  &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSFunctionWithFields) };

JSFunctionWithFields::JSFunctionWithFields(VM& vm, NativeExecutable* executable, JSGlobalObject* scope, Structure* structure)
    : Base(vm, executable, scope, structure)
{
}

JSFunctionWithFields* JSFunctionWithFields::create(VM& vm, JSGlobalObject* globalObject, NativeExecutable* executable, unsigned length, const String& name)
{
    JSFunctionWithFields* function = new (NotNull, allocateCell<JSFunctionWithFields>(vm)) JSFunctionWithFields(vm, executable, globalObject, globalObject->functionWithFieldsStructure());
    ASSERT(function->structure()->globalObject());
    function->finishCreation(vm, executable, length, name);
    return function;
}

template<typename Visitor>
void JSFunctionWithFields::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSFunctionWithFields* thisObject = jsCast<JSFunctionWithFields*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.appendValues(thisObject->m_internalFields, numberOfInternalFields);
}

DEFINE_VISIT_CHILDREN(JSFunctionWithFields);

Structure* JSFunctionWithFields::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    ASSERT(globalObject);
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

}
