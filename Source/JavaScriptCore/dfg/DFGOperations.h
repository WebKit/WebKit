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

namespace JSC {

struct GlobalResolveInfo;

namespace DFG {

enum PutKind { Direct, NotDirect };

extern "C" {

#if CALLING_CONVENTION_IS_STDCALL
#define DFG_OPERATION CDECL
#else
#define DFG_OPERATION
#endif

// These typedefs provide typechecking when generating calls out to helper routines;
// this helps prevent calling a helper routine with the wrong arguments!
/*
    Key:
    V: void
    J: JSValue
    P: pointer (void*)
    C: JSCell*
    A: JSArray*
    S: size_t
    Z: int32_t
    D: double
    I: Identifier*
    G: GlobalResolveInfo*
*/
typedef int32_t DFG_OPERATION (*Z_DFGOperation_D)(double);
typedef JSCell* DFG_OPERATION (*C_DFGOperation_E)(ExecState*);
typedef JSCell* DFG_OPERATION (*C_DFGOperation_EC)(ExecState*, JSCell*);
typedef JSCell* DFG_OPERATION (*C_DFGOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EA)(ExecState*, JSArray*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EJA)(ExecState*, EncodedJSValue, JSArray*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_ECJ)(ExecState*, JSCell*, EncodedJSValue);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EJJ)(ExecState*, EncodedJSValue, EncodedJSValue);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EJ)(ExecState*, EncodedJSValue);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EJP)(ExecState*, EncodedJSValue, void*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_ECI)(ExecState*, JSCell*, Identifier*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EJI)(ExecState*, EncodedJSValue, Identifier*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EP)(ExecState*, void*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EPP)(ExecState*, void*, void*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EGI)(ExecState*, GlobalResolveInfo*, Identifier*);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EPS)(ExecState*, void*, size_t);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_ESS)(ExecState*, size_t, size_t);
typedef EncodedJSValue DFG_OPERATION (*J_DFGOperation_EI)(ExecState*, Identifier*);
typedef size_t DFG_OPERATION (*S_DFGOperation_EJ)(ExecState*, EncodedJSValue);
typedef size_t DFG_OPERATION (*S_DFGOperation_EJJ)(ExecState*, EncodedJSValue, EncodedJSValue);
typedef void DFG_OPERATION (*V_DFGOperation_EJJJ)(ExecState*, EncodedJSValue, EncodedJSValue, EncodedJSValue);
typedef void DFG_OPERATION (*V_DFGOperation_ECJJ)(ExecState*, JSCell*, EncodedJSValue, EncodedJSValue);
typedef void DFG_OPERATION (*V_DFGOperation_EJPP)(ExecState*, EncodedJSValue, EncodedJSValue, void*);
typedef void DFG_OPERATION (*V_DFGOperation_EJCI)(ExecState*, EncodedJSValue, JSCell*, Identifier*);
typedef void DFG_OPERATION (*V_DFGOperation_EPZJ)(ExecState*, void*, int32_t, EncodedJSValue);
typedef void DFG_OPERATION (*V_DFGOperation_EAZJ)(ExecState*, JSArray*, int32_t, EncodedJSValue);
typedef double DFG_OPERATION (*D_DFGOperation_DD)(double, double);
typedef double DFG_OPERATION (*D_DFGOperation_EJ)(ExecState*, EncodedJSValue);
typedef void* DFG_OPERATION (*P_DFGOperation_E)(ExecState*);

