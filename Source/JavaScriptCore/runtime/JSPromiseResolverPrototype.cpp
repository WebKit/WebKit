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
#include "JSPromiseResolverPrototype.h"

#if ENABLE(PROMISES)

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromiseResolver.h"
#include "StructureInlines.h"

namespace JSC {

ASSERT_HAS_TRIVIAL_DESTRUCTOR(JSPromiseResolverPrototype);

// void fulfill(optional any value);
static EncodedJSValue JSC_HOST_CALL JSPromiseResolverPrototypeFuncFulfill(ExecState*);
// void resolve(optional any value);
static EncodedJSValue JSC_HOST_CALL JSPromiseResolverPrototypeFuncResolve(ExecState*);
// void reject(optional any value);
static EncodedJSValue JSC_HOST_CALL JSPromiseResolverPrototypeFuncReject(ExecState*);

}

#include "JSPromiseResolverPrototype.lut.h"

namespace JSC {

const ClassInfo JSPromiseResolverPrototype::s_info = { "PromiseResolverPrototype", &Base::s_info, 0, ExecState::promiseResolverPrototypeTable, CREATE_METHOD_TABLE(JSPromiseResolverPrototype) };

/* Source for JSPromiseResolverPrototype.lut.h
@begin promiseResolverPrototypeTable
  fulfill         JSPromiseResolverPrototypeFuncFulfill             DontEnum|Function 1
  resolve         JSPromiseResolverPrototypeFuncResolve             DontEnum|Function 1
  reject          JSPromiseResolverPrototypeFuncReject              DontEnum|Function 1
@end
*/

JSPromiseResolverPrototype* JSPromiseResolverPrototype::create(ExecState* exec, JSGlobalObject*, Structure* structure)
{
    JSPromiseResolverPrototype* object = new (NotNull, allocateCell<JSPromiseResolverPrototype>(*exec->heap())) JSPromiseResolverPrototype(exec, structure);
    object->finishCreation(exec, structure);
    return object;
}

Structure* JSPromiseResolverPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromiseResolverPrototype::JSPromiseResolverPrototype(ExecState* exec, Structure* structure)
    : JSNonFinalObject(exec->vm(), structure)
{
}

void JSPromiseResolverPrototype::finishCreation(ExecState* exec, Structure*)
{
    Base::finishCreation(exec->vm());
}

bool JSPromiseResolverPrototype::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, ExecState::promiseResolverPrototypeTable(exec), jsCast<JSPromiseResolverPrototype*>(object), propertyName, slot);
}

EncodedJSValue JSC_HOST_CALL JSPromiseResolverPrototypeFuncFulfill(ExecState* exec)
{
    JSPromiseResolver* thisObject = jsDynamicCast<JSPromiseResolver*>(exec->thisValue());
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver of fulfill must be a PromiseResolver"));

    thisObject->fulfillIfNotResolved(exec, exec->argument(0));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL JSPromiseResolverPrototypeFuncResolve(ExecState* exec)
{
    JSPromiseResolver* thisObject = jsDynamicCast<JSPromiseResolver*>(exec->thisValue());
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver of resolve must be a PromiseResolver"));

    thisObject->resolveIfNotResolved(exec, exec->argument(0));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL JSPromiseResolverPrototypeFuncReject(ExecState* exec)
{
    JSPromiseResolver* thisObject = jsDynamicCast<JSPromiseResolver*>(exec->thisValue());
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver of reject must be a PromiseResolver"));

    thisObject->rejectIfNotResolved(exec, exec->argument(0));
    return JSValue::encode(jsUndefined());
}

} // namespace JSC

#endif // ENABLE(PROMISES)
