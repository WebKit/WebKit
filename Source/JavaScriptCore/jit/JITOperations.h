/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JITOperations_h
#define JITOperations_h

#include "CallFrame.h"
#include "JSArray.h"
#include "JSCJSValue.h"
#include "MacroAssembler.h"
#include "PutKind.h"
#include "StructureStubInfo.h"
#include "Watchpoint.h"

namespace JSC {

#if CALLING_CONVENTION_IS_STDCALL
#define JIT_OPERATION CDECL
#else
#define JIT_OPERATION
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
    I: StringImpl*
*/
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_E)(ExecState*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EA)(ExecState*, JSArray*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EAZ)(ExecState*, JSArray*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EC)(ExecState*, JSCell*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ECI)(ExecState*, JSCell*, StringImpl*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ECJ)(ExecState*, JSCell*, EncodedJSValue);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EDA)(ExecState*, double, JSArray*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EI)(ExecState*, StringImpl*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJA)(ExecState*, EncodedJSValue, JSArray*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJI)(ExecState*, EncodedJSValue, StringImpl*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJJ)(ExecState*, EncodedJSValue, EncodedJSValue);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJssZ)(ExecState*, JSString*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJP)(ExecState*, EncodedJSValue, void*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EP)(ExecState*, void*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EPP)(ExecState*, void*, void*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EPS)(ExecState*, void*, size_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ESS)(ExecState*, size_t, size_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EZ)(ExecState*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EZIcfZ)(ExecState*, int32_t, InlineCallFrame*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EZZ)(ExecState*, int32_t, int32_t);
typedef JSCell* JIT_OPERATION (*C_JITOperation_E)(ExecState*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EZ)(ExecState*, int32_t);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EC)(ExecState*, JSCell*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EIcf)(ExecState*, InlineCallFrame*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EJssSt)(ExecState*, JSString*, Structure*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EJssJss)(ExecState*, JSString*, JSString*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EJssJssJss)(ExecState*, JSString*, JSString*, JSString*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EOZ)(ExecState*, JSObject*, int32_t);
typedef JSCell* JIT_OPERATION (*C_JITOperation_ESt)(ExecState*, Structure*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EZ)(ExecState*, int32_t);
typedef double JIT_OPERATION (*D_JITOperation_DD)(double, double);
typedef double JIT_OPERATION (*D_JITOperation_ZZ)(int32_t, int32_t);
typedef double JIT_OPERATION (*D_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef int32_t JIT_OPERATION (*Z_JITOperation_D)(double);
typedef size_t JIT_OPERATION (*S_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef size_t JIT_OPERATION (*S_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef size_t JIT_OPERATION (*S_JITOperation_EJJ)(ExecState*, EncodedJSValue, EncodedJSValue);
typedef size_t JIT_OPERATION (*S_JITOperation_J)(EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_E)(ExecState*);
typedef void JIT_OPERATION (*V_JITOperation_EOZD)(ExecState*, JSObject*, int32_t, double);
typedef void JIT_OPERATION (*V_JITOperation_EOZJ)(ExecState*, JSObject*, int32_t, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_EC)(ExecState*, JSCell*);
typedef void JIT_OPERATION (*V_JITOperation_ECIcf)(ExecState*, JSCell*, InlineCallFrame*);
typedef void JIT_OPERATION (*V_JITOperation_ECCIcf)(ExecState*, JSCell*, JSCell*, InlineCallFrame*);
typedef void JIT_OPERATION (*V_JITOperation_ECJJ)(ExecState*, JSCell*, EncodedJSValue, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_ECZ)(ExecState*, JSCell*, int32_t);
typedef void JIT_OPERATION (*V_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef void JIT_OPERATION (*V_JITOperation_EJCI)(ExecState*, EncodedJSValue, JSCell*, StringImpl*);
typedef void JIT_OPERATION (*V_JITOperation_EJJJ)(ExecState*, EncodedJSValue, EncodedJSValue, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_EJPP)(ExecState*, EncodedJSValue, void*, void*);
typedef void JIT_OPERATION (*V_JITOperation_EPZJ)(ExecState*, void*, int32_t, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_W)(WatchpointSet*);
typedef char* JIT_OPERATION (*P_JITOperation_E)(ExecState*);
typedef char* JIT_OPERATION (*P_JITOperation_EC)(ExecState*, JSCell*);
typedef char* JIT_OPERATION (*P_JITOperation_EJS)(ExecState*, EncodedJSValue, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_EO)(ExecState*, JSObject*);
typedef char* JIT_OPERATION (*P_JITOperation_EOS)(ExecState*, JSObject*, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_EOZ)(ExecState*, JSObject*, int32_t);
typedef char* JIT_OPERATION (*P_JITOperation_EPS)(ExecState*, void*, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_ES)(ExecState*, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_ESJss)(ExecState*, size_t, JSString*);
typedef char* JIT_OPERATION (*P_JITOperation_ESt)(ExecState*, Structure*);
typedef char* JIT_OPERATION (*P_JITOperation_EStJ)(ExecState*, Structure*, EncodedJSValue);
typedef char* JIT_OPERATION (*P_JITOperation_EStPS)(ExecState*, Structure*, void*, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_EStSS)(ExecState*, Structure*, size_t, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_EStZ)(ExecState*, Structure*, int32_t);
typedef char* JIT_OPERATION (*P_JITOperation_EZZ)(ExecState*, int32_t, int32_t);
typedef StringImpl* JIT_OPERATION (*I_JITOperation_EJss)(ExecState*, JSString*);
typedef JSString* JIT_OPERATION (*Jss_JITOperation_EZ)(ExecState*, int32_t);

extern "C" {

// This method is used to lookup an exception hander, keyed by faultLocation, which is
// the return location from one of the calls out to one of the helper operations above.

// According to C++ rules, a type used for the return signature of function with C linkage (i.e.
// 'extern "C"') needs to be POD; hence putting any constructors into it could cause either compiler
// warnings, or worse, a change in the ABI used to return these types.
struct JITHandler {
    union Union {
        struct Struct {
            ExecState* exec;
            void* handler;
        } s;
        uint64_t encoded;
    } u;
};

inline JITHandler createJITHandler(ExecState* exec, void* handler)
{
    JITHandler result;
    result.u.s.exec = exec;
    result.u.s.handler = handler;
    return result;
}

#if CPU(X86_64)
typedef JITHandler JITHandlerEncoded;
inline JITHandlerEncoded dfgHandlerEncoded(ExecState* exec, void* handler)
{
    return createJITHandler(exec, handler);
}
#else
typedef uint64_t JITHandlerEncoded;
inline JITHandlerEncoded dfgHandlerEncoded(ExecState* exec, void* handler)
{
    COMPILE_ASSERT(sizeof(JITHandler::Union) == sizeof(uint64_t), JITHandler_Union_is_64bit);
    return createJITHandler(exec, handler).u.encoded;
}
#endif
JITHandlerEncoded JIT_OPERATION lookupExceptionHandler(ExecState*) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationGetById(ExecState*, EncodedJSValue, StringImpl*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdBuildList(ExecState*, EncodedJSValue, StringImpl*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdOptimize(ExecState*, EncodedJSValue, StringImpl*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInOptimize(ExecState*, JSCell*, StringImpl*);
EncodedJSValue JIT_OPERATION operationIn(ExecState*, JSCell*, StringImpl*);
EncodedJSValue JIT_OPERATION operationGenericIn(ExecState*, JSCell*, EncodedJSValue);
EncodedJSValue JIT_OPERATION operationCallCustomGetter(ExecState*, JSCell*, PropertySlot::GetValueFunc, StringImpl*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationCallGetter(ExecState*, JSCell*, JSCell*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdStrict(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdNonStrict(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectStrict(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectNonStrict(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdStrictOptimize(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdNonStrictOptimize(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectStrictOptimize(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectNonStrictOptimize(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdStrictBuildList(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdNonStrictBuildList(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectStrictBuildList(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectNonStrictBuildList(ExecState*, EncodedJSValue encodedValue, JSCell* base, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationReallocateStorageAndFinishPut(ExecState*, JSObject*, Structure*, PropertyOffset, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationVirtualCall(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkCall(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkClosureCall(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationVirtualConstruct(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkConstruct(ExecState*) WTF_INTERNAL;

}

} // namespace JSC

#endif // JITOperations_h

