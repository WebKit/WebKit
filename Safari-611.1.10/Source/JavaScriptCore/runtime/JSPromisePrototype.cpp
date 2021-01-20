/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "JSPromisePrototype.h"

#include "BuiltinNames.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromisePrototype);

}

#include "JSPromisePrototype.lut.h"

namespace JSC {

const ClassInfo JSPromisePrototype::s_info = { "Promise", &Base::s_info, &promisePrototypeTable, nullptr, CREATE_METHOD_TABLE(JSPromisePrototype) };

/* Source for JSPromisePrototype.lut.h
@begin promisePrototypeTable
  catch        JSBuiltin            DontEnum|Function 1
  finally      JSBuiltin            DontEnum|Function 1
@end
*/

JSPromisePrototype* JSPromisePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    JSPromisePrototype* object = new (NotNull, allocateCell<JSPromisePrototype>(vm.heap)) JSPromisePrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    object->addOwnInternalSlots(vm, globalObject);
    return object;
}

Structure* JSPromisePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromisePrototype::JSPromisePrototype(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void JSPromisePrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().thenPublicName(), globalObject->promiseProtoThenFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

void JSPromisePrototype::addOwnInternalSlots(VM& vm, JSGlobalObject* globalObject)
{
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().thenPrivateName(), globalObject->promiseProtoThenFunction(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

} // namespace JSC
