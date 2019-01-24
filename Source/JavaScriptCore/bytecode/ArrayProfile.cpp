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
#include "JSCInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

#if !ASSERT_DISABLED
const char* const ArrayProfile::s_typeName = "ArrayProfile";
#endif

// Keep in sync with the order of TypedArrayType.
const ArrayModes typedArrayModes[NumberOfTypedArrayTypesExcludingDataView] = {
    Int8ArrayMode,
    Uint8ArrayMode,
    Uint8ClampedArrayMode,
    Int16ArrayMode,
    Uint16ArrayMode,
    Int32ArrayMode,
    Uint32ArrayMode,
    Float32ArrayMode,
    Float64ArrayMode,
};

void dumpArrayModes(PrintStream& out, ArrayModes arrayModes)
{
    if (!arrayModes) {
        out.print("<empty>");
        return;
    }
    
    if (arrayModes == ALL_ARRAY_MODES) {
        out.print("TOP");
        return;
    }
    
    CommaPrinter comma("|");
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArray))
        out.print(comma, "NonArray");
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithInt32))
        out.print(comma, "NonArrayWithInt32");
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithDouble))
        out.print(comma, "NonArrayWithDouble");
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithContiguous))
        out.print(comma, "NonArrayWithContiguous");
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithArrayStorage))
        out.print(comma, "NonArrayWithArrayStorage");
    if (arrayModes & asArrayModesIgnoringTypedArrays(NonArrayWithSlowPutArrayStorage))
        out.print(comma, "NonArrayWithSlowPutArrayStorage");
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayClass))
        out.print(comma, "ArrayClass");
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithUndecided))
        out.print(comma, "ArrayWithUndecided");
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithInt32))
        out.print(comma, "ArrayWithInt32");
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithDouble))
        out.print(comma, "ArrayWithDouble");
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithContiguous))
        out.print(comma, "ArrayWithContiguous");
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithArrayStorage))
        out.print(comma, "ArrayWithArrayStorage");
    if (arrayModes & asArrayModesIgnoringTypedArrays(ArrayWithSlowPutArrayStorage))
        out.print(comma, "ArrayWithSlowPutArrayStorage");
    if (arrayModes & asArrayModesIgnoringTypedArrays(CopyOnWriteArrayWithInt32))
        out.print(comma, "CopyOnWriteArrayWithInt32");
    if (arrayModes & asArrayModesIgnoringTypedArrays(CopyOnWriteArrayWithDouble))
        out.print(comma, "CopyOnWriteArrayWithDouble");
    if (arrayModes & asArrayModesIgnoringTypedArrays(CopyOnWriteArrayWithContiguous))
        out.print(comma, "CopyOnWriteArrayWithContiguous");

    if (arrayModes & Int8ArrayMode)
        out.print(comma, "Int8ArrayMode");
    if (arrayModes & Int16ArrayMode)
        out.print(comma, "Int16ArrayMode");
    if (arrayModes & Int32ArrayMode)
        out.print(comma, "Int32ArrayMode");
    if (arrayModes & Uint8ArrayMode)
        out.print(comma, "Uint8ArrayMode");
    if (arrayModes & Uint8ClampedArrayMode)
        out.print(comma, "Uint8ClampedArrayMode");
    if (arrayModes & Uint16ArrayMode)
        out.print(comma, "Uint16ArrayMode");
    if (arrayModes & Uint32ArrayMode)
        out.print(comma, "Uint32ArrayMode");
    if (arrayModes & Float32ArrayMode)
        out.print(comma, "Float32ArrayMode");
    if (arrayModes & Float64ArrayMode)
        out.print(comma, "Float64ArrayMode");
}

void ArrayProfile::computeUpdatedPrediction(const ConcurrentJSLocker& locker, CodeBlock* codeBlock)
{
    if (!m_lastSeenStructureID)
        return;
    
    Structure* lastSeenStructure = codeBlock->heap()->structureIDTable().get(m_lastSeenStructureID);
    computeUpdatedPrediction(locker, codeBlock, lastSeenStructure);
    m_lastSeenStructureID = 0;
}

void ArrayProfile::computeUpdatedPrediction(const ConcurrentJSLocker&, CodeBlock* codeBlock, Structure* lastSeenStructure)
{
    m_observedArrayModes |= arrayModesFromStructure(lastSeenStructure);
    
    if (!m_didPerformFirstRunPruning
        && hasTwoOrMoreBitsSet(m_observedArrayModes)) {
        m_observedArrayModes = arrayModesFromStructure(lastSeenStructure);
        m_didPerformFirstRunPruning = true;
    }
    
    m_mayInterceptIndexedAccesses |=
        lastSeenStructure->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero();
    JSGlobalObject* globalObject = codeBlock->globalObject();
    if (!globalObject->isOriginalArrayStructure(lastSeenStructure)
        && !globalObject->isOriginalTypedArrayStructure(lastSeenStructure))
        m_usesOriginalArrayStructures = false;
}

void ArrayProfile::observeIndexedRead(VM& vm, JSCell* cell, unsigned index)
{
    m_lastSeenStructureID = cell->structureID();

    if (JSObject* object = jsDynamicCast<JSObject*>(vm, cell)) {
        if (hasAnyArrayStorage(object->indexingType()) && index >= object->getVectorLength())
            setOutOfBounds();
        else if (index >= object->getArrayLength())
            setOutOfBounds();
    }

    if (JSString* string = jsDynamicCast<JSString*>(vm, cell)) {
        if (index >= string->length())
            setOutOfBounds();
    }
}

CString ArrayProfile::briefDescription(const ConcurrentJSLocker& locker, CodeBlock* codeBlock)
{
    computeUpdatedPrediction(locker, codeBlock);
    return briefDescriptionWithoutUpdating(locker);
}

CString ArrayProfile::briefDescriptionWithoutUpdating(const ConcurrentJSLocker&)
{
    StringPrintStream out;
    CommaPrinter comma;

    if (m_observedArrayModes)
        out.print(comma, ArrayModesDump(m_observedArrayModes));
    if (m_mayStoreToHole)
        out.print(comma, "Hole");
    if (m_outOfBounds)
        out.print(comma, "OutOfBounds");
    if (m_mayInterceptIndexedAccesses)
        out.print(comma, "Intercept");
    if (m_usesOriginalArrayStructures)
        out.print(comma, "Original");

    return out.toCString();
}

} // namespace JSC

