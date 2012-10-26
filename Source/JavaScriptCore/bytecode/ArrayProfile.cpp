/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <wtf/StringExtras.h>

namespace JSC {

const char* arrayModesToString(ArrayModes arrayModes)
{
    if (!arrayModes)
        return "0:<empty>";
    
    if (arrayModes == ALL_ARRAY_MODES)
        return "TOP";

    bool isNonArray = !!(arrayModes & asArrayModes(NonArray));
    bool isNonArrayWithContiguous = !!(arrayModes & asArrayModes(NonArrayWithContiguous));
    bool isNonArrayWithArrayStorage = !!(arrayModes & asArrayModes(NonArrayWithArrayStorage));
    bool isNonArrayWithSlowPutArrayStorage = !!(arrayModes & asArrayModes(NonArrayWithSlowPutArrayStorage));
    bool isArray = !!(arrayModes & asArrayModes(ArrayClass));
    bool isArrayWithContiguous = !!(arrayModes & asArrayModes(ArrayWithContiguous));
    bool isArrayWithArrayStorage = !!(arrayModes & asArrayModes(ArrayWithArrayStorage));
    bool isArrayWithSlowPutArrayStorage = !!(arrayModes & asArrayModes(ArrayWithSlowPutArrayStorage));
    
    static char result[256];
    snprintf(
        result, sizeof(result),
        "%u:%s%s%s%s%s%s%s%s",
        arrayModes,
        isNonArray ? "NonArray" : "",
        isNonArrayWithContiguous ? "NonArrayWithContiguous" : "",
        isNonArrayWithArrayStorage ? " NonArrayWithArrayStorage" : "",
        isNonArrayWithSlowPutArrayStorage ? "NonArrayWithSlowPutArrayStorage" : "",
        isArray ? "ArrayClass" : "",
        isArrayWithContiguous ? "ArrayWithContiguous" : "",
        isArrayWithArrayStorage ? " ArrayWithArrayStorage" : "",
        isArrayWithSlowPutArrayStorage ? "ArrayWithSlowPutArrayStorage" : "");
    
    return result;
}

void ArrayProfile::computeUpdatedPrediction(OperationInProgress operation)
{
    if (m_lastSeenStructure) {
        m_observedArrayModes |= arrayModeFromStructure(m_lastSeenStructure);
        m_mayInterceptIndexedAccesses |=
            m_lastSeenStructure->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero();
        if (!m_structureIsPolymorphic) {
            if (!m_expectedStructure)
                m_expectedStructure = m_lastSeenStructure;
            else if (m_expectedStructure != m_lastSeenStructure) {
                m_expectedStructure = 0;
                m_structureIsPolymorphic = true;
            }
        }
        m_lastSeenStructure = 0;
    }
    
    if (hasTwoOrMoreBitsSet(m_observedArrayModes)) {
        m_structureIsPolymorphic = true;
        m_expectedStructure = 0;
    }
    
    if (operation == Collection
        && m_expectedStructure
        && !Heap::isMarked(m_expectedStructure)) {
        m_expectedStructure = 0;
        m_structureIsPolymorphic = true;
    }
}

} // namespace JSC

