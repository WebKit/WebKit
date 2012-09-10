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
#include "DFGGraph.h"

#include "CodeBlock.h"
#include <wtf/BoundsCheckedPointer.h>

#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

// Creates an array of stringized names.
static const char* dfgOpNames[] = {
#define STRINGIZE_DFG_OP_ENUM(opcode, flags) #opcode ,
    FOR_EACH_DFG_OP(STRINGIZE_DFG_OP_ENUM)
#undef STRINGIZE_DFG_OP_ENUM
};

const char *Graph::opName(NodeType op)
{
    return dfgOpNames[op];
}

const char* Graph::nameOfVariableAccessData(VariableAccessData* variableAccessData)
{
    // Variables are already numbered. For readability of IR dumps, this returns
    // an alphabetic name for the variable access data, so that you don't have to
    // reason about two numbers (variable number and live range number), but instead
    // a number and a letter.
    
    unsigned index = std::numeric_limits<unsigned>::max();
    for (unsigned i = 0; i < m_variableAccessData.size(); ++i) {
        if (&m_variableAccessData[i] == variableAccessData) {
            index = i;
            break;
        }
    }
    
    ASSERT(index != std::numeric_limits<unsigned>::max());
    
    if (!index)
        return "A";

    static char buf[100];
    BoundsCheckedPointer<char> ptr(buf, sizeof(buf));
    
    while (index) {
        *ptr++ = 'A' + (index % 26);
        index /= 26;
    }
    
    if (variableAccessData->isCaptured())
        *ptr++ = '*';
    
    ptr.strcat(speculationToAbbreviatedString(variableAccessData->prediction()));
    
    *ptr++ = 0;
    
    return buf;
}

static void printWhiteSpace(unsigned amount)
{
    while (amount-- > 0)
        dataLog(" ");
}

void Graph::dumpCodeOrigin(const char* prefix, NodeIndex prevNodeIndex, NodeIndex nodeIndex)
{
    if (prevNodeIndex == NoNode)
        return;
    
    Node& currentNode = at(nodeIndex);
    Node& previousNode = at(prevNodeIndex);
    if (previousNode.codeOrigin.inlineCallFrame == currentNode.codeOrigin.inlineCallFrame)
        return;
    
    Vector<CodeOrigin> previousInlineStack = previousNode.codeOrigin.inlineStack();
    Vector<CodeOrigin> currentInlineStack = currentNode.codeOrigin.inlineStack();
    unsigned commonSize = std::min(previousInlineStack.size(), currentInlineStack.size());
    unsigned indexOfDivergence = commonSize;
    for (unsigned i = 0; i < commonSize; ++i) {
        if (previousInlineStack[i].inlineCallFrame != currentInlineStack[i].inlineCallFrame) {
            indexOfDivergence = i;
            break;
        }
    }
    
    // Print the pops.
    for (unsigned i = previousInlineStack.size(); i-- > indexOfDivergence;) {
        dataLog("%s", prefix);
        printWhiteSpace(i * 2);
        dataLog("<-- %p\n", previousInlineStack[i].inlineCallFrame->executable.get());
    }
    
    // Print the pushes.
    for (unsigned i = indexOfDivergence; i < currentInlineStack.size(); ++i) {
        dataLog("%s", prefix);
        printWhiteSpace(i * 2);
        dataLog("--> %p\n", currentInlineStack[i].inlineCallFrame->executable.get());
    }
}

int Graph::amountOfNodeWhiteSpace(Node& node)
{
    return (node.codeOrigin.inlineDepth() - 1) * 2;
}

void Graph::printNodeWhiteSpace(Node& node)
{
    printWhiteSpace(amountOfNodeWhiteSpace(node));
}

