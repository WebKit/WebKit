/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "FunctionExecutable.h"
#include "InferredValueInlines.h"
#include "ScriptExecutableInlines.h"

namespace JSC {

inline Structure* FunctionExecutable::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
{
    return Structure::create(vm, globalObject, proto, TypeInfo(FunctionExecutableType, StructureFlags), info());
}

inline void FunctionExecutable::finalizeUnconditionally(VM& vm, CollectionScope collectionScope)
{
    m_singleton.finalizeUnconditionally(vm, collectionScope);
    finalizeCodeBlockEdge(vm, m_codeBlockForCall);
    finalizeCodeBlockEdge(vm, m_codeBlockForConstruct);
    vm.heap.functionExecutableSpaceAndSet.outputConstraintsSet.remove(this);
}

inline FunctionCodeBlock* FunctionExecutable::replaceCodeBlockWith(VM& vm, CodeSpecializationKind kind, CodeBlock* newCodeBlock)
{
    if (kind == CodeForCall) {
        FunctionCodeBlock* oldCodeBlock = codeBlockForCall();
        m_codeBlockForCall.setMayBeNull(vm, this, newCodeBlock);
        return oldCodeBlock;
    }
    ASSERT(kind == CodeForConstruct);
    FunctionCodeBlock* oldCodeBlock = codeBlockForConstruct();
    m_codeBlockForConstruct.setMayBeNull(vm, this, newCodeBlock);
    return oldCodeBlock;
}

inline JSString* FunctionExecutable::toString(JSGlobalObject* globalObject)
{
    RareData& rareData = ensureRareData();
    if (!rareData.m_asString)
        return toStringSlow(globalObject);
    return rareData.m_asString.get();
}

} // namespace JSC

