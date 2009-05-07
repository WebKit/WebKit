/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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

#ifndef JSFunction_h
#define JSFunction_h

#include "InternalFunction.h"
#include "JSVariableObject.h"
#include "SymbolTable.h"
#include "Nodes.h"
#include "JSObject.h"

namespace JSC {

    class FunctionBodyNode;
    class FunctionPrototype;
    class JSActivation;
    class JSGlobalObject;

    class JSFunction : public InternalFunction {
        friend class JIT;
        friend class JITStubs;
        friend class VPtrSet;

        typedef InternalFunction Base;

        JSFunction(PassRefPtr<Structure> structure)
            : InternalFunction(structure)
        {
            clearScopeChain();
        }

    public:
        JSFunction(ExecState*, PassRefPtr<Structure>, int length, const Identifier&, NativeFunction);
        JSFunction(ExecState*, const Identifier&, FunctionBodyNode*, ScopeChainNode*);
        ~JSFunction();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        JSObject* construct(ExecState*, const ArgList&);
        JSValue call(ExecState*, JSValue thisValue, const ArgList&);

        void setScope(const ScopeChain& scopeChain) { setScopeChain(scopeChain); }
        ScopeChain& scope() { return scopeChain(); }

        void setBody(FunctionBodyNode* body) { m_body = body; }
        void setBody(PassRefPtr<FunctionBodyNode> body) { m_body = body; }
        FunctionBodyNode* body() const { return m_body.get(); }

        virtual void mark();

        static JS_EXPORTDATA const ClassInfo info;

        static PassRefPtr<Structure> createStructure(JSValue prototype) 
        { 
            return Structure::create(prototype, TypeInfo(ObjectType, ImplementsHasInstance)); 
        }

#if ENABLE(JIT)
        bool isHostFunction() const { return m_body && m_body->isHostFunction(); }
#else
        bool isHostFunction() const { return false; }
#endif
        NativeFunction nativeFunction()
        {
            return *reinterpret_cast<NativeFunction*>(m_data);
        }
    private:
        virtual const ClassInfo* classInfo() const { return &info; }

        virtual ConstructType getConstructData(ConstructData&);
        virtual CallType getCallData(CallData&);

        static JSValue argumentsGetter(ExecState*, const Identifier&, const PropertySlot&);
        static JSValue callerGetter(ExecState*, const Identifier&, const PropertySlot&);
        static JSValue lengthGetter(ExecState*, const Identifier&, const PropertySlot&);

        RefPtr<FunctionBodyNode> m_body;
        ScopeChain& scopeChain()
        {
            ASSERT(!isHostFunction());
            return *reinterpret_cast<ScopeChain*>(m_data);
        }
        void clearScopeChain()
        {
            ASSERT(!isHostFunction());
            new (m_data) ScopeChain(NoScopeChain());
        }
        void setScopeChain(ScopeChainNode* sc)
        {
            ASSERT(!isHostFunction());
            new (m_data) ScopeChain(sc);
        }
        void setScopeChain(const ScopeChain& sc)
        {
            ASSERT(!isHostFunction());
            *reinterpret_cast<ScopeChain*>(m_data) = sc;
        }
        void setNativeFunction(NativeFunction func)
        {
            *reinterpret_cast<NativeFunction*>(m_data) = func;
        }
        unsigned char m_data[sizeof(void*)];
    };

    JSFunction* asFunction(JSValue);

    inline JSFunction* asFunction(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&JSFunction::info));
        return static_cast<JSFunction*>(asObject(value));
    }

} // namespace JSC

#endif // JSFunction_h
