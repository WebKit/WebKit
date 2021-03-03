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

#pragma once

#if ENABLE(DFG_JIT)

#include "B3SparseCollection.h"
#include "BasicBlockLocation.h"
#include "CheckPrivateBrandVariant.h"
#include "CodeBlock.h"
#include "DFGAdjacencyList.h"
#include "DFGArithMode.h"
#include "DFGArrayMode.h"
#include "DFGCommon.h"
#include "DFGEpoch.h"
#include "DFGLazyJSValue.h"
#include "DFGMultiGetByOffsetData.h"
#include "DFGNodeFlags.h"
#include "DFGNodeOrigin.h"
#include "DFGNodeType.h"
#include "DFGObjectMaterializationData.h"
#include "DFGOpInfo.h"
#include "DFGRegisteredStructure.h"
#include "DFGRegisteredStructureSet.h"
#include "DFGTransition.h"
#include "DFGUseKind.h"
#include "DFGVariableAccessData.h"
#include "DOMJITSignature.h"
#include "DeleteByIdVariant.h"
#include "GetByIdVariant.h"
#include "JSCJSValue.h"
#include "Operands.h"
#include "PrivateFieldPutKind.h"
#include "PutByIdVariant.h"
#include "SetPrivateBrandVariant.h"
#include "SpeculatedType.h"
#include "TypeLocation.h"
#include "ValueProfile.h"
#include <type_traits>
#include <wtf/FastMalloc.h>
#include <wtf/ListDump.h>
#include <wtf/LoggingHashSet.h>

namespace JSC {

namespace DOMJIT {
class GetterSetter;
class CallDOMGetterSnippet;
class Signature;
}

namespace Profiler {
class ExecutionCounter;
}

class Snippet;

namespace DFG {

class Graph;
class PromotedLocationDescriptor;
struct BasicBlock;

struct StorageAccessData {
    PropertyOffset offset;
    unsigned identifierNumber;
};

struct MultiPutByOffsetData {
    unsigned identifierNumber;
    Vector<PutByIdVariant, 2> variants;
    
    bool writesStructures() const;
    bool reallocatesStorage() const;
};

struct MultiDeleteByOffsetData {
    unsigned identifierNumber;
    Vector<DeleteByIdVariant, 2> variants;

    bool writesStructures() const;
    bool allVariantsStoreEmpty() const;
};

struct MatchStructureVariant {
    RegisteredStructure structure;
    bool result;
};

struct MatchStructureData {
    Vector<MatchStructureVariant, 2> variants;
};

struct NewArrayBufferData {
    union {
        struct {
            unsigned vectorLengthHint;
            unsigned indexingMode;
        };
        uint64_t asQuadWord;
    };
};
static_assert(sizeof(IndexingType) <= sizeof(unsigned), "");
static_assert(sizeof(NewArrayBufferData) == sizeof(uint64_t), "");

struct DataViewData {
    union {
        struct {
            uint8_t byteSize;
            bool isSigned;
            bool isFloatingPoint; // Used for the DataViewSet node.
            TriState isLittleEndian;
        };
        uint64_t asQuadWord;
    };
};
static_assert(sizeof(DataViewData) == sizeof(uint64_t), "");

struct BranchTarget {
    BranchTarget()
        : block(nullptr)
        , count(PNaN)
    {
    }
    
    explicit BranchTarget(BasicBlock* block)
        : block(block)
        , count(PNaN)
    {
    }
    
    void setBytecodeIndex(unsigned bytecodeIndex)
    {
        block = bitwise_cast<BasicBlock*>(static_cast<uintptr_t>(bytecodeIndex));
    }
    unsigned bytecodeIndex() const { return bitwise_cast<uintptr_t>(block); }
    
    void dump(PrintStream&) const;
    
    BasicBlock* block;
    float count;
};

struct BranchData {
    static BranchData withBytecodeIndices(
        unsigned takenBytecodeIndex, unsigned notTakenBytecodeIndex)
    {
        BranchData result;
        result.taken.block = bitwise_cast<BasicBlock*>(static_cast<uintptr_t>(takenBytecodeIndex));
        result.notTaken.block = bitwise_cast<BasicBlock*>(static_cast<uintptr_t>(notTakenBytecodeIndex));
        return result;
    }
    
    unsigned takenBytecodeIndex() const { return taken.bytecodeIndex(); }
    unsigned notTakenBytecodeIndex() const { return notTaken.bytecodeIndex(); }
    
    BasicBlock*& forCondition(bool condition)
    {
        if (condition)
            return taken.block;
        return notTaken.block;
    }
    
    BranchTarget taken;
    BranchTarget notTaken;
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
        result.target.setBytecodeIndex(bytecodeIndex);
        return result;
    }
    
    LazyJSValue value;
    BranchTarget target;
};

struct SwitchData {
    // Initializes most fields to obviously invalid values. Anyone
    // constructing this should make sure to initialize everything they
    // care about manually.
    SwitchData()
        : switchTableIndex(UINT_MAX)
        , kind(static_cast<SwitchKind>(-1))
        , didUseJumpTable(false)
    {
    }
    
    Vector<SwitchCase> cases;
    BranchTarget fallThrough;
    size_t switchTableIndex;
    SwitchKind kind;
    bool didUseJumpTable;
};

struct EntrySwitchData {
    Vector<BasicBlock*> cases;
};

struct CallVarargsData {
    int firstVarArgOffset;
};

struct LoadVarargsData {
    VirtualRegister start; // Local for the first element. This is the first actual argument, not this.
    VirtualRegister count; // Local for the count.
    VirtualRegister machineStart;
    VirtualRegister machineCount;
    unsigned offset; // Which array element to start with. Usually this is 0.
    unsigned mandatoryMinimum; // The number of elements on the stack that must be initialized; if the array is too short then the missing elements must get undefined. Does not include "this".
    unsigned limit; // Maximum number of elements to load. Includes "this".
};

struct StackAccessData {
    StackAccessData()
        : format(DeadFlush)
    {
    }
    
    StackAccessData(Operand operand, FlushFormat format)
        : operand(operand)
        , format(format)
    {
    }
    
    Operand operand;
    VirtualRegister machineLocal;
    FlushFormat format;
    
    FlushedAt flushedAt() { return FlushedAt(format, machineLocal); }
};

struct CallDOMGetterData {
    FunctionPtr<CustomAccessorPtrTag> customAccessorGetter;
    const DOMJIT::GetterSetter* domJIT { nullptr };
    DOMJIT::CallDOMGetterSnippet* snippet { nullptr };
    unsigned identifierNumber { 0 };
    const ClassInfo* requiredClassInfo { nullptr };
};

enum class BucketOwnerType : uint32_t {
    Map,
    Set
};

// === Node ===
//
// Node represents a single operation in the data flow graph.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(DFGNode);
struct Node {
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DFGNode);
public:
    static const char HashSetTemplateInstantiationString[];
    
    enum VarArgTag { VarArg };
    
    Node() { }
    
    Node(NodeType op, NodeOrigin nodeOrigin, const AdjacencyList& children)
        : origin(nodeOrigin)
        , children(children)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
        , owner(nullptr)
    {
        m_misc.replacement = nullptr;
        setOpAndDefaultFlags(op);
    }
    
