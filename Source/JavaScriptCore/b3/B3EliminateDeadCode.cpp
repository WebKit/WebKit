/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "B3EliminateDeadCode.h"

#if ENABLE(B3_JIT)

#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include <wtf/GraphNodeWorklist.h>
#include <wtf/IndexSet.h>
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

// FIXME: this pass currently only eliminates values and variables, it does not seem to eliminate dead blocks.

bool eliminateDeadCodeImpl(Procedure& proc)
{
    bool changed = false;
    GraphNodeWorklist<Value*, IndexSet<Value*>> worklist;
    Vector<UpsilonValue*, 64> upsilons;
    for (BasicBlock* block : proc) {
        for (Value* value : *block) {
            Effects effects;
            // We don't care about effects of SSA operations, since we model them more
            // accurately than the effects() method does.
            if (value->opcode() != Phi && value->opcode() != Upsilon)
                effects = value->effects();
            
            if (effects.mustExecute())
                worklist.push(value);
            
            if (UpsilonValue* upsilon = value->as<UpsilonValue>())
                upsilons.append(upsilon);
        }
    }
    for (;;) {
        while (Value* value = worklist.pop()) {
            for (Value* child : value->children())
                worklist.push(child);
        }
        
        bool didPush = false;
        for (size_t upsilonIndex = 0; upsilonIndex < upsilons.size(); ++upsilonIndex) {
            UpsilonValue* upsilon = upsilons[upsilonIndex];
            if (worklist.saw(upsilon->phi())) {
                worklist.push(upsilon);
                upsilons[upsilonIndex--] = upsilons.last();
                upsilons.takeLast();
                didPush = true;
            }
        }
        if (!didPush)
            break;
    }

    IndexSet<Variable*> liveVariables;
    
    for (BasicBlock* block : proc) {
        size_t sourceIndex = 0;
        size_t targetIndex = 0;
        while (sourceIndex < block->size()) {
            Value* value = block->at(sourceIndex++);
            if (worklist.saw(value)) {
                if (VariableValue* variableValue = value->as<VariableValue>())
                    liveVariables.add(variableValue->variable());
                block->at(targetIndex++) = value;
            } else {
                proc.deleteValue(value);
                changed = true;
            }
        }
        block->values().resize(targetIndex);
    }

    for (Variable* variable : proc.variables()) {
        if (!liveVariables.contains(variable))
            proc.deleteVariable(variable);
    }
    return changed;
}

bool eliminateDeadCode(Procedure& proc)
{
    PhaseScope phaseScope(proc, "eliminateDeadCode");
    return eliminateDeadCodeImpl(proc);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
