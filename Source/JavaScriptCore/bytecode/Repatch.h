/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

#include "CacheableIdentifier.h"
#include "CallVariant.h"
#include "PutKind.h"

namespace JSC {

class CallLinkInfo;
class OptimizingCallLinkInfo;
class StructureStubInfo;

enum class GetByKind {
    ById,
    ByVal,
    TryById,
    ByIdWithThis,
    ByIdDirect,
    ByValWithThis,
    PrivateName,
    PrivateNameById,
};

enum class PutByKind {
    ById,
    ByVal,
};

enum class DelByKind {
    ById,
    ByVal
};

enum class InByKind {
    ById,
    ByVal,
    PrivateName
};

void repatchArrayGetByVal(JSGlobalObject*, CodeBlock*, JSValue base, JSValue index, StructureStubInfo&, GetByKind);
void repatchGetBy(JSGlobalObject*, CodeBlock*, JSValue, CacheableIdentifier, const PropertySlot&, StructureStubInfo&, GetByKind);
void repatchArrayPutByVal(JSGlobalObject*, CodeBlock*, JSValue base, JSValue index, StructureStubInfo&, PutKind, ECMAMode);
void repatchPutBy(JSGlobalObject*, CodeBlock*, JSValue, Structure*, CacheableIdentifier, const PutPropertySlot&, StructureStubInfo&, PutByKind, PutKind);
void repatchDeleteBy(JSGlobalObject*, CodeBlock*, DeletePropertySlot&, JSValue, Structure*, CacheableIdentifier, StructureStubInfo&, DelByKind, ECMAMode);
void repatchInBy(JSGlobalObject*, CodeBlock*, JSObject*, CacheableIdentifier, bool wasFound, const PropertySlot&, StructureStubInfo&, InByKind);
void repatchHasPrivateBrand(JSGlobalObject*, CodeBlock*, JSObject*, CacheableIdentifier, bool wasFound,  StructureStubInfo&);
void repatchCheckPrivateBrand(JSGlobalObject*, CodeBlock*, JSObject*, CacheableIdentifier, StructureStubInfo&);
void repatchSetPrivateBrand(JSGlobalObject*, CodeBlock*, JSObject*, Structure*, CacheableIdentifier, StructureStubInfo&);
void repatchInstanceOf(JSGlobalObject*, CodeBlock*, JSValue, JSValue prototype, StructureStubInfo&, bool wasFound);
void linkMonomorphicCall(VM&, CallFrame*, CallLinkInfo&, CodeBlock*, JSObject* callee, CodePtr<JSEntryPtrTag>);
void linkDirectCall(CallFrame*, OptimizingCallLinkInfo&, CodeBlock*, CodePtr<JSEntryPtrTag>);
void unlinkCall(VM&, CallLinkInfo&);
void linkPolymorphicCall(JSGlobalObject*, CallFrame*, CallLinkInfo&, CallVariant);
void resetGetBy(CodeBlock*, StructureStubInfo&, GetByKind);
void resetPutBy(CodeBlock*, StructureStubInfo&, PutByKind);
void resetDelBy(CodeBlock*, StructureStubInfo&, DelByKind);
void resetInBy(CodeBlock*, StructureStubInfo&, InByKind);
void resetHasPrivateBrand(CodeBlock*, StructureStubInfo&);
void resetInstanceOf(CodeBlock*, StructureStubInfo&);
void resetCheckPrivateBrand(CodeBlock*, StructureStubInfo&);
void resetSetPrivateBrand(CodeBlock*, StructureStubInfo&);
void ftlThunkAwareRepatchCall(CodeBlock*, CodeLocationCall<JSInternalPtrTag>, CodePtr<CFunctionPtrTag> newCalleeFunction);
CodePtr<JSEntryPtrTag> jsToWasmICCodePtr(CodeSpecializationKind, JSObject* callee);

} // namespace JSC
