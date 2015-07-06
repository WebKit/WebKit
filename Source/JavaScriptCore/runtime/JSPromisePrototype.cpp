/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if ENABLE(PROMISES)

#include "Error.h"
#include "JSCBuiltins.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "JSPromise.h"
#include "Microtask.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromisePrototype);

}

#include "JSPromisePrototype.lut.h"

namespace JSC {

const ClassInfo JSPromisePrototype::s_info = { "PromisePrototype", &JSNonFinalObject::s_info, &promisePrototypeTable, CREATE_METHOD_TABLE(JSPromisePrototype) };

/* Source for JSPromisePrototype.lut.h
@begin promisePrototypeTable
  then         JSPromisePrototypeFuncThen             DontEnum|Function 2
  catch        JSPromisePrototypeFuncCatch            DontEnum|Function 1
@end
*/

JSPromisePrototype* JSPromisePrototype::create(ExecState* exec, JSGlobalObject*, Structure* structure)
{
    VM& vm = exec->vm();
    JSPromisePrototype* object = new (NotNull, allocateCell<JSPromisePrototype>(vm.heap)) JSPromisePrototype(exec, structure);
    object->finishCreation(vm, structure);
    return object;
}

Structure* JSPromisePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromisePrototype::JSPromisePrototype(ExecState* exec, Structure* structure)
    : JSNonFinalObject(exec->vm(), structure)
{
}

void JSPromisePrototype::finishCreation(VM& vm, Structure*)
{
    Base::finishCreation(vm);
}

bool JSPromisePrototype::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, promisePrototypeTable, jsCast<JSPromisePrototype*>(object), propertyName, slot);
}

} // namespace JSC

#endif // ENABLE(PROMISES)
