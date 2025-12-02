/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(DFG_JIT)

#include "DFGBlockInsertionSet.h"
#include <wtf/HashMap.h>

namespace JSC { namespace DFG {

class CloneHelper {
public:
    CloneHelper(Graph& graph)
        : m_graph(graph)
        , m_blockInsertionSet(graph)
    {
        ASSERT(graph.m_form == ThreadedCPS || graph.m_form == LoadStore);
    }

    static bool isNodeCloneable(Graph&, UncheckedKeyHashSet<Node*>&, Node*);

#if ASSERT_ENABLED
    static UncheckedKeyHashSet<Node*>& debugVisitingSet()
    {
        static LazyNeverDestroyed<ThreadSpecific<UncheckedKeyHashSet<Node*>, WTF::CanBeGCThread::True>> s_visitingSet;
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            s_visitingSet.construct();
        });
        return *s_visitingSet.get();
    }
#endif

    template<typename CustomizeSuccessors>
    BasicBlock* cloneBlock(BasicBlock* const, const CustomizeSuccessors&);
    BasicBlock* blockClone(BasicBlock*);

    void clear();
    void finalize();

private:
    Node* cloneNode(BasicBlock*, Node*);
    Node* cloneNodeImpl(BasicBlock*, Node*);

    Graph& m_graph;
    BlockInsertionSet m_blockInsertionSet;
    UncheckedKeyHashMap<Node*, Node*> m_nodeClones;
    UncheckedKeyHashMap<BasicBlock*, BasicBlock*> m_blockClones;
};

template<typename CustomizeSuccessors>
BasicBlock* CloneHelper::cloneBlock(BasicBlock* const block, const CustomizeSuccessors& customizeSuccessors)
{
    auto iter = m_blockClones.find(block);
    if (iter != m_blockClones.end())
        return iter->value;

    auto* clone = m_blockInsertionSet.insert(m_graph.numBlocks(), block->executionCount);
    clone->cfaHasVisited = false;
    clone->cfaDidFinish = false;
    m_blockClones.add(block, clone);

    // 1. Clone phis
    clone->phis.resize(block->phis.size());
    for (size_t i = 0; i < block->phis.size(); ++i) {
        Node* bodyPhi = block->phis[i];
        Node* phiClone = m_graph.addNode(bodyPhi->prediction(), bodyPhi->op(), bodyPhi->origin, OpInfo(bodyPhi->variableAccessData()));
        m_nodeClones.add(bodyPhi, phiClone);
        clone->phis[i] = phiClone;
    }

    // 2. Clone nodes.
    for (Node* node : *block)
        cloneNode(clone, node);

    // 3. Clone variables and tail and head.
    auto replaceOperands = [&](auto& nodes) ALWAYS_INLINE_LAMBDA {
        for (uint32_t i = 0; i < nodes.size(); ++i) {
            if (auto& node = nodes.at(i)) {
                // This is used to update variablesAtHead (CPS BasicBlock Phis) and variablesAtTail
                // (CPS BasicBlock Phis + locals). Thus, we must have a clone for each of them.
                node = m_nodeClones.get(node);
                ASSERT(node);
            }
        }
    };
    replaceOperands(clone->variablesAtTail = block->variablesAtTail);
    replaceOperands(clone->variablesAtHead = block->variablesAtHead);

    // 4. Clone successors. (predecessors will be fixed in the resetReachability of finalize)
    if (!customizeSuccessors(block, clone)) {
        ASSERT(clone->numSuccessors() == block->numSuccessors());
        for (uint32_t i = 0; i < clone->numSuccessors(); ++i) {
            auto& successor = clone->successor(i);
            ASSERT(successor == block->successor(i));
            successor = cloneBlock(successor, customizeSuccessors);
        }
    }

#if ASSERT_ENABLED
    clone->cloneSource = block;
#endif
    clone->isExcludedFromFTLCodeSizeEstimation = true;
    return clone;
}

