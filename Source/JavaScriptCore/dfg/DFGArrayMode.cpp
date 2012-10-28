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

ArrayMode ArrayMode::fromObserved(ArrayProfile* profile, Array::Action action, bool makeSafe)
{
    switch (profile->observedArrayModes()) {
    case 0:
        return ArrayMode(Array::Unprofiled);
    case asArrayModes(NonArray):
        if (action == Array::Write && !profile->mayInterceptIndexedAccesses())
            return ArrayMode(Array::Contiguous, Array::NonArray, Array::OutOfBounds, Array::Convert); // FIXME: we don't know whether to go to contiguous or array storage. We're making a static guess here. In future we should use exit profiling for this.
        return ArrayMode(Array::SelectUsingPredictions);
    case asArrayModes(NonArrayWithContiguous):
        return ArrayMode(Array::Contiguous, Array::NonArray, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(ArrayWithContiguous):
        return ArrayMode(Array::Contiguous, Array::Array, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(NonArrayWithContiguous) | asArrayModes(ArrayWithContiguous):
        return ArrayMode(Array::Contiguous, Array::PossiblyArray, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(NonArrayWithArrayStorage):
        return ArrayMode(Array::ArrayStorage, Array::NonArray, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(NonArrayWithSlowPutArrayStorage):
    case asArrayModes(NonArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage):
        return ArrayMode(Array::SlowPutArrayStorage, Array::NonArray, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(ArrayWithArrayStorage):
        return ArrayMode(Array::ArrayStorage, Array::Array, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(ArrayWithSlowPutArrayStorage):
    case asArrayModes(ArrayWithArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage):
        return ArrayMode(Array::SlowPutArrayStorage, Array::Array, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithArrayStorage):
        return ArrayMode(Array::ArrayStorage, Array::PossiblyArray, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(NonArrayWithSlowPutArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage):
    case asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage):
        return ArrayMode(Array::SlowPutArrayStorage, Array::PossiblyArray, Array::AsIs).withSpeculation(profile, makeSafe);
    case asArrayModes(NonArrayWithContiguous) | asArrayModes(NonArrayWithArrayStorage):
        return ArrayMode(Array::ArrayStorage, Array::NonArray, Array::Convert).withSpeculation(profile, makeSafe);
    case asArrayModes(ArrayWithContiguous) | asArrayModes(ArrayWithArrayStorage):
        return ArrayMode(Array::ArrayStorage, Array::Array, Array::Convert).withSpeculation(profile, makeSafe);
    case asArrayModes(NonArrayWithContiguous) | asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithContiguous) | asArrayModes(ArrayWithArrayStorage):
        return ArrayMode(Array::ArrayStorage, Array::PossiblyArray, Array::Convert).withSpeculation(profile, makeSafe);
    case asArrayModes(NonArray) | asArrayModes(NonArrayWithContiguous):
        if (action == Array::Write && !profile->mayInterceptIndexedAccesses())
            return ArrayMode(Array::Contiguous, Array::NonArray, Array::OutOfBounds, Array::Convert);
        return ArrayMode(Array::SelectUsingPredictions);
    case asArrayModes(NonArray) | asArrayModes(NonArrayWithContiguous) | asArrayModes(NonArrayWithArrayStorage):
    case asArrayModes(NonArray) | asArrayModes(NonArrayWithArrayStorage):
        if (action == Array::Write && !profile->mayInterceptIndexedAccesses())
            return ArrayMode(Array::ArrayStorage, Array::NonArray, Array::OutOfBounds, Array::Convert);
        return ArrayMode(Array::SelectUsingPredictions);
    case asArrayModes(NonArray) | asArrayModes(NonArrayWithSlowPutArrayStorage):
    case asArrayModes(NonArray) | asArrayModes(NonArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage):
        if (action == Array::Write && !profile->mayInterceptIndexedAccesses())
            return ArrayMode(Array::SlowPutArrayStorage, Array::NonArray, Array::OutOfBounds, Array::Convert);
        return ArrayMode(Array::SelectUsingPredictions);
    default:
        // We know that this is possibly a kind of array for which, though there is no
        // useful data in the array profile, we may be able to extract useful data from
        // the value profiles of the inputs. Hence, we leave it as undecided, and let
        // the predictions propagator decide later.
        return ArrayMode(Array::SelectUsingPredictions);
    }
}

ArrayMode ArrayMode::refine(SpeculatedType base, SpeculatedType index) const
{
    if (!base || !index) {
        // It can be that we had a legitimate arrayMode but no incoming predictions. That'll
        // happen if we inlined code based on, say, a global variable watchpoint, but later
        // realized that the callsite could not have possibly executed. It may be worthwhile
        // to fix that, but for now I'm leaving it as-is.
        return ArrayMode(Array::ForceExit);
    }
    
    if (!isInt32Speculation(index) || !isCellSpeculation(base))
        return ArrayMode(Array::Generic);
    
    if (type() == Array::Unprofiled) {
        // If the indexing type wasn't recorded in the array profile but the values are
        // base=cell property=int, then we know that this access didn't execute.
        return ArrayMode(Array::ForceExit);
    }
    
    if (type() != Array::SelectUsingPredictions)
        return *this;
    
    if (isStringSpeculation(base))
        return ArrayMode(Array::String);
    
    if (isArgumentsSpeculation(base))
        return ArrayMode(Array::Arguments);
    
    if (isInt8ArraySpeculation(base))
        return ArrayMode(Array::Int8Array);
    
    if (isInt16ArraySpeculation(base))
        return ArrayMode(Array::Int16Array);
    
    if (isInt32ArraySpeculation(base))
        return ArrayMode(Array::Int32Array);
    
    if (isUint8ArraySpeculation(base))
        return ArrayMode(Array::Uint8Array);
    
    if (isUint8ClampedArraySpeculation(base))
        return ArrayMode(Array::Uint8ClampedArray);
    
    if (isUint16ArraySpeculation(base))
        return ArrayMode(Array::Uint16Array);
    
    if (isUint32ArraySpeculation(base))
        return ArrayMode(Array::Uint32Array);
    
    if (isFloat32ArraySpeculation(base))
        return ArrayMode(Array::Float32Array);
    
    if (isFloat64ArraySpeculation(base))
        return ArrayMode(Array::Float64Array);
    
    return ArrayMode(Array::Generic);
}

bool ArrayMode::alreadyChecked(AbstractValue& value) const
{
    switch (type()) {
    case Array::Generic:
        return true;
        
    case Array::ForceExit:
        return false;
        
    case Array::String:
        return isStringSpeculation(value.m_type);
        
    case Array::Contiguous:
        if (arrayClass() == Array::Array) {
            if (arrayModesAlreadyChecked(value.m_arrayModes, asArrayModes(ArrayWithContiguous)))
                return true;
            return value.m_currentKnownStructure.hasSingleton()
                && hasContiguous(value.m_currentKnownStructure.singleton()->indexingType())
                && (value.m_currentKnownStructure.singleton()->indexingType() & IsArray);
        }
        if (arrayModesAlreadyChecked(value.m_arrayModes, asArrayModes(NonArrayWithContiguous) | asArrayModes(ArrayWithContiguous)))
            return true;
        return value.m_currentKnownStructure.hasSingleton()
            && hasContiguous(value.m_currentKnownStructure.singleton()->indexingType());
        
    case Array::ArrayStorage:
        if (arrayClass() == Array::Array) {
            if (arrayModesAlreadyChecked(value.m_arrayModes, asArrayModes(ArrayWithArrayStorage)))
                return true;
            return value.m_currentKnownStructure.hasSingleton()
                && hasFastArrayStorage(value.m_currentKnownStructure.singleton()->indexingType())
                && (value.m_currentKnownStructure.singleton()->indexingType() & IsArray);
        }
        if (arrayModesAlreadyChecked(value.m_arrayModes, asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithArrayStorage)))
            return true;
        return value.m_currentKnownStructure.hasSingleton()
            && hasFastArrayStorage(value.m_currentKnownStructure.singleton()->indexingType());
        
    case Array::SlowPutArrayStorage:
        if (arrayClass() == Array::Array) {
            if (arrayModesAlreadyChecked(value.m_arrayModes, asArrayModes(ArrayWithArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage)))
                return true;
            return value.m_currentKnownStructure.hasSingleton()
                && hasArrayStorage(value.m_currentKnownStructure.singleton()->indexingType())
                && (value.m_currentKnownStructure.singleton()->indexingType() & IsArray);
        }
        if (arrayModesAlreadyChecked(value.m_arrayModes, asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage)))
            return true;
        return value.m_currentKnownStructure.hasSingleton()
            && hasArrayStorage(value.m_currentKnownStructure.singleton()->indexingType());
        
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
        
    case Array::SelectUsingPredictions:
    case Array::Unprofiled:
        break;
    }
    
    CRASH();
    return false;
}

const char* arrayTypeToString(Array::Type type)
{
    switch (type) {
    case Array::SelectUsingPredictions:
        return "SelectUsingPredictions";
    case Array::Unprofiled:
        return "Unprofiled";
    case Array::Generic:
        return "Generic";
    case Array::ForceExit:
        return "ForceExit";
    case Array::String:
        return "String";
    case Array::Contiguous:
        return "Contiguous";
    case Array::ArrayStorage:
        return "ArrayStorage";
    case Array::SlowPutArrayStorage:
        return "SlowPutArrayStorage";
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

const char* arrayClassToString(Array::Class arrayClass)
{
    switch (arrayClass) {
    case Array::Array:
        return "Array";
    case Array::NonArray:
        return "NonArray";
    case Array::PossiblyArray:
        return "PossiblyArray";
    default:
        return "Unknown!";
    }
}

const char* arraySpeculationToString(Array::Speculation speculation)
{
    switch (speculation) {
    case Array::InBounds:
        return "InBounds";
    case Array::ToHole:
        return "ToHole";
    case Array::OutOfBounds:
        return "OutOfBounds";
    default:
        return "Unknown!";
    }
}

const char* arrayConversionToString(Array::Conversion conversion)
{
    switch (conversion) {
    case Array::AsIs:
        return "AsIs";
    case Array::Convert:
        return "Convert";
    default:
        return "Unknown!";
    }
}

const char* ArrayMode::toString() const
{
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s%s%s%s", arrayTypeToString(type()), arrayClassToString(arrayClass()), arraySpeculationToString(speculation()), arrayConversionToString(conversion()));
    return buffer;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

