/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "B3FixSSA.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3BreakCriticalEdges.h"
#include "B3Dominators.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3SSACalculator.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableLiveness.h"
#include "B3VariableValue.h"
#include <wtf/CommaPrinter.h>
#include <wtf/IndexSet.h>
#include <wtf/IndexSparseSet.h>

namespace JSC { namespace B3 {

namespace {

namespace B3FixSSAInternal {
static const bool verbose = false;
}

void killDeadVariables(Procedure& proc)
{
    IndexSet<Variable*> liveVariables;
    for (Value* value : proc.values()) {
        if (value->opcode() == Get)
            liveVariables.add(value->as<VariableValue>()->variable());
    }

    for (Value* value : proc.values()) {
        if (value->opcode() == Set && !liveVariables.contains(value->as<VariableValue>()->variable()))
            value->replaceWithNop();
    }

    for (Variable* variable : proc.variables()) {
        if (!liveVariables.contains(variable))
            proc.deleteVariable(variable);
    }
}

void fixSSALocally(Procedure& proc)
{
    IndexSparseSet<KeyValuePair<unsigned, Value*>> mapping(proc.variables().size());
    for (BasicBlock* block : proc.blocksInPreOrder()) {
        mapping.clear();

        for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
            Value* value = block->at(valueIndex);
            value->performSubstitution();

            switch (value->opcode()) {
            case Get: {
                VariableValue* variableValue = value->as<VariableValue>();
                Variable* variable = variableValue->variable();

                if (KeyValuePair<unsigned, Value*>* replacement = mapping.get(variable->index()))
                    value->replaceWithIdentity(replacement->value);
                break;
            }
                
            case Set: {
                VariableValue* variableValue = value->as<VariableValue>();
                Variable* variable = variableValue->variable();

                mapping.set(variable->index(), value->child(0));
                break;
            }

            default:
                break;
            }
        }
    }
}

void fixSSAGlobally(Procedure& proc)
{
    VariableLiveness liveness(proc);
    
    // Kill any dead Set's. Each Set creates work for us, so this is profitable.
    for (BasicBlock* block : proc) {
        VariableLiveness::LocalCalc localCalc(liveness, block);
        for (unsigned valueIndex = block->size(); valueIndex--;) {
            Value* value = block->at(valueIndex);
            if (value->opcode() == Set && !localCalc.isLive(value->as<VariableValue>()->variable()))
                value->replaceWithNop();
            localCalc.execute(valueIndex);
        }
    }
    
    VariableLiveness::LiveAtHead liveAtHead = liveness.liveAtHead();
    
    SSACalculator ssa(proc);

    // Create a SSACalculator::Variable ("calcVar") for every variable.
    Vector<Variable*> calcVarToVariable;
    IndexMap<Variable*, SSACalculator::Variable*> variableToCalcVar(proc.variables().size());

    for (Variable* variable : proc.variables()) {
        SSACalculator::Variable* calcVar = ssa.newVariable();
        RELEASE_ASSERT(calcVar->index() == calcVarToVariable.size());
        calcVarToVariable.append(variable);
        variableToCalcVar[variable] = calcVar;
    }

    // Create Defs for all of the stores to the stack variable.
    for (BasicBlock* block : proc) {
        for (Value* value : *block) {
            if (value->opcode() != Set)
                continue;

            Variable* variable = value->as<VariableValue>()->variable();

            if (SSACalculator::Variable* calcVar = variableToCalcVar[variable])
                ssa.newDef(calcVar, block, value->child(0));
        }
    }

    // Decide where Phis are to be inserted. This creates them but does not insert them.
    {
        TimingScope timingScope("fixSSA: computePhis");
        ssa.computePhis(
            [&] (SSACalculator::Variable* calcVar, BasicBlock* block) -> Value* {
                Variable* variable = calcVarToVariable[calcVar->index()];
                if (!liveAtHead.isLiveAtHead(block, variable))
                    return nullptr;
                
                Value* phi = proc.add<Value>(Phi, variable->type(), block->at(0)->origin());
                if (B3FixSSAInternal::verbose) {
                    dataLog(
                        "Adding Phi for ", pointerDump(variable), " at ", *block, ": ",
                        deepDump(proc, phi), "\n");
                }
                return phi;
            });
    }

    // Now perform the conversion.
    TimingScope timingScope("fixSSA: convert");
    InsertionSet insertionSet(proc);
    IndexSparseSet<KeyValuePair<unsigned, Value*>> mapping(proc.variables().size());
    IndexSet<Value*> valuesToDelete;
    for (BasicBlock* block : proc.blocksInPreOrder()) {
        mapping.clear();
        
        auto ensureMapping = [&] (Variable* variable, unsigned valueIndex, Origin origin) -> Value* {
            KeyValuePair<unsigned, Value*>* found = mapping.get(variable->index());
            if (found)
                return found->value;
            
            SSACalculator::Variable* calcVar = variableToCalcVar[variable];
            SSACalculator::Def* def = ssa.reachingDefAtHead(block, calcVar);
            if (def) {
                mapping.set(variable->index(), def->value());
                return def->value();
            }
            
            return insertionSet.insertBottom(valueIndex, origin, variable->type());
        };

        for (SSACalculator::Def* phiDef : ssa.phisForBlock(block)) {
            Variable* variable = calcVarToVariable[phiDef->variable()->index()];

            insertionSet.insertValue(0, phiDef->value());
            mapping.set(variable->index(), phiDef->value());
        }

        for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
            Value* value = block->at(valueIndex);
            value->performSubstitution();

            switch (value->opcode()) {
            case Get: {
                VariableValue* variableValue = value->as<VariableValue>();
                Variable* variable = variableValue->variable();

                value->replaceWithIdentity(ensureMapping(variable, valueIndex, value->origin()));
                valuesToDelete.add(value);
                break;
            }
                
            case Set: {
                VariableValue* variableValue = value->as<VariableValue>();
                Variable* variable = variableValue->variable();

                mapping.set(variable->index(), value->child(0));
                value->replaceWithNop();
                break;
            }

            default:
                break;
            }
        }

        unsigned upsilonInsertionPoint = block->size() - 1;
        Origin upsilonOrigin = block->last()->origin();
        for (BasicBlock* successorBlock : block->successorBlocks()) {
            for (SSACalculator::Def* phiDef : ssa.phisForBlock(successorBlock)) {
                Value* phi = phiDef->value();
                SSACalculator::Variable* calcVar = phiDef->variable();
                Variable* variable = calcVarToVariable[calcVar->index()];

                Value* mappedValue = ensureMapping(variable, upsilonInsertionPoint, upsilonOrigin);
                if (B3FixSSAInternal::verbose) {
                    dataLog(
                        "Mapped value for ", *variable, " with successor Phi ", *phi,
                        " at end of ", *block, ": ", pointerDump(mappedValue), "\n");
                }
                
                insertionSet.insert<UpsilonValue>(
                    upsilonInsertionPoint, upsilonOrigin, mappedValue->foldIdentity(), phi);
            }
        }

        insertionSet.execute(block);
    }
    
