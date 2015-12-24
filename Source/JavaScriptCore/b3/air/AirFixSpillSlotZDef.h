/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef AirFixSpillSlotZDef_h
#define AirFixSpillSlotZDef_h

#include "AirCode.h"
#include "AirInsertionSet.h"
#include "AirInstInlines.h"

namespace JSC { namespace B3 { namespace Air {

template<typename IsSpillSlot>
void fixSpillSlotZDef(Code& code, const IsSpillSlot& isSpillSlot)
{
    // We could have introduced ZDef's to StackSlots that are wider than the def. In that case, we
    // need to emit code to zero-fill the top bits of the StackSlot.
    InsertionSet insertionSet(code);
    for (BasicBlock* block : code) {
        for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
            Inst& inst = block->at(instIndex);

            inst.forEachArg(
                [&] (Arg& arg, Arg::Role role, Arg::Type, Arg::Width width) {
                    if (!Arg::isZDef(role))
                        return;
                    if (!arg.isStack())
                        return;
                    if (!isSpillSlot(arg.stackSlot()))
                        return;
                    if (arg.stackSlot()->byteSize() == Arg::bytes(width))
                        return;

                    // Currently we only handle this simple case because it's the only one that
                    // arises: ZDef's are only 32-bit right now. So, when we hit these
                    // assertions it means that we need to implement those other kinds of zero
                    // fills.
                    RELEASE_ASSERT(arg.stackSlot()->byteSize() == 8);
                    RELEASE_ASSERT(width == Arg::Width32);

                    // We rely on the fact that there must be some way to move zero to a memory
                    // location without first burning a register. On ARM, we would do this using
                    // zr.
                    RELEASE_ASSERT(isValidForm(Move32, Arg::Imm, Arg::Stack));
                    insertionSet.insert(
                        instIndex + 1, Move32, inst.origin, Arg::imm(0), arg.withOffset(4));
                });
        }
        insertionSet.execute(block);
    }
}

} } } // namespace JSC::B3::Air

#endif // AirFixSpillSlotZDef_h

