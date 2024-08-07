/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "ArrayProfile.h"

#include "CodeBlock.h"
#include "JSCellInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSTypedArrays.h"
#include <wtf/CommaPrinter.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

// Keep in sync with the order of TypedArrayType.
const ArrayModes typedArrayModes[NumberOfTypedArrayTypesExcludingDataView] = {
    Int8ArrayMode,
    Uint8ArrayMode,
    Uint8ClampedArrayMode,
    Int16ArrayMode,
    Uint16ArrayMode,
    Int32ArrayMode,
    Uint32ArrayMode,
    Float16ArrayMode,
    Float32ArrayMode,
    Float64ArrayMode,
    BigInt64ArrayMode,
    BigUint64ArrayMode,
};

void dumpArrayModes(PrintStream& out, ArrayModes arrayModes)
{
    if (!arrayModes) {
        out.print("<empty>"_s);
        return;
    }
    
    if (arrayModes == ALL_ARRAY_MODES) {
        out.print("TOP"_s);
        return;
    }
    
    CommaPrinter comma("|"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArray))
        out.print(comma, "NonArray"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithInt32))
        out.print(comma, "NonArrayWithInt32"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithDouble))
        out.print(comma, "NonArrayWithDouble"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithContiguous))
        out.print(comma, "NonArrayWithContiguous"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithArrayStorage))
        out.print(comma, "NonArrayWithArrayStorage"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithSlowPutArrayStorage))
        out.print(comma, "NonArrayWithSlowPutArrayStorage"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayClass))
        out.print(comma, "ArrayClass"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithUndecided))
        out.print(comma, "ArrayWithUndecided"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithInt32))
        out.print(comma, "ArrayWithInt32"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithDouble))
        out.print(comma, "ArrayWithDouble"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithContiguous))
        out.print(comma, "ArrayWithContiguous"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithArrayStorage))
        out.print(comma, "ArrayWithArrayStorage"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithSlowPutArrayStorage))
        out.print(comma, "ArrayWithSlowPutArrayStorage"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(CopyOnWriteArrayWithInt32))
        out.print(comma, "CopyOnWriteArrayWithInt32"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(CopyOnWriteArrayWithDouble))
        out.print(comma, "CopyOnWriteArrayWithDouble"_s);
    if (arrayModes & asArrayModesIgnoringTypedArrays(CopyOnWriteArrayWithContiguous))
        out.print(comma, "CopyOnWriteArrayWithContiguous"_s);

    if (arrayModes & Int8ArrayMode)
        out.print(comma, "Int8ArrayMode"_s);
    if (arrayModes & Int16ArrayMode)
        out.print(comma, "Int16ArrayMode"_s);
    if (arrayModes & Int32ArrayMode)
        out.print(comma, "Int32ArrayMode"_s);
    if (arrayModes & Uint8ArrayMode)
        out.print(comma, "Uint8ArrayMode"_s);
    if (arrayModes & Uint8ClampedArrayMode)
        out.print(comma, "Uint8ClampedArrayMode"_s);
    if (arrayModes & Uint16ArrayMode)
        out.print(comma, "Uint16ArrayMode"_s);
    if (arrayModes & Uint32ArrayMode)
        out.print(comma, "Uint32ArrayMode"_s);
    if (arrayModes & Float16ArrayMode)
        out.print(comma, "Float16ArrayMode"_s);
    if (arrayModes & Float32ArrayMode)
        out.print(comma, "Float32ArrayMode"_s);
    if (arrayModes & Float64ArrayMode)
        out.print(comma, "Float64ArrayMode"_s);
    if (arrayModes & BigInt64ArrayMode)
        out.print(comma, "BigInt64ArrayMode"_s);
    if (arrayModes & BigUint64ArrayMode)
        out.print(comma, "BigUint64ArrayMode"_s);
}

void ArrayProfile::computeUpdatedPrediction(CodeBlock* codeBlock)
{
    auto lastSeenStructureID = std::exchange(m_lastSeenStructureID, StructureID());
    if (!lastSeenStructureID)
        return;

    computeUpdatedPrediction(codeBlock, lastSeenStructureID.decode());
}

void ArrayProfile::computeUpdatedPrediction(CodeBlock* codeBlock, Structure* lastSeenStructure)
{
    m_observedArrayModes |= arrayModesFromStructure(lastSeenStructure);

    if (!m_arrayProfileFlags.contains(ArrayProfileFlag::DidPerformFirstRunPruning) && hasTwoOrMoreBitsSet(m_observedArrayModes)) {
        m_observedArrayModes = arrayModesFromStructure(lastSeenStructure);
        m_arrayProfileFlags.add(ArrayProfileFlag::DidPerformFirstRunPruning);
    }

    if (lastSeenStructure->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
        m_arrayProfileFlags.add(ArrayProfileFlag::MayInterceptIndexedAccesses);

    JSGlobalObject* globalObject = codeBlock->globalObject();
    bool isResizableOrGrowableShared = false;
    if (!globalObject->isOriginalArrayStructure(lastSeenStructure) && !globalObject->isOriginalTypedArrayStructure(lastSeenStructure, isResizableOrGrowableShared))
        m_arrayProfileFlags.add(ArrayProfileFlag::UsesNonOriginalArrayStructures);

    if (isTypedArrayTypeIncludingDataView(lastSeenStructure->typeInfo().type())) {
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(lastSeenStructure->classInfoForCells()))
            m_arrayProfileFlags.add(ArrayProfileFlag::MayBeResizableOrGrowableSharedTypedArray);
    }
}

void ArrayProfile::observeIndexedRead(JSCell* cell, unsigned index)
{
    m_lastSeenStructureID = cell->structureID();

    if (JSObject* object = jsDynamicCast<JSObject*>(cell)) {
        if (hasAnyArrayStorage(object->indexingType()) && index >= object->getVectorLength())
            setOutOfBounds();
        else if (index >= object->getArrayLength())
            setOutOfBounds();
    }

    if (JSString* string = jsDynamicCast<JSString*>(cell)) {
        if (index >= string->length())
            setOutOfBounds();
    }
}

CString ArrayProfile::briefDescription(CodeBlock* codeBlock)
{
    computeUpdatedPrediction(codeBlock);
    return briefDescriptionWithoutUpdating();
}

CString ArrayProfile::briefDescriptionWithoutUpdating()
{
    StringPrintStream out;
    CommaPrinter comma;

    if (m_observedArrayModes)
        out.print(comma, ArrayModesDump(m_observedArrayModes));
    if (m_arrayProfileFlags.contains(ArrayProfileFlag::MayStoreHole))
        out.print(comma, "Hole"_s);
    if (m_arrayProfileFlags.contains(ArrayProfileFlag::OutOfBounds))
        out.print(comma, "OutOfBounds"_s);
    if (m_arrayProfileFlags.contains(ArrayProfileFlag::MayInterceptIndexedAccesses))
        out.print(comma, "Intercept"_s);
    if (!m_arrayProfileFlags.contains(ArrayProfileFlag::UsesNonOriginalArrayStructures))
        out.print(comma, "Original"_s);
    if (!m_arrayProfileFlags.contains(ArrayProfileFlag::MayBeResizableOrGrowableSharedTypedArray))
        out.print(comma, "Resizable"_s);

    return out.toCString();
}

} // namespace JSC

