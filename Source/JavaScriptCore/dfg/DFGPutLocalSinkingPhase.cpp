/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "DFGPutLocalSinkingPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBlockMapInlines.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGPreciseLocalClobberize.h"
#include "DFGSSACalculator.h"
#include "DFGValidate.h"
#include "JSCInlines.h"
#include "OperandsInlines.h"

namespace JSC { namespace DFG {

namespace {

bool verbose = false;

class VariableDeferral {
public:
    VariableDeferral(VariableAccessData* variable = nullptr)
        : m_variable(variable)
    {
    }
    
    static VariableDeferral conflict()
    {
        return VariableDeferral(conflictMarker());
    }
    
    bool operator!() const { return !m_variable; }
    
    bool hasVariable() const { return !!*this && !isConflict(); }
    
    VariableAccessData* variable() const
    {
        ASSERT(hasVariable());
        return m_variable;
    }
    
    bool isConflict() const
    {
        return m_variable == conflictMarker();
    }
    
    VariableDeferral merge(VariableDeferral other) const
    {
        if (*this == other || !other)
            return *this;
        if (!*this)
            return other;
        return conflict();
    }
    
    bool operator==(VariableDeferral other) const
    {
        return m_variable == other.m_variable;
    }
    
    void dump(PrintStream& out) const
    {
        if (!*this)
            out.print("-");
        else if (isConflict())
            out.print("Conflict");
        else
            out.print(RawPointer(m_variable));
    }
    
    void dumpInContext(PrintStream& out, DumpContext*) const
    {
        dump(out);
    }
    
private:
    static VariableAccessData* conflictMarker()
    {
        return bitwise_cast<VariableAccessData*>(static_cast<intptr_t>(1));
    }
    
    VariableAccessData* m_variable;
};

class PutLocalSinkingPhase : public Phase {
public:
    PutLocalSinkingPhase(Graph& graph)
        : Phase(graph, "PutLocal sinking")
    {
    }
    
    bool run()
    {
        // FIXME: One of the problems of this approach is that it will create a duplicate Phi graph
        // for sunken PutLocals in the presence of interesting control flow merges, and where the
        // value being PutLocal'd is also otherwise live in the DFG code. We could work around this
        // by doing the sinking over CPS, or maybe just by doing really smart hoisting. It's also
        // possible that the duplicate Phi graph can be deduplicated by LLVM. It would be best if we
        // could observe that there is already a Phi graph in place that does what we want. In
        // principle if we have a request to place a Phi at a particular place, we could just check
        // if there is already a Phi that does what we want. Because PutLocalSinkingPhase runs just
        // after SSA conversion, we have almost a guarantee that the Phi graph we produce here would
        // be trivially redundant to the one we already have.
        
        if (verbose) {
            dataLog("Graph before PutLocal sinking:\n");
            m_graph.dump();
        }
        
        SSACalculator ssaCalculator(m_graph);
        InsertionSet insertionSet(m_graph);
        
        // First figure out where various locals are live.
        BlockMap<Operands<bool>> liveAtHead(m_graph);
        BlockMap<Operands<bool>> liveAtTail(m_graph);
        
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            liveAtHead[block] = Operands<bool>(OperandsLike, block->variablesAtHead);
            liveAtTail[block] = Operands<bool>(OperandsLike, block->variablesAtHead);
            
            liveAtHead[block].fill(false);
            liveAtTail[block].fill(false);
        }
        
        bool changed;
        do {
            changed = false;
            
            for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
                BasicBlock* block = m_graph.block(blockIndex);
                if (!block)
                    continue;
                
                Operands<bool> live = liveAtTail[block];
                for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                    Node* node = block->at(nodeIndex);
                    if (verbose)
                        dataLog("Live at ", node, ": ", live, "\n");
                    
                    auto escapeHandler = [&] (VirtualRegister operand) {
                        if (operand.isHeader())
                            return;
                        if (verbose)
                            dataLog("    ", operand, " is live at ", node, "\n");
                        live.operand(operand) = true;
                    };
                    
                    preciseLocalClobberize(
                        m_graph, node, escapeHandler, escapeHandler,
                        [&] (VirtualRegister operand, Node* source) {
                            if (source == node) {
                                // This is a load. Ignore it.
                                return;
                            }
                            
                            RELEASE_ASSERT(node->op() == PutLocal);
                            live.operand(operand) = false;
                        });
                }
                
