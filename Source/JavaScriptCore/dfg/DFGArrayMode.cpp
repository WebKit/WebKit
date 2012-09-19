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
#include "DFGArrayMode.h"

#if ENABLE(DFG_JIT)

#include "DFGAbstractValue.h"

namespace JSC { namespace DFG {

Array::Mode fromObserved(ArrayProfile* profile, Array::Action action, bool makeSafe)
{
    switch (profile->observedArrayModes()) {
    case 0:
        return Array::Unprofiled;
    case asArrayModes(NonArray):
        if (action == Array::Write && !profile->mayInterceptIndexedAccesses())
            return Array::BlankToArrayStorage; // FIXME: we don't know whether to go to slow put mode, or not. This is a decent guess.
        return Array::Undecided;
    case asArrayModes(NonArrayWithArrayStorage):
        return makeSafe ? Array::ArrayStorageOutOfBounds : (profile->mayStoreToHole() ? Array::ArrayStorageToHole : Array::ArrayStorage);
    case asArrayModes(NonArrayWithSlowPutArrayStorage):
    case asArrayModes(NonArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage):
        return Array::SlowPutArrayStorage;
    case asArrayModes(ArrayWithArrayStorage):
        return makeSafe ? Array::ArrayWithArrayStorageOutOfBounds : (profile->mayStoreToHole() ? Array::ArrayWithArrayStorageToHole : Array::ArrayWithArrayStorage);
    case asArrayModes(ArrayWithSlowPutArrayStorage):
    case asArrayModes(ArrayWithArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage):
        return Array::ArrayWithSlowPutArrayStorage;
    case asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithArrayStorage):
        return makeSafe ? Array::PossiblyArrayWithArrayStorageOutOfBounds : (profile->mayStoreToHole() ? Array::PossiblyArrayWithArrayStorageToHole : Array::PossiblyArrayWithArrayStorage);
    case asArrayModes(NonArrayWithSlowPutArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage):
    case asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage):
        return Array::PossiblyArrayWithSlowPutArrayStorage;
    case asArrayModes(NonArray) | asArrayModes(NonArrayWithArrayStorage):
        if (action == Array::Write && !profile->mayInterceptIndexedAccesses())
            return Array::BlankToArrayStorage;
        return Array::Undecided;
    case asArrayModes(NonArray) | asArrayModes(NonArrayWithSlowPutArrayStorage):
    case asArrayModes(NonArray) | asArrayModes(NonArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage):
        if (action == Array::Write && !profile->mayInterceptIndexedAccesses())
            return Array::BlankToSlowPutArrayStorage;
        return Array::Undecided;
    default:
        // We know that this is possibly a kind of array for which, though there is no
        // useful data in the array profile, we may be able to extract useful data from
        // the value profiles of the inputs. Hence, we leave it as undecided, and let
        // the predictions propagator decide later.
        return Array::Undecided;
    }
}

Array::Mode refineArrayMode(Array::Mode arrayMode, SpeculatedType base, SpeculatedType index)
{
    if (!base || !index) {
        // It can be that we had a legitimate arrayMode but no incoming predictions. That'll
        // happen if we inlined code based on, say, a global variable watchpoint, but later
        // realized that the callsite could not have possibly executed. It may be worthwhile
        // to fix that, but for now I'm leaving it as-is.
        return Array::ForceExit;
    }
    
    if (!isInt32Speculation(index) || !isCellSpeculation(base))
        return Array::Generic;
    
    if (arrayMode == Array::Unprofiled) {
        // If the indexing type wasn't recorded in the array profile but the values are
        // base=cell property=int, then we know that this access didn't execute.
        return Array::ForceExit;
    }
    
    if (arrayMode != Array::Undecided)
        return arrayMode;
    
    if (isStringSpeculation(base))
        return Array::String;
    
    if (isArgumentsSpeculation(base))
        return Array::Arguments;
    
    if (isInt8ArraySpeculation(base))
        return Array::Int8Array;
    
    if (isInt16ArraySpeculation(base))
        return Array::Int16Array;
    
    if (isInt32ArraySpeculation(base))
        return Array::Int32Array;
    
    if (isUint8ArraySpeculation(base))
        return Array::Uint8Array;
    
    if (isUint8ClampedArraySpeculation(base))
        return Array::Uint8ClampedArray;
    
    if (isUint16ArraySpeculation(base))
        return Array::Uint16Array;
    
    if (isUint32ArraySpeculation(base))
        return Array::Uint32Array;
    
    if (isFloat32ArraySpeculation(base))
        return Array::Float32Array;
    
    if (isFloat64ArraySpeculation(base))
        return Array::Float64Array;
    
    return Array::Generic;
}

