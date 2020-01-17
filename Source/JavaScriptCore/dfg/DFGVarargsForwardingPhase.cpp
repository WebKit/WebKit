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
#include "DFGVarargsForwardingPhase.h"

#if ENABLE(DFG_JIT)

#include "ClonedArguments.h"
#include "DFGArgumentsUtilities.h"
#include "DFGClobberize.h"
#include "DFGForAllKills.h"
#include "DFGGraph.h"
#include "DFGPhase.h"
#include "JSCInlines.h"
#include <wtf/ListDump.h>

namespace JSC { namespace DFG {

namespace {


class VarargsForwardingPhase : public Phase {
    static constexpr bool verbose = false;
public:
    VarargsForwardingPhase(Graph& graph)
        : Phase(graph, "varargs forwarding")
    {
    }
    
    bool run()
    {
        DFG_ASSERT(m_graph, nullptr, m_graph.m_form != SSA);
        
        if (verbose) {
            dataLog("Graph before varargs forwarding:\n");
            m_graph.dump();
        }
        
        m_changed = false;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder())
            handleBlock(block);
        return m_changed;
    }

private:
    void handleBlock(BasicBlock* block)
    {
        for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
            Node* node = block->at(nodeIndex);
            switch (node->op()) {
            case CreateDirectArguments:
            case CreateClonedArguments:
                handleCandidate(block, nodeIndex);
                break;
            default:
                break;
            }
        }
    }
    
