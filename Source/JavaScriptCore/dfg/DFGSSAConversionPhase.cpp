/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "DFGSSAConversionPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGBasicBlockInlines.h"
#include "DFGBlockInsertionSet.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "DFGSSACalculator.h"
#include "DFGVariableAccessDataDump.h"
#include "JSCJSValueInlines.h"
#include "OperandsInlines.h"

#undef RELEASE_ASSERT
#define RELEASE_ASSERT(assertion) do { \
    if (!(assertion)) { \
        WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion); \
        CRASH(); \
    } \
} while (0)

namespace JSC { namespace DFG {

class SSAConversionPhase : public Phase {
    static constexpr bool verbose = false;
    
public:
    SSAConversionPhase(Graph& graph)
        : Phase(graph, "SSA conversion")
        , m_insertionSet(graph)
    {
    }
    
    bool run()
    {
        RELEASE_ASSERT(m_graph.m_form == ThreadedCPS);
        RELEASE_ASSERT(!m_graph.m_isInSSAConversion);
        m_graph.m_isInSSAConversion = true;
        
        m_graph.clearReplacements();
        m_graph.clearCPSCFGData();

        HashMap<unsigned, BasicBlock*, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> entrypointIndexToArgumentsBlock;

        m_graph.m_numberOfEntrypoints = m_graph.m_roots.size();
        m_graph.m_argumentFormats.resize(m_graph.m_numberOfEntrypoints);

        for (unsigned entrypointIndex = 0; entrypointIndex < m_graph.m_numberOfEntrypoints; ++entrypointIndex) {
            BasicBlock* oldRoot = m_graph.m_roots[entrypointIndex];
            entrypointIndexToArgumentsBlock.add(entrypointIndex, oldRoot);
            
            NodeOrigin origin = oldRoot->at(0)->origin;
            m_insertionSet.insertNode(
                0, SpecNone, InitializeEntrypointArguments, origin, OpInfo(entrypointIndex));
            m_insertionSet.insertNode(
                0, SpecNone, ExitOK, origin);
            m_insertionSet.execute(oldRoot);
        }

        if (m_graph.m_numberOfEntrypoints > 1) {
            BlockInsertionSet blockInsertionSet(m_graph);
            BasicBlock* newRoot = blockInsertionSet.insert(0, 1.0f);

            EntrySwitchData* entrySwitchData = m_graph.m_entrySwitchData.add();
            for (unsigned entrypointIndex = 0; entrypointIndex < m_graph.m_numberOfEntrypoints; ++entrypointIndex) {
                BasicBlock* oldRoot = m_graph.m_roots[entrypointIndex];
                entrySwitchData->cases.append(oldRoot);

                ASSERT(oldRoot->predecessors.isEmpty());
                oldRoot->predecessors.append(newRoot);

                if (oldRoot->isCatchEntrypoint) {
                    ASSERT(!!entrypointIndex);
                    m_graph.m_entrypointIndexToCatchBytecodeIndex.add(entrypointIndex, oldRoot->bytecodeBegin);
                }
            }

            RELEASE_ASSERT(entrySwitchData->cases[0] == m_graph.block(0)); // We strongly assume the normal call entrypoint is the first item in the list.

            const bool exitOK = false;
            NodeOrigin origin { CodeOrigin(BytecodeIndex(0)), CodeOrigin(BytecodeIndex(0)), exitOK };
            newRoot->appendNode(
                m_graph, SpecNone, EntrySwitch, origin, OpInfo(entrySwitchData));

            m_graph.m_roots.clear();
            m_graph.m_roots.append(newRoot);

            blockInsertionSet.execute();
        }
        
        SSACalculator calculator(m_graph);

        m_graph.ensureSSADominators();
        
        if (verbose) {
            dataLog("Graph before SSA transformation:\n");
            m_graph.dump();
        }

        // Create a SSACalculator::Variable for every root VariableAccessData.
        for (VariableAccessData& variable : m_graph.m_variableAccessData) {
            if (!variable.isRoot())
                continue;
            
            SSACalculator::Variable* ssaVariable = calculator.newVariable();
            ASSERT(ssaVariable->index() == m_variableForSSAIndex.size());
            m_variableForSSAIndex.append(&variable);
            m_ssaVariableForVariable.add(&variable, ssaVariable);
        }
        
        // Find all SetLocals and create Defs for them. We handle SetArgumentDefinitely by creating a
        // GetLocal, and recording the flush format.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            // Must process the block in forward direction because we want to see the last
            // assignment for every local.
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                if (node->op() != SetLocal && node->op() != SetArgumentDefinitely)
                    continue;
                
                VariableAccessData* variable = node->variableAccessData();
                
                Node* childNode;
                if (node->op() == SetLocal)
                    childNode = node->child1().node();
                else {
                    ASSERT(node->op() == SetArgumentDefinitely);
                    childNode = m_insertionSet.insertNode(
                        nodeIndex, node->variableAccessData()->prediction(),
                        GetStack, node->origin,
                        OpInfo(m_graph.m_stackAccessData.add(variable->operand(), variable->flushFormat())));
                    if (ASSERT_ENABLED)
                        m_argumentGetters.add(childNode);
                    m_argumentMapping.add(node, childNode);
                }
                
                calculator.newDef(
                    m_ssaVariableForVariable.get(variable), block, childNode);
            }
            