#define FOR_EACH_NODE_CLONE_STATUS(CLONE_STATUS) \
    CLONE_STATUS(AllocatePropertyStorage, Common) \
    CLONE_STATUS(ArithAbs, Common) \
    CLONE_STATUS(ArithAdd, Common) \
    CLONE_STATUS(ArithBitAnd, Common) \
    CLONE_STATUS(ArithBitLShift, Common) \
    CLONE_STATUS(ArithBitNot, Common) \
    CLONE_STATUS(ArithBitOr, Common) \
    CLONE_STATUS(ArithBitRShift, Common) \
    CLONE_STATUS(ArithBitXor, Common) \
    CLONE_STATUS(ArithBitURShift, Common) \
    CLONE_STATUS(ArithCeil, Common) \
    CLONE_STATUS(ArithClz32, Common) \
    CLONE_STATUS(ArithDiv, Common) \
    CLONE_STATUS(ArithF16Round, Common) \
    CLONE_STATUS(ArithFloor, Common) \
    CLONE_STATUS(ArithFRound, Common) \
    CLONE_STATUS(ArithIMul, Common) \
    CLONE_STATUS(ArithMax, Common) \
    CLONE_STATUS(ArithMin, Common) \
    CLONE_STATUS(ArithMod, Common) \
    CLONE_STATUS(ArithMul, Common) \
    CLONE_STATUS(ArithNegate, Common) \
    CLONE_STATUS(ArithPow, Common) \
    CLONE_STATUS(ArithRandom, Common) \
    CLONE_STATUS(ArithRound, Common) \
    CLONE_STATUS(ArithSqrt, Common) \
    CLONE_STATUS(ArithSub, Common) \
    CLONE_STATUS(ArithTrunc, Common) \
    CLONE_STATUS(ArithUnary, Common) \
    CLONE_STATUS(ArrayIncludes, Common) \
    CLONE_STATUS(ArrayIndexOf, Common) \
    CLONE_STATUS(ArrayPop, Common) \
    CLONE_STATUS(ArrayPush, Common) \
    CLONE_STATUS(ArraySlice, Common) \
    CLONE_STATUS(ArraySplice, Common) \
    CLONE_STATUS(Arrayify, Common) \
    CLONE_STATUS(ArrayifyToStructure, Common) \
    CLONE_STATUS(AssertNotEmpty, Common) \
    CLONE_STATUS(BooleanToNumber, Common) \
    CLONE_STATUS(BottomValue, Common) \
    CLONE_STATUS(Branch, Special) \
    CLONE_STATUS(Call, Common) \
    CLONE_STATUS(CallCustomAccessorGetter, Common) \
    CLONE_STATUS(CallDirectEval, Common) \
    CLONE_STATUS(CallVarargs, Special) \
    CLONE_STATUS(Check, Common) \
    CLONE_STATUS(CheckArray, Common) \
    CLONE_STATUS(CheckBadValue, Common) \
    CLONE_STATUS(CheckIdent, Common) \
    CLONE_STATUS(CheckIsConstant, Common) \
    CLONE_STATUS(CheckNotEmpty, Common) \
    CLONE_STATUS(CheckStructure, Common) \
    CLONE_STATUS(CheckStructureOrEmpty, Common) \
    CLONE_STATUS(CheckVarargs, Common) \
    CLONE_STATUS(CompareBelow, Common) \
    CLONE_STATUS(CompareBelowEq, Common) \
    CLONE_STATUS(CompareEq, Common) \
    CLONE_STATUS(CompareEqPtr, Common) \
    CLONE_STATUS(CompareGreater, Common) \
    CLONE_STATUS(CompareGreaterEq, Common) \
    CLONE_STATUS(CompareLess, Common) \
    CLONE_STATUS(CompareLessEq, Common) \
    CLONE_STATUS(CompareStrictEq, Common) \
    CLONE_STATUS(ConstantStoragePointer, Common) \
    CLONE_STATUS(Construct, Common) \
    CLONE_STATUS(ConstructVarargs, Special) \
    CLONE_STATUS(CreateActivation, Common) \
    CLONE_STATUS(CreateDirectArguments, Common) \
    CLONE_STATUS(CreateRest, Common) \
    CLONE_STATUS(DataViewGetByteLength, Common) \
    CLONE_STATUS(DataViewGetByteLengthAsInt52, Common) \
    CLONE_STATUS(DataViewGetFloat, Common) \
    CLONE_STATUS(DataViewGetInt, Common) \
    CLONE_STATUS(DataViewSet, Common) \
    CLONE_STATUS(DateGetTime, Common) \
    CLONE_STATUS(DateSetTime, Common) \
    CLONE_STATUS(Dec, Common) \
    CLONE_STATUS(DeleteById, Common) \
    CLONE_STATUS(DirectCall, Common) \
    CLONE_STATUS(DirectConstruct, Common) \
    CLONE_STATUS(DirectTailCallInlinedCaller, Common) \
    CLONE_STATUS(DoubleAsInt32, Common) \
    CLONE_STATUS(DoubleConstant, Common) \
    CLONE_STATUS(DoubleRep, Common) \
    CLONE_STATUS(EnumeratorGetByVal, Common) \
    CLONE_STATUS(EnumeratorHasOwnProperty, Common) \
    CLONE_STATUS(EnumeratorNextUpdateIndexAndMode, Common) \
    CLONE_STATUS(EnumeratorNextUpdatePropertyName, Common) \
    CLONE_STATUS(EnumeratorPutByVal, Common) \
    CLONE_STATUS(ExitOK, Common) \
    CLONE_STATUS(ExtractFromTuple, Common) \
    CLONE_STATUS(FilterCallLinkStatus, Common) \
    CLONE_STATUS(FilterGetByStatus, Common) \
    CLONE_STATUS(FilterInByStatus, Common) \
    CLONE_STATUS(FilterPutByStatus, Common) \
    CLONE_STATUS(Flush, Common) \
    CLONE_STATUS(GetArrayLength, Common) \
    CLONE_STATUS(GetButterfly, Common) \
    CLONE_STATUS(GetById, Common) \
    CLONE_STATUS(GetByIdFlush, Common) \
    CLONE_STATUS(GetByIdMegamorphic, Common) \
    CLONE_STATUS(GetByIdWithThis, Common) \
    CLONE_STATUS(GetByIdWithThisMegamorphic, Common) \
    CLONE_STATUS(GetByOffset, Common) \
    CLONE_STATUS(GetByVal, Common) \
    CLONE_STATUS(GetByValMegamorphic, Common) \
    CLONE_STATUS(GetByValWithThis, Common) \
    CLONE_STATUS(GetByValWithThisMegamorphic, Common) \
    CLONE_STATUS(GetClosureVar, Common) \
    CLONE_STATUS(GetExecutable, Common) \
    CLONE_STATUS(GetGlobalLexicalVariable, Common) \
    CLONE_STATUS(GetGlobalVar, Common) \
    CLONE_STATUS(GetIndexedPropertyStorage, Common) \
    CLONE_STATUS(GetInternalField, Common) \
    CLONE_STATUS(GetLocal, Common) \
    CLONE_STATUS(GetPropertyEnumerator, Common) \
    CLONE_STATUS(GetPrototypeOf, Common) \
    CLONE_STATUS(GetRegExpObjectLastIndex, Common) \
    CLONE_STATUS(GetRestLength, Common) \
    CLONE_STATUS(GetScope, Common) \
    CLONE_STATUS(GetEvalScope, Common) \
    CLONE_STATUS(GetUndetachedTypeArrayLength, Common) \
    CLONE_STATUS(GlobalIsNaN, Common) \
    CLONE_STATUS(HasOwnProperty, Common) \
    CLONE_STATUS(HasIndexedProperty, Common) \
    CLONE_STATUS(InById, Common) \
    CLONE_STATUS(InByVal, Common) \
    CLONE_STATUS(InByValMegamorphic, Common) \
    CLONE_STATUS(Inc, Common) \
    CLONE_STATUS(InstanceOf, Common) \
    CLONE_STATUS(InstanceOfCustom, Common) \
    CLONE_STATUS(InstanceOfMegamorphic, Common) \
    CLONE_STATUS(Int52Constant, Common) \
    CLONE_STATUS(Int52Rep, Common) \
    CLONE_STATUS(InvalidationPoint, Common) \
    CLONE_STATUS(IsCellWithType, Common) \
    CLONE_STATUS(IsEmptyStorage, Common) \
    CLONE_STATUS(IsNumber, Common) \
    CLONE_STATUS(IsObject, Common) \
    CLONE_STATUS(JSConstant, Common) \
    CLONE_STATUS(Jump, Common) \
    CLONE_STATUS(LoadMapValue, Common) \
    CLONE_STATUS(LoadVarargs, Special) \
    CLONE_STATUS(LogicalNot, Common) \
    CLONE_STATUS(LoopHint, Common) \
    CLONE_STATUS(MakeAtomString, Common) \
    CLONE_STATUS(MakeRope, Common) \
    CLONE_STATUS(MapGet, Common) \
    CLONE_STATUS(MapHash, Common) \
    CLONE_STATUS(MapIteratorKey, Common) \
    CLONE_STATUS(MapIteratorNext, Common) \
    CLONE_STATUS(MapIteratorValue, Common) \
    CLONE_STATUS(MapSet, Common) \
    CLONE_STATUS(MatchStructure, Common) \
    CLONE_STATUS(MovHint, Common) \
    CLONE_STATUS(MultiGetByOffset, Special) \
    CLONE_STATUS(MultiPutByOffset, Special) \
    CLONE_STATUS(NewArray, Common) \
    CLONE_STATUS(NewArrayBuffer, Common) \
    CLONE_STATUS(NewArrayWithButterfly, Common) \
    CLONE_STATUS(NewArrayWithSize, Common) \
    CLONE_STATUS(NewArrayWithSpread, Common) \
    CLONE_STATUS(NewFunction, Common) \
    CLONE_STATUS(NewInternalFieldObject, Common) \
    CLONE_STATUS(NewMap, Common) \
    CLONE_STATUS(NewObject, Common) \
    CLONE_STATUS(NewRegExp, Common) \
    CLONE_STATUS(NewRegExpUntyped, Common) \
    CLONE_STATUS(NewSet, Common) \
    CLONE_STATUS(NormalizeMapKey, Common) \
    CLONE_STATUS(NumberToStringWithValidRadixConstant, Common) \
    CLONE_STATUS(NukeStructureAndSetButterfly, Common) \
    CLONE_STATUS(ObjectAssign, Common) \
    CLONE_STATUS(ObjectKeys, Common) \
    CLONE_STATUS(ObjectToString, Common) \
    CLONE_STATUS(ParseInt, Common) \
    CLONE_STATUS(PhantomLocal, Common) \
    CLONE_STATUS(Phi, PreCloned) \
    CLONE_STATUS(PurifyNaN, Common) \
    CLONE_STATUS(PutById, Common) \
    CLONE_STATUS(PutByIdFlush, Common) \
    CLONE_STATUS(PutByIdMegamorphic, Common) \
    CLONE_STATUS(PutByIdWithThis, Common) \
    CLONE_STATUS(PutByOffset, Common) \
    CLONE_STATUS(PutByVal, Common) \
    CLONE_STATUS(PutByValDirectResolved, Common) \
    CLONE_STATUS(PutByValDirect, Common) \
    CLONE_STATUS(PutByValWithThis, Common) \
    CLONE_STATUS(PutClosureVar, Common) \
    CLONE_STATUS(PutGlobalVariable, Common) \
    CLONE_STATUS(PutInternalField, Common) \
    CLONE_STATUS(PutStructure, Common) \
    CLONE_STATUS(ReallocatePropertyStorage, Common) \
    CLONE_STATUS(RegExpTest, Common) \
    CLONE_STATUS(RegExpTestInline, Common) \
    CLONE_STATUS(ResolveRope, Common) \
    CLONE_STATUS(SameValue, Common) \
    CLONE_STATUS(SetAdd, Common) \
    CLONE_STATUS(SetArgumentCountIncludingThis, Common) \
    CLONE_STATUS(SetArgumentDefinitely, Common) \
    CLONE_STATUS(SetArgumentMaybe, Common) \
    CLONE_STATUS(SetCallee, Common) \
    CLONE_STATUS(SetLocal, Common) \
    CLONE_STATUS(SkipScope, Common) \
    CLONE_STATUS(Spread, Common) \
    CLONE_STATUS(StringCharAt, Common) \
    CLONE_STATUS(StringCharCodeAt, Common) \
    CLONE_STATUS(StringCodePointAt, Common) \
    CLONE_STATUS(StringFromCharCode, Common) \
    CLONE_STATUS(StringIndexOf, Common) \
    CLONE_STATUS(StringLocaleCompare, Common) \
    CLONE_STATUS(StringReplace, Common) \
    CLONE_STATUS(StringReplaceString, Common) \
    CLONE_STATUS(StringSlice, Common) \
    CLONE_STATUS(StringSubstring, Common) \
    CLONE_STATUS(StrCat, Common) \
    CLONE_STATUS(Switch, Special) \
    CLONE_STATUS(TailCallForwardVarargsInlinedCaller, Special) \
    CLONE_STATUS(TailCallInlinedCaller, Common) \
    CLONE_STATUS(TailCallVarargsInlinedCaller, Special) \
    CLONE_STATUS(ToLength, Common) \
    CLONE_STATUS(ToLowerCase, Common) \
    CLONE_STATUS(ToPrimitive, Common) \
    CLONE_STATUS(ToString, Common) \
    CLONE_STATUS(ToThis, Common) \
    CLONE_STATUS(TypeOf, Common) \
    CLONE_STATUS(TypeOfIsFunction, Common) \
    CLONE_STATUS(TypeOfIsObject, Common) \
    CLONE_STATUS(TypeOfIsUndefined, Common) \
    CLONE_STATUS(UInt32ToNumber, Common) \
    CLONE_STATUS(ValueAdd, Common) \
    CLONE_STATUS(ValueBitAnd, Common) \
    CLONE_STATUS(ValueBitLShift, Common) \
    CLONE_STATUS(ValueBitOr, Common) \
    CLONE_STATUS(ValueBitRShift, Common) \
    CLONE_STATUS(ValueBitXor, Common) \
    CLONE_STATUS(ValueBitURShift, Common) \
    CLONE_STATUS(ValueNegate, Common) \
    CLONE_STATUS(ValueRep, Common) \
    CLONE_STATUS(ValueSub, Common) \
    CLONE_STATUS(ValueToInt32, Common) \
    CLONE_STATUS(VarargsLength, Special) \
    CLONE_STATUS(ZombieHint, Common)

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
