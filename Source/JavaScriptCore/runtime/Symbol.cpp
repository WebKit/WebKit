/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2015-2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "Symbol.h"

#include "JSCJSValueInlines.h"
#include "SymbolObject.h"

namespace JSC {

const ClassInfo Symbol::s_info = { "symbol"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(Symbol) };

Symbol::Symbol(VM& vm)
    : Base(vm, vm.symbolStructure.get())
{
}

Symbol::Symbol(VM& vm, const String& string)
    : Base(vm, vm.symbolStructure.get())
    , m_privateName(PrivateName::Description, string)
{
}

Symbol::Symbol(VM& vm, SymbolImpl& uid)
    : Base(vm, vm.symbolStructure.get())
    , m_privateName(uid)
{
}

void Symbol::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    vm.symbolImplToSymbolMap.set(&m_privateName.uid(), this);
}

JSValue Symbol::toPrimitive(JSGlobalObject*, PreferredPrimitiveType) const
{
    return const_cast<Symbol*>(this);
}

JSObject* Symbol::toObject(JSGlobalObject* globalObject) const
{
    return SymbolObject::create(globalObject->vm(), globalObject->symbolObjectStructure(), const_cast<Symbol*>(this));
}

double Symbol::toNumber(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwTypeError(globalObject, scope, "Cannot convert a symbol to a number"_s);
    return 0.0;
}

void Symbol::destroy(JSCell* cell)
{
    static_cast<Symbol*>(cell)->Symbol::~Symbol();
}

String Symbol::descriptiveString() const
{
    return makeString("Symbol(", String(m_privateName.uid()), ')');
}

String Symbol::description() const
{
    auto& uid = m_privateName.uid();
    return uid.isNullSymbol() ? String() : uid;
}

Symbol* Symbol::create(VM& vm)
{
    Symbol* symbol = new (NotNull, allocateCell<Symbol>(vm)) Symbol(vm);
    symbol->finishCreation(vm);
    return symbol;
}

Symbol* Symbol::createWithDescription(VM& vm, const String& description)
{
    Symbol* symbol = new (NotNull, allocateCell<Symbol>(vm)) Symbol(vm, description);
    symbol->finishCreation(vm);
    return symbol;
}

Symbol* Symbol::create(VM& vm, SymbolImpl& uid)
{
    if (Symbol* symbol = vm.symbolImplToSymbolMap.get(&uid))
        return symbol;

    Symbol* symbol = new (NotNull, allocateCell<Symbol>(vm)) Symbol(vm, uid);
    symbol->finishCreation(vm);
    return symbol;
}

} // namespace JSC
