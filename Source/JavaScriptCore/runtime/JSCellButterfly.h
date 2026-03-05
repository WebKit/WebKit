/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/Butterfly.h>
#include <JavaScriptCore/IndexingHeader.h>
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSCell.h>
#include <JavaScriptCore/ResourceExhaustion.h>
#include <JavaScriptCore/Structure.h>
#include <JavaScriptCore/VirtualRegister.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class ClonedArguments;
class DirectArguments;
class JSSet;
class ScopedArguments;

// This is essentially a normal butterfly but it can also be handled as a cell since it has a cell header.
// The main use is for immutable, shared Butterflies for Array literals but JSCellButterflies are also used
// as a trailing array storage in JS Map/Set.
//
// Note: the name JSCellButterfly doesn't necessarily mean its contents are cells although they could be.
class JSCellButterfly : public JSCell {
    using Base = JSCell;

public:
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    DECLARE_EXPORT_INFO;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue, IndexingType);

    ALWAYS_INLINE static JSCellButterfly* tryCreate(VM& vm, Structure* structure, unsigned length)
    {
        if (length > IndexingHeader::maximumLength) [[unlikely]]
            return nullptr;

        // Because of the above maximumLength requirement, allocationSize can never overflow.
        void* buffer = tryAllocateCell<JSCellButterfly>(vm, allocationSize(length));
        if (!buffer) [[unlikely]]
            return nullptr;
        JSCellButterfly* result = new (NotNull, buffer) JSCellButterfly(vm, structure, length);
        result->finishCreation(vm);
        return result;
    }

    static JSCellButterfly* tryCreate(VM& vm, IndexingType indexingType, unsigned length)
    {
        return tryCreate(vm, vm.cellButterflyStructure(indexingType), length);
    }

    static JSCellButterfly* create(VM& vm, IndexingType indexingType, unsigned length)
    {
        auto* array = tryCreate(vm, indexingType, length);
        RELEASE_ASSERT_RESOURCE_AVAILABLE(array, MemoryExhaustion, "Crash intentionally because memory is exhausted.");
        return array;
    }

    ALWAYS_INLINE static JSCellButterfly* createFromArray(JSGlobalObject* globalObject, VM& vm, JSArray* array)
    {
        auto throwScope = DECLARE_THROW_SCOPE(vm);

        IndexingType indexingType = array->indexingType() & IndexingShapeMask;
        unsigned length = array->length();

        // FIXME: JSCellButterfly::createFromArray should support re-using non contiguous indexing types as well.
        if (isCopyOnWrite(indexingType)) {
            if (hasContiguous(indexingType))
                return JSCellButterfly::fromButterfly(array->butterfly());
        }

        JSCellButterfly* result = JSCellButterfly::tryCreate(vm, vm.cellButterflyStructure(CopyOnWriteArrayWithContiguous), length);
        if (!result) [[unlikely]] {
            throwOutOfMemoryError(globalObject, throwScope);
            return nullptr;
        }

        if (!length)
            return result;

        if (indexingType == ContiguousShape || indexingType == Int32Shape) {
            for (unsigned i = 0; i < length; i++) {
                JSValue value = array->butterfly()->contiguous().at(array, i).get();
                value = !!value ? value : jsUndefined();
                result->setIndex(vm, i, value);
            }
            return result;
        }

        if (indexingType == DoubleShape) {
            ASSERT(Options::allowDoubleShape());
            for (unsigned i = 0; i < length; i++) {
                double d = array->butterfly()->contiguousDouble().at(array, i);
                JSValue value = std::isnan(d) ? jsUndefined() : JSValue(JSValue::EncodeAsDouble, d);
                result->setIndex(vm, i, value);
            }
            return result;
        }

        for (unsigned i = 0; i < length; i++) {
            JSValue value = array->getDirectIndex(globalObject, i);
            if (!value) {
                // When we see a hole, we assume that it's safe to assume the get would have returned undefined.
                // We may still call into this function when !globalObject->isArrayIteratorProtocolFastAndNonObservable(),
                // however, if we do that, we ensure we're calling in with an array with all self properties between
                // [0, length).
                //
                // We may also call into this during OSR exit to materialize a phantom fixed array.
                // We may be creating a fixed array during OSR exit even after the iterator protocol changed.
                // But, when the phantom would have logically been created, the protocol hadn't been
                // changed. Therefore, it is sound to assume empty indices are jsUndefined().
                value = jsUndefined();
            }
            RETURN_IF_EXCEPTION(throwScope, nullptr);
            result->setIndex(vm, i, value);
        }
        return result;
    }

    static JSCellButterfly* createFromClonedArguments(JSGlobalObject*, ClonedArguments*);
    static JSCellButterfly* createFromDirectArguments(JSGlobalObject*, DirectArguments*);
    static JSCellButterfly* createFromScopedArguments(JSGlobalObject*, ScopedArguments*);
    static JSCellButterfly* createFromSet(JSGlobalObject*, JSSet*);
    static JSCellButterfly* createFromString(JSGlobalObject*, JSString*);
    static JSCellButterfly* tryCreateFromArgList(VM&, ArgList);

    unsigned publicLength() const { return m_header.publicLength(); }
    unsigned vectorLength() const { return m_header.vectorLength(); }
    unsigned length() const { return m_header.publicLength(); }

    Butterfly* toButterfly() const { return std::bit_cast<Butterfly*>(std::bit_cast<char*>(this) + offsetOfData()); }
    static JSCellButterfly* fromButterfly(Butterfly* butterfly) { return std::bit_cast<JSCellButterfly*>(std::bit_cast<char*>(butterfly) - offsetOfData()); }
    static bool isOnlyAtomStringsStructure(VM& vm, Butterfly* butterfly)
    {
        return fromButterfly(butterfly)->structure() == vm.cellButterflyOnlyAtomStringsStructure.get();
    }

    JSValue get(unsigned index) const
    {
        if (!hasDouble(indexingMode()))
            return toButterfly()->contiguous().at(this, index).get();
        double value = toButterfly()->contiguousDouble().at(this, index);
        // Holes are not supported yet.
        ASSERT(!std::isnan(value));
        return jsDoubleNumber(value);
    }

    DECLARE_VISIT_CHILDREN;

    void copyToArguments(JSGlobalObject*, JSValue* firstElementDest, unsigned offset, unsigned length);

    template<typename, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        // We allocate out of the JSValue gigacage as other code expects all butterflies to live there.
        return &vm.immutableButterflyAuxiliarySpace();
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
        return WTF::roundUpToMultipleOf<sizeof(WriteBarrier<Unknown>)>(sizeof(JSCellButterfly));
    }

    static constexpr ptrdiff_t offsetOfPublicLength()
    {
        return OBJECT_OFFSETOF(JSCellButterfly, m_header) + IndexingHeader::offsetOfPublicLength();
    }

    static constexpr ptrdiff_t offsetOfVectorLength()
    {
        return OBJECT_OFFSETOF(JSCellButterfly, m_header) + IndexingHeader::offsetOfVectorLength();
    }

    static Checked<size_t> allocationSize(Checked<size_t> numItems)
    {
        return offsetOfData() + numItems * sizeof(WriteBarrier<Unknown>);
    }

private:
    JSCellButterfly(VM& vm, Structure* structure, unsigned length)
        : Base(vm, structure)
    {
        m_header.setVectorLength(length);
        m_header.setPublicLength(length);
        if (hasContiguous(indexingType())) {
            for (unsigned index = 0; index < length; ++index)
                toButterfly()->contiguous().at(this, index).setStartingValue(JSValue());
        }
    }

    IndexingHeader m_header;
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
