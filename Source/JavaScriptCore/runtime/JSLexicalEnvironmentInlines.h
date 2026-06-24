/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSLexicalEnvironment.h"
#include "JSSymbolTableObjectInlines.h"
#include "StructureCreateInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

inline JSLexicalEnvironment::JSLexicalEnvironment(VM& vm, Structure* structure, JSScope* currentScope, SymbolTable* symbolTable, JSValue initialValue)
    : Base(vm, structure, currentScope, symbolTable)
{
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    for (unsigned i = this->symbolTable()->scopeSize(); i--;) {
        // Filling this with undefined/TDZEmptyValue is useful because that's what variables start out as.
        variableAt(ScopeOffset(i)).setStartingValue(initialValue);
    }
}

inline JSLexicalEnvironment* JSLexicalEnvironment::create(VM& vm, Structure* structure, JSScope* currentScope, SymbolTable* symbolTable, JSValue initialValue)
{
    JSLexicalEnvironment* result =
        new (
            NotNull,
            allocateCell<JSLexicalEnvironment>(vm, allocationSize(symbolTable)))
        JSLexicalEnvironment(vm, structure, currentScope, symbolTable, initialValue);
    result->finishCreation(vm);
    return result;
}

inline JSLexicalEnvironment* JSLexicalEnvironment::create(VM& vm, JSGlobalObject* globalObject, JSScope* currentScope, SymbolTable* symbolTable, JSValue initialValue)
{
    Structure* structure = globalObject->activationStructure();
    return create(vm, structure, currentScope, symbolTable, initialValue);
}

inline Structure* JSLexicalEnvironment::createStructure(VM& vm, JSGlobalObject* globalObject)
{
    return Structure::create(vm, globalObject, jsNull(), TypeInfo(LexicalEnvironmentType, StructureFlags), info());
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
