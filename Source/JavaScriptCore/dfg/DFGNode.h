/*
 * Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGNode_h
#define DFGNode_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGAbstractValue.h"
#include "DFGAdjacencyList.h"
#include "DFGArithMode.h"
#include "DFGArrayMode.h"
#include "DFGCommon.h"
#include "DFGLazyJSValue.h"
#include "DFGNodeFlags.h"
#include "DFGNodeOrigin.h"
#include "DFGNodeType.h"
#include "DFGVariableAccessData.h"
#include "JSCJSValue.h"
#include "Operands.h"
#include "SpeculatedType.h"
#include "StructureSet.h"
#include "ValueProfile.h"
#include <wtf/ListDump.h>

namespace JSC { namespace DFG {

class Graph;
struct BasicBlock;

struct StructureTransitionData {
    Structure* previousStructure;
    Structure* newStructure;
    
    StructureTransitionData() { }
    
    StructureTransitionData(Structure* previousStructure, Structure* newStructure)
        : previousStructure(previousStructure)
        , newStructure(newStructure)
    {
    }
};

struct NewArrayBufferData {
    unsigned startConstant;
    unsigned numConstants;
    IndexingType indexingType;
};

// The SwitchData and associated data structures duplicate the information in
// JumpTable. The DFG may ultimately end up using the JumpTable, though it may
// instead decide to do something different - this is entirely up to the DFG.
// These data structures give the DFG a higher-level semantic description of
// what is going on, which will allow it to make the right decision.
//
// Note that there will never be multiple SwitchCases in SwitchData::cases that
// have the same SwitchCase::value, since the bytecode's JumpTables never have
// duplicates - since the JumpTable maps a value to a target. It's a
// one-to-many mapping. So we may have duplicate targets, but never duplicate
// values.
struct SwitchCase {
    SwitchCase()
        : target(0)
    {
    }
    
    SwitchCase(LazyJSValue value, BasicBlock* target)
        : value(value)
        , target(target)
    {
    }
    
    static SwitchCase withBytecodeIndex(LazyJSValue value, unsigned bytecodeIndex)
    {
        SwitchCase result;
        result.value = value;
        result.target = bitwise_cast<BasicBlock*>(static_cast<uintptr_t>(bytecodeIndex));
        return result;
    }
    
    unsigned targetBytecodeIndex() const { return bitwise_cast<uintptr_t>(target); }
    
    LazyJSValue value;
    BasicBlock* target;
};

enum SwitchKind {
    SwitchImm,
    SwitchChar,
    SwitchString
};

struct SwitchData {
    // Initializes most fields to obviously invalid values. Anyone
    // constructing this should make sure to initialize everything they
    // care about manually.
    SwitchData()
        : fallThrough(0)
        , kind(static_cast<SwitchKind>(-1))
        , switchTableIndex(UINT_MAX)
        , didUseJumpTable(false)
    {
    }
    
    void setFallThroughBytecodeIndex(unsigned bytecodeIndex)
    {
        fallThrough = bitwise_cast<BasicBlock*>(static_cast<uintptr_t>(bytecodeIndex));
    }
    unsigned fallThroughBytecodeIndex() const { return bitwise_cast<uintptr_t>(fallThrough); }
    
    Vector<SwitchCase> cases;
    BasicBlock* fallThrough;
    SwitchKind kind;
    unsigned switchTableIndex;
    bool didUseJumpTable;
};

// This type used in passing an immediate argument to Node constructor;
// distinguishes an immediate value (typically an index into a CodeBlock data structure - 
// a constant index, argument, or identifier) from a Node*.
struct OpInfo {
    explicit OpInfo(int32_t value) : m_value(static_cast<uintptr_t>(value)) { }
    explicit OpInfo(uint32_t value) : m_value(static_cast<uintptr_t>(value)) { }
#if OS(DARWIN) || USE(JSVALUE64)
    explicit OpInfo(size_t value) : m_value(static_cast<uintptr_t>(value)) { }
#endif
    explicit OpInfo(void* value) : m_value(reinterpret_cast<uintptr_t>(value)) { }
    uintptr_t m_value;
};

// === Node ===
//
// Node represents a single operation in the data flow graph.
struct Node {
    enum VarArgTag { VarArg };
    
    Node() { }
    
    Node(NodeType op, NodeOrigin nodeOrigin, const AdjacencyList& children)
        : origin(nodeOrigin)
        , children(children)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
    {
        misc.replacement = 0;
        setOpAndDefaultFlags(op);
    }
    
    // Construct a node with up to 3 children, no immediate value.
    Node(NodeType op, NodeOrigin nodeOrigin, Edge child1 = Edge(), Edge child2 = Edge(), Edge child3 = Edge())
        : origin(nodeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
        , m_opInfo(0)
        , m_opInfo2(0)
    {
        misc.replacement = 0;
        setOpAndDefaultFlags(op);
        ASSERT(!(m_flags & NodeHasVarArgs));
    }

    // Construct a node with up to 3 children and an immediate value.
    Node(NodeType op, NodeOrigin nodeOrigin, OpInfo imm, Edge child1 = Edge(), Edge child2 = Edge(), Edge child3 = Edge())
        : origin(nodeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
        , m_opInfo(imm.m_value)
        , m_opInfo2(0)
    {
        misc.replacement = 0;
        setOpAndDefaultFlags(op);
        ASSERT(!(m_flags & NodeHasVarArgs));
    }

    // Construct a node with up to 3 children and two immediate values.
    Node(NodeType op, NodeOrigin nodeOrigin, OpInfo imm1, OpInfo imm2, Edge child1 = Edge(), Edge child2 = Edge(), Edge child3 = Edge())
        : origin(nodeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
        , m_opInfo(imm1.m_value)
        , m_opInfo2(imm2.m_value)
    {
        misc.replacement = 0;
        setOpAndDefaultFlags(op);
        ASSERT(!(m_flags & NodeHasVarArgs));
    }
    
    // Construct a node with a variable number of children and two immediate values.
    Node(VarArgTag, NodeType op, NodeOrigin nodeOrigin, OpInfo imm1, OpInfo imm2, unsigned firstChild, unsigned numChildren)
        : origin(nodeOrigin)
        , children(AdjacencyList::Variable, firstChild, numChildren)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
        , m_opInfo(imm1.m_value)
        , m_opInfo2(imm2.m_value)
    {
        misc.replacement = 0;
        setOpAndDefaultFlags(op);
        ASSERT(m_flags & NodeHasVarArgs);
    }
    
    NodeType op() const { return static_cast<NodeType>(m_op); }
    NodeFlags flags() const { return m_flags; }
    
    // This is not a fast method.
    unsigned index() const;
    
    void setOp(NodeType op)
    {
        m_op = op;
    }
    
    void setFlags(NodeFlags flags)
    {
        m_flags = flags;
    }
    
    bool mergeFlags(NodeFlags flags)
    {
        ASSERT(!(flags & NodeDoesNotExit));
        NodeFlags newFlags = m_flags | flags;
        if (newFlags == m_flags)
            return false;
        m_flags = newFlags;
        return true;
    }
    
    bool filterFlags(NodeFlags flags)
    {
        ASSERT(flags & NodeDoesNotExit);
        NodeFlags newFlags = m_flags & flags;
        if (newFlags == m_flags)
            return false;
        m_flags = newFlags;
        return true;
    }
    
    bool clearFlags(NodeFlags flags)
    {
        return filterFlags(~flags);
    }
    
    void setOpAndDefaultFlags(NodeType op)
    {
        m_op = op;
        m_flags = defaultFlags(op);
    }

    void convertToPhantom()
    {
        setOpAndDefaultFlags(Phantom);
    }

    void convertToPhantomUnchecked()
    {
        setOpAndDefaultFlags(Phantom);
    }

    void convertToIdentity()
    {
        RELEASE_ASSERT(child1());
        RELEASE_ASSERT(!child2());
        setOpAndDefaultFlags(Identity);
    }

    bool mustGenerate()
    {
        return m_flags & NodeMustGenerate;
    }
    
    void setCanExit(bool exits)
    {
        if (exits)
            m_flags &= ~NodeDoesNotExit;
        else
            m_flags |= NodeDoesNotExit;
    }
    
    bool canExit()
    {
        return !(m_flags & NodeDoesNotExit);
    }
    
    bool isConstant()
    {
        return op() == JSConstant;
    }
    
    bool isWeakConstant()
    {
        return op() == WeakJSConstant;
    }
    
    bool isStronglyProvedConstantIn(InlineCallFrame* inlineCallFrame)
    {
        return !!(flags() & NodeIsStaticConstant)
            && origin.semantic.inlineCallFrame == inlineCallFrame;
    }
    
    bool isStronglyProvedConstantIn(const CodeOrigin& codeOrigin)
    {
        return isStronglyProvedConstantIn(codeOrigin.inlineCallFrame);
    }
    
    bool isPhantomArguments()
    {
        return op() == PhantomArguments;
    }
    
    bool hasConstant()
    {
        switch (op()) {
        case JSConstant:
        case WeakJSConstant:
        case PhantomArguments:
            return true;
        default:
            return false;
        }
    }

    unsigned constantNumber()
    {
        ASSERT(isConstant());
        return m_opInfo;
    }
    
    void convertToConstant(unsigned constantNumber)
    {
        m_op = JSConstant;
        m_flags &= ~(NodeMustGenerate | NodeMightClobber | NodeClobbersWorld);
        m_opInfo = constantNumber;
        children.reset();
    }
    
    void convertToWeakConstant(JSCell* cell)
    {
        m_op = WeakJSConstant;
        m_flags &= ~(NodeMustGenerate | NodeMightClobber | NodeClobbersWorld);
        m_opInfo = bitwise_cast<uintptr_t>(cell);
        children.reset();
    }
    
    void convertToConstantStoragePointer(void* pointer)
    {
        ASSERT(op() == GetIndexedPropertyStorage);
        m_op = ConstantStoragePointer;
        m_opInfo = bitwise_cast<uintptr_t>(pointer);
    }
    
    void convertToGetLocalUnlinked(VirtualRegister local)
    {
        m_op = GetLocalUnlinked;
        m_flags &= ~(NodeMustGenerate | NodeMightClobber | NodeClobbersWorld);
        m_opInfo = local.offset();
        m_opInfo2 = VirtualRegister().offset();
        children.reset();
    }
    
    void convertToStructureTransitionWatchpoint(Structure* structure)
    {
        ASSERT(m_op == CheckStructure || m_op == ArrayifyToStructure);
        ASSERT(!child2());
        ASSERT(!child3());
        m_opInfo = bitwise_cast<uintptr_t>(structure);
        m_op = StructureTransitionWatchpoint;
    }
    
    void convertToStructureTransitionWatchpoint()
    {
        convertToStructureTransitionWatchpoint(structureSet().singletonStructure());
    }
    
    void convertToGetByOffset(unsigned storageAccessDataIndex, Edge storage)
    {
        ASSERT(m_op == GetById || m_op == GetByIdFlush);
        m_opInfo = storageAccessDataIndex;
        children.setChild2(children.child1());
        children.child2().setUseKind(KnownCellUse);
        children.setChild1(storage);
        m_op = GetByOffset;
        m_flags &= ~NodeClobbersWorld;
    }
    
    void convertToPutByOffset(unsigned storageAccessDataIndex, Edge storage)
    {
        ASSERT(m_op == PutById || m_op == PutByIdDirect);
        m_opInfo = storageAccessDataIndex;
        children.setChild3(children.child2());
        children.setChild2(children.child1());
        children.setChild1(storage);
        m_op = PutByOffset;
        m_flags &= ~NodeClobbersWorld;
    }
    
    void convertToPhantomLocal()
    {
        ASSERT(m_op == Phantom && (child1()->op() == Phi || child1()->op() == SetLocal || child1()->op() == SetArgument));
        m_op = PhantomLocal;
        m_opInfo = child1()->m_opInfo; // Copy the variableAccessData.
        children.setChild1(Edge());
    }
    
    void convertToGetLocal(VariableAccessData* variable, Node* phi)
    {
        ASSERT(m_op == GetLocalUnlinked);
        m_op = GetLocal;
        m_opInfo = bitwise_cast<uintptr_t>(variable);
        m_opInfo2 = 0;
        children.setChild1(Edge(phi));
    }
    
    void convertToToString()
    {
        ASSERT(m_op == ToPrimitive);
        m_op = ToString;
    }
    
    JSCell* weakConstant()
    {
        ASSERT(op() == WeakJSConstant);
        return bitwise_cast<JSCell*>(m_opInfo);
    }
    
    JSValue valueOfJSConstant(CodeBlock* codeBlock)
    {
        switch (op()) {
        case WeakJSConstant:
            return JSValue(weakConstant());
        case JSConstant:
            return codeBlock->constantRegister(FirstConstantRegisterIndex + constantNumber()).get();
        case PhantomArguments:
            return JSValue();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return JSValue(); // Have to return something in release mode.
        }
    }

    bool isInt32Constant(CodeBlock* codeBlock)
    {
        return isConstant() && valueOfJSConstant(codeBlock).isInt32();
    }
    
    bool isDoubleConstant(CodeBlock* codeBlock)
    {
        bool result = isConstant() && valueOfJSConstant(codeBlock).isDouble();
        if (result)
            ASSERT(!isInt32Constant(codeBlock));
        return result;
    }
    
    bool isNumberConstant(CodeBlock* codeBlock)
    {
        bool result = isConstant() && valueOfJSConstant(codeBlock).isNumber();
        ASSERT(result == (isInt32Constant(codeBlock) || isDoubleConstant(codeBlock)));
        return result;
    }
    
    bool isBooleanConstant(CodeBlock* codeBlock)
    {
        return isConstant() && valueOfJSConstant(codeBlock).isBoolean();
    }
    
    bool containsMovHint()
    {
        switch (op()) {
        case MovHint:
        case ZombieHint:
            return true;
        default:
            return false;
        }
    }
    
    bool hasVariableAccessData(Graph&);
    bool hasLocal(Graph& graph)
    {
        return hasVariableAccessData(graph);
    }
    
    // This is useful for debugging code, where a node that should have a variable
    // access data doesn't have one because it hasn't been initialized yet.
    VariableAccessData* tryGetVariableAccessData()
    {
        VariableAccessData* result = reinterpret_cast<VariableAccessData*>(m_opInfo);
        if (!result)
            return 0;
        return result->find();
    }
    
    VariableAccessData* variableAccessData()
    {
        return reinterpret_cast<VariableAccessData*>(m_opInfo)->find();
    }
    
    VirtualRegister local()
    {
        return variableAccessData()->local();
    }
    
    VirtualRegister machineLocal()
    {
        return variableAccessData()->machineLocal();
    }
    
    bool hasUnlinkedLocal()
    {
        switch (op()) {
        case GetLocalUnlinked:
        case ExtractOSREntryLocal:
        case MovHint:
        case ZombieHint:
            return true;
        default:
            return false;
        }
    }
    
    VirtualRegister unlinkedLocal()
    {
        ASSERT(hasUnlinkedLocal());
        return static_cast<VirtualRegister>(m_opInfo);
    }
    
    bool hasUnlinkedMachineLocal()
    {
        return op() == GetLocalUnlinked;
    }
    
    void setUnlinkedMachineLocal(VirtualRegister reg)
    {
        ASSERT(hasUnlinkedMachineLocal());
        m_opInfo2 = reg.offset();
    }
    
    VirtualRegister unlinkedMachineLocal()
    {
        ASSERT(hasUnlinkedMachineLocal());
        return VirtualRegister(m_opInfo2);
    }
    
    bool hasPhi()
    {
        return op() == Upsilon;
    }
    
    Node* phi()
    {
        ASSERT(hasPhi());
        return bitwise_cast<Node*>(m_opInfo);
    }

    bool isStoreBarrier()
    {
        switch (op()) {
        case StoreBarrier:
        case ConditionalStoreBarrier:
        case StoreBarrierWithNullCheck:
            return true;
        default:
            return false;
        }
    }

    bool hasIdentifier()
    {
        switch (op()) {
        case GetById:
        case GetByIdFlush:
        case PutById:
        case PutByIdDirect:
            return true;
        default:
            return false;
        }
    }

    unsigned identifierNumber()
    {
        ASSERT(hasIdentifier());
        return m_opInfo;
    }
    
    bool hasArithNodeFlags()
    {
        switch (op()) {
        case UInt32ToNumber:
        case ArithAdd:
        case ArithSub:
        case ArithNegate:
        case ArithMul:
        case ArithAbs:
        case ArithMin:
        case ArithMax:
        case ArithMod:
        case ArithDiv:
        case ValueAdd:
            return true;
        default:
            return false;
        }
    }
    
    // This corrects the arithmetic node flags, so that irrelevant bits are
    // ignored. In particular, anything other than ArithMul does not need
    // to know if it can speculate on negative zero.
    NodeFlags arithNodeFlags()
    {
        NodeFlags result = m_flags & NodeArithFlagsMask;
        if (op() == ArithMul || op() == ArithDiv || op() == ArithMod || op() == ArithNegate || op() == DoubleAsInt32)
            return result;
        return result & ~NodeBytecodeNeedsNegZero;
    }
    
    bool hasConstantBuffer()
    {
        return op() == NewArrayBuffer;
    }
    
    NewArrayBufferData* newArrayBufferData()
    {
        ASSERT(hasConstantBuffer());
        return reinterpret_cast<NewArrayBufferData*>(m_opInfo);
    }
    
    unsigned startConstant()
    {
        return newArrayBufferData()->startConstant;
    }
    
    unsigned numConstants()
    {
        return newArrayBufferData()->numConstants;
    }
    
    bool hasIndexingType()
    {
        switch (op()) {
        case NewArray:
        case NewArrayWithSize:
        case NewArrayBuffer:
            return true;
        default:
            return false;
        }
    }
    
    IndexingType indexingType()
    {
        ASSERT(hasIndexingType());
        if (op() == NewArrayBuffer)
            return newArrayBufferData()->indexingType;
        return m_opInfo;
    }
    
    bool hasTypedArrayType()
    {
        switch (op()) {
        case NewTypedArray:
            return true;
        default:
            return false;
        }
    }
    
    TypedArrayType typedArrayType()
    {
        ASSERT(hasTypedArrayType());
        TypedArrayType result = static_cast<TypedArrayType>(m_opInfo);
        ASSERT(isTypedView(result));
        return result;
    }
    
    bool hasInlineCapacity()
    {
        return op() == CreateThis;
    }

    unsigned inlineCapacity()
    {
        ASSERT(hasInlineCapacity());
        return m_opInfo;
    }

    void setIndexingType(IndexingType indexingType)
    {
        ASSERT(hasIndexingType());
        m_opInfo = indexingType;
    }
    
    bool hasRegexpIndex()
    {
        return op() == NewRegexp;
    }
    
    unsigned regexpIndex()
    {
        ASSERT(hasRegexpIndex());
        return m_opInfo;
    }
    
    bool hasVarNumber()
    {
        return op() == GetClosureVar || op() == PutClosureVar;
    }

    int varNumber()
    {
        ASSERT(hasVarNumber());
        return m_opInfo;
    }
    
    bool hasRegisterPointer()
    {
        return op() == GetGlobalVar || op() == PutGlobalVar;
    }
    
    WriteBarrier<Unknown>* registerPointer()
    {
        return bitwise_cast<WriteBarrier<Unknown>*>(m_opInfo);
    }
    
    bool hasResult()
    {
        return m_flags & NodeResultMask;
    }

    bool hasInt32Result()
    {
        return (m_flags & NodeResultMask) == NodeResultInt32;
    }
    
    bool hasNumberResult()
    {
        return (m_flags & NodeResultMask) == NodeResultNumber;
    }
    
    bool hasJSResult()
    {
        return (m_flags & NodeResultMask) == NodeResultJS;
    }
    
    bool hasBooleanResult()
    {
        return (m_flags & NodeResultMask) == NodeResultBoolean;
    }

    bool hasStorageResult()
    {
        return (m_flags & NodeResultMask) == NodeResultStorage;
    }

    bool isJump()
    {
        return op() == Jump;
    }

    bool isBranch()
    {
        return op() == Branch;
    }
    
    bool isSwitch()
    {
        return op() == Switch;
    }

    bool isTerminal()
    {
        switch (op()) {
        case Jump:
        case Branch:
        case Switch:
        case Return:
        case Unreachable:
            return true;
        default:
            return false;
        }
    }

    unsigned takenBytecodeOffsetDuringParsing()
    {
        ASSERT(isBranch() || isJump());
        return m_opInfo;
    }

    unsigned notTakenBytecodeOffsetDuringParsing()
    {
        ASSERT(isBranch());
        return m_opInfo2;
    }
    
    void setTakenBlock(BasicBlock* block)
    {
        ASSERT(isBranch() || isJump());
        m_opInfo = bitwise_cast<uintptr_t>(block);
    }
    
    void setNotTakenBlock(BasicBlock* block)
    {
        ASSERT(isBranch());
        m_opInfo2 = bitwise_cast<uintptr_t>(block);
    }
    
    BasicBlock*& takenBlock()
    {
        ASSERT(isBranch() || isJump());
        return *bitwise_cast<BasicBlock**>(&m_opInfo);
    }
    
    BasicBlock*& notTakenBlock()
    {
        ASSERT(isBranch());
        return *bitwise_cast<BasicBlock**>(&m_opInfo2);
    }
    
    SwitchData* switchData()
    {
        ASSERT(isSwitch());
        return bitwise_cast<SwitchData*>(m_opInfo);
    }
    
    unsigned numSuccessors()
    {
        switch (op()) {
        case Jump:
            return 1;
        case Branch:
            return 2;
        case Switch:
            return switchData()->cases.size() + 1;
        default:
            return 0;
        }
    }
    
    BasicBlock*& successor(unsigned index)
    {
        if (isSwitch()) {
            if (index < switchData()->cases.size())
                return switchData()->cases[index].target;
            RELEASE_ASSERT(index == switchData()->cases.size());
            return switchData()->fallThrough;
        }
        switch (index) {
        case 0:
            return takenBlock();
        case 1:
            return notTakenBlock();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return takenBlock();
        }
    }
    
    BasicBlock*& successorForCondition(bool condition)
    {
        ASSERT(isBranch());
        return condition ? takenBlock() : notTakenBlock();
    }
    
    bool hasHeapPrediction()
    {
        switch (op()) {
        case GetById:
        case GetByIdFlush:
        case GetByVal:
        case GetMyArgumentByVal:
        case GetMyArgumentByValSafe:
        case Call:
        case Construct:
        case GetByOffset:
        case GetClosureVar:
        case ArrayPop:
        case ArrayPush:
        case RegExpExec:
        case RegExpTest:
        case GetGlobalVar:
            return true;
        default:
            return false;
        }
    }
    
    SpeculatedType getHeapPrediction()
    {
        ASSERT(hasHeapPrediction());
        return static_cast<SpeculatedType>(m_opInfo2);
    }
    
    bool predictHeap(SpeculatedType prediction)
    {
        ASSERT(hasHeapPrediction());
        
        return mergeSpeculation(m_opInfo2, prediction);
    }
    
    bool hasFunction()
    {
        switch (op()) {
        case CheckFunction:
        case AllocationProfileWatchpoint:
            return true;
        default:
            return false;
        }
    }

    JSCell* function()
    {
        ASSERT(hasFunction());
        JSCell* result = reinterpret_cast<JSFunction*>(m_opInfo);
        ASSERT(JSValue(result).isFunction());
        return result;
    }
    
    bool hasExecutable()
    {
        return op() == CheckExecutable;
    }
    
    ExecutableBase* executable()
    {
        return jsCast<ExecutableBase*>(reinterpret_cast<JSCell*>(m_opInfo));
    }
    
    bool hasVariableWatchpointSet()
    {
        return op() == NotifyWrite || op() == VariableWatchpoint;
    }
    
    VariableWatchpointSet* variableWatchpointSet()
    {
        return reinterpret_cast<VariableWatchpointSet*>(m_opInfo);
    }
    
    bool hasTypedArray()
    {
        return op() == TypedArrayWatchpoint;
    }
    
    JSArrayBufferView* typedArray()
    {
        return reinterpret_cast<JSArrayBufferView*>(m_opInfo);
    }
    
    bool hasStoragePointer()
    {
        return op() == ConstantStoragePointer;
    }
    
    void* storagePointer()
    {
        return reinterpret_cast<void*>(m_opInfo);
    }

    bool hasStructureTransitionData()
    {
        switch (op()) {
        case PutStructure:
        case PhantomPutStructure:
        case AllocatePropertyStorage:
        case ReallocatePropertyStorage:
            return true;
        default:
            return false;
        }
    }
    
    StructureTransitionData& structureTransitionData()
    {
        ASSERT(hasStructureTransitionData());
        return *reinterpret_cast<StructureTransitionData*>(m_opInfo);
    }
    
    bool hasStructureSet()
    {
        switch (op()) {
        case CheckStructure:
            return true;
        default:
            return false;
        }
    }
    
    StructureSet& structureSet()
    {
        ASSERT(hasStructureSet());
        return *reinterpret_cast<StructureSet*>(m_opInfo);
    }
    
    bool hasStructure()
    {
        switch (op()) {
        case StructureTransitionWatchpoint:
        case ArrayifyToStructure:
        case NewObject:
        case NewStringObject:
            return true;
        default:
            return false;
        }
    }
    
    Structure* structure()
    {
        ASSERT(hasStructure());
        return reinterpret_cast<Structure*>(m_opInfo);
    }
    
    bool hasStorageAccessData()
    {
        return op() == GetByOffset || op() == PutByOffset;
    }
    
    unsigned storageAccessDataIndex()
    {
        ASSERT(hasStorageAccessData());
        return m_opInfo;
    }
    
    bool hasFunctionDeclIndex()
    {
        return op() == NewFunction
            || op() == NewFunctionNoCheck;
    }
    
    unsigned functionDeclIndex()
    {
        ASSERT(hasFunctionDeclIndex());
        return m_opInfo;
    }
    
    bool hasFunctionExprIndex()
    {
        return op() == NewFunctionExpression;
    }
    
    unsigned functionExprIndex()
    {
        ASSERT(hasFunctionExprIndex());
        return m_opInfo;
    }
    
    bool hasSymbolTable()
    {
        return op() == FunctionReentryWatchpoint;
    }
    
    SymbolTable* symbolTable()
    {
        ASSERT(hasSymbolTable());
        return reinterpret_cast<SymbolTable*>(m_opInfo);
    }
    
    bool hasArrayMode()
    {
        switch (op()) {
        case GetIndexedPropertyStorage:
        case GetArrayLength:
        case PutByValDirect:
        case PutByVal:
        case PutByValAlias:
        case GetByVal:
        case StringCharAt:
        case StringCharCodeAt:
        case CheckArray:
        case Arrayify:
        case ArrayifyToStructure:
        case ArrayPush:
        case ArrayPop:
            return true;
        default:
            return false;
        }
    }
    
    ArrayMode arrayMode()
    {
        ASSERT(hasArrayMode());
        if (op() == ArrayifyToStructure)
            return ArrayMode::fromWord(m_opInfo2);
        return ArrayMode::fromWord(m_opInfo);
    }
    
    bool setArrayMode(ArrayMode arrayMode)
    {
        ASSERT(hasArrayMode());
        if (this->arrayMode() == arrayMode)
            return false;
        m_opInfo = arrayMode.asWord();
        return true;
    }
    
    bool hasArithMode()
    {
        switch (op()) {
        case ArithAdd:
        case ArithSub:
        case ArithNegate:
        case ArithMul:
        case ArithDiv:
        case ArithMod:
        case UInt32ToNumber:
        case DoubleAsInt32:
            return true;
        default:
            return false;
        }
    }

    Arith::Mode arithMode()
    {
        ASSERT(hasArithMode());
        return static_cast<Arith::Mode>(m_opInfo);
    }
    
    void setArithMode(Arith::Mode mode)
    {
        m_opInfo = mode;
    }
    
    bool hasVirtualRegister()
    {
        return m_virtualRegister.isValid();
    }
    
    VirtualRegister virtualRegister()
    {
        ASSERT(hasResult());
        ASSERT(m_virtualRegister.isValid());
        return m_virtualRegister;
    }
    
    void setVirtualRegister(VirtualRegister virtualRegister)
    {
        ASSERT(hasResult());
        ASSERT(!m_virtualRegister.isValid());
        m_virtualRegister = virtualRegister;
    }
    
    bool hasExecutionCounter()
    {
        return op() == CountExecution;
    }
    
    Profiler::ExecutionCounter* executionCounter()
    {
        return bitwise_cast<Profiler::ExecutionCounter*>(m_opInfo);
    }

    bool shouldGenerate()
    {
        return m_refCount;
    }
    
    bool willHaveCodeGenOrOSR()
    {
        switch (op()) {
        case SetLocal:
        case MovHint:
        case ZombieHint:
        case PhantomArguments:
            return true;
        case Phantom:
        case HardPhantom:
            return child1().useKindUnchecked() != UntypedUse || child2().useKindUnchecked() != UntypedUse || child3().useKindUnchecked() != UntypedUse;
        default:
            return shouldGenerate();
        }
    }
    
    bool isSemanticallySkippable()
    {
        return op() == CountExecution;
    }

    unsigned refCount()
    {
        return m_refCount;
    }

    unsigned postfixRef()
    {
        return m_refCount++;
    }

    unsigned adjustedRefCount()
    {
        return mustGenerate() ? m_refCount - 1 : m_refCount;
    }
    
    void setRefCount(unsigned refCount)
    {
        m_refCount = refCount;
    }
    
    Edge& child1()
    {
        ASSERT(!(m_flags & NodeHasVarArgs));
        return children.child1();
    }
    
    // This is useful if you want to do a fast check on the first child
    // before also doing a check on the opcode. Use this with care and
    // avoid it if possible.
    Edge child1Unchecked()
    {
        return children.child1Unchecked();
    }

    Edge& child2()
    {
        ASSERT(!(m_flags & NodeHasVarArgs));
        return children.child2();
    }

    Edge& child3()
    {
        ASSERT(!(m_flags & NodeHasVarArgs));
        return children.child3();
    }
    
    unsigned firstChild()
    {
        ASSERT(m_flags & NodeHasVarArgs);
        return children.firstChild();
    }
    
    unsigned numChildren()
    {
        ASSERT(m_flags & NodeHasVarArgs);
        return children.numChildren();
    }
    
    UseKind binaryUseKind()
    {
        ASSERT(child1().useKind() == child2().useKind());
        return child1().useKind();
    }
    
    bool isBinaryUseKind(UseKind useKind)
    {
        return child1().useKind() == useKind && child2().useKind() == useKind;
    }
    
    SpeculatedType prediction()
    {
        return m_prediction;
    }
    
    bool predict(SpeculatedType prediction)
    {
        return mergeSpeculation(m_prediction, prediction);
    }
    
    bool shouldSpeculateInt32()
    {
        return isInt32Speculation(prediction());
    }
    
    bool shouldSpeculateInt32ForArithmetic()
    {
        return isInt32SpeculationForArithmetic(prediction());
    }
    
    bool shouldSpeculateInt32ExpectingDefined()
    {
        return isInt32SpeculationExpectingDefined(prediction());
    }
    
    bool shouldSpeculateMachineInt()
    {
        return isMachineIntSpeculation(prediction());
    }
    
    bool shouldSpeculateMachineIntForArithmetic()
    {
        return isMachineIntSpeculationForArithmetic(prediction());
    }
    
    bool shouldSpeculateMachineIntExpectingDefined()
    {
        return isMachineIntSpeculationExpectingDefined(prediction());
    }
    
    bool shouldSpeculateDouble()
    {
        return isDoubleSpeculation(prediction());
    }
    
    bool shouldSpeculateDoubleForArithmetic()
    {
        return isDoubleSpeculationForArithmetic(prediction());
    }
    
    bool shouldSpeculateNumber()
    {
        return isFullNumberSpeculation(prediction());
    }
    
    bool shouldSpeculateNumberExpectingDefined()
    {
        return isFullNumberSpeculationExpectingDefined(prediction());
    }
    
    bool shouldSpeculateBoolean()
    {
        return isBooleanSpeculation(prediction());
    }
   
    bool shouldSpeculateStringIdent()
    {
        return isStringIdentSpeculation(prediction());
    }
 
    bool shouldSpeculateString()
    {
        return isStringSpeculation(prediction());
    }
 
    bool shouldSpeculateStringObject()
    {
        return isStringObjectSpeculation(prediction());
    }
    
    bool shouldSpeculateStringOrStringObject()
    {
        return isStringOrStringObjectSpeculation(prediction());
    }
    
    bool shouldSpeculateFinalObject()
    {
        return isFinalObjectSpeculation(prediction());
    }
    
    bool shouldSpeculateFinalObjectOrOther()
    {
        return isFinalObjectOrOtherSpeculation(prediction());
    }
    
    bool shouldSpeculateArray()
    {
        return isArraySpeculation(prediction());
    }
    
    bool shouldSpeculateArguments()
    {
        return isArgumentsSpeculation(prediction());
    }
    
    bool shouldSpeculateInt8Array()
    {
        return isInt8ArraySpeculation(prediction());
    }
    
    bool shouldSpeculateInt16Array()
    {
        return isInt16ArraySpeculation(prediction());
    }
    
    bool shouldSpeculateInt32Array()
    {
        return isInt32ArraySpeculation(prediction());
    }
    
    bool shouldSpeculateUint8Array()
    {
        return isUint8ArraySpeculation(prediction());
    }

    bool shouldSpeculateUint8ClampedArray()
    {
        return isUint8ClampedArraySpeculation(prediction());
    }
    
    bool shouldSpeculateUint16Array()
    {
        return isUint16ArraySpeculation(prediction());
    }
    
    bool shouldSpeculateUint32Array()
    {
        return isUint32ArraySpeculation(prediction());
    }
    
    bool shouldSpeculateFloat32Array()
    {
        return isFloat32ArraySpeculation(prediction());
    }
    
    bool shouldSpeculateFloat64Array()
    {
        return isFloat64ArraySpeculation(prediction());
    }
    
    bool shouldSpeculateArrayOrOther()
    {
        return isArrayOrOtherSpeculation(prediction());
    }
    
    bool shouldSpeculateObject()
    {
        return isObjectSpeculation(prediction());
    }
    
    bool shouldSpeculateObjectOrOther()
    {
        return isObjectOrOtherSpeculation(prediction());
    }

    bool shouldSpeculateCell()
    {
        return isCellSpeculation(prediction());
    }
    
    static bool shouldSpeculateBoolean(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateBoolean() && op2->shouldSpeculateBoolean();
    }
    
    static bool shouldSpeculateInt32(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateInt32() && op2->shouldSpeculateInt32();
    }
    
    static bool shouldSpeculateInt32ForArithmetic(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateInt32ForArithmetic() && op2->shouldSpeculateInt32ForArithmetic();
    }
    
    static bool shouldSpeculateInt32ExpectingDefined(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateInt32ExpectingDefined() && op2->shouldSpeculateInt32ExpectingDefined();
    }
    
    static bool shouldSpeculateMachineInt(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateMachineInt() && op2->shouldSpeculateMachineInt();
    }
    
    static bool shouldSpeculateMachineIntForArithmetic(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateMachineIntForArithmetic() && op2->shouldSpeculateMachineIntForArithmetic();
    }
    
    static bool shouldSpeculateMachineIntExpectingDefined(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateMachineIntExpectingDefined() && op2->shouldSpeculateMachineIntExpectingDefined();
    }
    
    static bool shouldSpeculateDoubleForArithmetic(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateDoubleForArithmetic() && op2->shouldSpeculateDoubleForArithmetic();
    }
    
    static bool shouldSpeculateNumber(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateNumber() && op2->shouldSpeculateNumber();
    }
    
    static bool shouldSpeculateNumberExpectingDefined(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateNumberExpectingDefined() && op2->shouldSpeculateNumberExpectingDefined();
    }
    
    static bool shouldSpeculateFinalObject(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateFinalObject() && op2->shouldSpeculateFinalObject();
    }

    static bool shouldSpeculateArray(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateArray() && op2->shouldSpeculateArray();
    }
    
    bool canSpeculateInt32()
    {
        return nodeCanSpeculateInt32(arithNodeFlags());
    }
    
    bool canSpeculateInt52()
    {
        return nodeCanSpeculateInt52(arithNodeFlags());
    }
    
    void dumpChildren(PrintStream& out)
    {
        if (!child1())
            return;
        out.printf("@%u", child1()->index());
        if (!child2())
            return;
        out.printf(", @%u", child2()->index());
        if (!child3())
            return;
        out.printf(", @%u", child3()->index());
    }
    
    // NB. This class must have a trivial destructor.

    NodeOrigin origin;

    // References to up to 3 children, or links to a variable length set of children.
    AdjacencyList children;

private:
    unsigned m_op : 10; // real type is NodeType
    unsigned m_flags : 22;
    // The virtual register number (spill location) associated with this .
    VirtualRegister m_virtualRegister;
    // The number of uses of the result of this operation (+1 for 'must generate' nodes, which have side-effects).
    unsigned m_refCount;
    // The prediction ascribed to this node after propagation.
    SpeculatedType m_prediction;
    // Immediate values, accesses type-checked via accessors above. The first one is
    // big enough to store a pointer.
    uintptr_t m_opInfo;
    uintptr_t m_opInfo2;

public:
    // Fields used by various analyses.
    AbstractValue value;
    
    // Miscellaneous data that is usually meaningless, but can hold some analysis results
    // if you ask right. For example, if you do Graph::initializeNodeOwners(), misc.owner
    // will tell you which basic block a node belongs to. You cannot rely on this persisting
    // across transformations unless you do the maintenance work yourself. Other phases use
    // misc.replacement, but they do so manually: first you do Graph::clearReplacements()
    // and then you set, and use, replacement's yourself.
    //
    // Bottom line: don't use these fields unless you initialize them yourself, or by
    // calling some appropriate methods that initialize them the way you want. Otherwise,
    // these fields are meaningless.
    union {
        Node* replacement;
        BasicBlock* owner;
        bool needsBarrier;
    } misc;
};

inline bool nodeComparator(Node* a, Node* b)
{
    return a->index() < b->index();
}

template<typename T>
CString nodeListDump(const T& nodeList)
{
    return sortedListDump(nodeList, nodeComparator);
}

template<typename T>
CString nodeMapDump(const T& nodeMap, DumpContext* context = 0)
{
    Vector<typename T::KeyType> keys;
    for (
        typename T::const_iterator iter = nodeMap.begin();
        iter != nodeMap.end(); ++iter)
        keys.append(iter->key);
    std::sort(keys.begin(), keys.end(), nodeComparator);
    StringPrintStream out;
    CommaPrinter comma;
    for(unsigned i = 0; i < keys.size(); ++i)
        out.print(comma, keys[i], "=>", inContext(nodeMap.get(keys[i]), context));
    return out.toCString();
}

} } // namespace JSC::DFG

namespace WTF {

void printInternal(PrintStream&, JSC::DFG::SwitchKind);
void printInternal(PrintStream&, JSC::DFG::Node*);

inline JSC::DFG::Node* inContext(JSC::DFG::Node* node, JSC::DumpContext*) { return node; }

} // namespace WTF

using WTF::inContext;

#endif
#endif
