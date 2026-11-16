/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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

#pragma once

#include "JSSymbolTableObject.h"
#include "SymbolTableInlines.h"

namespace JSC {

inline JSSymbolTableObject::JSSymbolTableObject(VM& vm, Structure* structure, JSScope* scope, SymbolTable* symbolTable)
    : Base(vm, structure, scope)
    , m_symbolTable(symbolTable, WriteBarrierEarlyInit)
{
    ASSERT(symbolTable);
    symbolTable->notifyCreation(vm, this, "Allocated a scope");
}

inline void JSSymbolTableObject::setSymbolTable(VM& vm, SymbolTable* symbolTable)
{
    ASSERT(!m_symbolTable);
    symbolTable->notifyCreation(vm, this, "Allocated a scope");
    m_symbolTable.set(vm, this, symbolTable);
}

} // namespace JSC
