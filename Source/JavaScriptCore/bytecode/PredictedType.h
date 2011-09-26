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

#include "JSValue.h"

namespace JSC {

class Structure;

typedef uint16_t PredictedType;
static const PredictedType PredictNone          = 0x0000; // We don't know anything yet.
static const PredictedType PredictFinalObject   = 0x0001; // It's definitely a JSFinalObject.
static const PredictedType PredictArray         = 0x0002; // It's definitely a JSArray.
static const PredictedType PredictObjectOther   = 0x0010; // It's definitely an object but not JSFinalObject or JSArray.
static const PredictedType PredictObjectUnknown = 0x0020; // It's definitely an object, but we didn't record enough informatin to know more.
static const PredictedType PredictObjectMask    = 0x003f; // Bitmask used for testing for any kind of object prediction.
static const PredictedType PredictString        = 0x0040; // It's definitely a JSString.
static const PredictedType PredictCellOther     = 0x0080; // It's definitely a JSCell but not a subclass of JSObject and definitely not a JSString.
static const PredictedType PredictCell          = 0x00ff; // It's definitely a JSCell.
static const PredictedType PredictInt32         = 0x0100; // It's definitely an Int32.
static const PredictedType PredictDouble        = 0x0200; // It's definitely a Double.
static const PredictedType PredictNumber        = 0x0300; // It's either an Int32 or a Double.
static const PredictedType PredictBoolean       = 0x0400; // It's definitely a Boolean.
static const PredictedType PredictOther         = 0x4000; // It's definitely none of the above.
static const PredictedType PredictTop           = 0x7fff; // It can be any of the above.

inline bool isCellPrediction(PredictedType value)
{
    return !!(value & PredictCell) && !(value & ~PredictCell);
}

inline bool isObjectPrediction(PredictedType value)
{
    return !!(value & PredictObjectMask) && !(value & ~PredictObjectMask);
}

inline bool isFinalObjectPrediction(PredictedType value)
{
    return value == PredictFinalObject;
}

inline bool isStringPrediction(PredictedType value)
{
    return value == PredictString;
}

inline bool isArrayPrediction(PredictedType value)
{
    return value == PredictArray;
}

inline bool isInt32Prediction(PredictedType value)
{
    return value == PredictInt32;
}

inline bool isDoublePrediction(PredictedType value)
{
    return value == PredictDouble;
}

inline bool isNumberPrediction(PredictedType value)
{
    return !!(value & PredictNumber) && !(value & ~PredictNumber);
}

inline bool isBooleanPrediction(PredictedType value)
{
    return value == PredictBoolean;
}

#ifndef NDEBUG
const char* predictionToString(PredictedType value);
#endif

inline PredictedType mergePredictions(PredictedType left, PredictedType right)
{
    if (left & PredictObjectUnknown) {
        ASSERT(!(left & (PredictObjectMask & ~PredictObjectUnknown)));
        if (right & PredictObjectMask)
            return (left & ~PredictObjectUnknown) | right;
    } else if (right & PredictObjectUnknown) {
        ASSERT(!(right & (PredictObjectMask & ~PredictObjectUnknown)));
        if (left & PredictObjectMask)
            return (right & ~PredictObjectUnknown) | left;
    }
    return left | right;
}

template<typename T>
inline bool mergePrediction(T& left, PredictedType right)
{
    PredictedType newPrediction = static_cast<T>(mergePredictions(static_cast<PredictedType>(left), right));
    bool result = newPrediction != static_cast<PredictedType>(left);
    left = newPrediction;
    return result;
}

PredictedType predictionFromClassInfo(const ClassInfo*);
PredictedType predictionFromStructure(Structure*);
PredictedType predictionFromCell(JSCell*);
PredictedType predictionFromValue(JSValue);

} // namespace JSC

#endif // PredictedType_h
