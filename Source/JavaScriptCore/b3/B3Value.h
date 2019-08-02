/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3Bank.h"
#include "B3Effects.h"
#include "B3FrequentedBlock.h"
#include "B3Kind.h"
#include "B3Origin.h"
#include "B3SparseCollection.h"
#include "B3Type.h"
#include "B3ValueKey.h"
#include "B3Width.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/IteratorRange.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TriState.h>

namespace JSC { namespace B3 {

class BasicBlock;
class CheckValue;
class InsertionSet;
class PhiChildren;
class Procedure;

class JS_EXPORT_PRIVATE Value {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static const char* const dumpPrefix;

    static bool accepts(Kind) { return true; }

    virtual ~Value();

    unsigned index() const { return m_index; }
    
    // Note that the kind is immutable, except for replacing values with:
    // Identity, Nop, Oops, Jump, and Phi. See below for replaceWithXXX() methods.
    Kind kind() const { return m_kind; }
    
    Opcode opcode() const { return kind().opcode(); }
    
    // Note that the kind is meant to be immutable. Do this when you know that this is safe. It's not
    // usually safe.
    void setKindUnsafely(Kind kind) { m_kind = kind; }
    void setOpcodeUnsafely(Opcode opcode) { m_kind.setOpcode(opcode); }
    
    // It's good practice to mirror Kind methods here, so you can say value->isBlah()
    // instead of value->kind().isBlah().
    bool isChill() const { return kind().isChill(); }
    bool traps() const { return kind().traps(); }

    Origin origin() const { return m_origin; }
    void setOrigin(Origin origin) { m_origin = origin; }
    
    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }

    // This is useful when lowering. Note that this is only valid for non-void values.
    Bank resultBank() const { return bankForType(type()); }
    Width resultWidth() const { return widthForType(type()); }

    unsigned numChildren() const
    {
        if (m_numChildren == VarArgs)
            return childrenVector().size();
        return m_numChildren;
    }
    
    Value*& child(unsigned index)
    {
        ASSERT(index < numChildren());
        return m_numChildren == VarArgs ? childrenVector()[index] : childrenArray()[index];
    }
    Value* child(unsigned index) const
    {
        ASSERT(index < numChildren());
        return m_numChildren == VarArgs ? childrenVector()[index] : childrenArray()[index];
    }
    
    Value*& lastChild()
    {
        if (m_numChildren == VarArgs)
            return childrenVector().last();
        ASSERT(m_numChildren >= 1);
        return childrenArray()[m_numChildren - 1];
    }
    Value* lastChild() const
    {
        if (m_numChildren == VarArgs)
            return childrenVector().last();
        ASSERT(m_numChildren >= 1);
        return childrenArray()[m_numChildren - 1];
    }

    WTF::IteratorRange<Value**> children()
    {
        if (m_numChildren == VarArgs) {
            Vector<Value*, 3>& vec = childrenVector();
            return WTF::makeIteratorRange(&*vec.begin(), &*vec.end());
        }
        Value** buffer = childrenArray();
        return {buffer, buffer + m_numChildren };
    }
    WTF::IteratorRange<Value* const*> children() const
    {
        if (m_numChildren == VarArgs) {
            const Vector<Value*, 3>& vec = childrenVector();
            return WTF::makeIteratorRange(&*vec.begin(), &*vec.end());
        }
        Value* const* buffer = childrenArray();
        return {buffer, buffer + m_numChildren };
    }

    // If you want to replace all uses of this value with a different value, then replace this
    // value with Identity. Then do a pass of performSubstitution() on all of the values that use
    // this one. Usually we do all of this in one pass in pre-order, which ensures that the
    // X->replaceWithIdentity() calls happen before the performSubstitution() calls on X's users.
    void replaceWithIdentity(Value*);
    
    // It's often necessary to kill a value. It's tempting to replace the value with Nop or to
    // just remove it. But unless you are sure that the value is Void, you will probably still
    // have other values that use this one. Sure, you may kill those later, or you might not. This
    // method lets you kill a value safely. It will replace Void values with Nop and non-Void
    // values with Identities on bottom constants. For this reason, this takes a callback that is
    // responsible for creating bottoms. There's a utility for this, see B3BottomProvider.h. You
    // can also access that utility using replaceWithBottom(InsertionSet&, size_t).
    //
    // You're guaranteed that bottom is zero.
    template<typename BottomProvider>
    void replaceWithBottom(const BottomProvider&);
    
