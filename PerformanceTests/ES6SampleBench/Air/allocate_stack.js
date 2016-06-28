/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
"use strict";

function allocateStack(code)
{
    if (code.frameSize)
        throw new Error("Frame size already determined");
    
    function attemptAssignment(slot, offsetFromFP, otherSlots)
    {
        if (offsetFromFP > 0)
            throw new Error("Expect negative offset");
        
        offsetFromFP = -roundUpToMultipleOf(slot.alignment, -offsetFromFP);
        
        for (let otherSlot of otherSlots) {
            if (!otherSlot.offsetFromFP)
                continue;
            let overlap = rangesOverlap(
                offsetFromFP,
                offsetFromFP + slot.byteSize,
                otherSlot.offsetFromFP,
                otherSlot.offsetFromFP + otherSlot.byteSize);
            if (overlap)
                return false;
        }
        
        slot.setOffsetFromFP(offsetFromFP);
        return true;
    }
    
    function assign(slot, otherSlots)
    {
        if (attemptAssignment(slot, -slot.byteSize, otherSlots))
            return;
        
        for (let otherSlot of otherSlots) {
            if (!otherSlot.offsetFromFP)
                continue;
            if (attemptAssignment(slot, otherSlot.offsetFromFP - slot.byteSize, otherSlots))
                return;
        }
        
        throw new Error("Assignment failed");
    }
    
    // Allocate all of the escaped slots in order. This is kind of a crazy algorithm to allow for
    // the possibility of stack slots being assigned frame offsets before we even get here.
    let assignedEscapedStackSlots = [];
    let escapedStackSlotsWorklist = [];
    for (let slot of code.stackSlots) {
        if (slot.isLocked) {
            if (slot.offsetFromFP)
                assignedEscapedStackSlots.push(slot);
            else
                escapedStackSlotsWorklist.push(slot);
        } else {
            if (slot.offsetFromFP)
                throw new Error("Offset already assigned");
        }
    }
    
    // This is a fairly espensive loop, but it's OK because we'll usually only have a handful of
    // escaped stack slots.
    while (escapedStackSlotsWorklist.length) {
        let slot = escapedStackSlotsWorklist.pop();
        assign(slot, assignedEscapedStackSlots);
        assignedEscapedStackSlots.push(slot);
    }
    
    // Now we handle the spill slots.
    let liveness = new Liveness(StackSlot, code);
    let interference = new Map();
    for (let slot of code.stackSlots)
        interference.set(slot, new Set());
    let slots = [];
    
    for (let block of code) {
        let localCalc = liveness.localCalc(block);
        
        function interfere(instIndex)
        {
            Inst.forEachDef(
                StackSlot, block.get(instIndex), block.get(instIndex + 1),
                (slot, role, type, width) => {
                    if (!slot.isSpill)
                        return;
                    
                    for (let otherSlot of localCalc.liveSet) {
                        interference.get(slot).add(otherSlot);
                        interference.get(otherSlot).add(slot);
                    }
                });
        }
        
        for (let instIndex = block.size; instIndex--;) {
            // Kill dead stores. For simplicity we say that a store is killable if it has only late
            // defs and those late defs are to things that are dead right now. We only do that
            // because that's the only kind of dead stack store we will see here.
            let inst = block.at(instIndex);
            if (!inst.hasNonArgEffects) {
                let ok = true;
                inst.forEachArg((arg, role, type, width) => {
                    if (Arg.isEarlyDef(role)) {
                        ok = false;
                        return;
                    }
                    if (!Arg.isLateDef(role))
                        return;
                    if (!arg.isStack) {
                        ok = false;
                        return;
                    }
                    
                    let slot = arg.stackSlot;
                    if (!slot.isSpill) {
                        ok = false;
                        return;
                    }
                    
                    if (localCalc.liveSet.has(slot)) {
                        ok = false;
                        return;
                    }
                });
                if (ok)
                    inst.clear();
            }
            
            interfere(instIndex);
            localCalc.execute(instIndex);
        }
        interfere(-1);
        
        removeAllMatching(block.insts, inst => inst.opcode == Nop);
    }
    
    // Now we assign stack locations. At its heart this algorithm is just first-fit. For each
    // StackSlot we just want to find the offsetFromFP that is closest to zero while ensuring no
    // overlap with other StackSlots that this overlaps with.
    for (let slot of code.stackSlots) {
        if (slot.offsetFromFP)
            continue;
        
        assign(slot, assignedEscapedStackSlots.concat(Array.from(interference.get(slot))));
    }
    
    // Figure out how much stack we're using for stack slots.
    let frameSizeForStackSlots = 0;
    for (let slot of code.stackSlots) {
        frameSizeForStackSlots = Math.max(
            frameSizeForStackSlots,
            -slot.offsetFromFP);
    }
    
    frameSizeForStackSlots = roundUpToMultipleOf(stackAlignmentBytes, frameSizeForStackSlots);

    // No we need to deduce how much argument area we need.
    for (let block of code) {
        for (let inst of block) {
            for (let arg of inst.args) {
                if (arg.isCallArg) {
                    // For now, we assume that we use 8 bytes of the call arg. But that's not
                    // such an awesome assumption.
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=150454
                    if (arg.offset < 0)
                        throw new Error("Did not expect negative offset for callArg");
                    code.requestCallArgAreaSize(arg.offset + 8);
                }
            }
        }
    }
    
    code.setFrameSize(frameSizeForStackSlots + code.callArgAreaSize);
    
    // Finally transform the code to use Addrs instead of StackSlots. This is a lossless
    // transformation since we can search the StackSlots array to figure out which StackSlot any
    // offset-from-FP refers to.

    // FIXME: This may produce addresses that aren't valid if we end up with a ginormous stack frame.
    // We would have to scavenge for temporaries if this happened. Fortunately, this case will be
    // extremely rare so we can do crazy things when it arises.
    // https://bugs.webkit.org/show_bug.cgi?id=152530
    
    let insertionSet = new InsertionSet();
    for (let block of code) {
        for (let instIndex = 0; instIndex < block.size; ++instIndex) {
            let inst = block.at(instIndex);
            inst.forEachArg((arg, role, type, width) => {
                function stackAddr(offset)
                {
                    return Arg.createStackAddr(offset, code.frameSize, width);
                }
                
                switch (arg.kind) {
                case Arg.Stack: {
                    let slot = arg.stackSlot;
                    if (Arg.isZDef(role)
                        && slot.isSpill
                        && slot.byteSize > Arg.bytes(width)) {
                        // Currently we only handle this simple case because it's the only one
                        // that arises: ZDef's are only 32-bit right now. So, when we hit these
                        // assertions it means that we need to implement those other kinds of
                        // zero fills.
                        if (slot.byteSize != 8) {
                            throw new Error(
                                `Bad spill slot size for ZDef: ${slot.byteSize}, width is ${width}`);
                        }
                        if (width != 32)
                            throw new Error("Bad width for ZDef");
                        
                        insertionSet.insert(
                            instIndex + 1,
                            new Inst(
                                StoreZero32,
                                [stackAddr(arg.offset + 4 + slot.offsetFromFP)]));
                    }
                    return stackAddr(arg.offset + slot.offsetFromFP);
                }
                case Arg.CallArg:
                    return stackAddr(arg.offset - code.frameSize);
                default:
                    break;
                }
            });
        }
        insertionSet.execute(block.insts);
    }
}
