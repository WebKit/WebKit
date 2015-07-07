/*
 * Copyright (C) 2008, 2009, 2013 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "CopiedSpaceInlines.h"
#include "JSVariableObject.h"
#include "Nodes.h"
#include "SymbolTable.h"

namespace JSC {

class Register;
    
class JSActivation : public JSVariableObject {
private:
    JSActivation(VM&, CallFrame*, Register*, SymbolTable*);
    
public:
    typedef JSVariableObject Base;

    static JSActivation* create(VM& vm, CallFrame* callFrame, Register* registers, CodeBlock* codeBlock)
    {
        SymbolTable* symbolTable = codeBlock->symbolTable();
        ASSERT(codeBlock->codeType() == FunctionCode);
        JSActivation* activation = new (
            NotNull,
            allocateCell<JSActivation>(
                vm.heap,
                allocationSize(symbolTable)
            )
        ) JSActivation(vm, callFrame, registers, symbolTable);
        activation->finishCreation(vm);
        return activation;
    }
        
    static JSActivation* create(VM& vm, CallFrame* callFrame, CodeBlock* codeBlock)
    {
        return create(vm, callFrame, callFrame->registers() + codeBlock->framePointerOffsetToGetActivationRegisters(), codeBlock);
    }

    static void visitChildren(JSCell*, SlotVisitor&);

    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    static void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

    static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);

    static bool deleteProperty(JSCell*, ExecState*, PropertyName);

    static JSValue toThis(JSCell*, ExecState*, ECMAMode);

    void tearOff(VM&);
        
    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto) { return Structure::create(vm, globalObject, proto, TypeInfo(ActivationObjectType, StructureFlags), info()); }

    WriteBarrierBase<Unknown>& registerAt(int) const;
    bool isValidIndex(int) const;
    bool isValid(const SymbolTableEntry&) const;
    bool isTornOff();
    int registersOffset();
    static int registersOffset(SymbolTable*);

protected:
    static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesVisitChildren | OverridesGetPropertyNames | Base::StructureFlags;

private:
    bool symbolTableGet(PropertyName, PropertySlot&);
    bool symbolTableGet(PropertyName, PropertyDescriptor&);
    bool symbolTableGet(PropertyName, PropertySlot&, bool& slotIsWriteable);
    bool symbolTablePut(ExecState*, PropertyName, JSValue, bool shouldThrow);
    bool symbolTablePutWithAttributes(VM&, PropertyName, JSValue, unsigned attributes);

    static EncodedJSValue argumentsGetter(ExecState*, JSObject*, EncodedJSValue, PropertyName);

    static size_t allocationSize(SymbolTable*);
    static size_t storageOffset();

    WriteBarrier<Unknown>* storage(); // captureCount() number of registers.
};

extern int activationCount;
extern int allTheThingsCount;

inline JSActivation::JSActivation(VM& vm, CallFrame* callFrame, Register* registers, SymbolTable* symbolTable)
    : Base(
        vm,
        callFrame->lexicalGlobalObject()->activationStructure(),
        registers,
        callFrame->scope(),
        symbolTable)
{
    WriteBarrier<Unknown>* storage = this->storage();
    size_t captureCount = symbolTable->captureCount();
    for (size_t i = 0; i < captureCount; ++i)
        new (NotNull, &storage[i]) WriteBarrier<Unknown>;
}

JSActivation* asActivation(JSValue);

inline JSActivation* asActivation(JSValue value)
{
    ASSERT(asObject(value)->inherits(JSActivation::info()));
    return jsCast<JSActivation*>(asObject(value));
}
    
ALWAYS_INLINE JSActivation* Register::activation() const
{
    return asActivation(jsValue());
}

inline int JSActivation::registersOffset(SymbolTable* symbolTable)
{
    return storageOffset() + ((symbolTable->captureCount() - symbolTable->captureStart()  - 1) * sizeof(WriteBarrier<Unknown>));
}

inline void JSActivation::tearOff(VM& vm)
{
    ASSERT(!isTornOff());

    WriteBarrierBase<Unknown>* dst = reinterpret_cast_ptr<WriteBarrierBase<Unknown>*>(
        reinterpret_cast<char*>(this) + registersOffset(symbolTable()));
    WriteBarrierBase<Unknown>* src = m_registers;

    int captureEnd = symbolTable()->captureEnd();
    for (int i = symbolTable()->captureStart(); i > captureEnd; --i)
        dst[i].set(vm, this, src[i].get());

    m_registers = dst;
    ASSERT(isTornOff());
}

inline bool JSActivation::isTornOff()
{
    return m_registers == reinterpret_cast_ptr<WriteBarrierBase<Unknown>*>(
        reinterpret_cast<char*>(this) + registersOffset(symbolTable()));
}

inline size_t JSActivation::storageOffset()
{
    return WTF::roundUpToMultipleOf<sizeof(WriteBarrier<Unknown>)>(sizeof(JSActivation));
}

inline WriteBarrier<Unknown>* JSActivation::storage()
{
    return reinterpret_cast_ptr<WriteBarrier<Unknown>*>(
        reinterpret_cast<char*>(this) + storageOffset());
}

inline size_t JSActivation::allocationSize(SymbolTable* symbolTable)
{
    size_t objectSizeInBytes = WTF::roundUpToMultipleOf<sizeof(WriteBarrier<Unknown>)>(sizeof(JSActivation));
    size_t storageSizeInBytes = symbolTable->captureCount() * sizeof(WriteBarrier<Unknown>);
    return objectSizeInBytes + storageSizeInBytes;
}

inline bool JSActivation::isValidIndex(int index) const
{
    if (index > symbolTable()->captureStart())
        return false;
    if (index <= symbolTable()->captureEnd())
        return false;
    return true;
}

inline bool JSActivation::isValid(const SymbolTableEntry& entry) const
{
    return isValidIndex(entry.getIndex());
}

inline WriteBarrierBase<Unknown>& JSActivation::registerAt(int index) const
{
    ASSERT(isValidIndex(index));
    return Base::registerAt(index);
}

} // namespace JSC

#endif // JSActivation_h
