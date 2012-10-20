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

#ifndef DFGArrayMode_h
#define DFGArrayMode_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "ArrayProfile.h"
#include "SpeculatedType.h"

namespace JSC { namespace DFG {

struct AbstractValue;

// Use a namespace + enum instead of enum alone to avoid the namespace collision
// that would otherwise occur, since we say things like "Int8Array" and "JSArray"
// in lots of other places, to mean subtly different things.
namespace Array {
enum Action {
    Read,
    Write
};

enum Mode {
    Undecided, // Implies that we need predictions to decide. We will never get to the backend in this mode.
    Unprofiled, // Implies that array profiling didn't see anything. But that could be because the operands didn't comply with basic type assumptions (base is cell, property is int). This either becomes Generic or ForceExit depending on value profiling.
    ForceExit, // Implies that we have no idea how to execute this operation, so we should just give up.
    Generic,
    String,
    
    // Modes of conventional indexed storage where the check is non side-effecting.
    Contiguous,
    ContiguousToTail,
    ContiguousOutOfBounds,
    ArrayWithContiguous,
    ArrayWithContiguousToTail,
    ArrayWithContiguousOutOfBounds,
    PossiblyArrayWithContiguous,
    PossiblyArrayWithContiguousToTail,
    PossiblyArrayWithContiguousOutOfBounds,
    ArrayStorage,
    ArrayStorageToHole,
    SlowPutArrayStorage,
    ArrayStorageOutOfBounds,
    ArrayWithArrayStorage,
    ArrayWithArrayStorageToHole,
    ArrayWithSlowPutArrayStorage,
    ArrayWithArrayStorageOutOfBounds,
    PossiblyArrayWithArrayStorage,
    PossiblyArrayWithArrayStorageToHole,
    PossiblyArrayWithSlowPutArrayStorage,
    PossiblyArrayWithArrayStorageOutOfBounds,
    
    // Modes of conventional indexed storage where the check is side-effecting.
    ToContiguous,
    ToArrayStorage,
    ArrayToArrayStorage,
    PossiblyArrayToArrayStorage,
    ToSlowPutArrayStorage,
    