void Graph::dump(const char* prefix, NodeIndex nodeIndex)
{
    Node& node = at(nodeIndex);
    NodeType op = node.op();

    unsigned refCount = node.refCount();
    bool skipped = !refCount;
    bool mustGenerate = node.mustGenerate();
    if (mustGenerate)
        --refCount;
    
    dataLog("%s", prefix);
    printNodeWhiteSpace(node);

    // Example/explanation of dataflow dump output
    //
    //   14:   <!2:7>  GetByVal(@3, @13)
    //   ^1     ^2 ^3     ^4       ^5
    //
    // (1) The nodeIndex of this operation.
    // (2) The reference count. The number printed is the 'real' count,
    //     not including the 'mustGenerate' ref. If the node is
    //     'mustGenerate' then the count it prefixed with '!'.
    // (3) The virtual register slot assigned to this node.
    // (4) The name of the operation.
    // (5) The arguments to the operation. The may be of the form:
    //         @#   - a NodeIndex referencing a prior node in the graph.
    //         arg# - an argument number.
    //         $#   - the index in the CodeBlock of a constant { for numeric constants the value is displayed | for integers, in both decimal and hex }.
    //         id#  - the index in the CodeBlock of an identifier { if codeBlock is passed to dump(), the string representation is displayed }.
    //         var# - the index of a var on the global object, used by GetGlobalVar/PutGlobalVar operations.
    dataLog("% 4d:%s<%c%u:", (int)nodeIndex, skipped ? "  skipped  " : "           ", mustGenerate ? '!' : ' ', refCount);
    if (node.hasResult() && !skipped && node.hasVirtualRegister())
        dataLog("%u", node.virtualRegister());
    else
        dataLog("-");
    dataLog(">\t%s(", opName(op));
    bool hasPrinted = false;
    if (node.flags() & NodeHasVarArgs) {
        for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++) {
            if (hasPrinted)
                dataLog(", ");
            else
                hasPrinted = true;
            dataLog("%s@%u%s",
                    useKindToString(m_varArgChildren[childIdx].useKind()),
                    m_varArgChildren[childIdx].index(),
                    speculationToAbbreviatedString(
                        at(m_varArgChildren[childIdx]).prediction()));
        }
    } else {
        if (!!node.child1()) {
            dataLog("%s@%u%s",
                    useKindToString(node.child1().useKind()),
                    node.child1().index(),
                    speculationToAbbreviatedString(at(node.child1()).prediction()));
        }
        if (!!node.child2()) {
            dataLog(", %s@%u%s",
                    useKindToString(node.child2().useKind()),
                    node.child2().index(),
                    speculationToAbbreviatedString(at(node.child2()).prediction()));
        }
        if (!!node.child3()) {
            dataLog(", %s@%u%s",
                    useKindToString(node.child3().useKind()),
                    node.child3().index(),
                    speculationToAbbreviatedString(at(node.child3()).prediction()));
        }
        hasPrinted = !!node.child1();
    }

    if (strlen(nodeFlagsAsString(node.flags()))) {
        dataLog("%s%s", hasPrinted ? ", " : "", nodeFlagsAsString(node.flags()));
        hasPrinted = true;
    }
    if (node.hasArrayMode()) {
        dataLog("%s%s", hasPrinted ? ", " : "", modeToString(node.arrayMode()));
        hasPrinted = true;
    }
    if (node.hasVarNumber()) {
        dataLog("%svar%u", hasPrinted ? ", " : "", node.varNumber());
        hasPrinted = true;
    }
    if (node.hasRegisterPointer()) {
        dataLog(
            "%sglobal%u(%p)", hasPrinted ? ", " : "",
            globalObjectFor(node.codeOrigin)->findRegisterIndex(node.registerPointer()),
            node.registerPointer());
        hasPrinted = true;
    }
    if (node.hasIdentifier()) {
        dataLog("%sid%u{%s}", hasPrinted ? ", " : "", node.identifierNumber(), m_codeBlock->identifier(node.identifierNumber()).ustring().utf8().data());
        hasPrinted = true;
    }
    if (node.hasStructureSet()) {
        for (size_t i = 0; i < node.structureSet().size(); ++i) {
            dataLog("%sstruct(%p)", hasPrinted ? ", " : "", node.structureSet()[i]);
            hasPrinted = true;
        }
    }
    if (node.hasStructure()) {
        dataLog("%sstruct(%p)", hasPrinted ? ", " : "", node.structure());
        hasPrinted = true;
    }
    if (node.hasStructureTransitionData()) {
        dataLog("%sstruct(%p -> %p)", hasPrinted ? ", " : "", node.structureTransitionData().previousStructure, node.structureTransitionData().newStructure);
        hasPrinted = true;
    }
    if (node.hasStorageAccessData()) {
        StorageAccessData& storageAccessData = m_storageAccessData[node.storageAccessDataIndex()];
        dataLog("%sid%u{%s}", hasPrinted ? ", " : "", storageAccessData.identifierNumber, m_codeBlock->identifier(storageAccessData.identifierNumber).ustring().utf8().data());
        
        dataLog(", %lu", static_cast<unsigned long>(storageAccessData.offset));
        hasPrinted = true;
    }
    ASSERT(node.hasVariableAccessData() == node.hasLocal());
    if (node.hasVariableAccessData()) {
        VariableAccessData* variableAccessData = node.variableAccessData();
        int operand = variableAccessData->operand();
        if (operandIsArgument(operand))
            dataLog("%sarg%u(%s)", hasPrinted ? ", " : "", operandToArgument(operand), nameOfVariableAccessData(variableAccessData));
        else
            dataLog("%sr%u(%s)", hasPrinted ? ", " : "", operand, nameOfVariableAccessData(variableAccessData));
        hasPrinted = true;
    }
    if (node.hasConstantBuffer()) {
        if (hasPrinted)
            dataLog(", ");
        dataLog("%u:[", node.startConstant());
        for (unsigned i = 0; i < node.numConstants(); ++i) {
            if (i)
                dataLog(", ");
            dataLog("%s", m_codeBlock->constantBuffer(node.startConstant())[i].description());
        }
        dataLog("]");
        hasPrinted = true;
    }
    if (op == JSConstant) {
        dataLog("%s$%u", hasPrinted ? ", " : "", node.constantNumber());
        JSValue value = valueOfJSConstant(nodeIndex);
        dataLog(" = %s", value.description());
        hasPrinted = true;
    }
    if (op == WeakJSConstant) {
        dataLog("%s%p", hasPrinted ? ", " : "", node.weakConstant());
        hasPrinted = true;
    }
    if  (node.isBranch() || node.isJump()) {
        dataLog("%sT:#%u", hasPrinted ? ", " : "", node.takenBlockIndex());
        hasPrinted = true;
    }
    if  (node.isBranch()) {
        dataLog("%sF:#%u", hasPrinted ? ", " : "", node.notTakenBlockIndex());
        hasPrinted = true;
    }
    dataLog("%sbc#%u", hasPrinted ? ", " : "", node.codeOrigin.bytecodeIndex);
    hasPrinted = true;
    (void)hasPrinted;
    
    dataLog(")");

    if (!skipped) {
        if (node.hasVariableAccessData())
            dataLog("  predicting %s%s", speculationToString(node.variableAccessData()->prediction()), node.variableAccessData()->shouldUseDoubleFormat() ? ", forcing double" : "");
        else if (node.hasHeapPrediction())
            dataLog("  predicting %s", speculationToString(node.getHeapPrediction()));
    }
    
    dataLog("\n");
}

