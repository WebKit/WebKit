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

    class Register;
    
    class JSActivation : public JSVariableObject {
    private:
        JSActivation(JSGlobalData& globalData, CallFrame*, SharedSymbolTable*, size_t storageSize);
    
    public:
        typedef JSVariableObject Base;

        static JSActivation* create(JSGlobalData& globalData, CallFrame* callFrame, FunctionExecutable* functionExecutable)
        {
            size_t storageSize = JSActivation::storageSize(functionExecutable->symbolTable());
            JSActivation* activation = new (
                NotNull,
                allocateCell<JSActivation>(
                    globalData.heap,
                    allocationSize(storageSize)
                )
            ) JSActivation(globalData, callFrame, functionExecutable->symbolTable(), storageSize);
            activation->finishCreation(globalData);
            return activation;
        }

        static void visitChildren(JSCell*, SlotVisitor&);

        bool isDynamicScope(bool& requiresDynamicChecks) const;

        static bool getOwnPropertySlot(JSCell*, ExecState*, PropertyName, PropertySlot&);
        static void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
        JS_EXPORT_PRIVATE static bool getOwnPropertyDescriptor(JSObject*, ExecState*, PropertyName, PropertyDescriptor&);

        static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);

        static void putDirectVirtual(JSObject*, ExecState*, PropertyName, JSValue, unsigned attributes);
        static bool deleteProperty(JSCell*, ExecState*, PropertyName);

        static JSObject* toThisObject(JSCell*, ExecState*);

        void tearOff(JSGlobalData&);
        
        static const ClassInfo s_info;

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto) { return Structure::create(globalData, globalObject, proto, TypeInfo(ActivationObjectType, StructureFlags), &s_info); }

        bool isValid(const SymbolTableEntry&);
        bool isTornOff();

    protected:
        static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesVisitChildren | OverridesGetPropertyNames | Base::StructureFlags;

    private:
        bool symbolTableGet(PropertyName, PropertySlot&);
        bool symbolTableGet(PropertyName, PropertyDescriptor&);
        bool symbolTableGet(PropertyName, PropertySlot&, bool& slotIsWriteable);
        bool symbolTablePut(ExecState*, PropertyName, JSValue, bool shouldThrow);
        bool symbolTablePutWithAttributes(JSGlobalData&, PropertyName, JSValue, unsigned attributes);

        static JSValue argumentsGetter(ExecState*, JSValue, PropertyName);
        NEVER_INLINE PropertySlot::GetValueFunc getArgumentsGetter();

        static size_t allocationSize(size_t storageSize);
        static size_t storageSize(SharedSymbolTable*);
        static int captureStart(SharedSymbolTable*);

        int registerOffset();
        size_t storageSize();
        WriteBarrier<Unknown>* storage(); // storageSize() number of registers.
    };

    extern int activationCount;
    extern int allTheThingsCount;

    inline JSActivation::JSActivation(JSGlobalData& globalData, CallFrame* callFrame, SharedSymbolTable* symbolTable, size_t storageSize)
        : Base(
            globalData,
            callFrame->lexicalGlobalObject()->activationStructure(),
            callFrame->registers(),
            callFrame->scope(),
            symbolTable
        )
    {
        WriteBarrier<Unknown>* storage = this->storage();
        for (size_t i = 0; i < storageSize; ++i)
            new(&storage[i]) WriteBarrier<Unknown>;
    }

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
        requiresDynamicChecks = symbolTable()->usesNonStrictEval();
        return false;
    }

    inline int JSActivation::captureStart(SharedSymbolTable* symbolTable)
    {
        if (symbolTable->captureMode() == SharedSymbolTable::AllOfTheThings)
            return -CallFrame::offsetFor(symbolTable->parameterCountIncludingThis());
        return symbolTable->captureStart();
    }

    inline size_t JSActivation::storageSize(SharedSymbolTable* symbolTable)
    {
        return symbolTable->captureEnd() - captureStart(symbolTable);
    }

    inline int JSActivation::registerOffset()
    {
        return -captureStart(symbolTable());
    }

    inline size_t JSActivation::storageSize()
    {
        return storageSize(symbolTable());
    }

    inline void JSActivation::tearOff(JSGlobalData& globalData)
    {
        ASSERT(!isTornOff());

        int registerOffset = this->registerOffset();
        WriteBarrierBase<Unknown>* dst = storage() + registerOffset;
        WriteBarrierBase<Unknown>* src = m_registers;

        if (symbolTable()->captureMode() == SharedSymbolTable::AllOfTheThings) {
            int from = -registerOffset;
            int to = CallFrame::thisArgumentOffset(); // Skip 'this' because it's not lexically accessible.
            for (int i = from; i < to; ++i)
                dst[i].set(globalData, this, src[i].get());

            dst[RegisterFile::ArgumentCount].set(globalData, this, JSValue(
                CallFrame::create(reinterpret_cast<Register*>(src))->argumentCountIncludingThis()));

            int captureEnd = symbolTable()->captureEnd();
            for (int i = 0; i < captureEnd; ++i)
                dst[i].set(globalData, this, src[i].get());
        } else {
            int captureEnd = symbolTable()->captureEnd();
            for (int i = symbolTable()->captureStart(); i < captureEnd; ++i)
                dst[i].set(globalData, this, src[i].get());
        }

        m_registers = dst;
        ASSERT(isTornOff());
    }

    inline bool JSActivation::isTornOff()
    {
        return m_registers == storage() + registerOffset();
    }

    inline WriteBarrier<Unknown>* JSActivation::storage()
    {
        return reinterpret_cast<WriteBarrier<Unknown>*>(
            reinterpret_cast<char*>(this) +
                WTF::roundUpToMultipleOf<sizeof(WriteBarrier<Unknown>)>(sizeof(JSActivation))
        );
    }

    inline size_t JSActivation::allocationSize(size_t storageSize)
    {
        size_t objectSizeInBytes = WTF::roundUpToMultipleOf<sizeof(WriteBarrier<Unknown>)>(sizeof(JSActivation));
        size_t storageSizeInBytes = storageSize * sizeof(WriteBarrier<Unknown>);
        return objectSizeInBytes + storageSizeInBytes;
    }

    inline bool JSActivation::isValid(const SymbolTableEntry& entry)
    {
        if (entry.getIndex() < captureStart(symbolTable()))
            return false;
        if (entry.getIndex() >= symbolTable()->captureEnd())
            return false;
        return true;
    }

} // namespace JSC

#endif // JSActivation_h
