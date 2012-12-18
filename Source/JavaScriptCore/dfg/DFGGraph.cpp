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
#include "DFGVariableAccessDataDump.h"

#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

// Creates an array of stringized names.
static const char* dfgOpNames[] = {
#define STRINGIZE_DFG_OP_ENUM(opcode, flags) #opcode ,
    FOR_EACH_DFG_OP(STRINGIZE_DFG_OP_ENUM)
#undef STRINGIZE_DFG_OP_ENUM
};

Graph::Graph(JSGlobalData& globalData, CodeBlock* codeBlock, unsigned osrEntryBytecodeIndex, const Operands<JSValue>& mustHandleValues)
    : m_globalData(globalData)
    , m_codeBlock(codeBlock)
    , m_compilation(globalData.m_perBytecodeProfiler ? globalData.m_perBytecodeProfiler->newCompilation(codeBlock, Profiler::DFG) : 0)
    , m_profiledBlock(codeBlock->alternative())
    , m_hasArguments(false)
    , m_osrEntryBytecodeIndex(osrEntryBytecodeIndex)
    , m_mustHandleValues(mustHandleValues)
    , m_fixpointState(BeforeFixpoint)
{
    ASSERT(m_profiledBlock);
}

const char *Graph::opName(NodeType op)
{
    return dfgOpNames[op];
}

static void printWhiteSpace(PrintStream& out, unsigned amount)
{
    while (amount-- > 0)
        out.print(" ");
}

bool Graph::dumpCodeOrigin(PrintStream& out, const char* prefix, NodeIndex prevNodeIndex, NodeIndex nodeIndex)
{
    if (prevNodeIndex == NoNode)
        return false;
    
    Node& currentNode = at(nodeIndex);
    Node& previousNode = at(prevNodeIndex);
    if (previousNode.codeOrigin.inlineCallFrame == currentNode.codeOrigin.inlineCallFrame)
        return false;
    
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
    
    bool hasPrinted = false;
    
    // Print the pops.
    for (unsigned i = previousInlineStack.size(); i-- > indexOfDivergence;) {
        out.print(prefix);
        printWhiteSpace(out, i * 2);
        out.print("<-- ", *previousInlineStack[i].inlineCallFrame, "\n");
        hasPrinted = true;
    }
    
    // Print the pushes.
    for (unsigned i = indexOfDivergence; i < currentInlineStack.size(); ++i) {
        out.print(prefix);
        printWhiteSpace(out, i * 2);
        out.print("--> ", *currentInlineStack[i].inlineCallFrame, "\n");
        hasPrinted = true;
    }
    
    return hasPrinted;
}

int Graph::amountOfNodeWhiteSpace(Node& node)
{
    return (node.codeOrigin.inlineDepth() - 1) * 2;
}

void Graph::printNodeWhiteSpace(PrintStream& out, Node& node)
{
    printWhiteSpace(out, amountOfNodeWhiteSpace(node));
}

void Graph::dump(PrintStream& out, Edge edge)
{
    out.print(
        useKindToString(edge.useKind()),
        "@", edge.index(),
        AbbreviatedSpeculationDump(at(edge).prediction()));
}

