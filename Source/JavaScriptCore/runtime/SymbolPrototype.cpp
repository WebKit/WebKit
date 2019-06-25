/*
 * Copyright (C) 2012, 2016 Apple Inc. All rights reserved.
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
#include "SymbolObject.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL symbolProtoGetterDescription(ExecState*);
static EncodedJSValue JSC_HOST_CALL symbolProtoFuncToString(ExecState*);
static EncodedJSValue JSC_HOST_CALL symbolProtoFuncValueOf(ExecState*);

}

#include "SymbolPrototype.lut.h"

namespace JSC {

const ClassInfo SymbolPrototype::s_info = { "Symbol", &Base::s_info, &symbolPrototypeTable, nullptr, CREATE_METHOD_TABLE(SymbolPrototype) };

/* Source for SymbolPrototype.lut.h
@begin symbolPrototypeTable
  description       symbolProtoGetterDescription    DontEnum|Accessor 0
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
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(&vm, "Symbol"), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    ASSERT(inherits(vm, info()));

    JSFunction* toPrimitiveFunction = JSFunction::create(vm, globalObject, 1, "[Symbol.toPrimitive]"_s, symbolProtoFuncValueOf, NoIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->toPrimitiveSymbol, toPrimitiveFunction, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
}

// ------------------------------ Functions ---------------------------

static const ASCIILiteral SymbolDescriptionTypeError { "Symbol.prototype.description requires that |this| be a symbol or a symbol object"_s };
static const ASCIILiteral SymbolToStringTypeError { "Symbol.prototype.toString requires that |this| be a symbol or a symbol object"_s };
static const ASCIILiteral SymbolValueOfTypeError { "Symbol.prototype.valueOf requires that |this| be a symbol or a symbol object"_s };

inline Symbol* tryExtractSymbol(VM& vm, JSValue thisValue)
{
    if (thisValue.isSymbol())
        return asSymbol(thisValue);

    if (!thisValue.isObject())
        return nullptr;
    JSObject* thisObject = asObject(thisValue);
    if (!thisObject->inherits<SymbolObject>(vm))
        return nullptr;
    return asSymbol(jsCast<SymbolObject*>(thisObject)->internalValue());
}

EncodedJSValue JSC_HOST_CALL symbolProtoGetterDescription(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Symbol* symbol = tryExtractSymbol(vm, exec->thisValue());
    if (!symbol)
        return throwVMTypeError(exec, scope, SymbolDescriptionTypeError);
    scope.release();
    const auto description = symbol->description();
    return JSValue::encode(description.isNull() ? jsUndefined() : jsString(&vm, description));
}

EncodedJSValue JSC_HOST_CALL symbolProtoFuncToString(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Symbol* symbol = tryExtractSymbol(vm, exec->thisValue());
    if (!symbol)
        return throwVMTypeError(exec, scope, SymbolToStringTypeError);
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNontrivialString(&vm, symbol->descriptiveString())));
}

EncodedJSValue JSC_HOST_CALL symbolProtoFuncValueOf(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Symbol* symbol = tryExtractSymbol(vm, exec->thisValue());
    if (!symbol)
        return throwVMTypeError(exec, scope, SymbolValueOfTypeError);

    RELEASE_AND_RETURN(scope, JSValue::encode(symbol));
}

} // namespace JSC