    void replaceWithBottom(InsertionSet&, size_t index);

    // Use this if you want to kill a value and you are sure that the value is Void.
    void replaceWithNop();
    
    // Use this if you want to kill a value and you are sure that nobody is using it anymore.
    void replaceWithNopIgnoringType();
    
    void replaceWithPhi();
    
    // These transformations are only valid for terminals.
    void replaceWithJump(BasicBlock* owner, FrequentedBlock);
    void replaceWithOops(BasicBlock* owner);
    
    // You can use this form if owners are valid. They're usually not valid.
    void replaceWithJump(FrequentedBlock);
    void replaceWithOops();

    void dump(PrintStream&) const;
    void deepDump(const Procedure*, PrintStream&) const;
    
    virtual void dumpSuccessors(const BasicBlock*, PrintStream&) const;

    // This is how you cast Values. For example, if you want to do something provided that we have a
    // ArgumentRegValue, you can do:
    //
    // if (ArgumentRegValue* argumentReg = value->as<ArgumentRegValue>()) {
    //     things
    // }
    //
    // This will return null if this kind() != ArgumentReg. This works because this returns nullptr
    // if T::accepts(kind()) returns false.
    template<typename T>
    T* as();
    template<typename T>
    const T* as() const;

    // What follows are a bunch of helpers for inspecting and modifying values. Note that we have a
    // bunch of different idioms for implementing such helpers. You can use virtual methods, and
    // override from the various Value subclasses. You can put the method inside Value and make it
    // non-virtual, and the implementation can switch on kind. The method could be inline or not.
    // If a method is specific to some Value subclass, you could put it in the subclass, or you could
    // put it on Value anyway. It's fine to pick whatever feels right, and we shouldn't restrict
    // ourselves to any particular idiom.

    bool isConstant() const;
    bool isInteger() const;
    
    virtual Value* negConstant(Procedure&) const;
    virtual Value* addConstant(Procedure&, int32_t other) const;
    virtual Value* addConstant(Procedure&, const Value* other) const;
    virtual Value* subConstant(Procedure&, const Value* other) const;
    virtual Value* mulConstant(Procedure&, const Value* other) const;
    virtual Value* checkAddConstant(Procedure&, const Value* other) const;
    virtual Value* checkSubConstant(Procedure&, const Value* other) const;
    virtual Value* checkMulConstant(Procedure&, const Value* other) const;
    virtual Value* checkNegConstant(Procedure&) const;
    virtual Value* divConstant(Procedure&, const Value* other) const; // This chooses Div<Chill> semantics for integers.
    virtual Value* uDivConstant(Procedure&, const Value* other) const;
    virtual Value* modConstant(Procedure&, const Value* other) const; // This chooses Mod<Chill> semantics.
    virtual Value* uModConstant(Procedure&, const Value* other) const;
    virtual Value* bitAndConstant(Procedure&, const Value* other) const;
    virtual Value* bitOrConstant(Procedure&, const Value* other) const;
    virtual Value* bitXorConstant(Procedure&, const Value* other) const;
    virtual Value* shlConstant(Procedure&, const Value* other) const;
    virtual Value* sShrConstant(Procedure&, const Value* other) const;
    virtual Value* zShrConstant(Procedure&, const Value* other) const;
    virtual Value* rotRConstant(Procedure&, const Value* other) const;
    virtual Value* rotLConstant(Procedure&, const Value* other) const;
    virtual Value* bitwiseCastConstant(Procedure&) const;
    virtual Value* iToDConstant(Procedure&) const;
    virtual Value* iToFConstant(Procedure&) const;
    virtual Value* doubleToFloatConstant(Procedure&) const;
    virtual Value* floatToDoubleConstant(Procedure&) const;
    virtual Value* absConstant(Procedure&) const;
    virtual Value* ceilConstant(Procedure&) const;
    virtual Value* floorConstant(Procedure&) const;
    virtual Value* sqrtConstant(Procedure&) const;

    virtual TriState equalConstant(const Value* other) const;
    virtual TriState notEqualConstant(const Value* other) const;
    virtual TriState lessThanConstant(const Value* other) const;
    virtual TriState greaterThanConstant(const Value* other) const;
    virtual TriState lessEqualConstant(const Value* other) const;
    virtual TriState greaterEqualConstant(const Value* other) const;
    virtual TriState aboveConstant(const Value* other) const;
    virtual TriState belowConstant(const Value* other) const;
    virtual TriState aboveEqualConstant(const Value* other) const;
    virtual TriState belowEqualConstant(const Value* other) const;
    virtual TriState equalOrUnorderedConstant(const Value* other) const;
    
