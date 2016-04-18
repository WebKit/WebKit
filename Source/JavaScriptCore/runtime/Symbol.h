/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef Symbol_h
#define Symbol_h

#include "JSCell.h"
#include "JSString.h"
#include "PrivateName.h"

namespace JSC {

class Symbol final : public JSCell {
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal | OverridesToThis;

    DECLARE_EXPORT_INFO;

    static const bool needsDestruction = true;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(SymbolType, StructureFlags), info());
    }

    static Symbol* create(VM& vm)
    {
        Symbol* symbol = new (NotNull, allocateCell<Symbol>(vm.heap)) Symbol(vm);
        symbol->finishCreation(vm);
        return symbol;
    }

    static Symbol* create(ExecState* exec, JSString* description)
    {
        VM& vm = exec->vm();
        String desc = description->value(exec);
        Symbol* symbol = new (NotNull, allocateCell<Symbol>(vm.heap)) Symbol(vm, desc);
        symbol->finishCreation(vm);
        return symbol;
    }

    static Symbol* create(VM& vm, SymbolImpl& uid)
    {
        Symbol* symbol = new (NotNull, allocateCell<Symbol>(vm.heap)) Symbol(vm, uid);
        symbol->finishCreation(vm);
        return symbol;
    }

    const PrivateName& privateName() const { return m_privateName; }
    String descriptiveString() const;

    JSValue toPrimitive(ExecState*, PreferredPrimitiveType) const;
    bool getPrimitiveNumber(ExecState*, double& number, JSValue&) const;
    JSObject* toObject(ExecState*, JSGlobalObject*) const;
    double toNumber(ExecState*) const;

    static size_t offsetOfPrivateName() { return OBJECT_OFFSETOF(Symbol, m_privateName); }

protected:
    static void destroy(JSCell*);

    Symbol(VM&);
    Symbol(VM&, const String&);
    Symbol(VM&, SymbolImpl& uid);

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(inherits(info()));
    }

    PrivateName m_privateName;
};

Symbol* asSymbol(JSValue);

inline Symbol* asSymbol(JSValue value)
{
    ASSERT(value.asCell()->isSymbol());
    return jsCast<Symbol*>(value.asCell());
}

} // namespace JSC

#endif // Symbol_h
