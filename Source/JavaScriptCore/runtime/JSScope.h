/*
 * Copyright (C) 2012-2015  Apple Inc. All Rights Reserved.
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

#include "GetPutInfo.h"
#include "JSObject.h"
#include "VariableEnvironment.h"

namespace JSC {

class ScopeChainIterator;
class WatchpointSet;

class JSScope : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;
    static const unsigned StructureFlags = Base::StructureFlags;

    friend class LLIntOffsetsExtractor;
    static size_t offsetOfNext();

    static JSObject* objectAtScope(JSScope*);

    static JSValue resolve(ExecState*, JSScope*, const Identifier&);
    static ResolveOp abstractResolve(ExecState*, size_t depthOffset, JSScope*, const Identifier&, GetOrPut, ResolveType, InitializationMode);

    static bool hasConstantScope(ResolveType);
    static JSScope* constantScopeForCodeBlock(ResolveType, CodeBlock*);

    static void collectVariablesUnderTDZ(JSScope*, VariableEnvironment& result);

    static void visitChildren(JSCell*, SlotVisitor&);

    bool isVarScope();
    bool isLexicalScope();
    bool isModuleScope();
    bool isGlobalLexicalEnvironment();
    bool isCatchScope();
    bool isFunctionNameScopeObject();

    bool isNestedLexicalScope();

    ScopeChainIterator begin();
    ScopeChainIterator end();
    JSScope* next();

    JSGlobalObject* globalObject();
    VM* vm();
    JSObject* globalThis();

protected:
    JSScope(VM&, Structure*, JSScope* next);

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
    JSScope* scope() const { return m_node; }

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

inline JSGlobalObject* ExecState::lexicalGlobalObject() const
{
    return callee()->globalObject();
}

inline size_t JSScope::offsetOfNext()
{
    return OBJECT_OFFSETOF(JSScope, m_next);
}

} // namespace JSC

#endif // JSScope_h
