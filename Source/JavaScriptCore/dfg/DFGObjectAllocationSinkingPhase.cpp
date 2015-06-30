/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
#include "DFGObjectAllocationSinkingPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGAbstractHeap.h"
#include "DFGBlockMapInlines.h"
#include "DFGClobberize.h"
#include "DFGCombinedLiveness.h"
#include "DFGGraph.h"
#include "DFGInsertOSRHintsForUpdate.h"
#include "DFGInsertionSet.h"
#include "DFGLivenessAnalysisPhase.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "DFGPhase.h"
#include "DFGPromoteHeapAccess.h"
#include "DFGSSACalculator.h"
#include "DFGValidate.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

static bool verbose = false;

class ObjectAllocationSinkingPhase : public Phase {
public:
    ObjectAllocationSinkingPhase(Graph& graph)
        : Phase(graph, "object allocation sinking")
        , m_ssaCalculator(graph)
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        ASSERT(m_graph.m_fixpointState == FixpointNotConverged);
        
        m_graph.m_dominators.computeIfNecessary(m_graph);
        
        // Logically we wish to consider every NewObject and sink it. However it's probably not
        // profitable to sink a NewObject that will always escape. So, first we do a very simple
        // forward flow analysis that determines the set of NewObject nodes that have any chance
        // of benefiting from object allocation sinking. Then we fixpoint the following rules:
        //
        // - For each NewObject, we turn the original NewObject into a PhantomNewObject and then
        //   we insert MaterializeNewObject just before those escaping sites that come before any
        //   other escaping sites - that is, there is no path between the allocation and those sites
        //   that would see any other escape. Note that Upsilons constitute escaping sites. Then we
        //   insert additional MaterializeNewObject nodes on Upsilons that feed into Phis that mix
        //   materializations and the original PhantomNewObject. We then turn each PutByOffset over a
        //   PhantomNewObject into a PutHint.
        //
        // - We perform the same optimization for MaterializeNewObject. This allows us to cover
        //   cases where we had MaterializeNewObject flowing into a PutHint.
        //
        // We could also add this rule:
        //
        // - If all of the Upsilons of a Phi have a MaterializeNewObject that isn't used by anyone
        //   else, then replace the Phi with the MaterializeNewObject.
        //
        //   FIXME: Implement this. Note that this totally doable but it requires some gnarly
        //   code, and to be effective the pruner needs to be aware of it. Currently any Upsilon
        //   is considered to be an escape even by the pruner, so it's unlikely that we'll see
        //   many cases of Phi over Materializations.
        //   https://bugs.webkit.org/show_bug.cgi?id=136927
        
        if (!performSinking())
            return false;
        
        while (performSinking()) { }
        
        if (verbose) {
            dataLog("Graph after sinking:\n");
            m_graph.dump();
        }
        
        return true;
    }

