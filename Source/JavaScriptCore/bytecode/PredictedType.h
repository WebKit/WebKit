/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PredictedType_h
#define PredictedType_h

#include "ValueProfile.h"

namespace JSC {

typedef uint8_t PredictedType;
static const PredictedType PredictNone   = 0;
static const PredictedType PredictCell   = 0x01;
static const PredictedType PredictArray  = 0x03;
static const PredictedType PredictInt32  = 0x04;
static const PredictedType PredictDouble = 0x08;
static const PredictedType PredictNumber = 0x0c;
static const PredictedType PredictBoolean = 0x10;
static const PredictedType PredictTop    = 0x1f;
static const PredictedType StrongPredictionTag = 0x80;
static const PredictedType PredictionTagMask    = 0x80;

enum PredictionSource { WeakPrediction, StrongPrediction };

inline bool isCellPrediction(PredictedType value)
{
    return (value & PredictCell) == PredictCell && !(value & ~(PredictArray | PredictionTagMask));
}

inline bool isArrayPrediction(PredictedType value)
{
    return (value & ~PredictionTagMask) == PredictArray;
}

inline bool isInt32Prediction(PredictedType value)
{
    return (value & ~PredictionTagMask) == PredictInt32;
}

inline bool isDoublePrediction(PredictedType value)
{
    return (value & ~PredictionTagMask) == PredictDouble;
}

inline bool isNumberPrediction(PredictedType value)
{
    return !!(value & PredictNumber) && !(value & ~(PredictNumber | PredictionTagMask));
}

inline bool isBooleanPrediction(PredictedType value)
{
    return (value & ~PredictionTagMask) == PredictBoolean;
}

inline bool isStrongPrediction(PredictedType value)
{
    ASSERT(value != (PredictNone | StrongPredictionTag));
    return !!(value & StrongPredictionTag);
}

#ifndef NDEBUG
inline const char* predictionToString(PredictedType value)
{
    if (isStrongPrediction(value)) {
        switch (value & ~PredictionTagMask) {
        case PredictNone:
            return "p-strong-bottom";
        case PredictCell:
            return "p-strong-cell";
        case PredictArray:
            return "p-strong-array";
        case PredictInt32:
            return "p-strong-int32";
        case PredictDouble:
            return "p-strong-double";
        case PredictNumber:
            return "p-strong-number";
        case PredictBoolean:
            return "p-strong-boolean";
        default:
            return "p-strong-top";
        }
    }
    switch (value) {
    case PredictNone:
        return "p-weak-bottom";
    case PredictCell:
        return "p-weak-cell";
    case PredictArray:
        return "p-weak-array";
    case PredictInt32:
        return "p-weak-int32";
    case PredictDouble:
        return "p-weak-double";
    case PredictNumber:
        return "p-weak-number";
    case PredictBoolean:
        return "p-weak-boolean";
    default:
        return "p-weak-top";
    }
}
#endif

inline PredictedType mergePredictions(PredictedType left, PredictedType right)
{
    if (isStrongPrediction(left) == isStrongPrediction(right))
        return left | right;
    if (isStrongPrediction(left)) {
        ASSERT(!isStrongPrediction(right));
        return left;
    }
    ASSERT(!isStrongPrediction(left));
    ASSERT(isStrongPrediction(right));
    return right;
}

template<typename T>
inline bool mergePrediction(T& left, PredictedType right)
{
    PredictedType newPrediction = static_cast<T>(mergePredictions(static_cast<PredictedType>(left), right));
    bool result = newPrediction != static_cast<PredictedType>(left);
    left = newPrediction;
    return result;
}

inline PredictedType makePrediction(PredictedType type, PredictionSource source)
{
    ASSERT(!(type & StrongPredictionTag));
    ASSERT(source == StrongPrediction || source == WeakPrediction);
    if (type == PredictNone)
        return PredictNone;
    return type | (source == StrongPrediction ? StrongPredictionTag : 0);
}

#if ENABLE(VALUE_PROFILER)
inline PredictedType makePrediction(JSGlobalData& globalData, const ValueProfile& profile)
{
    ValueProfile::Statistics statistics;
    profile.computeStatistics(globalData, statistics);
    
    if (!statistics.samples)
        return PredictNone;
    
    if (statistics.int32s == statistics.samples)
        return StrongPredictionTag | PredictInt32;
    
    if (statistics.doubles == statistics.samples)
        return StrongPredictionTag | PredictDouble;
    
    if (statistics.int32s + statistics.doubles == statistics.samples)
        return StrongPredictionTag | PredictNumber;
    
    if (statistics.arrays == statistics.samples)
        return StrongPredictionTag | PredictArray;
    
    if (statistics.cells == statistics.samples)
        return StrongPredictionTag | PredictCell;
    
    if (statistics.booleans == statistics.samples)
        return StrongPredictionTag | PredictBoolean;
    
    return StrongPredictionTag | PredictTop;
}
#endif

} // namespace JSC

#endif // PredictedType_h