    // If the value is a comparison then this returns the inverted form of that comparison, if
    // possible. It can be impossible for double comparisons, where for example LessThan and
    // GreaterEqual behave differently. If this returns a value, it is a new value, which must be
    // either inserted into some block or deleted.
    Value* invertedCompare(Procedure&) const;

    bool hasInt32() const;
    int32_t asInt32() const;
    bool isInt32(int32_t) const;
    
    bool hasInt64() const;
    int64_t asInt64() const;
    bool isInt64(int64_t) const;

    bool hasInt() const;
    int64_t asInt() const;
    bool isInt(int64_t value) const;

    bool hasIntPtr() const;
    intptr_t asIntPtr() const;
    bool isIntPtr(intptr_t) const;

    bool hasDouble() const;
    double asDouble() const;
    bool isEqualToDouble(double) const; // We say "isEqualToDouble" because "isDouble" would be a bit equality.

    bool hasFloat() const;
    float asFloat() const;

    bool hasNumber() const;
    template<typename T> bool isRepresentableAs() const;
    template<typename T> T asNumber() const;

    // Booleans in B3 are Const32(0) or Const32(1). So this is true if the type is Int32 and the only
    // possible return values are 0 or 1. It's OK for this method to conservatively return false.
    bool returnsBool() const;

    bool isNegativeZero() const;

    bool isRounded() const;

    TriState asTriState() const;
    bool isLikeZero() const { return asTriState() == FalseTriState; }
    bool isLikeNonZero() const { return asTriState() == TrueTriState; }

    Effects effects() const;

    // This returns a ValueKey that describes that this Value returns when it executes. Returns an
    // empty ValueKey if this Value is impure. Note that an operation that returns Void could still
    // have a non-empty ValueKey. This happens for example with Check operations.
    ValueKey key() const;
    
    Value* foldIdentity() const;

    // Makes sure that none of the children are Identity's. If a child points to Identity, this will
    // repoint it at the Identity's child. For simplicity, this will follow arbitrarily long chains
    // of Identity's.
    bool performSubstitution();
    
    // Free values are those whose presence is guaranteed not to hurt code. We consider constants,
    // Identities, and Nops to be free. Constants are free because we hoist them to an optimal place.
    // Identities and Nops are free because we remove them.
    bool isFree() const;

    // Walk the ancestors of this value (i.e. the graph of things it transitively uses). This
    // either walks phis or not, depending on whether PhiChildren is null. Your callback gets
    // called with the signature:
    //
    //     (Value*) -> WalkStatus
    enum WalkStatus {
        Continue,
        IgnoreChildren,
        Stop
    };
    template<typename Functor>
    void walk(const Functor& functor, PhiChildren* = nullptr);

    // B3 purposefully only represents signed 32-bit offsets because that's what x86 can encode, and
    // ARM64 cannot encode anything bigger. The IsLegalOffset type trait is then used on B3 Value
    // methods to prevent implicit conversions by C++ from invalid offset types: these cause compilation
    // to fail, instead of causing implementation-defined behavior (which often turns to exploit).
    // OffsetType isn't sufficient to determine offset validity! Each Value opcode further has an
    // isLegalOffset runtime method used to determine value legality at runtime. This is exposed to users
    // of B3 to force them to reason about the target's offset.
    typedef int32_t OffsetType;
    template<typename Int>
    struct IsLegalOffset {
        static constexpr bool value = std::is_integral<Int>::value
            && std::is_signed<Int>::value
            && sizeof(Int) <= sizeof(OffsetType);
    };

protected:
    Value* cloneImpl() const;

    void replaceWith(Kind, Type, BasicBlock*);
    void replaceWith(Kind, Type, BasicBlock*, Value*);

    virtual void dumpChildren(CommaPrinter&, PrintStream&) const;
    virtual void dumpMeta(CommaPrinter&, PrintStream&) const;

    // The specific value of VarArgs does not matter, but the value of the others is assumed to match their meaning.
    enum NumChildren : uint8_t { Zero = 0, One = 1, Two = 2, Three = 3, VarArgs = 4};

