/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "JSCustomGetterFunction.h"

#include "IdentifierInlines.h"
#include "JSCJSValueInlines.h"

namespace JSC {

const ClassInfo JSCustomGetterFunction::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSCustomGetterFunction) };
static JSC_DECLARE_HOST_FUNCTION(customGetterFunctionCall);

JSC_DEFINE_HOST_FUNCTION(customGetterFunctionCall, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSCustomGetterFunction* customGetterFunction = jsCast<JSCustomGetterFunction*>(callFrame->jsCallee());
    JSValue thisValue = callFrame->thisValue();
    auto getter = customGetterFunction->getter();

    if (auto domAttribute = customGetterFunction->domAttribute()) {
        if (!thisValue.inherits(domAttribute->classInfo))
            return throwVMDOMAttributeGetterTypeError(globalObject, scope, domAttribute->classInfo, customGetterFunction->propertyName());
    }

    RELEASE_AND_RETURN(scope, getter(globalObject, JSValue::encode(thisValue), customGetterFunction->propertyName()));
}

JSCustomGetterFunction::JSCustomGetterFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, const PropertyName& propertyName, CustomFunctionPointer getter, std::optional<DOMAttributeAnnotation> domAttribute)
    : Base(vm, executable, globalObject, structure)
    , m_propertyName(Identifier::fromUid(vm, propertyName.uid()))
    , m_getter(getter)
    , m_domAttribute(domAttribute)
{
}

JSCustomGetterFunction* JSCustomGetterFunction::create(VM& vm, JSGlobalObject* globalObject, const PropertyName& propertyName, CustomFunctionPointer getter, std::optional<DOMAttributeAnnotation> domAttribute)
{
    ASSERT(getter);
    NativeExecutable* executable = vm.getHostFunction(customGetterFunctionCall, ImplementationVisibility::Public, callHostFunctionAsConstructor, String(propertyName.publicName()));
    Structure* structure = globalObject->customGetterFunctionStructure();
    JSCustomGetterFunction* function = new (NotNull, allocateCell<JSCustomGetterFunction>(vm)) JSCustomGetterFunction(vm, executable, globalObject, structure, propertyName, getter, domAttribute);

    // Can't do this during initialization because getHostFunction might do a GC allocation.
    auto name = makeString("get "_s, propertyName.publicName());
    function->finishCreation(vm, executable, 0, name);
    return function;
}

void JSCustomGetterFunction::destroy(JSCell* cell)
{
    static_cast<JSCustomGetterFunction*>(cell)->~JSCustomGetterFunction();
}

} // namespace JSC
