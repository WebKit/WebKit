/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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
#include "JSMarkingConstraintPrivate.h"

#include "APICast.h"
#include "JSCInlines.h"
#include "SimpleMarkingConstraint.h"

using namespace JSC;

namespace {

Atomic<unsigned> constraintCounter;

struct Marker : JSMarker {
    SlotVisitor* visitor;
};

bool isMarked(JSMarkerRef markerRef, JSObjectRef objectRef)
{
    if (!objectRef)
        return true; // Null is an immortal object.
    
    return static_cast<Marker*>(markerRef)->visitor->vm().heap.isMarked(toJS(objectRef));
}

void mark(JSMarkerRef markerRef, JSObjectRef objectRef)
{
    if (!objectRef)
        return;
    
    static_cast<Marker*>(markerRef)->visitor->appendHiddenUnbarriered(toJS(objectRef));
}

} // anonymous namespace

void JSContextGroupAddMarkingConstraint(JSContextGroupRef group, JSMarkingConstraint constraintCallback, void *userData)
{
    VM& vm = *toJS(group);
    JSLockHolder locker(vm);
    
    unsigned constraintIndex = constraintCounter.exchangeAdd(1);
    
    // This is a guess. The algorithm should be correct no matter what we pick. This means
    // that we expect this constraint to mark things even during a stop-the-world full GC, but
    // we don't expect it to be able to mark anything at the very start of a GC before anything
    // else gets marked.
    ConstraintVolatility volatility = ConstraintVolatility::GreyedByMarking;
    
    auto constraint = makeUnique<SimpleMarkingConstraint>(
        toCString("Amc", constraintIndex, "(", RawPointer(bitwise_cast<void*>(constraintCallback)), ")"),
        toCString("API Marking Constraint #", constraintIndex, " (", RawPointer(bitwise_cast<void*>(constraintCallback)), ", ", RawPointer(userData), ")"),
        [constraintCallback, userData]
        (SlotVisitor& slotVisitor) {
            Marker marker;
            marker.IsMarked = isMarked;
            marker.Mark = mark;
            marker.visitor = &slotVisitor;
            
            constraintCallback(&marker, userData);
        },
        volatility,
        ConstraintConcurrency::Sequential);
    
    vm.heap.addMarkingConstraint(WTFMove(constraint));
}