// These routines are provide callbacks out to C++ implementations of operations too complex to JIT.
JSCell* DFG_OPERATION operationNewObject(ExecState*);
JSCell* DFG_OPERATION operationCreateThis(ExecState*, JSCell* encodedOp1);
JSCell* DFG_OPERATION operationCreateThisInlined(ExecState*, JSCell* encodedOp1, JSCell* constructor);
EncodedJSValue DFG_OPERATION operationConvertThis(ExecState*, EncodedJSValue encodedOp1);
EncodedJSValue DFG_OPERATION operationValueAdd(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
EncodedJSValue DFG_OPERATION operationValueAddNotNumber(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
EncodedJSValue DFG_OPERATION operationGetByVal(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty);
EncodedJSValue DFG_OPERATION operationGetByValCell(ExecState*, JSCell*, EncodedJSValue encodedProperty);
EncodedJSValue DFG_OPERATION operationGetById(ExecState*, EncodedJSValue, Identifier*);
EncodedJSValue DFG_OPERATION operationGetByIdBuildList(ExecState*, EncodedJSValue, Identifier*);
EncodedJSValue DFG_OPERATION operationGetByIdProtoBuildList(ExecState*, EncodedJSValue, Identifier*);
EncodedJSValue DFG_OPERATION operationGetByIdOptimize(ExecState*, EncodedJSValue, Identifier*);
EncodedJSValue DFG_OPERATION operationGetMethodOptimize(ExecState*, EncodedJSValue, Identifier*);
EncodedJSValue DFG_OPERATION operationResolve(ExecState*, Identifier*);
EncodedJSValue DFG_OPERATION operationResolveBase(ExecState*, Identifier*);
EncodedJSValue DFG_OPERATION operationResolveBaseStrictPut(ExecState*, Identifier*);
EncodedJSValue DFG_OPERATION operationResolveGlobal(ExecState*, GlobalResolveInfo*, Identifier*);
EncodedJSValue DFG_OPERATION operationToPrimitive(ExecState*, EncodedJSValue);
EncodedJSValue DFG_OPERATION operationStrCat(ExecState*, void* start, size_t);
EncodedJSValue DFG_OPERATION operationNewArray(ExecState*, void* start, size_t);
EncodedJSValue DFG_OPERATION operationNewArrayBuffer(ExecState*, size_t, size_t);
EncodedJSValue DFG_OPERATION operationNewRegexp(ExecState*, void*);
void DFG_OPERATION operationPutByValStrict(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue);
void DFG_OPERATION operationPutByValNonStrict(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue);
void DFG_OPERATION operationPutByValCellStrict(ExecState*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue);
void DFG_OPERATION operationPutByValCellNonStrict(ExecState*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue);
void DFG_OPERATION operationPutByValBeyondArrayBounds(ExecState*, JSArray*, int32_t index, EncodedJSValue encodedValue);
EncodedJSValue DFG_OPERATION operationArrayPush(ExecState*, EncodedJSValue encodedValue, JSArray*);
EncodedJSValue DFG_OPERATION operationArrayPop(ExecState*, JSArray*);
void DFG_OPERATION operationPutByIdStrict(ExecState*, EncodedJSValue encodedValue, JSCell* base, Identifier*);
void DFG_OPERATION operationPutByIdNonStrict(ExecState*, EncodedJSValue encodedValue, JSCell* base, Identifier*);
void DFG_OPERATION operationPutByIdDirectStrict(ExecState*, EncodedJSValue encodedValue, JSCell* base, Identifier*);
void DFG_OPERATION operationPutByIdDirectNonStrict(ExecState*, EncodedJSValue encodedValue, JSCell* base, Identifier*);
void DFG_OPERATION operationPutByIdStrictOptimize(ExecState*, EncodedJSValue encodedValue, JSCell* base, Identifier*);
void DFG_OPERATION operationPutByIdNonStrictOptimize(ExecState*, EncodedJSValue encodedValue, JSCell* base, Identifier*);
void DFG_OPERATION operationPutByIdDirectStrictOptimize(ExecState*, EncodedJSValue encodedValue, JSCell* base, Identifier*);
void DFG_OPERATION operationPutByIdDirectNonStrictOptimize(ExecState*, EncodedJSValue encodedValue, JSCell* base, Identifier*);
// These comparisons return a boolean within a size_t such that the value is zero extended to fill the register.
size_t DFG_OPERATION operationCompareLess(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
size_t DFG_OPERATION operationCompareLessEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
size_t DFG_OPERATION operationCompareGreater(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
size_t DFG_OPERATION operationCompareGreaterEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
size_t DFG_OPERATION operationCompareEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
size_t DFG_OPERATION operationCompareStrictEqCell(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
size_t DFG_OPERATION operationCompareStrictEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);
void* DFG_OPERATION operationVirtualCall(ExecState*);
void* DFG_OPERATION operationLinkCall(ExecState*);
void* DFG_OPERATION operationVirtualConstruct(ExecState*);
void* DFG_OPERATION operationLinkConstruct(ExecState*);

// This method is used to lookup an exception hander, keyed by faultLocation, which is
// the return location from one of the calls out to one of the helper operations above.
struct DFGHandler {
    DFGHandler(ExecState* exec, void* handler)
    {
        u.s.exec = exec;
        u.s.handler = handler;
    }

#if !CPU(X86_64)
    uint64_t encoded()
    {
        COMPILE_ASSERT(sizeof(Union) == sizeof(uint64_t), DFGHandler_Union_is_64bit);
        return u.encoded;
    }
#endif

    union Union {
        struct Struct {
            ExecState* exec;
            void* handler;
        } s;
        uint64_t encoded;
    } u;
};
#if CPU(X86_64)
typedef DFGHandler DFGHandlerEncoded;
inline DFGHandlerEncoded dfgHandlerEncoded(ExecState* exec, void* handler)
{
    return DFGHandler(exec, handler);
}
#else
typedef uint64_t DFGHandlerEncoded;
inline DFGHandlerEncoded dfgHandlerEncoded(ExecState* exec, void* handler)
{
    return DFGHandler(exec, handler).encoded();
}
#endif
DFGHandlerEncoded DFG_OPERATION lookupExceptionHandler(ExecState*, ReturnAddressPtr faultLocation);

// These operations implement the implicitly called ToInt32, ToNumber, and ToBoolean conversions from ES5.
double DFG_OPERATION dfgConvertJSValueToNumber(ExecState*, EncodedJSValue);
// This conversion returns an int32_t within a size_t such that the value is zero extended to fill the register.
size_t DFG_OPERATION dfgConvertJSValueToInt32(ExecState*, EncodedJSValue);
size_t DFG_OPERATION dfgConvertJSValueToBoolean(ExecState*, EncodedJSValue);

#if DFG_ENABLE(VERBOSE_SPECULATION_FAILURE)
void DFG_OPERATION debugOperationPrintSpeculationFailure(ExecState*, void*);
#endif

} // extern "C"
} } // namespace JSC::DFG

#endif
#endif
