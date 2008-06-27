/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#ifndef JSActivation_h
#define JSActivation_h

#include "JSVariableObject.h"
#include "SymbolTable.h"
#include "nodes.h"

namespace KJS {

    class Register;
    
    class JSActivation : public JSVariableObject {
    typedef JSVariableObject Base;
    public:
        JSActivation(PassRefPtr<FunctionBodyNode>, Register**, int registerOffset);
        virtual ~JSActivation();
        
        virtual bool isActivationObject() const;
        virtual bool isDynamicScope() const;

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);

        virtual void put(ExecState*, const Identifier&, JSValue*);
        virtual void putWithAttributes(ExecState*, const Identifier&, JSValue*, unsigned attributes);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        virtual JSObject* toThisObject(ExecState*) const;

        virtual void mark();
        
        void copyRegisters();
        
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

    private:
        struct JSActivationData : public JSVariableObjectData {
            JSActivationData(PassRefPtr<FunctionBodyNode> functionBody_, Register** registerBase, int registerOffset)
                : JSVariableObjectData(&functionBody_->symbolTable(), registerBase, registerOffset)
                , functionBody(functionBody_)
                , argumentsObject(0)
            {
            }

            RefPtr<FunctionBodyNode> functionBody; // Owns the symbol table and code block
            JSObject* argumentsObject;
        };
        
        static JSValue* argumentsGetter(ExecState*, const Identifier&, const PropertySlot&);
        NEVER_INLINE PropertySlot::GetValueFunc getArgumentsGetter();
        NEVER_INLINE JSObject* createArgumentsObject(ExecState*);

        JSActivationData* d() const { return static_cast<JSActivationData*>(JSVariableObject::d); }
    };
    
} // namespace KJS

#endif // JSActivation_h
