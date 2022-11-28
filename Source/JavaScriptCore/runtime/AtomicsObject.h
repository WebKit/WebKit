/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "JSObject.h"

namespace JSC {

class AtomicsObject final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(AtomicsObject, Base);
        return &vm.plainObjectSpace();
    }
    
    static AtomicsObject* create(VM&, JSGlobalObject*, Structure*);
    
    DECLARE_INFO;
    
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

private:
    AtomicsObject(VM&, Structure*);
    void finishCreation(VM&, JSGlobalObject*);
};

JSC_DECLARE_JIT_OPERATION(operationAtomicsAdd, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand));
JSC_DECLARE_JIT_OPERATION(operationAtomicsAnd, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand));
JSC_DECLARE_JIT_OPERATION(operationAtomicsCompareExchange, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index, EncodedJSValue expected, EncodedJSValue newValue));
JSC_DECLARE_JIT_OPERATION(operationAtomicsExchange, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand));
JSC_DECLARE_JIT_OPERATION(operationAtomicsIsLockFree, EncodedJSValue, (JSGlobalObject*, EncodedJSValue size));
JSC_DECLARE_JIT_OPERATION(operationAtomicsLoad, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index));
JSC_DECLARE_JIT_OPERATION(operationAtomicsOr, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand));
JSC_DECLARE_JIT_OPERATION(operationAtomicsStore, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand));
JSC_DECLARE_JIT_OPERATION(operationAtomicsSub, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand));
JSC_DECLARE_JIT_OPERATION(operationAtomicsXor, EncodedJSValue, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand));

JS_EXPORT_PRIVATE EncodedJSValue getWaiterListSize(JSGlobalObject*, CallFrame*);

} // namespace JSC

