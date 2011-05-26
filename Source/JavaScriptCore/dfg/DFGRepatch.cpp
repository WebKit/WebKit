/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGRepatch.h"

#if ENABLE(DFG_JIT)

#include "DFGOperations.h"
#include "RepatchBuffer.h"

namespace JSC { namespace DFG {

static void dfgRepatchCall(CodeBlock* codeblock, CodeLocationCall call, FunctionPtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchBuffer.relink(call, newCalleeFunction);
}

static void dfgRepatchGetByIdSelf(CodeBlock* codeBlock, StructureStubInfo& stubInfo, Structure* structure, size_t offset)
{
    RepatchBuffer repatchBuffer(codeBlock);

    // Only optimize once!
    repatchBuffer.relink(stubInfo.callReturnLocation, operationGetById);

    // Patch the structure check & the offset of the load.
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelPtrAtOffset(-(intptr_t)stubInfo.u.unset.deltaCheckToCall), structure);
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.u.unset.deltaCallToLoad), sizeof(JSValue) * offset);
}

static bool tryCacheGetByID(ExecState* exec, JSValue baseValue, const Identifier&, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    // FIXME: Write a test that proves we need to check for recursion here just
    // like the interpreter does, then add a check for recursion.

    CodeBlock* codeBlock = exec->codeBlock();
    JSGlobalData* globalData = &exec->globalData();

    // FIXME: should support length access for Array & String.

    // FIXME: Cache property access for immediates.
    if (!baseValue.isCell())
        return false;
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    if (!slot.isCacheable())
        return false;
    if (structure->isUncacheableDictionary())
        return false;

    // Optimize self access.
    if (slot.slotBase() == baseValue) {
        if ((slot.cachedPropertyType() != PropertySlot::Value) || ((slot.cachedOffset() * sizeof(JSValue)) > (unsigned)MacroAssembler::MaximumCompactPtrAlignedAddressOffset))
            return false;

        dfgRepatchGetByIdSelf(codeBlock, stubInfo, structure, slot.cachedOffset());
        stubInfo.initGetByIdSelf(*globalData, codeBlock->ownerExecutable(), structure);
        return true;
    }

    // FIXME: should support prototype & chain accesses!
    return false;
}

void dfgRepatchGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    bool cached = tryCacheGetByID(exec, baseValue, propertyName, slot, stubInfo);
    if (!cached)
        dfgRepatchCall(exec->codeBlock(), stubInfo.callReturnLocation, operationGetById);
}

} } // namespace JSC::DFG

#endif