bool modeAlreadyChecked(AbstractValue& value, Array::Mode arrayMode)
{
    switch (arrayMode) {
    case Array::Generic:
        return true;
        
    case Array::ForceExit:
        return false;
        
    case Array::String:
        return isStringSpeculation(value.m_type);
        
    case Array::ArrayStorage:
    case Array::ArrayStorageToHole:
    case Array::ArrayStorageOutOfBounds:
    case Array::PossiblyArrayWithArrayStorage:
    case Array::PossiblyArrayWithArrayStorageToHole:
    case Array::PossiblyArrayWithArrayStorageOutOfBounds:
        return value.m_currentKnownStructure.hasSingleton()
            && (value.m_currentKnownStructure.singleton()->indexingType() & HasArrayStorage);
        
    case Array::SlowPutArrayStorage:
    case Array::PossiblyArrayWithSlowPutArrayStorage:
        return value.m_currentKnownStructure.hasSingleton()
            && (value.m_currentKnownStructure.singleton()->indexingType() & (HasArrayStorage | HasSlowPutArrayStorage));
        
    case Array::ArrayWithArrayStorage:
    case Array::ArrayWithArrayStorageToHole:
    case Array::ArrayWithArrayStorageOutOfBounds:
        return value.m_currentKnownStructure.hasSingleton()
            && (value.m_currentKnownStructure.singleton()->indexingType() & HasArrayStorage)
            && (value.m_currentKnownStructure.singleton()->indexingType() & IsArray);
        
    case Array::ArrayWithSlowPutArrayStorage:
        return value.m_currentKnownStructure.hasSingleton()
            && (value.m_currentKnownStructure.singleton()->indexingType() & (HasArrayStorage | HasSlowPutArrayStorage))
            && (value.m_currentKnownStructure.singleton()->indexingType() & IsArray);
        
    case ALL_EFFECTFUL_ARRAY_STORAGE_MODES:
        return false;
        
    case Array::Arguments:
        return isArgumentsSpeculation(value.m_type);
        
    case Array::Int8Array:
        return isInt8ArraySpeculation(value.m_type);
        
    case Array::Int16Array:
        return isInt16ArraySpeculation(value.m_type);
        
    case Array::Int32Array:
        return isInt32ArraySpeculation(value.m_type);
        
    case Array::Uint8Array:
        return isUint8ArraySpeculation(value.m_type);
        
    case Array::Uint8ClampedArray:
        return isUint8ClampedArraySpeculation(value.m_type);
        
    case Array::Uint16Array:
        return isUint16ArraySpeculation(value.m_type);
        
    case Array::Uint32Array:
        return isUint32ArraySpeculation(value.m_type);

    case Array::Float32Array:
        return isFloat32ArraySpeculation(value.m_type);

    case Array::Float64Array:
        return isFloat64ArraySpeculation(value.m_type);
        
    case Array::Undecided:
    case Array::Unprofiled:
        break;
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

const char* modeToString(Array::Mode mode)
{
    switch (mode) {
    case Array::Undecided:
        return "Undecided";
    case Array::Unprofiled:
        return "Unprofiled";
    case Array::Generic:
        return "Generic";
    case Array::ForceExit:
        return "ForceExit";
    case Array::String:
        return "String";
    case Array::ArrayStorage:
        return "ArrayStorage";
    case Array::ArrayStorageToHole:
        return "ArrayStorageToHole";
    case Array::SlowPutArrayStorage:
        return "SlowPutArrayStorage";
    case Array::ArrayStorageOutOfBounds:
        return "ArrayStorageOutOfBounds";
    case Array::ArrayWithArrayStorage:
        return "ArrayWithArrayStorage";
    case Array::ArrayWithArrayStorageToHole:
        return "ArrayWithArrayStorageToHole";
    case Array::ArrayWithSlowPutArrayStorage:
        return "ArrayWithSlowPutArrayStorage";
    case Array::ArrayWithArrayStorageOutOfBounds:
        return "ArrayWithArrayStorageOutOfBounds";
    case Array::PossiblyArrayWithArrayStorage:
        return "PossiblyArrayWithArrayStorage";
    case Array::PossiblyArrayWithArrayStorageToHole:
        return "PossiblyArrayWithArrayStorageToHole";
    case Array::PossiblyArrayWithSlowPutArrayStorage:
        return "PossiblyArrayWithSlowPutArrayStorage";
    case Array::PossiblyArrayWithArrayStorageOutOfBounds:
        return "PossiblyArrayWithArrayStorageOutOfBounds";
    case Array::BlankToArrayStorage:
        return "BlankToArrayStorage";
    case Array::BlankToSlowPutArrayStorage:
        return "BlankToSlowPutArrayStorage";
    case Array::Arguments:
        return "Arguments";
    case Array::Int8Array:
        return "Int8Array";
    case Array::Int16Array:
        return "Int16Array";
    case Array::Int32Array:
        return "Int32Array";
    case Array::Uint8Array:
        return "Uint8Array";
    case Array::Uint8ClampedArray:
        return "Uint8ClampedArray";
    case Array::Uint16Array:
        return "Uint16Array";
    case Array::Uint32Array:
        return "Uint32Array";
    case Array::Float32Array:
        return "Float32Array";
    case Array::Float64Array:
        return "Float64Array";
    default:
        // Better to return something then it is to crash. Remember, this method
        // is being called from our main diagnostic tool, the IR dumper. It's like
        // a stack trace. So if we get here then probably something has already
        // gone wrong.
        return "Unknown!";
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