            m_insertionSet.execute(block);
        }
        
        // Decide where Phis are to be inserted. This creates the Phi's but doesn't insert them
        // yet. We will later know where to insert based on where SSACalculator tells us to.
        calculator.computePhis(
            [&] (SSACalculator::Variable* ssaVariable, BasicBlock* block) -> Node* {
                VariableAccessData* variable = m_variableForSSAIndex[ssaVariable->index()];
                
                // Prune by liveness. This doesn't buy us much other than compile times.
                Node* headNode = block->variablesAtHead.operand(variable->operand());
                if (!headNode)
                    return nullptr;

                // There is the possibiltiy of "rebirths". The SSA calculator will already prune
                // rebirths for the same VariableAccessData. But it will not be able to prune
                // rebirths that arose from the same local variable number but a different
                // VariableAccessData. We do that pruning here.
                //
                // Here's an example of a rebirth that this would catch:
                //
                //     var x;
                //     if (foo) {
                //         if (bar) {
                //             x = 42;
                //         } else {
                //             x = 43;
                //         }
                //         print(x);
                //         x = 44;
                //     } else {
                //         x = 45;
                //     }
                //     print(x); // Without this check, we'd have a Phi for x = 42|43 here.
                //
                // FIXME: Consider feeding local variable numbers, not VariableAccessData*'s, as
                // the "variables" for SSACalculator. That would allow us to eliminate this
                // special case.
                // https://bugs.webkit.org/show_bug.cgi?id=136641
                if (headNode->variableAccessData() != variable)
                    return nullptr;
                
                Node* phiNode = m_graph.addNode(
                    variable->prediction(), Phi, block->at(0)->origin.withInvalidExit());
                FlushFormat format = variable->flushFormat();
                NodeFlags result = resultFor(format);
                phiNode->mergeFlags(result);
                return phiNode;
            });
        
        if (verbose) {
            dataLog("Computed Phis, about to transform the graph.\n");
            dataLog("\n");
            dataLog("Graph:\n");
            m_graph.dump();
            dataLog("\n");
            dataLog("Mappings:\n");
            for (unsigned i = 0; i < m_variableForSSAIndex.size(); ++i)
                dataLog("    ", i, ": ", VariableAccessDataDump(m_graph, m_variableForSSAIndex[i]), "\n");
            dataLog("\n");
            dataLog("SSA calculator: ", calculator, "\n");
        }
        
        // Do the bulk of the SSA conversion. For each block, this tracks the operand->Node
        // mapping based on a combination of what the SSACalculator tells us, and us walking over
        // the block in forward order. We use our own data structure, valueForOperand, for
        // determining the local mapping, but we rely on SSACalculator for the non-local mapping.
        //
        // This does three things at once:
        //
        // - Inserts the Phis in all of the places where they need to go. We've already created
        //   them and they are accounted for in the SSACalculator's data structures, but we
        //   haven't inserted them yet, mostly because we want to insert all of a block's Phis in
        //   one go to amortize the cost of node insertion.
        //
        // - Create and insert Upsilons.
        //
        // - Convert all of the preexisting SSA nodes (other than the old CPS Phi nodes) into SSA
        //   form by replacing as follows:
        //
        //   - MovHint has KillLocal prepended to it.
        //
        //   - GetLocal die and get replaced with references to the node specified by
        //     valueForOperand.
        //
        //   - SetLocal turns into PutStack if it's flushed, or turns into a Check otherwise.
        //
        //   - Flush is removed.
        //
        //   - PhantomLocal becomes Phantom, and its child is whatever is specified by
        //     valueForOperand.
        //
        //   - SetArgumentDefinitely is removed. Note that GetStack nodes have already been inserted.
        //
        //   - SetArgumentMaybe is removed. It should not have any data flow uses.
        Operands<Node*> valueForOperand(OperandsLike, m_graph.block(0)->variablesAtHead);
        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            valueForOperand.clear();
            
