/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef B3Value_h
#define B3Value_h

#if ENABLE(B3_JIT)

#include "AirArg.h"
#include "B3Effects.h"
#include "B3Opcode.h"
#include "B3Origin.h"
#include "B3Type.h"
#include "B3ValueKey.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

namespace JSC { namespace B3 {

class BasicBlock;
class CheckValue;
class Procedure;

class JS_EXPORT_PRIVATE Value {
    WTF_MAKE_NONCOPYABLE(Value);
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef Vector<Value*, 3> AdjacencyList;

    static const char* const dumpPrefix;

    static bool accepts(Opcode) { return true; }

    virtual ~Value();

    unsigned index() const { return m_index; }
    
    // Note that the opcode is immutable, except for replacing values with Identity or Nop.
    Opcode opcode() const { return m_opcode; }

    Origin origin() const { return m_origin; }
    
    Value*& child(unsigned index) { return m_children[index]; }
    Value* child(unsigned index) const { return m_children[index]; }

    Value*& lastChild() { return m_children.last(); }
    Value* lastChild() const { return m_children.last(); }

    unsigned numChildren() const { return m_children.size(); }

    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }

    // This is useful when lowering. Note that this is only valid for non-void values.
    Air::Arg::Type airType() const { return Air::Arg::typeForB3Type(type()); }

    AdjacencyList& children() { return m_children; } 
    const AdjacencyList& children() const { return m_children; }

    void replaceWithIdentity(Value*);
    void replaceWithNop();

    void dump(PrintStream&) const;
    void deepDump(PrintStream&) const;

    // This is how you cast Values. For example, if you want to do something provided that we have a
    // ArgumentRegValue, you can do:
    //
    // if (ArgumentRegValue* argumentReg = value->as<ArgumentRegValue>()) {
    //     things
    // }
    //
    // This will return null if this opcode() != ArgumentReg. This works because this returns nullptr
    // if T::accepts(opcode()) returns false.
    template<typename T>
    T* as();
    template<typename T>
    const T* as() const;

    // What follows are a bunch of helpers for inspecting and modifying values. Note that we have a
    // bunch of different idioms for implementing such helpers. You can use virtual methods, and
    // override from the various Value subclasses. You can put the method inside Value and make it
    // non-virtual, and the implementation can switch on opcode. The method could be inline or not.
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
    virtual Value* divConstant(Procedure&, const Value* other) const; // This chooses ChillDiv semantics for integers.
    virtual Value* modConstant(Procedure&, const Value* other) const; // This chooses ChillMod semantics for integers.
    virtual Value* bitAndConstant(Procedure&, const Value* other) const;
    virtual Value* bitOrConstant(Procedure&, const Value* other) const;
    virtual Value* bitXorConstant(Procedure&, const Value* other) const;
    virtual Value* shlConstant(Procedure&, const Value* other) const;
    virtual Value* sShrConstant(Procedure&, const Value* other) const;
    virtual Value* zShrConstant(Procedure&, const Value* other) const;
    virtual Value* bitwiseCastConstant(Procedure&) const;
    virtual Value* doubleToFloatConstant(Procedure&) const;
    virtual Value* floatToDoubleConstant(Procedure&) const;
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
    template<typename T> bool representableAs() const;
    template<typename T> T asNumber() const;

    // Booleans in B3 are Const32(0) or Const32(1). So this is true if the type is Int32 and the only
    // possible return values are 0 or 1. It's OK for this method to conservatively return false.
    bool returnsBool() const;

    bool isNegativeZero() const;

    TriState asTriState() const;
    bool isLikeZero() const { return asTriState() == FalseTriState; }
    bool isLikeNonZero() const { return asTriState() == TrueTriState; }

    Effects effects() const;

    // This returns a ValueKey that describes that this Value returns when it executes. Returns an
    // empty ValueKey if this Value is impure. Note that an operation that returns Void could still
    // have a non-empty ValueKey. This happens for example with Check operations.
    ValueKey key() const;

    // Makes sure that none of the children are Identity's. If a child points to Identity, this will
    // repoint it at the Identity's child. For simplicity, this will follow arbitrarily long chains
    // of Identity's.
    void performSubstitution();

protected:
    virtual void dumpChildren(CommaPrinter&, PrintStream&) const;
    virtual void dumpMeta(CommaPrinter&, PrintStream&) const;

private:
    friend class Procedure;

