/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "B3BreakCriticalEdges.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3SSACalculator.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableLiveness.h"
#include "B3VariableValue.h"
#include "CompilerTimingScope.h"
#include <wtf/CommaPrinter.h>
#include <wtf/IndexSet.h>
#include <wtf/IndexSparseSet.h>

namespace JSC { namespace B3 {

namespace {

namespace B3FixSSAInternal {
static constexpr bool verbose = false;
}

// Many variables often have trivial local liveness (i.e. within a single block), which we can resolve
// without global SSA. Returns true if no variables remain.
bool fixSSALocally(Procedure& proc, IndexSet<Value*>& valuesToDelete)
{
    IndexSparseSet<KeyValuePair<unsigned, Value*>> mapping(proc.variables().size());
    IndexSet<Variable*> liveVariables;
    Vector<Value*> sets;
    for (BasicBlock* block : proc) {
        mapping.clear();

        for (Value* value : *block) {
            switch (value->opcode()) {
            case Get: {
                Variable* variable = value->as<VariableValue>()->variable();
                if (KeyValuePair<unsigned, Value*>* replacement = mapping.get(variable->index())) {
                    value->replaceWithIdentity(replacement->value);
                    valuesToDelete.add(value);
                } else
                    liveVariables.add(variable);
                break;
            }

            case Set: {
                Variable* variable = value->as<VariableValue>()->variable();
                mapping.set(variable->index(), value->child(0)->foldIdentity());
                sets.append(value);
                break;
            }

            default:
                break;
            }
        }
    }

    for (Value* set : sets) {
        if (!liveVariables.contains(set->as<VariableValue>()->variable()))
            set->replaceWithNop();
    }

    if (liveVariables.isEmpty()) {
        proc.deleteAllVariables();
        return true;
    }

    for (Variable* variable : proc.variables()) {
        if (!liveVariables.contains(variable))
            proc.deleteVariable(variable);
    }
    return false;
}

void fixSSAGlobally(Procedure& proc, IndexSet<Value*>& valuesToDelete)
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

    for (BasicBlock* block : proc) {
        for (Value* value : *block) {
            if (value->opcode() != Set)
                continue;

            Variable* variable = value->as<VariableValue>()->variable();
            ssa.newDefBlock(variableToCalcVar[variable], block);
        }
    }

    // Decide where Phis are to be inserted. This creates them but does not insert them.
    {
        CompilerTimingScope timingScope("B3"_s, "fixSSA: computePhis"_s);
        ssa.computePhis(
            [&](SSACalculator::Variable* calcVar, BasicBlock* block) -> Value* {
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
    CompilerTimingScope timingScope("B3"_s, "fixSSA: convert"_s);
    InsertionSet insertionSet(proc);

    Vector<Value*> reachingDefs(FillWith { }, proc.variables().size(), nullptr);
    Vector<std::pair<unsigned, Value*>> undoLog;
    Vector<unsigned, 16> undoLogSizeAtEntry;
    auto setReachingDef = [&](Variable* variable, Value* value) {
        undoLog.append({ variable->index(), reachingDefs[variable->index()] });
        reachingDefs[variable->index()] = value;
    };

    proc.dominators().forAllBlocksInDominatorTreePreOrder(
        [&](BasicBlock* block) {
            undoLogSizeAtEntry.append(undoLog.size());

            auto reachingDef = [&](Variable* variable, unsigned valueIndex, Origin origin) -> Value* {
                if (Value* def = reachingDefs[variable->index()])
                    return def;
                Value* bottom = insertionSet.insertBottom(valueIndex, origin, variable->type());
                setReachingDef(variable, bottom);
                return bottom;
            };

            for (SSACalculator::Def* phiDef : ssa.phisForBlock(block)) {
                Variable* variable = calcVarToVariable[phiDef->variable()->index()];

                insertionSet.insertValue(0, phiDef->value());
                setReachingDef(variable, phiDef->value());
            }

            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                Value* value = block->at(valueIndex);
                value->performSubstitution();

                switch (value->opcode()) {
                case Get: {
                    Variable* variable = value->as<VariableValue>()->variable();
                    value->replaceWithIdentity(reachingDef(variable, valueIndex, value->origin()));
                    valuesToDelete.add(value);
                    break;
                }

                case Set: {
                    Variable* variable = value->as<VariableValue>()->variable();
                    setReachingDef(variable, value->child(0));
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
                    Variable* variable = calcVarToVariable[phiDef->variable()->index()];

                    Value* mappedValue = reachingDef(variable, upsilonInsertionPoint, upsilonOrigin);
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
        },
        [&](BasicBlock* block) {
            unsigned undoLogSize = undoLogSizeAtEntry.takeLast();
            while (undoLog.size() > undoLogSize) {
                auto [index, previous] = undoLog.takeLast();
                reachingDefs[index] = previous;
            }

            // Now that we've visited every block these `Get`s dominate, and all their users have been
            // substituted, we can delete the `Get`. This isn't strictly necessary, but it leaves the IR
            // tidy for phases that do size estimates.
            block->values().removeAllMatching(
                [&](Value* value) -> bool {
                    if (!valuesToDelete.remove(value) && value->opcode() != Nop)
                        return false;

                    proc.deleteValue(value);
                    return true;
                });
        });

    if (B3FixSSAInternal::verbose) {
        dataLog("B3 after SSA conversion:\n");
        dataLog(proc);
    }
}

} // anonymous namespace

void demoteValues(Procedure& proc, const IndexSet<Value*>& values)
{
    UncheckedKeyHashMap<Value*, Variable*> map;
    UncheckedKeyHashMap<Value*, Variable*> phiMap;

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
    PhaseScope phaseScope(proc, "fixSSA"_s);

    if (proc.variables().isEmpty())
        return false;
    
    IndexSet<Value*> valuesToDelete;
    if (fixSSALocally(proc, valuesToDelete)) {
        // Users of a forwarded Get can be in any block, so substitute everywhere before deleting.
        for (Value* value : proc.values())
            value->performSubstitution();
        for (BasicBlock* block : proc) {
            block->values().removeAllMatching(
                [&](Value* value) -> bool {
                    if (!valuesToDelete.remove(value))
                        return false;

                    proc.deleteValue(value);
                    return true;
                });
        }
        return false;
    }

    breakCriticalEdges(proc);

    fixSSAGlobally(proc, valuesToDelete);

    proc.deleteAllVariables();

    return true;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

