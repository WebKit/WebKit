/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "CagedBarrierPtr.h"
#include "DirectArgumentsOffset.h"
#include "GenericArguments.h"
#include <wtf/CagedPtr.h>

namespace JSC {

class LLIntOffsetsExtractor;
    
// This is an Arguments-class object that we create when you say "arguments" inside a function,
// and none of the arguments are captured in the function's activation. The function will copy all
// of its arguments into this object, and all subsequent accesses to the arguments will go through
// this object thereafter. Special support is in place for mischevious events like the arguments
// being deleted (something like "delete arguments[0]") or reconfigured (broadly, we say deletions
// and reconfigurations mean that the respective argument was "overridden").
class DirectArguments final : public GenericArguments<DirectArguments> {
private:
    DirectArguments(VM&, Structure*, WriteBarrier<Unknown>* storage);
    
public:
    template<typename CellType>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        RELEASE_ASSERT(!CellType::needsDestruction);
        return &vm.directArgumentsSpace;
    }

    // Creates an arguments object but leaves it uninitialized. This is dangerous if we GC right
    // after allocation.
    static DirectArguments* createUninitialized(VM&, Structure*, unsigned length, unsigned capacity);
    
    // Creates an arguments object and initializes everything to the empty value. Use this if you
    // cannot guarantee that you'll immediately initialize all of the elements.
    static DirectArguments* create(VM&, Structure*, unsigned length, unsigned capacity);
    
    // Creates an arguments object by copying the argumnets from the stack.
    static DirectArguments* createByCopying(ExecState*);

    static size_t estimatedSize(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);
    
    uint32_t internalLength() const
    {
        return storageHeader().length;
    }
    
    uint32_t length(ExecState* exec) const
    {
        if (UNLIKELY(m_mappedArguments)) {
            VM& vm = exec->vm();
            auto scope = DECLARE_THROW_SCOPE(vm);
            JSValue value = get(exec, vm.propertyNames->length);
            RETURN_IF_EXCEPTION(scope, 0);
            scope.release();
            return value.toUInt32(exec);
        }
        return storageHeader().length;
    }
    
    bool isMappedArgument(uint32_t i) const
    {
        return i < storageHeader().length && (!m_mappedArguments || !m_mappedArguments[i]);
    }

    bool isMappedArgumentInDFG(uint32_t i) const
    {
        return i < storageHeader().length && !overrodeThings();
    }
    
    JSValue getIndexQuickly(uint32_t i) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(isMappedArgument(i));
        WriteBarrier<Unknown>* storage = this->storage();
        auto* ptr = &storage[i];
        return preciseIndexMaskPtr(i, storageHeader(storage).length, ptr)->get();
    }
    
    void setIndexQuickly(VM& vm, uint32_t i, JSValue value)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(isMappedArgument(i));
        WriteBarrier<Unknown>* storage = this->storage();
        auto* ptr = &storage[i];
        preciseIndexMaskPtr(i, storageHeader(storage).length, ptr)->set(vm, this, value);
    }
    
    WriteBarrier<JSFunction>& callee()
    {
        return m_callee;
    }
    
    WriteBarrier<Unknown>& argument(DirectArgumentsOffset offset)
    {
        ASSERT(offset);
        ASSERT_WITH_SECURITY_IMPLICATION(offset.offset() < std::max(storageHeader().length, storageHeader().minCapacity));
        WriteBarrier<Unknown>* storage = this->storage();
        auto* ptr = &storage[offset.offset()];
        return *preciseIndexMaskPtr(
            offset.offset(),
            std::max(storageHeader(storage).length, storageHeader(storage).minCapacity),
            ptr);
    }
    
    // Methods intended for use by the GenericArguments mixin.
    bool overrodeThings() const { return !!m_mappedArguments; }
    void overrideThings(VM&);
    void overrideThingsIfNecessary(VM&);
    void unmapArgument(VM&, unsigned index);

    void initModifiedArgumentsDescriptorIfNecessary(VM& vm)
    {
        GenericArguments<DirectArguments>::initModifiedArgumentsDescriptorIfNecessary(vm, storageHeader().length);
    }

    void setModifiedArgumentDescriptor(VM& vm, unsigned index)
    {
        GenericArguments<DirectArguments>::setModifiedArgumentDescriptor(vm, index, storageHeader().length);
    }

    bool isModifiedArgumentDescriptor(unsigned index)
    {
        return GenericArguments<DirectArguments>::isModifiedArgumentDescriptor(index, storageHeader().length);
    }

    void copyToArguments(ExecState*, VirtualRegister firstElementDest, unsigned offset, unsigned length);

    DECLARE_INFO;
    
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);
    
    static ptrdiff_t offsetOfCallee() { return OBJECT_OFFSETOF(DirectArguments, m_callee); }
    static ptrdiff_t offsetOfMappedArguments() { return OBJECT_OFFSETOF(DirectArguments, m_mappedArguments); }
    static ptrdiff_t offsetOfModifiedArgumentsDescriptor() { return OBJECT_OFFSETOF(DirectArguments, m_modifiedArgumentsDescriptor); }
    static ptrdiff_t offsetOfStorage() { return OBJECT_OFFSETOF(DirectArguments, m_storage); }

    static ptrdiff_t offsetOfLengthInStorage() { return OBJECT_OFFSETOF(StorageHeader, length) - sizeof(WriteBarrier<Unknown>); }
    static ptrdiff_t offsetOfMinCapacityInStorage() { return OBJECT_OFFSETOF(StorageHeader, minCapacity) - sizeof(WriteBarrier<Unknown>); }
    
    static size_t storageSize(Checked<size_t> capacity)
    {
        return (sizeof(WriteBarrier<Unknown>) * (capacity + static_cast<size_t>(1))).unsafeGet();
    }
    
    static size_t storageHeaderSize() { return sizeof(WriteBarrier<Unknown>); }
    
    static size_t allocationSize(size_t inlineSize)
    {
        RELEASE_ASSERT(!inlineSize);
        return sizeof(DirectArguments);
    }
    
private:
    friend class LLIntOffsetsExtractor;
    
    struct StorageHeader {
        uint32_t length; // Always the actual length of captured arguments and never what was stored into the length property.
        uint32_t minCapacity; // The max of this and length determines the capacity of this object. It may be the actual capacity, or maybe something smaller. We arrange it this way to be kind to the JITs.
    };
    
    WriteBarrier<Unknown>* storage() const
    {
        return m_storage.get().unpoisoned();
    }
    
    static StorageHeader& storageHeader(WriteBarrier<Unknown>* storage)
    {
        static_assert(sizeof(StorageHeader) == sizeof(WriteBarrier<Unknown>), "StorageHeader needs to be the same size as a JSValue");
        return *bitwise_cast<StorageHeader*>(storage - 1);
    }

    StorageHeader& storageHeader() const
    {
        return storageHeader(storage());
    }
    
    unsigned mappedArgumentsSize();
    
    WriteBarrier<JSFunction> m_callee;
    CagedBarrierPtr<Gigacage::Primitive, bool> m_mappedArguments; // If non-null, it means that length, callee, and caller are fully materialized properties.
    AuxiliaryBarrier<Poisoned<DirectArgumentsPoison, WriteBarrier<Unknown>*>> m_storage;
};

} // namespace JSC
