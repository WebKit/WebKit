/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(B3_JIT)

#include <wtf/Vector.h>

namespace JSC { namespace B3 { namespace Air {

class Code;
class StackSlot;

// This is a collection of utilities shared by both stack allocators
// (allocateStackByGraphColoring and allocateRegistersAndStackByLinearScan).

// Attempt to put the given stack slot at the given offset. Returns false if this would cause
// the slot to overlap with any of the given slots.
bool attemptAssignment(StackSlot*, intptr_t offsetFromFP, const Vector<StackSlot*>& adjacent);

// Performs a first-fit assignment (smallest possible offset) of the given stack slot such that
// it does not overlap with any of the adjacent slots.
void assign(StackSlot*, const Vector<StackSlot*>& adjacent);

// Allocates all stack slots that escape - that is, that don't have live ranges that can be
// determined by looking at their uses. Returns a vector of slots that got assigned offsets.
// This assumes that no stack allocation has happened previously, and so frame size is zero.
Vector<StackSlot*> allocateAndGetEscapedStackSlotsWithoutChangingFrameSize(Code&);

// Same as allocateAndGetEscapedStackSlotsWithoutChangingFrameSize, but does not return the
// assigned slots, and does set the frame size based on the largest extent of any of the
// allocated slots.
void allocateEscapedStackSlots(Code&);

// Updates Code::frameSize based on the largest extent of any stack slot. This is useful to
// call after performing stack allocation.
void updateFrameSizeBasedOnStackSlots(Code&);

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

