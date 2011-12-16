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

#include "config.h"
#include "PredictedType.h"

#include "JSByteArray.h"
#include "JSFunction.h"
#include "ValueProfile.h"
#include <wtf/BoundsCheckedPointer.h>

namespace JSC {

#ifndef NDEBUG
const char* predictionToString(PredictedType value)
{
    if (value == PredictNone)
        return "None";
    
    static const int size = 256;
    static char description[size];
    BoundsCheckedPointer<char> ptr(description, size);
    
    bool isTop = true;
    
    if (value & PredictCellOther)
        ptr.strcat("Othercell");
    else
        isTop = false;
    
    if (value & PredictObjectOther)
        ptr.strcat("Otherobj");
    else
        isTop = false;
    
    if (value & PredictFinalObject)
        ptr.strcat("Final");
    else
        isTop = false;

    if (value & PredictArray)
        ptr.strcat("Array");
    else
        isTop = false;
    
    if (value & PredictByteArray)
        ptr.strcat("Bytearray");
    else
        isTop = false;
    
    if (value & PredictInt8Array)
        ptr.strcat("Int8array");
    else
        isTop = false;
    
    if (value & PredictInt16Array)
        ptr.strcat("Int16array");
    else
        isTop = false;
    
    if (value & PredictInt32Array)
        ptr.strcat("Int32array");
    else
        isTop = false;
    
    if (value & PredictUint8Array)
        ptr.strcat("Uint8array");
    else
        isTop = false;
    
    if (value & PredictUint16Array)
        ptr.strcat("Uint16array");
    else
        isTop = false;
    
    if (value & PredictUint32Array)
        ptr.strcat("Uint32array");
    else
        isTop = false;
    
    if (value & PredictFloat32Array)
        ptr.strcat("Float32array");
    else
        isTop = false;
    
    if (value & PredictFloat64Array)
        ptr.strcat("Float64array");
    else
        isTop = false;
    
    if (value & PredictFunction)
        ptr.strcat("Function");
    else
        isTop = false;
    
    if (value & PredictString)
        ptr.strcat("String");
    else
        isTop = false;
    
    if (value & PredictInt32)
        ptr.strcat("Int");
    else
        isTop = false;
    
    if (value & PredictDoubleReal)
        ptr.strcat("Doublereal");
    else
        isTop = false;
    
    if (value & PredictDoubleNaN)
        ptr.strcat("Doublenan");
    else
        isTop = false;
    
    if (value & PredictBoolean)
        ptr.strcat("Bool");
    else
        isTop = false;
    
    if (value & PredictOther)
        ptr.strcat("Other");
    else
        isTop = false;
    
    if (isTop)
        return "Top";
    
    *ptr++ = 0;
    
    return description;
}
#endif

PredictedType predictionFromClassInfo(const ClassInfo* classInfo)
{
    if (classInfo == &JSFinalObject::s_info)
        return PredictFinalObject;
    
    if (classInfo == &JSArray::s_info)
        return PredictArray;
    
    if (classInfo == &JSString::s_info)
        return PredictString;
    
    if (classInfo->isSubClassOf(&JSFunction::s_info))
        return PredictFunction;

    if (classInfo->isSubClassOf(&JSByteArray::s_info))
        return PredictByteArray;
    
    if (classInfo->typedArrayStorageType != TypedArrayNone) {
        switch (classInfo->typedArrayStorageType) {
        case TypedArrayInt8:
            return PredictInt8Array;
        case TypedArrayInt16:
            return PredictInt16Array;
        case TypedArrayInt32:
            return PredictInt32Array;
        case TypedArrayUint8:
            return PredictUint8Array;
        case TypedArrayUint16:
            return PredictUint16Array;
        case TypedArrayUint32:
            return PredictUint32Array;
        case TypedArrayFloat32:
            return PredictFloat32Array;
        case TypedArrayFloat64:
            return PredictFloat64Array;
        default:
            break;
        }
    }
    
    if (classInfo->isSubClassOf(&JSObject::s_info))
        return PredictObjectOther;
    
    return PredictCellOther;
}

PredictedType predictionFromStructure(Structure* structure)
{
    return predictionFromClassInfo(structure->classInfo());
}

PredictedType predictionFromCell(JSCell* cell)
{
    return predictionFromStructure(cell->structure());
}

PredictedType predictionFromValue(JSValue value)
{
    if (value.isInt32())
        return PredictInt32;
    if (value.isDouble()) {
        double number = value.asNumber();
        if (number == number)
            return PredictDoubleReal;
        return PredictDoubleNaN;
    }
    if (value.isCell())
        return predictionFromCell(value.asCell());
    if (value.isBoolean())
        return PredictBoolean;
    ASSERT(value.isUndefinedOrNull());
    return PredictOther;
}

} // namespace JSC