    char* childrenAlloc() { return bitwise_cast<char*>(this) + adjacencyListOffset(); }
    const char* childrenAlloc() const { return bitwise_cast<const char*>(this) + adjacencyListOffset(); }
    Vector<Value*, 3>& childrenVector()
    {
        ASSERT(m_numChildren == VarArgs);
        return *bitwise_cast<Vector<Value*, 3>*>(childrenAlloc());
    }
    const Vector<Value*, 3>& childrenVector() const
    {
        ASSERT(m_numChildren == VarArgs);
        return *bitwise_cast<Vector<Value*, 3> const*>(childrenAlloc());
    }
    Value** childrenArray()
    {
        ASSERT(m_numChildren != VarArgs);
        return bitwise_cast<Value**>(childrenAlloc());
    }
    Value* const* childrenArray() const
    {
        ASSERT(m_numChildren != VarArgs);
        return bitwise_cast<Value* const*>(childrenAlloc());
    }

    template<typename... Arguments>
    static Opcode opcodeFromConstructor(Kind kind, Arguments...) { return kind.opcode(); }
    ALWAYS_INLINE static size_t adjacencyListSpace(Kind kind)
    {
        switch (kind.opcode()) {
        case FramePointer:
        case Nop:
        case Phi:
        case Jump:
        case Oops:
        case EntrySwitch:
        case ArgumentReg:
        case Const32:
        case Const64:
        case ConstFloat:
        case ConstDouble:
        case Fence:
        case SlotBase:
        case Get:
            return 0;
        case Return:
        case Identity:
        case Opaque:
        case Neg:
        case Clz:
        case Abs:
        case Ceil:
        case Floor:
        case Sqrt:
        case SExt8:
        case SExt16:
        case Trunc:
        case SExt32:
        case ZExt32:
        case FloatToDouble:
        case IToD:
        case DoubleToFloat:
        case IToF:
        case BitwiseCast:
        case Branch:
        case Depend:
        case Load8Z:
        case Load8S:
        case Load16Z:
        case Load16S:
        case Load:
        case Switch:
        case Upsilon:
        case Extract:
        case Set:
        case WasmAddress:
        case WasmBoundsCheck:
            return sizeof(Value*);
        case Add:
        case Sub:
        case Mul:
        case Div:
        case UDiv:
        case Mod:
        case UMod:
        case BitAnd:
        case BitOr:
        case BitXor:
        case Shl:
        case SShr:
        case ZShr:
        case RotR:
        case RotL:
        case Equal:
        case NotEqual:
        case LessThan:
        case GreaterThan:
        case LessEqual:
        case GreaterEqual:
        case Above:
        case Below:
        case AboveEqual:
        case BelowEqual:
        case EqualOrUnordered:
        case AtomicXchgAdd:
        case AtomicXchgAnd:
        case AtomicXchgOr:
        case AtomicXchgSub:
        case AtomicXchgXor:
        case AtomicXchg:
        case Store8:
        case Store16:
        case Store:
            return 2 * sizeof(Value*);
        case Select:
        case AtomicWeakCAS:
        case AtomicStrongCAS:
            return 3 * sizeof(Value*);
        case CCall:
        case Check:
        case CheckAdd:
        case CheckSub:
        case CheckMul:
        case Patchpoint:
            return sizeof(Vector<Value*, 3>);
#ifdef NDEBUG
        default:
            break;
#endif
        }
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }

private:
    static char* allocateSpace(Opcode opcode, size_t size)
    {
        size_t adjacencyListSpace = Value::adjacencyListSpace(opcode);
        // We must allocate enough space that replaceWithIdentity can work without buffer overflow.
        size_t allocIdentitySize = sizeof(Value) + sizeof(Value*);
        size_t allocSize = std::max(size + adjacencyListSpace, allocIdentitySize);
        return static_cast<char*>(WTF::fastMalloc(allocSize));
    }

protected:
    template<typename ValueType, typename... Arguments>
    static ValueType* allocate(Arguments... arguments)
    {
        char* alloc = allocateSpace(ValueType::opcodeFromConstructor(arguments...), sizeof(ValueType));
        return new (alloc) ValueType(arguments...);
    }
    template<typename ValueType>
    static ValueType* allocate(const ValueType& valueToClone)
    {
        char* alloc = allocateSpace(valueToClone.opcode(), sizeof(ValueType));
        ValueType* result = new (alloc) ValueType(valueToClone);
        result->buildAdjacencyList(sizeof(ValueType), valueToClone);
        return result;
    }