    Arguments,
    Int8Array,
    Int16Array,
    Int32Array,
    Uint8Array,
    Uint8ClampedArray,
    Uint16Array,
    Uint32Array,
    Float32Array,
    Float64Array
};
} // namespace Array

// Helpers for 'case' statements. For example, saying "case AllArrayStorageModes:"
// is the same as having multiple case statements listing off all of the modes that
// have the word "ArrayStorage" in them.

// First: helpers for non-side-effecting checks.
#define NON_ARRAY_CONTIGUOUS_MODES                         \
    Array::Contiguous:                                     \
    case Array::ContiguousToTail:                          \
    case Array::ContiguousOutOfBounds:                     \
    case Array::PossiblyArrayWithContiguous:               \
    case Array::PossiblyArrayWithContiguousToTail:         \
    case Array::PossiblyArrayWithContiguousOutOfBounds
#define ARRAY_WITH_CONTIGUOUS_MODES                        \
    Array::ArrayWithContiguous:                            \
    case Array::ArrayWithContiguousToTail:                 \
    case Array::ArrayWithContiguousOutOfBounds
#define ALL_CONTIGUOUS_MODES                               \
    NON_ARRAY_CONTIGUOUS_MODES:                            \
    case ARRAY_WITH_CONTIGUOUS_MODES
#define IN_BOUNDS_CONTIGUOUS_MODES                         \
    Array::Contiguous:                                     \
    case Array::ArrayWithContiguous:                       \
    case Array::PossiblyArrayWithContiguous
#define CONTIGUOUS_TO_TAIL_MODES                           \
    Array::ContiguousToTail:                               \
    case Array::ArrayWithContiguousToTail:                 \
    case Array::PossiblyArrayWithContiguousToTail
#define OUT_OF_BOUNDS_CONTIGUOUS_MODES                     \
    Array::ContiguousOutOfBounds:                          \
    case Array::ArrayWithContiguousOutOfBounds:            \
    case Array::PossiblyArrayWithContiguousOutOfBounds
#define NON_ARRAY_ARRAY_STORAGE_MODES                      \
    Array::ArrayStorage:                                   \
    case Array::ArrayStorageToHole:                        \
    case Array::SlowPutArrayStorage:                       \
    case Array::ArrayStorageOutOfBounds:                   \
    case Array::PossiblyArrayWithArrayStorage:             \
    case Array::PossiblyArrayWithArrayStorageToHole:       \
    case Array::PossiblyArrayWithSlowPutArrayStorage:      \
    case Array::PossiblyArrayWithArrayStorageOutOfBounds
#define ARRAY_WITH_ARRAY_STORAGE_MODES                     \
    Array::ArrayWithArrayStorage:                          \
    case Array::ArrayWithArrayStorageToHole:               \
    case Array::ArrayWithSlowPutArrayStorage:              \
    case Array::ArrayWithArrayStorageOutOfBounds
#define ALL_ARRAY_STORAGE_MODES                            \
    NON_ARRAY_ARRAY_STORAGE_MODES:                         \
    case ARRAY_WITH_ARRAY_STORAGE_MODES
#define IN_BOUNDS_ARRAY_STORAGE_MODES                      \
    Array::ArrayStorage:                                   \
    case Array::ArrayWithArrayStorage:                     \
    case Array::PossiblyArrayWithArrayStorage
#define ARRAY_STORAGE_TO_HOLE_MODES                        \
    Array::ArrayStorageToHole:                             \
    case Array::ArrayWithArrayStorageToHole:               \
    case Array::PossiblyArrayWithArrayStorageToHole
#define SLOW_PUT_ARRAY_STORAGE_MODES                       \
    Array::SlowPutArrayStorage:                            \
    case Array::ArrayWithSlowPutArrayStorage:              \
    case Array::PossiblyArrayWithSlowPutArrayStorage
#define OUT_OF_BOUNDS_ARRAY_STORAGE_MODES                  \
    Array::ArrayStorageOutOfBounds:                        \
    case Array::ArrayWithArrayStorageOutOfBounds:          \
    case Array::PossiblyArrayWithArrayStorageOutOfBounds

// Next: helpers for side-effecting checks.
#define NON_ARRAY_EFFECTFUL_MODES                          \
    Array::ToContiguous:                                   \
    case Array::ToArrayStorage:                            \
    case Array::ToSlowPutArrayStorage:                     \
    case Array::PossiblyArrayToArrayStorage
#define ARRAY_EFFECTFUL_MODES                              \
    Array::ArrayToArrayStorage
#define ALL_EFFECTFUL_CONTIGUOUS_MODES                     \
    Array::ToContiguous
#define ALL_EFFECTFUL_ARRAY_STORAGE_MODES                  \
    Array::ToArrayStorage:                                 \
    case Array::ToSlowPutArrayStorage:                     \
    case Array::ArrayToArrayStorage:                       \
    case Array::PossiblyArrayToArrayStorage
#define SLOW_PUT_EFFECTFUL_ARRAY_STORAGE_MODES             \
    Array::ToSlowPutArrayStorage
#define ALL_EFFECTFUL_MODES                                \
    ALL_EFFECTFUL_CONTIGUOUS_MODES:                        \
    case ALL_EFFECTFUL_ARRAY_STORAGE_MODES

Array::Mode fromObserved(ArrayProfile*, Array::Action, bool makeSafe);

Array::Mode refineArrayMode(Array::Mode, SpeculatedType base, SpeculatedType index);

bool modeAlreadyChecked(AbstractValue&, Array::Mode);

const char* modeToString(Array::Mode);

inline bool modeUsesButterfly(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case ALL_CONTIGUOUS_MODES:
    case ALL_ARRAY_STORAGE_MODES:
    case ALL_EFFECTFUL_MODES:
        return true;
    default:
        return false;
    }
}

inline bool modeIsJSArray(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case ARRAY_WITH_CONTIGUOUS_MODES:
    case ARRAY_WITH_ARRAY_STORAGE_MODES:
    case ARRAY_EFFECTFUL_MODES:
        return true;
    default:
        return false;
    }
}

inline bool isInBoundsAccess(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case IN_BOUNDS_CONTIGUOUS_MODES:
    case CONTIGUOUS_TO_TAIL_MODES:
    case ARRAY_STORAGE_TO_HOLE_MODES:
    case IN_BOUNDS_ARRAY_STORAGE_MODES:
        return true;
    default:
        return false;
    }
}

inline bool isSlowPutAccess(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case SLOW_PUT_ARRAY_STORAGE_MODES:
    case SLOW_PUT_EFFECTFUL_ARRAY_STORAGE_MODES:
        return true;
    default:
        return false;
    }
}

inline bool mayStoreToTail(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case CONTIGUOUS_TO_TAIL_MODES:
    case OUT_OF_BOUNDS_CONTIGUOUS_MODES:
    case ALL_EFFECTFUL_CONTIGUOUS_MODES:
        return true;
    default:
        return false;
    }
}

