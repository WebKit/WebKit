/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
private:
    AtomicsObject(VM&, Structure*);

public:
    typedef JSNonFinalObject Base;
    
    static AtomicsObject* create(VM&, JSGlobalObject*, Structure*);
    
    DECLARE_INFO;
    
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

protected:
    void finishCreation(VM&, JSGlobalObject*);
};

EncodedJSValue JIT_OPERATION operationAtomicsAdd(ExecState* exec, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand);
EncodedJSValue JIT_OPERATION operationAtomicsAnd(ExecState* exec, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand);
EncodedJSValue JIT_OPERATION operationAtomicsCompareExchange(ExecState* exec, EncodedJSValue base, EncodedJSValue index, EncodedJSValue expected, EncodedJSValue newValue);
EncodedJSValue JIT_OPERATION operationAtomicsExchange(ExecState* exec, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand);
EncodedJSValue JIT_OPERATION operationAtomicsIsLockFree(ExecState* exec, EncodedJSValue size);
EncodedJSValue JIT_OPERATION operationAtomicsLoad(ExecState* exec, EncodedJSValue base, EncodedJSValue index);
EncodedJSValue JIT_OPERATION operationAtomicsOr(ExecState* exec, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand);
EncodedJSValue JIT_OPERATION operationAtomicsStore(ExecState* exec, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand);
EncodedJSValue JIT_OPERATION operationAtomicsSub(ExecState* exec, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand);
EncodedJSValue JIT_OPERATION operationAtomicsXor(ExecState* exec, EncodedJSValue base, EncodedJSValue index, EncodedJSValue operand);

} // namespace JSC

