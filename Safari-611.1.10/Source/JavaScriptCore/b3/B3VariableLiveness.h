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

#include "B3BasicBlock.h"
#include "B3CFG.h"
#include "B3Procedure.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include <wtf/Liveness.h>

namespace JSC { namespace B3 {

struct VariableLivenessAdapter {
    static constexpr const char* name = "VariableLiveness";
    typedef B3::CFG CFG;
    typedef Variable* Thing;
    
    VariableLivenessAdapter(Procedure& proc)
        : proc(proc)
    {
    }
    
    void prepareToCompute()
    {
    }
    
    unsigned numIndices()
    {
        return proc.variables().size();
    }
    
    static unsigned valueToIndex(Variable* var) { return var->index(); }
    Variable* indexToValue(unsigned index) { return proc.variables()[index]; }
    
    unsigned blockSize(BasicBlock* block)
    {
        return block->size();
    }
    
    template<typename Func>
    void forEachUse(BasicBlock* block, unsigned valueBoundaryIndex, const Func& func)
    {
        // We want all of the uses that happen between valueBoundaryIndex-1 and
        // valueBoundaryIndex. Since the Get opcode is the only value that has a use and since
        // this is an early use, we only care about block[valueBoundaryIndex].
        Value* value = block->get(valueBoundaryIndex);
        if (!value)
            return;
        if (value->opcode() != Get)
            return;
        func(value->as<VariableValue>()->variable()->index());
    }
    
    template<typename Func>
    void forEachDef(BasicBlock* block, unsigned valueBoundaryIndex, const Func& func)
    {
        // We want all of the defs that happen between valueBoundaryIndex-1 and
        // valueBoundaryIndex. Since the Set opcode is the only value that has a def and since
        // this is an late def, we only care about block[valueBoundaryIndex - 1].
        Value* value = block->get(valueBoundaryIndex - 1);
        if (!value)
            return;
        if (value->opcode() != Set)
            return;
        func(value->as<VariableValue>()->variable()->index());
    }
    
    Procedure& proc;
};

class VariableLiveness : public WTF::Liveness<VariableLivenessAdapter> {
public:
    VariableLiveness(Procedure&);
    ~VariableLiveness();
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