inline bool mayStoreToHole(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case ARRAY_STORAGE_TO_HOLE_MODES:
    case OUT_OF_BOUNDS_ARRAY_STORAGE_MODES:
    case SLOW_PUT_ARRAY_STORAGE_MODES:
    case ALL_EFFECTFUL_ARRAY_STORAGE_MODES:
        return true;
    default:
        return false;
    }
}

inline bool canCSEStorage(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case Array::Undecided:
    case Array::Unprofiled:
    case Array::ForceExit:
    case Array::Generic:
    case Array::Arguments:
        return false;
    default:
        return true;
    }
}

inline bool lengthNeedsStorage(Array::Mode arrayMode)
{
    return modeIsJSArray(arrayMode);
}

inline Array::Mode modeForPut(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case Array::String:
        return Array::Generic;
#if USE(JSVALUE32_64)
    case Array::Arguments:
        return Array::Generic;
#endif
    default:
        return arrayMode;
    }
}

inline bool modeIsSpecific(Array::Mode mode)
{
    switch (mode) {
    case Array::Undecided:
    case Array::Unprofiled:
    case Array::ForceExit:
    case Array::Generic:
        return false;
    default:
        return true;
    }
}

inline bool modeSupportsLength(Array::Mode mode)
{
    switch (mode) {
    case Array::Undecided:
    case Array::Unprofiled:
    case Array::ForceExit:
    case Array::Generic:
    case NON_ARRAY_CONTIGUOUS_MODES:
    case NON_ARRAY_ARRAY_STORAGE_MODES:
    case NON_ARRAY_EFFECTFUL_MODES:
        return false;
    default:
        return true;
    }
}

inline bool benefitsFromStructureCheck(Array::Mode mode)
{
    switch (mode) {
    case ALL_EFFECTFUL_MODES:
    case Array::Undecided:
    case Array::Unprofiled:
    case Array::ForceExit:
    case Array::Generic:
        return false;
    default:
        return true;
    }
}

inline bool isEffectful(Array::Mode mode)
{
    switch (mode) {
    case ALL_EFFECTFUL_MODES:
        return true;
    default:
        return false;
    }
}

// This returns the set of array modes that will pass filtering of a CheckArray or
// Arrayify with the given mode.
inline ArrayModes arrayModesFor(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case Array::Generic:
        return ALL_ARRAY_MODES;
    case Array::Contiguous:
    case Array::ContiguousToTail:
    case Array::ContiguousOutOfBounds:
    case Array::ToContiguous:
        return asArrayModes(NonArrayWithContiguous);
    case Array::PossiblyArrayWithContiguous:
    case Array::PossiblyArrayWithContiguousToTail:
    case Array::PossiblyArrayWithContiguousOutOfBounds:
        return asArrayModes(NonArrayWithContiguous) | asArrayModes(ArrayWithContiguous);
    case ARRAY_WITH_CONTIGUOUS_MODES:
        return asArrayModes(ArrayWithContiguous);
    case Array::ArrayStorage:
    case Array::ArrayStorageToHole:
    case Array::ArrayStorageOutOfBounds:
    case Array::ToArrayStorage:
        return asArrayModes(NonArrayWithArrayStorage);
    case Array::ToSlowPutArrayStorage:
    case Array::SlowPutArrayStorage:
        return asArrayModes(NonArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage);
    case Array::PossiblyArrayWithArrayStorage:
    case Array::PossiblyArrayWithArrayStorageToHole:
    case Array::PossiblyArrayWithArrayStorageOutOfBounds:
    case Array::PossiblyArrayToArrayStorage:
        return asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithArrayStorage);
    case Array::PossiblyArrayWithSlowPutArrayStorage:
        return asArrayModes(NonArrayWithArrayStorage) | asArrayModes(ArrayWithArrayStorage) | asArrayModes(NonArrayWithSlowPutArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage);
    case Array::ArrayWithArrayStorage:
    case Array::ArrayWithArrayStorageToHole:
    case Array::ArrayWithArrayStorageOutOfBounds:
    case Array::ArrayToArrayStorage:
        return asArrayModes(ArrayWithArrayStorage);
    case Array::ArrayWithSlowPutArrayStorage:
        return asArrayModes(ArrayWithArrayStorage) | asArrayModes(ArrayWithSlowPutArrayStorage);
    default:
        return asArrayModes(NonArray);
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGArrayMode_h