void Graph::dumpBlockHeader(const char* prefix, BlockIndex blockIndex, PhiNodeDumpMode phiNodeDumpMode)
{
    BasicBlock* block = m_blocks[blockIndex].get();

    dataLog("%sBlock #%u (bc#%u): %s%s\n", prefix, (int)blockIndex, block->bytecodeBegin, block->isReachable ? "" : " (skipped)", block->isOSRTarget ? " (OSR target)" : "");
    dataLog("%s  Predecessors:", prefix);
    for (size_t i = 0; i < block->m_predecessors.size(); ++i)
        dataLog(" #%u", block->m_predecessors[i]);
    dataLog("\n");
    if (m_dominators.isValid()) {
        dataLog("%s  Dominated by:", prefix);
        for (size_t i = 0; i < m_blocks.size(); ++i) {
            if (!m_dominators.dominates(i, blockIndex))
                continue;
            dataLog(" #%lu", static_cast<unsigned long>(i));
        }
        dataLog("\n");
        dataLog("%s  Dominates:", prefix);
        for (size_t i = 0; i < m_blocks.size(); ++i) {
            if (!m_dominators.dominates(blockIndex, i))
                continue;
            dataLog(" #%lu", static_cast<unsigned long>(i));
        }
        dataLog("\n");
    }
    dataLog("%s  Phi Nodes:", prefix);
    for (size_t i = 0; i < block->phis.size(); ++i) {
        NodeIndex phiNodeIndex = block->phis[i];
        Node& phiNode = at(phiNodeIndex);
        if (!phiNode.shouldGenerate() && phiNodeDumpMode == DumpLivePhisOnly)
            continue;
        dataLog(" @%u->(", phiNodeIndex);
        if (phiNode.child1()) {
            dataLog("@%u", phiNode.child1().index());
            if (phiNode.child2()) {
                dataLog(", @%u", phiNode.child2().index());
                if (phiNode.child3())
                    dataLog(", @%u", phiNode.child3().index());
            }
        }
        dataLog(")%s", i + 1 < block->phis.size() ? "," : "");
    }
    dataLog("\n");
}

