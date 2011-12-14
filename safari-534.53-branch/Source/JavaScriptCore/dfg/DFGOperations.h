/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef DFGOperations_h
#define DFGOperations_h

#if ENABLE(DFG_JIT)

#include <dfg/DFGJITCompiler.h>

namespace JSC { namespace DFG {

enum PutKind { Direct, NotDirect };

typedef intptr_t RegisterSizedBoolean;

extern "C" {

// These typedefs provide typechecking when generating calls out to helper routines;
// this helps prevent calling a helper routine with the wrong arguments!
typedef EncodedJSValue (*J_DFGOperation_EJJ)(ExecState*, EncodedJSValue, EncodedJSValue);
typedef EncodedJSValue (*J_DFGOperation_EJ)(ExecState*, EncodedJSValue);
typedef EncodedJSValue (*J_DFGOperation_EJP)(ExecState*, EncodedJSValue, void*);
typedef EncodedJSValue (*J_DFGOperation_EJI)(ExecState*, EncodedJSValue, Identifier*);
typedef EncodedJSValue (*J_DFGOperation_EP)(ExecState*, void*);
typedef EncodedJSValue (*J_DFGOperation_EI)(ExecState*, Identifier*);
typedef RegisterSizedBoolean (*Z_DFGOperation_EJ)(ExecState*, EncodedJSValue);
typedef RegisterSizedBoolean (*Z_DFGOperation_EJJ)(ExecState*, EncodedJSValue, EncodedJSValue);
typedef void (*V_DFGOperation_EJJJ)(ExecState*, EncodedJSValue, EncodedJSValue, EncodedJSValue);
typedef void (*V_DFGOperation_EJJP)(ExecState*, EncodedJSValue, EncodedJSValue, void*);
typedef void (*V_DFGOperation_EJJI)(ExecState*, EncodedJSValue, EncodedJSValue, Identifier*);
typedef double (*D_DFGOperation_DD)(double, double);
typedef void *(*P_DFGOperation_E)(ExecState*);

// These routines are provide callbacks out to C++ implementations of operations too complex to JIT.
EncodedJSValue operationConvertThis(ExecState*, EncodedJSValue encodedOp1);
EncodedJSValue operationValueAdd(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
EncodedJSValue operationArithAdd(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
EncodedJSValue operationArithSub(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
EncodedJSValue operationArithMul(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
EncodedJSValue operationArithDiv(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
EncodedJSValue operationArithMod(EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
EncodedJSValue operationGetByVal(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty);
EncodedJSValue operationGetById(ExecState*, EncodedJSValue encodedBase, Identifier*);
EncodedJSValue operationGetByIdBuildList(ExecState*, EncodedJSValue encodedBase, Identifier*);
EncodedJSValue operationGetByIdProtoBuildList(ExecState*, EncodedJSValue encodedBase, Identifier*);
EncodedJSValue operationGetByIdOptimize(ExecState*, EncodedJSValue encodedBase, Identifier*);
EncodedJSValue operationGetMethodOptimize(ExecState*, EncodedJSValue encodedBase, Identifier*);
EncodedJSValue operationInstanceOf(ExecState*, EncodedJSValue value, EncodedJSValue base, EncodedJSValue prototype);
EncodedJSValue operationResolve(ExecState*, Identifier*);
EncodedJSValue operationResolveBase(ExecState*, Identifier*);
EncodedJSValue operationResolveBaseStrictPut(ExecState*, Identifier*);
void operationThrowHasInstanceError(ExecState*, EncodedJSValue base);
void operationPutByValStrict(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue);
void operationPutByValNonStrict(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue);
void operationPutByValBeyondArrayBounds(ExecState*, JSArray*, int32_t index, EncodedJSValue encodedValue);
void operationPutByIdStrict(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier*);
void operationPutByIdNonStrict(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier*);
void operationPutByIdDirectStrict(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier*);
void operationPutByIdDirectNonStrict(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier*);
void operationPutByIdStrictOptimize(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier*);
void operationPutByIdNonStrictOptimize(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier*);
void operationPutByIdDirectStrictOptimize(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier*);
void operationPutByIdDirectNonStrictOptimize(ExecState*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, Identifier*);
RegisterSizedBoolean operationCompareLess(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
RegisterSizedBoolean operationCompareLessEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
RegisterSizedBoolean operationCompareGreater(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
RegisterSizedBoolean operationCompareGreaterEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
RegisterSizedBoolean operationCompareEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
RegisterSizedBoolean operationCompareStrictEqCell(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
RegisterSizedBoolean operationCompareStrictEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
void* operationVirtualCall(ExecState*);
void* operationLinkCall(ExecState*);
void* operationVirtualConstruct(ExecState*);
void* operationLinkConstruct(ExecState*);

// This method is used to lookup an exception hander, keyed by faultLocation, which is
// the return location from one of the calls out to one of the helper operations above.
struct DFGHandler {
    DFGHandler(ExecState* exec, void* handler)
        : exec(exec)
        , handler(handler)
    {
    }

    ExecState* exec;
    void* handler;
};
DFGHandler lookupExceptionHandler(ExecState*, ReturnAddressPtr faultLocation);

// These operations implement the implicitly called ToInt32, ToNumber, and ToBoolean conversions from ES5.
double dfgConvertJSValueToNumber(ExecState*, EncodedJSValue);
int32_t dfgConvertJSValueToInt32(ExecState*, EncodedJSValue);
RegisterSizedBoolean dfgConvertJSValueToBoolean(ExecState*, EncodedJSValue);

} // extern "C"
} } // namespace JSC::DFG

#endif
#endif
