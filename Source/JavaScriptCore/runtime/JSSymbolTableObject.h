/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef JSSymbolTableObject_h
#define JSSymbolTableObject_h

#include "JSScope.h"
#include "PropertyDescriptor.h"
#include "SymbolTable.h"

namespace JSC {

class JSSymbolTableObject : public JSScope {
public:
    typedef JSScope Base;
    
    SymbolTable* symbolTable() const { return m_symbolTable.get(); }
    
    JS_EXPORT_PRIVATE static bool deleteProperty(JSCell*, ExecState*, PropertyName);
    JS_EXPORT_PRIVATE static void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
    
protected:
    static const unsigned StructureFlags = IsEnvironmentRecord | OverridesVisitChildren | OverridesGetPropertyNames | Base::StructureFlags;
    
    JSSymbolTableObject(VM& vm, Structure* structure, JSScope* scope, SymbolTable* symbolTable = 0)
        : Base(vm, structure, scope)
    {
        if (symbolTable)
            m_symbolTable.set(vm, this, symbolTable);
    }

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        if (!m_symbolTable)
            m_symbolTable.set(vm, this, SymbolTable::create(vm));
    }

    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<SymbolTable> m_symbolTable;
};

template<typename SymbolTableObjectType>
inline bool symbolTableGet(
    SymbolTableObjectType* object, PropertyName propertyName, PropertySlot& slot)
{
    SymbolTable& symbolTable = *object->symbolTable();
    ConcurrentJITLocker locker(symbolTable.m_lock);
    SymbolTable::Map::iterator iter = symbolTable.find(locker, propertyName.uid());
    if (iter == symbolTable.end(locker))
        return false;
    SymbolTableEntry::Fast entry = iter->value;
    ASSERT(!entry.isNull());
    slot.setValue(object, entry.getAttributes() | DontDelete, object->registerAt(entry.getIndex()).get());
    return true;
}

template<typename SymbolTableObjectType>
inline bool symbolTableGet(
    SymbolTableObjectType* object, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    SymbolTable& symbolTable = *object->symbolTable();
    ConcurrentJITLocker locker(symbolTable.m_lock);
    SymbolTable::Map::iterator iter = symbolTable.find(locker, propertyName.uid());
    if (iter == symbolTable.end(locker))
        return false;
    SymbolTableEntry::Fast entry = iter->value;
    ASSERT(!entry.isNull());
    descriptor.setDescriptor(
        object->registerAt(entry.getIndex()).get(), entry.getAttributes() | DontDelete);
    return true;
}

template<typename SymbolTableObjectType>
inline bool symbolTableGet(
    SymbolTableObjectType* object, PropertyName propertyName, PropertySlot& slot,
    bool& slotIsWriteable)
{
    SymbolTable& symbolTable = *object->symbolTable();
    ConcurrentJITLocker locker(symbolTable.m_lock);
    SymbolTable::Map::iterator iter = symbolTable.find(locker, propertyName.uid());
    if (iter == symbolTable.end(locker))
        return false;
    SymbolTableEntry::Fast entry = iter->value;
    ASSERT(!entry.isNull());
    slot.setValue(object, entry.getAttributes() | DontDelete, object->registerAt(entry.getIndex()).get());
    slotIsWriteable = !entry.isReadOnly();
    return true;
}

template<typename SymbolTableObjectType>
inline bool symbolTablePut(
    SymbolTableObjectType* object, ExecState* exec, PropertyName propertyName, JSValue value,
    bool shouldThrow)
{
    VM& vm = exec->vm();
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(object));
    
    WriteBarrierBase<Unknown>* reg;
    {
        SymbolTable& symbolTable = *object->symbolTable();
        GCSafeConcurrentJITLocker locker(symbolTable.m_lock, exec->vm().heap);
        SymbolTable::Map::iterator iter = symbolTable.find(locker, propertyName.uid());
        if (iter == symbolTable.end(locker))
            return false;
        bool wasFat;
        SymbolTableEntry::Fast fastEntry = iter->value.getFast(wasFat);
        ASSERT(!fastEntry.isNull());
        if (fastEntry.isReadOnly()) {
            if (shouldThrow)
                throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
            return true;
        }
        if (VariableWatchpointSet* set = iter->value.watchpointSet())
            set->notifyWrite(value);
        reg = &object->registerAt(fastEntry.getIndex());
    }
    // I'd prefer we not hold lock while executing barriers, since I prefer to reserve
    // the right for barriers to be able to trigger GC. And I don't want to hold VM
    // locks while GC'ing.
    reg->set(vm, object, value);
    return true;
}

template<typename SymbolTableObjectType>
inline bool symbolTablePutWithAttributes(
    SymbolTableObjectType* object, VM& vm, PropertyName propertyName,
    JSValue value, unsigned attributes)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(object));

    WriteBarrierBase<Unknown>* reg;
    {
        SymbolTable& symbolTable = *object->symbolTable();
        ConcurrentJITLocker locker(symbolTable.m_lock);
        SymbolTable::Map::iterator iter = symbolTable.find(locker, propertyName.uid());
        if (iter == symbolTable.end(locker))
            return false;
        SymbolTableEntry& entry = iter->value;
        ASSERT(!entry.isNull());
        if (VariableWatchpointSet* set = entry.watchpointSet())
            set->notifyWrite(value);
        entry.setAttributes(attributes);
        reg = &object->registerAt(entry.getIndex());
    }
    reg->set(vm, object, value);
    return true;
}

} // namespace JSC

#endif // JSSymbolTableObject_h