    // Protected so it will only be called from allocate above, possibly through the subclasses'copy constructors
    Value(const Value&) = default;

    Value(Value&&) = delete;
    Value& operator=(const Value&) = delete;
    Value& operator=(Value&&) = delete;
    
    size_t adjacencyListOffset() const;

    friend class Procedure;
    friend class SparseCollection<Value>;

private:
    template<typename... Arguments>
    void buildAdjacencyList(NumChildren numChildren, Arguments... arguments)
    {
        if (numChildren == VarArgs) {
            new (childrenAlloc()) Vector<Value*, 3> { arguments... };
            return;
        }
        ASSERT(numChildren == sizeof...(arguments));
        new (childrenAlloc()) Value*[sizeof...(arguments)] { arguments... };
    }
    void buildAdjacencyList(size_t offset, const Value& valueToClone)
    {
        switch (valueToClone.m_numChildren) {
        case VarArgs:
            new (bitwise_cast<char*>(this) + offset) Vector<Value*, 3> (valueToClone.childrenVector());
            break;
        case Three:
            bitwise_cast<Value**>(bitwise_cast<char*>(this) + offset)[2] = valueToClone.childrenArray()[2];
            FALLTHROUGH;
        case Two:
            bitwise_cast<Value**>(bitwise_cast<char*>(this) + offset)[1] = valueToClone.childrenArray()[1];
            FALLTHROUGH;
        case One:
            bitwise_cast<Value**>(bitwise_cast<char*>(this) + offset)[0] = valueToClone.childrenArray()[0];
            break;
        case Zero:
            break;
        }
    }
    
    // Checks that this kind is valid for use with B3::Value.
    ALWAYS_INLINE static NumChildren numChildrenForKind(Kind kind, unsigned numArgs)
    {
        switch (kind.opcode()) {
        case FramePointer:
        case Nop:
        case Phi:
        case Jump:
        case Oops:
        case EntrySwitch:
            if (UNLIKELY(numArgs))
                badKind(kind, numArgs);
            return Zero;
        case Return:
            if (UNLIKELY(numArgs > 1))
                badKind(kind, numArgs);
            return numArgs ? One : Zero;
        case Identity:
        case Opaque:
        case Neg:
        case Clz:
        case Abs:
        case Ceil:
        case Floor:
        case Sqrt:
        case SExt8:
        case SExt16:
        case Trunc:
        case SExt32:
        case ZExt32:
        case FloatToDouble:
        case IToD:
        case DoubleToFloat:
        case IToF:
        case BitwiseCast:
        case Branch:
        case Depend:
            if (UNLIKELY(numArgs != 1))
                badKind(kind, numArgs);
            return One;
        case Add:
        case Sub:
        case Mul:
        case Div:
        case UDiv:
        case Mod:
        case UMod:
        case BitAnd:
        case BitOr:
        case BitXor:
        case Shl:
        case SShr:
        case ZShr:
        case RotR:
        case RotL:
        case Equal:
        case NotEqual:
        case LessThan:
        case GreaterThan:
        case LessEqual:
        case GreaterEqual:
        case Above:
        case Below:
        case AboveEqual:
        case BelowEqual:
        case EqualOrUnordered:
            if (UNLIKELY(numArgs != 2))
                badKind(kind, numArgs);
            return Two;
        case Select:
            if (UNLIKELY(numArgs != 3))
                badKind(kind, numArgs);
            return Three;
        default:
            badKind(kind, numArgs);
            break;
        }
        return VarArgs;
    }

protected:
    enum CheckedOpcodeTag { CheckedOpcode };
    
