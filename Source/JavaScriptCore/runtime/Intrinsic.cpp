/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "Intrinsic.h"

#include <wtf/PrintStream.h>

namespace JSC {

const char* intrinsicName(Intrinsic intrinsic)
{
    switch (intrinsic) {
    case NoIntrinsic:
        return "NoIntrinsic";
    case AbsIntrinsic:
        return "AbsIntrinsic";
    case ACosIntrinsic:
        return "ACosIntrinsic";
    case ASinIntrinsic:
        return "ASinIntrinsic";
    case ATanIntrinsic:
        return "ATanIntrinsic";
    case ACoshIntrinsic:
        return "ACoshIntrinsic";
    case ASinhIntrinsic:
        return "ASinhIntrinsic";
    case ATanhIntrinsic:
        return "ATanhIntrinsic";
    case MinIntrinsic:
        return "MinIntrinsic";
    case MaxIntrinsic:
        return "MaxIntrinsic";
    case SqrtIntrinsic:
        return "SqrtIntrinsic";
    case SinIntrinsic:
        return "SinIntrinsic";
    case CbrtIntrinsic:
        return "CbrtIntrinsic";
    case Clz32Intrinsic:
        return "Clz32Intrinsic";
    case CosIntrinsic:
        return "CosIntrinsic";
    case TanIntrinsic:
        return "TanIntrinsic";
    case CoshIntrinsic:
        return "CoshIntrinsic";
    case SinhIntrinsic:
        return "SinhIntrinsic";
    case TanhIntrinsic:
        return "TanhIntrinsic";
    case ArrayIndexOfIntrinsic:
        return "ArrayIndexOfIntrinsic";
    case ArrayPushIntrinsic:
        return "ArrayPushIntrinsic";
    case ArrayPopIntrinsic:
        return "ArrayPopIntrinsic";
    case ArraySliceIntrinsic:
        return "ArraySliceIntrinsic";
    case CharCodeAtIntrinsic:
        return "CharCodeAtIntrinsic";
    case CharAtIntrinsic:
        return "CharAtIntrinsic";
    case FromCharCodeIntrinsic:
        return "FromCharCodeIntrinsic";
    case PowIntrinsic:
        return "PowIntrinsic";
    case FloorIntrinsic:
        return "FloorIntrinsic";
    case CeilIntrinsic:
        return "CeilIntrinsic";
    case RoundIntrinsic:
        return "RoundIntrinsic";
    case ExpIntrinsic:
        return "ExpIntrinsic";
    case Expm1Intrinsic:
        return "Expm1Intrinsic";
    case LogIntrinsic:
        return "LogIntrinsic";
    case Log10Intrinsic:
        return "Log10Intrinsic";
    case Log1pIntrinsic:
        return "Log1pIntrinsic";
    case Log2Intrinsic:
        return "Log2Intrinsic";
    case RegExpExecIntrinsic:
        return "RegExpExecIntrinsic";
    case RegExpTestIntrinsic:
        return "RegExpTestIntrinsic";
    case RegExpTestFastIntrinsic:
        return "RegExpTestFastIntrinsic";
    case RegExpMatchFastIntrinsic:
        return "RegExpMatchFastIntrinsic";
    case ObjectCreateIntrinsic:
        return "ObjectCreateIntrinsic";
    case ObjectGetPrototypeOfIntrinsic:
        return "ObjectGetPrototypeOfIntrinsic";
    case ObjectIsIntrinsic:
        return "ObjectIsIntrinsic";
    case ObjectKeysIntrinsic:
        return "ObjectKeysIntrinsic";
    case ReflectGetPrototypeOfIntrinsic:
        return "ReflectGetPrototypeOfIntrinsic";
    case StringPrototypeValueOfIntrinsic:
        return "StringPrototypeValueOfIntrinsic";
    case StringPrototypeReplaceIntrinsic:
        return "StringPrototypeReplaceIntrinsic";
    case StringPrototypeReplaceRegExpIntrinsic:
        return "StringPrototypeReplaceRegExpIntrinsic";
    case StringPrototypeSliceIntrinsic:
        return "StringPrototypeSliceIntrinsic";
    case StringPrototypeToLowerCaseIntrinsic:
        return "StringPrototypeToLowerCaseIntrinsic";
    case NumberPrototypeToStringIntrinsic:
        return "NumberPrototypeToStringIntrinsic";
    case NumberIsIntegerIntrinsic:
        return "NumberIsIntegerIntrinsic";
    case IMulIntrinsic:
        return "IMulIntrinsic";
    case RandomIntrinsic:
        return "RandomIntrinsic";
    case FRoundIntrinsic:
        return "FRoundIntrinsic";
    case TruncIntrinsic:
        return "TruncIntrinsic";
    case IsTypedArrayViewIntrinsic:
        return "IsTypedArrayViewIntrinsic";
    case BoundThisNoArgsFunctionCallIntrinsic:
        return "BoundThisNoArgsFunctionCallIntrinsic";
    case JSMapGetIntrinsic:
        return "JSMapGetIntrinsic";
    case JSMapHasIntrinsic:
        return "JSMapHasIntrinsic";
    case JSMapSetIntrinsic:
        return "JSMapSetIntrinsic";
    case JSMapBucketHeadIntrinsic:
        return "JSMapBucketHeadIntrinsic";
    case JSMapBucketNextIntrinsic:
        return "JSMapBucketNextIntrinsic";
    case JSMapBucketKeyIntrinsic:
        return "JSMapBucketKeyIntrinsic";
    case JSMapBucketValueIntrinsic:
        return "JSMapBucketValueIntrinsic";
    case JSSetHasIntrinsic:
        return "JSSetHasIntrinsic";
    case JSSetAddIntrinsic:
        return "JSSetAddIntrinsic";
    case JSSetBucketHeadIntrinsic:
        return "JSSetBucketHeadIntrinsic";
    case JSSetBucketNextIntrinsic:
        return "JSSetBucketNextIntrinsic";
    case JSSetBucketKeyIntrinsic:
        return "JSSetBucketKeyIntrinsic";
    case JSWeakMapGetIntrinsic:
        return "JSWeakMapGetIntrinsic";
    case JSWeakMapHasIntrinsic:
        return "JSWeakMapHasIntrinsic";
    case JSWeakMapSetIntrinsic:
        return "JSWeakMapSetIntrinsic";
    case JSWeakSetHasIntrinsic:
        return "JSWeakSetHasIntrinsic";
    case JSWeakSetAddIntrinsic:
        return "JSWeakSetAddIntrinsic";
    case HasOwnPropertyIntrinsic:
        return "HasOwnPropertyIntrinsic";
    case AtomicsAddIntrinsic:
        return "AtomicsAddIntrinsic";
    case AtomicsAndIntrinsic:
        return "AtomicsAndIntrinsic";
    case AtomicsCompareExchangeIntrinsic:
        return "AtomicsCompareExchangeIntrinsic";
    case AtomicsExchangeIntrinsic:
        return "AtomicsExchangeIntrinsic";
    case AtomicsIsLockFreeIntrinsic:
        return "AtomicsIsLockFreeIntrinsic";
    case AtomicsLoadIntrinsic:
        return "AtomicsLoadIntrinsic";
    case AtomicsOrIntrinsic:
        return "AtomicsOrIntrinsic";
    case AtomicsStoreIntrinsic:
        return "AtomicsStoreIntrinsic";
    case AtomicsSubIntrinsic:
        return "AtomicsSubIntrinsic";
    case AtomicsWaitIntrinsic:
        return "AtomicsWaitIntrinsic";
    case AtomicsWakeIntrinsic:
        return "AtomicsWakeIntrinsic";
    case AtomicsXorIntrinsic:
        return "AtomicsXorIntrinsic";
    case ParseIntIntrinsic:
        return "ParseIntIntrinsic";
    case TypedArrayLengthIntrinsic:
        return "TypedArrayLengthIntrinsic";
    case TypedArrayByteLengthIntrinsic:
        return "TypedArrayByteLengthIntrinsic";
    case TypedArrayByteOffsetIntrinsic:
        return "TypedArrayByteOffsetIntrinsic";
    case UnderscoreProtoIntrinsic:
        return "UnderscoreProtoIntrinsic";
    case DFGTrueIntrinsic:
        return "DFGTrueIntrinsic";
    case FTLTrueIntrinsic:
        return "FTLTrueIntrinsic";
    case OSRExitIntrinsic:
        return "OSRExitIntrinsic";
    case IsFinalTierIntrinsic:
        return "IsFinalTierIntrinsic";
    case SetInt32HeapPredictionIntrinsic:
        return "SetInt32HeapPredictionIntrinsic";
    case CheckInt32Intrinsic:
        return "CheckInt32Intrinsic";
    case FiatInt52Intrinsic:
        return "FiatInt52Intrinsic";
    case CPUMfenceIntrinsic:
        return "CPUMfenceIntrinsic";
    case CPURdtscIntrinsic:
        return "CPURdtscIntrinsic";
    case CPUCpuidIntrinsic:
        return "CPUCpuidIntrinsic";
    case CPUPauseIntrinsic:
        return "CPUPauseIntrinsic";
    case DataViewGetInt8:
        return "DataViewGetInt8";
    case DataViewGetUint8:
        return "DataViewGetUint8";
    case DataViewGetInt16:
        return "DataViewGetInt16";
    case DataViewGetUint16:
        return "DataViewGetUint16";
    case DataViewGetInt32:
        return "DataViewGetInt32";
    case DataViewGetUint32:
        return "DataViewGetUint32";
    case DataViewGetFloat32:
        return "DataViewGetFloat32";
    case DataViewGetFloat64:
        return "DataViewGetFloat64";
    case DataViewSetInt8:
        return "DataViewSetInt8";
    case DataViewSetUint8:
        return "DataViewSetUint8";
    case DataViewSetInt16:
        return "DataViewSetInt16";
    case DataViewSetUint16:
        return "DataViewSetUint16";
    case DataViewSetInt32:
        return "DataViewSetInt32";
    case DataViewSetUint32:
        return "DataViewSetUint32";
    case DataViewSetFloat32:
        return "DataViewSetFloat32";
    case DataViewSetFloat64:
        return "DataViewSetFloat64";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::Intrinsic intrinsic)
{
    out.print(JSC::intrinsicName(intrinsic));
}

} // namespace WTF

