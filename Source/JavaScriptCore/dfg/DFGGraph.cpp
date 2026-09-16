/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "CodeBlockWithJITType.h"
#include "DFGBackwardsCFG.h"
#include "DFGBackwardsDominators.h"
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
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "OperandsInlines.h"
#include "ProfilerSupport.h"
#include "RegExpObject.h"
#include "SlotVisitorInlines.h"
#include "Snippet.h"
#include "StackAlignment.h"
#include "StructureInlines.h"
#include <array>
#include <wtf/CommaPrinter.h>
#include <wtf/GraphOrdering.h>
#include <wtf/ListDump.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace DFG {

static constexpr bool dumpOSRAvailabilityData = false;

// Creates an array of stringized names.
static constexpr auto dfgOpNames = WTF::toArray<ASCIILiteral>({
#define STRINGIZE_DFG_OP_ENUM(opcode, flags) #opcode ## _s ,
    FOR_EACH_DFG_OP(STRINGIZE_DFG_OP_ENUM)
#undef STRINGIZE_DFG_OP_ENUM
});

Graph::Graph(VM& vm, Plan& plan)
    : m_vm(vm)
    , m_plan(plan)
    , m_codeBlock(m_plan.codeBlock())
    , m_profiledBlock(m_codeBlock->alternative())
    , m_ssaCFG(makeUniqueWithoutFastMallocCheck<SSACFG>(*this))
    , m_nextMachineLocal(0)
    , m_fixpointState(BeforeFixpoint)
    , m_structureRegistrationState(HaveNotStartedRegistering)
    , m_form(LoadStore)
    , m_unificationState(LocallyUnified)
    , m_refCountState(EverythingIsLive)
{
    ASSERT(m_profiledBlock);
    
    m_hasDebuggerEnabled = m_profiledBlock->wasCompiledWithDebuggingOpcodes() || Options::forceDebuggerBytecodeGeneration();
    
    m_indexingCache = makeUniqueWithoutFastMallocCheck<FlowIndexing>(*this);
    m_abstractValuesCache = makeUniqueWithoutFastMallocCheck<FlowMap<AbstractValue>>(*this);

    registerStructure(vm.structureStructure.get());
    this->stringStructure = registerStructure(vm.stringStructure.get());
    this->symbolStructure = registerStructure(vm.symbolStructure.get());

    if (Options::dumpIonGraph()) {
        m_ionGraphFunction = JSON::Object::create();
        auto passes = JSON::Array::create();
        m_ionGraphPasses = passes.get();
        m_ionGraphFunction->setString("name"_s, m_codeBlock->inferredNameWithHash());
        m_ionGraphFunction->setString("tier"_s, m_plan.isFTL() ? "FTL"_s : "DFG"_s);
        m_ionGraphFunction->setBoolean("osr"_s, m_plan.mode() == JITCompilationMode::FTLForOSREntry);
        m_ionGraphFunction->setArray("passes"_s, WTF::move(passes));
    }
}

Graph::~Graph()
{
    dumpAndReleaseIonGraph();
}

ASCIILiteral Graph::opName(NodeType op)
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

void Graph::dump(PrintStream& out, const char* prefixStr, Node* node, DumpContext* context, bool inIonGraph)
{
    NodeType op = node->op();

    unsigned refCount = node->refCount();
    bool mustGenerate = node->mustGenerate();
    if (mustGenerate)
        --refCount;

    CommaPrinter comma;
    if (inIonGraph) {
        out.print(opName(op), "("_s);
        DFG_NODE_DO_TO_CHILDREN(*this, node, [&](Node*, Edge edge) {
            out.print(comma);
            if (!edge.isProved())
                out.print("Check:");
            out.print(edge.useKind(), ":");
            if (DFG::doesKill(edge.killStatusUnchecked()))
                out.print("Kill:");
            out.print(opName(edge->op()), "#"_s, edge->index());
        });
    } else {
        Prefix myPrefix(prefixStr);
        Prefix& prefix = prefixStr ? myPrefix : m_prefix;
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
            out.print("-"_s);
        out.print(">\t"_s, opName(op), "("_s);
        DFG_NODE_DO_TO_CHILDREN(*this, node, [&](Node*, Edge edge) {
            out.print(comma, edge);
        });
    }

    if (toUTF8CString(NodeFlagsDump(node->flags())) != "<empty>"_s)
        out.print(comma, NodeFlagsDump(node->flags()));
    if (node->prediction())
        out.print(comma, SpeculationDump(node->prediction()));
    if (node->hasNumberOfArgumentsToSkip())
        out.print(comma, "numberOfArgumentsToSkip = "_s, node->numberOfArgumentsToSkip());
    if (node->hasNumberOfBoundArguments())
        out.print(comma, "numberOfBoundArguments = "_s, node->numberOfBoundArguments());
    if (node->hasArrayMode())
        out.print(comma, node->arrayMode());
    if (node->hasArrayModes())
        out.print(comma, ArrayModesDump(node->arrayModes()));
    if (node->hasArithUnaryType())
        out.print(comma, "Type:"_s, node->arithUnaryType());
    if (node->hasArithMode())
        out.print(comma, node->arithMode());
    if (node->hasArithRoundingMode())
        out.print(comma, "Rounding:"_s, node->arithRoundingMode());
    if (node->hasScopeOffset())
        out.print(comma, node->scopeOffset());
    if (node->hasDirectArgumentsOffset())
        out.print(comma, node->capturedArgumentsOffset());
    if (node->hasArgumentIndex())
        out.print(comma, node->argumentIndex());
    if (node->hasRegisterPointer())
        out.print(comma, "global", "("_s, RawPointer(node->variablePointer()), ")"_s);
    if (node->hasIdentifier() && node->identifierNumber() != UINT32_MAX)
        out.print(comma, "id"_s, node->identifierNumber(), "{"_s, identifiers()[node->identifierNumber()], "}"_s);
    if (node->hasCacheableIdentifier() && node->cacheableIdentifier())
        out.print(comma, "cachable-id {"_s, node->cacheableIdentifier(), "}"_s);
    if (node->hasCacheType())
        out.print(comma, node->cacheType());
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
        out.print(", ID:"_s, node->transition()->next->id().bits());
    }
    if (node->op() == CheckFieldType) {
        if (FieldTypeRecord* record = node->fieldTypeRecord()) {
            out.print(comma, "owner:"_s, record->owner().bits(), ", offset:"_s, record->offset(),
                ", expected:"_s, record->expected().bits());
            if (record->isGeneralized())
                out.print(", generalized"_s);
        } else
            out.print(comma, "<null record>"_s);
    }
    if (node->hasCellOperand()) {
        if (!node->cellOperand()->value() || !node->cellOperand()->value().isCell())
            out.print(comma, "invalid cell operand: "_s, node->cellOperand()->value());
        else {
            out.print(comma, pointerDump(node->cellOperand()->value().asCell()));
            if (node->cellOperand()->value().isCell()) {
                CallVariant variant(node->cellOperand()->value().asCell());
                if (ExecutableBase* executable = variant.executable()) {
                    if (executable->isHostFunction())
                        out.print(comma, "<host function>"_s);
                    else if (FunctionExecutable* functionExecutable = dynamicDowncast<FunctionExecutable>(executable))
                        out.print(comma, FunctionExecutableDump(functionExecutable));
                    else
                        out.print(comma, "<non-function executable>"_s);
                }
            }
        }
    }
    if (node->hasQueriedType()) {
        JSTypeRange range = node->queriedType();
        if (range.first == range.last)
            out.print(comma, range.first);
        else
            out.print(comma, range.first, "..."_s, range.last);
    }
    if (node->hasStructureFlags())
        out.print(comma, node->structureFlags());
    if (node->hasStorageAccessData()) {
        StorageAccessData& storageAccessData = node->storageAccessData();
        out.print(comma, "id"_s, storageAccessData.identifierNumber, "{"_s, identifiers()[storageAccessData.identifierNumber], "}"_s);
        out.print(", "_s, static_cast<ptrdiff_t>(storageAccessData.offset));
    }
    if (node->hasMultiGetByOffsetData()) {
        MultiGetByOffsetData& data = node->multiGetByOffsetData();
        out.print(comma, "id", data.identifierNumber, "{", identifiers()[data.identifierNumber], "}");
        for (unsigned i = 0; i < data.cases.size(); ++i)
            out.print(comma, inContext(data.cases[i], context));
    }
    if (node->hasMultiPutByOffsetData()) {
        MultiPutByOffsetData& data = node->multiPutByOffsetData();
        out.print(comma, "id"_s, data.identifierNumber, "{"_s, identifiers()[data.identifierNumber], "}"_s);
        for (unsigned i = 0; i < data.variants.size(); ++i)
            out.print(comma, inContext(data.variants[i], context));
    }
    if (node->hasMultiDeleteByOffsetData()) {
        MultiDeleteByOffsetData& data = node->multiDeleteByOffsetData();
        out.print(comma, "id"_s, data.identifierNumber, "{"_s, identifiers()[data.identifierNumber], "}"_s);
        for (unsigned i = 0; i < data.variants.size(); ++i)
            out.print(comma, inContext(data.variants[i], context));
    }
    if (node->hasMatchStructureData()) {
        for (MatchStructureVariant& variant : node->matchStructureData().variants)
            out.print(comma, inContext(*variant.structure.get(), context), "=>"_s, variant.result);
    }
    ASSERT(node->hasVariableAccessData(*this) == node->accessesStack(*this));
    if (node->hasVariableAccessData(*this)) {
        VariableAccessData* variableAccessData = node->tryGetVariableAccessData();
        if (variableAccessData) {
            Operand operand = variableAccessData->operand();
            out.print(comma, variableAccessData->operand(), "("_s, VariableAccessDataDump(*this, variableAccessData), ")"_s);
            operand = variableAccessData->machineLocal();
            if (operand.isValid())
                out.print(comma, "machine:"_s, operand);
        }
    }
    if (node->hasStackAccessData()) {
        StackAccessData* data = node->stackAccessData();
        out.print(comma, data->operand);
        if (data->machineLocal.isValid())
            out.print(comma, "machine:"_s, data->machineLocal);
        out.print(comma, data->format);
    }
    if (node->hasUnlinkedOperand())
        out.print(comma, node->unlinkedOperand());
    if (node->hasVectorLengthHint())
        out.print(comma, "vectorLengthHint = "_s, node->vectorLengthHint());
    if (node->hasLazyJSValue())
        out.print(comma, node->lazyJSValue());
    if (node->hasIndexingType())
        out.print(comma, IndexingTypeDump(node->indexingMode()));
    if (node->hasTypedArrayType())
        out.print(comma, node->typedArrayType());
    if (node->hasPhi())
        out.print(comma, "^"_s, node->phi()->index());
    if (node->hasExecutionCounter())
        out.print(comma, RawPointer(node->executionCounter()));
    if (node->hasWatchpointSet())
        out.print(comma, RawPointer(node->watchpointSet()));
    if (node->hasStoragePointer())
        out.print(comma, RawPointer(node->storagePointer()));
    if (node->hasObjectMaterializationData())
        out.print(comma, node->objectMaterializationData());
    if (node->hasCallVarargsData())
        out.print(comma, "firstVarArgOffset = "_s, node->callVarargsData()->firstVarArgOffset);
    if (node->hasLoadVarargsData()) {
        LoadVarargsData* data = node->loadVarargsData();
        out.print(comma, "start = "_s, data->start, ", count = "_s, data->count);
        if (data->machineStart.isValid())
            out.print(", machineStart = ", data->machineStart);
        if (data->machineCount.isValid())
            out.print(", machineCount = "_s, data->machineCount);
        out.print(", offset = "_s, data->offset, ", mandatoryMinimum = "_s, data->mandatoryMinimum);
        out.print(", limit = "_s, data->limit);
    }
    if (node->hasInternalFieldIndex())
        out.print(comma, "internalFieldIndex = "_s, node->internalFieldIndex());
    if (node->hasCallDOMGetterData()) {
        CallDOMGetterData* data = node->callDOMGetterData();
        out.print(comma, "id"_s, data->identifierNumber, "{"_s, identifiers()[data->identifierNumber], "}"_s);
        out.print(", domJIT = "_s, RawPointer(data->domJIT));
    }
    if (node->hasIgnoreLastIndexIsWritable())
        out.print(comma, "ignoreLastIndexIsWritable = "_s, node->ignoreLastIndexIsWritable());
    if (node->hasIntrinsic())
        out.print(comma, "intrinsic = "_s, node->intrinsic());
    if (node->isConstant())
        out.print(comma, pointerDumpInContext(node->constant(), context));
    if (node->hasCallLinkStatus())
        out.print(comma, *node->callLinkStatus());
    if (node->hasGetByStatus())
        out.print(comma, *node->getByStatus());
    if (node->hasInByStatus())
        out.print(comma, *node->inByStatus());
    if (node->hasPutByStatus())
        out.print(comma, *node->putByStatus());
    if (node->hasEnumeratorMetadata())
        out.print(comma, "enumeratorModes = "_s, node->enumeratorMetadata().toRaw());
    if (node->hasExtractOffset())
        out.print(comma, "<<"_s, node->extractOffset());
    if (node->isJump())
        out.print(comma, "T:"_s, *node->targetBlock());
    if (node->isBranch())
        out.print(comma, "T:"_s, node->branchData()->taken, ", F:"_s, node->branchData()->notTaken);
    if (node->isSwitch()) {
        SwitchData* data = node->switchData();
        out.print(comma, data->kind);
        for (unsigned i = 0; i < data->cases.size(); ++i)
            out.print(comma, inContext(data->cases[i].value, context), ":"_s, data->cases[i].target);
        out.print(comma, "default:"_s, data->fallThrough);
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
        out.print(comma, "R:"_s, sortedListDump(reads.direct(), ","_s));
    if (!writes.isEmpty())
        out.print(comma, "W:"_s, sortedListDump(writes.direct(), ","_s));
    ExitMode exitMode = mayExit(*this, node);
    if (exitMode != DoesNotExit)
        out.print(comma, exitMode);
    if (clobbersExitState(*this, node))
        out.print(comma, "ClobbersExit"_s);
    if (node->origin.isSet()) {
        out.print(comma);
        node->origin.semantic.bytecodeIndex().dump(out, inIonGraph);
        if (node->origin.semantic != node->origin.forExit && node->origin.forExit.isSet()) {
            out.print(comma, "exit: "_s);
            node->origin.forExit.dump(out, inIonGraph);
        }
    }
    out.print(comma, node->origin.exitOK ? "ExitValid"_s : "ExitInvalid"_s);
    if (node->origin.wasHoisted)
        out.print(comma, "WasHoisted"_s);
    out.print(")");

    if (node->accessesStack(*this) && node->tryGetVariableAccessData())
        out.print("  predicting "_s, SpeculationDump(node->tryGetVariableAccessData()->prediction()));
    else if (node->hasHeapPrediction())
        out.print("  predicting "_s, SpeculationDump(node->getHeapPrediction()));

    if (!inIonGraph)
        out.print("\n"_s);
}

