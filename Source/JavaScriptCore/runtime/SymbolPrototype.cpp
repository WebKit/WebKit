/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#include "IntegrityInlines.h"
#include "JSCInlines.h"
#include "SymbolObject.h"

namespace JSC {

static JSC_DECLARE_CUSTOM_GETTER(symbolProtoGetterDescription);
static JSC_DECLARE_HOST_FUNCTION(symbolProtoFuncToString);
static JSC_DECLARE_HOST_FUNCTION(symbolProtoFuncValueOf);

}

#include "SymbolPrototype.lut.h"

namespace JSC {

const ClassInfo SymbolPrototype::s_info = { "Symbol"_s, &Base::s_info, &symbolPrototypeTable, nullptr, CREATE_METHOD_TABLE(SymbolPrototype) };

/* Source for SymbolPrototype.lut.h
@begin symbolPrototypeTable
  description       symbolProtoGetterDescription    DontEnum|ReadOnly|CustomAccessor
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
    ASSERT(inherits(info()));

    JSFunction* toPrimitiveFunction = JSFunction::create(vm, globalObject, 1, "[Symbol.toPrimitive]"_s, symbolProtoFuncValueOf, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, vm.propertyNames->toPrimitiveSymbol, toPrimitiveFunction, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// ------------------------------ Functions ---------------------------

static const ASCIILiteral SymbolDescriptionTypeError { "Symbol.prototype.description requires that |this| be a symbol or a symbol object"_s };
static const ASCIILiteral SymbolToStringTypeError { "Symbol.prototype.toString requires that |this| be a symbol or a symbol object"_s };
static const ASCIILiteral SymbolValueOfTypeError { "Symbol.prototype.valueOf requires that |this| be a symbol or a symbol object"_s };

inline Symbol* tryExtractSymbol(JSValue thisValue)
{
    if (thisValue.isSymbol())
        return asSymbol(thisValue);

    if (!thisValue.isObject())
        return nullptr;
    JSObject* thisObject = asObject(thisValue);
    if (!thisObject->inherits<SymbolObject>())
        return nullptr;
    return asSymbol(jsCast<SymbolObject*>(thisObject)->internalValue());
}

JSC_DEFINE_CUSTOM_GETTER(symbolProtoGetterDescription, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Symbol* symbol = tryExtractSymbol(JSValue::decode(thisValue));
    if (!symbol)
        return throwVMTypeError(globalObject, scope, SymbolDescriptionTypeError);
    scope.release();
    Integrity::auditStructureID(symbol->structureID());
    auto description = symbol->description();
    return JSValue::encode(description.isNull() ? jsUndefined() : jsString(vm, WTFMove(description)));
}

JSC_DEFINE_HOST_FUNCTION(symbolProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Symbol* symbol = tryExtractSymbol(callFrame->thisValue());
    if (!symbol)
        return throwVMTypeError(globalObject, scope, SymbolToStringTypeError);
    Integrity::auditStructureID(symbol->structureID());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNontrivialString(vm, symbol->descriptiveString())));
}

JSC_DEFINE_HOST_FUNCTION(symbolProtoFuncValueOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Symbol* symbol = tryExtractSymbol(callFrame->thisValue());
    if (!symbol)
        return throwVMTypeError(globalObject, scope, SymbolValueOfTypeError);

    Integrity::auditStructureID(symbol->structureID());
    RELEASE_AND_RETURN(scope, JSValue::encode(symbol));
}

} // namespace JSC