    // Checks that this opcode is valid for use with B3::Value.
#if ASSERT_DISABLED
    static void checkOpcode(Opcode) { }
#else
    static void checkOpcode(Opcode);
#endif

protected:
    enum CheckedOpcodeTag { CheckedOpcode };
    
    // Instantiate values via Procedure.
    // This form requires specifying the type explicitly:
    template<typename... Arguments>
    explicit Value(unsigned index, CheckedOpcodeTag, Opcode opcode, Type type, Origin origin, Value* firstChild, Arguments... arguments)
        : m_index(index)
        , m_opcode(opcode)
        , m_type(type)
        , m_origin(origin)
        , m_children{ firstChild, arguments... }
    {
    }
    // This form is for specifying the type explicitly when the opcode has no children:
    explicit Value(unsigned index, CheckedOpcodeTag, Opcode opcode, Type type, Origin origin)
        : m_index(index)
        , m_opcode(opcode)
        , m_type(type)
        , m_origin(origin)
    {
    }
    // This form is for those opcodes that can infer their type from the opcode and first child:
    template<typename... Arguments>
    explicit Value(unsigned index, CheckedOpcodeTag, Opcode opcode, Origin origin, Value* firstChild)
        : m_index(index)
        , m_opcode(opcode)
        , m_type(typeFor(opcode, firstChild))
        , m_origin(origin)
        , m_children{ firstChild }
    {
    }
    // This form is for those opcodes that can infer their type from the opcode and first and second child:
    template<typename... Arguments>
    explicit Value(unsigned index, CheckedOpcodeTag, Opcode opcode, Origin origin, Value* firstChild, Value* secondChild, Arguments... arguments)
        : m_index(index)
        , m_opcode(opcode)
        , m_type(typeFor(opcode, firstChild, secondChild))
        , m_origin(origin)
        , m_children{ firstChild, secondChild, arguments... }
    {
    }
    // This form is for those opcodes that can infer their type from the opcode alone, and that don't
    // take any arguments:
    explicit Value(unsigned index, CheckedOpcodeTag, Opcode opcode, Origin origin)
        : m_index(index)
        , m_opcode(opcode)
        , m_type(typeFor(opcode, nullptr))
        , m_origin(origin)
    {
    }
    // Use this form for varargs.
    explicit Value(unsigned index, CheckedOpcodeTag, Opcode opcode, Type type, Origin origin, const AdjacencyList& children)
        : m_index(index)
        , m_opcode(opcode)
        , m_type(type)
        , m_origin(origin)
        , m_children(children)
    {
    }
    explicit Value(unsigned index, CheckedOpcodeTag, Opcode opcode, Type type, Origin origin, AdjacencyList&& children)
        : m_index(index)
        , m_opcode(opcode)
        , m_type(type)
        , m_origin(origin)
        , m_children(WTF::move(children))
    {
    }

    // This is the constructor you end up actually calling, if you're instantiating Value
    // directly.
    template<typename... Arguments>
    explicit Value(unsigned index, Opcode opcode, Arguments&&... arguments)
        : Value(index, CheckedOpcode, opcode, std::forward<Arguments>(arguments)...)
    {
        checkOpcode(opcode);
    }

private:
    friend class CheckValue; // CheckValue::convertToAdd() modifies m_opcode.
    
    static Type typeFor(Opcode, Value* firstChild, Value* secondChild = nullptr);

    // This group of fields is arranged to fit in 64 bits.
    unsigned m_index;
    Opcode m_opcode;
    Type m_type;
    
    Origin m_origin;
    AdjacencyList m_children;

public:
    BasicBlock* owner { nullptr }; // computed by Procedure::resetValueOwners().
};

class DeepValueDump {
public:
    DeepValueDump(const Value* value)
        : m_value(value)
    {
    }

    void dump(PrintStream& out) const
    {
        if (m_value)
            m_value->deepDump(out);
        else
            out.print("<null>");
    }

private:
    const Value* m_value;
};

inline DeepValueDump deepDump(const Value* value)
{
    return DeepValueDump(value);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3Value_h