    void handleCandidate(BasicBlock* block, unsigned candidateNodeIndex)
    {
        // We expect calls into this function to be rare. So, this is written in a simple O(n) manner.
        
        Node* candidate = block->at(candidateNodeIndex);
        if (verbose)
            dataLog("Handling candidate ", candidate, "\n");
        
        // We eliminate GetButterfly over CreateClonedArguments if the butterfly is only
        // used by a GetByOffset  that loads the CreateClonedArguments's length. We also
        // eliminate it if the GetButterfly node is totally unused.
        Vector<Node*, 1> candidateButterflies; 

        // Find the index of the last node in this block to use the candidate, and look for escaping
        // sites.
        unsigned lastUserIndex = candidateNodeIndex;
        Vector<VirtualRegister, 2> relevantLocals; // This is a set. We expect it to be a small set.
        for (unsigned nodeIndex = candidateNodeIndex + 1; nodeIndex < block->size(); ++nodeIndex) {
            Node* node = block->at(nodeIndex);

            auto defaultEscape = [&] {
                if (m_graph.uses(node, candidate)) {
                    if (verbose)
                        dataLog("    Escape at ", node, "\n");
                    return true;
                }
                return false;
            };

            bool validGetByOffset = false;
            switch (node->op()) {
            case MovHint:
                if (node->child1() != candidate)
                    break;
                ASSERT_WITH_MESSAGE(!node->unlinkedOperand().isTmp(), "We don't currently support a tmp referring to an arguments object.");
                lastUserIndex = nodeIndex;
                if (!relevantLocals.contains(node->unlinkedOperand().virtualRegister()))
                    relevantLocals.append(node->unlinkedOperand().virtualRegister());
                break;
                
            case CheckVarargs:
            case Check: {
                bool sawEscape = false;
                m_graph.doToChildren(
                    node,
                    [&] (Edge edge) {
                        if (edge == candidate)
                            lastUserIndex = nodeIndex;
                        
                        if (edge.willNotHaveCheck())
                            return;
                        
                        if (alreadyChecked(edge.useKind(), SpecObject))
                            return;
                        
                        sawEscape = true;
                    });
                if (sawEscape) {
                    if (verbose)
                        dataLog("    Escape at ", node, "\n");
                    return;
                }
                break;
            }
                
            case VarargsLength:
            case LoadVarargs:
                if (m_graph.uses(node, candidate))
                    lastUserIndex = nodeIndex;
                break;
                
            case CallVarargs:
            case ConstructVarargs:
            case TailCallVarargs:
            case TailCallVarargsInlinedCaller:
                if (node->child1() == candidate || node->child2() == candidate) {
                    if (verbose)
                        dataLog("    Escape at ", node, "\n");
                    return;
                }
                if (node->child2() == candidate)
                    lastUserIndex = nodeIndex;
                break;
                
            case SetLocal:
                if (node->child1() == candidate && node->variableAccessData()->isLoadedFrom()) {
                    if (verbose)
                        dataLog("    Escape at ", node, "\n");
                    return;
                }
                break;

            case GetArrayLength: {
                if (node->arrayMode().type() == Array::DirectArguments && node->child1() == candidate && node->child1()->op() == CreateDirectArguments) {
                    lastUserIndex = nodeIndex;
                    break;
                }
                if (defaultEscape())
                    return;
                break;
            }
            
            case GetButterfly: {
                if (node->child1() == candidate && candidate->op() == CreateClonedArguments) {
                    lastUserIndex = nodeIndex;
                    candidateButterflies.append(node);
                    break;
                }
                if (defaultEscape())
                    return;
                break;
            }
                
            case FilterGetByStatus:
            case FilterPutByIdStatus:
            case FilterCallLinkStatus:
            case FilterInByIdStatus:
                break;

            case GetByOffset: {
                if (node->child1()->op() == GetButterfly
                    && candidateButterflies.contains(node->child1().node())
                    && node->child2() == candidate
                    && node->storageAccessData().offset == clonedArgumentsLengthPropertyOffset) {
                    ASSERT(node->child1()->child1() == candidate);
                    ASSERT(isOutOfLineOffset(clonedArgumentsLengthPropertyOffset));
                    // We're good to go. This is getting the length of the arguments.
                    lastUserIndex = nodeIndex;
                    validGetByOffset = true;
                    break;
                }
                if (defaultEscape())
                    return;
                break;
            }

            default:
                if (defaultEscape())
                    return;
                break;
            }

            if (!validGetByOffset) {
                for (Node* butterfly : candidateButterflies) {
                    if (m_graph.uses(node, butterfly)) {
                        if (verbose)
                            dataLog("    Butterfly escaped at ", node, "\n");
                        return;
                    }
                }
            }

            forAllKilledOperands(
                m_graph, node, block->tryAt(nodeIndex + 1),
                [&] (Operand operand) {
                    if (verbose)
                        dataLog("    Killing ", operand, " while we are interested in ", listDump(relevantLocals), "\n");
                    for (unsigned i = 0; i < relevantLocals.size(); ++i) {
                        if (operand == relevantLocals[i]) {
                            relevantLocals[i--] = relevantLocals.last();
                            relevantLocals.removeLast();
                            lastUserIndex = nodeIndex;
                        }
                    }
                });
        }
        if (verbose)
            dataLog("Selected lastUserIndex = ", lastUserIndex, ", ", block->at(lastUserIndex), "\n");
        
        // We're still in business. Determine if between the candidate and the last user there is any
        // effect that could interfere with sinking.
        for (unsigned nodeIndex = candidateNodeIndex + 1; nodeIndex <= lastUserIndex; ++nodeIndex) {
            Node* node = block->at(nodeIndex);
            
            // We have our own custom switch to detect some interferences that clobberize() wouldn't know
            // about, and also some of the common ones, too. In particular, clobberize() doesn't know
            // that Flush, MovHint, ZombieHint, and KillStack are bad because it's not worried about
            // what gets read on OSR exit.
            switch (node->op()) {
            case MovHint:
            case ZombieHint:
            case KillStack:
                if (argumentsInvolveStackSlot(candidate, node->unlinkedOperand())) {
                    if (verbose)
                        dataLog("    Interference at ", node, "\n");
                    return;
                }
                break;
                
            case PutStack:
                if (argumentsInvolveStackSlot(candidate, node->stackAccessData()->operand)) {
                    if (verbose)
                        dataLog("    Interference at ", node, "\n");
                    return;
                }
                break;
                
            case SetLocal:
            case Flush:
                if (argumentsInvolveStackSlot(candidate, node->operand())) {
                    if (verbose)
                        dataLog("    Interference at ", node, "\n");
                    return;
                }
                break;
                
            default: {
                bool doesInterfere = false;
                clobberize(
                    m_graph, node, NoOpClobberize(),
                    [&] (AbstractHeap heap) {
                        if (heap.kind() != Stack) {
                            ASSERT(!heap.overlaps(Stack));
                            return;
                        }
                        ASSERT(!heap.payload().isTop());
                        if (argumentsInvolveStackSlot(candidate, heap.operand()))
                            doesInterfere = true;
                    },
                    NoOpClobberize());
                if (doesInterfere) {
                    if (verbose)
                        dataLog("    Interference at ", node, "\n");
                    return;
                }
            } }
        }
        
        // We can make this work.
        if (verbose)
            dataLog("    Will do forwarding!\n");
        m_changed = true;
        
        // Transform the program.
        switch (candidate->op()) {
        case CreateDirectArguments:
            candidate->setOpAndDefaultFlags(PhantomDirectArguments);
            break;

        case CreateClonedArguments:
            candidate->setOpAndDefaultFlags(PhantomClonedArguments);
            break;
            
        default:
            DFG_CRASH(m_graph, candidate, "bad node type");
            break;
        }

        InsertionSet insertionSet(m_graph);
        for (unsigned nodeIndex = candidateNodeIndex + 1; nodeIndex <= lastUserIndex; ++nodeIndex) {
            Node* node = block->at(nodeIndex);
            switch (node->op()) {
            case Check:
            case CheckVarargs:
            case MovHint:
            case PutHint:
                // We don't need to change anything with these.
                break;
                
            case VarargsLength: {
                if (node->argumentsChild() != candidate)
                    break;

                node->convertToIdentityOn(emitCodeToGetArgumentsArrayLength(insertionSet, candidate, nodeIndex, node->origin, /* addThis = */ true));
                break;
            }

            case LoadVarargs:
                if (node->argumentsChild() != candidate)
                    break;
                node->setOpAndDefaultFlags(ForwardVarargs);
                break;
                
            case CallVarargs:
                if (node->child3() != candidate)
                    break;
                node->setOpAndDefaultFlags(CallForwardVarargs);
                break;
                
            case ConstructVarargs:
                if (node->child3() != candidate)
                    break;
                node->setOpAndDefaultFlags(ConstructForwardVarargs);
                break;

            case TailCallVarargs:
                if (node->child3() != candidate)
                    break;
                node->setOpAndDefaultFlags(TailCallForwardVarargs);
                break;

            case TailCallVarargsInlinedCaller:
                if (node->child3() != candidate)
                    break;
                node->setOpAndDefaultFlags(TailCallForwardVarargsInlinedCaller);
                break;

            case SetLocal:
                // This is super odd. We don't have to do anything here, since in DFG IR, the phantom
                // arguments nodes do produce a JSValue. Also, we know that if this SetLocal referenecs a
                // candidate then the SetLocal - along with all of its references - will die off pretty
                // soon, since it has no real users. DCE will surely kill it. If we make it to SSA, then
                // SSA conversion will kill it.
                break;

            case GetButterfly: {
                if (node->child1().node() == candidate) {
                    ASSERT(candidateButterflies.contains(node));
                    node->child1() = Edge();
                    node->remove(m_graph);
                }
                break;
            }

            case FilterGetByStatus:
            case FilterPutByIdStatus:
            case FilterCallLinkStatus:
            case FilterInByIdStatus:
                if (node->child1().node() == candidate)
                    node->remove(m_graph);
                break;

            case GetByOffset: {
                if (node->child2() == candidate) {
                    ASSERT(candidateButterflies.contains(node->child1().node())); // It's no longer a GetButterfly node, but it should've been a candidate butterfly.
                    ASSERT(node->storageAccessData().offset == clonedArgumentsLengthPropertyOffset);
                    node->convertToIdentityOn(
                        emitCodeToGetArgumentsArrayLength(insertionSet, candidate, nodeIndex, node->origin));
                }
                break;
            }

            case GetArrayLength:
                if (node->arrayMode().type() == Array::DirectArguments && node->child1() == candidate) {
                    node->convertToIdentityOn(
                        emitCodeToGetArgumentsArrayLength(insertionSet, candidate, nodeIndex, node->origin));
                }
                break;

            default:
                if (!ASSERT_ENABLED)
                    break;
                m_graph.doToChildren(
                    node,
                    [&] (Edge edge) {
                        DFG_ASSERT(m_graph, node, edge != candidate);
                    });
                break;
            }
        }

        insertionSet.execute(block);
    }
    
    bool m_changed;
};

} // anonymous namespace

bool performVarargsForwarding(Graph& graph)
{
    return runPhase<VarargsForwardingPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