                if (live == liveAtHead[block])
                    continue;
                
                liveAtHead[block] = live;
                changed = true;
                
                for (BasicBlock* predecessor : block->predecessors) {
                    for (size_t i = live.size(); i--;)
                        liveAtTail[predecessor][i] |= live[i];
                }
            }
            
        } while (changed);
        
        // All of the arguments should be live at head of root. Note that we may find that some
        // locals are live at head of root. This seems wrong but isn't. This will happen for example
        // if the function accesses closure variable #42 for some other function and we either don't
        // have variable #42 at all or we haven't set it at root, for whatever reason. Basically this
        // arises since our aliasing for closure variables is conservatively based on variable number
        // and ignores the owning symbol table. We should probably fix this eventually and make our
        // aliasing more precise.
        //
        // For our purposes here, the imprecision in the aliasing is harmless. It just means that we
        // may not do as much Phi pruning as we wanted.
        for (size_t i = liveAtHead.atIndex(0).numberOfArguments(); i--;)
            DFG_ASSERT(m_graph, nullptr, liveAtHead.atIndex(0).argument(i));
        
        // Next identify where we would want to sink PutLocals to. We say that there is a deferred
        // flush if we had a PutLocal with a given VariableAccessData* but it hasn't been
        // materialized yet. Deferrals have the following lattice; but it's worth noting that the
        // TOP part of the lattice serves an entirely different purpose than the rest of the lattice:
        // it just means that we're in a region of code where nobody should have been relying on the
        // value. The rest of the lattice means that we either have a PutLocal that is deferred (i.e.
        // still needs to be executed) or there isn't one (because we've alraedy executed it).
        //
        // Bottom:
        //     Instantiated as VariableDeferral(). 
        //     Means that all previous PutLocals have been executed so there is nothing deferred.
        //     During merging this is subordinate to the other kinds of deferrals, because it
        //     represents the fact that we've already executed all necessary PutLocals. This implies
        //     that there *had* been some PutLocals that we should have executed.
        //
        // Top:
        //     Instantiated as VariableDeferral::conflict().
        //     Represents the fact that we know, via forward flow, that there isn't any value in the
        //     given local that anyone should have been relying on. This comes into play at the
        //     prologue (because in SSA form at the prologue no local has any value) or when we merge
        //     deferrals for different VariableAccessData*'s. A VAD encompasses a lexical scope in
        //     which the local has some semantic meaning; if we had stores from different lexical
        //     scopes that got merged together then we know that we're not in either scope anymore.
        //     Note that this is all approximate and only precise enough to later answer questions
        //     pertinent to sinking. For example, this doesn't always detect when a local is no
        //     longer semantically relevant - we may well have a deferral from inside some inlined
        //     call survive outside of that inlined code, and this is generally OK. In the worst case
        //     it means that we might think that a deferral that is actually dead must still be
        //     executed. But we usually catch that with liveness. Liveness doesn't always catch it
        //     because liveness is conservative.
        //
        //     What Top does give us is detects situations where we both don't need to care about a
        //     deferral and there is no way that we could reason about it anyway. If we merged
        //     deferrals for different variables then we wouldn't know the format to use. So, we
        //     use Top in that case because that's also a case where we know that we can ignore the
        //     deferral.
        //
        // Deferral with a concrete VariableAccessData*:
        //     Instantiated as VariableDeferral(someVariableAccessData)
        //     Represents the fact that the original code would have done a PutLocal but we haven't
        //     identified an operation that would have observed that PutLocal.
        //
        // This code has some interesting quirks because of the fact that neither liveness nor
        // deferrals are very precise. They are only precise enough to be able to correctly tell us
        // when we may [sic] need to execute PutLocals. This means that they may report the need to
        // execute a PutLocal in cases where we actually don't really need it, and that's totally OK.
        BlockMap<Operands<VariableDeferral>> deferredAtHead(m_graph);
        BlockMap<Operands<VariableDeferral>> deferredAtTail(m_graph);
        
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            deferredAtHead[block] =
                Operands<VariableDeferral>(OperandsLike, block->variablesAtHead);
            deferredAtTail[block] =
                Operands<VariableDeferral>(OperandsLike, block->variablesAtHead);
        }
        