            // CPS will claim that the root block has all arguments live. But we have already done
            // the first step of SSA conversion: argument locals are no longer live at head;
            // instead we have GetStack nodes for extracting the values of arguments. So, we
            // skip the at-head available value calculation for the root block.
            if (block != m_graph.block(0)) {
                for (size_t i = valueForOperand.size(); i--;) {
                    Node* nodeAtHead = block->variablesAtHead[i];
                    if (!nodeAtHead)
                        continue;
                    
                    VariableAccessData* variable = nodeAtHead->variableAccessData();
                    
                    if (verbose)
                        dataLog("Considering live variable ", VariableAccessDataDump(m_graph, variable), " at head of block ", *block, "\n");
                    
                    SSACalculator::Variable* ssaVariable = m_ssaVariableForVariable.get(variable);
                    SSACalculator::Def* def = calculator.reachingDefAtHead(block, ssaVariable);
                    if (!def) {
                        // If we are required to insert a Phi, then we won't have a reaching def
                        // at head.
                        continue;
                    }
                    
                    Node* node = def->value();
                    if (node->replacement()) {
                        // This will occur when a SetLocal had a GetLocal as its source. The
                        // GetLocal would get replaced with an actual SSA value by the time we get
                        // here. Note that the SSA value with which the GetLocal got replaced
                        // would not in turn have a replacement.
                        node = node->replacement();
                        ASSERT(!node->replacement());
                    }
                    if (verbose)
                        dataLog("Mapping: ", valueForOperand.operandForIndex(i), " -> ", node, "\n");
                    valueForOperand[i] = node;
                }
            }
            
            // Insert Phis by asking the calculator what phis there are in this block. Also update
            // valueForOperand with those Phis. For Phis associated with variables that are not
            // flushed, we also insert a MovHint.
            size_t phiInsertionPoint = 0;
            for (SSACalculator::Def* phiDef : calculator.phisForBlock(block)) {
                VariableAccessData* variable = m_variableForSSAIndex[phiDef->variable()->index()];
                
                m_insertionSet.insert(phiInsertionPoint, phiDef->value());
                valueForOperand.operand(variable->operand()) = phiDef->value();
                
                m_insertionSet.insertNode(
                    phiInsertionPoint, SpecNone, MovHint, block->at(0)->origin.withInvalidExit(),
                    OpInfo(variable->operand()), phiDef->value()->defaultEdge());
            }

            if (block->at(0)->origin.exitOK)
                m_insertionSet.insertNode(phiInsertionPoint, SpecNone, ExitOK, block->at(0)->origin);
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                
                if (verbose) {
                    dataLog("Processing node ", node, ":\n");
                    m_graph.dump(WTF::dataFile(), "    ", node);
                }
                
                m_graph.performSubstitution(node);
                
                switch (node->op()) {
                case MovHint: {
                    m_insertionSet.insertNode(
                        nodeIndex, SpecNone, KillStack, node->origin,
                        OpInfo(node->unlinkedOperand()));
                    node->origin.exitOK = false; // KillStack clobbers exit.
                    break;
                }
                    
                case SetLocal: {
                    VariableAccessData* variable = node->variableAccessData();
                    Node* child = node->child1().node();
                    
                    if (!!(node->flags() & NodeIsFlushed)) {
                        node->convertToPutStack(
                            m_graph.m_stackAccessData.add(
                                variable->operand(), variable->flushFormat()));
                    } else
                        node->remove(m_graph);
                    
                    if (verbose)
                        dataLog("Mapping: ", variable->operand(), " -> ", child, "\n");
                    valueForOperand.operand(variable->operand()) = child;
                    break;
                }
                    
                case GetStack: {
                    ASSERT(m_argumentGetters.contains(node));
                    valueForOperand.operand(node->stackAccessData()->operand) = node;
                    break;
                }
                    
                case GetLocal: {
                    VariableAccessData* variable = node->variableAccessData();
                    node->children.reset();
                    
                    node->remove(m_graph);
                    if (verbose)
                        dataLog("Replacing node ", node, " with ", valueForOperand.operand(variable->operand()), "\n");
                    node->setReplacement(valueForOperand.operand(variable->operand()));
                    break;
                }
                    
                case Flush: {
                    node->children.reset();
                    node->remove(m_graph);
                    break;
                }
                    
                case PhantomLocal: {
                    ASSERT(node->child1().useKind() == UntypedUse);
                    VariableAccessData* variable = node->variableAccessData();
                    node->child1() = valueForOperand.operand(variable->operand())->defaultEdge();
                    node->remove(m_graph);
                    break;
                }
                    
                case SetArgumentDefinitely: {
                    node->remove(m_graph);
                    break;
                }

                case SetArgumentMaybe: {
                    node->remove(m_graph);
                    break;
                }
                    
                default:
                    break;
                }
            }
            