private:
    bool performSinking()
    {
        m_graph.computeRefCounts();
        performLivenessAnalysis(m_graph);
        performOSRAvailabilityAnalysis(m_graph);
        m_combinedLiveness = CombinedLiveness(m_graph);
        
        CString graphBeforeSinking;
        if (Options::verboseValidationFailure() && Options::validateGraphAtEachPhase()) {
            StringPrintStream out;
            m_graph.dump(out);
            graphBeforeSinking = out.toCString();
        }
        
        if (verbose) {
            dataLog("Graph before sinking:\n");
            m_graph.dump();
        }
        
        determineMaterializationPoints();
        if (m_sinkCandidates.isEmpty())
            return false;
        
        // At this point we are committed to sinking the sinking candidates.
        placeMaterializationPoints();
        lowerNonReadingOperationsOnPhantomAllocations();
        promoteSunkenFields();
        
        if (Options::validateGraphAtEachPhase())
            validate(m_graph, DumpGraph, graphBeforeSinking);
        
        if (verbose)
            dataLog("Sinking iteration changed the graph.\n");
        return true;
    }
    
    void determineMaterializationPoints()
    {
        // The premise of this pass is that if there exists a point in the program where some
        // path from a phantom allocation site to that point causes materialization, then *all*
        // paths cause materialization. This should mean that there are never any redundant
        // materializations.
        
        m_sinkCandidates.clear();
        m_materializationToEscapee.clear();
        m_materializationSiteToMaterializations.clear();
        
        BlockMap<HashMap<Node*, bool>> materializedAtHead(m_graph);
        BlockMap<HashMap<Node*, bool>> materializedAtTail(m_graph);
        
        bool changed;
        do {
            if (verbose)
                dataLog("Doing iteration of materialization point placement.\n");
            changed = false;
            for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
                HashMap<Node*, bool> materialized = materializedAtHead[block];
                if (verbose)
                    dataLog("    Looking at block ", pointerDump(block), "\n");
                for (Node* node : *block) {
                    handleNode(
                        node,
                        [&] () {
                            materialized.add(node, false);
                        },
                        [&] (Node* escapee) {
                            auto iter = materialized.find(escapee);
                            if (iter != materialized.end()) {
                                if (verbose)
                                    dataLog("    ", escapee, " escapes at ", node, "\n");
                                iter->value = true;
                            }
                        });
                }
                
                if (verbose)
                    dataLog("    Materialized at tail of ", pointerDump(block), ": ", mapDump(materialized), "\n");
                
                if (materialized == materializedAtTail[block])
                    continue;
                
                materializedAtTail[block] = materialized;
                changed = true;
                
                // Only propagate things to our successors if they are alive in all successors.
                // So, we prune materialized-at-tail to only include things that are live.
                Vector<Node*> toRemove;
                for (auto pair : materialized) {
                    if (!m_combinedLiveness.liveAtTail[block].contains(pair.key))
                        toRemove.append(pair.key);
                }
                for (Node* key : toRemove)
                    materialized.remove(key);
                
                for (BasicBlock* successorBlock : block->successors()) {
                    for (auto pair : materialized) {
                        materializedAtHead[successorBlock].add(
                            pair.key, false).iterator->value |= pair.value;
                    }
                }
            }
        } while (changed);
        
        // Determine the sink candidates. Broadly, a sink candidate is a node that handleNode()
        // believes is sinkable, and one of the following is true:
        //
        // 1) There exists a basic block with only backward outgoing edges (or no outgoing edges)
        //    in which the node wasn't materialized. This is meant to catch effectively-infinite
        //    loops in which we don't need to have allocated the object.
        //
        // 2) There exists a basic block at the tail of which the node is not materialized and the
        //    node is dead.
        //
        // 3) The sum of execution counts of the materializations is less than the sum of
        //    execution counts of the original node.
        //
        // We currently implement only rule #2.
        // FIXME: Implement the two other rules.
        // https://bugs.webkit.org/show_bug.cgi?id=137073 (rule #1)
        // https://bugs.webkit.org/show_bug.cgi?id=137074 (rule #3)
        
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (auto pair : materializedAtTail[block]) {
                if (pair.value)
                    continue; // It was materialized.
                
                if (m_combinedLiveness.liveAtTail[block].contains(pair.key))
                    continue; // It might still get materialized in all of the successors.
                
                // We know that it died in this block and it wasn't materialized. That means that
                // if we sink this allocation, then *this* will be a path along which we never
                // have to allocate. Profit!
                m_sinkCandidates.add(pair.key);
            }
        }
        
        if (m_sinkCandidates.isEmpty())
            return;
        
        // A materialization edge exists at any point where a node escapes but hasn't been
        // materialized yet. We do this in two parts. First we find all of the nodes that cause
        // escaping to happen, where the escapee had not yet been materialized. This catches
        // everything but loops. We then catch loops - as well as weirder control flow constructs -
        // in a subsequent pass that looks at places in the CFG where an edge exists from a block
        // that hasn't materialized to a block that has. We insert the materialization along such an
        // edge, and we rely on the fact that critical edges were already broken so that we actually
        // either insert the materialization at the head of the successor or the tail of the
        // predecessor.
        //
        // FIXME: This can create duplicate allocations when we really only needed to perform one.
        // For example:
        //
        //     var o = new Object();
        //     if (rare) {
        //         if (stuff)
        //             call(o); // o escapes here.
        //         return;
        //     }
        //     // o doesn't escape down here.
        //
        // In this example, we would place a materialization point at call(o) and then we would find
        // ourselves having to insert another one at the implicit else case of that if statement
        // ('cause we've broken critical edges). We would instead really like to just have one
        // materialization point right at the top of the then case of "if (rare)". To do this, we can
        // find the LCA of the various materializations in the dom tree.
        // https://bugs.webkit.org/show_bug.cgi?id=137124
        
        // First pass: find intra-block materialization points.
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            HashSet<Node*> materialized;
            for (auto pair : materializedAtHead[block]) {
                if (pair.value && m_sinkCandidates.contains(pair.key))
                    materialized.add(pair.key);
            }
            
            if (verbose)
                dataLog("Placing materialization points in ", pointerDump(block), " with materialization set ", listDump(materialized), "\n");
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                
                handleNode(
                    node,
                    [&] () { },
                    [&] (Node* escapee) {
                        if (verbose)
                            dataLog("Seeing ", escapee, " escape at ", node, "\n");
                        
                        if (!m_sinkCandidates.contains(escapee)) {
                            if (verbose)
                                dataLog("    It's not a sink candidate.\n");
                            return;
                        }
                        
                        if (!materialized.add(escapee).isNewEntry) {
                            if (verbose)
                                dataLog("   It's already materialized.\n");
                            return;
                        }
                        
                        createMaterialize(escapee, node);
                    });
            }
        }
        
        // Second pass: find CFG edge materialization points.
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (BasicBlock* successorBlock : block->successors()) {
                for (auto pair : materializedAtHead[successorBlock]) {
                    Node* allocation = pair.key;
                    
                    // We only care if it is materialized in the successor.
                    if (!pair.value)
                        continue;
                    
                    // We only care about sinking candidates.
                    if (!m_sinkCandidates.contains(allocation))
                        continue;
                    
                    // We only care if it isn't materialized in the predecessor.
                    if (materializedAtTail[block].get(allocation))
                        continue;
                    
                    // We need to decide if we put the materialization at the head of the successor,
                    // or the tail of the predecessor. It needs to be placed so that the allocation
                    // is never materialized before, and always materialized after.
                    
                    // Is it never materialized in any of successor's predecessors? I like to think
                    // of "successors' predecessors" and "predecessor's successors" as the "shadow",
                    // because of what it looks like when you draw it.
                    bool neverMaterializedInShadow = true;
                    for (BasicBlock* shadowBlock : successorBlock->predecessors) {
                        if (materializedAtTail[shadowBlock].get(allocation)) {
                            neverMaterializedInShadow = false;
                            break;
                        }
                    }
                    
                    if (neverMaterializedInShadow) {
                        createMaterialize(allocation, successorBlock->firstOriginNode());
                        continue;
                    }
                    
                    // Because we had broken critical edges, it must be the case that the
                    // predecessor's successors all materialize the object. This is because the
                    // previous case is guaranteed to catch the case where the successor only has
                    // one predecessor. When critical edges are broken, this is also the only case
                    // where the predecessor could have had multiple successors. Therefore we have
                    // already handled the case where the predecessor has multiple successors.
                    DFG_ASSERT(m_graph, block, block->numSuccessors() == 1);
                    
                    createMaterialize(allocation, block->terminal());
                }
            }
        }
    }
    
    void placeMaterializationPoints()
    {
        m_ssaCalculator.reset();
        
        // The "variables" are the object allocations that we are sinking. So, nodeToVariable maps
        // sink candidates (aka escapees) to the SSACalculator's notion of Variable, and indexToNode
        // maps in the opposite direction using the SSACalculator::Variable::index() as the key.
        HashMap<Node*, SSACalculator::Variable*> nodeToVariable;
        Vector<Node*> indexToNode;
        
        for (Node* node : m_sinkCandidates) {
            SSACalculator::Variable* variable = m_ssaCalculator.newVariable();
            nodeToVariable.add(node, variable);
            ASSERT(indexToNode.size() == variable->index());
            indexToNode.append(node);
        }
        
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                if (SSACalculator::Variable* variable = nodeToVariable.get(node))
                    m_ssaCalculator.newDef(variable, block, node);
                
                for (Node* materialize : m_materializationSiteToMaterializations.get(node)) {
                    m_ssaCalculator.newDef(
                        nodeToVariable.get(m_materializationToEscapee.get(materialize)),
                        block, materialize);
                }
            }
        }
        
        m_ssaCalculator.computePhis(
            [&] (SSACalculator::Variable* variable, BasicBlock* block) -> Node* {
                Node* allocation = indexToNode[variable->index()];
                if (!m_combinedLiveness.liveAtHead[block].contains(allocation))
                    return nullptr;
                
                Node* phiNode = m_graph.addNode(allocation->prediction(), Phi, NodeOrigin());
                phiNode->mergeFlags(NodeResultJS);
                return phiNode;
            });
        
        // Place Phis in the right places. Replace all uses of any allocation with the appropriate
        // materialization. Create the appropriate Upsilon nodes.
        LocalOSRAvailabilityCalculator availabilityCalculator;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            HashMap<Node*, Node*> mapping;
            
            for (Node* candidate : m_combinedLiveness.liveAtHead[block]) {
                SSACalculator::Variable* variable = nodeToVariable.get(candidate);
                if (!variable)
                    continue;
                
                SSACalculator::Def* def = m_ssaCalculator.reachingDefAtHead(block, variable);
                if (!def)
                    continue;
                
                mapping.set(indexToNode[variable->index()], def->value());
            }
            
            availabilityCalculator.beginBlock(block);
            for (SSACalculator::Def* phiDef : m_ssaCalculator.phisForBlock(block)) {
                m_insertionSet.insert(0, phiDef->value());
                
                Node* originalNode = indexToNode[phiDef->variable()->index()];
                insertOSRHintsForUpdate(
                    m_insertionSet, 0, NodeOrigin(), availabilityCalculator.m_availability,
                    originalNode, phiDef->value());

                mapping.set(originalNode, phiDef->value());
            }
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);

                for (Node* materialize : m_materializationSiteToMaterializations.get(node)) {
                    Node* escapee = m_materializationToEscapee.get(materialize);
                    m_insertionSet.insert(nodeIndex, materialize);
                    insertOSRHintsForUpdate(
                        m_insertionSet, nodeIndex, node->origin,
                        availabilityCalculator.m_availability, escapee, materialize);
                    mapping.set(escapee, materialize);
                }
                    
                availabilityCalculator.executeNode(node);
                
                m_graph.doToChildren(
                    node,
                    [&] (Edge& edge) {
                        if (Node* materialize = mapping.get(edge.node()))
                            edge.setNode(materialize);
                    });
                
                // If you cause an escape, you shouldn't see the original escapee.
                if (validationEnabled()) {
                    handleNode(
                        node,
                        [&] () { },
                        [&] (Node* escapee) {
                            DFG_ASSERT(m_graph, node, !m_sinkCandidates.contains(escapee));
                        });
                }
            }
            
            NodeAndIndex terminal = block->findTerminal();
            size_t upsilonInsertionPoint = terminal.index;
            Node* upsilonWhere = terminal.node;
            NodeOrigin upsilonOrigin = upsilonWhere->origin;
            for (BasicBlock* successorBlock : block->successors()) {
                for (SSACalculator::Def* phiDef : m_ssaCalculator.phisForBlock(successorBlock)) {
                    Node* phiNode = phiDef->value();
                    SSACalculator::Variable* variable = phiDef->variable();
                    Node* allocation = indexToNode[variable->index()];
                    
                    Node* incoming = mapping.get(allocation);
                    DFG_ASSERT(m_graph, incoming, incoming != allocation);
                    
                    m_insertionSet.insertNode(
                        upsilonInsertionPoint, SpecNone, Upsilon, upsilonOrigin,
                        OpInfo(phiNode), incoming->defaultEdge());
                }
            }
            
            m_insertionSet.execute(block);
        }
        
        // At this point we have dummy materialization nodes along with edges to them. This means
        // that the part of the control flow graph that prefers to see actual object allocations
        // is completely fixed up, except for the materializations themselves.
    }
    
    void lowerNonReadingOperationsOnPhantomAllocations()
    {
        // Lower everything but reading operations on phantom allocations. We absolutely have to
        // lower all writes so as to reveal them to the SSA calculator. We cannot lower reads
        // because the whole point is that those go away completely.
        
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                switch (node->op()) {
                case PutByOffset: {
                    Node* target = node->child2().node();
                    if (m_sinkCandidates.contains(target)) {
                        ASSERT(target->isPhantomObjectAllocation());
                        node->convertToPutByOffsetHint();
                    }
                    break;
                }

                case PutClosureVar: {
                    Node* target = node->child1().node();
                    if (m_sinkCandidates.contains(target)) {
                        ASSERT(target->isPhantomActivationAllocation());
                        node->convertToPutClosureVarHint();
                    }
                    break;
                }
                    
                case PutStructure: {
                    Node* target = node->child1().node();
                    if (m_sinkCandidates.contains(target)) {
                        ASSERT(target->isPhantomObjectAllocation());
                        Node* structure = m_insertionSet.insertConstant(
                            nodeIndex, node->origin, JSValue(node->transition()->next));
                        node->convertToPutStructureHint(structure);
                    }
                    break;
                }
                    
                case NewObject: {
                    if (m_sinkCandidates.contains(node)) {
                        Node* structure = m_insertionSet.insertConstant(
                            nodeIndex + 1, node->origin, JSValue(node->structure()));
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            PromotedHeapLocation(StructurePLoc, node).createHint(
                                m_graph, node->origin, structure));
                        node->convertToPhantomNewObject();
                    }
                    break;
                }
                    
                case MaterializeNewObject: {
                    if (m_sinkCandidates.contains(node)) {
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            PromotedHeapLocation(StructurePLoc, node).createHint(
                                m_graph, node->origin, m_graph.varArgChild(node, 0).node()));
                        for (unsigned i = 0; i < node->objectMaterializationData().m_properties.size(); ++i) {
                            unsigned identifierNumber =
                                node->objectMaterializationData().m_properties[i].m_identifierNumber;
                            m_insertionSet.insert(
                                nodeIndex + 1,
                                PromotedHeapLocation(
                                    NamedPropertyPLoc, node, identifierNumber).createHint(
                                    m_graph, node->origin,
                                    m_graph.varArgChild(node, i + 1).node()));
                        }
                        node->convertToPhantomNewObject();
                    }
                    break;
                }

                case NewFunction: {
                    if (m_sinkCandidates.contains(node)) {
                        Node* executable = m_insertionSet.insertConstant(
                            nodeIndex + 1, node->origin, node->cellOperand());
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            PromotedHeapLocation(FunctionExecutablePLoc, node).createHint(
                                m_graph, node->origin, executable));
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            PromotedHeapLocation(FunctionActivationPLoc, node).createHint(
                                m_graph, node->origin, node->child1().node()));
                        node->convertToPhantomNewFunction();
                    }
                    break;
                }

                case CreateActivation: {
                    if (m_sinkCandidates.contains(node)) {
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            PromotedHeapLocation(ActivationScopePLoc, node).createHint(
                                m_graph, node->origin, node->child1().node()));
                        Node* symbolTableNode = m_insertionSet.insertConstant(
                            nodeIndex + 1, node->origin, node->cellOperand());
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            PromotedHeapLocation(ActivationSymbolTablePLoc, node).createHint(
                                m_graph, node->origin, symbolTableNode));

                        {
                            SymbolTable* symbolTable = node->castOperand<SymbolTable*>();
                            Node* undefined = m_insertionSet.insertConstant(
                                nodeIndex + 1, node->origin, jsUndefined());
                            ConcurrentJITLocker locker(symbolTable->m_lock);
                            for (auto iter = symbolTable->begin(locker), end = symbolTable->end(locker); iter != end; ++iter) {
                                m_insertionSet.insert(
                                    nodeIndex + 1,
                                    PromotedHeapLocation(
                                        ClosureVarPLoc, node, iter->value.scopeOffset().offset()).createHint(
                                        m_graph, node->origin, undefined));
                            }
                        }

                        node->convertToPhantomCreateActivation();
                    }
                    break;
                }

                case MaterializeCreateActivation: {
                    if (m_sinkCandidates.contains(node)) {
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            PromotedHeapLocation(ActivationScopePLoc, node).createHint(
                                m_graph, node->origin, m_graph.varArgChild(node, 0).node()));
                        Node* symbolTableNode = m_insertionSet.insertConstant(
                            nodeIndex + 1, node->origin, node->cellOperand());
                        m_insertionSet.insert(
                            nodeIndex + 1,
                            PromotedHeapLocation(ActivationSymbolTablePLoc, node).createHint(
                                m_graph, node->origin, symbolTableNode));
                        ObjectMaterializationData& data = node->objectMaterializationData();
                        for (unsigned i = 0; i < data.m_properties.size(); ++i) {
                            unsigned identifierNumber = data.m_properties[i].m_identifierNumber;
                            m_insertionSet.insert(
                                nodeIndex + 1,
                                PromotedHeapLocation(
                                    ClosureVarPLoc, node, identifierNumber).createHint(
                                    m_graph, node->origin,
                                    m_graph.varArgChild(node, i + 1).node()));
                        }
                        node->convertToPhantomCreateActivation();
                    }
                    break;
                }

                case StoreBarrier: {
                    DFG_CRASH(m_graph, node, "Unexpected store barrier during sinking.");
                    break;
                }
                    
                default:
                    break;
                }
                
                m_graph.doToChildren(
                    node,
                    [&] (Edge& edge) {
                        if (m_sinkCandidates.contains(edge.node()))
                            edge.setUseKind(KnownCellUse);
                    });
            }
            m_insertionSet.execute(block);
        }
    }
    
    void promoteSunkenFields()
    {
        // Collect the set of heap locations that we will be operating over.
        HashSet<PromotedHeapLocation> locations;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                promoteHeapAccess(
                    node,
                    [&] (PromotedHeapLocation location, Edge) {
                        if (m_sinkCandidates.contains(location.base()))
                            locations.add(location);
                    },
                    [&] (PromotedHeapLocation location) {
                        if (m_sinkCandidates.contains(location.base()))
                            locations.add(location);
                    });
            }
        }
        
        // Figure out which locations belong to which allocations.
        m_locationsForAllocation.clear();
        for (PromotedHeapLocation location : locations) {
            auto result = m_locationsForAllocation.add(location.base(), Vector<PromotedHeapLocation>());
            ASSERT(!result.iterator->value.contains(location));
            result.iterator->value.append(location);
        }
        
        // For each sunken thingy, make sure we create Bottom values for all of its fields.
        // Note that this has the hilarious slight inefficiency of creating redundant hints for
        // things that were previously materializations. This should only impact compile times and
        // not code quality, and it's necessary for soundness without some data structure hackage.
        // For example, a MaterializeNewObject that we choose to sink may have new fields added to
        // it conditionally. That would necessitate Bottoms.
        Node* bottom = nullptr;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            if (block == m_graph.block(0))
                bottom = m_insertionSet.insertConstant(0, NodeOrigin(), jsUndefined());
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                for (PromotedHeapLocation location : m_locationsForAllocation.get(node)) {
                    m_insertionSet.insert(
                        nodeIndex + 1, location.createHint(m_graph, node->origin, bottom));
                }
            }
            m_insertionSet.execute(block);
        }

        m_ssaCalculator.reset();

        // Collect the set of "variables" that we will be sinking.
        m_locationToVariable.clear();
        m_indexToLocation.clear();
        for (PromotedHeapLocation location : locations) {
            SSACalculator::Variable* variable = m_ssaCalculator.newVariable();
            m_locationToVariable.add(location, variable);
            ASSERT(m_indexToLocation.size() == variable->index());
            m_indexToLocation.append(location);
        }
        
        // Create Defs from the existing hints.
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                promoteHeapAccess(
                    node,
                    [&] (PromotedHeapLocation location, Edge value) {
                        if (!m_sinkCandidates.contains(location.base()))
                            return;
                        SSACalculator::Variable* variable = m_locationToVariable.get(location);
                        m_ssaCalculator.newDef(variable, block, value.node());
                    },
                    [&] (PromotedHeapLocation) { });
            }
        }
        
        // OMG run the SSA calculator to create Phis!
        m_ssaCalculator.computePhis(
            [&] (SSACalculator::Variable* variable, BasicBlock* block) -> Node* {
                PromotedHeapLocation location = m_indexToLocation[variable->index()];
                if (!m_combinedLiveness.liveAtHead[block].contains(location.base()))
                    return nullptr;
                
                Node* phiNode = m_graph.addNode(SpecHeapTop, Phi, NodeOrigin());
                phiNode->mergeFlags(NodeResultJS);
                return phiNode;
            });
        
        // Place Phis in the right places, replace all uses of any load with the appropriate
        // value, and create the appropriate Upsilon nodes.
        m_graph.clearReplacements();
        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            // This mapping table is intended to be lazy. If something is omitted from the table,
            // it means that there haven't been any local stores to that promoted heap location.
            m_localMapping.clear();
            
            // Insert the Phi functions that we had previously created.
            for (SSACalculator::Def* phiDef : m_ssaCalculator.phisForBlock(block)) {
                PromotedHeapLocation location = m_indexToLocation[phiDef->variable()->index()];
                
                m_insertionSet.insert(
                    0, phiDef->value());
                m_insertionSet.insert(
                    0, location.createHint(m_graph, NodeOrigin(), phiDef->value()));
                m_localMapping.add(location, phiDef->value());
            }
            
            if (verbose)
                dataLog("Local mapping at ", pointerDump(block), ": ", mapDump(m_localMapping), "\n");
            
            // Process the block and replace all uses of loads with the promoted value.
            for (Node* node : *block) {
                m_graph.performSubstitution(node);
                
                if (Node* escapee = m_materializationToEscapee.get(node))
                    populateMaterialize(block, node, escapee);
                
                promoteHeapAccess(
                    node,
                    [&] (PromotedHeapLocation location, Edge value) {
                        if (m_sinkCandidates.contains(location.base()))
                            m_localMapping.set(location, value.node());
                    },
                    [&] (PromotedHeapLocation location) {
                        if (m_sinkCandidates.contains(location.base())) {
                            switch (node->op()) {
                            case CheckStructure:
                                node->convertToCheckStructureImmediate(resolve(block, location));
                                break;

                            default:
                                node->replaceWith(resolve(block, location));
                                break;
                            }
                        }
                    });
            }
            
            // Gotta drop some Upsilons.
            NodeAndIndex terminal = block->findTerminal();
            size_t upsilonInsertionPoint = terminal.index;
            NodeOrigin upsilonOrigin = terminal.node->origin;
            for (BasicBlock* successorBlock : block->successors()) {
                for (SSACalculator::Def* phiDef : m_ssaCalculator.phisForBlock(successorBlock)) {
                    Node* phiNode = phiDef->value();
                    SSACalculator::Variable* variable = phiDef->variable();
                    PromotedHeapLocation location = m_indexToLocation[variable->index()];
                    Node* incoming = resolve(block, location);
                    
                    m_insertionSet.insertNode(
                        upsilonInsertionPoint, SpecNone, Upsilon, upsilonOrigin,
                        OpInfo(phiNode), incoming->defaultEdge());
                }
            }
            
            m_insertionSet.execute(block);
        }
    }
    
    Node* resolve(BasicBlock* block, PromotedHeapLocation location)
    {
        if (Node* result = m_localMapping.get(location))
            return result;
        
        // This implies that there is no local mapping. Find a non-local mapping.
        SSACalculator::Def* def = m_ssaCalculator.nonLocalReachingDef(
            block, m_locationToVariable.get(location));
        ASSERT(def);
        ASSERT(def->value());
        m_localMapping.add(location, def->value());
        return def->value();
    }

    template<typename SinkCandidateFunctor, typename EscapeFunctor>
    void handleNode(
        Node* node,
        const SinkCandidateFunctor& sinkCandidate,
        const EscapeFunctor& escape)
    {
        switch (node->op()) {
        case NewObject:
        case MaterializeNewObject:
        case MaterializeCreateActivation:
            sinkCandidate();
            m_graph.doToChildren(
                node,
                [&] (Edge edge) {
                    escape(edge.node());
                });
            break;

        case NewFunction:
            if (!node->castOperand<FunctionExecutable*>()->singletonFunction()->isStillValid())
                sinkCandidate();
            escape(node->child1().node());
            break;

        case CreateActivation:
            if (!node->castOperand<SymbolTable*>()->singletonScope()->isStillValid())
                sinkCandidate();
            escape(node->child1().node());
            break;

        case Check:
            m_graph.doToChildren(
                node,
                [&] (Edge edge) {
                    if (edge.willNotHaveCheck())
                        return;
                    
                    if (alreadyChecked(edge.useKind(), SpecObject))
                        return;
                    
                    escape(edge.node());
                });
            break;

        case MovHint:
        case PutHint:
            break;

        case PutStructure:
        case CheckStructure:
        case GetByOffset:
        case MultiGetByOffset:
        case GetGetterSetterByOffset: {
            Node* target = node->child1().node();
            if (!target->isObjectAllocation())
                escape(target);
            break;
        }
            
        case PutByOffset: {
            Node* target = node->child2().node();
            if (!target->isObjectAllocation()) {
                escape(target);
                escape(node->child1().node());
            }
            escape(node->child3().node());
            break;
        }

        case GetClosureVar: {
            Node* target = node->child1().node();
            if (!target->isActivationAllocation())
                escape(target);
            break;
        }

        case PutClosureVar: {
            Node* target = node->child1().node();
            if (!target->isActivationAllocation())
                escape(target);
            escape(node->child2().node());
            break;
        }

        case GetScope: {
            Node* target = node->child1().node();
            if (!target->isFunctionAllocation())
                escape(target);
            break;
        }

        case GetExecutable: {
            Node* target = node->child1().node();
            if (!target->isFunctionAllocation())
                escape(target);
            break;
        }

        case SkipScope: {
            Node* target = node->child1().node();
            if (!target->isActivationAllocation())
                escape(target);
            break;
        }
            
        case MultiPutByOffset:
            // FIXME: In the future we should be able to handle this. It's just a matter of
            // building the appropriate *Hint variant of this instruction, along with a
            // PhantomStructureSelect node - since this transforms the Structure in a conditional
            // way.
            // https://bugs.webkit.org/show_bug.cgi?id=136924
            escape(node->child1().node());
            escape(node->child2().node());
            break;

        default:
            m_graph.doToChildren(
                node,
                [&] (Edge edge) {
                    escape(edge.node());
                });
            break;
        }
    }
    
    Node* createMaterialize(Node* escapee, Node* where)
    {
        Node* result = nullptr;
        
        switch (escapee->op()) {
        case NewObject:
        case MaterializeNewObject: {
            ObjectMaterializationData* data = m_graph.m_objectMaterializationData.add();
            
            result = m_graph.addNode(
                escapee->prediction(), Node::VarArg, MaterializeNewObject,
                NodeOrigin(
                    escapee->origin.semantic,
                    where->origin.forExit),
                OpInfo(data), OpInfo(), 0, 0);
            break;
        }

        case NewFunction:
            result = m_graph.addNode(
                escapee->prediction(), NewFunction,
                NodeOrigin(
                    escapee->origin.semantic,
                    where->origin.forExit),
                OpInfo(escapee->cellOperand()),
                escapee->child1());
            break;

        case CreateActivation:
        case MaterializeCreateActivation: {
            ObjectMaterializationData* data = m_graph.m_objectMaterializationData.add();
            FrozenValue* symbolTable = escapee->cellOperand();
            result = m_graph.addNode(
                escapee->prediction(), Node::VarArg, MaterializeCreateActivation,
                NodeOrigin(
                    escapee->origin.semantic,
                    where->origin.forExit),
                OpInfo(data), OpInfo(symbolTable), 0, 0);
            break;
        }

        default:
            DFG_CRASH(m_graph, escapee, "Bad escapee op");
            break;
        }
        
        if (verbose)
            dataLog("Creating materialization point at ", where, " for ", escapee, ": ", result, "\n");
        
        m_materializationToEscapee.add(result, escapee);
        m_materializationSiteToMaterializations.add(
            where, Vector<Node*>()).iterator->value.append(result);
        
        return result;
    }
    
    void populateMaterialize(BasicBlock* block, Node* node, Node* escapee)
    {
        switch (node->op()) {
        case MaterializeNewObject: {
            ObjectMaterializationData& data = node->objectMaterializationData();
            unsigned firstChild = m_graph.m_varArgChildren.size();
            
            Vector<PromotedHeapLocation> locations = m_locationsForAllocation.get(escapee);
            
            PromotedHeapLocation structure(StructurePLoc, escapee);
            ASSERT(locations.contains(structure));
            
            m_graph.m_varArgChildren.append(Edge(resolve(block, structure), KnownCellUse));
            
            for (unsigned i = 0; i < locations.size(); ++i) {
                switch (locations[i].kind()) {
                case StructurePLoc: {
                    ASSERT(locations[i] == structure);
                    break;
                }
                    
                case NamedPropertyPLoc: {
                    Node* value = resolve(block, locations[i]);
                    if (value->op() == BottomValue) {
                        // We can skip Bottoms entirely.
                        break;
                    }
                    
                    data.m_properties.append(PhantomPropertyValue(locations[i].info()));
                    m_graph.m_varArgChildren.append(value);
                    break;
                }
                    
                default:
                    DFG_CRASH(m_graph, node, "Bad location kind");
                }
            }
            
            node->children = AdjacencyList(
                AdjacencyList::Variable,
                firstChild, m_graph.m_varArgChildren.size() - firstChild);
            break;
        }

        case MaterializeCreateActivation: {
            ObjectMaterializationData& data = node->objectMaterializationData();

            unsigned firstChild = m_graph.m_varArgChildren.size();

            Vector<PromotedHeapLocation> locations = m_locationsForAllocation.get(escapee);

            PromotedHeapLocation scope(ActivationScopePLoc, escapee);
            PromotedHeapLocation symbolTable(ActivationSymbolTablePLoc, escapee);
            ASSERT(locations.contains(scope));

            m_graph.m_varArgChildren.append(Edge(resolve(block, scope), KnownCellUse));

            for (unsigned i = 0; i < locations.size(); ++i) {
                switch (locations[i].kind()) {
                case ActivationScopePLoc: {
                    ASSERT(locations[i] == scope);
                    break;
                }

                case ActivationSymbolTablePLoc: {
                    ASSERT(locations[i] == symbolTable);
                    break;
                }

                case ClosureVarPLoc: {
                    Node* value = resolve(block, locations[i]);
                    if (value->op() == BottomValue)
                        break;

                    data.m_properties.append(PhantomPropertyValue(locations[i].info()));
                    m_graph.m_varArgChildren.append(value);
                    break;
                }

                default:
                    DFG_CRASH(m_graph, node, "Bad location kind");
                }
            }

            node->children = AdjacencyList(
                AdjacencyList::Variable,
                firstChild, m_graph.m_varArgChildren.size() - firstChild);
            break;
        }

        case NewFunction: {
            if (!ASSERT_DISABLED) {
                Vector<PromotedHeapLocation> locations = m_locationsForAllocation.get(escapee);

                ASSERT(locations.size() == 2);

                PromotedHeapLocation executable(FunctionExecutablePLoc, escapee);
                ASSERT(locations.contains(executable));

                PromotedHeapLocation activation(FunctionActivationPLoc, escapee);
                ASSERT(locations.contains(activation));

                for (unsigned i = 0; i < locations.size(); ++i) {
                    switch (locations[i].kind()) {
                    case FunctionExecutablePLoc: {
                        ASSERT(locations[i] == executable);
                        break;
                    }

                    case FunctionActivationPLoc: {
                        ASSERT(locations[i] == activation);
                        break;
                    }

                    default:
                        DFG_CRASH(m_graph, node, "Bad location kind");
                    }
                }
            }

            break;
        }

        default:
            DFG_CRASH(m_graph, node, "Bad materialize op");
            break;
        }
    }
    
    CombinedLiveness m_combinedLiveness;
    SSACalculator m_ssaCalculator;
    HashSet<Node*> m_sinkCandidates;
    HashMap<Node*, Node*> m_materializationToEscapee;
    HashMap<Node*, Vector<Node*>> m_materializationSiteToMaterializations;
    HashMap<Node*, Vector<PromotedHeapLocation>> m_locationsForAllocation;
    HashMap<PromotedHeapLocation, SSACalculator::Variable*> m_locationToVariable;
    Vector<PromotedHeapLocation> m_indexToLocation;
    HashMap<PromotedHeapLocation, Node*> m_localMapping;
    InsertionSet m_insertionSet;
};
    
bool performObjectAllocationSinking(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Object Allocation Sinking Phase");
    return runPhase<ObjectAllocationSinkingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

