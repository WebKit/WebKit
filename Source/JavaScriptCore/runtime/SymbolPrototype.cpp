/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "SymbolPrototype.h"

#include "Error.h"
#include "JSCInlines.h"
#include "JSString.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL symbolProtoFuncToString(ExecState*);
static EncodedJSValue JSC_HOST_CALL symbolProtoFuncValueOf(ExecState*);

}

#include "SymbolPrototype.lut.h"

namespace JSC {

const ClassInfo SymbolPrototype::s_info = { "Symbol", &Base::s_info, &symbolPrototypeTable, CREATE_METHOD_TABLE(SymbolPrototype) };

/* Source for SymbolPrototype.lut.h
@begin symbolPrototypeTable
  toString          symbolProtoFuncToString         DontEnum|Function 0
  valueOf           symbolProtoFuncValueOf          DontEnum|Function 0
@end
*/

SymbolPrototype::SymbolPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void SymbolPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(&vm, "Symbol"), DontEnum | ReadOnly);
    ASSERT(inherits(info()));

    JSC_NATIVE_FUNCTION(vm.propertyNames->toPrimitiveSymbol, symbolProtoFuncValueOf, DontEnum | ReadOnly, 1);
}

bool SymbolPrototype::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<Base>(exec, symbolPrototypeTable, jsCast<SymbolPrototype*>(object), propertyName, slot);
}

// ------------------------------ Functions ---------------------------

EncodedJSValue JSC_HOST_CALL symbolProtoFuncToString(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    Symbol* symbol = nullptr;
    if (thisValue.isSymbol())
        symbol = asSymbol(thisValue);
    else if (!thisValue.isObject())
        return throwVMTypeError(exec);
    else {
        JSObject* thisObject = asObject(thisValue);
        if (!thisObject->inherits(SymbolObject::info()))
            return throwVMTypeError(exec);
        symbol = asSymbol(jsCast<SymbolObject*>(thisObject)->internalValue());
    }

    return JSValue::encode(jsNontrivialString(exec, symbol->descriptiveString()));
}

EncodedJSValue JSC_HOST_CALL symbolProtoFuncValueOf(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (thisValue.isSymbol())
        return JSValue::encode(thisValue);

    if (!thisValue.isObject())
        return throwVMTypeError(exec);

    JSObject* thisObject = asObject(thisValue);
    if (!thisObject->inherits(SymbolObject::info()))
        return throwVMTypeError(exec);

    return JSValue::encode(jsCast<SymbolObject*>(thisObject)->internalValue());
}

} // namespace JSC