    // Instantiate values via Procedure.
    // This form requires specifying the type explicitly:
    template<typename... Arguments>
    explicit Value(CheckedOpcodeTag, Kind kind, Type type, NumChildren numChildren, Origin origin, Value* firstChild, Arguments... arguments)
        : m_kind(kind)
        , m_type(type)
        , m_numChildren(numChildren)
        , m_origin(origin)
    {
        buildAdjacencyList(numChildren, firstChild, arguments...);
    }
    // This form is for specifying the type explicitly when the opcode has no children:
    explicit Value(CheckedOpcodeTag, Kind kind, Type type, NumChildren numChildren, Origin origin)
        : m_kind(kind)
        , m_type(type)
        , m_numChildren(numChildren)
        , m_origin(origin)
    {
        buildAdjacencyList(numChildren);
    }
    // This form is for those opcodes that can infer their type from the opcode alone, and that don't
    // take any arguments:
    explicit Value(CheckedOpcodeTag, Kind kind, NumChildren numChildren, Origin origin)
        : m_kind(kind)
        , m_type(typeFor(kind, nullptr))
        , m_numChildren(numChildren)
        , m_origin(origin)
    {
        buildAdjacencyList(numChildren);
    }
    // This form is for those opcodes that can infer their type from the opcode and first child:
    explicit Value(CheckedOpcodeTag, Kind kind, NumChildren numChildren, Origin origin, Value* firstChild)
        : m_kind(kind)
        , m_type(typeFor(kind, firstChild))
        , m_numChildren(numChildren)
        , m_origin(origin)
    {
        buildAdjacencyList(numChildren, firstChild);
    }
    // This form is for those opcodes that can infer their type from the opcode and first and second child:
    template<typename... Arguments>
    explicit Value(CheckedOpcodeTag, Kind kind, NumChildren numChildren, Origin origin, Value* firstChild, Value* secondChild, Arguments... arguments)
        : m_kind(kind)
        , m_type(typeFor(kind, firstChild, secondChild))
        , m_numChildren(numChildren)
        , m_origin(origin)
    {
        buildAdjacencyList(numChildren, firstChild, secondChild, arguments...);
    }

    // This is the constructor you end up actually calling, if you're instantiating Value
    // directly.
    explicit Value(Kind kind, Type type, Origin origin)
        : Value(CheckedOpcode, kind, type, Zero, origin)
    {
        RELEASE_ASSERT(numChildrenForKind(kind, 0) == Zero);
    }
    // We explicitly convert the extra arguments to Value* (they may be pointers to some subclasses of Value) to limit template explosion
    template<typename... Arguments>
    explicit Value(Kind kind, Origin origin, Arguments... arguments)
        : Value(CheckedOpcode, kind, numChildrenForKind(kind, sizeof...(arguments)), origin, static_cast<Value*>(arguments)...)
    {
    }
    template<typename... Arguments>
    explicit Value(Kind kind, Type type, Origin origin, Value* firstChild, Arguments... arguments)
        : Value(CheckedOpcode, kind, type, numChildrenForKind(kind, 1 + sizeof...(arguments)), origin, firstChild, static_cast<Value*>(arguments)...)
    {
    }

private:
    friend class CheckValue; // CheckValue::convertToAdd() modifies m_kind.

    static Type typeFor(Kind, Value* firstChild, Value* secondChild = nullptr);

    // m_index to m_numChildren are arranged to fit in 64 bits.
protected:
    unsigned m_index { UINT_MAX };
private:
    Kind m_kind;
    Type m_type;
protected:
    NumChildren m_numChildren;
private:
    Origin m_origin;

    NO_RETURN_DUE_TO_CRASH static void badKind(Kind, unsigned);

public:
    BasicBlock* owner { nullptr }; // computed by Procedure::resetValueOwners().
};

class DeepValueDump {
public:
    DeepValueDump(const Procedure* proc, const Value* value)
        : m_proc(proc)
        , m_value(value)
    {
    }

