/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#include "ArrayPrototype.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGBackwardsCFG.h"
#include "DFGBackwardsDominators.h"
#include "DFGBlockWorklist.h"
#include "DFGCFG.h"
#include "DFGClobberSet.h"
#include "DFGClobbersExitState.h"
#include "DFGControlEquivalenceAnalysis.h"
#include "DFGDominators.h"
#include "DFGFlowIndexing.h"
#include "DFGFlowMap.h"
#include "DFGMayExit.h"
#include "DFGNaturalLoops.h"
#include "DFGVariableAccessDataDump.h"
#include "FullBytecodeLiveness.h"
#include "FunctionExecutableDump.h"
#include "GetterSetter.h"
#include "JIT.h"
#include "JSLexicalEnvironment.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "OperandsInlines.h"
#include "Snippet.h"
#include "StackAlignment.h"
#include "StructureInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>

namespace JSC { namespace DFG {

static constexpr bool dumpOSRAvailabilityData = false;

// Creates an array of stringized names.
static const char* dfgOpNames[] = {
#define STRINGIZE_DFG_OP_ENUM(opcode, flags) #opcode ,
    FOR_EACH_DFG_OP(STRINGIZE_DFG_OP_ENUM)
#undef STRINGIZE_DFG_OP_ENUM
};

Graph::Graph(VM& vm, Plan& plan)
    : m_vm(vm)
    , m_plan(plan)
    , m_codeBlock(m_plan.codeBlock())
    , m_profiledBlock(m_codeBlock->alternative())
    , m_ssaCFG(makeUnique<SSACFG>(*this))
    , m_nextMachineLocal(0)
    , m_fixpointState(BeforeFixpoint)
    , m_structureRegistrationState(HaveNotStartedRegistering)
    , m_form(LoadStore)
    , m_unificationState(LocallyUnified)
    , m_refCountState(EverythingIsLive)
{
    ASSERT(m_profiledBlock);
    
    m_hasDebuggerEnabled = m_profiledBlock->wasCompiledWithDebuggingOpcodes() || Options::forceDebuggerBytecodeGeneration();
    
    m_indexingCache = makeUnique<FlowIndexing>(*this);
    m_abstractValuesCache = makeUnique<FlowMap<AbstractValue>>(*this);

    registerStructure(vm.structureStructure.get());
    this->stringStructure = registerStructure(vm.stringStructure.get());
    this->symbolStructure = registerStructure(vm.symbolStructure.get());
}

Graph::~Graph()
{
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

bool Graph::dumpCodeOrigin(PrintStream& out, const char* prefixStr, Node*& previousNodeRef, Node* currentNode, DumpContext* context)
{
    Prefix myPrefix(prefixStr);
    Prefix& prefix = prefixStr ? myPrefix : m_prefix;

    if (!currentNode->origin.semantic)
        return false;
    
    Node* previousNode = previousNodeRef;
    previousNodeRef = currentNode;

    if (!previousNode)
        return false;
    
    if (previousNode->origin.semantic.inlineCallFrame() == currentNode->origin.semantic.inlineCallFrame())
        return false;
    
    Vector<CodeOrigin> previousInlineStack = previousNode->origin.semantic.inlineStack();
    Vector<CodeOrigin> currentInlineStack = currentNode->origin.semantic.inlineStack();
    unsigned commonSize = std::min(previousInlineStack.size(), currentInlineStack.size());
    unsigned indexOfDivergence = commonSize;
    for (unsigned i = 0; i < commonSize; ++i) {
        if (previousInlineStack[i].inlineCallFrame() != currentInlineStack[i].inlineCallFrame()) {
            indexOfDivergence = i;
            break;
        }
    }
    
    bool hasPrinted = false;
    
    // Print the pops.
    for (unsigned i = previousInlineStack.size(); i-- > indexOfDivergence;) {
        out.print(prefix);
        printWhiteSpace(out, i * 2);
        out.print("<-- ", inContext(*previousInlineStack[i].inlineCallFrame(), context), "\n");
        hasPrinted = true;
    }
    
    // Print the pushes.
    for (unsigned i = indexOfDivergence; i < currentInlineStack.size(); ++i) {
        out.print(prefix);
        printWhiteSpace(out, i * 2);
        out.print("--> ", inContext(*currentInlineStack[i].inlineCallFrame(), context), "\n");
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

void Graph::dump(PrintStream& out, const char* prefixStr, Node* node, DumpContext* context)
{
    Prefix myPrefix(prefixStr);
    Prefix& prefix = prefixStr ? myPrefix : m_prefix;

    NodeType op = node->op();

    unsigned refCount = node->refCount();
    bool mustGenerate = node->mustGenerate();
    if (mustGenerate)
        --refCount;

    out.print(prefix);
    printNodeWhiteSpace(out, node);

    // Example/explanation of dataflow dump output
    //
    //   D@14:   <!2:7>  GetByVal(@3, @13)
    //     ^1     ^2 ^3     ^4       ^5
    //
    // (1) The nodeIndex of this operation.
    // (2) The reference count. The number printed is the 'real' count,
    //     not including the 'mustGenerate' ref. If the node is
    //     'mustGenerate' then the count it prefixed with '!'.
    // (3) The virtual register slot assigned to this node.
    // (4) The name of the operation.
    // (5) The arguments to the operation. The may be of the form:
    //         D@#  - a NodeIndex referencing a prior node in the graph.
    //         arg# - an argument number.
    //         id#  - the index in the CodeBlock of an identifier { if codeBlock is passed to dump(), the string representation is displayed }.
    //         var# - the index of a var on the global object, used by GetGlobalVar/GetGlobalLexicalVariable/PutGlobalVariable operations.
    int nodeIndex = node->index();
    const char* prefixPadding = nodeIndex < 10 ? "   " : nodeIndex < 100 ? "  " : " ";
    out.printf("%sD@%d:<%c%u:", prefixPadding, nodeIndex, mustGenerate ? '!' : ' ', refCount);
    if (node->hasResult() && node->hasVirtualRegister() && node->virtualRegister().isValid())
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
    if (node->hasNumberOfArgumentsToSkip())
        out.print(comma, "numberOfArgumentsToSkip = ", node->numberOfArgumentsToSkip());
    if (node->hasArrayMode())
        out.print(comma, node->arrayMode());
    if (node->hasArithUnaryType())
        out.print(comma, "Type:", node->arithUnaryType());
    if (node->hasArithMode())
        out.print(comma, node->arithMode());
    if (node->hasArithRoundingMode())
        out.print(comma, "Rounding:", node->arithRoundingMode());
    if (node->hasScopeOffset())
        out.print(comma, node->scopeOffset());
    if (node->hasDirectArgumentsOffset())
        out.print(comma, node->capturedArgumentsOffset());
    if (node->hasArgumentIndex())
        out.print(comma, node->argumentIndex());
    if (node->hasRegisterPointer())
        out.print(comma, "global", "(", RawPointer(node->variablePointer()), ")");
    if (node->hasIdentifier() && node->identifierNumber() != UINT32_MAX)
        out.print(comma, "id", node->identifierNumber(), "{", identifiers()[node->identifierNumber()], "}");
    if (node->hasCacheableIdentifier() && node->cacheableIdentifier())
        out.print(comma, "cachable-id {", node->cacheableIdentifier(), "}");
    if (node->hasPromotedLocationDescriptor())
        out.print(comma, node->promotedLocationDescriptor());
    if (node->hasClassInfo())
        out.print(comma, *node->classInfo());
    if (node->hasStructureSet())
        out.print(comma, inContext(node->structureSet().toStructureSet(), context));
    if (node->hasStructure())
        out.print(comma, inContext(*node->structure().get(), context));
    if (node->op() == CPUIntrinsic)
        out.print(comma, intrinsicName(node->intrinsic()));
    if (node->hasTransition()) {
        out.print(comma, pointerDumpInContext(node->transition(), context));
#if USE(JSVALUE64)
        out.print(", ID:", node->transition()->next->id());
#else
        out.print(", ID:", RawPointer(node->transition()->next.get()));
#endif
    }
    if (node->hasCellOperand()) {
        if (!node->cellOperand()->value() || !node->cellOperand()->value().isCell())
            out.print(comma, "invalid cell operand: ", node->cellOperand()->value());
        else {
            out.print(comma, pointerDump(node->cellOperand()->value().asCell()));
            if (node->cellOperand()->value().isCell()) {
                CallVariant variant(node->cellOperand()->value().asCell());
                if (ExecutableBase* executable = variant.executable()) {
                    if (executable->isHostFunction())
                        out.print(comma, "<host function>");
                    else if (FunctionExecutable* functionExecutable = jsDynamicCast<FunctionExecutable*>(m_vm, executable))
                        out.print(comma, FunctionExecutableDump(functionExecutable));
                    else
                        out.print(comma, "<non-function executable>");
                }
            }
        }
    }
    if (node->hasQueriedType())
        out.print(comma, node->queriedType());
    if (node->hasStorageAccessData()) {
        StorageAccessData& storageAccessData = node->storageAccessData();
        out.print(comma, "id", storageAccessData.identifierNumber, "{", identifiers()[storageAccessData.identifierNumber], "}");
        out.print(", ", static_cast<ptrdiff_t>(storageAccessData.offset));
    }
    if (node->hasMultiGetByOffsetData()) {
        MultiGetByOffsetData& data = node->multiGetByOffsetData();
        out.print(comma, "id", data.identifierNumber, "{", identifiers()[data.identifierNumber], "}");
        for (unsigned i = 0; i < data.cases.size(); ++i)
            out.print(comma, inContext(data.cases[i], context));
    }
    if (node->hasMultiPutByOffsetData()) {
        MultiPutByOffsetData& data = node->multiPutByOffsetData();
        out.print(comma, "id", data.identifierNumber, "{", identifiers()[data.identifierNumber], "}");
        for (unsigned i = 0; i < data.variants.size(); ++i)
            out.print(comma, inContext(data.variants[i], context));
    }
    if (node->hasMultiDeleteByOffsetData()) {
        MultiDeleteByOffsetData& data = node->multiDeleteByOffsetData();
        out.print(comma, "id", data.identifierNumber, "{", identifiers()[data.identifierNumber], "}");
        for (unsigned i = 0; i < data.variants.size(); ++i)
            out.print(comma, inContext(data.variants[i], context));
    }
    if (node->hasMatchStructureData()) {
        for (MatchStructureVariant& variant : node->matchStructureData().variants)
            out.print(comma, inContext(*variant.structure.get(), context), "=>", variant.result);
    }
    ASSERT(node->hasVariableAccessData(*this) == node->accessesStack(*this));
    if (node->hasVariableAccessData(*this)) {
        VariableAccessData* variableAccessData = node->tryGetVariableAccessData();
        if (variableAccessData) {
            Operand operand = variableAccessData->operand();
            out.print(comma, variableAccessData->operand(), "(", VariableAccessDataDump(*this, variableAccessData), ")");
            operand = variableAccessData->machineLocal();
            if (operand.isValid())
                out.print(comma, "machine:", operand);
        }
    }
    if (node->hasStackAccessData()) {
        StackAccessData* data = node->stackAccessData();
        out.print(comma, data->operand);
        if (data->machineLocal.isValid())
            out.print(comma, "machine:", data->machineLocal);
        out.print(comma, data->format);
    }
    if (node->hasUnlinkedOperand())
        out.print(comma, node->unlinkedOperand());
    if (node->hasVectorLengthHint())
        out.print(comma, "vectorLengthHint = ", node->vectorLengthHint());
    if (node->hasLazyJSValue())
        out.print(comma, node->lazyJSValue());
    if (node->hasIndexingType())
        out.print(comma, IndexingTypeDump(node->indexingMode()));
    if (node->hasTypedArrayType())
        out.print(comma, node->typedArrayType());
    if (node->hasPhi())
        out.print(comma, "^", node->phi()->index());
    if (node->hasExecutionCounter())
        out.print(comma, RawPointer(node->executionCounter()));
    if (node->hasWatchpointSet())
        out.print(comma, RawPointer(node->watchpointSet()));
    if (node->hasStoragePointer())
        out.print(comma, RawPointer(node->storagePointer()));
    if (node->hasObjectMaterializationData())
        out.print(comma, node->objectMaterializationData());
    if (node->hasCallVarargsData())
        out.print(comma, "firstVarArgOffset = ", node->callVarargsData()->firstVarArgOffset);
    if (node->hasLoadVarargsData()) {
        LoadVarargsData* data = node->loadVarargsData();
        out.print(comma, "start = ", data->start, ", count = ", data->count);
        if (data->machineStart.isValid())
            out.print(", machineStart = ", data->machineStart);
        if (data->machineCount.isValid())
            out.print(", machineCount = ", data->machineCount);
        out.print(", offset = ", data->offset, ", mandatoryMinimum = ", data->mandatoryMinimum);
        out.print(", limit = ", data->limit);
    }
    if (node->hasIsInternalPromise())
        out.print(comma, "isInternalPromise = ", node->isInternalPromise());
    if (node->hasInternalFieldIndex())
        out.print(comma, "internalFieldIndex = ", node->internalFieldIndex());
    if (node->hasCallDOMGetterData()) {
        CallDOMGetterData* data = node->callDOMGetterData();
        out.print(comma, "id", data->identifierNumber, "{", identifiers()[data->identifierNumber], "}");
        out.print(", domJIT = ", RawPointer(data->domJIT));
    }
    if (node->hasIgnoreLastIndexIsWritable())
        out.print(comma, "ignoreLastIndexIsWritable = ", node->ignoreLastIndexIsWritable());
    if (node->hasIntrinsic())
        out.print(comma, "intrinsic = ", node->intrinsic());
    if (node->isConstant())
        out.print(comma, pointerDumpInContext(node->constant(), context));
    if (node->hasCallLinkStatus())
        out.print(comma, *node->callLinkStatus());
    if (node->hasGetByStatus())
        out.print(comma, *node->getByStatus());
    if (node->hasInByIdStatus())
        out.print(comma, *node->inByIdStatus());
    if (node->hasPutByIdStatus())
        out.print(comma, *node->putByIdStatus());
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
    if (node->isEntrySwitch()) {
        EntrySwitchData* data = node->entrySwitchData();
        for (unsigned i = 0; i < data->cases.size(); ++i)
            out.print(comma, BranchTarget(data->cases[i]));
    }
    ClobberSet reads;
    ClobberSet writes;
    addReadsAndWrites(*this, node, reads, writes);
    if (!reads.isEmpty())
        out.print(comma, "R:", sortedListDump(reads.direct(), ","));
    if (!writes.isEmpty())
        out.print(comma, "W:", sortedListDump(writes.direct(), ","));
    ExitMode exitMode = mayExit(*this, node);
    if (exitMode != DoesNotExit)
        out.print(comma, exitMode);
    if (clobbersExitState(*this, node))
        out.print(comma, "ClobbersExit");
    if (node->origin.isSet()) {
        out.print(comma, node->origin.semantic.bytecodeIndex());
        if (node->origin.semantic != node->origin.forExit && node->origin.forExit.isSet())
            out.print(comma, "exit: ", node->origin.forExit);
    }
    out.print(comma, node->origin.exitOK ? "ExitValid" : "ExitInvalid");
    if (node->origin.wasHoisted)
        out.print(comma, "WasHoisted");
    out.print(")");

    if (node->accessesStack(*this) && node->tryGetVariableAccessData())
        out.print("  predicting ", SpeculationDump(node->tryGetVariableAccessData()->prediction()));
    else if (node->hasHeapPrediction())
        out.print("  predicting ", SpeculationDump(node->getHeapPrediction()));
    
    out.print("\n");
}

bool Graph::terminalsAreValid()
{
    for (BasicBlock* block : blocksInNaturalOrder()) {
        if (!block->terminal())
            return false;
    }
    return true;
}

static BasicBlock* unboxLoopNode(const CPSCFG::Node& node) { return node.node(); }
static BasicBlock* unboxLoopNode(BasicBlock* block) { return block; }

void Graph::dumpBlockHeader(PrintStream& out, const char* prefixStr, BasicBlock* block, PhiNodeDumpMode phiNodeDumpMode, DumpContext* context)
{
    Prefix myPrefix(prefixStr);
    Prefix& prefix = prefixStr ? myPrefix : m_prefix;

    out.print(prefix, "Block ", *block, " (", inContext(block->at(0)->origin.semantic, context), "):",
        block->isReachable ? "" : " (skipped)", block->isOSRTarget ? " (OSR target)" : "", block->isCatchEntrypoint ? " (Catch Entrypoint)" : "", "\n");
    if (block->executionCount == block->executionCount)
        out.print(prefix, "  Execution count: ", block->executionCount, "\n");
    out.print(prefix, "  Predecessors:");
    for (size_t i = 0; i < block->predecessors.size(); ++i)
        out.print(" ", *block->predecessors[i]);
    out.print("\n");
    out.print(prefix, "  Successors:");
    if (block->terminal()) {
        for (BasicBlock* successor : block->successors()) {
            out.print(" ", *successor);
        }
    } else
        out.print(" <invalid>");
    out.print("\n");

    auto printDominators = [&] (auto& dominators) {
        out.print(prefix, "  Dominated by: ", dominators.dominatorsOf(block), "\n");
        out.print(prefix, "  Dominates: ", dominators.blocksDominatedBy(block), "\n");
        out.print(prefix, "  Dominance Frontier: ", dominators.dominanceFrontierOf(block), "\n");
        out.print(prefix, "  Iterated Dominance Frontier: ",
            dominators.iteratedDominanceFrontierOf(typename std::remove_reference<decltype(dominators)>::type::List { block }), "\n");
    };

    if (terminalsAreValid()) {
        if (m_ssaDominators)
            printDominators(*m_ssaDominators);
        else if (m_cpsDominators)
            printDominators(*m_cpsDominators);
    }

    if (m_backwardsDominators && terminalsAreValid()) {
        out.print(prefix, "  Backwards dominates by: ", m_backwardsDominators->dominatorsOf(block), "\n");
        out.print(prefix, "  Backwards dominates: ", m_backwardsDominators->blocksDominatedBy(block), "\n");
    }
    if (m_controlEquivalenceAnalysis && terminalsAreValid()) {
        out.print(prefix, "  Control equivalent to:");
        for (BasicBlock* otherBlock : blocksInNaturalOrder()) {
            if (m_controlEquivalenceAnalysis->areEquivalent(block, otherBlock))
                out.print(" ", *otherBlock);
        }
        out.print("\n");
    }

    auto printNaturalLoops = [&] (auto& naturalLoops) {
        if (const auto* loop = naturalLoops->headerOf(block)) {
            out.print(prefix, "  Loop header, contains:");
            Vector<BlockIndex> sortedBlockList;
            for (unsigned i = 0; i < loop->size(); ++i)
                sortedBlockList.append(unboxLoopNode(loop->at(i))->index);
            std::sort(sortedBlockList.begin(), sortedBlockList.end());
            for (unsigned i = 0; i < sortedBlockList.size(); ++i)
                out.print(" #", sortedBlockList[i]);
            out.print("\n");
        }
        
        auto containingLoops = naturalLoops->loopsOf(block);
        if (!containingLoops.isEmpty()) {
            out.print(prefix, "  Containing loop headers:");
            for (unsigned i = 0; i < containingLoops.size(); ++i)
                out.print(" ", *unboxLoopNode(containingLoops[i]->header()));
            out.print("\n");
        }
    };

    if (m_ssaNaturalLoops)
        printNaturalLoops(m_ssaNaturalLoops);
    else if (m_cpsNaturalLoops)
        printNaturalLoops(m_cpsNaturalLoops);

    if (!block->phis.isEmpty()) {
        out.print(prefix, "  Phi Nodes:");
        for (size_t i = 0; i < block->phis.size(); ++i) {
            Node* phiNode = block->phis[i];
            ASSERT(phiNode->op() == Phi);
            if (!phiNode->shouldGenerate() && phiNodeDumpMode == DumpLivePhisOnly)
                continue;
            out.print(" D@", phiNode->index(), "<", phiNode->operand(), ",", phiNode->refCount(), ">->(");
            if (phiNode->child1()) {
                out.print("D@", phiNode->child1()->index());
                if (phiNode->child2()) {
                    out.print(", D@", phiNode->child2()->index());
                    if (phiNode->child3())
                        out.print(", D@", phiNode->child3()->index());
                }
            }
            out.print(")", i + 1 < block->phis.size() ? "," : "");
        }
        out.print("\n");
    }
}

void Graph::dump(PrintStream& out, DumpContext* context)
{
    Prefix& prefix = m_prefix;
    DumpContext myContext;
    myContext.graph = this;
    if (!context)
        context = &myContext;
    
    out.print("\n");
    out.print(prefix, "DFG for ", CodeBlockWithJITType(m_codeBlock, JITType::DFGJIT), ":\n");
    out.print(prefix, "  Fixpoint state: ", m_fixpointState, "; Form: ", m_form, "; Unification state: ", m_unificationState, "; Ref count state: ", m_refCountState, "\n");
    if (m_form == SSA) {
        for (unsigned entrypointIndex = 0; entrypointIndex < m_argumentFormats.size(); ++entrypointIndex)
            out.print(prefix, "  Argument formats for entrypoint index: ", entrypointIndex, " : ", listDump(m_argumentFormats[entrypointIndex]), "\n");
    }
    else {
        for (const auto& pair : m_rootToArguments)
            out.print(prefix, "  Arguments for block#", pair.key->index, ": ", listDump(pair.value), "\n");
    }
    out.print("\n");
    
    Node* lastNode = nullptr;
    for (size_t b = 0; b < m_blocks.size(); ++b) {
        BasicBlock* block = m_blocks[b].get();
        if (!block)
            continue;
        prefix.blockIndex = block->index;
        dumpBlockHeader(out, Prefix::noString, block, DumpAllPhis, context);
        out.print(prefix, "  States: ", block->cfaStructureClobberStateAtHead);
        if (!block->cfaHasVisited)
            out.print(", CurrentlyCFAUnreachable");
        if (!block->intersectionOfCFAHasVisited)
            out.print(", CFAUnreachable");
        out.print("\n");
        switch (m_form) {
        case LoadStore:
        case ThreadedCPS: {
            out.print(prefix, "  Vars Before: ");
            if (block->cfaHasVisited)
                out.print(inContext(block->valuesAtHead, context));
            else
                out.print("<empty>");
            out.print("\n");
            out.print(prefix, "  Intersected Vars Before: ");
            if (block->intersectionOfCFAHasVisited)
                out.print(inContext(block->intersectionOfPastValuesAtHead, context));
            else
                out.print("<empty>");
            out.print("\n");
            out.print(prefix, "  Var Links: ", block->variablesAtHead, "\n");
            break;
        }
            
        case SSA: {
            RELEASE_ASSERT(block->ssa);
            if (dumpOSRAvailabilityData)
                out.print(prefix, "  Availability: ", block->ssa->availabilityAtHead, "\n");
            out.print(prefix, "  Live: ", nodeListDump(block->ssa->liveAtHead), "\n");
            out.print(prefix, "  Values: ", nodeValuePairListDump(block->ssa->valuesAtHead, context), "\n");
            break;
        } }
        for (size_t i = 0; i < block->size(); ++i) {
            prefix.clearNodeIndex();
            dumpCodeOrigin(out, Prefix::noString, lastNode, block->at(i), context);
            prefix.nodeIndex = i;
            dump(out, Prefix::noString, block->at(i), context);
        }
        prefix.clearNodeIndex();
        out.print(prefix, "  States: ", block->cfaBranchDirection, ", ", block->cfaStructureClobberStateAtTail);
        if (!block->cfaDidFinish)
            out.print(", CFAInvalidated");
        out.print("\n");
        switch (m_form) {
        case LoadStore:
        case ThreadedCPS: {
            out.print(prefix, "  Vars After: ");
            if (block->cfaHasVisited)
                out.print(inContext(block->valuesAtTail, context));
            else
                out.print("<empty>");
            out.print("\n");
            out.print(prefix, "  Var Links: ", block->variablesAtTail, "\n");
            break;
        }
            
        case SSA: {
            RELEASE_ASSERT(block->ssa);
            if (dumpOSRAvailabilityData)
                out.print(prefix, "  Availability: ", block->ssa->availabilityAtTail, "\n");
            out.print(prefix, "  Live: ", nodeListDump(block->ssa->liveAtTail), "\n");
            out.print(prefix, "  Values: ", nodeValuePairListDump(block->ssa->valuesAtTail, context), "\n");
            break;
        } }
        out.print("\n");
    }
    prefix.clearBlockIndex();

    out.print(prefix, "GC Values:\n");
    for (FrozenValue* value : m_frozenValues) {
        if (value->pointsToHeap())
            out.print(prefix, "    ", inContext(*value, &myContext), "\n");
    }

    out.print(inContext(watchpoints(), &myContext));
    
    if (!myContext.isEmpty()) {
        StringPrintStream prefixStr;
        prefixStr.print(prefix);
        myContext.dump(out, prefixStr.toCString().data());
        out.print("\n");
    }
}

void Graph::deleteNode(Node* node)
{
    if (validationEnabled() && m_form == SSA) {
        for (BasicBlock* block : blocksInNaturalOrder()) {
            DFG_ASSERT(*this, node, !block->ssa->liveAtHead.contains(node));
            DFG_ASSERT(*this, node, !block->ssa->liveAtTail.contains(node));
        }
    }

    m_nodes.remove(node);
}

void Graph::packNodeIndices()
{
    m_nodes.packIndices();
}

void Graph::dethread()
{
    if (m_form == LoadStore || m_form == SSA)
        return;
    
    if (logCompilationChanges())
        dataLog("Dethreading DFG graph.\n");
    
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
    
    if (!successor->predecessors.contains(block))
        successor->predecessors.append(block);
}

void Graph::determineReachability()
{
    Vector<BasicBlock*, 16> worklist;
    for (BasicBlock* entrypoint : m_roots) {
        entrypoint->isReachable = true;
        worklist.append(entrypoint);
    }
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

namespace {

class RefCountCalculator {
public:
    RefCountCalculator(Graph& graph)
        : m_graph(graph)
    {
    }
    
    void calculate()
    {
        // First reset the counts to 0 for all nodes.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = block->size(); indexInBlock--;)
                block->at(indexInBlock)->setRefCount(0);
            for (unsigned phiIndex = block->phis.size(); phiIndex--;)
                block->phis[phiIndex]->setRefCount(0);
        }
    
        // Now find the roots:
        // - Nodes that are must-generate.
        // - Nodes that are reachable from type checks.
        // Set their ref counts to 1 and put them on the worklist.
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned indexInBlock = block->size(); indexInBlock--;) {
                Node* node = block->at(indexInBlock);
                DFG_NODE_DO_TO_CHILDREN(m_graph, node, findTypeCheckRoot);
                if (!(node->flags() & NodeMustGenerate))
                    continue;
                if (!node->postfixRef())
                    m_worklist.append(node);
            }
        }
        
        while (!m_worklist.isEmpty()) {
            while (!m_worklist.isEmpty()) {
                Node* node = m_worklist.last();
                m_worklist.removeLast();
                ASSERT(node->shouldGenerate()); // It should not be on the worklist unless it's ref'ed.
                DFG_NODE_DO_TO_CHILDREN(m_graph, node, countEdge);
            }
            
            if (m_graph.m_form == SSA) {
                // Find Phi->Upsilon edges, which are represented as meta-data in the
                // Upsilon.
                for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
                    BasicBlock* block = m_graph.block(blockIndex);
                    if (!block)
                        continue;
                    for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                        Node* node = block->at(nodeIndex);
                        if (node->op() != Upsilon)
                            continue;
                        if (node->shouldGenerate())
                            continue;
                        if (node->phi()->shouldGenerate())
                            countNode(node);
                    }
                }
            }
        }
    }
    
private:
    void findTypeCheckRoot(Node*, Edge edge)
    {
        // We may have an "unproved" untyped use for code that is unreachable. The CFA
        // will just not have gotten around to it.
        if (edge.isProved() || edge.willNotHaveCheck())
            return;
        if (!edge->postfixRef())
            m_worklist.append(edge.node());
    }
    
    void countNode(Node* node)
    {
        if (node->postfixRef())
            return;
        m_worklist.append(node);
    }
    
    void countEdge(Node*, Edge edge)
    {
        // Don't count edges that are already counted for their type checks.
        if (!(edge.isProved() || edge.willNotHaveCheck()))
            return;
        countNode(edge.node());
    }
    
    Graph& m_graph;
    Vector<Node*, 128> m_worklist;
};

} // anonymous namespace

void Graph::computeRefCounts()
{
    RefCountCalculator calculator(*this);
    calculator.calculate();
}

void Graph::killBlockAndItsContents(BasicBlock* block)
{
    if (auto& ssaData = block->ssa)
        ssaData->invalidate();
    for (unsigned phiIndex = block->phis.size(); phiIndex--;)
        deleteNode(block->phis[phiIndex]);
    for (Node* node : *block)
        deleteNode(node);
    
    killBlock(block);
}

void Graph::killUnreachableBlocks()
{
    invalidateNodeLiveness();

    for (BlockIndex blockIndex = 0; blockIndex < numBlocks(); ++blockIndex) {
        BasicBlock* block = this->block(blockIndex);
        if (!block)
            continue;
        if (block->isReachable)
            continue;

        dataLogIf(Options::verboseDFGBytecodeParsing(), "Basic block #", blockIndex, " was killed because it was unreachable\n");
        killBlockAndItsContents(block);
    }
}

void Graph::invalidateCFG()
{
    m_cpsDominators = nullptr;
    m_ssaDominators = nullptr;
    m_cpsNaturalLoops = nullptr;
    m_ssaNaturalLoops = nullptr;
    m_controlEquivalenceAnalysis = nullptr;
    m_backwardsDominators = nullptr;
    m_backwardsCFG = nullptr;
    m_cpsCFG = nullptr;
}

void Graph::invalidateNodeLiveness()
{
    if (m_form != SSA)
        return;

    for (BasicBlock* block : blocksInNaturalOrder())
        block->ssa->invalidate();
}

void Graph::substituteGetLocal(BasicBlock& block, unsigned startIndexInBlock, VariableAccessData* variableAccessData, Node* newGetLocal)
{
    for (unsigned indexInBlock = startIndexInBlock; indexInBlock < block.size(); ++indexInBlock) {
        Node* node = block[indexInBlock];
        bool shouldContinue = true;
        switch (node->op()) {
        case SetLocal: {
            if (node->operand() == variableAccessData->operand())
                shouldContinue = false;
            break;
        }
                
        case GetLocal: {
            if (node->variableAccessData() != variableAccessData)
                continue;
            substitute(block, indexInBlock, node, newGetLocal);
            Node* oldTailNode = block.variablesAtTail.operand(variableAccessData->operand());
            if (oldTailNode == node)
                block.variablesAtTail.operand(variableAccessData->operand()) = newGetLocal;
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

BlockList Graph::blocksInPreOrder()
{
    BlockList result;
    result.reserveInitialCapacity(m_blocks.size());
    BlockWorklist worklist;
    for (BasicBlock* entrypoint : m_roots)
        worklist.push(entrypoint);
    while (BasicBlock* block = worklist.pop()) {
        result.append(block);
        for (unsigned i = block->numSuccessors(); i--;)
            worklist.push(block->successor(i));
    }

    if (validationEnabled()) {
        // When iterating over pre order, we should see dominators
        // before things they dominate.
        auto validateResults = [&] (auto& dominators) {
            for (unsigned i = 0; i < result.size(); ++i) {
                BasicBlock* a = result[i];
                if (!a)
                    continue;
                for (unsigned j = 0; j < result.size(); ++j) {
                    BasicBlock* b = result[j];
                    if (!b || a == b)
                        continue;
                    if (dominators.dominates(a, b))
                        RELEASE_ASSERT(i < j);
                }
            }
        };

        if (m_form == SSA || m_isInSSAConversion)
            validateResults(ensureSSADominators());
        else
            validateResults(ensureCPSDominators());
    }
    return result;
}

BlockList Graph::blocksInPostOrder(bool isSafeToValidate)
{
    BlockList result;
    result.reserveInitialCapacity(m_blocks.size());
    PostOrderBlockWorklist worklist;
    for (BasicBlock* entrypoint : m_roots)
        worklist.push(entrypoint);
    while (BlockWithOrder item = worklist.pop()) {
        switch (item.order) {
        case VisitOrder::Pre:
            worklist.pushPost(item.node);
            for (unsigned i = item.node->numSuccessors(); i--;)
                worklist.push(item.node->successor(i));
            break;
        case VisitOrder::Post:
            result.append(item.node);
            break;
        }
    }

    if (isSafeToValidate && validationEnabled()) { // There are users of this where we haven't yet built of the CFG enough to be able to run dominators.
        auto validateResults = [&] (auto& dominators) {
            // When iterating over reverse post order, we should see dominators
            // before things they dominate.
            for (unsigned i = 0; i < result.size(); ++i) {
                BasicBlock* a = result[i];
                if (!a)
                    continue;
                for (unsigned j = 0; j < result.size(); ++j) {
                    BasicBlock* b = result[j];
                    if (!b || a == b)
                        continue;
                    if (dominators.dominates(a, b))
                        RELEASE_ASSERT(i > j);
                }
            }
        };

        if (m_form == SSA || m_isInSSAConversion)
            validateResults(ensureSSADominators());
        else
            validateResults(ensureCPSDominators());
    }

    return result;
}

void Graph::clearReplacements()
{
    for (BlockIndex blockIndex = numBlocks(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned phiIndex = block->phis.size(); phiIndex--;)
            block->phis[phiIndex]->setReplacement(nullptr);
        for (unsigned nodeIndex = block->size(); nodeIndex--;)
            block->at(nodeIndex)->setReplacement(nullptr);
    }
}

void Graph::clearEpochs()
{
    for (BlockIndex blockIndex = numBlocks(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned phiIndex = block->phis.size(); phiIndex--;)
            block->phis[phiIndex]->setEpoch(Epoch());
        for (unsigned nodeIndex = block->size(); nodeIndex--;)
            block->at(nodeIndex)->setEpoch(Epoch());
    }
}

void Graph::initializeNodeOwners()
{
    for (BlockIndex blockIndex = numBlocks(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned phiIndex = block->phis.size(); phiIndex--;)
            block->phis[phiIndex]->owner = block;
        for (unsigned nodeIndex = block->size(); nodeIndex--;)
            block->at(nodeIndex)->owner = block;
    }
}

void Graph::clearFlagsOnAllNodes(NodeFlags flags)
{
    for (BlockIndex blockIndex = numBlocks(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        for (unsigned phiIndex = block->phis.size(); phiIndex--;)
            block->phis[phiIndex]->clearFlags(flags);
        for (unsigned nodeIndex = block->size(); nodeIndex--;)
            block->at(nodeIndex)->clearFlags(flags);
    }
}

bool Graph::watchCondition(const ObjectPropertyCondition& key)
{
    if (!key.isWatchable())
        return false;

    DesiredWeakReferences& weakReferences = m_plan.weakReferences();
    weakReferences.addLazily(key.object());
    if (key.hasPrototype())
        weakReferences.addLazily(key.prototype());
    if (key.hasRequiredValue())
        weakReferences.addLazily(key.requiredValue());

    m_plan.watchpoints().addLazily(key);

    if (key.kind() == PropertyCondition::Presence)
        m_safeToLoad.add(std::make_pair(key.object(), key.offset()));
    
    return true;
}

bool Graph::watchConditions(const ObjectPropertyConditionSet& keys)
{
    if (!keys.isValid())
        return false;

    for (const ObjectPropertyCondition& key : keys) {
        if (!watchCondition(key))
            return false;
    }
    return true;
}

bool Graph::isSafeToLoad(JSObject* base, PropertyOffset offset)
{
    return m_safeToLoad.contains(std::make_pair(base, offset));
}

bool Graph::watchGlobalProperty(JSGlobalObject* globalObject, unsigned identifierNumber)
{
    UniquedStringImpl* uid = identifiers()[identifierNumber];
    // If we already have a WatchpointSet, and it is already invalidated, it means that this scope operation must be changed from GlobalProperty to GlobalLexicalVar,
    // but we still have stale metadata here since we have not yet executed this bytecode operation since the invalidation. Just emitting ForceOSRExit to update the
    // metadata when it reaches to this code.
    if (auto* watchpoint = globalObject->getReferencedPropertyWatchpointSet(uid)) {
        if (!watchpoint->isStillValid())
            return false;
    }
    globalProperties().addLazily(DesiredGlobalProperty(globalObject, identifierNumber));
    return true;
}

FullBytecodeLiveness& Graph::livenessFor(CodeBlock* codeBlock)
{
    HashMap<CodeBlock*, std::unique_ptr<FullBytecodeLiveness>>::iterator iter = m_bytecodeLiveness.find(codeBlock);
    if (iter != m_bytecodeLiveness.end())
        return *iter->value;
    
    std::unique_ptr<FullBytecodeLiveness> liveness = makeUnique<FullBytecodeLiveness>();
    codeBlock->livenessAnalysis().computeFullLiveness(codeBlock, *liveness);
    FullBytecodeLiveness& result = *liveness;
    m_bytecodeLiveness.add(codeBlock, WTFMove(liveness));
    return result;
}

FullBytecodeLiveness& Graph::livenessFor(InlineCallFrame* inlineCallFrame)
{
    return livenessFor(baselineCodeBlockFor(inlineCallFrame));
}

bool Graph::isLiveInBytecode(Operand operand, CodeOrigin codeOrigin)
{
    static constexpr bool verbose = false;
    
    if (verbose)
        dataLog("Checking of operand is live: ", operand, "\n");
    bool isCallerOrigin = false;

    CodeOrigin* codeOriginPtr = &codeOrigin;
    auto* inlineCallFrame = codeOriginPtr->inlineCallFrame();
    // We need to handle tail callers because we may decide to exit to the
    // the return bytecode following the tail call.
    for (; codeOriginPtr; codeOriginPtr = inlineCallFrame ? &inlineCallFrame->directCaller : nullptr) {
        inlineCallFrame = codeOriginPtr->inlineCallFrame();
        if (operand.isTmp()) {
            unsigned tmpOffset = inlineCallFrame ? inlineCallFrame->tmpOffset : 0;
            unsigned operandIndex = static_cast<unsigned>(operand.value());

            ASSERT(operand.value() >= 0);
            // This tmp should have belonged to someone we inlined.
            if (operandIndex > tmpOffset + maxNumCheckpointTmps)
                return false;

            CodeBlock* codeBlock = baselineCodeBlockFor(inlineCallFrame);
            if (!codeBlock->numTmps() || operandIndex < tmpOffset)
                continue;

            auto bitMap = tmpLivenessForCheckpoint(*codeBlock, codeOriginPtr->bytecodeIndex());
            return bitMap.get(operandIndex - tmpOffset);
        }

        VirtualRegister reg = operand.virtualRegister() - codeOriginPtr->stackOffset();
        
        if (verbose)
            dataLog("reg = ", reg, "\n");

        if (operand.virtualRegister().offset() < codeOriginPtr->stackOffset() + CallFrame::headerSizeInRegisters) {
            if (reg.isArgument()) {
                RELEASE_ASSERT(reg.offset() < CallFrame::headerSizeInRegisters);


                if (inlineCallFrame->isClosureCall
                    && reg == CallFrameSlot::callee) {
                    if (verbose)
                        dataLog("Looks like a callee.\n");
                    return true;
                }
                
                if (inlineCallFrame->isVarargs()
                    && reg == CallFrameSlot::argumentCountIncludingThis) {
                    if (verbose)
                        dataLog("Looks like the argument count.\n");
                    return true;
                }
                
                return false;
            }

            if (verbose)
                dataLog("Asking the bytecode liveness.\n");
            CodeBlock* codeBlock = baselineCodeBlockFor(inlineCallFrame);
            FullBytecodeLiveness& fullLiveness = livenessFor(codeBlock);
            BytecodeIndex bytecodeIndex = codeOriginPtr->bytecodeIndex();
            return fullLiveness.virtualRegisterIsLive(reg, bytecodeIndex, appropriateLivenessCalculationPoint(*codeOriginPtr, isCallerOrigin));
        }

        // Arguments are always live. This would be redundant if it wasn't for our
        // op_call_varargs inlining.
        if (inlineCallFrame && reg.isArgument()
            && static_cast<size_t>(reg.toArgument()) < inlineCallFrame->argumentsWithFixup.size()) {
            if (verbose)
                dataLog("Argument is live.\n");
            return true;
        }

        isCallerOrigin = true;
    }

    if (operand.isTmp())
        return false;

    if (verbose)
        dataLog("Ran out of stack, returning true.\n");
    return true;    
}

BitVector Graph::localsAndTmpsLiveInBytecode(CodeOrigin codeOrigin)
{
    BitVector result;
    unsigned numLocals = block(0)->variablesAtHead.numberOfLocals();
    result.ensureSize(numLocals + block(0)->variablesAtHead.numberOfTmps());
    forAllLocalsAndTmpsLiveInBytecode(
        codeOrigin,
        [&] (Operand operand) {
            unsigned offset = operand.isTmp() ? numLocals + operand.value() : operand.toLocal();
            result.quickSet(offset);
        });
    return result;
}

unsigned Graph::parameterSlotsForArgCount(unsigned argCount)
{
    size_t frameSize = CallFrame::headerSizeInRegisters + argCount;
    size_t alignedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), frameSize);
    return alignedFrameSize - CallerFrameAndPC::sizeInRegisters;
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
    for (InlineCallFrameSet::iterator iter = m_plan.inlineCallFrames()->begin(); !!iter; ++iter) {
        InlineCallFrame* inlineCallFrame = *iter;
        CodeBlock* codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
        unsigned requiredCount = VirtualRegister(inlineCallFrame->stackOffset).toLocal() + 1 + JIT::frameRegisterCountFor(codeBlock);
        count = std::max(count, requiredCount);
    }
    return count;
}

unsigned Graph::requiredRegisterCountForExecutionAndExit()
{
    // FIXME: We should make sure that frameRegisterCount() and requiredRegisterCountForExit()
    // never overflows. https://bugs.webkit.org/show_bug.cgi?id=173852
    return std::max(frameRegisterCount(), requiredRegisterCountForExit());
}

JSValue Graph::tryGetConstantProperty(
    JSValue base, const RegisteredStructureSet& structureSet, PropertyOffset offset)
{
    if (!base || !base.isObject())
        return JSValue();
    
    JSObject* object = asObject(base);
    
    for (unsigned i = structureSet.size(); i--;) {
        RegisteredStructure structure = structureSet[i];

        WatchpointSet* set = structure->propertyReplacementWatchpointSet(offset);
        if (!set || !set->isStillValid())
            return JSValue();
        
        ASSERT(structure->isValidOffset(offset));
        ASSERT(!structure->isUncacheableDictionary());
        
        watchpoints().addLazily(set);
    }
    
    // What follows may require some extra thought. We need this load to load a valid JSValue. If
    // our profiling makes sense and we're still on track to generate code that won't be
    // invalidated, then we have nothing to worry about. We do, however, have to worry about
    // loading - and then using - an invalid JSValue in the case that unbeknownst to us our code
    // is doomed.
    //
    // One argument in favor of this code is that it should definitely work because the butterfly
    // is always set before the structure. However, we don't currently have a fence between those
    // stores. It's not clear if this matters, however. We only shrink the propertyStorage while
    // holding the Structure's lock. So, for this to fail, you'd need an access on a constant
    // object pointer such that the inline caches told us that the object had a structure that it
    // did not *yet* have, and then later,the object transitioned to that structure that the inline
    // caches had already seen. And then the processor reordered the stores. Seems unlikely and
    // difficult to test. I believe that this is worth revisiting but it isn't worth losing sleep
    // over. Filed:
    // https://bugs.webkit.org/show_bug.cgi?id=134641
    //
    // For now, we just do the minimal thing: defend against the structure right now being
    // incompatible with the getDirect we're trying to do. The easiest way to do that is to
    // determine if the structure belongs to the proven set.

    Structure* structure = object->structure(m_vm);
    if (!structureSet.toStructureSet().contains(structure))
        return JSValue();

    return object->getDirectConcurrently(structure, offset);
}

JSValue Graph::tryGetConstantProperty(JSValue base, Structure* structure, PropertyOffset offset)
{
    return tryGetConstantProperty(base, RegisteredStructureSet(registerStructure(structure)), offset);
}

JSValue Graph::tryGetConstantProperty(
    JSValue base, const StructureAbstractValue& structure, PropertyOffset offset)
{
    if (structure.isInfinite()) {
        // FIXME: If we just converted the offset to a uid, we could do ObjectPropertyCondition
        // watching to constant-fold the property.
        // https://bugs.webkit.org/show_bug.cgi?id=147271
        return JSValue();
    }
    
    return tryGetConstantProperty(base, structure.set(), offset);
}

JSValue Graph::tryGetConstantProperty(const AbstractValue& base, PropertyOffset offset)
{
    return tryGetConstantProperty(base.m_value, base.m_structure, offset);
}

AbstractValue Graph::inferredValueForProperty(
    const AbstractValue& base, PropertyOffset offset,
    StructureClobberState clobberState)
{
    if (JSValue value = tryGetConstantProperty(base, offset)) {
        AbstractValue result;
        result.set(*this, *freeze(value), clobberState);
        return result;
    }

    return AbstractValue::heapTop();
}

JSValue Graph::tryGetConstantClosureVar(JSValue base, ScopeOffset offset)
{
    // This has an awesome concurrency story. See comment for GetGlobalVar in ByteCodeParser.
    
    if (!base)
        return JSValue();
    
    JSLexicalEnvironment* activation = jsDynamicCast<JSLexicalEnvironment*>(m_vm, base);
    if (!activation)
        return JSValue();
    
    SymbolTable* symbolTable = activation->symbolTable();
    JSValue value;
    WatchpointSet* set;
    {
        ConcurrentJSLocker locker(symbolTable->m_lock);
        
        SymbolTableEntry* entry = symbolTable->entryFor(locker, offset);
        if (!entry)
            return JSValue();
        
        set = entry->watchpointSet();
        if (!set)
            return JSValue();
        
        if (set->state() != IsWatched)
            return JSValue();
        
        ASSERT(entry->scopeOffset() == offset);
        value = activation->variableAt(offset).get();
        if (!value)
            return JSValue();
    }
    
    watchpoints().addLazily(set);
    
    return value;
}

JSValue Graph::tryGetConstantClosureVar(const AbstractValue& value, ScopeOffset offset)
{
    return tryGetConstantClosureVar(value.m_value, offset);
}

JSValue Graph::tryGetConstantClosureVar(Node* node, ScopeOffset offset)
{
    if (!node->hasConstant())
        return JSValue();
    return tryGetConstantClosureVar(node->asJSValue(), offset);
}

JSArrayBufferView* Graph::tryGetFoldableView(JSValue value)
{
    if (!value)
        return nullptr;
    JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(m_vm, value);
    if (!view)
        return nullptr;
    if (!view->length())
        return nullptr;
    WTF::loadLoadFence();
    freeze(view);
    watchpoints().addLazily(view);
    return view;
}

JSArrayBufferView* Graph::tryGetFoldableView(JSValue value, ArrayMode arrayMode)
{
    if (arrayMode.type() != Array::AnyTypedArray && arrayMode.typedArrayType() == NotTypedArray)
        return nullptr;
    return tryGetFoldableView(value);
}

void Graph::registerFrozenValues()
{
    ConcurrentJSLocker locker(m_codeBlock->m_lock);
    m_codeBlock->constants().shrink(0);
    m_codeBlock->constantsSourceCodeRepresentation().resize(0);
    for (FrozenValue* value : m_frozenValues) {
        if (!value->pointsToHeap())
            continue;
        
        ASSERT(value->structure());
        ASSERT(m_plan.weakReferences().contains(value->structure()));

        switch (value->strength()) {
        case WeakValue: {
            m_plan.weakReferences().addLazily(value->value().asCell());
            break;
        }
        case StrongValue: {
            unsigned constantIndex = m_codeBlock->addConstantLazily(locker);
            // We already have a barrier on the code block.
            m_codeBlock->constants()[constantIndex].setWithoutWriteBarrier(value->value());
            break;
        } }
    }
    m_codeBlock->constants().shrinkToFit();
    m_codeBlock->constantsSourceCodeRepresentation().shrinkToFit();
}

void Graph::visitChildren(SlotVisitor& visitor)
{
    for (FrozenValue* value : m_frozenValues) {
        visitor.appendUnbarriered(value->value());
        visitor.appendUnbarriered(value->structure());
    }
}

FrozenValue* Graph::freeze(JSValue value)
{
    if (UNLIKELY(!value))
        return FrozenValue::emptySingleton();

    // There are weird relationships in how optimized CodeBlocks
    // point to other CodeBlocks. We don't want to have them be
    // part of the weak pointer set. For example, an optimized CodeBlock
    // having a weak pointer to itself will cause it to get collected.
    RELEASE_ASSERT(!jsDynamicCast<CodeBlock*>(m_vm, value));
    
    auto result = m_frozenValueMap.add(JSValue::encode(value), nullptr);
    if (LIKELY(!result.isNewEntry))
        return result.iterator->value;

    if (value.isUInt32())
        m_uint32ValuesInUse.append(value.asUInt32());
    
    FrozenValue frozenValue = FrozenValue::freeze(value);
    if (Structure* structure = frozenValue.structure())
        registerStructure(structure);
    
    return result.iterator->value = m_frozenValues.add(frozenValue);
}

FrozenValue* Graph::freezeStrong(JSValue value)
{
    FrozenValue* result = freeze(value);
    result->strengthenTo(StrongValue);
    return result;
}

void Graph::convertToConstant(Node* node, FrozenValue* value)
{
    if (value->structure())
        assertIsRegistered(value->structure());
    node->convertToConstant(value);
}

void Graph::convertToConstant(Node* node, JSValue value)
{
    convertToConstant(node, freeze(value));
}

void Graph::convertToStrongConstant(Node* node, JSValue value)
{
    convertToConstant(node, freezeStrong(value));
}

FrozenValue* Graph::bottomValueMatchingSpeculation(SpeculatedType prediction)
{
    // It probably doesn't matter what we return here.
    if (prediction == SpecNone)
        return freeze(JSValue());

    if (speculationContains(prediction, SpecOther))
        return freeze(jsNull());

    if (speculationContains(prediction, SpecBoolean))
        return freeze(jsBoolean(true));

    if (speculationContains(prediction, SpecFullNumber))
        return freeze(jsNumber(0));

    if (speculationContains(prediction, SpecBigInt))
        return freeze(m_vm.heapBigIntConstantOne.get());

    if (speculationContains(prediction, SpecString | SpecSymbol))
        return freeze(m_vm.smallStrings.emptyString());

    if (speculationContains(prediction, SpecCellOther | SpecObject))
        return freeze(jsNull());

    ASSERT(speculationContains(prediction, SpecEmpty));
    return freeze(JSValue());
}

RegisteredStructure Graph::registerStructure(Structure* structure, StructureRegistrationResult& result)
{
    m_plan.weakReferences().addLazily(structure);
    if (m_plan.watchpoints().consider(structure))
        result = StructureRegisteredAndWatched;
    else
        result = StructureRegisteredNormally;
    return RegisteredStructure::createPrivate(structure);
}

void Graph::registerAndWatchStructureTransition(Structure* structure)
{
    m_plan.weakReferences().addLazily(structure);
    m_plan.watchpoints().addLazily(structure->transitionWatchpointSet());
}

void Graph::assertIsRegistered(Structure* structure)
{
    // It's convenient to be able to call this with a maybe-null structure.
    if (!structure)
        return;

    DFG_ASSERT(*this, nullptr, m_plan.weakReferences().contains(structure));

    if (!structure->dfgShouldWatch())
        return;
    if (watchpoints().isWatched(structure->transitionWatchpointSet()))
        return;
    
    DFG_CRASH(*this, nullptr, toCString("Structure ", pointerDump(structure), " is watchable but isn't being watched.").data());
}

static void logDFGAssertionFailure(
    Graph& graph, const CString& whileText, const char* file, int line, const char* function,
    const char* assertion)
{
    startCrashing();
    dataLog("DFG ASSERTION FAILED: ", assertion, "\n");
    dataLog(file, "(", line, ") : ", function, "\n");
    dataLog("\n");
    dataLog(whileText);
    dataLog("Graph at time of failure:\n");
    graph.dump();
    dataLog("\n");
    dataLog("DFG ASSERTION FAILED: ", assertion, "\n");
    dataLog(file, "(", line, ") : ", function, "\n");
}

void Graph::logAssertionFailure(
    std::nullptr_t, const char* file, int line, const char* function, const char* assertion)
{
    logDFGAssertionFailure(*this, "", file, line, function, assertion);
}

void Graph::logAssertionFailure(
    Node* node, const char* file, int line, const char* function, const char* assertion)
{
    logDFGAssertionFailure(*this, toCString("While handling node ", node, "\n\n"), file, line, function, assertion);
}

void Graph::logAssertionFailure(
    BasicBlock* block, const char* file, int line, const char* function, const char* assertion)
{
    logDFGAssertionFailure(*this, toCString("While handling block ", pointerDump(block), "\n\n"), file, line, function, assertion);
}

CPSCFG& Graph::ensureCPSCFG()
{
    RELEASE_ASSERT(m_form != SSA && !m_isInSSAConversion);
    if (!m_cpsCFG)
        m_cpsCFG = makeUnique<CPSCFG>(*this);
    return *m_cpsCFG;
}

CPSDominators& Graph::ensureCPSDominators()
{
    RELEASE_ASSERT(m_form != SSA && !m_isInSSAConversion);
    if (!m_cpsDominators)
        m_cpsDominators = makeUnique<CPSDominators>(*this);
    return *m_cpsDominators;
}

SSADominators& Graph::ensureSSADominators()
{
    RELEASE_ASSERT(m_form == SSA || m_isInSSAConversion);
    if (!m_ssaDominators)
        m_ssaDominators = makeUnique<SSADominators>(*this);
    return *m_ssaDominators;
}

CPSNaturalLoops& Graph::ensureCPSNaturalLoops()
{
    RELEASE_ASSERT(m_form != SSA && !m_isInSSAConversion);
    ensureCPSDominators();
    if (!m_cpsNaturalLoops)
        m_cpsNaturalLoops = makeUnique<CPSNaturalLoops>(*this);
    return *m_cpsNaturalLoops;
}

SSANaturalLoops& Graph::ensureSSANaturalLoops()
{
    RELEASE_ASSERT(m_form == SSA);
    ensureSSADominators();
    if (!m_ssaNaturalLoops)
        m_ssaNaturalLoops = makeUnique<SSANaturalLoops>(*this);
    return *m_ssaNaturalLoops;
}

BackwardsCFG& Graph::ensureBackwardsCFG()
{
    // We could easily relax this in the future to work over CPS, but today, it's only used in SSA.
    RELEASE_ASSERT(m_form == SSA); 
    if (!m_backwardsCFG)
        m_backwardsCFG = makeUnique<BackwardsCFG>(*this);
    return *m_backwardsCFG;
}

BackwardsDominators& Graph::ensureBackwardsDominators()
{
    RELEASE_ASSERT(m_form == SSA);
    if (!m_backwardsDominators)
        m_backwardsDominators = makeUnique<BackwardsDominators>(*this);
    return *m_backwardsDominators;
}

ControlEquivalenceAnalysis& Graph::ensureControlEquivalenceAnalysis()
{
    RELEASE_ASSERT(m_form == SSA);
    if (!m_controlEquivalenceAnalysis)
        m_controlEquivalenceAnalysis = makeUnique<ControlEquivalenceAnalysis>(*this);
    return *m_controlEquivalenceAnalysis;
}

MethodOfGettingAValueProfile Graph::methodOfGettingAValueProfileFor(Node* currentNode, Node* operandNode)
{
    // This represents IR like `CurrentNode(@operandNode)`. For example: `GetByVal(..., Int32:@GetLocal)`.

    for (Node* node = operandNode; node;) {
        if (node->accessesStack(*this)) {
            if (m_form != SSA && node->operand().isArgument()) {
                int argument = node->operand().toArgument();
                Node* argumentNode = m_rootToArguments.find(block(0))->value[argument];
                // FIXME: We should match SetArgumentDefinitely nodes at other entrypoints as well:
                // https://bugs.webkit.org/show_bug.cgi?id=175841
                if (argumentNode && node->variableAccessData() == argumentNode->variableAccessData()) {
                    CodeBlock* profiledBlock = baselineCodeBlockFor(node->origin.semantic);
                    return &profiledBlock->valueProfileForArgument(argument);
                }
            }
        }

        // currentNode is null when we're doing speculation checks for checkArgumentTypes().
        if (!currentNode || node->origin.semantic != currentNode->origin.semantic || !currentNode->hasResult()) {
            CodeBlock* profiledBlock = baselineCodeBlockFor(node->origin.semantic);

            if (node->accessesStack(*this)) {
                if (node->op() == GetLocal) {
                    return MethodOfGettingAValueProfile::fromLazyOperand(
                        profiledBlock,
                        LazyOperandValueProfileKey(
                            node->origin.semantic.bytecodeIndex(), node->operand()));
                }
            }

            if (node->hasHeapPrediction())
                return &profiledBlock->valueProfileForBytecodeIndex(node->origin.semantic.bytecodeIndex());

            if (profiledBlock->hasBaselineJITProfiling()) {
                if (BinaryArithProfile* result = profiledBlock->binaryArithProfileForBytecodeIndex(node->origin.semantic.bytecodeIndex()))
                    return result;
                if (UnaryArithProfile* result = profiledBlock->unaryArithProfileForBytecodeIndex(node->origin.semantic.bytecodeIndex()))
                    return result;
            }
        }

        switch (node->op()) {
        case BooleanToNumber:
        case Identity:
        case ValueRep:
        case DoubleRep:
        case Int52Rep:
            node = node->child1().node();
            break;
        default:
            node = nullptr;
        }
    }
    
    return MethodOfGettingAValueProfile();
}

bool Graph::getRegExpPrototypeProperty(JSObject* regExpPrototype, Structure* regExpPrototypeStructure, UniquedStringImpl* uid, JSValue& returnJSValue)
{
    unsigned attributesUnused;
    PropertyOffset offset = regExpPrototypeStructure->getConcurrently(uid, attributesUnused);
    if (!isValidOffset(offset))
        return false;

    JSValue value = tryGetConstantProperty(regExpPrototype, regExpPrototypeStructure, offset);
    if (!value)
        return false;

    // We only care about functions and getters at this point. If you want to access other properties
    // you'll have to add code for those types.
    JSFunction* function = jsDynamicCast<JSFunction*>(m_vm, value);
    if (!function) {
        GetterSetter* getterSetter = jsDynamicCast<GetterSetter*>(m_vm, value);

        if (!getterSetter)
            return false;

        returnJSValue = JSValue(getterSetter);
        return true;
    }

    returnJSValue = value;
    return true;
}

bool Graph::isStringPrototypeMethodSane(JSGlobalObject* globalObject, UniquedStringImpl* uid)
{
    ObjectPropertyConditionSet conditions = generateConditionsForPrototypeEquivalenceConcurrently(m_vm, globalObject, globalObject->stringObjectStructure(), globalObject->stringPrototype(), uid);

    if (!conditions.isValid())
        return false;

    ObjectPropertyCondition equivalenceCondition = conditions.slotBaseCondition();
    RELEASE_ASSERT(equivalenceCondition.hasRequiredValue());
    JSFunction* function = jsDynamicCast<JSFunction*>(m_vm, equivalenceCondition.condition().requiredValue());
    if (!function)
        return false;

    if (function->executable()->intrinsicFor(CodeForCall) != StringPrototypeValueOfIntrinsic)
        return false;
    
    return watchConditions(conditions);
}


bool Graph::canOptimizeStringObjectAccess(const CodeOrigin& codeOrigin)
{
    if (hasExitSite(codeOrigin, BadCache) || hasExitSite(codeOrigin, BadConstantCache))
        return false;

    JSGlobalObject* globalObject = globalObjectFor(codeOrigin);
    Structure* stringObjectStructure = globalObjectFor(codeOrigin)->stringObjectStructure();
    registerStructure(stringObjectStructure);
    ASSERT(stringObjectStructure->storedPrototype().isObject());
    ASSERT(stringObjectStructure->storedPrototype().asCell()->classInfo(stringObjectStructure->storedPrototype().asCell()->vm()) == StringPrototype::info());

    if (!watchConditions(generateConditionsForPropertyMissConcurrently(m_vm, globalObject, stringObjectStructure, m_vm.propertyNames->toPrimitiveSymbol.impl())))
        return false;

    // We're being conservative here. We want DFG's ToString on StringObject to be
    // used in both numeric contexts (that would call valueOf()) and string contexts
    // (that would call toString()). We don't want the DFG to have to distinguish
    // between the two, just because that seems like it would get confusing. So we
    // just require both methods to be sane.
    if (!isStringPrototypeMethodSane(globalObject, m_vm.propertyNames->valueOf.impl()))
        return false;
    return isStringPrototypeMethodSane(globalObject, m_vm.propertyNames->toString.impl());
}

bool Graph::willCatchExceptionInMachineFrame(CodeOrigin codeOrigin, CodeOrigin& opCatchOriginOut, HandlerInfo*& catchHandlerOut)
{
    if (!m_hasExceptionHandlers)
        return false;

    BytecodeIndex bytecodeIndexToCheck = codeOrigin.bytecodeIndex();
    while (1) {
        InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame();
        CodeBlock* codeBlock = baselineCodeBlockFor(inlineCallFrame);
        if (HandlerInfo* handler = codeBlock->handlerForBytecodeIndex(bytecodeIndexToCheck)) {
            opCatchOriginOut = CodeOrigin(BytecodeIndex(handler->target), inlineCallFrame);
            catchHandlerOut = handler;
            return true;
        }

        if (!inlineCallFrame)
            return false;

        bytecodeIndexToCheck = inlineCallFrame->directCaller.bytecodeIndex();
        codeOrigin = inlineCallFrame->directCaller;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool Graph::canDoFastSpread(Node* node, const AbstractValue& value)
{
    // The parameter 'value' is the AbstractValue for child1 (the thing being spread).
    ASSERT(node->op() == Spread);

    if (node->child1().useKind() != ArrayUse) {
        // Note: we only speculate on ArrayUse when we've set up the necessary watchpoints
        // to prove that the iteration protocol is non-observable starting from ArrayPrototype.
        return false;
    }

    // FIXME: We should add profiling of the incoming operand to Spread
    // so we can speculate in such a way that we guarantee that this
    // function would return true:
    // https://bugs.webkit.org/show_bug.cgi?id=171198

    if (!value.m_structure.isFinite())
        return false;

    ArrayPrototype* arrayPrototype = globalObjectFor(node->child1()->origin.semantic)->arrayPrototype();
    bool allGood = true;
    value.m_structure.forEach([&] (RegisteredStructure structure) {
        allGood &= structure->hasMonoProto()
            && structure->storedPrototype() == arrayPrototype
            && !structure->isDictionary()
            && structure->getConcurrently(m_vm.propertyNames->iteratorSymbol.impl()) == invalidOffset
            && !structure->mayInterceptIndexedAccesses();
    });

    return allGood;
}

void Graph::clearCPSCFGData()
{
    m_cpsNaturalLoops = nullptr;
    m_cpsDominators = nullptr;
    m_cpsCFG = nullptr;
}

void Prefix::dump(PrintStream& out) const
{
    if (!m_enabled)
        return;

    if (!noHeader) {
        if (nodeIndex >= 0)
            out.printf("%3d ", nodeIndex);
        else
            out.printf("    ");

        if (blockIndex >= 0)
            out.printf("%2d ", blockIndex);
        else
            out.printf("   ");

        if (phaseNumber >= 0)
            out.printf("%2d: ", phaseNumber);
        else
            out.printf("  : ");
    }
    if (prefixStr)
        out.printf("%s", prefixStr);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
