/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All Rights Reserved.
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

#ifndef JSScope_h
#define JSScope_h

#include "JSObject.h"

namespace JSC {

class ScopeChainIterator;
class VariableWatchpointSet;

enum ResolveMode {
    ThrowIfNotFound,
    DoNotThrowIfNotFound
};

enum ResolveType {
    // Lexical scope guaranteed a certain type of variable access.
    GlobalProperty,
    GlobalVar,
    ClosureVar,

    // Ditto, but at least one intervening scope used non-strict eval, which
    // can inject an intercepting var delcaration at runtime.
    GlobalPropertyWithVarInjectionChecks,
    GlobalVarWithVarInjectionChecks,
    ClosureVarWithVarInjectionChecks,

    // Lexical scope didn't prove anything -- probably because of a 'with' scope.
    Dynamic
};

const char* resolveModeName(ResolveMode mode);
const char* resolveTypeName(ResolveType type);

inline ResolveType makeType(ResolveType type, bool needsVarInjectionChecks)
{
    if (!needsVarInjectionChecks)
        return type;

    switch (type) {
    case GlobalProperty:
        return GlobalPropertyWithVarInjectionChecks;
    case GlobalVar:
        return GlobalVarWithVarInjectionChecks;
    case ClosureVar:
        return ClosureVarWithVarInjectionChecks;
    case GlobalPropertyWithVarInjectionChecks:
    case GlobalVarWithVarInjectionChecks:
    case ClosureVarWithVarInjectionChecks:
    case Dynamic:
        return type;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return type;
}

inline bool needsVarInjectionChecks(ResolveType type)
{
    switch (type) {
    case GlobalProperty:
    case GlobalVar:
    case ClosureVar:
        return false;
    case GlobalPropertyWithVarInjectionChecks:
    case GlobalVarWithVarInjectionChecks:
    case ClosureVarWithVarInjectionChecks:
    case Dynamic:
        return true;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return true;
    }
}

struct ResolveOp {
    ResolveOp(ResolveType type, size_t depth, Structure* structure, JSActivation* activation, VariableWatchpointSet* watchpointSet, uintptr_t operand)
        : type(type)
        , depth(depth)
        , structure(structure)
        , activation(activation)
        , watchpointSet(watchpointSet)
        , operand(operand)
    {
    }

    ResolveType type;
    size_t depth;
    Structure* structure;
    JSActivation* activation;
    VariableWatchpointSet* watchpointSet;
    uintptr_t operand;
};

class ResolveModeAndType {
    typedef unsigned Operand;
public:
    static const size_t shift = sizeof(Operand) * 8 / 2;
    static const unsigned mask = (1 << shift) - 1;

    ResolveModeAndType(ResolveMode resolveMode, ResolveType resolveType)
        : m_operand((resolveMode << shift) | resolveType)
    {
    }

    explicit ResolveModeAndType(unsigned operand)
        : m_operand(operand)
    {
    }

    ResolveMode mode() { return static_cast<ResolveMode>(m_operand >> shift); }
    ResolveType type() { return static_cast<ResolveType>(m_operand & mask); }
    unsigned operand() { return m_operand; }

private:
    Operand m_operand;
};

enum GetOrPut { Get, Put };

class JSScope : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    friend class LLIntOffsetsExtractor;
    static size_t offsetOfNext();

    JS_EXPORT_PRIVATE static JSObject* objectAtScope(JSScope*);

    static JSValue resolve(ExecState*, JSScope*, const Identifier&);
    static ResolveOp abstractResolve(ExecState*, JSScope*, const Identifier&, GetOrPut, ResolveType);

    static void visitChildren(JSCell*, SlotVisitor&);

    ScopeChainIterator begin();
    ScopeChainIterator end();
    JSScope* next();
    int depth();

    JSGlobalObject* globalObject();
    VM* vm();
    JSObject* globalThis();

protected:
    JSScope(VM&, Structure*, JSScope* next);
    static const unsigned StructureFlags = OverridesVisitChildren | Base::StructureFlags;

private:
    WriteBarrier<JSScope> m_next;
};

inline JSScope::JSScope(VM& vm, Structure* structure, JSScope* next)
    : Base(vm, structure)
    , m_next(vm, this, next, WriteBarrier<JSScope>::MayBeNull)
{
}

class ScopeChainIterator {
public:
    ScopeChainIterator(JSScope* node)
        : m_node(node)
    {
    }

    JSObject* get() const { return JSScope::objectAtScope(m_node); }
    JSObject* operator->() const { return JSScope::objectAtScope(m_node); }

    ScopeChainIterator& operator++() { m_node = m_node->next(); return *this; }

    // postfix ++ intentionally omitted

    bool operator==(const ScopeChainIterator& other) const { return m_node == other.m_node; }
    bool operator!=(const ScopeChainIterator& other) const { return m_node != other.m_node; }

private:
    JSScope* m_node;
};

inline ScopeChainIterator JSScope::begin()
{
    return ScopeChainIterator(this); 
}

inline ScopeChainIterator JSScope::end()
{ 
    return ScopeChainIterator(0); 
}

inline JSScope* JSScope::next()
{ 
    return m_next.get();
}

inline JSGlobalObject* JSScope::globalObject()
{ 
    return structure()->globalObject();
}

inline VM* JSScope::vm()
{ 
    return MarkedBlock::blockFor(this)->vm();
}

inline Register& Register::operator=(JSScope* scope)
{
    *this = JSValue(scope);
    return *this;
}

inline JSScope* Register::scope() const
{
    return jsCast<JSScope*>(jsValue());
}

inline VM& ExecState::vm() const
{
    ASSERT(scope()->vm());
    return *scope()->vm();
}

inline JSGlobalObject* ExecState::lexicalGlobalObject() const
{
    return scope()->globalObject();
}

inline JSObject* ExecState::globalThisValue() const
{
    return scope()->globalThis();
}

inline size_t JSScope::offsetOfNext()
{
    return OBJECT_OFFSETOF(JSScope, m_next);
}

} // namespace JSC

#endif // JSScope_h
