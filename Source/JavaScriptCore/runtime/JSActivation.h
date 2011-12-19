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
#include "SymbolTable.h"
#include "Nodes.h"

namespace JSC {

    class Arguments;
    class Register;
    
    class JSActivation : public JSVariableObject {
    private:
        JSActivation(CallFrame*, FunctionExecutable*);
    
    public:
        typedef JSVariableObject Base;

        static JSActivation* create(JSGlobalData& globalData, CallFrame* callFrame, FunctionExecutable* funcExec)
        {
            JSActivation* activation = new (NotNull, allocateCell<JSActivation>(globalData.heap)) JSActivation(callFrame, funcExec);
            activation->finishCreation(callFrame);
            return activation;
        }

        static void finalize(JSCell*);

        static void visitChildren(JSCell*, SlotVisitor&);

        bool isDynamicScope(bool& requiresDynamicChecks) const;

        static bool getOwnPropertySlot(JSCell*, ExecState*, const Identifier&, PropertySlot&);
        static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

        static void put(JSCell*, ExecState*, const Identifier&, JSValue, PutPropertySlot&);

        static void putWithAttributes(JSObject*, ExecState*, const Identifier&, JSValue, unsigned attributes);
        static bool deleteProperty(JSCell*, ExecState*, const Identifier& propertyName);

        static JSObject* toThisObject(JSCell*, ExecState*);

        void tearOff(JSGlobalData&);
        
        static const ClassInfo s_info;

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto) { return Structure::create(globalData, globalObject, proto, TypeInfo(ActivationObjectType, StructureFlags), &s_info); }

    protected:
        void finishCreation(CallFrame*);
        static const unsigned StructureFlags = IsEnvironmentRecord | OverridesGetOwnPropertySlot | OverridesVisitChildren | OverridesGetPropertyNames | JSVariableObject::StructureFlags;

    private:
        bool symbolTableGet(const Identifier&, PropertySlot&);
        bool symbolTableGet(const Identifier&, PropertyDescriptor&);
        bool symbolTableGet(const Identifier&, PropertySlot&, bool& slotIsWriteable);
        bool symbolTablePut(JSGlobalData&, const Identifier&, JSValue);
        bool symbolTablePutWithAttributes(JSGlobalData&, const Identifier&, JSValue, unsigned attributes);

        static JSValue argumentsGetter(ExecState*, JSValue, const Identifier&);
        NEVER_INLINE PropertySlot::GetValueFunc getArgumentsGetter();

        int m_numCapturedArgs;
        int m_numCapturedVars : 31;
        bool m_requiresDynamicChecks : 1;
        int m_argumentsRegister;
    };

    JSActivation* asActivation(JSValue);

    inline JSActivation* asActivation(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&JSActivation::s_info));
        return static_cast<JSActivation*>(asObject(value));
    }
    
    ALWAYS_INLINE JSActivation* Register::activation() const
    {
        return asActivation(jsValue());
    }

    inline bool JSActivation::isDynamicScope(bool& requiresDynamicChecks) const
    {
        requiresDynamicChecks = m_requiresDynamicChecks;
        return false;
    }

    inline void JSActivation::tearOff(JSGlobalData& globalData)
    {
        ASSERT(!m_registerArray);
        ASSERT(m_numCapturedVars + m_numCapturedArgs);

        int registerOffset = CallFrame::offsetFor(m_numCapturedArgs + 1);
        size_t registerArraySize = registerOffset + m_numCapturedVars;

        OwnArrayPtr<WriteBarrier<Unknown> > registerArray = adoptArrayPtr(new WriteBarrier<Unknown>[registerArraySize]);
        WriteBarrier<Unknown>* registers = registerArray.get() + registerOffset;

        // Copy all arguments that can be captured by name or by the arguments object.
        for (int i = 0; i < m_numCapturedArgs; ++i) {
            int index = CallFrame::argumentOffset(i);
            registers[index].set(globalData, this, m_registers[index].get());
        }

        // Skip 'this' and call frame.

        // Copy all captured vars.
        for (int i = 0; i < m_numCapturedVars; ++i)
            registers[i].set(globalData, this, m_registers[i].get());

        setRegisters(registers, registerArray.release());
    }

} // namespace JSC

#endif // JSActivation_h
