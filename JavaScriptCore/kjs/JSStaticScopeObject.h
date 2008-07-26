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

namespace KJS{
    
    class JSStaticScopeObject : public JSVariableObject {
    protected:
        using JSVariableObject::JSVariableObjectData;
        struct JSStaticScopeObjectData : public JSVariableObjectData {
            JSStaticScopeObjectData()
                : JSVariableObjectData(&symbolTable, &registerStore + 1)
            {
                registerArraySize = 1;
            }
            SymbolTable symbolTable;
            Register registerStore;
        };
        
    public:
        JSStaticScopeObject(const Identifier& ident, JSValue* value, unsigned attributes)
            : JSVariableObject(new JSStaticScopeObjectData())
        {
            JSStaticScopeObjectData* data = static_cast<JSStaticScopeObjectData*>(d);
            data->registerStore = value;
            symbolTable().add(ident.ustring().rep(), SymbolTableEntry(-1, attributes));
        }
        virtual ~JSStaticScopeObject();
        bool isDynamicScope() const;
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&, bool& slotIsWriteable);
        void putWithAttributes(ExecState*, const Identifier&, JSValue*, unsigned attributes);
    };

}

#endif // !JSStaticScopeObject_h
