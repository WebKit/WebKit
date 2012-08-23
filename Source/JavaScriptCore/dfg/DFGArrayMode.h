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
enum Mode {
    Undecided, // Implies that we need predictions to decide. We will never get to the backend in this mode.
    ForceExit, // Implies that we have no idea how to execute this operation, so we should just give up.
    Generic,
    String,
    JSArray,
    JSArrayOutOfBounds,
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

Array::Mode fromObserved(ArrayModes modes, bool makeSafe);

Array::Mode refineArrayMode(Array::Mode, SpeculatedType base, SpeculatedType index);

bool modeAlreadyChecked(AbstractValue&, Array::Mode);

const char* modeToString(Array::Mode);

inline bool canCSEStorage(Array::Mode arrayMode)
{
    switch (arrayMode) {
    case Array::Undecided:
    case Array::ForceExit:
    case Array::Generic:
    case Array::Arguments:
        return false;
    default:
        return true;
    }
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

inline bool modesCompatibleForStorageLoad(Array::Mode left, Array::Mode right)
{
    if (left == right)
        return true;
    
    bool leftIsJSArray =
        left == Array::JSArray
        || left == Array::JSArrayOutOfBounds;
    
    bool rightIsJSArray =
        right == Array::JSArray
        || right == Array::JSArrayOutOfBounds;
    
    if (leftIsJSArray && rightIsJSArray)
        return true;
    
    return false;
}

inline bool modeSupportsLength(Array::Mode mode)
{
    switch (mode) {
    case Array::Undecided:
    case Array::ForceExit:
    case Array::Generic:
        return false;
    default:
        return true;
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGArrayMode_h

