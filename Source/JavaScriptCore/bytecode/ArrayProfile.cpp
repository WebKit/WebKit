/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include <wtf/StringExtras.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

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
    if (arrayModes & asArrayModes(NonArray))
        out.print(comma, "NonArray");
    if (arrayModes & asArrayModes(NonArrayWithInt32))
        out.print(comma, "NonArrayWithInt32");
    if (arrayModes & asArrayModes(NonArrayWithDouble))
        out.print(comma, "NonArrayWithDouble");
    if (arrayModes & asArrayModes(NonArrayWithContiguous))
        out.print(comma, "NonArrayWithContiguous");
    if (arrayModes & asArrayModes(NonArrayWithArrayStorage))
        out.print(comma, "NonArrayWithArrayStorage");
    if (arrayModes & asArrayModes(NonArrayWithSlowPutArrayStorage))
        out.print(comma, "NonArrayWithSlowPutArrayStorage");
    if (arrayModes & asArrayModes(ArrayClass))
        out.print(comma, "ArrayClass");
    if (arrayModes & asArrayModes(ArrayWithUndecided))
        out.print(comma, "ArrayWithUndecided");
    if (arrayModes & asArrayModes(ArrayWithInt32))
        out.print(comma, "ArrayWithInt32");
    if (arrayModes & asArrayModes(ArrayWithDouble))
        out.print(comma, "ArrayWithDouble");
    if (arrayModes & asArrayModes(ArrayWithContiguous))
        out.print(comma, "ArrayWithContiguous");
    if (arrayModes & asArrayModes(ArrayWithArrayStorage))
        out.print(comma, "ArrayWithArrayStorage");
    if (arrayModes & asArrayModes(ArrayWithSlowPutArrayStorage))
        out.print(comma, "ArrayWithSlowPutArrayStorage");
}

void ArrayProfile::computeUpdatedPrediction(const ConcurrentJITLocker&, CodeBlock* codeBlock)
{
    if (!m_lastSeenStructure)
        return;
    
    m_observedArrayModes |= arrayModeFromStructure(m_lastSeenStructure);
    
    if (!m_didPerformFirstRunPruning
        && hasTwoOrMoreBitsSet(m_observedArrayModes)) {
        m_observedArrayModes = arrayModeFromStructure(m_lastSeenStructure);
        m_didPerformFirstRunPruning = true;
    }
    
    m_mayInterceptIndexedAccesses |=
        m_lastSeenStructure->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero();
    JSGlobalObject* globalObject = codeBlock->globalObject();
    if (!globalObject->isOriginalArrayStructure(m_lastSeenStructure)
        && !globalObject->isOriginalTypedArrayStructure(m_lastSeenStructure))
        m_usesOriginalArrayStructures = false;
    m_lastSeenStructure = 0;
}

CString ArrayProfile::briefDescription(const ConcurrentJITLocker& locker, CodeBlock* codeBlock)
{
    computeUpdatedPrediction(locker, codeBlock);
    return briefDescriptionWithoutUpdating(locker);
}

CString ArrayProfile::briefDescriptionWithoutUpdating(const ConcurrentJITLocker&)
{
    StringPrintStream out;
    
    bool hasPrinted = false;
    
    if (m_observedArrayModes) {
        if (hasPrinted)
            out.print(", ");
        out.print(ArrayModesDump(m_observedArrayModes));
        hasPrinted = true;
    }
    
    if (m_mayStoreToHole) {
        if (hasPrinted)
            out.print(", ");
        out.print("Hole");
        hasPrinted = true;
    }
    
    if (m_outOfBounds) {
        if (hasPrinted)
            out.print(", ");
        out.print("OutOfBounds");
        hasPrinted = true;
    }
    
    if (m_mayInterceptIndexedAccesses) {
        if (hasPrinted)
            out.print(", ");
        out.print("Intercept");
        hasPrinted = true;
    }
    
    if (m_usesOriginalArrayStructures) {
        if (hasPrinted)
            out.print(", ");
        out.print("Original");
        hasPrinted = true;
    }
    
    UNUSED_PARAM(hasPrinted);
    
    return out.toCString();
}

} // namespace JSC

