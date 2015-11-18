/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FTLCommonValues.h"

#include "B3Type.h"
#include "FTLAbbreviations.h"

#if ENABLE(FTL_JIT)

namespace JSC { namespace FTL {

CommonValues::CommonValues(LContext context)
#if FTL_USES_B3
    : voidType(B3::Void)
    , boolean(B3::Int32)
    , int32(B3::Int32)
    , int64(B3::Int64)
    , intPtr(B3::pointerType())
    , doubleType(B3::Double)
#else
    : voidType(FTL::voidType(context))
    , boolean(int1Type(context))
    , int32(int32Type(context))
    , int64(int64Type(context))
    , intPtr(intPtrType(context))
    , doubleType(FTL::doubleType(context))
    , int8(int8Type(context))
    , int16(int16Type(context))
    , floatType(FTL::floatType(context))
    , ref8(pointerType(int8))
    , ref16(pointerType(int16))
    , ref32(pointerType(int32))
    , ref64(pointerType(int64))
    , refPtr(pointerType(intPtr))
    , refFloat(pointerType(floatType))
    , refDouble(pointerType(doubleType))
    , booleanTrue(constInt(boolean, true, ZeroExtend))
    , booleanFalse(constInt(boolean, false, ZeroExtend))
    , int8Zero(constInt(int8, 0, SignExtend))
    , int32Zero(constInt(int32, 0, SignExtend))
    , int32One(constInt(int32, 1, SignExtend))
    , int64Zero(constInt(int64, 0, SignExtend))
    , intPtrZero(constInt(intPtr, 0, SignExtend))
    , intPtrOne(constInt(intPtr, 1, SignExtend))
    , intPtrTwo(constInt(intPtr, 2, SignExtend))
    , intPtrThree(constInt(intPtr, 3, SignExtend))
    , intPtrFour(constInt(intPtr, 4, SignExtend))
    , intPtrEight(constInt(intPtr, 8, SignExtend))
    , intPtrPtr(constInt(intPtr, sizeof(void*), SignExtend))
    , doubleZero(constReal(doubleType, 0))
    , rangeKind(mdKindID(context, "range"))
    , profKind(mdKindID(context, "prof"))
    , branchWeights(mdString(context, "branch_weights"))
    , nonNegativeInt32(constInt(int32, 0, SignExtend), constInt(int32, 1ll << 31, SignExtend))
#endif // !FTL_USES_B3
    , m_context(context)
    , m_module(0)
{
#if FTL_USES_B3
    // Plenty of values are uninitialized. The branch is just there for NORETURN.
    if (!m_module)
        CRASH();
#endif
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