void Graph::dump()
{
    NodeIndex lastNodeIndex = NoNode;
    for (size_t b = 0; b < m_blocks.size(); ++b) {
        BasicBlock* block = m_blocks[b].get();
        if (!block)
            continue;
        dumpBlockHeader("", b, DumpAllPhis);
        dataLog("  vars before: ");
        if (block->cfaHasVisited)
            dumpOperands(block->valuesAtHead, WTF::dataFile());
        else
            dataLog("<empty>");
        dataLog("\n");
        dataLog("  var links: ");
        dumpOperands(block->variablesAtHead, WTF::dataFile());
        dataLog("\n");
        for (size_t i = 0; i < block->size(); ++i) {
            dumpCodeOrigin("", lastNodeIndex, block->at(i));
            dump("", block->at(i));
            lastNodeIndex = block->at(i);
        }
        dataLog("  vars after: ");
        if (block->cfaHasVisited)
            dumpOperands(block->valuesAtTail, WTF::dataFile());
        else
            dataLog("<empty>");
        dataLog("\n");
        dataLog("  var links: ");
        dumpOperands(block->variablesAtTail, WTF::dataFile());
        dataLog("\n");
    }
}

// FIXME: Convert this to be iterative, not recursive.
#define DO_TO_CHILDREN(node, thingToDo) do {                            \
        Node& _node = (node);                                           \
        if (_node.flags() & NodeHasVarArgs) {                           \
            for (unsigned _childIdx = _node.firstChild();               \
                 _childIdx < _node.firstChild() + _node.numChildren();  \
                 _childIdx++)                                           \
                thingToDo(m_varArgChildren[_childIdx]);                 \
        } else {                                                        \
            if (!_node.child1()) {                                      \
                ASSERT(!_node.child2()                                  \
                       && !_node.child3());                             \
                break;                                                  \
            }                                                           \
            thingToDo(_node.child1());                                  \
                                                                        \
            if (!_node.child2()) {                                      \
                ASSERT(!_node.child3());                                \
                break;                                                  \
            }                                                           \
            thingToDo(_node.child2());                                  \
                                                                        \
            if (!_node.child3())                                        \
                break;                                                  \
            thingToDo(_node.child3());                                  \
        }                                                               \
    } while (false)

void Graph::refChildren(NodeIndex op)
{
    DO_TO_CHILDREN(at(op), ref);
}

