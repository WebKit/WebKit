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
#include "CopiedSpaceInlineMethods.h"
#include "JSVariableObject.h"
#include "Nodes.h"
#include "SymbolTable.h"

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
            activation->finishCreation(callFrame, funcExec);
            return activation;
        }

        static void visitChildren(JSCell*, SlotVisitor&);

        bool isDynamicScope(bool& requiresDynamicChecks) const;

        static bool getOwnPropertySlot(JSCell*, ExecState*, PropertyName, PropertySlot&);
        static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
        JS_EXPORT_PRIVATE static bool getOwnPropertyDescriptor(JSObject*, ExecState*, PropertyName, PropertyDescriptor&);

        static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);

        static void putDirectVirtual(JSObject*, ExecState*, PropertyName, JSValue, unsigned attributes);
        static bool deleteProperty(JSCell*, ExecState*, PropertyName);

        static JSObject* toThisObject(JSCell*, ExecState*);

        void tearOff(JSGlobalData&);
        
        static const ClassInfo s_info;

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto) { return Structure::create(globalData, globalObject, proto, TypeInfo(ActivationObjectType, StructureFlags), &s_info); }

        bool isValidScopedLookup(int index) { return index < m_numCapturedVars; }

    protected:
        void finishCreation(CallFrame*, FunctionExecutable*);
        static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesVisitChildren | OverridesGetPropertyNames | Base::StructureFlags;

    private:
        bool symbolTableGet(PropertyName, PropertySlot&);
        bool symbolTableGet(PropertyName, PropertyDescriptor&);
        bool symbolTableGet(PropertyName, PropertySlot&, bool& slotIsWriteable);
        bool symbolTablePut(ExecState*, PropertyName, JSValue, bool shouldThrow);
        bool symbolTablePutWithAttributes(JSGlobalData&, PropertyName, JSValue, unsigned attributes);

        static JSValue argumentsGetter(ExecState*, JSValue, PropertyName);
        NEVER_INLINE PropertySlot::GetValueFunc getArgumentsGetter();

        size_t registerOffset();
        size_t registerArraySize();
        size_t registerArraySizeInBytes();

        StorageBarrier m_registerArray; // Independent copy of registers, used when a variable object copies its registers out of the register file.
        unsigned m_numCapturedArgs;
        unsigned m_numCapturedVars : 28;
        unsigned m_isTornOff : 1;
        unsigned m_requiresDynamicChecks : 1;
        unsigned m_argumentsRegister : 2;
    };

    JSActivation* asActivation(JSValue);

    inline JSActivation* asActivation(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&JSActivation::s_info));
        return jsCast<JSActivation*>(asObject(value));
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

    inline size_t JSActivation::registerOffset()
    {
        if (!m_numCapturedArgs)
            return 0;

        size_t capturedArgumentCountIncludingThis = m_numCapturedArgs + 1;
        return CallFrame::offsetFor(capturedArgumentCountIncludingThis);
    }

    inline size_t JSActivation::registerArraySize()
    {
        return registerOffset() + m_numCapturedVars;
    }

    inline size_t JSActivation::registerArraySizeInBytes()
    {
        return registerArraySize() * sizeof(WriteBarrierBase<Unknown>);
    }

    inline void JSActivation::tearOff(JSGlobalData& globalData)
    {
        ASSERT(!m_registerArray);
        ASSERT(m_numCapturedVars + m_numCapturedArgs);

        void* allocation = 0;
        if (!globalData.heap.tryAllocateStorage(registerArraySizeInBytes(), &allocation))
            CRASH();
        PropertyStorage registerArray = static_cast<PropertyStorage>(allocation);
        PropertyStorage registers = registerArray + registerOffset();

        // arguments
        int from = CallFrame::argumentOffset(m_numCapturedArgs - 1);
        int to = CallFrame::thisArgumentOffset(); // Skip 'this' because it's not lexically accessible.
        for (int i = from; i < to; ++i)
            registers[i].set(globalData, this, m_registers[i].get());

        // vars
        from = 0;
        to = m_numCapturedVars;
        for (int i = from; i < to; ++i)
            registers[i].set(globalData, this, m_registers[i].get());

        m_registerArray.set(globalData, this, registerArray);
        m_registers = registers;
        m_isTornOff = true;
    }

} // namespace JSC

#endif // JSActivation_h
