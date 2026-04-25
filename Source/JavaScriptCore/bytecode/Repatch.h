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

namespace JSC {

class CallLinkInfo;
class DirectCallLinkInfo;
class OptimizingCallLinkInfo;
class PropertyInlineCache;

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
    ByIdStrict,
    ByIdSloppy,
    ByValStrict,
    ByValSloppy,
    ByIdDirectStrict,
    ByIdDirectSloppy,
    ByValDirectStrict,
    ByValDirectSloppy,
    DefinePrivateNameById,
    DefinePrivateNameByVal,
    SetPrivateNameById,
    SetPrivateNameByVal,
};

enum class DelByKind {
    ByIdStrict,
    ByIdSloppy,
    ByValStrict,
    ByValSloppy,
};

enum class InByKind {
    ById,
    ByVal,
    PrivateName
};

CacheableIdentifier nonStringPrimitiveKeyForSubscript(VM&, JSValue subscript);

void repatchArrayGetByVal(JSGlobalObject*, CodeBlock*, JSValue base, JSValue index, PropertyInlineCache&, GetByKind);
void repatchGetBy(JSGlobalObject*, CodeBlock*, JSValue, CacheableIdentifier, const PropertySlot&, PropertyInlineCache&, GetByKind, bool isNonStringPrimitiveKey);
void repatchArrayPutByVal(JSGlobalObject*, CodeBlock*, JSValue base, JSValue index, PropertyInlineCache&, PutByKind);
void repatchPutBy(JSGlobalObject*, CodeBlock*, JSValue, Structure*, CacheableIdentifier, const PutPropertySlot&, PropertyInlineCache&, PutByKind, bool isNonStringPrimitiveKey);
void repatchDeleteBy(JSGlobalObject*, CodeBlock*, DeletePropertySlot&, JSValue, Structure*, CacheableIdentifier, PropertyInlineCache&, DelByKind, ECMAMode);
void repatchArrayInByVal(JSGlobalObject*, CodeBlock*, JSValue base, JSValue index, PropertyInlineCache&, InByKind);
void repatchInBy(JSGlobalObject*, CodeBlock*, JSObject*, CacheableIdentifier, bool wasFound, const PropertySlot&, PropertyInlineCache&, InByKind);
void repatchHasPrivateBrand(JSGlobalObject*, CodeBlock*, JSObject*, CacheableIdentifier, bool wasFound,  PropertyInlineCache&);
void repatchCheckPrivateBrand(JSGlobalObject*, CodeBlock*, JSObject*, CacheableIdentifier, PropertyInlineCache&);
void repatchSetPrivateBrand(JSGlobalObject*, CodeBlock*, JSObject*, Structure*, CacheableIdentifier, PropertyInlineCache&);
void repatchInstanceOf(JSGlobalObject*, CodeBlock*, JSValue, JSValue prototype, PropertyInlineCache&, bool wasFound);
void linkMonomorphicCall(VM&, JSCell*, CallLinkInfo&, CodeBlock*, JSObject* callee, CodePtr<JSEntryPtrTag>);
void linkDirectCall(DirectCallLinkInfo&, CodeBlock*, CodePtr<JSEntryPtrTag>);
void linkPolymorphicCall(VM&, JSCell*, CallFrame*, CallLinkInfo&, CallVariant);
void resetGetBy(CodeBlock*, PropertyInlineCache&, GetByKind);
void resetPutBy(CodeBlock*, PropertyInlineCache&, PutByKind);
void resetDelBy(CodeBlock*, PropertyInlineCache&, DelByKind);
void resetInBy(CodeBlock*, PropertyInlineCache&, InByKind);
void resetHasPrivateBrand(CodeBlock*, PropertyInlineCache&);
void resetInstanceOf(CodeBlock*, PropertyInlineCache&);
void resetCheckPrivateBrand(CodeBlock*, PropertyInlineCache&);
void resetSetPrivateBrand(CodeBlock*, PropertyInlineCache&);

void repatchGetBySlowPathCall(CodeBlock*, PropertyInlineCache&, GetByKind);
void repatchPutBySlowPathCall(CodeBlock*, PropertyInlineCache&, PutByKind);
void repatchInBySlowPathCall(CodeBlock*, PropertyInlineCache&, InByKind);

void ftlThunkAwareRepatchCall(CodeBlock*, CodeLocationCall<JSInternalPtrTag>, CodePtr<CFunctionPtrTag> newCalleeFunction);
CodePtr<JSEntryPtrTag> jsToWasmICCodePtr(CodeSpecializationKind, JSObject* callee);

} // namespace JSC