            // We want to insert Upsilons just before the end of the block. On the surface this
            // seems dangerous because the Upsilon will have a checking UseKind. But, we will not
            // actually be performing the check at the point of the Upsilon; the check will
            // already have been performed at the point where the original SetLocal was.
            NodeAndIndex terminal = block->findTerminal();
            size_t upsilonInsertionPoint = terminal.index;
            NodeOrigin upsilonOrigin = terminal.node->origin;
            for (unsigned successorIndex = block->numSuccessors(); successorIndex--;) {
                BasicBlock* successorBlock = block->successor(successorIndex);
                for (SSACalculator::Def* phiDef : calculator.phisForBlock(successorBlock)) {
                    Node* phiNode = phiDef->value();
                    SSACalculator::Variable* ssaVariable = phiDef->variable();
                    VariableAccessData* variable = m_variableForSSAIndex[ssaVariable->index()];
                    FlushFormat format = variable->flushFormat();

                    // We can use an unchecked use kind because the SetLocal was turned into a Check.
                    // We have to use an unchecked use because at least sometimes, the end of the block
                    // is not exitOK.
                    UseKind useKind = uncheckedUseKindFor(format);

                    dataLogLnIf(verbose, "Inserting Upsilon for ", variable->operand(), " propagating ", valueForOperand.operand(variable->operand()), " to ", phiNode);
                    
                    m_insertionSet.insertNode(
                        upsilonInsertionPoint, SpecNone, Upsilon, upsilonOrigin,
                        OpInfo(phiNode), Edge(
                            valueForOperand.operand(variable->operand()),
                            useKind));
                }
            }
            
            m_insertionSet.execute(block);
        }
        
        // Free all CPS phis and reset variables vectors.
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned phiIndex = block->phis.size(); phiIndex--;)
                m_graph.deleteNode(block->phis[phiIndex]);
            block->phis.clear();
            block->variablesAtHead.clear();
            block->variablesAtTail.clear();
            block->valuesAtHead.clear();
            block->valuesAtHead.clear();
            block->ssa = makeUnique<BasicBlock::SSAData>(block);
        }

        for (auto& pair : entrypointIndexToArgumentsBlock) {
            unsigned entrypointIndex = pair.key;
            BasicBlock* oldRoot = pair.value;
            ArgumentsVector& arguments = m_graph.m_rootToArguments.find(oldRoot)->value;
            Vector<FlushFormat> argumentFormats;
            argumentFormats.reserveInitialCapacity(arguments.size());
            for (unsigned i = 0; i < arguments.size(); ++i) {
                Node* node = m_argumentMapping.get(arguments[i]);
                RELEASE_ASSERT(node);
                argumentFormats.uncheckedAppend(node->stackAccessData()->format);
            }
            m_graph.m_argumentFormats[entrypointIndex] = WTFMove(argumentFormats);
        }

        m_graph.m_rootToArguments.clear();

        RELEASE_ASSERT(m_graph.m_isInSSAConversion);
        m_graph.m_isInSSAConversion = false;

        m_graph.m_form = SSA;

        if (verbose) {
            dataLog("Graph after SSA transformation:\n");
            m_graph.dump();
        }

        return true;
    }

private:
    InsertionSet m_insertionSet;
    HashMap<VariableAccessData*, SSACalculator::Variable*> m_ssaVariableForVariable;
    HashMap<Node*, Node*> m_argumentMapping;
    HashSet<Node*> m_argumentGetters;
    Vector<VariableAccessData*> m_variableForSSAIndex;
};

bool performSSAConversion(Graph& graph)
{
    bool result = runPhase<SSAConversionPhase>(graph);
    return result;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

