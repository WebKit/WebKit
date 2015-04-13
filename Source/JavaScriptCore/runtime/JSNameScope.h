/*
 * Copyright (C) 2008, 2009, 2015 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSNameScope_h
#define JSNameScope_h

#include "JSEnvironmentRecord.h"
#include "JSGlobalObject.h"

namespace JSC {

// Used for scopes with a single named variable: catch and named function expression.
class JSNameScope : public JSEnvironmentRecord {
public:
    typedef JSEnvironmentRecord Base;
    static const unsigned StructureFlags = Base::StructureFlags| OverridesGetOwnPropertySlot;

    enum Type {
        CatchScope,
        FunctionNameScope
    };

    template<typename T>
    static T* create(VM& vm, JSGlobalObject* globalObject, JSScope* currentScope, SymbolTable* symbolTable, JSValue value)
    {
        T* scopeObject = new (
            NotNull, allocateCell<T>(vm.heap, allocationSizeForScopeSize(1)))
            T(vm, globalObject, currentScope, symbolTable);
        scopeObject->finishCreation(vm, value);
        return scopeObject;
    }
    
    static JSNameScope* create(VM&, JSGlobalObject*, JSScope* currentScope, SymbolTable*, JSValue, Type);

    static JSValue toThis(JSCell*, ExecState*, ECMAMode);
    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);

    DECLARE_INFO;

    JSValue value() { return variableAt(ScopeOffset(0)).get(); }

protected:
    void finishCreation(VM& vm, JSValue value)
    {
        Base::finishCreationUninitialized(vm);
        variableAt(ScopeOffset(0)).set(vm, this, value);
    }

    JSNameScope(VM& vm, Structure* structure, JSScope* next, SymbolTable* symbolTable)
        : Base(vm, structure, next, symbolTable)
    {
        ASSERT(symbolTable->scopeSize() == 1);
    }
};

}

#endif // JSNameScope_h
