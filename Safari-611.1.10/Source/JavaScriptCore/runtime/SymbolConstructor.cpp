/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "SymbolConstructor.h"

#include "JSCInlines.h"
#include "SymbolPrototype.h"
#include <wtf/text/SymbolRegistry.h>

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(symbolConstructorFor);
static JSC_DECLARE_HOST_FUNCTION(symbolConstructorKeyFor);

}

#include "SymbolConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(SymbolConstructor);

const ClassInfo SymbolConstructor::s_info = { "Function", &Base::s_info, &symbolConstructorTable, nullptr, CREATE_METHOD_TABLE(SymbolConstructor) };

/* Source for SymbolConstructor.lut.h
@begin symbolConstructorTable
  for       symbolConstructorFor       DontEnum|Function 1
  keyFor    symbolConstructorKeyFor    DontEnum|Function 1
@end
*/

static JSC_DECLARE_HOST_FUNCTION(callSymbol);
static JSC_DECLARE_HOST_FUNCTION(constructSymbol);

SymbolConstructor::SymbolConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callSymbol, constructSymbol)
{
}

#define INITIALIZE_WELL_KNOWN_SYMBOLS(name) \
putDirectWithoutTransition(vm, Identifier::fromString(vm, #name), Symbol::create(vm, static_cast<SymbolImpl&>(*vm.propertyNames->name##Symbol.impl())), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

void SymbolConstructor::finishCreation(VM& vm, SymbolPrototype* prototype)
{
    Base::finishCreation(vm, 0, vm.propertyNames->Symbol.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_WELL_KNOWN_SYMBOLS)
}

// ------------------------------ Functions ---------------------------

JSC_DEFINE_HOST_FUNCTION(callSymbol, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue description = callFrame->argument(0);
    if (description.isUndefined())
        return JSValue::encode(Symbol::create(vm));

    String string = description.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(Symbol::createWithDescription(vm, string));
}

JSC_DEFINE_HOST_FUNCTION(constructSymbol, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMError(globalObject, scope, createNotAConstructorError(globalObject, callFrame->jsCallee()));
}

JSC_DEFINE_HOST_FUNCTION(symbolConstructorFor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* stringKey = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String string = stringKey->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(Symbol::create(vm, vm.symbolRegistry().symbolForKey(string)));
}

const ASCIILiteral SymbolKeyForTypeError { "Symbol.keyFor requires that the first argument be a symbol"_s };

JSC_DEFINE_HOST_FUNCTION(symbolConstructorKeyFor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue symbolValue = callFrame->argument(0);
    if (!symbolValue.isSymbol())
        return JSValue::encode(throwTypeError(globalObject, scope, SymbolKeyForTypeError));

    PrivateName privateName = asSymbol(symbolValue)->privateName();
    SymbolImpl& uid = privateName.uid();
    if (!uid.symbolRegistry())
        return JSValue::encode(jsUndefined());

    ASSERT(uid.symbolRegistry() == &vm.symbolRegistry());
    return JSValue::encode(jsString(vm, &uid));
}

} // namespace JSC
