/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "JSVariableObject.h"
#include "RegisterFile.h"
#include "SymbolTable.h"
#include "Nodes.h"

namespace JSC {

    class Arguments;
    class Register;
    
    class JSActivation : public JSVariableObject {
        typedef JSVariableObject Base;
    public:
        JSActivation(CallFrame*, NonNullPassRefPtr<FunctionExecutable>);
        virtual ~JSActivation();

        virtual void markChildren(MarkStack&);

        virtual bool isDynamicScope() const;

        virtual bool isActivationObject() const { return true; }

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);

        virtual void put(ExecState*, const Identifier&, JSValue, PutPropertySlot&);

        virtual void putWithAttributes(ExecState*, const Identifier&, JSValue, unsigned attributes);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        virtual JSObject* toThisObject(ExecState*) const;

        void copyRegisters(Arguments* arguments);
        
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

        static PassRefPtr<Structure> createStructure(JSValue proto) { return Structure::create(proto, TypeInfo(ObjectType, OverridesGetOwnPropertySlot | NeedsThisConversion | OverridesMarkChildren | OverridesGetPropertyNames)); }

    private:
        struct JSActivationData : public JSVariableObjectData {
            JSActivationData(NonNullPassRefPtr<FunctionExecutable> _functionExecutable, Register* registers)
                : JSVariableObjectData(_functionExecutable->generatedBytecode().symbolTable(), registers)
                , functionExecutable(_functionExecutable)
            {
                // We have to manually ref and deref the symbol table as JSVariableObjectData
                // doesn't know about SharedSymbolTable
                functionExecutable->generatedBytecode().sharedSymbolTable()->ref();
            }
            ~JSActivationData()
            {
                static_cast<SharedSymbolTable*>(symbolTable)->deref();
            }

            RefPtr<FunctionExecutable> functionExecutable;
        };
        
        static JSValue argumentsGetter(ExecState*, const Identifier&, const PropertySlot&);
        NEVER_INLINE PropertySlot::GetValueFunc getArgumentsGetter();

        JSActivationData* d() const { return static_cast<JSActivationData*>(JSVariableObject::d); }
    };

    JSActivation* asActivation(JSValue);

    inline JSActivation* asActivation(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&JSActivation::info));
        return static_cast<JSActivation*>(asObject(value));
    }

} // namespace JSC

#endif // JSActivation_h