bool Graph::terminalsAreValid()
{
    for (BasicBlock* block : blocksInNaturalOrder()) {
        if (!block->terminal())
            return false;
    }
    return true;
}

static BasicBlock* NODELETE unboxLoopNode(const CPSCFG::Node& node) { return node.node(); }
static BasicBlock* NODELETE unboxLoopNode(BasicBlock* block) { return block; }

void Graph::dumpBlockHeader(PrintStream& out, const char* prefixStr, BasicBlock* block, PhiNodeDumpMode phiNodeDumpMode, DumpContext* context)
{
    Prefix myPrefix(prefixStr);
    Prefix& prefix = prefixStr ? myPrefix : m_prefix;

    out.print(prefix, "Block ", *block);
#if ASSERT_ENABLED
    if (block->cloneSource)
        out.print("<-", block->cloneSource);
#endif
    out.print(" (", inContext(block->at(0)->origin.semantic, context), "):", block->isReachable ? "" : " (skipped)", block->isOSRTarget ? " (OSR target)" : "", block->isCatchEntrypoint ? " (Catch Entrypoint)" : "", "\n");
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
                sortedBlockList.append(unboxLoopNode(loop->at(i))->index());
            std::ranges::sort(sortedBlockList);
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

            out.print(" D@", phiNode->index(), "<", phiNode->operand(), ",", phiNode->refCount());
            if (toUTF8CString(NodeFlagsDump(phiNode->flags())) != "<empty>"_s)
                out.print(", ", NodeFlagsDump(phiNode->flags()));
            out.print(">->(");
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
            out.print(prefix, "  Arguments for block#", pair.key->index(), ": ", listDump(pair.value), "\n");
    }
    out.print("\n");
    
    Node* lastNode = nullptr;
    for (size_t b = 0; b < m_blocks.size(); ++b) {
        BasicBlock* block = m_blocks[b].get();
        if (!block)
            continue;
        prefix.blockIndex = block->index();
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
            if (block->intersectionOfCFAHasVisited && block->intersectionOfPastValuesAtHead.size())
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
    for (FrozenValue& value : m_frozenValues) {
        if (value.pointsToHeap())
            out.print(prefix, "    ", inContext(value, &myContext), "\n");
    }

    out.print(inContext(watchpoints(), &myContext));
    
    if (!myContext.isEmpty()) {
        StringPrintStream prefixStr;
        prefixStr.print(prefix);
        myContext.dump(out, prefixStr.toUTF8CString().legacyCStringPointer());
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

void Graph::clearAbstractValues()
{
    m_abstractValuesCache->clear();
}

void Graph::dethread()
{
    if (m_form == LoadStore || m_form == SSA)
        return;

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

void Graph::clearReachability()
{
    for (BlockIndex blockIndex = m_blocks.size(); blockIndex--;) {
        BasicBlock* block = m_blocks[blockIndex].get();
        if (!block)
            continue;
        block->isReachable = false;
        block->predecessors.clear();
    }
}

void Graph::resetReachability()
{
    clearReachability();
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
        for (auto& tupleData : m_graph.m_tupleData)
            tupleData.refCount = 0;

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
        countNode(edge.node());
    }
    
    void countNode(Node* node)
    {
        if (node->postfixRef())
            return;
        m_worklist.append(node);
    }
    
    void countEdge(Node* node, Edge edge)
    {
        // Don't count edges that are already counted for their type checks.
        if (!(edge.isProved() || edge.willNotHaveCheck()))
            return;
        // Tuples are special and have a reference count for each result.
        if (node->op() == ExtractFromTuple)
            m_graph.m_tupleData.at(node->tupleIndex()).refCount++;
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
    CFG cfg(*this);
    appendNodesInOrder(cfg, m_roots, GraphOrder::PreOrder, result);

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
    CFG cfg(*this);
    appendNodesInOrder(cfg, m_roots, GraphOrder::PostOrder, result);

    if (isSafeToValidate && validationEnabled()) { // There are users of this where we haven't yet built the CFG enough to be able to run dominators.
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
    if (m_plan.isUnlinked())
        return false;

    if (!key.isWatchable(PropertyCondition::MakeNoChanges))
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
    if (m_plan.isUnlinked())
        return false;

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

GetByOffsetMethod Graph::promoteToConstant(GetByOffsetMethod method)
{
    if (method.kind() == GetByOffsetMethod::LoadFromPrototype
        && tryWatch(method.prototype()->structure())) {
        if (JSValue constant = tryGetConstantProperty(method.prototype()->value(), method.prototype()->structure(), method.offset()))
            return GetByOffsetMethod::constant(freeze(constant));
    }

    return method;
}

bool Graph::watchGlobalProperty(JSGlobalObject* globalObject, unsigned identifierNumber)
{
    if (m_plan.isUnlinked())
        return false;

    UniquedStringImpl* uid = identifiers()[identifierNumber];
    // If we already have a WatchpointSet, and it is already invalidated, it means that this scope operation must be changed from GlobalProperty to GlobalLexicalVar,
    // but we still have stale metadata here since we have not yet executed this bytecode operation since the invalidation. Just emitting ForceOSRExit to update the
    // metadata when it reaches to this code.
    if (auto* watchpoint = globalObject->getReferencedPropertyWatchpointSet(uid)) {
        if (!watchpoint->isStillValid())
            return false;
    }
    watchpoints().addLazily(DesiredGlobalProperty(globalObject, identifierNumber));
    return true;
}

FullBytecodeLiveness& Graph::livenessFor(CodeBlock* codeBlock)
{
    UncheckedKeyHashMap<CodeBlock*, std::unique_ptr<FullBytecodeLiveness>>::iterator iter = m_bytecodeLiveness.find(codeBlock);
    if (iter != m_bytecodeLiveness.end())
        return *iter->value;
    
    std::unique_ptr<FullBytecodeLiveness> liveness = codeBlock->livenessAnalysis().computeFullLiveness(codeBlock);
    FullBytecodeLiveness& result = *liveness;
    m_bytecodeLiveness.add(codeBlock, WTF::move(liveness));
    return result;
}

FullBytecodeLiveness& Graph::livenessFor(InlineCallFrame* inlineCallFrame)
{
    return livenessFor(baselineCodeBlockFor(inlineCallFrame));
}

bool Graph::isLiveInBytecode(Operand operand, CodeOrigin codeOrigin)
{
    static constexpr bool verbose = false;
    
    dataLogLnIf(verbose, "Checking of operand is live: ", operand);
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
        
        dataLogLnIf(verbose, "reg = ", reg);

        if (operand.virtualRegister().offset() < codeOriginPtr->stackOffset() + CallFrame::headerSizeInRegisters) {
            if (reg.isArgument()) {
                RELEASE_ASSERT(reg.offset() < CallFrame::headerSizeInRegisters);


                if (inlineCallFrame->isClosureCall
                    && reg == CallFrameSlot::callee) {
                    dataLogLnIf(verbose, "Looks like a callee.");
                    return true;
                }
                
                if (inlineCallFrame->isVarargs()
                    && reg == CallFrameSlot::argumentCountIncludingThis) {
                    dataLogLnIf(verbose, "Looks like the argument count.");
                    return true;
                }
                
                return false;
            }

            dataLogLnIf(verbose, "Asking the bytecode liveness.");
            CodeBlock* codeBlock = baselineCodeBlockFor(inlineCallFrame);
            FullBytecodeLiveness& fullLiveness = livenessFor(codeBlock);
            BytecodeIndex bytecodeIndex = codeOriginPtr->bytecodeIndex();
            return fullLiveness.virtualRegisterIsLive(reg, bytecodeIndex, appropriateLivenessCalculationPoint(*codeOriginPtr, isCallerOrigin));
        }

        // Arguments are always live. This would be redundant if it wasn't for our
        // op_call_varargs inlining.
        if (inlineCallFrame && reg.isArgument()
            && static_cast<size_t>(reg.toArgument()) < inlineCallFrame->m_argumentsWithFixup.size()) {
            dataLogLnIf(verbose, "Argument is live.");
            return true;
        }

        isCallerOrigin = true;
    }

    if (operand.isTmp())
        return false;

    dataLogLnIf(verbose, "Ran out of stack, returning true.");
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
    for (InlineCallFrameSet::iterator iter = m_plan.inlineCallFrames().unsafeGet()->begin(); !!iter; ++iter) {
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
    if (m_plan.isUnlinked())
        return JSValue();

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
        
        watchpoints().addLazily(*set);
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

    JSValue result;
    auto set = structureSet.toStructureSet();
    {
        Locker cellLock { object->cellLock() };
        Structure* structure = object->structure();
        if (!set.contains(structure))
            return JSValue();
        result = object->getDirectConcurrently(cellLock, structure, offset);
    }

    if (!result)
        return JSValue();

    // If all structures are watched, we don't need to consider whether object transitions and changes the value.
    // If the object gets transition while compiling, then it invalidates the code.
    bool allAreWatchable = true;
    for (unsigned i = structureSet.size(); i--;) {
        RegisteredStructure structure = structureSet[i];
        if (!structure->dfgMayWatch()) {
            allAreWatchable = false;
            break;
        }
    }
    if (allAreWatchable) {
        for (unsigned i = structureSet.size(); i--;)
            watch(structureSet[i].get());
        return result;
    }

    // However, if structures transitions are not watched, then object can get to the one of the structures transitively while it is changing the value.
    // But we can still optimize it if StructureSet is only one: in that case, there is no way to fulfill Structure requirement while changing the property
    // and avoiding the replacement watchpoint firing.
    if (structureSet.size() != 1)
        return JSValue();

    return result;
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

// Turns the runtime-recorded field type into an abstract value; a clear result means no information and the
// caller falls back to heapTop. Never infer a field type from this compilation's own speculation:
// CheckStructure(GetByOffset(...)) shows the field held T at some point, not always, and using it is unsound.
// Soundness rests on three mechanisms: the C++ store paths generalize the record (JSObjectInlines.h
// maintainFieldTypeRecord); every JIT and IC store path either checks inline or declines the site and
// generalizes, so do not delete that store-side machinery; and tryWatch() below, which catches the object
// already in the field transitioning with no store to the field at all.
AbstractValue Graph::fieldTypeAssumptionValue(
    RegisteredStructure baseStructure, PropertyOffset offset, StructureClobberState clobberState,
    FieldTypeNarrowingGuard guard)
{
    AbstractValue result;

    if (!Options::useFieldTypeAssumptions())
        return result;
    if (!Options::useFieldTypeNarrowing()) [[unlikely]]
        return result;
    if (!baseStructure || offset == invalidOffset)
        return result;
    // Unlinked code cannot hold structure-identity dependencies.
    if (m_plan.isUnlinked())
        return result;

    auto* table = m_vm.fieldTypeWatchpoints();
    if (!table || !table->sizeRelaxed())
        return result;

    bool logging = Options::logFieldTypes();

    // Offsets are inherited by every descendant of the structure that introduced them, so the record
    // is keyed on the owner. Safe off-thread: previousID() is written for concurrent use.
    Structure* loadSite = baseStructure.get();
    Structure* owner = fieldTypeOwnerFor(loadSite, offset);
    if (!owner) {
        if (logging) [[unlikely]]
            dataLogLn("[fieldtype] DECLINE no-owner loadSite=", loadSite->id().bits(), " offset=", offset);
        return result;
    }

    RefPtr record = fieldTypeRecordFor(owner, offset);
    StructureID expectedID = fieldTypeExpectedFor(owner, offset);
    if (!expectedID) {
        if (logging) [[unlikely]] {
            // "absent" means no creation path recorded this field, a coverage bug, while "generalized" means
            // the field really is polymorphic. Ask the table, since fieldTypeRecordFor nulls out both.
            const char* why = "absent";
            if (auto* table = m_vm.fieldTypeWatchpoints(); table && table->hasEntryFor(owner->id(), offset))
                why = "generalized";
            dataLogLn("[fieldtype] DECLINE ", why, " owner=", owner->id().bits(), " offset=", offset);
        }
        return result;
    }

    // No property-replacement-watchpoint requirement, which cost 7.4% on delta-blue: every tier checks
    // replace stores inline instead, and the paths with nowhere to bake an expected StructureID (the
    // PutByIdReplaceHandler thunk, the InlineAccess stub, the LLInt metadata cache) decline and generalize.

    Structure* expected = expectedID.decode();
    if (!expected)
        return result;

    // BLAST-RADIUS CAP. A withdrawal fires a CodeBlockJettisoningWatchpoint, and JSC's DFG has exactly one
    // currency for a watchpoint-backed assumption -- every DesiredWatchpoints path installs one, so there is no
    // cheap "exit instead of jettison" to move onto. What IS controllable is how many CodeBlocks a single
    // withdrawal can discard. Measured on raytrace: THREE claims (direction 107 dependents, color 106,
    // position 69) account for 282 invalidated CodeBlocks and the whole 5.2-point regression, and the claims are
    // contradicted only after those dependents have accrued, so no admission or provenance rule can see it coming
    // (seven count axes plus provenance are all closed, todo/24).
    //
    // Declining to narrow is SOUND unconditionally: it forgoes an optimisation and registers no dependency.
    if (unsigned cap = Options::fieldTypeMaxDependentsPerClaim()) [[unlikely]] {
        if (record->dependents() >= cap) {
            if (logging) [[unlikely]]
                dataLogLn("[fieldtype] DECLINE dependent-cap owner=", owner->id().bits(), " offset=", offset,
                    " dependents=", record->dependents());
            return result;
        }
    }

    if (!tryWatch(expected)) {
        if (logging) [[unlikely]]
            dataLogLn("[fieldtype] DECLINE not-watchable expected=", expectedID.bits());
        return result;
    }

    // BLAST-RADIUS CAP, measured for acorn-wtb 2026-08-26. The cap above is the whole mechanism; the maxima that
    // make it usable are: delta-blue 556 dependents on its widest claim, acorn-wtb 2,544 and 738, babel-wtb 32.
    // A threshold in (556, 738) therefore blocks acorn's two widest claims -- including Token.type, whose 738
    // dependents span 29 functions and take 22 CodeBlocks down at one instant (verified/17) -- while declining
    // NOTHING on delta-blue. It cannot help babel-wtb, whose regression is therefore not fan-out driven.
    // PAY FOR WHAT YOU USE. If this claim has been narrowed on repeatedly and has never once let
    // DFGConstantFoldingPhase fold a structure check, the narrowing is buying nothing while its dependency exposes
    // every consumer to a jettison on withdrawal -- acorn-wtb's 4,498 narrowings fold ~2 checks and cost 36
    // CodeBlocks (verified/17, verified/19). Declining is sound with no further argument: it forgoes an
    // optimisation and registers no dependency. See FieldTypeRecord::noteNarrowing for the measured densities.
    if (Options::useFieldTypeFoldGatedNarrowing() && record) [[unlikely]] {
        if (record->narrowingHasProvedWorthless()) {
            if (logging) [[unlikely]] {
                dataLogLn("[fieldtype] DECLINE foldless owner=", owner->id().bits(), " offset=", offset,
                    " narrowings=", record->narrowings(), " folds=0");
            }
            return result;
        }
        record->noteNarrowing();
    }


    // Fired whenever a store generalizes the field, which jettisons this compilation.
    //
    // CEILING MEASUREMENT for the only untried lever on the raytrace family. These two lines are where the 282
    // dependents come from -- store-side registration sheds none of them (REGISTER-WATCHPOINT 61 -> 0 leaves the
    // counts at 121/107/60). The sound fix would emit a CheckFieldType at the narrowed load, which loads
    // record->addressOfExpected() at runtime and needs no dependency, so a withdrawal would cost one OSR exit per
    // function instead of jettisoning every dependent. That is a restructuring of how the AI and
    // ConstantFoldingPhase cooperate, since narrowing happens in the abstract interpreter which cannot insert
    // nodes. Skipping the registration measures the BENEFIT half alone -- UNSOUND (narrowed code is never
    // invalidated) but it bounds what the sound version could recover before it is worth building.
    // HALF 2: with a CheckFieldType inserted at the load (DFGConstantFoldingPhase), the narrowing is justified by
    // a runtime check rather than by this dependency, so the compilation must not be jettisoned on withdrawal --
    // it exits and recompiles instead. Keep the record alive because CheckFieldType bakes its claim-slot address.
    //
    // FIXED 2026-08-26. This used to `return result` here, i.e. narrow NOTHING, so the option measured
    // `useFieldTypeNarrowing=0` PLUS redundant checks -- which is how a ~12.5-point item was retired on a
    // "delta-blue -13.2%" taken against zero narrowing. It now falls through and narrows; the only thing the
    // option changes is WHO justifies the narrowing, a runtime check instead of a jettisoning watchpoint.
    //
    // Requires a live `record`: the check bakes record->addressOfExpected(), so a word-only claim (no entry, hence
    // no address) has nothing to load and must keep the watchpoint. Requires LoadedCheck: MultiGetByOffset gets no
    // check inserted, so dropping its dependency would leave a narrowing that outlives the claim.
    bool narrowingIsGuardedByLoadedCheck = Options::useFieldTypeConsumerLoadedCheck()
        && !Options::fieldTypeMeasureLoadedCheckCostOnly()
        && guard == FieldTypeNarrowingGuard::LoadedCheck
        && record;
    if (narrowingIsGuardedByLoadedCheck) [[unlikely]]
        m_plan.keepFieldTypeRecordAlive(*record);

    // RISK PROBE: keep the record alive so the inserted CheckFieldType has a slot address to bake, but do NOT
    // return -- fall through and narrow as usual. delta-blue then pays the check while keeping the folding, which
    // isolates the cost of loading from the loss of narrowing that the broken leg was measuring.
    if (Options::fieldTypeMeasureLoadedCheckCostOnly()) [[unlikely]]
        m_plan.keepFieldTypeRecordAlive(*record);

    if (logging) [[unlikely]] {
        // The compiling CodeBlock is what makes this actionable: the (owner, offset) pair alone cannot say which
        // function the narrowing lands in, so a disassembly-level comparison has no target to aim at.
        dataLogLn("[fieldtype] NARROW loadSite=", loadSite->id().bits(), " owner=", owner->id().bits(),
            " offset=", offset, " expected=", expectedID.bits(),
            " inFunc=", m_codeBlock->inferredNameWithHash(),
            " tier=", m_codeBlock->jitType());
    }

    if (Options::useDollarVM()) [[unlikely]]
        m_vm.fieldTypeNarrowCount().fetch_add(1, std::memory_order_relaxed);
    // The runtime check subsumes the dependency: CheckFieldType reloads the claim on every execution, so a
    // withdrawal makes the compare fail and the site OSR-exits. Registering here as well would reintroduce exactly
    // the jettison this is removing -- 36 CodeBlocks on acorn-wtb from 3 withdrawals, one of which fans out to 738
    // dependents and takes 22 CodeBlocks down at the same instant.
    if (Options::useFieldTypeConsumerWatchpointRegistration() && !narrowingIsGuardedByLoadedCheck) [[likely]] {
        record->noteDependent();
        watchpoints().addLazily(record->watchpoints());
    }

    result.set(*this, registerStructure(expected));
    if (clobberState == StructuresAreClobbered)
        result.clobberStructures(*this);
    return result;
}

// The owner comes from StorageAccessData, not the base's structure: at a creating PutByOffset the base still
// carries the old structure (PutByOffset then PutStructure), which does not own the offset.
RefPtr<FieldTypeRecord> Graph::fieldTypeRecordForStore(const StorageAccessData& data)
{
    if (!Options::useFieldTypeAssumptions())
        return nullptr;
    if (!data.fieldTypeOwner || data.offset == invalidOffset)
        return nullptr;

    auto* table = m_vm.fieldTypeWatchpoints();
    if (!table)
        return nullptr;

    // recordForStoreSite, not fieldTypeRecordFor: a site compiled with no check must not leave the field claimable.
    RefPtr record = table->recordForStoreSite(data.fieldTypeOwner->id(), data.offset, "dfg-owner-known");
    if (!record || !record->expected()) {
        if (Options::logFieldTypes()) [[unlikely]] {
            dataLogLn("[fieldtype] STORE-DECLINE no-record (owner-known) owner=", data.fieldTypeOwner->id().bits(), " offset=", data.offset);
        }
        return nullptr;
    }
    if (Options::logFieldTypes()) [[unlikely]]
        dataLogLn("[fieldtype] STORE-CHECK (owner-known) owner=", data.fieldTypeOwner->id().bits(), " offset=", data.offset, " expected=", fieldTypeExpectedFor(data.fieldTypeOwner.get(), data.offset).bits());
    return record;
}

Structure* Graph::commonFieldTypeOwner(const StructureSet& set, PropertyOffset offset)
{
    Structure* owner = nullptr;
    for (unsigned i = set.size(); i--;) {
        Structure* candidate = set[i]->findOffsetOwner(offset);
        if (!candidate)
            return nullptr;
        if (!owner)
            owner = candidate;
        else if (owner != candidate)
            return nullptr;
    }
    return owner;
}

StructureID Graph::provenSingleStructure(const AbstractValue& value)
{
    if (value.m_type & ~SpecCell)
        return StructureID();
    if (!value.m_structure.isFinite() || value.m_structure.size() != 1)
        return StructureID();
    Structure* structure = value.m_structure.at(0).get();
    return structure ? structure->id() : StructureID();
}

bool Graph::handleMaterializedField(Structure* structure, PropertyOffset offset, StructureID provenValueStructure)
{
    if (!Options::useFieldTypeAssumptions() || !structure || structure->isDictionary() || offset == invalidOffset)
        return false;
    auto* table = m_vm.fieldTypeWatchpoints();
    if (!table)
        return false;
    Structure* owner = structure->findOffsetOwner(offset);
    if (!owner) {
        // No single claim describes this field, so fall back to what this path did before.
        poisonFieldTypeAcrossAncestry(structure, offset);
        return false;
    }
    StructureID claimed = table->expectedFor(owner->id(), offset);
    if (!claimed) {
        poisonFieldTypeForUncheckedWrite(structure, offset);
        return false;
    }
    if (provenValueStructure && provenValueStructure == claimed) {
        if (Options::logFieldTypes()) [[unlikely]]
            dataLogLn("[fieldtype] MATERIALIZE-PROVEN owner=", owner->id().bits(), " offset=", offset, " claimed=", claimed.bits());
        return false;
    }
    if (Options::logFieldTypes()) [[unlikely]]
        dataLogLn("[fieldtype] MATERIALIZE-NEEDS-RECORD owner=", owner->id().bits(), " offset=", offset,
            " claimed=", claimed.bits(), " proven=", provenValueStructure.bits());
    return true;
}


Edge Graph::storedValueEdge(Node* node)
{
    switch (node->op()) {
    case PutByOffset:
        return node->child3();
    case MultiPutByOffset:
        return node->child2();
    default:
        return Edge();
    }
}

void Graph::poisonFieldTypeAcrossAncestry(Structure* base, PropertyOffset offset, StructureID provenValueStructure)
{
    if (!Options::useFieldTypeUncheckedWritePoisoning()) [[unlikely]]
        return;
    if (!Options::useFieldTypeAssumptions() || !base || offset == invalidOffset)
        return;
    auto* table = m_vm.fieldTypeWatchpoints();
    if (!table)
        return;
    for (Structure* candidate = base; candidate; candidate = candidate->previousID()) {
        // recordFor, not recordForStoreSite: creating an entry for every ancestor at this offset pollutes the
        // table with unrelated properties that merely share it, which the C++ replace hook then withdraws.
        RefPtr record = table->recordFor(candidate->id(), offset);
        if (record && record->expected()) {
            if (Options::useFieldTypeAncestryProof() && provenValueStructure && record->expected() == provenValueStructure) {
                // The store provably satisfies this claim, so it cannot violate it. Sound with no dependency:
                // claims only ever generalise, never re-point.
                if (Options::logFieldTypes()) [[unlikely]]
                    dataLogLn("[fieldtype] ANCESTRY-PROVEN owner=", candidate->id().bits(), " offset=", offset);
                continue;
            }
            if (Options::logFieldTypes()) [[unlikely]]
                dataLogLn("[fieldtype] POISON-UNPROVABLE-OWNER owner=", candidate->id().bits(), " offset=", offset);
            m_plan.addFieldTypeToGeneralize(*record);
        }
    }
}

void Graph::poisonAllFieldTypesAtOffset(PropertyOffset offset)
{
    if (!Options::useFieldTypeUncheckedWritePoisoning()) [[unlikely]]
        return;
    if (!Options::useFieldTypeAssumptions() || offset == invalidOffset)
        return;
    auto* table = m_vm.fieldTypeWatchpoints();
    if (!table)
        return;
    // The enumerated walk below reaches every MATERIALISED claim. A word-only claim (useFieldTypeLazyEntries)
    // has no table entry and is therefore invisible to it, so record the offset instead: from here no word-only
    // claim at this offset may be established or materialised, which is what keeps one from ever being narrowed
    // on behind this unchecked store. Word-only claims have no dependents, so nothing compiled needs discarding.
    table->notePoisonedOffset(offset);
    Vector<RefPtr<FieldTypeRecord>> victims = table->recordsAtOffset(offset);
    if (Options::logFieldTypes() && !victims.isEmpty()) [[unlikely]]
        dataLogLn("[fieldtype] POISON-ALL-AT-OFFSET offset=", offset, " count=", victims.size());
    for (auto& record : victims) {
        if (record)
            m_plan.addFieldTypeToGeneralize(*record);
    }
}

void Graph::poisonFieldTypeForUncheckedWrite(Structure* structure, PropertyOffset offset)
{
    if (!Options::useFieldTypeUncheckedWritePoisoning()) [[unlikely]]
        return;
    if (!Options::useFieldTypeAssumptions())
        return;
    if (!structure || offset == invalidOffset)
        return;
    auto* table = m_vm.fieldTypeWatchpoints();
    if (!table)
        return;
    Structure* owner = structure->findOffsetOwner(offset);
    if (!owner)
        return;

    // recordForStoreSite, so a field with no record yet is poisoned too: a later claim would be falsified here.
    RefPtr record = table->recordForStoreSite(owner->id(), offset, "dfg-unchecked-write");
    if (record && record->expected()) {
        if (Options::logFieldTypes()) [[unlikely]]
            dataLogLn("[fieldtype] POISON-UNCHECKED-WRITE owner=", owner->id().bits(), " offset=", offset);
        m_plan.addFieldTypeToGeneralize(*record);
    }
}

Structure* Graph::fieldTypeOwnerFor(Structure* site, PropertyOffset offset)
{
    if (!site || offset == invalidOffset)
        return nullptr;
    if (!Options::useFieldTypeCompilerOwnerMemo()) [[unlikely]]
        return site->findOffsetOwner(offset);
    uint64_t key = fieldTypeMemoKey(site->id(), offset);
    auto memo = m_fieldTypeOwnerMemo.find(key);
    if (memo != m_fieldTypeOwnerMemo.end()) [[likely]]
        return memo->value;
    Structure* owner = site->findOffsetOwner(offset);
    m_fieldTypeOwnerMemo.add(key, owner);
    return owner;
}

RefPtr<FieldTypeRecord> Graph::fieldTypeRecordFor(Structure* owner, PropertyOffset offset)
{
    if (!Options::useFieldTypeAssumptions())
        return nullptr;
    if (!owner || offset == invalidOffset)
        return nullptr;

    // Memoised, record and expected together: the mutator may generalize concurrently, so this must give
    // exactly one answer per compilation. See the header.
    uint64_t key = fieldTypeMemoKey(owner->id(), offset);
    auto memo = m_fieldTypeRecordMemo.find(key);
    if (memo != m_fieldTypeRecordMemo.end())
        return memo->value.expected ? memo->value.record : nullptr;

    FieldTypeDecision decision;
    if (auto* table = m_vm.fieldTypeWatchpoints()) {
        RefPtr record = table->recordFor(owner->id(), offset);
        if (record) {
            StructureID expected = record->expected();
            if (expected) {
                decision.record = WTF::move(record);
                decision.expected = expected;
            }
        }
    }
    m_fieldTypeRecordMemo.add(key, decision);
    return decision.expected ? decision.record : nullptr;
}

// The expected structure this compilation committed to for (owner, offset), or a null StructureID if it
// declined. Always use this rather than FieldTypeRecord::expected(), which can change under us.
StructureID Graph::fieldTypeExpectedFor(Structure* owner, PropertyOffset offset)
{
    if (!owner || offset == invalidOffset)
        return StructureID();
    if (!fieldTypeRecordFor(owner, offset))
        return StructureID();
    auto memo = m_fieldTypeRecordMemo.find(fieldTypeMemoKey(owner->id(), offset));
    ASSERT(memo != m_fieldTypeRecordMemo.end());
    return memo->value.expected;
}

RefPtr<FieldTypeRecord> Graph::fieldTypeRecordForStore(const StorageAccessData& data, const StructureAbstractValue& baseStructures, StructureID provenValueStructure)
{
    // A creating store already knows its owner; nothing to resolve.
    if (data.fieldTypeOwner)
        return fieldTypeRecordForStore(data);

    if (!Options::useFieldTypeAssumptions())
        return nullptr;
    // isFinite() excludes Top and clobbered sets, where not every base structure is known and no owner can be proven.
    if (data.offset == invalidOffset || !baseStructures.isFinite() || !baseStructures.size()) {
        if (Options::logFieldTypes()) [[unlikely]]
            dataLogLn("[fieldtype] STORE-DECLINE base-not-finite offset=", data.offset, " finite=", baseStructures.isFinite(), " size=", baseStructures.isFinite() ? baseStructures.size() : 0);
        // The base could be any object, so no claim at this offset can survive this store.
        poisonAllFieldTypesAtOffset(data.offset);
        return nullptr;
    }

    auto* table = m_vm.fieldTypeWatchpoints();
    if (!table)
        return nullptr;

    // Every possible base structure must agree on the owner, or one check cannot maintain all their records.
    Structure* owner = nullptr;
    for (size_t i = baseStructures.size(); i--;) {
        Structure* candidate = fieldTypeOwnerFor(baseStructures.at(i).get(), data.offset);
        if (!candidate) {
            // Unprovable owner (a dictionary, or offsets reused after deletion): subtract the claims.
            for (size_t j = baseStructures.size(); j--;)
                poisonFieldTypeAcrossAncestry(baseStructures.at(j).get(), data.offset, provenValueStructure);
            return nullptr;
        }
        if (!owner)
            owner = candidate;
        else if (owner != candidate) {
            for (size_t j = baseStructures.size(); j--;)
                poisonFieldTypeAcrossAncestry(baseStructures.at(j).get(), data.offset, provenValueStructure);
            return nullptr;
        }
    }
    if (!owner) {
        if (Options::logFieldTypes()) [[unlikely]]
            dataLogLn("[fieldtype] STORE-DECLINE no-owner offset=", data.offset);
        return nullptr;
    }

    // recordForStoreSite POISONS as a side effect of being asked: on a new entry it inserts the permanently-
    // generalised (null) record and returns nullptr, so this function then declines the very check it is capable
    // of emitting. Measured consequence: STORE-CHECK is **0** on raytrace, delta-blue, chai-wtb and babel-wtb
    // alike, against 565 / 173 / 79 / 1747 STORE-DECLINE(no-record) -- the store-side check never fires anywhere
    // in the suite, while the pre-emptive poisoning destroys claims the hot functions were narrowing on. Turning
    // the whole store-site machinery off recovers **raytrace +6.01 points (112%)** and *improves* delta-blue
    // (+9.47% -> +9.88%), so the poisoning is pure cost on both sides of the ledger.
    //
    // With this option false the lookup does not poison. UNSOUND until the "claim established later" hole is
    // closed: a store site compiled with no check must be invalidated if a claim forms afterwards, which the
    // emitted check cannot do by itself -- it loads addressOfExpected() and exits when it reads ZERO
    // (DFGSpeculativeJIT.cpp:14831), which covers WITHDRAWAL but not ESTABLISHMENT. The verifier
    // (validateFieldTypes=1) is the gate for that hole, and it is the reason this ships behind a flag.
    RefPtr record = Options::useFieldTypeDFGStoreSitePoisoning()
        ? table->recordForStoreSite(owner->id(), data.offset)
        : table->recordFor(owner->id(), data.offset);
    if (!record || !record->expected()) {
        if (Options::logFieldTypes()) [[unlikely]] {
            dataLogLn("[fieldtype] STORE-DECLINE no-record owner=", owner->id().bits(), " offset=", data.offset);
        }
        return nullptr;
    }
    if (Options::logFieldTypes()) [[unlikely]]
        dataLogLn("[fieldtype] STORE-CHECK owner=", owner->id().bits(), " offset=", data.offset, " expected=", fieldTypeExpectedFor(owner, data.offset).bits());
    return record;
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

    // Single structure, so DFGConstantFoldingPhase can resolve the owner and insert a CheckFieldType after the load.
    AbstractValue inferred = fieldTypeAssumptionValue(
        base.m_structure.onlyStructure(), offset, clobberState, FieldTypeNarrowingGuard::LoadedCheck);
    if (!inferred.isClear())
        return inferred;

    return AbstractValue::heapTop();
}

AbstractValue Graph::inferredValueForProperty(const AbstractValue& base, const RegisteredStructureSet& structureSet, PropertyOffset offset, StructureClobberState clobberState)
{
    if (JSValue value = tryGetConstantProperty(base.m_value, structureSet, offset)) {
        AbstractValue result;
        result.set(*this, *freeze(value), clobberState);
        return result;
    }

    // MultiGetByOffset merges over a structure set. DFGConstantFoldingPhase deliberately inserts no CheckFieldType
    // here, so this narrowing has no runtime guard and must keep the watchpoint dependency.
    AbstractValue inferred = fieldTypeAssumptionValue(
        structureSet.onlyStructure(), offset, clobberState, FieldTypeNarrowingGuard::WatchpointOnly);
    if (!inferred.isClear())
        return inferred;

    return AbstractValue::heapTop();
}

JSValue Graph::tryGetConstantClosureVar(JSValue base, ScopeOffset offset)
{
    // This has an awesome concurrency story. See comment for GetGlobalVar in ByteCodeParser.

    if (m_plan.isUnlinked())
        return JSValue();
    
    if (!base)
        return JSValue();
    
    JSLexicalEnvironment* activation = dynamicDowncast<JSLexicalEnvironment>(base);
    if (!activation)
        return JSValue();
    
    SymbolTable* symbolTable = activation->symbolTable();
    JSValue value;
    InlineWatchpointSet* set;
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
    
    watchpoints().addLazily(*set);
    
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
    if (m_plan.isUnlinked())
        return nullptr;
    if (!value)
        return nullptr;
    JSArrayBufferView* view = dynamicDowncast<JSArrayBufferView>(value);
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

JSValue Graph::tryGetConstantGetter(Node* getterSetter)
{
    auto* cell = getterSetter->dynamicCastConstant<GetterSetter*>();
    if (!cell)
        return JSValue();
    return cell->getterConcurrently();
}

JSValue Graph::tryGetConstantSetter(Node* getterSetter)
{
    auto* cell = getterSetter->dynamicCastConstant<GetterSetter*>();
    if (!cell)
        return JSValue();
    return cell->setterConcurrently();
}

ObjectPropertyConditionSet Graph::tryEnsureAbsence(JSGlobalObject* globalObject, const StructureSet& structureSet, CacheableIdentifier identifier)
{
    if (structureSet.isEmpty())
        return ObjectPropertyConditionSet::invalid();

    Structure* headStructure = structureSet.onlyStructure();
    if (!headStructure)
        return ObjectPropertyConditionSet::invalid();

    auto isAbsenceCacheable = [&](Structure* structure) {
        // Absences cannot be cached on any dictionaries, including CachedDictionaryKind, because
        // new properties can be added without structure transitions.
        if (structure->isDictionary())
            return false;
        if (structure->typeInfo().overridesGetOwnPropertySlot())
            return false;
        if (!structure->propertyAccessesAreCacheable())
            return false;
        if (!structure->propertyAccessesAreCacheableForAbsence())
            return false;
        if (structure->isDictionary())
            return false;
        unsigned attributes;
        if (isValidOffset(structure->getConcurrently(identifier.uid(), attributes)))
            return false;
        if (structure->hasPolyProto())
            return false;
        return true;
    };

    // generateConditionsForPropertyMissConcurrently only walks the prototype chain, so validate
    // headStructure first.
    if (!isAbsenceCacheable(headStructure))
        return ObjectPropertyConditionSet::invalid();

    auto result = generateConditionsForPropertyMissConcurrently(globalObject->vm(), globalObject, headStructure, identifier.uid());
    if (!result.isValid())
        return result;

    for (auto& condition : result) {
        auto* object = condition.object();
        if (!object)
            return ObjectPropertyConditionSet::invalid();

        if (!isAbsenceCacheable(object->structure()))
            return ObjectPropertyConditionSet::invalid();
    }
    return result;
}

static RegExp* constantRegExpFor(Graph& graph, Node* node, JSGlobalObject*& globalObject)
{
    if (RegExpObject* regExpObject = node->dynamicCastConstant<RegExpObject*>()) {
        globalObject = regExpObject->realm();
        return regExpObject->regExp();
    }
    if (node->op() == NewRegExp) {
        globalObject = graph.globalObjectFor(node->origin.semantic);
        return node->castOperand<RegExp*>();
    }
    return nullptr;
}

const WTF::BitSet<256>* Graph::tryGetConstantRegExpFirstCharacterBitmap(Node* node, FirstCharacterFilterPosition position)
{
    JSGlobalObject* globalObject = nullptr;
    RegExp* regExp = constantRegExpFor(*this, node, globalObject);
    if (!regExp)
        return nullptr;

    // The filter reads one fixed position, so the flags must guarantee a match can only begin there.
    switch (position) {
    case FirstCharacterFilterPosition::AtStart:
        if (regExp->globalOrSticky())
            return nullptr;
        break;
    case FirstCharacterFilterPosition::AtLastIndex:
        if (!regExp->sticky())
            return nullptr;
        break;
    }

    if (globalObject->isRegExpRecompiled())
        return nullptr;

    const WTF::BitSet<256>* bitmap = regExpFirstCharacterBitmap(regExp, position);
    if (!bitmap)
        return nullptr;

    // Only now that a filter will really be emitted is it worth constraining this compilation to the
    // realm's RegExp-recompiled watchpoint. Registering it earlier would let any .compile() in the
    // realm jettison code that baked no bitmap at all.
    watchpoints().addLazily(globalObject->regExpRecompiledWatchpointSet());
    return bitmap;
}

// nullopt: not a constant or not compiled yet, read RegExp::m_minimumSize at runtime. 0: constant, but no input length can be rejected.
std::optional<unsigned> Graph::tryGetConstantRegExpTestMinimumSize(Node* node)
{
    JSGlobalObject* globalObject = nullptr;
    RegExp* regExp = constantRegExpFor(*this, node, globalObject);
    if (!regExp || globalObject->isRegExpRecompiled())
        return std::nullopt;

    unsigned minimumSize = 0;
    {
        Locker locker { regExp->cellLock() };
        if (!regExp->hasCode())
            return std::nullopt;
        minimumSize = regExp->minimumSize();
    }

    if (regExp->globalOrSticky() || !minimumSize)
        return 0;

    watchpoints().addLazily(globalObject->regExpRecompiledWatchpointSet());
    return minimumSize;
}

const WTF::BitSet<256>* Graph::regExpFirstCharacterBitmap(RegExp* regExp, FirstCharacterFilterPosition position)
{
    const WTF::BitSet<256>* bitmap = regExp->firstCharacterBitmap(position);
    if (!bitmap)
        return nullptr;

    m_plan.weakReferences().addLazily(regExp);
    return bitmap;
}

void Graph::registerFrozenValues()
{
    ConcurrentJSLocker locker(m_codeBlock->m_lock);
    m_codeBlock->constants().shrink(0);
    for (FrozenValue& value : m_frozenValues) {
        if (!value.pointsToHeap())
            continue;
        
        ASSERT(value.structure());
        ASSERT(m_plan.weakReferences().contains(value.structure()));

        switch (value.strength()) {
        case WeakValue: {
            m_plan.weakReferences().addLazily(value.value().asCell());
            break;
        }
        case StrongValue: {
            unsigned constantIndex = m_codeBlock->addConstantLazily(locker);
            // We already have a barrier on the code block.
            m_codeBlock->constants()[constantIndex].setWithoutWriteBarrier(value.value());
            break;
        } }
    }
    m_codeBlock->constants().shrinkToFit();
}

template<typename Visitor>
ALWAYS_INLINE void Graph::visitChildrenImpl(Visitor& visitor)
{
    for (FrozenValue& value : m_frozenValues) {
        visitor.appendUnbarriered(value.value());
        visitor.appendUnbarriered(value.structure());
    }
}

void Graph::visitChildren(AbstractSlotVisitor& visitor) { visitChildrenImpl(visitor); }
void Graph::visitChildren(SlotVisitor& visitor) { visitChildrenImpl(visitor); }

FrozenValue* Graph::freeze(JSValue value)
{
    RELEASE_ASSERT(!m_plan.isInSafepoint());
    if (!value) [[unlikely]]
        return FrozenValue::emptySingleton();

    // There are weird relationships in how optimized CodeBlocks
    // point to other CodeBlocks. We don't want to have them be
    // part of the weak pointer set. For example, an optimized CodeBlock
    // having a weak pointer to itself will cause it to get collected.
    RELEASE_ASSERT(!is<CodeBlock>(value));
    
    auto result = m_frozenValueMap.add(JSValue::encode(value), nullptr);
    if (!result.isNewEntry) [[likely]]
        return result.iterator->value;

    if (value.isUInt32())
        m_uint32ValuesInUse.append(value.asUInt32());
    
    FrozenValue frozenValue = FrozenValue::freeze(value);
    if (Structure* structure = frozenValue.structure())
        registerStructure(structure);
    
    return result.iterator->value = &m_frozenValues.alloc(frozenValue);
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

RegisteredStructure Graph::registerStructure(Structure* structure)
{
    if (!isWatched(structure)) {
        m_plan.weakReferences().addLazily(structure);
        m_plan.watchpoints().addRegisteredNotWatched(structure);
    }
    return RegisteredStructure::createPrivate(structure);
}

bool Graph::tryWatch(Structure* structure)
{
    if (!structure->dfgMayWatch())
        return false;
    watch(structure);
    return true;
}

void Graph::watch(Structure* structure)
{
    if (Options::verboseDFGFailure() && !structure->dfgMayWatch()) [[unlikely]] {
        dataLogLn("DFG: Graph::watch on a non-watchable structure ", RawPointer(structure),
            "; could be caused by race in which mutator fired watchpoint after deciding to watch"
            " or might indicate that we're incorrectly deciding to watch.");
    }
    m_plan.weakReferences().addLazily(structure);
    m_plan.watchpoints().takeRegisteredNotWatched(structure);
    m_plan.watchpoints().addLazily(structure->transitionWatchpointSet());
}

bool Graph::isWatched(Structure* structure)
{
    return m_plan.watchpoints().isWatched(structure->transitionWatchpointSet());
}

void Graph::assertIsRegistered(Structure* structure)
{
    // It's convenient to be able to call this with a maybe-null structure.
    if (!structure)
        return;

    DFG_ASSERT(*this, nullptr, m_plan.weakReferences().contains(structure));

    if (!structure->dfgMayWatch())
        return;
    if (isWatched(structure))
        return;
    if (m_plan.watchpoints().isRegisteredNotWatched(structure))
        return;

    DFG_CRASH(*this, nullptr, toUTF8CString("Structure ", pointerDump(structure), " is watchable but isn't being watched.").legacyCStringPointer());
}

static void logDFGAssertionFailure(
    Graph& graph, const UTF8CString& whileText, const char* file, int line, const char* function,
    const char* assertion)
{
    startCrashing();
    graph.dumpAndReleaseIonGraph();
    WTF::dataFile().atomically([&](auto&) {
        dataLogLn("DFG ASSERTION FAILED: ", assertion);
        dataLogLn(file, "(", line, ") : ", function);
        dataLogLn();
        dataLog(whileText);
        dataLogLn("Graph at time of failure:");
        dataLog(graph);
        dataLogLn();
        dataLogLn("DFG ASSERTION FAILED: ", assertion);
        dataLogLn(file, "(", line, ") : ", function);
    });
}

void Graph::logAssertionFailure(
    std::nullptr_t, const char* file, int line, const char* function, const char* assertion)
{
    logDFGAssertionFailure(*this, ""_s, file, line, function, assertion);
}

void Graph::logAssertionFailure(
    Node* node, const char* file, int line, const char* function, const char* assertion)
{
    logDFGAssertionFailure(*this, toUTF8CString("While handling node ", node, "\n\n"), file, line, function, assertion);
}

void Graph::logAssertionFailure(
    BasicBlock* block, const char* file, int line, const char* function, const char* assertion)
{
    logDFGAssertionFailure(*this, toUTF8CString("While handling block ", pointerDump(block), "\n\n"), file, line, function, assertion);
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
        m_cpsDominators = makeUniqueWithoutFastMallocCheck<CPSDominators>(*this);
    return *m_cpsDominators;
}

SSADominators& Graph::ensureSSADominators()
{
    RELEASE_ASSERT(m_form == SSA || m_isInSSAConversion);
    if (!m_ssaDominators)
        m_ssaDominators = makeUniqueWithoutFastMallocCheck<SSADominators>(*this);
    return *m_ssaDominators;
}

CPSNaturalLoops& Graph::ensureCPSNaturalLoops()
{
    RELEASE_ASSERT(m_form != SSA && !m_isInSSAConversion);
    ensureCPSDominators();
    if (!m_cpsNaturalLoops)
        m_cpsNaturalLoops = makeUniqueWithoutFastMallocCheck<CPSNaturalLoops>(*this);
    return *m_cpsNaturalLoops;
}

SSANaturalLoops& Graph::ensureSSANaturalLoops()
{
    RELEASE_ASSERT(m_form == SSA);
    ensureSSADominators();
    if (!m_ssaNaturalLoops)
        m_ssaNaturalLoops = makeUniqueWithoutFastMallocCheck<SSANaturalLoops>(*this);
    return *m_ssaNaturalLoops;
}

BackwardsCFG& Graph::ensureBackwardsCFG()
{
    // We could easily relax this in the future to work over CPS, but today, it's only used in SSA.
    RELEASE_ASSERT(m_form == SSA); 
    if (!m_backwardsCFG)
        m_backwardsCFG = makeUniqueWithoutFastMallocCheck<BackwardsCFG>(*this);
    return *m_backwardsCFG;
}

BackwardsDominators& Graph::ensureBackwardsDominators()
{
    RELEASE_ASSERT(m_form == SSA);
    if (!m_backwardsDominators)
        m_backwardsDominators = makeUniqueWithoutFastMallocCheck<BackwardsDominators>(*this);
    return *m_backwardsDominators;
}

ControlEquivalenceAnalysis& Graph::ensureControlEquivalenceAnalysis()
{
    RELEASE_ASSERT(m_form == SSA);
    if (!m_controlEquivalenceAnalysis)
        m_controlEquivalenceAnalysis = makeUniqueWithoutFastMallocCheck<ControlEquivalenceAnalysis>(*this);
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
                if (argumentNode && node->variableAccessData() == argumentNode->variableAccessData())
                    return MethodOfGettingAValueProfile::argumentValueProfile(node->origin.semantic, node->operand());
            }
        }

        // currentNode is null when we're doing speculation checks for checkArgumentTypes().
        if (!currentNode || node->origin.semantic != currentNode->origin.semantic || !currentNode->hasResult()) {
            CodeBlock* profiledBlock = baselineCodeBlockFor(node->origin.semantic);

            if (node->accessesStack(*this)) {
                if (node->op() == GetLocal)
                    return MethodOfGettingAValueProfile::lazyOperandValueProfile(node->origin.semantic, node->operand());
            }

            if (node->hasHeapPrediction()) {
                auto instruction = profiledBlock->instructions().at(node->origin.semantic.bytecodeIndex());
                OpcodeID opcodeID = instruction->opcodeID();
                switch (opcodeID) {
                case op_tail_call:
                case op_tail_call_varargs: {
                    InlineCallFrame* inlineCallFrame = node->origin.semantic.inlineCallFrame();
                    if (!inlineCallFrame)
                        return { }; // TailCall in the outermost function.

                    CodeOrigin* codeOrigin = inlineCallFrame->getCallerSkippingTailCalls();
                    if (!codeOrigin)
                        return { };

                    CodeBlock* callerBlock = baselineCodeBlockFor(*codeOrigin);
                    auto* valueProfile = callerBlock->tryGetValueProfileForBytecodeIndex(codeOrigin->bytecodeIndex());
                    if (!valueProfile)
                        return { };

                    return MethodOfGettingAValueProfile::bytecodeValueProfile(*codeOrigin);
                }
                case op_call_ignore_result:
                    return { };
                default: {
                    auto* valueProfile = profiledBlock->tryGetValueProfileForBytecodeIndex(node->origin.semantic.bytecodeIndex());
                    if (!valueProfile)
                        return { };

                    return MethodOfGettingAValueProfile::bytecodeValueProfile(node->origin.semantic);
                }
                }
            }

            if (profiledBlock->hasBaselineJITProfiling()) {
                if (profiledBlock->binaryArithProfileForBytecodeIndex(node->origin.semantic.bytecodeIndex()))
                    return MethodOfGettingAValueProfile::binaryArithProfile(node->origin.semantic);
                if (profiledBlock->unaryArithProfileForBytecodeIndex(node->origin.semantic.bytecodeIndex()))
                    return MethodOfGettingAValueProfile::unaryArithProfile(node->origin.semantic);
            }
        }

        switch (node->op()) {
        case BooleanToNumber:
        case Identity:
        case ValueRep:
        case DoubleRep:
        case Int52Rep:
        case PurifyNaN:
            node = node->child1().node();
            break;
        default:
            node = nullptr;
        }
    }
    
    return { };
}

bool Graph::getPrototypeProperty(JSObject* prototype, Structure* prototypeStructure, UniquedStringImpl* uid, JSValue& returnJSValue)
{
    if (m_plan.isUnlinked())
        return false;

    PropertyOffset offset = prototypeStructure->getConcurrently(uid);
    if (!isValidOffset(offset))
        return false;

    JSValue value = tryGetConstantProperty(prototype, prototypeStructure, offset);
    if (!value)
        return false;

    // We only care about functions and getters at this point. If you want to access other properties
    // you'll have to add code for those types.
    JSFunction* function = dynamicDowncast<JSFunction>(value);
    if (!function) {
        GetterSetter* getterSetter = dynamicDowncast<GetterSetter>(value);

        if (!getterSetter)
            return false;

        returnJSValue = JSValue(getterSetter);
        return true;
    }

    returnJSValue = value;
    return true;
}

bool Graph::canOptimizeStringObjectAccess(const CodeOrigin& codeOrigin)
{
    if (m_plan.isUnlinked())
        return false;

    if (hasExitSite(codeOrigin, BadCache) || hasExitSite(codeOrigin, BadConstantCache))
        return false;

    if (!isWatchingStringSymbolToPrimitiveWatchpoint(codeOrigin))
        return false;

    // We're being conservative here. We want DFG's ToString on StringObject to be
    // used in both numeric contexts (that would call valueOf()) and string contexts
    // (that would call toString()). We don't want the DFG to have to distinguish
    // between the two, just because that seems like it would get confusing. So we
    // just require both methods to be sane.
    if (!isWatchingStringValueOfWatchpoint(codeOrigin))
        return false;

    if (!isWatchingStringToStringWatchpoint(codeOrigin))
        return false;

    return true;
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

    if (m_plan.isUnlinked())
        return false;

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

    JSGlobalObject* globalObject = globalObjectFor(node->child1()->origin.semantic);
    ArrayPrototype* arrayPrototype = globalObject->arrayPrototype();
    bool allGood = true;
    value.m_structure.forEach([&] (RegisteredStructure structure) {
        allGood &= structure->realm() == globalObject 
            && structure->hasMonoProto()
            && structure->storedPrototype() == arrayPrototype
            && !structure->isDictionary()
            && structure->getConcurrently(m_vm.propertyNames->iteratorSymbol.impl()) == invalidOffset
            && !structure->mayInterceptIndexedAccesses();
    });

    return allGood;
}

bool Graph::canDoFastSpreadWithStructureCheck(Node* node)
{
    ASSERT(node->op() == Spread);

    if (m_plan.isUnlinked())
        return false;

    return node->child1().useKind() == ArrayUse;
}

bool Graph::isNeverResizableOrGrowableSharedTypedArrayIncludingDataView(const AbstractValue& value)
{
    auto& structureSet = value.m_structure;
    if (!structureSet.isFinite())
        return false;

    if (structureSet.isClear())
        return false;

    bool allAreNonResizable = true;
    structureSet.forEach(
        [&](RegisteredStructure structure) {
            if (isResizableOrGrowableSharedTypedArrayIncludingDataView(structure->classInfoForCells()))
                allAreNonResizable = false;
        });
    return allAreNonResizable;
}

void Graph::clearCPSCFGData()
{
    m_cpsNaturalLoops = nullptr;
    m_cpsDominators = nullptr;
    m_cpsCFG = nullptr;
}

void Graph::freeDFGIRAfterLowering()
{
    m_blocks.clear();
    m_roots.clear();
    m_varArgChildren.clear();
    m_nodes.clearAll();

    m_bytecodeLiveness.clear();
    m_safeToLoad.clear();
    m_cpsDominators = nullptr;
    m_ssaDominators = nullptr;
    m_cpsNaturalLoops = nullptr;
    m_ssaNaturalLoops = nullptr;
    m_ssaCFG = nullptr;
    m_cpsCFG = nullptr;
    m_backwardsCFG = nullptr;
    m_backwardsDominators = nullptr;
    m_controlEquivalenceAnalysis = nullptr;
}

const BoyerMooreHorspoolTable<uint8_t>* Graph::tryAddStringSearchTable8(const String& string)
{
    constexpr unsigned minPatternLength = 9;
    if (string.length() > BoyerMooreHorspoolTable<uint8_t>::maxPatternLength)
        return nullptr;
    if (string.length() < minPatternLength)
        return nullptr;
    return m_stringSearchTable8.ensure(string, [&]() {
        return makeUnique<BoyerMooreHorspoolTable<uint8_t>>(string);
    }).iterator->value.get();
}

const ConcatKeyAtomStringCache* Graph::tryAddConcatKeyAtomStringCache(const String& s0, const String& s1, ConcatKeyAtomStringCache::Mode mode)
{
    if ((s0.length() + s1.length()) > ConcatKeyAtomStringCache::maxStringLengthForCache)
        return nullptr;
    m_concatKeyAtomStringCaches.append(makeUnique<ConcatKeyAtomStringCache>(m_codeBlock, mode));
    return m_concatKeyAtomStringCaches.last().get();
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

void Graph::dumpAndReleaseIonGraph()
{
    if (m_ionGraphFunction) [[unlikely]] {
        ASCIILiteral tier = m_plan.isFTL() ? "FTL"_s : "DFG"_s;
        bool osr = m_plan.mode() == JITCompilationMode::FTLForOSREntry;
        ProfilerSupport::dumpIonGraphFunction(m_codeBlock->inferredNameWithHash(), tier, osr, m_ionGraphFunction.releaseNonNull());
    }
}

void Graph::appendIonGraphPass(const String& passName)
{
    if (m_form == LoadStore) // This is even not setting up predecessors. It is not meaningful to have a graph at this point yet.
        return;

    auto pass = JSON::Object::create();
    pass->setString("name"_s, makeString("DFG: "_s, passName));
    {
        auto ionGraph = JSON::Object::create();
        auto ionBlocks = JSON::Array::create();
        ionGraph->setArray("blocks"_s, ionBlocks);

        DumpContext context;
        context.graph = this;

        for (auto* block : blocksInNaturalOrder()) {
            if (!block)
                continue;

            auto ionBlock = JSON::Object::create();
            auto attributes = JSON::Array::create();
            auto predecessors = JSON::Array::create();
            auto successors = JSON::Array::create();
            auto instructions = JSON::Array::create();

            if (block->isOSRTarget)
                attributes->pushString("osr"_s);

            if (block->isCatchEntrypoint)
                attributes->pushString("catch"_s);

            for (size_t i = 0; i < block->size(); ++i) {
                auto instruction = JSON::Object::create();
                auto inputs = JSON::Array::create();
                auto* node = block->at(i);

                DFG_NODE_DO_TO_CHILDREN(*this, node, [&](Node*, Edge edge) {
                    inputs->pushInteger(edge->index());
                });

                StringPrintStream stream;
                dump(stream, nullptr, node, &context, /* inIonGraph */ true);
                if (node->numSuccessors()) {
                    CommaPrinter comma(", "_s, " -> "_s);
                    for (unsigned i = 0; i < node->numSuccessors(); ++i) {
                        auto* block = node->successor(i);
                        if (!block)
                            continue;
                        stream.print(comma, "block "_s, block->index());
                    }
                }

                instruction->setInteger("ptr"_s, node->index() + 1);
                instruction->setInteger("id"_s, node->index());
                instruction->setString("opcode"_s, stream.toString());
                instruction->setArray("attributes"_s, JSON::Array::create());
                instruction->setArray("inputs"_s, WTF::move(inputs));
                instruction->setArray("uses"_s, JSON::Array::create());
                instruction->setArray("memInputs"_s, JSON::Array::create());
                instruction->setString("type"_s, ""_s);

                instructions->pushObject(WTF::move(instruction));
            }

            for (auto* predecessor : block->predecessors)
                predecessors->pushInteger(predecessor->index());

            for (auto* successor : block->successors())
                successors->pushInteger(successor->index());

            ionBlock->setInteger("ptr"_s, block->index() + 1);
            ionBlock->setInteger("id"_s, block->index());
            ionBlock->setInteger("loopDepth"_s, 0);
            ionBlock->setArray("attributes"_s, WTF::move(attributes));
            ionBlock->setArray("predecessors"_s, WTF::move(predecessors));
            ionBlock->setArray("successors"_s, WTF::move(successors));
            ionBlock->setArray("instructions"_s, WTF::move(instructions));
            ionBlocks->pushObject(ionBlock);
        }

        pass->setObject("mir"_s, WTF::move(ionGraph)); // MIR stands for SpiderMonkey's middle-level IR.
    }
    {
        auto ionGraph = JSON::Object::create();
        ionGraph->setArray("blocks"_s, JSON::Array::create());
        pass->setObject("lir"_s, WTF::move(ionGraph)); // LIR stands for SpiderMonkey's low-level IR.
    }
    m_ionGraphPasses->pushObject(pass);
}

UncheckedKeyHashMap<Node*, uint32_t> Graph::collectIRDumpDebugInfo(IRDumpDebugInfo& debugInfo)
{
    UncheckedKeyHashMap<Node*, uint32_t> nodeToLineIndex;
    for (BlockIndex blockIndex = 0; blockIndex < numBlocks(); ++blockIndex) {
        auto* block = this->block(blockIndex);
        if (!block)
            continue;
        debugInfo.irLines.append({ { }, blockIndex });
        for (size_t i = 0; i < block->size(); ++i) {
            Node* node = block->at(i);
            uint32_t lineIndex = debugInfo.irLines.size();
            nodeToLineIndex.add(node, lineIndex);
            debugInfo.irLines.append({ Graph::opName(node->op()), 0 });
        }
    }
    return nodeToLineIndex;
}

} } // namespace JSC::DFG

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(DFG_JIT)
