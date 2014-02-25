/*
 * Copyright (C) 2011, 2013, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "BytecodeLivenessAnalysisInlines.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGClobberSet.h"
#include "DFGJITCode.h"
#include "DFGVariableAccessDataDump.h"
#include "FullBytecodeLiveness.h"
#include "FunctionExecutableDump.h"
#include "JIT.h"
#include "JSActivation.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "OperandsInlines.h"
#include "JSCInlines.h"
#include "StackAlignment.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>

namespace JSC { namespace DFG {

// Creates an array of stringized names.
static const char* dfgOpNames[] = {
#define STRINGIZE_DFG_OP_ENUM(opcode, flags) #opcode ,
    FOR_EACH_DFG_OP(STRINGIZE_DFG_OP_ENUM)
#undef STRINGIZE_DFG_OP_ENUM
};

Graph::Graph(VM& vm, Plan& plan, LongLivedState& longLivedState)
    : m_vm(vm)
    , m_plan(plan)
    , m_codeBlock(m_plan.codeBlock.get())
    , m_profiledBlock(m_codeBlock->alternative())
    , m_allocator(longLivedState.m_allocator)
    , m_mustHandleAbstractValues(OperandsLike, plan.mustHandleValues)
    , m_inlineCallFrames(adoptPtr(new InlineCallFrameSet()))
    , m_hasArguments(false)
    , m_nextMachineLocal(0)
    , m_machineCaptureStart(std::numeric_limits<int>::max())
    , m_fixpointState(BeforeFixpoint)
    , m_form(LoadStore)
    , m_unificationState(LocallyUnified)
    , m_refCountState(EverythingIsLive)
{
    ASSERT(m_profiledBlock);
    
    for (unsigned i = m_mustHandleAbstractValues.size(); i--;)
        m_mustHandleAbstractValues[i].setMostSpecific(*this, plan.mustHandleValues[i]);
}

Graph::~Graph()
{
    m_allocator.freeAll();
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

bool Graph::dumpCodeOrigin(PrintStream& out, const char* prefix, Node* previousNode, Node* currentNode, DumpContext* context)
{
    if (!previousNode)
        return false;
    
    if (previousNode->origin.semantic.inlineCallFrame == currentNode->origin.semantic.inlineCallFrame)
        return false;
    
    Vector<CodeOrigin> previousInlineStack = previousNode->origin.semantic.inlineStack();
    Vector<CodeOrigin> currentInlineStack = currentNode->origin.semantic.inlineStack();
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
        out.print("<-- ", inContext(*previousInlineStack[i].inlineCallFrame, context), "\n");
        hasPrinted = true;
    }
    
    // Print the pushes.
    for (unsigned i = indexOfDivergence; i < currentInlineStack.size(); ++i) {
        out.print(prefix);
        printWhiteSpace(out, i * 2);
        out.print("--> ", inContext(*currentInlineStack[i].inlineCallFrame, context), "\n");
        hasPrinted = true;
    }
    
    return hasPrinted;
}

int Graph::amountOfNodeWhiteSpace(Node* node)
{
    return (node->origin.semantic.inlineDepth() - 1) * 2;
}

void Graph::printNodeWhiteSpace(PrintStream& out, Node* node)
{
    printWhiteSpace(out, amountOfNodeWhiteSpace(node));
}

void Graph::dump(PrintStream& out, const char* prefix, Node* node, DumpContext* context)
{
    NodeType op = node->op();

    unsigned refCount = node->refCount();
    bool skipped = !refCount;
    bool mustGenerate = node->mustGenerate();
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
    out.printf("% 4d:%s<%c%u:", (int)node->index(), skipped ? "  skipped  " : "           ", mustGenerate ? '!' : ' ', refCount);
    if (node->hasResult() && !skipped && node->hasVirtualRegister())
        out.print(node->virtualRegister());
    else
        out.print("-");
    out.print(">\t", opName(op), "(");
    CommaPrinter comma;
    if (node->flags() & NodeHasVarArgs) {
        for (unsigned childIdx = node->firstChild(); childIdx < node->firstChild() + node->numChildren(); childIdx++) {
            if (!m_varArgChildren[childIdx])
                continue;
            out.print(comma, m_varArgChildren[childIdx]);
        }
    } else {
        if (!!node->child1() || !!node->child2() || !!node->child3())
            out.print(comma, node->child1());
        if (!!node->child2() || !!node->child3())
            out.print(comma, node->child2());
        if (!!node->child3())
            out.print(comma, node->child3());
    }

    if (toCString(NodeFlagsDump(node->flags())) != "<empty>")
        out.print(comma, NodeFlagsDump(node->flags()));
    if (node->prediction())
        out.print(comma, SpeculationDump(node->prediction()));
    if (node->hasArrayMode())
        out.print(comma, node->arrayMode());
    if (node->hasArithMode())
        out.print(comma, node->arithMode());
    if (node->hasVarNumber())
        out.print(comma, node->varNumber());
    if (node->hasRegisterPointer())
        out.print(comma, "global", globalObjectFor(node->origin.semantic)->findRegisterIndex(node->registerPointer()), "(", RawPointer(node->registerPointer()), ")");
    if (node->hasIdentifier())
        out.print(comma, "id", node->identifierNumber(), "{", identifiers()[node->identifierNumber()], "}");
    if (node->hasStructureSet())
        out.print(comma, inContext(node->structureSet(), context));
    if (node->hasStructure())
        out.print(comma, inContext(*node->structure(), context));
    if (node->hasStructureTransitionData())
        out.print(comma, inContext(*node->structureTransitionData().previousStructure, context), " -> ", inContext(*node->structureTransitionData().newStructure, context));
    if (node->hasFunction()) {
        out.print(comma, "function(", RawPointer(node->function()), ", ");
        if (node->function()->inherits(JSFunction::info())) {
            JSFunction* function = jsCast<JSFunction*>(node->function());
            if (function->isHostFunction())
                out.print("<host function>");
            else
                out.print(FunctionExecutableDump(function->jsExecutable()));
        } else
            out.print("<not JSFunction>");
        out.print(")");
    }
    if (node->hasExecutable()) {
        if (node->executable()->inherits(FunctionExecutable::info()))
            out.print(comma, "executable(", FunctionExecutableDump(jsCast<FunctionExecutable*>(node->executable())), ")");
        else
            out.print(comma, "executable(not function: ", RawPointer(node->executable()), ")");
    }
    if (node->hasFunctionDeclIndex()) {
        FunctionExecutable* executable = m_codeBlock->functionDecl(node->functionDeclIndex());
        out.print(comma, FunctionExecutableDump(executable));
    }
    if (node->hasFunctionExprIndex()) {
        FunctionExecutable* executable = m_codeBlock->functionExpr(node->functionExprIndex());
        out.print(comma, FunctionExecutableDump(executable));
    }
    if (node->hasStorageAccessData()) {
        StorageAccessData& storageAccessData = m_storageAccessData[node->storageAccessDataIndex()];
        out.print(comma, "id", storageAccessData.identifierNumber, "{", identifiers()[storageAccessData.identifierNumber], "}");
        out.print(", ", static_cast<ptrdiff_t>(storageAccessData.offset));
    }
    if (node->hasMultiGetByOffsetData()) {
        MultiGetByOffsetData& data = node->multiGetByOffsetData();
        out.print(comma, "id", data.identifierNumber, "{", identifiers()[data.identifierNumber], "}");
        for (unsigned i = 0; i < data.variants.size(); ++i)
            out.print(comma, inContext(data.variants[i], context));
    }
    if (node->hasMultiPutByOffsetData()) {
        MultiPutByOffsetData& data = node->multiPutByOffsetData();
        out.print(comma, "id", data.identifierNumber, "{", identifiers()[data.identifierNumber], "}");
        for (unsigned i = 0; i < data.variants.size(); ++i)
            out.print(comma, inContext(data.variants[i], context));
    }
    ASSERT(node->hasVariableAccessData(*this) == node->hasLocal(*this));
    if (node->hasVariableAccessData(*this)) {
        VariableAccessData* variableAccessData = node->tryGetVariableAccessData();
        if (variableAccessData) {
            VirtualRegister operand = variableAccessData->local();
            if (operand.isArgument())
                out.print(comma, "arg", operand.toArgument(), "(", VariableAccessDataDump(*this, variableAccessData), ")");
            else
                out.print(comma, "loc", operand.toLocal(), "(", VariableAccessDataDump(*this, variableAccessData), ")");
            
            operand = variableAccessData->machineLocal();
            if (operand.isValid()) {
                if (operand.isArgument())
                    out.print(comma, "machine:arg", operand.toArgument());
                else
                    out.print(comma, "machine:loc", operand.toLocal());
            }
        }
    }
    if (node->hasUnlinkedLocal()) {
        VirtualRegister operand = node->unlinkedLocal();
        if (operand.isArgument())
            out.print(comma, "arg", operand.toArgument());
        else
            out.print(comma, "loc", operand.toLocal());
    }
    if (node->hasUnlinkedMachineLocal()) {
        VirtualRegister operand = node->unlinkedMachineLocal();
        if (operand.isValid()) {
            if (operand.isArgument())
                out.print(comma, "machine:arg", operand.toArgument());
            else
                out.print(comma, "machine:loc", operand.toLocal());
        }
    }
    if (node->hasConstantBuffer()) {
        out.print(comma);
        out.print(node->startConstant(), ":[");
        CommaPrinter anotherComma;
        for (unsigned i = 0; i < node->numConstants(); ++i)
            out.print(anotherComma, inContext(m_codeBlock->constantBuffer(node->startConstant())[i], context));
        out.print("]");
    }
    if (node->hasIndexingType())
        out.print(comma, IndexingTypeDump(node->indexingType()));
    if (node->hasTypedArrayType())
        out.print(comma, node->typedArrayType());
    if (node->hasPhi())
        out.print(comma, "^", node->phi()->index());
    if (node->hasExecutionCounter())
        out.print(comma, RawPointer(node->executionCounter()));
    if (node->hasVariableWatchpointSet())
        out.print(comma, RawPointer(node->variableWatchpointSet()));
    if (node->hasTypedArray())
        out.print(comma, inContext(JSValue(node->typedArray()), context));
    if (node->hasStoragePointer())
        out.print(comma, RawPointer(node->storagePointer()));
    if (op == JSConstant) {
        out.print(comma, "$", node->constantNumber());
        JSValue value = valueOfJSConstant(node);
        out.print(" = ", inContext(value, context));
    }
    if (op == WeakJSConstant)
        out.print(comma, RawPointer(node->weakConstant()), " (", inContext(*node->weakConstant()->structure(), context), ")");
    if (node->isJump())
        out.print(comma, "T:", *node->targetBlock());
    if (node->isBranch())
        out.print(comma, "T:", node->branchData()->taken, ", F:", node->branchData()->notTaken);
    if (node->isSwitch()) {
        SwitchData* data = node->switchData();
        out.print(comma, data->kind);
        for (unsigned i = 0; i < data->cases.size(); ++i)
            out.print(comma, inContext(data->cases[i].value, context), ":", data->cases[i].target);
        out.print(comma, "default:", data->fallThrough);
    }
    ClobberSet reads;
    ClobberSet writes;
    addReadsAndWrites(*this, node, reads, writes);
    if (!reads.isEmpty())
        out.print(comma, "R:", sortedListDump(reads.direct(), ","));
    if (!writes.isEmpty())
        out.print(comma, "W:", sortedListDump(writes.direct(), ","));
    out.print(comma, "bc#", node->origin.semantic.bytecodeIndex);
    if (node->origin.semantic != node->origin.forExit)
        out.print(comma, "exit: ", node->origin.forExit);
    
    out.print(")");

    if (!skipped) {
        if (node->hasVariableAccessData(*this) && node->tryGetVariableAccessData())
            out.print("  predicting ", SpeculationDump(node->tryGetVariableAccessData()->prediction()));
        else if (node->hasHeapPrediction())
            out.print("  predicting ", SpeculationDump(node->getHeapPrediction()));
    }
    
    out.print("\n");
}

void Graph::dumpBlockHeader(PrintStream& out, const char* prefix, BasicBlock* block, PhiNodeDumpMode phiNodeDumpMode, DumpContext* context)
{
    out.print(prefix, "Block ", *block, " (", inContext(block->at(0)->origin.semantic, context), "): ", block->isReachable ? "" : "(skipped)", block->isOSRTarget ? " (OSR target)" : "", "\n");
    out.print(prefix, "  Predecessors:");
    for (size_t i = 0; i < block->predecessors.size(); ++i)
        out.print(" ", *block->predecessors[i]);
    out.print("\n");
    if (m_dominators.isValid()) {
        out.print(prefix, "  Dominated by:");
        for (size_t i = 0; i < m_blocks.size(); ++i) {
            if (!m_dominators.dominates(i, block->index))
                continue;
            out.print(" #", i);
        }
        out.print("\n");
        out.print(prefix, "  Dominates:");
        for (size_t i = 0; i < m_blocks.size(); ++i) {
            if (!m_dominators.dominates(block->index, i))
                continue;
            out.print(" #", i);
        }
        out.print("\n");
    }
    if (m_naturalLoops.isValid()) {
        if (const NaturalLoop* loop = m_naturalLoops.headerOf(block)) {
            out.print(prefix, "  Loop header, contains:");
            Vector<BlockIndex> sortedBlockList;
            for (unsigned i = 0; i < loop->size(); ++i)
                sortedBlockList.append(loop->at(i)->index);
            std::sort(sortedBlockList.begin(), sortedBlockList.end());
            for (unsigned i = 0; i < sortedBlockList.size(); ++i)
                out.print(" #", sortedBlockList[i]);
            out.print("\n");
        }
        
        Vector<const NaturalLoop*> containingLoops =
            m_naturalLoops.loopsOf(block);
        if (!containingLoops.isEmpty()) {
            out.print(prefix, "  Containing loop headers:");
            for (unsigned i = 0; i < containingLoops.size(); ++i)
                out.print(" ", *containingLoops[i]->header());
            out.print("\n");
        }
    }
    if (!block->phis.isEmpty()) {
        out.print(prefix, "  Phi Nodes:");
        for (size_t i = 0; i < block->phis.size(); ++i) {
            Node* phiNode = block->phis[i];
            if (!phiNode->shouldGenerate() && phiNodeDumpMode == DumpLivePhisOnly)
                continue;
            out.print(" @", phiNode->index(), "<", phiNode->refCount(), ">->(");
            if (phiNode->child1()) {
                out.print("@", phiNode->child1()->index());
                if (phiNode->child2()) {
                    out.print(", @", phiNode->child2()->index());
                    if (phiNode->child3())
                        out.print(", @", phiNode->child3()->index());
                }
            }
            out.print(")", i + 1 < block->phis.size() ? "," : "");
        }
        out.print("\n");
    }
}

void Graph::dump(PrintStream& out, DumpContext* context)
{
    DumpContext myContext;
    myContext.graph = this;
    if (!context)
        context = &myContext;
    
    dataLog("\n");
    dataLog("DFG for ", CodeBlockWithJITType(m_codeBlock, JITCode::DFGJIT), ":\n");
    dataLog("  Fixpoint state: ", m_fixpointState, "; Form: ", m_form, "; Unification state: ", m_unificationState, "; Ref count state: ", m_refCountState, "\n");
    dataLog("\n");
    
    Node* lastNode = 0;
    for (size_t b = 0; b < m_blocks.size(); ++b) {
        BasicBlock* block = m_blocks[b].get();
        if (!block)
            continue;
        dumpBlockHeader(out, "", block, DumpAllPhis, context);
        switch (m_form) {
        case LoadStore:
        case ThreadedCPS: {
            out.print("  vars before: ");
            if (block->cfaHasVisited)
                out.print(inContext(block->valuesAtHead, context));
            else
                out.print("<empty>");
            out.print("\n");
            out.print("  var links: ", block->variablesAtHead, "\n");
            break;
        }
            
        case SSA: {
            RELEASE_ASSERT(block->ssa);
            out.print("  Flush format: ", block->ssa->flushAtHead, "\n");
            out.print("  Availability: ", block->ssa->availabilityAtHead, "\n");
            out.print("  Live: ", nodeListDump(block->ssa->liveAtHead), "\n");
            out.print("  Values: ", nodeMapDump(block->ssa->valuesAtHead, context), "\n");
            break;
        } }
        for (size_t i = 0; i < block->size(); ++i) {
            dumpCodeOrigin(out, "", lastNode, block->at(i), context);
            dump(out, "", block->at(i), context);
            lastNode = block->at(i);
        }
        switch (m_form) {
        case LoadStore:
        case ThreadedCPS: {
            out.print("  vars after: ");
            if (block->cfaHasVisited)
                out.print(inContext(block->valuesAtTail, context));
            else
                out.print("<empty>");
            out.print("\n");
            out.print("  var links: ", block->variablesAtTail, "\n");
            break;
        }
            
        case SSA: {
            RELEASE_ASSERT(block->ssa);
            out.print("  Flush format: ", block->ssa->flushAtTail, "\n");
            out.print("  Availability: ", block->ssa->availabilityAtTail, "\n");
            out.print("  Live: ", nodeListDump(block->ssa->liveAtTail), "\n");
            out.print("  Values: ", nodeMapDump(block->ssa->valuesAtTail, context), "\n");
            break;
        } }
        dataLog("\n");
    }
    
    if (!myContext.isEmpty()) {
        myContext.dump(WTF::dataFile());
        dataLog("\n");
    }
}

void Graph::dethread()
{
    if (m_form == LoadStore || m_form == SSA)
        return;
    
    if (logCompilationChanges())
        dataLog("Dethreading DFG graph.\n");
    
    SamplingRegion samplingRegion("DFG Dethreading");
    
    for (BlockIndex blockIndex = m_blocks.size(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned phiIndex = block->phis.size(); phiIndex--;) {
            Node* phi = block->phis[phiIndex];
            phi->children.reset();
        }
    }
    
    m_form = LoadStore;
}

void Graph::handleSuccessor(Vector<BasicBlock*, 16>& worklist, BasicBlock* block, BasicBlock* successor)
{
    if (!successor->isReachable) {
        successor->isReachable = true;
        worklist.append(successor);
    }
    
    successor->predecessors.append(block);
}

void Graph::determineReachability()
{
    Vector<BasicBlock*, 16> worklist;
    worklist.append(block(0));
    block(0)->isReachable = true;
    while (!worklist.isEmpty()) {
        BasicBlock* block = worklist.takeLast();
        for (unsigned i = block->numSuccessors(); i--;)
            handleSuccessor(worklist, block, block->successor(i));
    }
}

void Graph::resetReachability()
{
    for (BlockIndex blockIndex = m_blocks.size(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        block->isReachable = false;
        block->predecessors.clear();
    }
    
    determineReachability();
}

void Graph::killBlockAndItsContents(BasicBlock* block)
{
    for (unsigned phiIndex = block->phis.size(); phiIndex--;)
        m_allocator.free(block->phis[phiIndex]);
    for (unsigned nodeIndex = block->size(); nodeIndex--;)
        m_allocator.free(block->at(nodeIndex));
    
    killBlock(block);
}

void Graph::killUnreachableBlocks()
{
    for (BlockIndex blockIndex = 0; blockIndex < numBlocks(); ++blockIndex) {
        BasicBlock* block = this->block(blockIndex);
        if (!block)
            continue;
        if (block->isReachable)
            continue;
        
        killBlockAndItsContents(block);
    }
}

void Graph::resetExitStates()
{
    for (BlockIndex blockIndex = 0; blockIndex < m_blocks.size(); ++blockIndex) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned indexInBlock = block->size(); indexInBlock--;)
            block->at(indexInBlock)->setCanExit(true);
    }
}

void Graph::invalidateCFG()
{
    m_dominators.invalidate();
    m_naturalLoops.invalidate();
}

void Graph::substituteGetLocal(BasicBlock& block, unsigned startIndexInBlock, VariableAccessData* variableAccessData, Node* newGetLocal)
{
    if (variableAccessData->isCaptured()) {
        // Let CSE worry about this one.
        return;
    }
    for (unsigned indexInBlock = startIndexInBlock; indexInBlock < block.size(); ++indexInBlock) {
        Node* node = block[indexInBlock];
        bool shouldContinue = true;
        switch (node->op()) {
        case SetLocal: {
            if (node->local() == variableAccessData->local())
                shouldContinue = false;
            break;
        }
                
        case GetLocal: {
            if (node->variableAccessData() != variableAccessData)
                continue;
            substitute(block, indexInBlock, node, newGetLocal);
            Node* oldTailNode = block.variablesAtTail.operand(variableAccessData->local());
            if (oldTailNode == node)
                block.variablesAtTail.operand(variableAccessData->local()) = newGetLocal;
            shouldContinue = false;
            break;
        }
                
        default:
            break;
        }
        if (!shouldContinue)
            break;
    }
}

void Graph::addForDepthFirstSort(Vector<BasicBlock*>& result, Vector<BasicBlock*, 16>& worklist, HashSet<BasicBlock*>& seen, BasicBlock* block)
{
    if (seen.contains(block))
        return;
    
    result.append(block);
    worklist.append(block);
    seen.add(block);
}

void Graph::getBlocksInDepthFirstOrder(Vector<BasicBlock*>& result)
{
    Vector<BasicBlock*, 16> worklist;
    HashSet<BasicBlock*> seen;
    addForDepthFirstSort(result, worklist, seen, block(0));
    while (!worklist.isEmpty()) {
        BasicBlock* block = worklist.takeLast();
        for (unsigned i = block->numSuccessors(); i--;)
            addForDepthFirstSort(result, worklist, seen, block->successor(i));
    }
}

void Graph::clearReplacements()
{
    for (BlockIndex blockIndex = numBlocks(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned phiIndex = block->phis.size(); phiIndex--;)
            block->phis[phiIndex]->misc.replacement = 0;
        for (unsigned nodeIndex = block->size(); nodeIndex--;)
            block->at(nodeIndex)->misc.replacement = 0;
    }
}

void Graph::initializeNodeOwners()
{
    for (BlockIndex blockIndex = numBlocks(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned phiIndex = block->phis.size(); phiIndex--;)
            block->phis[phiIndex]->misc.owner = block;
        for (unsigned nodeIndex = block->size(); nodeIndex--;)
            block->at(nodeIndex)->misc.owner = block;
    }
}

FullBytecodeLiveness& Graph::livenessFor(CodeBlock* codeBlock)
{
    HashMap<CodeBlock*, std::unique_ptr<FullBytecodeLiveness>>::iterator iter = m_bytecodeLiveness.find(codeBlock);
    if (iter != m_bytecodeLiveness.end())
        return *iter->value;
    
    std::unique_ptr<FullBytecodeLiveness> liveness = std::make_unique<FullBytecodeLiveness>();
    codeBlock->livenessAnalysis().computeFullLiveness(*liveness);
    FullBytecodeLiveness& result = *liveness;
    m_bytecodeLiveness.add(codeBlock, std::move(liveness));
    return result;
}

FullBytecodeLiveness& Graph::livenessFor(InlineCallFrame* inlineCallFrame)
{
    return livenessFor(baselineCodeBlockFor(inlineCallFrame));
}

bool Graph::isLiveInBytecode(VirtualRegister operand, CodeOrigin codeOrigin)
{
    for (;;) {
        VirtualRegister reg = VirtualRegister(
            operand.offset() - codeOrigin.stackOffset());
        
        if (operand.offset() < codeOrigin.stackOffset() + JSStack::CallFrameHeaderSize) {
            if (reg.isArgument()) {
                RELEASE_ASSERT(reg.offset() < JSStack::CallFrameHeaderSize);
                
                if (!codeOrigin.inlineCallFrame->isClosureCall)
                    return false;
                
                if (reg.offset() == JSStack::Callee)
                    return true;
                if (reg.offset() == JSStack::ScopeChain)
                    return true;
                
                return false;
            }
            
            return livenessFor(codeOrigin.inlineCallFrame).operandIsLive(
                reg.offset(), codeOrigin.bytecodeIndex);
        }
        
        InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame;
        if (!inlineCallFrame)
            break;

        // Arguments are always live. This would be redundant if it wasn't for our
        // op_call_varargs inlining.
        // FIXME: 'this' might not be live, but we don't have a way of knowing.
        // https://bugs.webkit.org/show_bug.cgi?id=128519
        if (reg.isArgument()
            && static_cast<size_t>(reg.toArgument()) < inlineCallFrame->arguments.size())
            return true;
        
        codeOrigin = inlineCallFrame->caller;
    }
    
    return true;
}

unsigned Graph::frameRegisterCount()
{
    unsigned result = m_nextMachineLocal + std::max(m_parameterSlots, static_cast<unsigned>(maxFrameExtentForSlowPathCallInRegisters));
    return roundLocalRegisterCountForFramePointerOffset(result);
}

unsigned Graph::stackPointerOffset()
{
    return virtualRegisterForLocal(frameRegisterCount() - 1).offset();
}

unsigned Graph::requiredRegisterCountForExit()
{
    unsigned count = JIT::frameRegisterCountFor(m_profiledBlock);
    for (InlineCallFrameSet::iterator iter = m_inlineCallFrames->begin(); !!iter; ++iter) {
        InlineCallFrame* inlineCallFrame = *iter;
        CodeBlock* codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
        unsigned requiredCount = VirtualRegister(inlineCallFrame->stackOffset).toLocal() + 1 + JIT::frameRegisterCountFor(codeBlock);
        count = std::max(count, requiredCount);
    }
    return count;
}

unsigned Graph::requiredRegisterCountForExecutionAndExit()
{
    return std::max(frameRegisterCount(), requiredRegisterCountForExit());
}

JSActivation* Graph::tryGetActivation(Node* node)
{
    if (!node->hasConstant())
        return 0;
    return jsDynamicCast<JSActivation*>(valueOfJSConstant(node));
}

WriteBarrierBase<Unknown>* Graph::tryGetRegisters(Node* node)
{
    JSActivation* activation = tryGetActivation(node);
    if (!activation)
        return 0;
    if (!activation->isTornOff())
        return 0;
    return activation->registers();
}

JSArrayBufferView* Graph::tryGetFoldableView(Node* node)
{
    if (!node->hasConstant())
        return 0;
    JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(valueOfJSConstant(node));
    if (!view)
        return 0;
    if (!watchpoints().isStillValid(view))
        return 0;
    return view;
}

JSArrayBufferView* Graph::tryGetFoldableView(Node* node, ArrayMode arrayMode)
{
    if (arrayMode.typedArrayType() == NotTypedArray)
        return 0;
    return tryGetFoldableView(node);
}

JSArrayBufferView* Graph::tryGetFoldableViewForChild1(Node* node)
{
    return tryGetFoldableView(child(node, 0).node(), node->arrayMode());
}

void Graph::visitChildren(SlotVisitor& visitor)
{
    for (BlockIndex blockIndex = numBlocks(); blockIndex--;) {
        BasicBlock* block = this->block(blockIndex);
        if (!block)
            continue;
        
        for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
            Node* node = block->at(nodeIndex);
            
            switch (node->op()) {
            case JSConstant:
            case WeakJSConstant:
                visitor.appendUnbarrieredReadOnlyValue(valueOfJSConstant(node));
                break;
                
            case CheckFunction:
                visitor.appendUnbarrieredReadOnlyPointer(node->function());
                break;
                
            case CheckExecutable:
                visitor.appendUnbarrieredReadOnlyPointer(node->executable());
                break;
                
            case CheckStructure:
                for (unsigned i = node->structureSet().size(); i--;)
                    visitor.appendUnbarrieredReadOnlyPointer(node->structureSet()[i]);
                break;
                
            case StructureTransitionWatchpoint:
            case NewObject:
            case ArrayifyToStructure:
            case NewStringObject:
                visitor.appendUnbarrieredReadOnlyPointer(node->structure());
                break;
                
            case PutStructure:
            case PhantomPutStructure:
            case AllocatePropertyStorage:
            case ReallocatePropertyStorage:
                visitor.appendUnbarrieredReadOnlyPointer(
                    node->structureTransitionData().previousStructure);
                visitor.appendUnbarrieredReadOnlyPointer(
                    node->structureTransitionData().newStructure);
                break;
                
            default:
                break;
            }
        }
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