        deferredAtHead.atIndex(0).fill(VariableDeferral::conflict());
        
        do {
            changed = false;
            
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                Operands<VariableDeferral> deferred = deferredAtHead[block];
                
                for (Node* node : *block) {
                    if (verbose)
                        dataLog("Deferred at ", node, ":", deferred, "\n");
                    
                    if (node->op() == KillLocal) {
                        deferred.operand(node->unlinkedLocal()) = VariableDeferral::conflict();
                        continue;
                    }
                    
                    auto escapeHandler = [&] (VirtualRegister operand) {
                        if (operand.isHeader())
                            return;
                        // We will materialize just before any reads.
                        deferred.operand(operand) = VariableDeferral();
                    };
                    
                    preciseLocalClobberize(
                        m_graph, node, escapeHandler, escapeHandler,
                        [&] (VirtualRegister operand, Node* source) {
                            if (source == node) {
                                // This is a load. Ignore it.
                                return;
                            }
                            
                            deferred.operand(operand) = VariableDeferral(node->variableAccessData());
                        });
                }
                
                if (deferred == deferredAtTail[block])
                    continue;
                
                deferredAtTail[block] = deferred;
                changed = true;
                
                for (BasicBlock* successor : block->successors()) {
                    for (size_t i = deferred.size(); i--;) {
                        if (verbose)
                            dataLog("Considering ", VirtualRegister(deferred.operandForIndex(i)), " at ", pointerDump(block), "->", pointerDump(successor), ": ", deferred[i], " and ", deferredAtHead[successor][i], " merges to ");

                        deferredAtHead[successor][i] =
                            deferredAtHead[successor][i].merge(deferred[i]);
                        
                        if (verbose)
                            dataLog(deferredAtHead[successor][i], "\n");
                    }
                }
            }
            
        } while (changed);
        
        // We wish to insert PutLocals at all of the materialization points, which are defined
        // implicitly as the places where we set deferred to Dead while it was previously not Dead.
        // To do this, we may need to build some Phi functions to handle stuff like this:
        //
        // Before:
        //
        //     if (p)
        //         PutLocal(r42, @x)
        //     else
        //         PutLocal(r42, @y)
        //
        // After:
        //
        //     if (p)
        //         Upsilon(@x, ^z)
        //     else
        //         Upsilon(@y, ^z)
        //     z: Phi()
        //     PutLocal(r42, @z)
        //
        // This means that we have an SSACalculator::Variable for each local, and a Def is any
        // PutLocal in the original program. The original PutLocals will simply vanish.
        
        Operands<SSACalculator::Variable*> operandToVariable(
            OperandsLike, m_graph.block(0)->variablesAtHead);
        Vector<VirtualRegister> indexToOperand;
        for (size_t i = m_graph.block(0)->variablesAtHead.size(); i--;) {
            VirtualRegister operand(m_graph.block(0)->variablesAtHead.operandForIndex(i));
            
            SSACalculator::Variable* variable = ssaCalculator.newVariable();
            operandToVariable.operand(operand) = variable;
            ASSERT(indexToOperand.size() == variable->index());
            indexToOperand.append(operand);
        }
        
        HashSet<Node*> putLocalsToSink;
        
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                switch (node->op()) {
                case PutLocal:
                    putLocalsToSink.add(node);
                    ssaCalculator.newDef(
                        operandToVariable.operand(node->local()), block, node->child1().node());
                    break;
                case GetLocal:
                    ssaCalculator.newDef(
                        operandToVariable.operand(node->local()), block, node);
                    break;
                default:
                    break;
                }
            }
        }
        
        ssaCalculator.computePhis(
            [&] (SSACalculator::Variable* variable, BasicBlock* block) -> Node* {
                VirtualRegister operand = indexToOperand[variable->index()];
                
                if (!liveAtHead[block].operand(operand))
                    return nullptr;
                
                VariableDeferral variableDeferral = deferredAtHead[block].operand(operand);

                // We could have an invalid deferral because liveness is imprecise.
                if (!variableDeferral.hasVariable())
                    return nullptr;

                if (verbose)
                    dataLog("Adding Phi for ", operand, " at ", pointerDump(block), "\n");
                
                Node* phiNode = m_graph.addNode(SpecHeapTop, Phi, NodeOrigin());
                DFG_ASSERT(m_graph, nullptr, variableDeferral.hasVariable());
                FlushFormat format = variableDeferral.variable()->flushFormat();
                phiNode->mergeFlags(resultFor(format));
                return phiNode;
            });
        
        Operands<Node*> mapping(OperandsLike, m_graph.block(0)->variablesAtHead);
        Operands<VariableDeferral> deferred;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            mapping.fill(nullptr);
            
            for (size_t i = mapping.size(); i--;) {
                VirtualRegister operand(mapping.operandForIndex(i));
                
                SSACalculator::Variable* variable = operandToVariable.operand(operand);
                SSACalculator::Def* def = ssaCalculator.reachingDefAtHead(block, variable);
                if (!def)
                    continue;
                
                mapping.operand(operand) = def->value();
            }
            
            if (verbose)
                dataLog("Mapping at top of ", pointerDump(block), ": ", mapping, "\n");
            
            for (SSACalculator::Def* phiDef : ssaCalculator.phisForBlock(block)) {
                VirtualRegister operand = indexToOperand[phiDef->variable()->index()];
                
                insertionSet.insert(0, phiDef->value());
                
                if (verbose)
                    dataLog("   Mapping ", operand, " to ", phiDef->value(), "\n");
                mapping.operand(operand) = phiDef->value();
            }
            
            deferred = deferredAtHead[block];
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                if (verbose)
                    dataLog("Deferred at ", node, ":", deferred, "\n");
                
                switch (node->op()) {
                case PutLocal: {
                    VariableAccessData* variable = node->variableAccessData();
                    VirtualRegister operand = variable->local();
                    deferred.operand(operand) = VariableDeferral(variable);
                    if (verbose)
                        dataLog("   Mapping ", operand, " to ", node->child1().node(), " at ", node, "\n");
                    mapping.operand(operand) = node->child1().node();
                    break;
                }
                    
                case KillLocal: {
                    deferred.operand(node->unlinkedLocal()) = VariableDeferral();
                    break;
                }
                
                default: {
                    auto escapeHandler = [&] (VirtualRegister operand) {
                        if (operand.isHeader())
                            return;
                    
                        VariableDeferral variableDeferral = deferred.operand(operand);
                        if (!variableDeferral.hasVariable())
                            return;
                    
                        // Gotta insert a PutLocal.
                        if (verbose)
                            dataLog("Inserting a PutLocal for ", operand, " at ", node, "\n");

                        Node* incoming = mapping.operand(operand);
                        DFG_ASSERT(m_graph, node, incoming);
                    
                        insertionSet.insertNode(
                            nodeIndex, SpecNone, PutLocal, node->origin,
                            OpInfo(variableDeferral.variable()),
                            Edge(incoming, useKindFor(variableDeferral.variable()->flushFormat())));
                    
                        deferred.operand(operand) = nullptr;
                    };
                
                    preciseLocalClobberize(
                        m_graph, node, escapeHandler, escapeHandler,
                        [&] (VirtualRegister, Node*) { });
                    
                    // If we're a GetLocal, then we also create a mapping.
                    // FIXME: We should be able to just eliminate such GetLocals, when we know
                    // what their incoming value will be.
                    // https://bugs.webkit.org/show_bug.cgi?id=141624
                    if (node->op() == GetLocal) {
                        VariableAccessData* variable = node->variableAccessData();
                        VirtualRegister operand = variable->local();
                        mapping.operand(operand) = node;
                    }
                    break;
                } }
            }
            
            size_t upsilonInsertionPoint = block->size() - 1;
            NodeOrigin upsilonOrigin = block->last()->origin;
            for (BasicBlock* successorBlock : block->successors()) {
                for (SSACalculator::Def* phiDef : ssaCalculator.phisForBlock(successorBlock)) {
                    Node* phiNode = phiDef->value();
                    SSACalculator::Variable* variable = phiDef->variable();
                    VirtualRegister operand = indexToOperand[variable->index()];
                    if (verbose)
                        dataLog("Creating Upsilon for ", operand, " at ", pointerDump(block), "->", pointerDump(successorBlock), "\n");
                    VariableDeferral variableDeferral =
                        deferredAtHead[successorBlock].operand(operand);
                    DFG_ASSERT(m_graph, nullptr, variableDeferral.hasVariable());
                    FlushFormat format = variableDeferral.variable()->flushFormat();
                    UseKind useKind = useKindFor(format);
                    Node* incoming = mapping.operand(operand);
                    if (!incoming) {
                        // This can totally happen, see tests/stress/put-local-conservative.js.
                        // This arises because deferral and liveness are both conservative.
                        // Conservative liveness means that a load from a *different* closure
                        // variable may lead us to believe that our local is live. Conservative
                        // deferral may lead us to believe that the local doesn't have a top deferral
                        // because someone has done something that would have forced it to be
                        // materialized. The basic pattern is:
                        //
                        // GetClosureVar(loc42) // loc42's deferral is now bottom
                        // if (predicate1)
                        //     PutClosureVar(loc42) // prevent GCSE of our GetClosureVar's
                        // if (predicate2)
                        //     PutLocal(loc42) // we now have a concrete deferral
                        // // we still have the concrete deferral because we merged with bottom
                        // GetClosureVar(loc42) // force materialization
                        //
                        // We will have a Phi with no incoming value form the basic block that
                        // bypassed the PutLocal.
                        
                        // Note: we sort of could have used the equivalent of LLVM's undef here. The
                        // point is that it's OK to just leave random bits in the local if we're
                        // coming down this path. But, we don't have a way of saying that in our IR
                        // right now and anyway it probably doesn't matter that much.
                        
                        incoming = insertionSet.insertBottomConstantForUse(
                            upsilonInsertionPoint, upsilonOrigin, useKind).node();
                    }
                    
                    insertionSet.insertNode(
                        upsilonInsertionPoint, SpecNone, Upsilon, upsilonOrigin,
                        OpInfo(phiNode), Edge(incoming, useKind));
                }
            }
            
            insertionSet.execute(block);
        }
        
        // Finally eliminate the sunken PutLocals by turning them into Phantoms. This keeps whatever
        // type check they were doing. Also prepend KillLocals to them to ensure that we know that
        // the relevant value was *not* stored to the stack.
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                
                if (!putLocalsToSink.contains(node))
                    continue;
                
                insertionSet.insertNode(
                    nodeIndex, SpecNone, KillLocal, node->origin, OpInfo(node->local().offset()));
                node->convertToPhantom();
            }
            
            insertionSet.execute(block);
        }
        
        if (verbose) {
            dataLog("Graph after PutLocal sinking:\n");
            m_graph.dump();
        }
        
        return true;
    }
};

} // anonymous namespace
    
bool performPutLocalSinking(Graph& graph)
{
    SamplingRegion samplingRegion("DFG PutLocal Sinking Phase");
    return runPhase<PutLocalSinkingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