    void dump(PrintStream& out) const;

private:
    const Procedure* m_proc;
    const Value* m_value;
};

inline DeepValueDump deepDump(const Procedure& proc, const Value* value)
{
    return DeepValueDump(&proc, value);
}
inline DeepValueDump deepDump(const Value* value)
{
    return DeepValueDump(nullptr, value);
}

// The following macros are designed for subclasses of B3::Value to use.
// They are never required for correctness, but can improve the performance of child/lastChild/numChildren/children methods,
// for users that already know the specific subclass of Value they are manipulating.
// The first set is to be used when you know something about the number of children of all values of a class, including its subclasses:
// - B3_SPECIALIZE_VALUE_FOR_NO_CHILDREN: always 0 children (e.g. Const32Value)
// - B3_SPECIALIZE_VALUE_FOR_FIXED_CHILDREN(n): always n children, with n in {1, 2, 3} (e.g. UpsilonValue, with n = 1)
// - B3_SPECIALIZE_VALUE_FOR_NON_VARARGS_CHILDREN: different numbers of children, but never a variable number at runtime (e.g. MemoryValue, that can have between 1 and 3 children)
// - B3_SPECIALIZE_VALUE_FOR_VARARGS_CHILDREN: always a varargs (e.g. CCallValue)
// The second set is only to be used by classes that we know are not further subclassed by anyone adding fields,
// as they hardcode the offset of the children array/vector (which is equal to the size of the object).
// - B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_FIXED_CHILDREN
// - B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_VARARGS_CHILDREN
#define B3_SPECIALIZE_VALUE_FOR_NO_CHILDREN \
    unsigned numChildren() const { return 0; } \
    WTF::IteratorRange<Value**> children() { return {nullptr, nullptr}; } \
    WTF::IteratorRange<Value* const*> children() const { return { nullptr, nullptr}; }

#define B3_SPECIALIZE_VALUE_FOR_FIXED_CHILDREN(n) \
public: \
    unsigned numChildren() const { return n; } \
    Value*& child(unsigned index) \
    { \
        ASSERT(index <= n); \
        return childrenArray()[index]; \
    } \
    Value* child(unsigned index) const \
    { \
        ASSERT(index <= n); \
        return childrenArray()[index]; \
    } \
    Value*& lastChild() \
    { \
        return childrenArray()[n - 1]; \
    } \
    Value* lastChild() const \
    { \
        return childrenArray()[n - 1]; \
    } \
    WTF::IteratorRange<Value**> children() \
    { \
        Value** buffer = childrenArray(); \
        return {buffer, buffer + n }; \
    } \
    WTF::IteratorRange<Value* const*> children() const \
    { \
        Value* const* buffer = childrenArray(); \
        return {buffer, buffer + n }; \
    } \

#define B3_SPECIALIZE_VALUE_FOR_NON_VARARGS_CHILDREN \
public: \
    unsigned numChildren() const { return m_numChildren; } \
    Value*& child(unsigned index) { return childrenArray()[index]; } \
    Value* child(unsigned index) const { return childrenArray()[index]; } \
    Value*& lastChild() { return childrenArray()[numChildren() - 1]; } \
    Value* lastChild() const { return childrenArray()[numChildren() - 1]; } \
    WTF::IteratorRange<Value**> children() \
    { \
        Value** buffer = childrenArray(); \
        return {buffer, buffer + numChildren() }; \
    } \
    WTF::IteratorRange<Value* const*> children() const \
    { \
        Value* const* buffer = childrenArray(); \
        return {buffer, buffer + numChildren() }; \
    } \

#define B3_SPECIALIZE_VALUE_FOR_VARARGS_CHILDREN \
public: \
    unsigned numChildren() const { return childrenVector().size(); } \
    Value*& child(unsigned index) { return childrenVector()[index]; } \
    Value* child(unsigned index) const { return childrenVector()[index]; } \
    Value*& lastChild() { return childrenVector().last(); } \
    Value* lastChild() const { return childrenVector().last(); } \
    WTF::IteratorRange<Value**> children() \
    { \
        Vector<Value*, 3>& vec = childrenVector(); \
        return WTF::makeIteratorRange(&*vec.begin(), &*vec.end()); \
    } \
    WTF::IteratorRange<Value* const*> children() const \
    { \
        const Vector<Value*, 3>& vec = childrenVector(); \
        return WTF::makeIteratorRange(&*vec.begin(), &*vec.end()); \
    } \

// Only use this for classes with no subclass that add new fields (as it uses sizeof(*this))
// Also there is no point in applying this to classes with no children, as they don't have a children array to access.
#define B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_FIXED_CHILDREN \
private: \
    Value** childrenArray() \
    { \
        return bitwise_cast<Value**>(bitwise_cast<char*>(this) + sizeof(*this)); \
    } \
    Value* const* childrenArray() const \
    { \
        return bitwise_cast<Value* const*>(bitwise_cast<char const*>(this) + sizeof(*this)); \
    }

// Only use this for classes with no subclass that add new fields (as it uses sizeof(*this))
#define B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_VARARGS_CHILDREN \
private: \
    Vector<Value*, 3>& childrenVector() \
    { \
        return *bitwise_cast<Vector<Value*, 3>*>(bitwise_cast<char*>(this) + sizeof(*this)); \
    } \
    const Vector<Value*, 3>& childrenVector() const \
    { \
        return *bitwise_cast<Vector<Value*, 3> const*>(bitwise_cast<char const*>(this) + sizeof(*this)); \
    } \

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
