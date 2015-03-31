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
#include "SymbolConstructor.h"

#include "Error.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "SymbolPrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(SymbolConstructor);

const ClassInfo SymbolConstructor::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(SymbolConstructor) };

SymbolConstructor::SymbolConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

#define INITIALIZE_WELL_KNOWN_SYMBOLS(name) \
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, #name), Symbol::create(vm, vm.propertyNames->name##Symbol.impl()), DontEnum | DontDelete | ReadOnly);

void SymbolConstructor::finishCreation(VM& vm, SymbolPrototype* prototype)
{
    Base::finishCreation(vm, prototype->classInfo()->className);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), DontDelete | ReadOnly | DontEnum);

    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_WELL_KNOWN_SYMBOLS)
}

static EncodedJSValue JSC_HOST_CALL callSymbol(ExecState* exec)
{
    JSValue description = exec->argument(0);
    if (description.isUndefined())
        return JSValue::encode(Symbol::create(exec->vm()));
    return JSValue::encode(Symbol::create(exec, description.toString(exec)));
}

ConstructType SymbolConstructor::getConstructData(JSCell*, ConstructData&)
{
    return ConstructTypeNone;
}

CallType SymbolConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callSymbol;
    return CallTypeHost;
}

} // namespace JSC
