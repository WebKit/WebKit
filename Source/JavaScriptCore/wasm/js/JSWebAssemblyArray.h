/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmFormat.h"
#include "WasmLimits.h"
#include "WasmOps.h"
#include "WasmTypeDefinition.h"
#include "WebAssemblyGCObjectBase.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

// Ideally this would just subclass TrailingArray<JSWebAssemblyArray, uint8_t> but we need the m_size field to be in units
// of element size rather than byte size.
class JSWebAssemblyArray final : public WebAssemblyGCObjectBase {
public:
    using Base = WebAssemblyGCObjectBase;

    template<typename CellType, SubspaceAccess mode>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.heap.cellSpace;
    }

    DECLARE_INFO;

    static inline TypeInfoBlob typeInfoBlob();
    static inline WebAssemblyGCStructure* createStructure(VM&, JSGlobalObject*, Ref<const Wasm::TypeDefinition>&&, Ref<const Wasm::RTT>&&);

    static JSWebAssemblyArray* tryCreate(VM& vm, WebAssemblyGCStructure* structure, unsigned size);

    DECLARE_VISIT_CHILDREN;

    static Wasm::FieldType elementType(const WebAssemblyGCStructure* structure) { return structure->typeDefinition().as<Wasm::ArrayType>()->elementType(); }
    Wasm::FieldType elementType() const { return elementType(gcStructure()); }
    static bool needsAlignmentCheck(Wasm::StorageType type) { return type.unpacked().isV128(); }
    size_t size() const { return m_size; }
    size_t sizeInBytes() const { return size() * elementType().type.elementSize(); }

    template<typename T> inline std::span<T> span() LIFETIME_BOUND;

    template<typename T>
    std::span<const T> span() const LIFETIME_BOUND { return const_cast<JSWebAssemblyArray*>(this)->span<T>(); }

    bool elementsAreRefTypes() const
    {
        return Wasm::isRefType(elementType().type.unpacked());
    }

    inline std::span<uint64_t> refTypeSpan() LIFETIME_BOUND;

    ALWAYS_INLINE auto visitSpan(auto functor);
    ALWAYS_INLINE auto visitSpanNonVector(auto functor);

    inline uint64_t get(uint32_t index);
    inline v128_t getVector(uint32_t index);
    inline void set(VM&, uint32_t index, uint64_t value);
    inline void set(VM&, uint32_t index, v128_t value);

    void fill(VM&, uint32_t, uint64_t, uint32_t);
    void fill(VM&, uint32_t, v128_t, uint32_t);
    void copy(VM&, JSWebAssemblyArray&, uint32_t, uint32_t, uint32_t);

#if ASSERT_ENABLED
    // 'isUnpopulated' is a flag used by the 'array.new_elem' instruction implementation to indicate that the array contains nulls that have not yet been replaced
    // with the expected elements. Validation should be skipped for this array because these transient nulls may disagree with the declared element type.
    // NOTE: the caller should use a memory fence to order the store of the flag relative to the prior stores into the array.
    void setIsUnpopulated(bool value) { m_isUnpopulated = value; }
#endif

    // We add 8 bytes for v128 arrays since a non-PreciseAllocation will have the wrong alignment as the base pointer for a PreciseAllocation is shifted by 8.
    // Note: Technically this isn't needed since the GC/malloc always allocates 16 byte chunks so for non-precise v128 allocations
    // there will be a 8 spare bytes at the end. This is just a bit more explicit and shouldn't make a difference.
    static constexpr ptrdiff_t v128AlignmentShift = 8;
    static std::optional<unsigned> allocationSizeInBytes(Wasm::FieldType fieldType, unsigned size)
    {
        unsigned elementSize = fieldType.type.elementSize();
        if (productOverflows<uint32_t>(elementSize, size) || elementSize * size > Wasm::maxArraySizeInBytes) [[unlikely]]
            return std::nullopt;
        return sizeof(JSWebAssemblyArray) + size * elementSize + static_cast<size_t>(needsAlignmentCheck(fieldType.type) * v128AlignmentShift);
    }

    static constexpr ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(JSWebAssemblyArray, m_size); }
    static constexpr ptrdiff_t offsetOfData() { return sizeof(JSWebAssemblyArray); }

private:
    friend class LLIntOffsetsExtractor;
    inline std::span<uint8_t> bytes();

    // NB: It's *HIGHLY* recommended that you don't use these directly since you'll have to remember to clean up the alignment for v128.
    uint8_t* data() LIFETIME_BOUND { return reinterpret_cast<uint8_t*>(this) + offsetOfData(); }
    const uint8_t* data() const LIFETIME_BOUND { return const_cast<JSWebAssemblyArray*>(this)->data(); }

    JSWebAssemblyArray(VM&, WebAssemblyGCStructure*, unsigned);

    DECLARE_DEFAULT_FINISH_CREATION;

    unsigned m_size;

#if ASSERT_ENABLED
    bool m_isUnpopulated { false };
#endif
};

static_assert(std::is_final_v<JSWebAssemblyArray>, "JSWebAssemblyArray is a TrailingArray-like object so must know about all members");
// We still have to check for PreciseAllocations since those are correctly aligned for v128 but this asserts our shifted offset will be correct.
// FIXME: Fix this check for 32-bit.
static_assert(isAddress32Bit() || !((JSWebAssemblyArray::offsetOfData()) % 16), "JSWebAssemblyArray storage needs to be aligned for v128_t");

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
