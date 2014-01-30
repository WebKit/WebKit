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

#ifndef FTLIntrinsicRepository_h
#define FTLIntrinsicRepository_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "DFGOperations.h"
#include "FTLAbbreviations.h"
#include "FTLCommonValues.h"

namespace JSC { namespace FTL {

#define FOR_EACH_FTL_INTRINSIC(macro) \
    macro(addWithOverflow32, "llvm.sadd.with.overflow.i32", functionType(structType(m_context, int32, boolean), int32, int32)) \
    macro(addWithOverflow64, "llvm.sadd.with.overflow.i64", functionType(structType(m_context, int64, boolean), int64, int64)) \
    macro(doubleAbs, "llvm.fabs.f64", functionType(doubleType, doubleType)) \
    macro(doubleSin, "llvm.sin.f64", functionType(doubleType, doubleType)) \
    macro(doubleCos, "llvm.cos.f64", functionType(doubleType, doubleType)) \
    macro(doubleSqrt, "llvm.sqrt.f64", functionType(doubleType, doubleType)) \
    macro(frameAddress, "llvm.frameaddress", functionType(pointerType(int8), int32)) \
    macro(mulWithOverflow32, "llvm.smul.with.overflow.i32", functionType(structType(m_context, int32, boolean), int32, int32)) \
    macro(mulWithOverflow64, "llvm.smul.with.overflow.i64", functionType(structType(m_context, int64, boolean), int64, int64)) \
    macro(patchpointInt64, "llvm.experimental.patchpoint.i64", functionType(int64, int64, int32, ref8, int32, Variadic)) \
    macro(patchpointVoid, "llvm.experimental.patchpoint.void", functionType(voidType, int64, int32, ref8, int32, Variadic)) \
    macro(stackmap, "llvm.experimental.stackmap", functionType(voidType, int64, int32, Variadic)) \
    macro(subWithOverflow32, "llvm.ssub.with.overflow.i32", functionType(structType(m_context, int32, boolean), int32, int32)) \
    macro(subWithOverflow64, "llvm.ssub.with.overflow.i64", functionType(structType(m_context, int64, boolean), int64, int64)) \
    macro(trap, "llvm.trap", functionType(voidType)) \
    macro(x86SSE2CvtTSD2SI, "llvm.x86.sse2.cvttsd2si", functionType(int32, vectorType(doubleType, 2)))

#define FOR_EACH_FUNCTION_TYPE(macro) \
    macro(C_JITOperation_EC, functionType(intPtr, intPtr, intPtr)) \
    macro(C_JITOperation_EJ, functionType(intPtr, intPtr, int64)) \
    macro(C_JITOperation_EJssJss, functionType(intPtr, intPtr, intPtr, intPtr)) \
    macro(C_JITOperation_EJssJssJss, functionType(intPtr, intPtr, intPtr, intPtr, intPtr)) \
    macro(C_JITOperation_ESt, functionType(intPtr, intPtr, intPtr)) \
    macro(I_JITOperation_EJss, functionType(intPtr, intPtr, intPtr)) \
    macro(J_JITOperation_E, functionType(int64, intPtr)) \
    macro(J_JITOperation_EA, functionType(int64, intPtr, intPtr)) \
    macro(J_JITOperation_EAZ, functionType(int64, intPtr, intPtr, int32)) \
    macro(J_JITOperation_EDA, functionType(int64, intPtr, doubleType, intPtr)) \
    macro(J_JITOperation_EJA, functionType(int64, intPtr, int64, intPtr)) \
    macro(J_JITOperation_EJJ, functionType(int64, intPtr, int64, int64)) \
    macro(J_JITOperation_EJssZ, functionType(int64, intPtr, intPtr, int32)) \
    macro(J_JITOperation_ESsiJI, functionType(int64, intPtr, intPtr, int64, intPtr)) \
    macro(Jss_JITOperation_EZ, functionType(intPtr, intPtr, int32)) \
    macro(P_JITOperation_E, functionType(intPtr, intPtr)) \
    macro(P_JITOperation_EC, functionType(intPtr, intPtr, intPtr)) \
    macro(P_JITOperation_EO, functionType(intPtr, intPtr, intPtr)) \
    macro(P_JITOperation_ESt, functionType(intPtr, intPtr, intPtr)) \
    macro(P_JITOperation_EStPS, functionType(intPtr, intPtr, intPtr, intPtr, intPtr)) \
    macro(P_JITOperation_EStSS, functionType(intPtr, intPtr, intPtr, intPtr, intPtr)) \
    macro(P_JITOperation_EStZ, functionType(intPtr, intPtr, intPtr, int32)) \
    macro(S_JITOperation_EJ, functionType(intPtr, intPtr, int64)) \
    macro(S_JITOperation_EJJ, functionType(intPtr, intPtr, int64, int64)) \
    macro(V_JITOperation_EJJJ, functionType(voidType, intPtr, int64, int64, int64)) \
    macro(V_JITOperation_EOZD, functionType(voidType, intPtr, intPtr, int32, doubleType)) \
    macro(V_JITOperation_EOZJ, functionType(voidType, intPtr, intPtr, int32, int64)) \
    macro(V_JITOperation_EC, functionType(voidType, intPtr, intPtr)) \
    macro(V_JITOperation_EVws, functionType(voidType, intPtr, intPtr)) \
    macro(Z_JITOperation_D, functionType(int32, doubleType))

class IntrinsicRepository : public CommonValues {
public:
    IntrinsicRepository(LContext);
    
#define INTRINSIC_GETTER(ourName, llvmName, type) \
    LValue ourName##Intrinsic() {                 \
        if (!m_##ourName)                         \
            return ourName##IntrinsicSlow();      \
        return m_##ourName;                       \
    }
    FOR_EACH_FTL_INTRINSIC(INTRINSIC_GETTER)
#undef INTRINSIC_GETTER

#define FUNCTION_TYPE_GETTER(typeName, type) \
    LType typeName()                         \
    {                                        \
        if (!m_##typeName)                   \
            return typeName##Slow();         \
        return m_##typeName;                 \
    }
    FOR_EACH_FUNCTION_TYPE(FUNCTION_TYPE_GETTER)
#undef FUNCTION_TYPE_GETTER
    
#define FUNCTION_TYPE_RESOLVER(typeName, type) \
    LType operationType(JSC::typeName)         \
    {                                          \
        return typeName();                     \
    }
    FOR_EACH_FUNCTION_TYPE(FUNCTION_TYPE_RESOLVER)
#undef FUNCTION_TYPE_RESOLVER
    
private:
#define INTRINSIC_GETTER_SLOW_DECLARATION(ourName, llvmName, type) \
    LValue ourName##IntrinsicSlow();
    FOR_EACH_FTL_INTRINSIC(INTRINSIC_GETTER_SLOW_DECLARATION)
#undef INTRINSIC_GETTER

#define INTRINSIC_FIELD_DECLARATION(ourName, llvmName, type) LValue m_##ourName;
    FOR_EACH_FTL_INTRINSIC(INTRINSIC_FIELD_DECLARATION)
#undef INTRINSIC_FIELD_DECLARATION

#define FUNCTION_TYPE_GETTER_SLOW_DECLARATION(typeName, type) \
    LType typeName##Slow();
    FOR_EACH_FUNCTION_TYPE(FUNCTION_TYPE_GETTER_SLOW_DECLARATION)
#undef FUNCTION_TYPE_GETTER_SLOW_DECLARATION

#define FUNCTION_TYPE_FIELD_DECLARATION(typeName, type) \
    LType m_##typeName;
    FOR_EACH_FUNCTION_TYPE(FUNCTION_TYPE_FIELD_DECLARATION)
#undef FUNCTION_TYPE_FIELD_DECLARATION
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLIntrinsicRepository_h

