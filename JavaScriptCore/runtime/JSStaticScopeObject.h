/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
    protected:
        using JSVariableObject::JSVariableObjectData;
        struct JSStaticScopeObjectData : public JSVariableObjectData {
            JSStaticScopeObjectData()
                : JSVariableObjectData(&symbolTable, &registerStore + 1)
            {
            }
            SymbolTable symbolTable;
            Register registerStore;
        };
        
    public:
        JSStaticScopeObject(ExecState* exec, const Identifier& ident, JSValue value, unsigned attributes)
            : JSVariableObject(exec->globalData().staticScopeStructure, new JSStaticScopeObjectData())
        {
            d()->registerStore = value;
            symbolTable().add(ident.ustring().rep(), SymbolTableEntry(-1, attributes));
        }
        virtual ~JSStaticScopeObject();
        virtual void mark();
        bool isDynamicScope() const;
        virtual JSObject* toThisObject(ExecState*) const;
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual void put(ExecState*, const Identifier&, JSValue, PutPropertySlot&);
        void putWithAttributes(ExecState*, const Identifier&, JSValue, unsigned attributes);

        static PassRefPtr<Structure> createStructure(JSValue proto) { return Structure::create(proto, TypeInfo(ObjectType, NeedsThisConversion)); }

    private:
        JSStaticScopeObjectData* d() { return static_cast<JSStaticScopeObjectData*>(JSVariableObject::d); }
    };

}

#endif // JSStaticScopeObject_h
