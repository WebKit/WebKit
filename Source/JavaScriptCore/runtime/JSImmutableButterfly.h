/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "Butterfly.h"
#include "IndexingHeader.h"
#include "JSCell.h"
#include "Structure.h"
#include "VirtualRegister.h"

namespace JSC {

class JSImmutableButterfly : public JSCell {
    using Base = JSCell;

public:
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, IndexingType indexingType)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSImmutableButterflyType, StructureFlags), info(), indexingType);
    }

    ALWAYS_INLINE static JSImmutableButterfly* tryCreate(VM& vm, Structure* structure, unsigned size)
    {
        Checked<size_t, RecordOverflow> checkedAllocationSize = allocationSize(size);
        if (UNLIKELY(checkedAllocationSize.hasOverflowed()))
            return nullptr;

        void* buffer = tryAllocateCell<JSImmutableButterfly>(vm.heap, checkedAllocationSize.unsafeGet());
        if (UNLIKELY(!buffer))
            return nullptr;
        JSImmutableButterfly* result = new (NotNull, buffer) JSImmutableButterfly(vm, structure, size);
        result->finishCreation(vm);
        return result;
    }

    static JSImmutableButterfly* create(VM& vm, IndexingType indexingType, unsigned length)
    {
        auto* array = tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(indexingType) - NumberOfIndexingShapes].get(), length);
        RELEASE_ASSERT(array);
        return array;
    }

    static JSImmutableButterfly* createSentinel(VM& vm)
    {
        return create(vm, CopyOnWriteArrayWithContiguous, 0);
    }

    unsigned publicLength() const { return m_header.publicLength(); }
    unsigned vectorLength() const { return m_header.vectorLength(); }
    unsigned length() const { return m_header.publicLength(); }

    Butterfly* toButterfly() const { return bitwise_cast<Butterfly*>(bitwise_cast<char*>(this) + offsetOfData()); }
    static JSImmutableButterfly* fromButterfly(Butterfly* butterfly) { return bitwise_cast<JSImmutableButterfly*>(bitwise_cast<char*>(butterfly) - offsetOfData()); }

    JSValue get(unsigned index) const
    {
        if (!hasDouble(indexingMode()))
            return toButterfly()->contiguous().at(this, index).get();
        double value = toButterfly()->contiguousDouble().at(this, index);
        // Holes are not supported yet.
        ASSERT(!std::isnan(value));
        return jsNumber(value);
    }

    static void visitChildren(JSCell*, SlotVisitor&);

    void copyToArguments(ExecState*, VirtualRegister firstElementDest, unsigned offset, unsigned length);

    template<typename>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        // We allocate out of the JSValue gigacage as other code expects all butterflies to live there.
        return &vm.immutableButterflyJSValueGigacageAuxiliarySpace;
    }

    // Only call this if you just allocated this butterfly.
    void setIndex(VM& vm, unsigned index, JSValue value)
    {
        if (hasDouble(indexingType()))
            toButterfly()->contiguousDouble().atUnsafe(index) = value.asNumber();
        else
            toButterfly()->contiguous().atUnsafe(index).set(vm, this, value);
    }

    static constexpr size_t offsetOfData()
    {
        return WTF::roundUpToMultipleOf<sizeof(WriteBarrier<Unknown>)>(sizeof(JSImmutableButterfly));
    }

private:
    static Checked<size_t, RecordOverflow> allocationSize(Checked<size_t, RecordOverflow> numItems)
    {
        return offsetOfData() + numItems * sizeof(WriteBarrier<Unknown>);
    }

    JSImmutableButterfly(VM& vm, Structure* structure, unsigned length)
        : Base(vm, structure)
    {
        m_header.setVectorLength(length);
        m_header.setPublicLength(length);
    }

    IndexingHeader m_header;
};

} // namespace JSC
