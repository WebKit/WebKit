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

#include "B3BasicBlockInlines.h"
#include "B3Const32Value.h"
#include "B3Const64Value.h"
#include "B3ConstDoubleValue.h"
#include "B3ConstPtrValue.h"
#include "B3ProcedureInlines.h"
#include "B3Type.h"
#include "B3ValueInlines.h"
#include "FTLAbbreviations.h"

#if ENABLE(FTL_JIT)

namespace JSC { namespace FTL {

using namespace B3;

CommonValues::CommonValues(LContext context)
#if FTL_USES_B3
    : voidType(B3::Void)
    , boolean(B3::Int32)
    , int32(B3::Int32)
    , int64(B3::Int64)
    , intPtr(B3::pointerType())
    , floatType(B3::Float)
    , doubleType(B3::Double)
#else
    : voidType(FTL::voidType(context))
    , boolean(int1Type(context))
    , int32(int32Type(context))
    , int64(int64Type(context))
    , intPtr(intPtrType(context))
    , floatType(FTL::floatType(context))
    , doubleType(FTL::doubleType(context))
    , int8(int8Type(context))
    , int16(int16Type(context))
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
    , intPtrEight(constInt(intPtr, 8, SignExtend))
    , doubleZero(constReal(doubleType, 0))
    , rangeKind(mdKindID(context, "range"))
    , profKind(mdKindID(context, "prof"))
    , branchWeights(mdString(context, "branch_weights"))
    , nonNegativeInt32(constInt(int32, 0, SignExtend), constInt(int32, 1ll << 31, SignExtend))
#endif // !FTL_USES_B3
    , m_context(context)
    , m_module(0)
{
}

#if FTL_USES_B3
void CommonValues::initializeConstants(B3::Procedure& proc, B3::BasicBlock* block)
{
    int32Zero = block->appendNew<Const32Value>(proc, Origin(), 0);
    int32One = block->appendNew<Const32Value>(proc, Origin(), 1);
    booleanTrue = int32One;
    booleanFalse = int32Zero;
    int64Zero = block->appendNew<Const64Value>(proc, Origin(), 0);
    intPtrZero = block->appendNew<ConstPtrValue>(proc, Origin(), 0);
    intPtrOne = block->appendNew<ConstPtrValue>(proc, Origin(), 1);
    intPtrTwo = block->appendNew<ConstPtrValue>(proc, Origin(), 2);
    intPtrThree = block->appendNew<ConstPtrValue>(proc, Origin(), 3);
    intPtrEight = block->appendNew<ConstPtrValue>(proc, Origin(), 8);
    doubleZero = block->appendNew<ConstDoubleValue>(proc, Origin(), 0.);
}
#endif

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