    // This is isn't strictly necessary, but it leaves the IR nice and tidy, which is particularly
    // useful for phases that do size estimates.
    for (BasicBlock* block : proc) {
        block->values().removeAllMatching(
            [&] (Value* value) -> bool {
                if (!valuesToDelete.contains(value) && value->opcode() != Nop)
                    return false;
                
                proc.deleteValue(value);
                return true;
            });
    }

    if (B3FixSSAInternal::verbose) {
        dataLog("B3 after SSA conversion:\n");
        dataLog(proc);
    }
}

} // anonymous namespace

void demoteValues(Procedure& proc, const IndexSet<Value*>& values)
{
    HashMap<Value*, Variable*> map;
    HashMap<Value*, Variable*> phiMap;

    // Create stack slots.
    for (Value* value : values.values(proc.values())) {
        map.add(value, proc.addVariable(value->type()));

        if (value->opcode() == Phi)
            phiMap.add(value, proc.addVariable(value->type()));
    }

    if (B3FixSSAInternal::verbose) {
        dataLog("Demoting values as follows:\n");
        dataLog("   map = ");
        CommaPrinter comma;
        for (auto& entry : map)
            dataLog(comma, *entry.key, "=>", *entry.value);
        dataLog("\n");
        dataLog("   phiMap = ");
        comma = CommaPrinter();
        for (auto& entry : phiMap)
            dataLog(comma, *entry.key, "=>", *entry.value);
        dataLog("\n");
    }

    // Change accesses to the values to accesses to the stack slots.
    InsertionSet insertionSet(proc);
    for (BasicBlock* block : proc) {
        if (block->numPredecessors()) {
            // Deal with terminals that produce values (i.e. patchpoint terminals, like the ones we
            // generate for the allocation fast path).
            Value* value = block->predecessor(0)->last();
            Variable* variable = map.get(value);
            if (variable) {
                RELEASE_ASSERT(block->numPredecessors() == 1); // Critical edges better be broken.
                insertionSet.insert<VariableValue>(0, Set, value->origin(), variable, value);
            }
        }
        
        for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
            Value* value = block->at(valueIndex);

            if (value->opcode() == Phi) {
                if (Variable* variable = phiMap.get(value)) {
                    value->replaceWithIdentity(
                        insertionSet.insert<VariableValue>(
                            valueIndex, Get, value->origin(), variable));
                }
            } else {
                for (Value*& child : value->children()) {
                    if (Variable* variable = map.get(child)) {
                        child = insertionSet.insert<VariableValue>(
                            valueIndex, Get, value->origin(), variable);
                    }
                }

                if (UpsilonValue* upsilon = value->as<UpsilonValue>()) {
                    if (Variable* variable = phiMap.get(upsilon->phi())) {
                        insertionSet.insert<VariableValue>(
                            valueIndex, Set, upsilon->origin(), variable, upsilon->child(0));
                        value->replaceWithNop();
                    }
                }
            }

            if (Variable* variable = map.get(value)) {
                if (valueIndex + 1 < block->size()) {
                    insertionSet.insert<VariableValue>(
                        valueIndex + 1, Set, value->origin(), variable, value);
                }
            }
        }
        insertionSet.execute(block);
    }
}

bool fixSSA(Procedure& proc)
{
    PhaseScope phaseScope(proc, "fixSSA");

    if (proc.variables().isEmpty())
        return false;
    
    // Lots of variables have trivial local liveness. We can allocate those without any
    // trouble.
    fixSSALocally(proc);

    // Just for sanity, remove any unused variables first. It's unlikely that this code has any
    // bugs having to do with dead variables, but it would be silly to have to fix such a bug if
    // it did arise.
    killDeadVariables(proc);
    
    if (proc.variables().isEmpty())
        return false;
    
    breakCriticalEdges(proc);

    fixSSAGlobally(proc);
    
    return true;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

