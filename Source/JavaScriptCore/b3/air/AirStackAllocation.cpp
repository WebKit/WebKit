/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "AirStackAllocation.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "StackAlignment.h"
#include <wtf/ListDump.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

namespace AirStackAllocationInternal {
static constexpr bool verbose = false;
}

template<typename Collection>
void updateFrameSizeBasedOnStackSlotsImpl(Code& code, const Collection& collection)
{
    unsigned frameSize = 0;
    for (StackSlot* slot : collection)
        frameSize = std::max(frameSize, static_cast<unsigned>(-slot->offsetFromFP()));
    code.setFrameSize(WTF::roundUpToMultipleOf(stackAlignmentBytes(), frameSize) + stackAdjustmentForAlignment());
}

} // anonymous namespace

bool attemptAssignment(
    StackSlot* slot, intptr_t offsetFromFP, const Vector<StackSlot*>& otherSlots)
{
    if (AirStackAllocationInternal::verbose)
        dataLog("Attempting to assign ", pointerDump(slot), " to ", offsetFromFP, " with interference ", pointerListDump(otherSlots), "\n");

    // Need to align it to the slot's desired alignment.
    offsetFromFP = -WTF::roundUpToMultipleOf(slot->alignment(), -offsetFromFP);
    
    for (StackSlot* otherSlot : otherSlots) {
        if (!otherSlot->offsetFromFP())
            continue;
        bool overlap = WTF::rangesOverlap(
            offsetFromFP,
            offsetFromFP + static_cast<intptr_t>(slot->byteSize()),
            otherSlot->offsetFromFP(),
            otherSlot->offsetFromFP() + static_cast<intptr_t>(otherSlot->byteSize()));
        if (overlap)
            return false;
    }

    if (AirStackAllocationInternal::verbose)
        dataLog("Assigned ", pointerDump(slot), " to ", offsetFromFP, "\n");
    slot->setOffsetFromFP(offsetFromFP);
    return true;
}

void assign(StackSlot* slot, const Vector<StackSlot*>& otherSlots)
{
    if (AirStackAllocationInternal::verbose)
        dataLog("Attempting to assign ", pointerDump(slot), " with interference ", pointerListDump(otherSlots), "\n");
    
    if (attemptAssignment(slot, -static_cast<intptr_t>(slot->byteSize()), otherSlots))
        return;

    for (StackSlot* otherSlot : otherSlots) {
        if (!otherSlot->offsetFromFP())
            continue;
        bool didAssign = attemptAssignment(
            slot, otherSlot->offsetFromFP() - static_cast<intptr_t>(slot->byteSize()), otherSlots);
        if (didAssign)
            return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

Vector<StackSlot*> allocateAndGetEscapedStackSlotsWithoutChangingFrameSize(Code& code)
{
    // Allocate all of the escaped slots in order. This is kind of a crazy algorithm to allow for
    // the possibility of stack slots being assigned frame offsets before we even get here.
    RELEASE_ASSERT(code.frameSize() == stackAdjustmentForAlignment());
    Vector<StackSlot*> assignedEscapedStackSlots;
    Vector<StackSlot*> escapedStackSlotsWorklist;
    for (StackSlot* slot : code.stackSlots()) {
        if (slot->isLocked()) {
            if (slot->offsetFromFP())
                assignedEscapedStackSlots.append(slot);
            else
                escapedStackSlotsWorklist.append(slot);
        } else {
            // It would be super strange to have an unlocked stack slot that has an offset already.
            ASSERT(!slot->offsetFromFP());
        }
    }
    // This is a fairly expensive loop, but it's OK because we'll usually only have a handful of
    // escaped stack slots.
    while (!escapedStackSlotsWorklist.isEmpty()) {
        StackSlot* slot = escapedStackSlotsWorklist.takeLast();
        assign(slot, assignedEscapedStackSlots);
        assignedEscapedStackSlots.append(slot);
    }
    return assignedEscapedStackSlots;
}

void allocateEscapedStackSlots(Code& code)
{
    updateFrameSizeBasedOnStackSlotsImpl(
        code, allocateAndGetEscapedStackSlotsWithoutChangingFrameSize(code));
}

void updateFrameSizeBasedOnStackSlots(Code& code)
{
    updateFrameSizeBasedOnStackSlotsImpl(code, code.stackSlots());
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