    // Construct a node with up to 3 children, no immediate value.
    Node(NodeType op, NodeOrigin nodeOrigin, Edge child1 = Edge(), Edge child2 = Edge(), Edge child3 = Edge())
        : origin(nodeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
        , owner(nullptr)
    {
        m_misc.replacement = nullptr;
        setOpAndDefaultFlags(op);
        ASSERT(!(m_flags & NodeHasVarArgs));
    }

    // Construct a node with up to 3 children, no immediate value.
    Node(NodeFlags result, NodeType op, NodeOrigin nodeOrigin, Edge child1 = Edge(), Edge child2 = Edge(), Edge child3 = Edge())
        : origin(nodeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
        , owner(nullptr)
    {
        m_misc.replacement = nullptr;
        setOpAndDefaultFlags(op);
        setResult(result);
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
        , owner(nullptr)
    {
        m_misc.replacement = nullptr;
        setOpAndDefaultFlags(op);
        ASSERT(!(m_flags & NodeHasVarArgs));
    }

    // Construct a node with up to 3 children and an immediate value.
    Node(NodeFlags result, NodeType op, NodeOrigin nodeOrigin, OpInfo imm, Edge child1 = Edge(), Edge child2 = Edge(), Edge child3 = Edge())
        : origin(nodeOrigin)
        , children(AdjacencyList::Fixed, child1, child2, child3)
        , m_virtualRegister(VirtualRegister())
        , m_refCount(1)
        , m_prediction(SpecNone)
        , m_opInfo(imm.m_value)
        , owner(nullptr)
    {
        m_misc.replacement = nullptr;
        setOpAndDefaultFlags(op);
        setResult(result);
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
        , owner(nullptr)
    {
        m_misc.replacement = nullptr;
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
        , owner(nullptr)
    {
        m_misc.replacement = nullptr;
        setOpAndDefaultFlags(op);
        ASSERT(m_flags & NodeHasVarArgs);
    }
    
    NodeType op() const { return static_cast<NodeType>(m_op); }
    NodeFlags flags() const { return m_flags; }

    unsigned index() const { return m_index; }
    
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
        NodeFlags newFlags = m_flags | flags;
        if (newFlags == m_flags)
            return false;
        m_flags = newFlags;
        return true;
    }
    
    bool filterFlags(NodeFlags flags)
    {
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
    
    void setResult(NodeFlags result)
    {
        ASSERT(!(result & ~NodeResultMask));
        clearFlags(NodeResultMask);
        mergeFlags(result);
    }
    
    NodeFlags result() const
    {
        return flags() & NodeResultMask;
    }
    
    void setOpAndDefaultFlags(NodeType op)
    {
        m_op = op;
        m_flags = defaultFlags(op);
    }

    void remove(Graph&);
    void removeWithoutChecks();

    void convertToCheckStructure(RegisteredStructureSet* set)
    {
        setOpAndDefaultFlags(CheckStructure);
        m_opInfo = set;
    }

    void convertToCheckStructureOrEmpty(RegisteredStructureSet* set)
    {
        if (SpecCellCheck & SpecEmpty)
            setOpAndDefaultFlags(CheckStructureOrEmpty);
        else
            setOpAndDefaultFlags(CheckStructure);
        m_opInfo = set;
    }

    void convertCheckStructureOrEmptyToCheckStructure()
    {
        ASSERT(op() == CheckStructureOrEmpty);
        setOpAndDefaultFlags(CheckStructure);
    }

    void convertToCheckStructureImmediate(Node* structure)
    {
        ASSERT(op() == CheckStructure || op() == CheckStructureOrEmpty);
        m_op = CheckStructureImmediate;
        children.setChild1(Edge(structure, CellUse));
    }

    void convertCheckArrayOrEmptyToCheckArray()
    {
        ASSERT(op() == CheckArrayOrEmpty);
        setOpAndDefaultFlags(CheckArray);
    }
    
    void replaceWith(Graph&, Node* other);
    void replaceWithWithoutChecks(Node* other);

    void convertToIdentity();
    void convertToIdentityOn(Node*);

    bool mustGenerate() const
    {
        return m_flags & NodeMustGenerate;
    }

    bool hasVarArgs() const
    {
        return m_flags & NodeHasVarArgs;
    }
    
    bool isConstant()
    {
        switch (op()) {
        case JSConstant:
        case DoubleConstant:
        case Int52Constant:
            return true;
        default:
            return false;
        }
    }
    
    bool hasConstant()
    {
        switch (op()) {
        case CheckIsConstant:
        case JSConstant:
        case DoubleConstant:
        case Int52Constant:
            return true;
            
        case PhantomDirectArguments:
        case PhantomClonedArguments:
            // These pretend to be the empty value constant for the benefit of the DFG backend, which
            // otherwise wouldn't take kindly to a node that doesn't compute a value.
            return true;
            
        default:
            return false;
        }
    }

    FrozenValue* constant()
    {
        ASSERT(hasConstant());
        
        if (op() == PhantomDirectArguments || op() == PhantomClonedArguments) {
            // These pretend to be the empty value constant for the benefit of the DFG backend, which
            // otherwise wouldn't take kindly to a node that doesn't compute a value.
            return FrozenValue::emptySingleton();
        }
        
        return m_opInfo.as<FrozenValue*>();
    }
    
    // Don't call this directly - use Graph::convertToConstant() instead!
    void convertToConstant(FrozenValue* value)
    {
        if (hasDoubleResult())
            m_op = DoubleConstant;
        else if (hasInt52Result())
            m_op = Int52Constant;
        else
            m_op = JSConstant;
        m_flags &= ~(NodeMustGenerate | NodeHasVarArgs);
        m_opInfo = value;
        children.reset();
    }

    void convertToLazyJSConstant(Graph&, LazyJSValue);
    
    void convertToConstantStoragePointer(void* pointer)
    {
        ASSERT(op() == GetIndexedPropertyStorage);
        m_op = ConstantStoragePointer;
        m_opInfo = pointer;
        children.reset();
    }
    
    void convertToPutStack(StackAccessData* data)
    {
        m_op = PutStack;
        m_flags |= NodeMustGenerate;
        m_opInfo = data;
        m_opInfo2 = OpInfoWrapper();
    }
    
    void convertToGetStack(StackAccessData* data)
    {
        m_op = GetStack;
        m_flags &= ~NodeMustGenerate;
        m_opInfo = data;
        m_opInfo2 = OpInfoWrapper();
        children.reset();
    }
    
    void convertToGetByOffset(StorageAccessData& data, Edge storage, Edge base)
    {
        ASSERT(m_op == GetById || m_op == GetByIdFlush || m_op == GetByIdDirect || m_op == GetByIdDirectFlush || m_op == GetPrivateNameById || m_op == MultiGetByOffset);
        m_opInfo = &data;
        children.setChild1(storage);
        children.setChild2(base);
        m_op = GetByOffset;
        m_flags &= ~NodeMustGenerate;
    }
    
    void convertToMultiGetByOffset(MultiGetByOffsetData* data)
    {
        RELEASE_ASSERT(m_op == GetById || m_op == GetByIdFlush || m_op == GetByIdDirect || m_op == GetByIdDirectFlush || m_op == GetPrivateNameById);
        m_opInfo = data;
        child1().setUseKind(CellUse);
        m_op = MultiGetByOffset;
        RELEASE_ASSERT(m_flags & NodeMustGenerate);
    }
    
    void convertToPutByOffset(StorageAccessData& data, Edge storage, Edge base)
    {
        ASSERT(m_op == PutById || m_op == PutByIdDirect || m_op == PutByIdFlush || m_op == MultiPutByOffset || m_op == PutPrivateNameById);
        m_opInfo = &data;
        children.setChild3(children.child2());
        children.setChild2(base);
        children.setChild1(storage);
        m_op = PutByOffset;
    }
    
    void convertToMultiPutByOffset(MultiPutByOffsetData* data)
    {
        ASSERT(m_op == PutById || m_op == PutByIdDirect || m_op == PutByIdFlush || m_op == PutPrivateNameById);
        m_opInfo = data;
        m_op = MultiPutByOffset;
    }
    
    void convertToPhantomNewObject()
    {
        ASSERT(m_op == NewObject);
        m_op = PhantomNewObject;
        m_flags &= ~NodeHasVarArgs;
        m_flags |= NodeMustGenerate;
        m_opInfo = OpInfoWrapper();
        m_opInfo2 = OpInfoWrapper();
        children = AdjacencyList();
    }

    void convertToPhantomNewFunction()
    {
        ASSERT(m_op == NewFunction || m_op == NewGeneratorFunction || m_op == NewAsyncFunction || m_op == NewAsyncGeneratorFunction);
        m_op = PhantomNewFunction;
        m_flags |= NodeMustGenerate;
        m_opInfo = OpInfoWrapper();
        m_opInfo2 = OpInfoWrapper();
        children = AdjacencyList();
    }

    void convertToPhantomNewGeneratorFunction()
    {
        ASSERT(m_op == NewGeneratorFunction);
        m_op = PhantomNewGeneratorFunction;
        m_flags |= NodeMustGenerate;
        m_opInfo = OpInfoWrapper();
        m_opInfo2 = OpInfoWrapper();
        children = AdjacencyList();
    }

    void convertToPhantomNewInternalFieldObject()
    {
        ASSERT(m_op == NewInternalFieldObject);
        m_op = PhantomNewInternalFieldObject;
        m_flags &= ~NodeHasVarArgs;
        m_flags |= NodeMustGenerate;
        m_opInfo = OpInfoWrapper();
        m_opInfo2 = OpInfoWrapper();
        children = AdjacencyList();
    }

    void convertToPhantomNewAsyncFunction()
    {
        ASSERT(m_op == NewAsyncFunction);
        m_op = PhantomNewAsyncFunction;
        m_flags |= NodeMustGenerate;
        m_opInfo = OpInfoWrapper();
        m_opInfo2 = OpInfoWrapper();
        children = AdjacencyList();
    }

    void convertToPhantomNewAsyncGeneratorFunction()
    {
        ASSERT(m_op == NewAsyncGeneratorFunction);
        m_op = PhantomNewAsyncGeneratorFunction;
        m_flags |= NodeMustGenerate;
        m_opInfo = OpInfoWrapper();
        m_opInfo2 = OpInfoWrapper();
        children = AdjacencyList();
    }
    
    void convertToPhantomCreateActivation()
    {
        ASSERT(m_op == CreateActivation);
        m_op = PhantomCreateActivation;
        m_flags &= ~NodeHasVarArgs;
        m_flags |= NodeMustGenerate;
        m_opInfo = OpInfoWrapper();
        m_opInfo2 = OpInfoWrapper();
        children = AdjacencyList();
    }

    void convertToPhantomNewRegexp()
    {
        ASSERT(m_op == NewRegexp);
        setOpAndDefaultFlags(PhantomNewRegexp);
        m_opInfo = OpInfoWrapper();
        m_opInfo2 = OpInfoWrapper();
        children = AdjacencyList();
    }

    void convertPhantomToPhantomLocal()
    {
        ASSERT(m_op == Phantom && (child1()->op() == Phi || child1()->op() == SetLocal || child1()->op() == SetArgumentDefinitely));
        m_op = PhantomLocal;
        m_opInfo = child1()->m_opInfo; // Copy the variableAccessData.
        children.setChild1(Edge());
    }
    
    void convertFlushToPhantomLocal()
    {
        ASSERT(m_op == Flush);
        m_op = PhantomLocal;
        children = AdjacencyList();
    }
    
    void convertToToString()
    {
        ASSERT(m_op == ToPrimitive || m_op == StringValueOf || m_op == ToPropertyKey);
        m_op = ToString;
    }

    void convertToArithNegate()
    {
        ASSERT(m_op == ArithAbs && child1().useKind() == Int32Use);
        m_op = ArithNegate;
    }

    void convertToCompareEqPtr(FrozenValue* cell, Edge node)
    {
        ASSERT(m_op == CompareStrictEq || m_op == SameValue);
        setOpAndDefaultFlags(CompareEqPtr);
        children.setChild1(node);
        children.setChild2(Edge());
        m_opInfo = cell;
    }

    void convertToNumberToStringWithValidRadixConstant(int32_t radix)
    {
        ASSERT(m_op == NumberToStringWithRadix);
        ASSERT(2 <= radix && radix <= 36);
        setOpAndDefaultFlags(NumberToStringWithValidRadixConstant);
        children.setChild2(Edge());
        m_opInfo = radix;
    }

    void convertToGetGlobalThis()
    {
        ASSERT(m_op == ToThis);
        setOpAndDefaultFlags(GetGlobalThis);
        children.setChild1(Edge());
    }

    void convertToCallObjectConstructor(FrozenValue* globalObject)
    {
        ASSERT(m_op == ToObject);
        setOpAndDefaultFlags(CallObjectConstructor);
        m_opInfo = globalObject;
    }

    void convertToNewStringObject(RegisteredStructure structure)
    {
        ASSERT(m_op == CallObjectConstructor || m_op == ToObject);
        setOpAndDefaultFlags(NewStringObject);
        m_opInfo = structure;
        m_opInfo2 = OpInfoWrapper();
    }

    void convertToNewObject(RegisteredStructure structure)
    {
        ASSERT(m_op == CallObjectConstructor || m_op == CreateThis || m_op == ObjectCreate);
        setOpAndDefaultFlags(NewObject);
        children.reset();
        m_opInfo = structure;
        m_opInfo2 = OpInfoWrapper();
    }

    void convertToNewInternalFieldObject(RegisteredStructure structure)
    {
        ASSERT(m_op == CreatePromise);
        setOpAndDefaultFlags(NewInternalFieldObject);
        children.reset();
        m_opInfo = structure;
        m_opInfo2 = OpInfoWrapper();
    }

    void convertToNewInternalFieldObjectWithInlineFields(NodeType newOp, RegisteredStructure structure)
    {
        ASSERT(m_op == CreateAsyncGenerator || m_op == CreateGenerator);
        setOpAndDefaultFlags(newOp);
        children.reset();
        m_opInfo = structure;
        m_opInfo2 = OpInfoWrapper();
    }

    void convertToNewArrayBuffer(FrozenValue* immutableButterfly);
    
    void convertToDirectCall(FrozenValue*);

    void convertToCallDOM(Graph&);

    void convertToRegExpExecNonGlobalOrStickyWithoutChecks(FrozenValue* regExp);
    void convertToRegExpMatchFastGlobalWithoutChecks(FrozenValue* regExp);

    void convertToSetRegExpObjectLastIndex()
    {
        setOp(SetRegExpObjectLastIndex);
        m_opInfo = false;
    }

    void convertToInById(CacheableIdentifier identifier)
    {
        ASSERT(m_op == InByVal);
        setOpAndDefaultFlags(InById);
        children.setChild2(Edge());
        m_opInfo = identifier;
        m_opInfo2 = OpInfoWrapper();
    }
    
    JSValue asJSValue()
    {
        return constant()->value();
    }
     
    bool isInt32Constant()
    {
        return isConstant() && constant()->value().isInt32();
    }
     
    int32_t asInt32()
    {
        return asJSValue().asInt32();
    }
     
    uint32_t asUInt32()
    {
        return asInt32();
    }
     
    bool isDoubleConstant()
    {
        return isConstant() && constant()->value().isDouble();
    }
     
    bool isNumberConstant()
    {
        return isConstant() && constant()->value().isNumber();
    }
    
    double asNumber()
    {
        return asJSValue().asNumber();
    }
     
    bool isAnyIntConstant()
    {
        return isConstant() && constant()->value().isAnyInt();
    }
     
    int64_t asAnyInt()
    {
        return asJSValue().asAnyInt();
    }
     
    bool isBooleanConstant()
    {
        return isConstant() && constant()->value().isBoolean();
    }
     
    bool asBoolean()
    {
        return constant()->value().asBoolean();
    }

    bool isUndefinedOrNullConstant()
    {
        return isConstant() && constant()->value().isUndefinedOrNull();
    }

    bool isCellConstant()
    {
        return isConstant() && constant()->value() && constant()->value().isCell();
    }
     
    JSCell* asCell()
    {
        return constant()->value().asCell();
    }
     
    template<typename T>
    T dynamicCastConstant(VM& vm)
    {
        if (!isCellConstant())
            return nullptr;
        return jsDynamicCast<T>(vm, asCell());
    }
    
    bool hasLazyJSValue()
    {
        return op() == LazyJSConstant;
    }

    LazyJSValue lazyJSValue()
    {
        ASSERT(hasLazyJSValue());
        return *m_opInfo.as<LazyJSValue*>();
    }

    String tryGetString(Graph&);

    JSValue initializationValueForActivation() const
    {
        ASSERT(op() == CreateActivation);
        return m_opInfo2.as<FrozenValue*>()->value();
    }

    bool hasArgumentsChild()
    {
        switch (op()) {
        case GetMyArgumentByVal:
        case GetMyArgumentByValOutOfBounds:
        case VarargsLength:
        case LoadVarargs:
        case ForwardVarargs:
        case CallVarargs:
        case CallForwardVarargs:
        case ConstructVarargs:
        case ConstructForwardVarargs:
        case TailCallVarargs:
        case TailCallForwardVarargs:
        case TailCallVarargsInlinedCaller:
        case TailCallForwardVarargsInlinedCaller:
            return true;
        default:
            return false;
        }
    }

    Edge& argumentsChild()
    {
        switch (op()) {
        case GetMyArgumentByVal:
        case GetMyArgumentByValOutOfBounds:
        case VarargsLength:
            return child1();
        case LoadVarargs:
        case ForwardVarargs:
            return child2();
        case CallVarargs:
        case CallForwardVarargs:
        case ConstructVarargs:
        case ConstructForwardVarargs:
        case TailCallVarargs:
        case TailCallForwardVarargs:
        case TailCallVarargsInlinedCaller:
        case TailCallForwardVarargsInlinedCaller:
            return child3();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return child1();
        }
    }

    bool containsMovHint()
    {
        switch (op()) {
        case MovHint:
            return true;
        default:
            return false;
        }
    }
    
    bool hasVariableAccessData(Graph&);
    bool accessesStack(Graph& graph)
    {
        return hasVariableAccessData(graph);
    }
    
    // This is useful for debugging code, where a node that should have a variable
    // access data doesn't have one because it hasn't been initialized yet.
    VariableAccessData* tryGetVariableAccessData()
    {
        VariableAccessData* result = m_opInfo.as<VariableAccessData*>();
        if (!result)
            return nullptr;
        return result->find();
    }
    
    VariableAccessData* variableAccessData()
    {
        return m_opInfo.as<VariableAccessData*>()->find();
    }
    
    Operand operand()
    {
        return variableAccessData()->operand();
    }
    
    VirtualRegister machineLocal()
    {
        return variableAccessData()->machineLocal();
    }
    
    bool hasUnlinkedOperand()
    {
        switch (op()) {
        case ExtractOSREntryLocal:
        case MovHint:
        case KillStack:
            return true;
        default:
            return false;
        }
    }
    
    Operand unlinkedOperand()
    {
        ASSERT(hasUnlinkedOperand());
        return Operand::fromBits(m_opInfo.as<uint64_t>());
    }
    
    bool hasStackAccessData()
    {
        switch (op()) {
        case PutStack:
        case GetStack:
            return true;
        default:
            return false;
        }
    }
    
    StackAccessData* stackAccessData()
    {
        ASSERT(hasStackAccessData());
        return m_opInfo.as<StackAccessData*>();
    }

    unsigned argumentCountIncludingThis()
    {
        ASSERT(op() == SetArgumentCountIncludingThis);
        return m_opInfo.as<unsigned>();
    }
    
    bool hasPhi()
    {
        return op() == Upsilon;
    }
    
    Node* phi()
    {
        ASSERT(hasPhi());
        return m_opInfo.as<Node*>();
    }

    bool isStoreBarrier()
    {
        return op() == StoreBarrier || op() == FencedStoreBarrier;
    }

    bool hasCacheableIdentifier()
    {
        switch (op()) {
        case TryGetById:
        case GetById:
        case GetByIdFlush:
        case GetByIdWithThis:
        case GetByIdDirect:
        case GetByIdDirectFlush:
        case GetPrivateNameById:
        case DeleteById:
        case InById:
        case PutById:
        case PutByIdFlush:
        case PutByIdDirect:
        case PutByIdWithThis:
        case PutPrivateNameById:
            return true;
        default:
            return false;
        }
    }

    CacheableIdentifier cacheableIdentifier()
    {
        ASSERT(hasCacheableIdentifier());
        return CacheableIdentifier::createFromRawBits(m_opInfo.as<uintptr_t>());
    }

    bool hasIdentifier()
    {
        switch (op()) {
        case PutGetterById:
        case PutSetterById:
        case PutGetterSetterById:
        case GetDynamicVar:
        case PutDynamicVar:
        case ResolveScopeForHoistingFuncDeclInEval:
        case ResolveScope:
        case ToObject:
            return true;
        default:
            return false;
        }
    }

    unsigned identifierNumber()
    {
        ASSERT(hasIdentifier());
        return m_opInfo.as<unsigned>();
    }

    bool hasGetPutInfo()
    {
        switch (op()) {
        case GetDynamicVar:
        case PutDynamicVar:
            return true;
        default:
            return false;
        }
    }

    unsigned getPutInfo()
    {
        ASSERT(hasGetPutInfo());
        return static_cast<unsigned>(m_opInfo.as<uint64_t>() >> 32);
    }

    bool hasAccessorAttributes()
    {
        switch (op()) {
        case PutGetterById:
        case PutSetterById:
        case PutGetterSetterById:
        case PutGetterByVal:
        case PutSetterByVal:
            return true;
        default:
            return false;
        }
    }

    int32_t accessorAttributes()
    {
        ASSERT(hasAccessorAttributes());
        switch (op()) {
        case PutGetterById:
        case PutSetterById:
        case PutGetterSetterById:
            return m_opInfo2.as<int32_t>();
        case PutGetterByVal:
        case PutSetterByVal:
            return m_opInfo.as<int32_t>();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return 0;
        }
    }
    
    bool hasPromotedLocationDescriptor()
    {
        return op() == PutHint;
    }
    
    PromotedLocationDescriptor promotedLocationDescriptor();
    
    // This corrects the arithmetic node flags, so that irrelevant bits are
    // ignored. In particular, anything other than ArithMul or ValueMul does not need
    // to know if it can speculate on negative zero.
    NodeFlags arithNodeFlags()
    {
        NodeFlags result = m_flags & NodeArithFlagsMask;
        if (op() == ArithMul || op() == ArithDiv || op() == ValueDiv || op() == ArithMod || op() == ArithNegate || op() == ArithPow || op() == ArithRound || op() == ArithFloor || op() == ArithCeil || op() == ArithTrunc || op() == DoubleAsInt32 || op() == ValueNegate || op() == ValueMul || op() == ValueDiv)
            return result;
        return result & ~NodeBytecodeNeedsNegZero;
    }

    bool mayHaveNonIntResult()
    {
        return m_flags & NodeMayHaveNonIntResult;
    }
    
    bool mayHaveDoubleResult()
    {
        return m_flags & NodeMayHaveDoubleResult;
    }
    
    bool mayHaveNonNumericResult()
    {
        return m_flags & NodeMayHaveNonNumericResult;
    }

    bool mayHaveBigInt32Result()
    {
        return m_flags & NodeMayHaveBigInt32Result;
    }

    bool mayHaveHeapBigIntResult()
    {
        return m_flags & NodeMayHaveHeapBigIntResult;
    }

    bool mayHaveBigIntResult()
    {
        return mayHaveBigInt32Result() || mayHaveHeapBigIntResult();
    }

    bool hasNewArrayBufferData()
    {
        return op() == NewArrayBuffer || op() == PhantomNewArrayBuffer;
    }
    
    NewArrayBufferData newArrayBufferData()
    {
        ASSERT(hasNewArrayBufferData());
        return m_opInfo2.asNewArrayBufferData();
    }

    unsigned hasVectorLengthHint()
    {
        switch (op()) {
        case NewArray:
        case NewArrayBuffer:
        case PhantomNewArrayBuffer:
            return true;
        default:
            return false;
        }
    }
    
    unsigned vectorLengthHint()
    {
        ASSERT(hasVectorLengthHint());
        if (op() == NewArray)
            return m_opInfo2.as<unsigned>();
        return newArrayBufferData().vectorLengthHint;
    }
    
    bool hasIndexingType()
    {
        switch (op()) {
        case NewArray:
        case NewArrayWithSize:
        case NewArrayBuffer:
        case PhantomNewArrayBuffer:
            return true;
        default:
            return false;
        }
    }

    BitVector* bitVector()
    {
        ASSERT(op() == NewArrayWithSpread || op() == PhantomNewArrayWithSpread);
        return m_opInfo.as<BitVector*>();
    }

    // Return the indexing type that an array allocation *wants* to use. It may end up using a different
    // type if we're having a bad time. You can determine the actual indexing type by asking the global
    // object:
    //
    //     m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(node->indexingType())
    //
    // This will give you a Structure*, and that will have some indexing type that may be different from
    // the this one.
    IndexingType indexingType()
    {
        ASSERT(hasIndexingType());
        if (op() == NewArrayBuffer || op() == PhantomNewArrayBuffer)
            return static_cast<IndexingType>(newArrayBufferData().indexingMode) & IndexingTypeMask;
        return static_cast<IndexingType>(m_opInfo.as<uint32_t>());
    }

    IndexingType indexingMode()
    {
        ASSERT(hasIndexingType());
        if (op() == NewArrayBuffer || op() == PhantomNewArrayBuffer)
            return static_cast<IndexingType>(newArrayBufferData().indexingMode);
        return static_cast<IndexingType>(m_opInfo.as<uint32_t>());
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
        TypedArrayType result = static_cast<TypedArrayType>(m_opInfo.as<uint32_t>());
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
        return m_opInfo.as<unsigned>();
    }

    bool hasIsInternalPromise()
    {
        return op() == CreatePromise;
    }

    bool isInternalPromise()
    {
        ASSERT(hasIsInternalPromise());
        return m_opInfo2.as<bool>();
    }

    void setIndexingType(IndexingType indexingType)
    {
        ASSERT(hasIndexingType());
        m_opInfo = indexingType;
    }
    
    bool hasScopeOffset()
    {
        return op() == GetClosureVar || op() == PutClosureVar;
    }

    ScopeOffset scopeOffset()
    {
        ASSERT(hasScopeOffset());
        return ScopeOffset(m_opInfo.as<uint32_t>());
    }

    unsigned hasInternalFieldIndex()
    {
        return op() == GetInternalField || op() == PutInternalField;
    }

    unsigned internalFieldIndex()
    {
        ASSERT(hasInternalFieldIndex());
        return m_opInfo.as<uint32_t>();
    }
    
    bool hasDirectArgumentsOffset()
    {
        return op() == GetFromArguments || op() == PutToArguments;
    }
    
    DirectArgumentsOffset capturedArgumentsOffset()
    {
        ASSERT(hasDirectArgumentsOffset());
        return DirectArgumentsOffset(m_opInfo.as<uint32_t>());
    }
    
    bool hasRegisterPointer()
    {
        return op() == GetGlobalVar || op() == GetGlobalLexicalVariable || op() == PutGlobalVariable;
    }
    
    WriteBarrier<Unknown>* variablePointer()
    {
        return m_opInfo.as<WriteBarrier<Unknown>*>();
    }
    
    bool hasCallVarargsData()
    {
        switch (op()) {
        case CallVarargs:
        case CallForwardVarargs:
        case TailCallVarargs:
        case TailCallForwardVarargs:
        case TailCallVarargsInlinedCaller:
        case TailCallForwardVarargsInlinedCaller:
        case ConstructVarargs:
        case ConstructForwardVarargs:
            return true;
        default:
            return false;
        }
    }
    
    CallVarargsData* callVarargsData()
    {
        ASSERT(hasCallVarargsData());
        return m_opInfo.as<CallVarargsData*>();
    }
    
    bool hasLoadVarargsData()
    {
        return op() == LoadVarargs || op() == ForwardVarargs || op() == VarargsLength;
    }
    
    LoadVarargsData* loadVarargsData()
    {
        ASSERT(hasLoadVarargsData());
        return m_opInfo.as<LoadVarargsData*>();
    }

    InlineCallFrame* argumentsInlineCallFrame()
    {
        ASSERT(op() == GetArgumentCountIncludingThis);
        return m_opInfo.as<InlineCallFrame*>();
    }

    bool hasQueriedType()
    {
        return op() == IsCellWithType;
    }

    JSType queriedType()
    {
        static_assert(std::is_same<uint8_t, std::underlying_type<JSType>::type>::value, "Ensure that uint8_t is the underlying type for JSType.");
        return static_cast<JSType>(m_opInfo.as<uint32_t>());
    }

    bool hasSpeculatedTypeForQuery()
    {
        return op() == IsCellWithType;
    }

    Optional<SpeculatedType> speculatedTypeForQuery()
    {
        return speculationFromJSType(queriedType());
    }
    
    bool hasResult()
    {
        return !!result();
    }
    
    bool hasInt32Result()
    {
        return result() == NodeResultInt32;
    }

    bool hasInt52Result()
    {
        return result() == NodeResultInt52;
    }
    
    bool hasNumberResult()
    {
        return result() == NodeResultNumber;
    }

    bool hasNumberOrAnyIntResult()
    {
        return hasNumberResult() || hasInt32Result() || hasInt52Result();
    }
    
    bool hasNumericResult()
    {
        switch (op()) {
        case ValueSub:
        case ValueMul:
        case ValueBitAnd:
        case ValueBitOr:
        case ValueBitXor:
        case ValueBitNot:
        case ValueBitLShift:
        case ValueBitRShift:
        case ValueNegate:
            return true;
        default:
            return false;
        }
    }

    bool hasDoubleResult()
    {
        return result() == NodeResultDouble;
    }
    
    bool hasJSResult()
    {
        return result() == NodeResultJS;
    }
    
    bool hasBooleanResult()
    {
        return result() == NodeResultBoolean;
    }

    bool hasStorageResult()
    {
        return result() == NodeResultStorage;
    }
    
    UseKind defaultUseKind()
    {
        return useKindForResult(result());
    }
    
    Edge defaultEdge()
    {
        return Edge(this, defaultUseKind());
    }

    bool isJump()
    {
        return op() == Jump;
    }

    bool isBranch()
    {
        return op() == Branch;
    }
    
    bool isSwitch() const
    {
        return op() == Switch;
    }

    bool isEntrySwitch() const
    {
        return op() == EntrySwitch;
    }

    bool isTerminal()
    {
        switch (op()) {
        case Jump:
        case Branch:
        case Switch:
        case EntrySwitch:
        case Return:
        case TailCall:
        case DirectTailCall:
        case TailCallVarargs:
        case TailCallForwardVarargs:
        case Unreachable:
        case Throw:
        case ThrowStaticError:
            return true;
        default:
            return false;
        }
    }

    bool isFunctionTerminal()
    {
        if (isTerminal() && !numSuccessors())
            return true;

        return false;
    }

    // As is described in DFGNodeType.h's ForceOSRExit, this is a pseudo-terminal.
    // It means that execution should fall out of DFG at this point, but execution
    // does continue in the basic block - just in a different compiler.
    // FIXME: This is used for lightweight reachability decision. But this should
    // be replaced with AI-based reachability ideally.
    bool isPseudoTerminal()
    {
        switch (op()) {
        case ForceOSRExit:
        case CheckBadValue:
            return true;
        default:
            return false;
        }
    }

    unsigned targetBytecodeOffsetDuringParsing()
    {
        ASSERT(isJump());
        return m_opInfo.as<unsigned>();
    }

    BasicBlock*& targetBlock()
    {
        ASSERT(isJump());
        return *bitwise_cast<BasicBlock**>(&m_opInfo.u.pointer);
    }
    
    BranchData* branchData()
    {
        ASSERT(isBranch());
        return m_opInfo.as<BranchData*>();
    }
    
    SwitchData* switchData()
    {
        ASSERT(isSwitch());
        return m_opInfo.as<SwitchData*>();
    }

    EntrySwitchData* entrySwitchData()
    {
        ASSERT(isEntrySwitch());
        return m_opInfo.as<EntrySwitchData*>();
    }

    bool hasIntrinsic()
    {
        switch (op()) {
        case CPUIntrinsic:
        case DateGetTime:
        case DateGetInt32OrNaN:
            return true;
        default:
            return false;
        }
    }

    Intrinsic intrinsic()
    {
        ASSERT(hasIntrinsic());
        return m_opInfo.as<Intrinsic>();
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
        case EntrySwitch:
            return entrySwitchData()->cases.size();
        default:
            return 0;
        }
    }
    
    BasicBlock*& successor(unsigned index)
    {
        if (isSwitch()) {
            if (index < switchData()->cases.size())
                return switchData()->cases[index].target.block;
            RELEASE_ASSERT(index == switchData()->cases.size());
            return switchData()->fallThrough.block;
        } else if (isEntrySwitch())
            return entrySwitchData()->cases[index];

        switch (index) {
        case 0:
            if (isJump())
                return targetBlock();
            return branchData()->taken.block;
        case 1:
            return branchData()->notTaken.block;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return targetBlock();
        }
    }
    
    class SuccessorsIterable {
    public:
        SuccessorsIterable()
            : m_terminal(nullptr)
        {
        }
        
        SuccessorsIterable(Node* terminal)
            : m_terminal(terminal)
        {
        }
        
        class iterator {
        public:
            iterator()
                : m_terminal(nullptr)
                , m_index(UINT_MAX)
            {
            }
            
            iterator(Node* terminal, unsigned index)
                : m_terminal(terminal)
                , m_index(index)
            {
            }
            
            BasicBlock* operator*()
            {
                return m_terminal->successor(m_index);
            }
            
            iterator& operator++()
            {
                m_index++;
                return *this;
            }
            
            bool operator==(const iterator& other) const
            {
                return m_index == other.m_index;
            }
            
            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }
        private:
            Node* m_terminal;
            unsigned m_index;
        };
        
        iterator begin()
        {
            return iterator(m_terminal, 0);
        }
        
        iterator end()
        {
            return iterator(m_terminal, m_terminal->numSuccessors());
        }

        size_t size() const { return m_terminal->numSuccessors(); }
        BasicBlock* at(size_t index) const { return m_terminal->successor(index); }
        BasicBlock* operator[](size_t index) const { return at(index); }
        
    private:
        Node* m_terminal;
    };
    
    SuccessorsIterable successors()
    {
        return SuccessorsIterable(this);
    }
    
    BasicBlock*& successorForCondition(bool condition)
    {
        return branchData()->forCondition(condition);
    }
    
    bool hasHeapPrediction()
    {
        switch (op()) {
        case ArithAbs:
        case ArithRound:
        case ArithFloor:
        case ArithCeil:
        case ArithTrunc:
        case GetDirectPname:
        case GetById:
        case GetByIdFlush:
        case GetByIdWithThis:
        case GetByIdDirect:
        case GetByIdDirectFlush:
        case GetPrototypeOf:
        case TryGetById:
        case GetByVal:
        case GetByValWithThis:
        case GetPrivateName:
        case GetPrivateNameById:
        case Call:
        case DirectCall:
        case TailCallInlinedCaller:
        case DirectTailCallInlinedCaller:
        case Construct:
        case DirectConstruct:
        case CallVarargs:
        case CallEval:
        case TailCallVarargsInlinedCaller:
        case ConstructVarargs:
        case CallForwardVarargs:
        case TailCallForwardVarargsInlinedCaller:
        case GetByOffset:
        case MultiGetByOffset:
        case GetClosureVar:
        case GetInternalField:
        case GetFromArguments:
        case GetArgument:
        case ArrayPop:
        case ArrayPush:
        case RegExpExec:
        case RegExpExecNonGlobalOrSticky:
        case RegExpTest:
        case RegExpMatchFast:
        case RegExpMatchFastGlobal:
        case GetGlobalVar:
        case GetGlobalLexicalVariable:
        case StringReplace:
        case StringReplaceRegExp:
        case ToNumber:
        case ToNumeric:
        case ToObject:
        case CallNumberConstructor:
        case ValueBitAnd:
        case ValueBitOr:
        case ValueBitXor:
        case ValueBitNot:
        case ValueBitLShift:
        case ValueBitRShift:
        case CallObjectConstructor:
        case LoadKeyFromMapBucket:
        case LoadValueFromMapBucket:
        case CallDOMGetter:
        case CallDOM:
        case ParseInt:
        case AtomicsAdd:
        case AtomicsAnd:
        case AtomicsCompareExchange:
        case AtomicsExchange:
        case AtomicsLoad:
        case AtomicsOr:
        case AtomicsStore:
        case AtomicsSub:
        case AtomicsXor:
        case GetDynamicVar:
        case ExtractValueFromWeakMapGet:
        case ToThis:
        case DataViewGetInt:
        case DataViewGetFloat:
        case DateGetInt32OrNaN:
            return true;
        default:
            return false;
        }
    }
    
    SpeculatedType getHeapPrediction()
    {
        ASSERT(hasHeapPrediction());
        return m_opInfo2.as<SpeculatedType>();
    }

    void setHeapPrediction(SpeculatedType prediction)
    {
        ASSERT(hasHeapPrediction());
        m_opInfo2 = prediction;
    }

    SpeculatedType getForcedPrediction()
    {
        ASSERT(op() == IdentityWithProfile);
        return m_opInfo.as<SpeculatedType>();
    }

    uint32_t catchOSREntryIndex() const
    {
        ASSERT(op() == ExtractCatchLocal);
        return m_opInfo.as<uint32_t>();
    }

    SpeculatedType catchLocalPrediction()
    {
        ASSERT(op() == ExtractCatchLocal);
        return m_opInfo2.as<SpeculatedType>();
    }
    
    bool hasCellOperand()
    {
        switch (op()) {
        case CheckIsConstant:
            return isCell(child1().useKind());
        case OverridesHasInstance:
        case NewFunction:
        case NewGeneratorFunction:
        case NewAsyncFunction:
        case NewAsyncGeneratorFunction:
        case CreateActivation:
        case MaterializeCreateActivation:
        case NewRegexp:
        case NewArrayBuffer:
        case PhantomNewArrayBuffer:
        case CompareEqPtr:
        case CallObjectConstructor:
        case DirectCall:
        case DirectTailCall:
        case DirectConstruct:
        case DirectTailCallInlinedCaller:
        case RegExpExecNonGlobalOrSticky:
        case RegExpMatchFastGlobal:
            return true;
        default:
            return false;
        }
    }

    FrozenValue* cellOperand()
    {
        ASSERT(hasCellOperand());
        return m_opInfo.as<FrozenValue*>();
    }
    
    template<typename T>
    T castOperand()
    {
        return cellOperand()->cast<T>();
    }
    
    void setCellOperand(FrozenValue* value)
    {
        ASSERT(hasCellOperand());
        m_opInfo = value;
    }
    
    bool hasWatchpointSet()
    {
        return op() == NotifyWrite;
    }
    
    WatchpointSet* watchpointSet()
    {
        ASSERT(hasWatchpointSet());
        return m_opInfo.as<WatchpointSet*>();
    }
    
    bool hasStoragePointer()
    {
        return op() == ConstantStoragePointer;
    }
    
    void* storagePointer()
    {
        ASSERT(hasStoragePointer());
        return m_opInfo.as<void*>();
    }

    bool hasUidOperand()
    {
        return op() == CheckIdent;
    }

    UniquedStringImpl* uidOperand()
    {
        ASSERT(hasUidOperand());
        return m_opInfo.as<UniquedStringImpl*>();
    }

    bool hasTypeInfoOperand()
    {
        return op() == CheckTypeInfoFlags;
    }

    unsigned typeInfoOperand()
    {
        ASSERT(hasTypeInfoOperand() && m_opInfo.as<uint32_t>() <= static_cast<uint32_t>(UCHAR_MAX));
        return m_opInfo.as<uint32_t>();
    }

    bool hasTransition()
    {
        switch (op()) {
        case PutStructure:
        case AllocatePropertyStorage:
        case ReallocatePropertyStorage:
            return true;
        default:
            return false;
        }
    }
    
    Transition* transition()
    {
        ASSERT(hasTransition());
        return m_opInfo.as<Transition*>();
    }
    
    bool hasStructureSet()
    {
        switch (op()) {
        case CheckStructure:
        case CheckStructureOrEmpty:
        case CheckStructureImmediate:
        case MaterializeNewObject:
            return true;
        default:
            return false;
        }
    }
    
    const RegisteredStructureSet& structureSet()
    {
        ASSERT(hasStructureSet());
        return *m_opInfo.as<RegisteredStructureSet*>();
    }
    
    bool hasStructure()
    {
        switch (op()) {
        case ArrayifyToStructure:
        case MaterializeNewInternalFieldObject:
        case NewObject:
        case NewGenerator:
        case NewAsyncGenerator:
        case NewInternalFieldObject:
        case NewStringObject:
            return true;
        default:
            return false;
        }
    }
    
    RegisteredStructure structure()
    {
        ASSERT(hasStructure());
        return m_opInfo.asRegisteredStructure();
    }
    
    bool hasStorageAccessData()
    {
        switch (op()) {
        case GetByOffset:
        case PutByOffset:
        case GetGetterSetterByOffset:
            return true;
        default:
            return false;
        }
    }
    
    StorageAccessData& storageAccessData()
    {
        ASSERT(hasStorageAccessData());
        return *m_opInfo.as<StorageAccessData*>();
    }
    
    bool hasMultiGetByOffsetData()
    {
        return op() == MultiGetByOffset;
    }
    
    MultiGetByOffsetData& multiGetByOffsetData()
    {
        ASSERT(hasMultiGetByOffsetData());
        return *m_opInfo.as<MultiGetByOffsetData*>();
    }
    
    bool hasMultiPutByOffsetData()
    {
        return op() == MultiPutByOffset;
    }
    
    MultiPutByOffsetData& multiPutByOffsetData()
    {
        ASSERT(hasMultiPutByOffsetData());
        return *m_opInfo.as<MultiPutByOffsetData*>();
    }

    bool hasMultiDeleteByOffsetData()
    {
        return op() == MultiDeleteByOffset;
    }

    MultiDeleteByOffsetData& multiDeleteByOffsetData()
    {
        ASSERT(hasMultiDeleteByOffsetData());
        return *m_opInfo.as<MultiDeleteByOffsetData*>();
    }
    
    bool hasMatchStructureData()
    {
        return op() == MatchStructure;
    }
    
    MatchStructureData& matchStructureData()
    {
        ASSERT(hasMatchStructureData());
        return *m_opInfo.as<MatchStructureData*>();
    }
    
    bool hasObjectMaterializationData()
    {
        switch (op()) {
        case MaterializeNewObject:
        case MaterializeNewInternalFieldObject:
        case MaterializeCreateActivation:
            return true;

        default:
            return false;
        }
    }
    
    ObjectMaterializationData& objectMaterializationData()
    {
        ASSERT(hasObjectMaterializationData());
        return *m_opInfo2.as<ObjectMaterializationData*>();
    }

    bool isObjectAllocation()
    {
        switch (op()) {
        case NewObject:
        case MaterializeNewObject:
            return true;
        default:
            return false;
        }
    }
    
    bool isPhantomObjectAllocation()
    {
        switch (op()) {
        case PhantomNewObject:
            return true;
        default:
            return false;
        }
    }
    
    bool isActivationAllocation()
    {
        switch (op()) {
        case CreateActivation:
        case MaterializeCreateActivation:
            return true;
        default:
            return false;
        }
    }

    bool isPhantomActivationAllocation()
    {
        switch (op()) {
        case PhantomCreateActivation:
            return true;
        default:
            return false;
        }
    }

    bool isFunctionAllocation()
    {
        switch (op()) {
        case NewFunction:
        case NewGeneratorFunction:
        case NewAsyncGeneratorFunction:
        case NewAsyncFunction:
            return true;
        default:
            return false;
        }
    }

    bool isPhantomFunctionAllocation()
    {
        switch (op()) {
        case PhantomNewFunction:
        case PhantomNewGeneratorFunction:
        case PhantomNewAsyncFunction:
        case PhantomNewAsyncGeneratorFunction:
            return true;
        default:
            return false;
        }
    }

    bool isPhantomAllocation()
    {
        switch (op()) {
        case PhantomNewObject:
        case PhantomDirectArguments:
        case PhantomCreateRest:
        case PhantomSpread:
        case PhantomNewArrayWithSpread:
        case PhantomNewArrayBuffer:
        case PhantomClonedArguments:
        case PhantomNewFunction:
        case PhantomNewGeneratorFunction:
        case PhantomNewAsyncFunction:
        case PhantomNewAsyncGeneratorFunction:
        case PhantomNewInternalFieldObject:
        case PhantomCreateActivation:
        case PhantomNewRegexp:
            return true;
        default:
            return false;
        }
    }
    
    bool hasArrayMode()
    {
        switch (op()) {
        case GetIndexedPropertyStorage:
        case GetArrayLength:
        case GetVectorLength:
        case InByVal:
        case PutByValDirect:
        case PutByVal:
        case PutByValAlias:
        case GetByVal:
        case StringCharAt:
        case StringCharCodeAt:
        case StringCodePointAt:
        case CheckArray:
        case CheckArrayOrEmpty:
        case Arrayify:
        case ArrayifyToStructure:
        case ArrayPush:
        case ArrayPop:
        case ArrayIndexOf:
        case HasIndexedProperty:
        case HasEnumerableIndexedProperty:
        case AtomicsAdd:
        case AtomicsAnd:
        case AtomicsCompareExchange:
        case AtomicsExchange:
        case AtomicsLoad:
        case AtomicsOr:
        case AtomicsStore:
        case AtomicsSub:
        case AtomicsXor:
            return true;
        default:
            return false;
        }
    }
    
    ArrayMode arrayMode()
    {
        ASSERT(hasArrayMode());
        if (op() == ArrayifyToStructure)
            return ArrayMode::fromWord(m_opInfo2.as<uint32_t>());
        return ArrayMode::fromWord(m_opInfo.as<uint32_t>());
    }
    
    bool setArrayMode(ArrayMode arrayMode)
    {
        ASSERT(hasArrayMode());
        if (this->arrayMode() == arrayMode)
            return false;
        m_opInfo = arrayMode.asWord();
        return true;
    }

    bool hasECMAMode()
    {
        switch (op()) {
        case CallEval:
        case DeleteById:
        case DeleteByVal:
        case PutById:
        case PutByIdDirect:
        case PutByIdFlush:
        case PutByIdWithThis:
        case PutByVal:
        case PutByValAlias:
        case PutByValDirect:
        case PutByValWithThis:
        case PutDynamicVar:
        case ToThis:
            return true;
        default:
            return false;
        }
    }

    ECMAMode ecmaMode()
    {
        ASSERT(hasECMAMode());
        switch (op()) {
        case CallEval:
        case DeleteByVal:
        case PutByValWithThis:
        case ToThis:
            return ECMAMode::fromByte(m_opInfo.as<uint8_t>());
        case DeleteById:
        case PutById:
        case PutByIdDirect:
        case PutByIdFlush:
        case PutByIdWithThis:
        case PutByVal:
        case PutByValAlias:
        case PutByValDirect:
        case PutDynamicVar:
            return ECMAMode::fromByte(m_opInfo2.as<uint8_t>());
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return ECMAMode::strict();
        }
    }
    
    bool hasPrivateFieldPutKind()
    {
        if (op() == PutPrivateName || op() == PutPrivateNameById)
            return true;
        return false;
    }

    PrivateFieldPutKind privateFieldPutKind()
    {
        ASSERT(hasPrivateFieldPutKind());
        return PrivateFieldPutKind::fromByte(m_opInfo2.as<uint8_t>());
    }

    bool hasArithMode()
    {
        switch (op()) {
        case ArithAbs:
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
        return static_cast<Arith::Mode>(m_opInfo.as<uint32_t>());
    }
    
    void setArithMode(Arith::Mode mode)
    {
        m_opInfo = mode;
    }

    bool hasArithRoundingMode()
    {
        return op() == ArithRound || op() == ArithFloor || op() == ArithCeil || op() == ArithTrunc;
    }

    Arith::RoundingMode arithRoundingMode()
    {
        ASSERT(hasArithRoundingMode());
        return static_cast<Arith::RoundingMode>(m_opInfo.as<uint32_t>());
    }

    void setArithRoundingMode(Arith::RoundingMode mode)
    {
        ASSERT(hasArithRoundingMode());
        m_opInfo = static_cast<uint32_t>(mode);
    }

    bool hasArithUnaryType()
    {
        return op() == ArithUnary;
    }

    Arith::UnaryType arithUnaryType()
    {
        ASSERT(hasArithUnaryType());
        return static_cast<Arith::UnaryType>(m_opInfo.as<uint32_t>());
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
        return m_opInfo.as<Profiler::ExecutionCounter*>();
    }

    unsigned entrypointIndex()
    {
        ASSERT(op() == InitializeEntrypointArguments);
        return m_opInfo.as<unsigned>();
    }

    DataViewData dataViewData()
    {
        ASSERT(op() == DataViewGetInt || op() == DataViewGetFloat || op() == DataViewSet);
        return bitwise_cast<DataViewData>(m_opInfo.as<uint64_t>());
    }

    bool shouldGenerate()
    {
        return m_refCount;
    }
    
    // Return true if the execution of this Node does not affect our ability to OSR to the FTL.
    // FIXME: Isn't this just like checking if the node has effects?
    bool isSemanticallySkippable()
    {
        return op() == CountExecution || op() == InvalidationPoint;
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
    
    bool isBinaryUseKind(UseKind left, UseKind right)
    {
        return child1().useKind() == left && child2().useKind() == right;
    }
    
    bool isBinaryUseKind(UseKind useKind)
    {
        return isBinaryUseKind(useKind, useKind);
    }
    
    Edge childFor(UseKind useKind)
    {
        if (child1().useKind() == useKind)
            return child1();
        if (child2().useKind() == useKind)
            return child2();
        if (child3().useKind() == useKind)
            return child3();
        return Edge();
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

    bool shouldSpeculateNotInt32()
    {
        return isNotInt32Speculation(prediction());
    }
    
    bool sawBooleans()
    {
        return !!(prediction() & SpecBoolean);
    }
    
    bool shouldSpeculateInt32OrBoolean()
    {
        return isInt32OrBooleanSpeculation(prediction());
    }
    
    bool shouldSpeculateInt32ForArithmetic()
    {
        return isInt32SpeculationForArithmetic(prediction());
    }
    
    bool shouldSpeculateInt32OrBooleanForArithmetic()
    {
        return isInt32OrBooleanSpeculationForArithmetic(prediction());
    }
    
    bool shouldSpeculateInt32OrBooleanExpectingDefined()
    {
        return isInt32OrBooleanSpeculationExpectingDefined(prediction());
    }
    
    bool shouldSpeculateInt52()
    {
        // We have to include SpecInt32Only here for two reasons:
        // 1. We diligently write code that first checks if we should speculate Int32.
        // For example:
        // if (shouldSpeculateInt32()) ... 
        // else if (shouldSpeculateInt52()) ...
        // This means we it's totally valid to speculate Int52 when we're dealing
        // with a type that's the union of Int32 and Int52.
        //
        // It would be a performance mistake to not include Int32 here because we obviously
        // have variables that are the union of Int32 and Int52 values, and it's better
        // to speculate Int52 than double in that situation.
        //
        // 2. We also write code where we ask if the inputs can be Int52, like if
        // we know via profiling that an Add overflows, we may not emit an Int32 add.
        // However, we only emit such an add if both inputs can be Int52, and Int32
        // can trivially become Int52.
        //
        return enableInt52() && isInt32OrInt52Speculation(prediction());
    }
    
    bool shouldSpeculateDouble()
    {
        return isDoubleSpeculation(prediction());
    }
    
    bool shouldSpeculateDoubleReal()
    {
        return isDoubleRealSpeculation(prediction());
    }
    
    bool shouldSpeculateNumber()
    {
        return isFullNumberSpeculation(prediction());
    }
    
    bool shouldSpeculateNumberOrBoolean()
    {
        return isFullNumberOrBooleanSpeculation(prediction());
    }
    
    bool shouldSpeculateNumberOrBooleanExpectingDefined()
    {
        return isFullNumberOrBooleanSpeculationExpectingDefined(prediction());
    }
    
    bool shouldSpeculateBoolean()
    {
        return isBooleanSpeculation(prediction());
    }

    bool shouldSpeculateNotBoolean()
    {
        return isNotBooleanSpeculation(prediction());
    }
    
    bool shouldSpeculateOther()
    {
        return isOtherSpeculation(prediction());
    }
    
    bool shouldSpeculateMisc()
    {
        return isMiscSpeculation(prediction());
    }
   
    bool shouldSpeculateStringIdent()
    {
        return isStringIdentSpeculation(prediction());
    }
    
    bool shouldSpeculateNotStringVar()
    {
        return isNotStringVarSpeculation(prediction());
    }
 
    bool shouldSpeculateString()
    {
        return isStringSpeculation(prediction());
    }

    bool shouldSpeculateNotString()
    {
        return isNotStringSpeculation(prediction());
    }
 
    bool shouldSpeculateStringOrOther()
    {
        return isStringOrOtherSpeculation(prediction());
    }
 
    bool shouldSpeculateStringObject()
    {
        return isStringObjectSpeculation(prediction());
    }
    
    bool shouldSpeculateStringOrStringObject()
    {
        return isStringOrStringObjectSpeculation(prediction());
    }

    bool shouldSpeculateRegExpObject()
    {
        return isRegExpObjectSpeculation(prediction());
    }
    
    bool shouldSpeculateSymbol()
    {
        return isSymbolSpeculation(prediction());
    }

#if USE(BIGINT32)
    bool shouldSpeculateBigInt32()
    {
        return isBigInt32Speculation(prediction());
    }
#endif

    bool shouldSpeculateHeapBigInt()
    {
        return isHeapBigIntSpeculation(prediction());
    }
    
    bool shouldSpeculateBigInt()
    {
        return isBigIntSpeculation(prediction());
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

    bool shouldSpeculateFunction()
    {
        return isFunctionSpeculation(prediction());
    }

    bool shouldSpeculateProxyObject()
    {
        return isProxyObjectSpeculation(prediction());
    }

    bool shouldSpeculateDerivedArray()
    {
        return isDerivedArraySpeculation(prediction());
    }
    
    bool shouldSpeculateDirectArguments()
    {
        return isDirectArgumentsSpeculation(prediction());
    }
    
    bool shouldSpeculateScopedArguments()
    {
        return isScopedArgumentsSpeculation(prediction());
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
    
    bool shouldSpeculateCellOrOther()
    {
        return isCellOrOtherSpeculation(prediction());
    }
    
    bool shouldSpeculateNotCell()
    {
        return isNotCellSpeculation(prediction());
    }

    bool shouldSpeculateNotCellNorBigInt()
    {
        return isNotCellNorBigIntSpeculation(prediction());
    }
    
    bool shouldSpeculateUntypedForArithmetic()
    {
        return isUntypedSpeculationForArithmetic(prediction());
    }

    static bool shouldSpeculateUntypedForArithmetic(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateUntypedForArithmetic() || op2->shouldSpeculateUntypedForArithmetic();
    }
    
    bool shouldSpeculateUntypedForBitOps()
    {
        return isUntypedSpeculationForBitOps(prediction());
    }
    
    static bool shouldSpeculateUntypedForBitOps(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateUntypedForBitOps() || op2->shouldSpeculateUntypedForBitOps();
    }
    
    static bool shouldSpeculateBoolean(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateBoolean() && op2->shouldSpeculateBoolean();
    }
    
    static bool shouldSpeculateInt32(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateInt32() && op2->shouldSpeculateInt32();
    }
    
    static bool shouldSpeculateInt32OrBoolean(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateInt32OrBoolean()
            && op2->shouldSpeculateInt32OrBoolean();
    }
    
    static bool shouldSpeculateInt32OrBooleanForArithmetic(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateInt32OrBooleanForArithmetic()
            && op2->shouldSpeculateInt32OrBooleanForArithmetic();
    }
    
    static bool shouldSpeculateInt32OrBooleanExpectingDefined(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateInt32OrBooleanExpectingDefined()
            && op2->shouldSpeculateInt32OrBooleanExpectingDefined();
    }
    
    static bool shouldSpeculateInt52(Node* op1, Node* op2)
    {
        return enableInt52() && op1->shouldSpeculateInt52() && op2->shouldSpeculateInt52();
    }
    
    static bool shouldSpeculateNumber(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateNumber() && op2->shouldSpeculateNumber();
    }
    
    static bool shouldSpeculateNumberOrBoolean(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateNumberOrBoolean()
            && op2->shouldSpeculateNumberOrBoolean();
    }
    
    static bool shouldSpeculateNumberOrBooleanExpectingDefined(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateNumberOrBooleanExpectingDefined()
            && op2->shouldSpeculateNumberOrBooleanExpectingDefined();
    }

    static bool shouldSpeculateSymbol(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateSymbol() && op2->shouldSpeculateSymbol();
    }
    
    static bool shouldSpeculateBigInt(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateBigInt() && op2->shouldSpeculateBigInt();
    }

#if USE(BIGINT32)
    static bool shouldSpeculateBigInt32(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateBigInt32() && op2->shouldSpeculateBigInt32();
    }
#endif

    static bool shouldSpeculateHeapBigInt(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateHeapBigInt() && op2->shouldSpeculateHeapBigInt();
    }

    static bool shouldSpeculateFinalObject(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateFinalObject() && op2->shouldSpeculateFinalObject();
    }

    static bool shouldSpeculateArray(Node* op1, Node* op2)
    {
        return op1->shouldSpeculateArray() && op2->shouldSpeculateArray();
    }
    
    bool canSpeculateInt32(RareCaseProfilingSource source)
    {
        return nodeCanSpeculateInt32(arithNodeFlags(), source);
    }
    
    bool canSpeculateInt52(RareCaseProfilingSource source)
    {
        return nodeCanSpeculateInt52(arithNodeFlags(), source);
    }

    bool canSpeculateBigInt32(RareCaseProfilingSource source)
    {
        return nodeCanSpeculateBigInt32(arithNodeFlags(), source);
    }
    
    RareCaseProfilingSource sourceFor(PredictionPass pass)
    {
        if (pass == PrimaryPass || child1()->sawBooleans() || (child2() && child2()->sawBooleans()))
            return DFGRareCase;
        return AllRareCases;
    }
    
    bool canSpeculateInt32(PredictionPass pass)
    {
        return canSpeculateInt32(sourceFor(pass));
    }
    
    bool canSpeculateInt52(PredictionPass pass)
    {
        return canSpeculateInt52(sourceFor(pass));
    }

    bool canSpeculateBigInt32(PredictionPass pass)
    {
        return canSpeculateBigInt32(sourceFor(pass));
    }

    bool hasTypeLocation()
    {
        return op() == ProfileType;
    }

    TypeLocation* typeLocation()
    {
        ASSERT(hasTypeLocation());
        return m_opInfo.as<TypeLocation*>();
    }

    bool hasBasicBlockLocation()
    {
        return op() == ProfileControlFlow;
    }

    BasicBlockLocation* basicBlockLocation()
    {
        ASSERT(hasBasicBlockLocation());
        return m_opInfo.as<BasicBlockLocation*>();
    }

    bool hasCallDOMGetterData() const
    {
        return op() == CallDOMGetter;
    }

    CallDOMGetterData* callDOMGetterData()
    {
        ASSERT(hasCallDOMGetterData());
        return m_opInfo.as<CallDOMGetterData*>();
    }

    bool hasClassInfo() const
    {
        return op() == CheckJSCast || op() == CheckNotJSCast;
    }

    const ClassInfo* classInfo()
    {
        ASSERT(hasClassInfo());
        return m_opInfo.as<const ClassInfo*>();
    }

    bool hasSignature() const
    {
        // Note that this does not include TailCall node types intentionally.
        // CallDOM node types are always converted from Call.
        return op() == Call || op() == CallDOM;
    }

    const DOMJIT::Signature* signature()
    {
        return m_opInfo.as<const DOMJIT::Signature*>();
    }

    const ClassInfo* requiredDOMJITClassInfo()
    {
        switch (op()) {
        case CallDOMGetter:
            return callDOMGetterData()->requiredClassInfo;
        case CallDOM:
            return signature()->classInfo;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        return nullptr;
    }

    Node* replacement() const
    {
        return m_misc.replacement;
    }
    
    void setReplacement(Node* replacement)
    {
        m_misc.replacement = replacement;
    }
    
    Epoch epoch() const
    {
        return Epoch::fromUnsigned(m_misc.epoch);
    }
    
    void setEpoch(Epoch epoch)
    {
        m_misc.epoch = epoch.toUnsigned();
    }
    
    bool hasNumberOfArgumentsToSkip()
    {
        return op() == CreateRest || op() == PhantomCreateRest || op() == GetRestLength || op() == GetMyArgumentByVal || op() == GetMyArgumentByValOutOfBounds;
    }

    unsigned numberOfArgumentsToSkip()
    {
        ASSERT(hasNumberOfArgumentsToSkip());
        return m_opInfo.as<unsigned>();
    }

    bool hasArgumentIndex()
    {
        return op() == GetArgument;
    }

    unsigned argumentIndex()
    {
        ASSERT(hasArgumentIndex());
        return m_opInfo.as<unsigned>();
    }

    bool hasBucketOwnerType()
    {
        return op() == GetMapBucketNext || op() == LoadKeyFromMapBucket || op() == LoadValueFromMapBucket;
    }

    BucketOwnerType bucketOwnerType()
    {
        ASSERT(hasBucketOwnerType());
        return m_opInfo.as<BucketOwnerType>();
    }

    bool hasValidRadixConstant()
    {
        return op() == NumberToStringWithValidRadixConstant;
    }

    int32_t validRadixConstant()
    {
        ASSERT(hasValidRadixConstant());
        return m_opInfo.as<int32_t>();
    }

    bool hasIgnoreLastIndexIsWritable()
    {
        return op() == SetRegExpObjectLastIndex;
    }

    bool ignoreLastIndexIsWritable()
    {
        ASSERT(hasIgnoreLastIndexIsWritable());
        return m_opInfo.as<uint32_t>();
    }

    uint32_t errorType()
    {
        ASSERT(op() == ThrowStaticError);
        return m_opInfo.as<uint32_t>();
    }
    
    bool hasCallLinkStatus()
    {
        return op() == FilterCallLinkStatus;
    }
    
    CallLinkStatus* callLinkStatus()
    {
        ASSERT(hasCallLinkStatus());
        return m_opInfo.as<CallLinkStatus*>();
    }
    
    bool hasGetByStatus()
    {
        return op() == FilterGetByStatus;
    }
    
    GetByStatus* getByStatus()
    {
        ASSERT(hasGetByStatus());
        return m_opInfo.as<GetByStatus*>();
    }
    
    bool hasInByIdStatus()
    {
        return op() == FilterInByIdStatus;
    }
    
    InByIdStatus* inByIdStatus()
    {
        ASSERT(hasInByIdStatus());
        return m_opInfo.as<InByIdStatus*>();
    }
    
    bool hasPutByIdStatus()
    {
        return op() == FilterPutByIdStatus;
    }
    
    PutByIdStatus* putByIdStatus()
    {
        ASSERT(hasPutByIdStatus());
        return m_opInfo.as<PutByIdStatus*>();
    }

    bool hasDeleteByStatus()
    {
        return op() == FilterDeleteByStatus;
    }

    DeleteByStatus* deleteByStatus()
    {
        ASSERT(hasDeleteByStatus());
        return m_opInfo.as<DeleteByStatus*>();
    }

    bool hasCheckPrivateBrandStatus()
    {
        return op() == FilterCheckPrivateBrandStatus;
    }

    CheckPrivateBrandStatus* checkPrivateBrandStatus()
    {
        ASSERT(hasCheckPrivateBrandStatus());
        return m_opInfo.as<CheckPrivateBrandStatus*>();
    }

    bool hasSetPrivateBrandStatus()
    {
        return op() == FilterSetPrivateBrandStatus;
    }

    SetPrivateBrandStatus* setPrivateBrandStatus()
    {
        ASSERT(hasSetPrivateBrandStatus());
        return m_opInfo.as<SetPrivateBrandStatus*>();
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

    NodeOrigin origin;

    // References to up to 3 children, or links to a variable length set of children.
    AdjacencyList children;

private:
    friend class B3::SparseCollection<Node>;

    unsigned m_index { std::numeric_limits<unsigned>::max() };
    unsigned m_op : 10; // real type is NodeType
    unsigned m_flags : 21;
    // The virtual register number (spill location) associated with this .
    VirtualRegister m_virtualRegister;
    // The number of uses of the result of this operation (+1 for 'must generate' nodes, which have side-effects).
    unsigned m_refCount;
    // The prediction ascribed to this node after propagation.
    SpeculatedType m_prediction { SpecNone };
    // Immediate values, accesses type-checked via accessors above.
    struct OpInfoWrapper {
        OpInfoWrapper()
        {
            u.int64 = 0;
        }
        OpInfoWrapper(uint32_t intValue)
        {
            u.int64 = 0;
            u.int32 = intValue;
        }
        OpInfoWrapper(uint64_t intValue)
        {
            u.int64 = intValue;
        }
        OpInfoWrapper(void* pointer)
        {
            u.int64 = 0;
            u.pointer = pointer;
        }
        OpInfoWrapper(const void* constPointer)
        {
            u.int64 = 0;
            u.constPointer = constPointer;
        }
        OpInfoWrapper(RegisteredStructure structure)
        {
            u.int64 = 0;
            u.pointer = bitwise_cast<void*>(structure);
        }
        OpInfoWrapper(CacheableIdentifier identifier)
        {
            u.int64 = 0;
            u.pointer = bitwise_cast<void*>(identifier.rawBits());
        }
        OpInfoWrapper& operator=(uint32_t int32)
        {
            u.int64 = 0;
            u.int32 = int32;
            return *this;
        }
        OpInfoWrapper& operator=(int32_t int32)
        {
            u.int64 = 0;
            u.int32 = int32;
            return *this;
        }
        OpInfoWrapper& operator=(uint64_t int64)
        {
            u.int64 = int64;
            return *this;
        }
        OpInfoWrapper& operator=(void* pointer)
        {
            u.int64 = 0;
            u.pointer = pointer;
            return *this;
        }
        OpInfoWrapper& operator=(const void* constPointer)
        {
            u.int64 = 0;
            u.constPointer = constPointer;
            return *this;
        }
        OpInfoWrapper& operator=(RegisteredStructure structure)
        {
            u.int64 = 0;
            u.pointer = bitwise_cast<void*>(structure);
            return *this;
        }
        OpInfoWrapper& operator=(CacheableIdentifier identifier)
        {
            u.int64 = 0;
            u.pointer = bitwise_cast<void*>(identifier.rawBits());
            return *this;
        }
        OpInfoWrapper& operator=(NewArrayBufferData newArrayBufferData)
        {
            u.int64 = bitwise_cast<uint64_t>(newArrayBufferData);
            return *this;
        }
        template <typename T>
        ALWAYS_INLINE auto as() const -> typename std::enable_if<std::is_pointer<T>::value && !std::is_const<typename std::remove_pointer<T>::type>::value, T>::type
        {
            return static_cast<T>(u.pointer);
        }
        template <typename T>
        ALWAYS_INLINE auto as() const -> typename std::enable_if<std::is_pointer<T>::value && std::is_const<typename std::remove_pointer<T>::type>::value, T>::type
        {
            return static_cast<T>(u.constPointer);
        }
        template <typename T>
        ALWAYS_INLINE auto as() const -> typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value) && sizeof(T) <= 4, T>::type
        {
            return static_cast<T>(u.int32);
        }
        template <typename T>
        ALWAYS_INLINE auto as() const -> typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value) && sizeof(T) == 8, T>::type
        {
            return static_cast<T>(u.int64);
        }
        ALWAYS_INLINE RegisteredStructure asRegisteredStructure() const
        {
            return bitwise_cast<RegisteredStructure>(u.pointer);
        }
        ALWAYS_INLINE NewArrayBufferData asNewArrayBufferData() const
        {
            return bitwise_cast<NewArrayBufferData>(u.int64);
        }

        union {
            uint32_t int32;
            uint64_t int64;
            void* pointer;
            const void* constPointer;
        } u;
    };
    OpInfoWrapper m_opInfo;
    OpInfoWrapper m_opInfo2;

    // Miscellaneous data that is usually meaningless, but can hold some analysis results
    // if you ask right. For example, if you do Graph::initializeNodeOwners(), Node::owner
    // will tell you which basic block a node belongs to. You cannot rely on this persisting
    // across transformations unless you do the maintenance work yourself. Other phases use
    // Node::replacement, but they do so manually: first you do Graph::clearReplacements()
    // and then you set, and use, replacement's yourself. Same thing for epoch.
    //
    // Bottom line: don't use these fields unless you initialize them yourself, or by
    // calling some appropriate methods that initialize them the way you want. Otherwise,
    // these fields are meaningless.
private:
    union {
        Node* replacement;
        unsigned epoch;
    } m_misc;
public:
    BasicBlock* owner;
};

// Uncomment this to log NodeSet operations.
// typedef LoggingHashSet<Node::HashSetTemplateInstantiationString, Node*> NodeSet;
typedef HashSet<Node*> NodeSet;

struct NodeComparator {
    template<typename NodePtrType>
    bool operator()(NodePtrType a, NodePtrType b) const
    {
        return a->index() < b->index();
    }
};

template<typename T>
CString nodeListDump(const T& nodeList)
{
    return sortedListDump(nodeList, NodeComparator());
}

template<typename T>
CString nodeMapDump(const T& nodeMap, DumpContext* context = nullptr)
{
    Vector<typename T::KeyType> keys;
    for (
        typename T::const_iterator iter = nodeMap.begin();
        iter != nodeMap.end(); ++iter)
        keys.append(iter->key);
    std::sort(keys.begin(), keys.end(), NodeComparator());
    StringPrintStream out;
    CommaPrinter comma;
    for(unsigned i = 0; i < keys.size(); ++i)
        out.print(comma, keys[i], "=>", inContext(nodeMap.get(keys[i]), context));
    return out.toCString();
}

template<typename T>
CString nodeValuePairListDump(const T& nodeValuePairList, DumpContext* context = nullptr)
{
    using V = typename T::ValueType;
    T sortedList = nodeValuePairList;
    std::sort(sortedList.begin(), sortedList.end(), [](const V& a, const V& b) {
        return NodeComparator()(a.node, b.node);
    });

    StringPrintStream out;
    CommaPrinter comma;
    for (const auto& pair : sortedList)
        out.print(comma, pair.node, "=>", inContext(pair.value, context));
    return out.toCString();
}

} } // namespace JSC::DFG

namespace WTF {

void printInternal(PrintStream&, JSC::DFG::SwitchKind);
void printInternal(PrintStream&, JSC::DFG::Node*);

inline JSC::DFG::Node* inContext(JSC::DFG::Node* node, JSC::DumpContext*) { return node; }

template<>
struct LoggingHashKeyTraits<JSC::DFG::Node*> {
    static void print(PrintStream& out, JSC::DFG::Node* key)
    {
        out.print("bitwise_cast<::JSC::DFG::Node*>(", RawPointer(key), "lu)");
    }
};

} // namespace WTF

using WTF::inContext;

#endif
