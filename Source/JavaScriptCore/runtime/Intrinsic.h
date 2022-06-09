/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#include "IterationKind.h"
#include <optional>

namespace JSC {

enum Intrinsic : uint8_t {
    // Call intrinsics.
    NoIntrinsic,
    AbsIntrinsic,
    ACosIntrinsic,
    ASinIntrinsic,
    ATanIntrinsic,
    ACoshIntrinsic,
    ASinhIntrinsic,
    ATanhIntrinsic,
    MinIntrinsic,
    MaxIntrinsic,
    SqrtIntrinsic,
    SinIntrinsic,
    CbrtIntrinsic,
    Clz32Intrinsic,
    CosIntrinsic,
    TanIntrinsic,
    CoshIntrinsic,
    SinhIntrinsic,
    TanhIntrinsic,
    ArrayPushIntrinsic,
    ArrayPopIntrinsic,
    ArraySliceIntrinsic,
    ArrayIndexOfIntrinsic,
    ArrayValuesIntrinsic,
    ArrayKeysIntrinsic,
    ArrayEntriesIntrinsic,
    CharCodeAtIntrinsic,
    CharAtIntrinsic,
    DatePrototypeGetTimeIntrinsic,
    DatePrototypeGetFullYearIntrinsic,
    DatePrototypeGetUTCFullYearIntrinsic,
    DatePrototypeGetMonthIntrinsic,
    DatePrototypeGetUTCMonthIntrinsic,
    DatePrototypeGetDateIntrinsic,
    DatePrototypeGetUTCDateIntrinsic,
    DatePrototypeGetDayIntrinsic,
    DatePrototypeGetUTCDayIntrinsic,
    DatePrototypeGetHoursIntrinsic,
    DatePrototypeGetUTCHoursIntrinsic,
    DatePrototypeGetMinutesIntrinsic,
    DatePrototypeGetUTCMinutesIntrinsic,
    DatePrototypeGetSecondsIntrinsic,
    DatePrototypeGetUTCSecondsIntrinsic,
    DatePrototypeGetMillisecondsIntrinsic,
    DatePrototypeGetUTCMillisecondsIntrinsic,
    DatePrototypeGetTimezoneOffsetIntrinsic,
    DatePrototypeGetYearIntrinsic,
    FromCharCodeIntrinsic,
    PowIntrinsic,
    FloorIntrinsic,
    CeilIntrinsic,
    RoundIntrinsic,
    ExpIntrinsic,
    Expm1Intrinsic,
    LogIntrinsic,
    Log10Intrinsic,
    Log1pIntrinsic,
    Log2Intrinsic,
    RegExpExecIntrinsic,
    RegExpTestIntrinsic,
    RegExpTestFastIntrinsic,
    RegExpMatchFastIntrinsic,
    ObjectAssignIntrinsic,
    ObjectCreateIntrinsic,
    ObjectGetOwnPropertyNamesIntrinsic,
    ObjectGetPrototypeOfIntrinsic,
    ObjectIsIntrinsic,
    ObjectKeysIntrinsic,
    ReflectGetPrototypeOfIntrinsic,
    StringPrototypeCodePointAtIntrinsic,
    StringPrototypeValueOfIntrinsic,
    StringPrototypeReplaceIntrinsic,
    StringPrototypeReplaceRegExpIntrinsic,
    StringPrototypeSliceIntrinsic,
    StringPrototypeToLowerCaseIntrinsic,
    NumberPrototypeToStringIntrinsic,
    NumberIsIntegerIntrinsic,
    IMulIntrinsic,
    RandomIntrinsic,
    FRoundIntrinsic,
    TruncIntrinsic,
    TypedArrayValuesIntrinsic,
    TypedArrayKeysIntrinsic,
    TypedArrayEntriesIntrinsic,
    IsTypedArrayViewIntrinsic,
    BoundFunctionCallIntrinsic,
    JSMapGetIntrinsic,
    JSMapHasIntrinsic,
    JSMapSetIntrinsic,
    JSMapValuesIntrinsic,
    JSMapKeysIntrinsic,
    JSMapEntriesIntrinsic,
    JSMapBucketHeadIntrinsic,
    JSMapBucketNextIntrinsic,
    JSMapBucketKeyIntrinsic,
    JSMapBucketValueIntrinsic,
    JSSetHasIntrinsic,
    JSSetAddIntrinsic,
    JSSetValuesIntrinsic,
    JSSetEntriesIntrinsic,
    JSSetBucketHeadIntrinsic,
    JSSetBucketNextIntrinsic,
    JSSetBucketKeyIntrinsic,
    JSWeakMapGetIntrinsic,
    JSWeakMapHasIntrinsic,
    JSWeakMapSetIntrinsic,
    JSWeakSetHasIntrinsic,
    JSWeakSetAddIntrinsic,
    HasOwnPropertyIntrinsic,
    AtomicsAddIntrinsic,
    AtomicsAndIntrinsic,
    AtomicsCompareExchangeIntrinsic,
    AtomicsExchangeIntrinsic,
    AtomicsIsLockFreeIntrinsic,
    AtomicsLoadIntrinsic,
    AtomicsNotifyIntrinsic,
    AtomicsOrIntrinsic,
    AtomicsStoreIntrinsic,
    AtomicsSubIntrinsic,
    AtomicsWaitIntrinsic,
    AtomicsXorIntrinsic,
    ParseIntIntrinsic,
    FunctionToStringIntrinsic,

    // Getter intrinsics.
    TypedArrayLengthIntrinsic,
    TypedArrayByteLengthIntrinsic,
    TypedArrayByteOffsetIntrinsic,
    UnderscoreProtoIntrinsic,

    // Debugging intrinsics. These are meant to be used as testing hacks within
    // jsc.cpp and should never be exposed to users.
    DFGTrueIntrinsic,
    FTLTrueIntrinsic,
    OSRExitIntrinsic,
    IsFinalTierIntrinsic,
    SetInt32HeapPredictionIntrinsic,
    CheckInt32Intrinsic,
    FiatInt52Intrinsic,

    // These are used for $vm performance debugging features.
    CPUMfenceIntrinsic,
    CPURdtscIntrinsic,
    CPUCpuidIntrinsic,
    CPUPauseIntrinsic,

    DataViewGetInt8,
    DataViewGetUint8,
    DataViewGetInt16,
    DataViewGetUint16,
    DataViewGetInt32,
    DataViewGetUint32,
    DataViewGetFloat32,
    DataViewGetFloat64,
    DataViewSetInt8,
    DataViewSetUint8,
    DataViewSetInt16,
    DataViewSetUint16,
    DataViewSetInt32,
    DataViewSetUint32,
    DataViewSetFloat32,
    DataViewSetFloat64,

    WasmFunctionIntrinsic,
};

std::optional<IterationKind> interationKindForIntrinsic(Intrinsic);

const char* intrinsicName(Intrinsic);

} // namespace JSC

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::Intrinsic);

} // namespace WTF