void Graph::derefChildren(NodeIndex op)
{
    DO_TO_CHILDREN(at(op), deref);
}

void Graph::predictArgumentTypes()
{
    ASSERT(m_codeBlock->numParameters() >= 1);
    for (size_t arg = 0; arg < static_cast<size_t>(m_codeBlock->numParameters()); ++arg) {
        ValueProfile* profile = m_profiledBlock->valueProfileForArgument(arg);
        if (!profile)
            continue;
        
        at(m_arguments[arg]).variableAccessData()->predict(profile->computeUpdatedPrediction());
        
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLog("Argument [%zu] prediction: %s\n", arg, speculationToString(at(m_arguments[arg]).variableAccessData()->prediction()));
#endif
    }
}

void Graph::handleSuccessor(Vector<BlockIndex, 16>& worklist, BlockIndex blockIndex, BlockIndex successorIndex)
{
    BasicBlock* successor = m_blocks[successorIndex].get();
    if (!successor->isReachable) {
        successor->isReachable = true;
        worklist.append(successorIndex);
    }
    
    successor->m_predecessors.append(blockIndex);
}

void Graph::collectGarbage()
{
    // First reset the counts to 0 for all nodes.
    for (unsigned i = size(); i--;)
        at(i).setRefCount(0);
    
    // Now find the roots: the nodes that are must-generate. Set their ref counts to
    // 1 and put them on the worklist.
    Vector<NodeIndex, 128> worklist;
    for (BlockIndex blockIndex = 0; blockIndex < m_blocks.size(); ++blockIndex) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned indexInBlock = block->size(); indexInBlock--;) {
            NodeIndex nodeIndex = block->at(indexInBlock);
            Node& node = at(nodeIndex);
            if (!(node.flags() & NodeMustGenerate))
                continue;
            node.setRefCount(1);
            worklist.append(nodeIndex);
        }
    }
    
    while (!worklist.isEmpty()) {
        NodeIndex nodeIndex = worklist.last();
        worklist.removeLast();
        Node& node = at(nodeIndex);
        ASSERT(node.shouldGenerate()); // It should not be on the worklist unless it's ref'ed.
        if (node.flags() & NodeHasVarArgs) {
            for (unsigned childIdx = node.firstChild();
                 childIdx < node.firstChild() + node.numChildren();
                 ++childIdx) {
                NodeIndex childNodeIndex = m_varArgChildren[childIdx].index();
                if (!at(childNodeIndex).ref())
                    continue;
                worklist.append(childNodeIndex);
            }
        } else if (node.child1()) {
            if (at(node.child1()).ref())
                worklist.append(node.child1().index());
            if (node.child2()) {
                if (at(node.child2()).ref())
                    worklist.append(node.child2().index());
                if (node.child3()) {
                    if (at(node.child3()).ref())
                        worklist.append(node.child3().index());
                }
            }
        }
    }
}

void Graph::determineReachability()
{
    Vector<BlockIndex, 16> worklist;
    worklist.append(0);
    m_blocks[0]->isReachable = true;
    while (!worklist.isEmpty()) {
        BlockIndex index = worklist.last();
        worklist.removeLast();
        
        BasicBlock* block = m_blocks[index].get();
        ASSERT(block->isLinked);
        
        Node& node = at(block->last());
        ASSERT(node.isTerminal());
        
        if (node.isJump())
            handleSuccessor(worklist, index, node.takenBlockIndex());
        else if (node.isBranch()) {
            handleSuccessor(worklist, index, node.takenBlockIndex());
            handleSuccessor(worklist, index, node.notTakenBlockIndex());
        }
    }
}

void Graph::resetReachability()
{
    for (BlockIndex blockIndex = m_blocks.size(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        block->isReachable = false;
        block->m_predecessors.clear();
    }
    
    determineReachability();
}

void Graph::resetExitStates()
{
    for (unsigned i = size(); i--;)
        at(i).setCanExit(true);
}

} } // namespace JSC::DFG

#endif