void Graph::dump(PrintStream& out, const char* prefix, NodeIndex nodeIndex)
{
    Node& node = at(nodeIndex);
    NodeType op = node.op();

    unsigned refCount = node.refCount();
    bool skipped = !refCount;
    bool mustGenerate = node.mustGenerate();
    if (mustGenerate)
        --refCount;

    out.print(prefix);
    printNodeWhiteSpace(out, node);

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
    out.printf("% 4d:%s<%c%u:", (int)nodeIndex, skipped ? "  skipped  " : "           ", mustGenerate ? '!' : ' ', refCount);
    if (node.hasResult() && !skipped && node.hasVirtualRegister())
        out.print(node.virtualRegister());
    else
        out.print("-");
    out.print(">\t", opName(op), "(");
    bool hasPrinted = false;
    if (node.flags() & NodeHasVarArgs) {
        for (unsigned childIdx = node.firstChild(); childIdx < node.firstChild() + node.numChildren(); childIdx++) {
            if (hasPrinted)
                out.print(", ");
            else
                hasPrinted = true;
            if (!m_varArgChildren[childIdx])
                continue;
            dump(out, m_varArgChildren[childIdx]);
        }
    } else {
        if (!!node.child1()) {
            dump(out, node.child1());
            hasPrinted = true;
        }
        if (!!node.child2()) {
            out.print(", "); // Whether or not there is a first child, we print a comma to ensure that we see a blank entry if there wasn't one.
            dump(out, node.child2());
            hasPrinted = true;
        }
        if (!!node.child3()) {
            if (!node.child1() && !node.child2())
                out.print(", "); // If the third child is the first non-empty one then make sure we have two blanks preceding it.
            out.print(", ");
            dump(out, node.child3());
            hasPrinted = true;
        }
    }

    if (strlen(nodeFlagsAsString(node.flags()))) {
        out.print(hasPrinted ? ", " : "", nodeFlagsAsString(node.flags()));
        hasPrinted = true;
    }
    if (node.hasArrayMode()) {
        out.print(hasPrinted ? ", " : "", node.arrayMode());
        hasPrinted = true;
    }
    if (node.hasVarNumber()) {
        out.print(hasPrinted ? ", " : "", "var", node.varNumber());
        hasPrinted = true;
    }
    if (node.hasRegisterPointer()) {
        out.print(hasPrinted ? ", " : "", "global", globalObjectFor(node.codeOrigin)->findRegisterIndex(node.registerPointer()), "(", RawPointer(node.registerPointer()), ")");
        hasPrinted = true;
    }
    if (node.hasIdentifier()) {
        out.print(hasPrinted ? ", " : "", "id", node.identifierNumber(), "{", m_codeBlock->identifier(node.identifierNumber()).string(), "}");
        hasPrinted = true;
    }
    if (node.hasStructureSet()) {
        for (size_t i = 0; i < node.structureSet().size(); ++i) {
            out.print(hasPrinted ? ", " : "", "struct(", RawPointer(node.structureSet()[i]), ": ", IndexingTypeDump(node.structureSet()[i]->indexingType()), ")");
            hasPrinted = true;
        }
    }
    if (node.hasStructure()) {
        out.print(hasPrinted ? ", " : "", "struct(", RawPointer(node.structure()), ": ", IndexingTypeDump(node.structure()->indexingType()), ")");
        hasPrinted = true;
    }
    if (node.hasStructureTransitionData()) {
        out.print(hasPrinted ? ", " : "", "struct(", RawPointer(node.structureTransitionData().previousStructure), " -> ", RawPointer(node.structureTransitionData().newStructure), ")");
        hasPrinted = true;
    }
    if (node.hasFunction()) {
        out.print(hasPrinted ? ", " : "", RawPointer(node.function()));
        hasPrinted = true;
    }
    if (node.hasStorageAccessData()) {
        StorageAccessData& storageAccessData = m_storageAccessData[node.storageAccessDataIndex()];
        out.print(hasPrinted ? ", " : "", "id", storageAccessData.identifierNumber, "{", m_codeBlock->identifier(storageAccessData.identifierNumber).string(), "}");
        out.print(", ", static_cast<ptrdiff_t>(storageAccessData.offset));
        hasPrinted = true;
    }
    ASSERT(node.hasVariableAccessData() == node.hasLocal());
    if (node.hasVariableAccessData()) {
        VariableAccessData* variableAccessData = node.variableAccessData();
        int operand = variableAccessData->operand();
        if (operandIsArgument(operand))
            out.print(hasPrinted ? ", " : "", "arg", operandToArgument(operand), "(", VariableAccessDataDump(*this, variableAccessData), ")");
        else
            out.print(hasPrinted ? ", " : "", "r", operand, "(", VariableAccessDataDump(*this, variableAccessData), ")");
        hasPrinted = true;
    }
    if (node.hasConstantBuffer()) {
        if (hasPrinted)
            out.print(", ");
        out.print(node.startConstant(), ":[");
        for (unsigned i = 0; i < node.numConstants(); ++i) {
            if (i)
                out.print(", ");
            out.print(m_codeBlock->constantBuffer(node.startConstant())[i]);
        }
        out.print("]");
        hasPrinted = true;
    }
    if (node.hasIndexingType()) {
        if (hasPrinted)
            out.print(", ");
        out.print(IndexingTypeDump(node.indexingType()));
        hasPrinted = true;
    }
    if (node.hasExecutionCounter()) {
        if (hasPrinted)
            out.print(", ");
        out.print(RawPointer(node.executionCounter()));
        hasPrinted = true;
    }
    if (op == JSConstant) {
        out.print(hasPrinted ? ", " : "", "$", node.constantNumber());
        JSValue value = valueOfJSConstant(nodeIndex);
        out.print(" = ", value);
        hasPrinted = true;
    }
    if (op == WeakJSConstant) {
        out.print(hasPrinted ? ", " : "", RawPointer(node.weakConstant()));
        hasPrinted = true;
    }
    if  (node.isBranch() || node.isJump()) {
        out.print(hasPrinted ? ", " : "", "T:#", node.takenBlockIndex());
        hasPrinted = true;
    }
    if  (node.isBranch()) {
        out.print(hasPrinted ? ", " : "", "F:#", node.notTakenBlockIndex());
        hasPrinted = true;
    }
    out.print(hasPrinted ? ", " : "", "bc#", node.codeOrigin.bytecodeIndex);
    hasPrinted = true;
    
    (void)hasPrinted;
    
    out.print(")");

    if (!skipped) {
        if (node.hasVariableAccessData())
            out.print("  predicting ", SpeculationDump(node.variableAccessData()->prediction()), node.variableAccessData()->shouldUseDoubleFormat() ? ", forcing double" : "");
        else if (node.hasHeapPrediction())
            out.print("  predicting ", SpeculationDump(node.getHeapPrediction()));
    }
    
    out.print("\n");
}

