/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#ifndef JSStaticScopeObject_h
#define JSStaticScopeObject_h

#include "JSVariableObject.h"

namespace JSC{
    
    class JSStaticScopeObject : public JSVariableObject {
    public:
        typedef JSVariableObject Base;

        static JSStaticScopeObject* create(ExecState* exec, const Identifier& identifier, JSValue value, unsigned attributes)
        {
            JSStaticScopeObject* scopeObject = new (NotNull, allocateCell<JSStaticScopeObject>(*exec->heap())) JSStaticScopeObject(exec);
            scopeObject->finishCreation(exec, identifier, value, attributes);
            return scopeObject;
        }

        static void visitChildren(JSCell*, SlotVisitor&);
        bool isDynamicScope(bool& requiresDynamicChecks) const;
        static JSObject* toThisObject(JSCell*, ExecState*);
        static bool getOwnPropertySlot(JSCell*, ExecState*, const Identifier&, PropertySlot&);
        static void put(JSCell*, ExecState*, const Identifier&, JSValue, PutPropertySlot&);

        static void putDirectVirtual(JSObject*, ExecState*, const Identifier&, JSValue, unsigned attributes);

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto) { return Structure::create(globalData, globalObject, proto, TypeInfo(StaticScopeObjectType, StructureFlags), &s_info); }

        static const ClassInfo s_info;

    protected:
        void finishCreation(ExecState* exec, const Identifier& identifier, JSValue value, unsigned attributes)
        {
            Base::finishCreation(exec->globalData());
            m_registerStore.set(exec->globalData(), this, value);
            symbolTable().add(identifier.impl(), SymbolTableEntry(-1, attributes));
        }
        static const unsigned StructureFlags = IsEnvironmentRecord | OverridesGetOwnPropertySlot | OverridesVisitChildren | OverridesGetPropertyNames | JSVariableObject::StructureFlags;

    private:
        JSStaticScopeObject(ExecState* exec)
            : JSVariableObject(exec->globalData(), exec->globalData().staticScopeStructure.get(), &m_symbolTable, reinterpret_cast<Register*>(&m_registerStore + 1))
        {
        }

        static void destroy(JSCell*);

        SymbolTable m_symbolTable;
        WriteBarrier<Unknown> m_registerStore;
    };

    inline bool JSStaticScopeObject::isDynamicScope(bool&) const
    {
        return false;
    }

}

#endif // JSStaticScopeObject_h
