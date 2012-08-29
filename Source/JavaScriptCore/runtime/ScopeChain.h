/*
 *  Copyright (C) 2003, 2008, 2009 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef ScopeChain_h
#define ScopeChain_h

#include "JSCell.h"
#include "Structure.h"
#include <wtf/FastAllocBase.h>

namespace JSC {

    class JSGlobalData;
    class JSGlobalObject;
    class JSObject;
    class LLIntOffsetsExtractor;
    class ScopeChainIterator;
    class SlotVisitor;
    
    class ScopeChainNode : public JSCell {
    private:
        ScopeChainNode(ScopeChainNode* next, JSObject* object, JSGlobalData* globalData, JSGlobalObject* globalObject, JSObject* globalThis)
            : JSCell(*globalData, globalData->scopeChainNodeStructure.get())
            , globalData(globalData)
            , next(*globalData, this, next, WriteBarrier<ScopeChainNode>::MayBeNull)
            , object(*globalData, this, object)
            , globalObject(*globalData, this, globalObject)
            , globalThis(*globalData, this, globalThis)
        {
        }

    protected:
        void finishCreation(JSGlobalData* globalData, JSGlobalObject* globalObject)
        {
            Base::finishCreation(*globalData);
            ASSERT_UNUSED(globalObject, globalObject);
        }

    public:
        typedef JSCell Base;

        static ScopeChainNode* create(ExecState* exec, ScopeChainNode* next, JSObject* object, JSGlobalData* globalData, JSGlobalObject* globalObject, JSObject* globalThis)
        {
            ScopeChainNode* node = new (NotNull, allocateCell<ScopeChainNode>(*exec->heap())) ScopeChainNode(next, object, globalData, globalObject, globalThis);
            node->finishCreation(globalData, globalObject);
            return node;
        }
        static ScopeChainNode* create(ScopeChainNode* next, JSObject* object, JSGlobalData* globalData, JSGlobalObject* globalObject, JSObject* globalThis)
        {
            ScopeChainNode* node = new (NotNull, allocateCell<ScopeChainNode>(globalData->heap)) ScopeChainNode(next, object, globalData, globalObject, globalThis);
            node->finishCreation(globalData, globalObject);
            return node;
        }
        
        JSGlobalData* globalData;
        WriteBarrier<ScopeChainNode> next;
    private:
        WriteBarrier<JSObject> object;
    public:
        WriteBarrier<JSGlobalObject> globalObject;
        WriteBarrier<JSObject> globalThis;

        ScopeChainNode* push(JSObject*);
        ScopeChainNode* pop();

        ScopeChainIterator begin();
        ScopeChainIterator end();

        int localDepth();

#ifndef NDEBUG        
        void print();
#endif
        
        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto) { return Structure::create(globalData, globalObject, proto, TypeInfo(CompoundType, StructureFlags), &s_info); }
        static void visitChildren(JSCell*, SlotVisitor&);
        static JS_EXPORTDATA const ClassInfo s_info;

    private:
        friend class LLIntOffsetsExtractor;
        friend class ScopeChainIterator;
        friend class JSScope;
        friend class DFG::SpeculativeJIT;
        friend class JIT;
        
        static const unsigned StructureFlags = OverridesVisitChildren;
    };

    inline ScopeChainNode* ScopeChainNode::push(JSObject* o)
    {
        ASSERT(o);
        return ScopeChainNode::create(this, o, globalData, globalObject.get(), globalThis.get());
    }

    inline ScopeChainNode* ScopeChainNode::pop()
    {
        ASSERT(next);
        return next.get();
    }

    ALWAYS_INLINE JSGlobalData& ExecState::globalData() const
    {
        ASSERT(scopeChain()->globalData);
        return *scopeChain()->globalData;
    }

    ALWAYS_INLINE JSGlobalObject* ExecState::lexicalGlobalObject() const
    {
        return scopeChain()->globalObject.get();
    }
    
    ALWAYS_INLINE JSObject* ExecState::globalThisValue() const
    {
        return scopeChain()->globalThis.get();
    }
    
    ALWAYS_INLINE ScopeChainNode* Register::scopeChain() const
    {
        return static_cast<ScopeChainNode*>(jsValue().asCell());
    }
    
    ALWAYS_INLINE Register& Register::operator=(ScopeChainNode* scopeChain)
    {
        *this = JSValue(scopeChain);
        return *this;
    }

} // namespace JSC

#endif // ScopeChain_h
