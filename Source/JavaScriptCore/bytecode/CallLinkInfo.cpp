/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include "CallLinkInfo.h"

#include "DFGOperations.h"
#include "DFGThunks.h"
#include "RepatchBuffer.h"

#if ENABLE(JIT)
namespace JSC {

void CallLinkInfo::unlink(VM& vm, RepatchBuffer& repatchBuffer)
{
    ASSERT(isLinked());
    
    if (Options::showDisassembly())
        dataLog("Unlinking call from ", callReturnLocation, " to ", pointerDump(repatchBuffer.codeBlock()), "\n");

    repatchBuffer.revertJumpReplacementToBranchPtrWithPatch(RepatchBuffer::startOfBranchPtrWithPatchOnRegister(hotPathBegin), static_cast<MacroAssembler::RegisterID>(calleeGPR), 0);
    repatchBuffer.relink(
        callReturnLocation,
        vm.getCTIStub(linkThunkGeneratorFor(
            callType == Construct ? CodeForConstruct : CodeForCall,
            isFTL ? MustPreserveRegisters : RegisterPreservationNotRequired)).code());
    hasSeenShouldRepatch = false;
    callee.clear();
    stub.clear();

    // It will be on a list if the callee has a code block.
    if (isOnList())
        remove();
}

} // namespace JSC
#endif // ENABLE(JIT)