void Graph::dumpBlockHeader(PrintStream& out, const char* prefix, BlockIndex blockIndex, PhiNodeDumpMode phiNodeDumpMode)
{
    BasicBlock* block = m_blocks[blockIndex].get();

    out.print(prefix, "Block #", blockIndex, " (", at(block->at(0)).codeOrigin, "): ", block->isReachable ? "" : "(skipped)", block->isOSRTarget ? " (OSR target)" : "", "\n");
    out.print(prefix, "  Predecessors:");
    for (size_t i = 0; i < block->m_predecessors.size(); ++i)
        out.print(" #", block->m_predecessors[i]);
    out.print("\n");
    if (m_dominators.isValid()) {
        out.print(prefix, "  Dominated by:");
        for (size_t i = 0; i < m_blocks.size(); ++i) {
            if (!m_dominators.dominates(i, blockIndex))
                continue;
            out.print(" #", i);
        }
        out.print("\n");
        out.print(prefix, "  Dominates:");
        for (size_t i = 0; i < m_blocks.size(); ++i) {
            if (!m_dominators.dominates(blockIndex, i))
                continue;
            out.print(" #", i);
        }
        out.print("\n");
    }
    out.print(prefix, "  Phi Nodes:");
    for (size_t i = 0; i < block->phis.size(); ++i) {
        NodeIndex phiNodeIndex = block->phis[i];
        Node& phiNode = at(phiNodeIndex);
        if (!phiNode.shouldGenerate() && phiNodeDumpMode == DumpLivePhisOnly)
            continue;
        out.print(" @", phiNodeIndex, "<", phiNode.refCount(), ">->(");
        if (phiNode.child1()) {
            out.print("@", phiNode.child1().index());
            if (phiNode.child2()) {
                out.print(", @", phiNode.child2().index());
                if (phiNode.child3())
                    out.print(", @", phiNode.child3().index());
            }
        }
        out.print(")", i + 1 < block->phis.size() ? "," : "");
    }
    out.print("\n");
}

void Graph::dump(PrintStream& out)
{
    NodeIndex lastNodeIndex = NoNode;
    for (size_t b = 0; b < m_blocks.size(); ++b) {
        BasicBlock* block = m_blocks[b].get();
        if (!block)
            continue;
        dumpBlockHeader(out, "", b, DumpAllPhis);
        out.print("  vars before: ");
        if (block->cfaHasVisited)
            dumpOperands(block->valuesAtHead, out);
        else
            out.print("<empty>");
        out.print("\n");
        out.print("  var links: ");
        dumpOperands(block->variablesAtHead, out);
        out.print("\n");
        for (size_t i = 0; i < block->size(); ++i) {
            dumpCodeOrigin(out, "", lastNodeIndex, block->at(i));
            dump(out, "", block->at(i));
            lastNodeIndex = block->at(i);
        }
        out.print("  vars after: ");
        if (block->cfaHasVisited)
            dumpOperands(block->valuesAtTail, out);
        else
            out.print("<empty>");
        out.print("\n");
        out.print("  var links: ");
        dumpOperands(block->variablesAtTail, out);
        out.print("\n");
    }
}

// FIXME: Convert this to be iterative, not recursive.
#define DO_TO_CHILDREN(node, thingToDo) do {                            \
        Node& _node = (node);                                           \
        if (_node.flags() & NodeHasVarArgs) {                           \
            for (unsigned _childIdx = _node.firstChild();               \
                 _childIdx < _node.firstChild() + _node.numChildren();  \
                 _childIdx++) {                                         \
                if (!!m_varArgChildren[_childIdx])                      \
                    thingToDo(m_varArgChildren[_childIdx]);             \
            }                                                           \
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
        dataLog(
            "Argument [", arg, "] prediction: ",
            SpeculationDump(at(m_arguments[arg]).variableAccessData()->prediction()), "\n");
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
                if (!m_varArgChildren[childIdx])
                    continue;
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
