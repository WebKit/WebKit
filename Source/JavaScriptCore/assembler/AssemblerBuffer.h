/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#if ENABLE(ASSEMBLER)

#include "ExecutableAllocator.h"
#include "JITCompilationEffort.h"
#include "stdint.h"
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#if CPU(ARM64E)
#include <wtf/PtrTag.h>
#endif
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/UnalignedAccess.h>

namespace JSC {
    class AssemblerData;

    typedef ThreadSpecific<AssemblerData, WTF::CanBeGCThread::True> ThreadSpecificAssemblerData;

    JS_EXPORT_PRIVATE ThreadSpecificAssemblerData& threadSpecificAssemblerData();
    JS_EXPORT_PRIVATE ThreadSpecificAssemblerData& threadSpecificAssemblerHashes();

    class LinkBuffer;

    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AssemblerData);

    struct AssemblerLabel {
        inline AssemblerLabel() { setOffset(std::numeric_limits<uint32_t>::max()); }
        inline AssemblerLabel(const AssemblerLabel& other) { setOffset(other.offset()); }
        inline AssemblerLabel(AssemblerLabel&& other) { setOffset(other.offset()); }
        inline explicit AssemblerLabel(uint32_t offset) { setOffset(offset); }

        AssemblerLabel& operator=(const AssemblerLabel& other) { setOffset(other.offset()); return *this; }
        AssemblerLabel& operator=(AssemblerLabel&& other) { setOffset(other.offset()); return *this; }

        bool isSet() const { return (offset() != std::numeric_limits<uint32_t>::max()); }

        inline AssemblerLabel labelAtOffset(int offset) const
        {
            return AssemblerLabel(this->offset() + offset);
        }

        bool operator==(const AssemblerLabel& other) const { return offset() == other.offset(); }

        inline uint32_t offset() const
        {
#if CPU(ARM64E)
            return static_cast<uint32_t>(untagInt(m_offset, bitwise_cast<PtrTag>(this)));
#else
            return m_offset;
#endif
        }

    private:
        inline void setOffset(uint32_t offset)
        {
#if CPU(ARM64E)
            m_offset = tagInt(static_cast<uint64_t>(offset), bitwise_cast<PtrTag>(this));
#else
            m_offset = offset;
#endif
        }

#if CPU(ARM64E)
        uint64_t m_offset;
#else
        uint32_t m_offset;
#endif
    };

    class AssemblerData {
        WTF_MAKE_NONCOPYABLE(AssemblerData);
        static constexpr size_t InlineCapacity = 128;
    public:
        AssemblerData()
            : m_buffer(m_inlineBuffer)
            , m_capacity(InlineCapacity)
        {
        }

        AssemblerData(size_t initialCapacity)
        {
            if (initialCapacity <= InlineCapacity) {
                m_capacity = InlineCapacity;
                m_buffer = m_inlineBuffer;
            } else {
                m_capacity = initialCapacity;
                m_buffer = static_cast<char*>(AssemblerDataMalloc::malloc(m_capacity));
            }
        }

        AssemblerData(AssemblerData&& other)
        {
            if (other.isInlineBuffer()) {
                ASSERT(other.m_capacity == InlineCapacity);
                memcpy(m_inlineBuffer, other.m_inlineBuffer, InlineCapacity);
                m_buffer = m_inlineBuffer;
            } else
                m_buffer = other.m_buffer;
            m_capacity = other.m_capacity;

            other.m_buffer = other.m_inlineBuffer;
            other.m_capacity = InlineCapacity;
        }

        AssemblerData& operator=(AssemblerData&& other)
        {
            if (m_buffer && !isInlineBuffer())
                AssemblerDataMalloc::free(m_buffer);

            if (other.isInlineBuffer()) {
                ASSERT(other.m_capacity == InlineCapacity);
                memcpy(m_inlineBuffer, other.m_inlineBuffer, InlineCapacity);
                m_buffer = m_inlineBuffer;
            } else
                m_buffer = other.m_buffer;
            m_capacity = other.m_capacity;

            other.m_buffer = other.m_inlineBuffer;
            other.m_capacity = InlineCapacity;
            return *this;
        }

        void takeBufferIfLarger(AssemblerData&& other)
        {
            if (other.isInlineBuffer())
                return;

            if (m_capacity >= other.m_capacity)
                return;

            if (m_buffer && !isInlineBuffer())
                AssemblerDataMalloc::free(m_buffer);

            m_buffer = other.m_buffer;
            m_capacity = other.m_capacity;

            other.m_buffer = other.m_inlineBuffer;
            other.m_capacity = InlineCapacity;
        }

        ~AssemblerData()
        {
            clear();
        }

        void clear()
        {
            if (m_buffer && !isInlineBuffer()) {
                AssemblerDataMalloc::free(m_buffer);
                m_capacity = InlineCapacity;
                m_buffer = m_inlineBuffer;
            }
        }

        char* buffer() const { return m_buffer; }

        unsigned capacity() const { return m_capacity; }

        void grow(unsigned extraCapacity = 0)
        {
            m_capacity = m_capacity + m_capacity / 2 + extraCapacity;
            if (isInlineBuffer()) {
                m_buffer = static_cast<char*>(AssemblerDataMalloc::malloc(m_capacity));
                memcpy(m_buffer, m_inlineBuffer, InlineCapacity);
            } else
                m_buffer = static_cast<char*>(AssemblerDataMalloc::realloc(m_buffer, m_capacity));
        }

    private:
        bool isInlineBuffer() const { return m_buffer == m_inlineBuffer; }
        char* m_buffer;
        char m_inlineBuffer[InlineCapacity];
        unsigned m_capacity;
    };

#if CPU(ARM64E)
#if PLATFORM(MAC)
    class ARM64EHash {
    public:
        ARM64EHash(void* initialHash)
            : m_hash(static_cast<uint32_t>(bitwise_cast<uintptr_t>(initialHash)))
        {
        }

        ALWAYS_INLINE uint32_t update(uint32_t value, uint32_t, void*)
        {
            uint64_t input = value ^ m_hash;
            uint64_t a = static_cast<uint32_t>(tagInt(input, static_cast<PtrTag>(0)) >> 39);
            uint64_t b = tagInt(input, static_cast<PtrTag>(0xb7e151628aed2a6a)) >> 23;
            m_hash = a ^ b;
            return m_hash;
        }

    private:
        uint32_t m_hash;
    };
#else
    class ARM64EHash {
    public:
        ARM64EHash(void* diversifier)
        {
            setUpdatedHash(0, 0, diversifier);
        }

        ALWAYS_INLINE uint32_t update(uint32_t instruction, uint32_t index, void* diversifier)
        {
            uint32_t currentHash = this->currentHash(index, diversifier);
            uint64_t nextIndex = index + 1;
            uint32_t output = nextValue(instruction, nextIndex, currentHash);
            setUpdatedHash(output, nextIndex, diversifier);
            return output;
        }

    private:
        static constexpr uint8_t initializationNamespace = 0x11;

        static ALWAYS_INLINE PtrTag makeDiversifier(uint8_t namespaceTag, uint64_t index, uint32_t value)
        {
            // <namespaceTag:8><index:24><value:32>
            return static_cast<PtrTag>((static_cast<uint64_t>(namespaceTag) << 56) + ((index & 0xFFFFFF) << 32) + value);
        }

        static ALWAYS_INLINE uint32_t nextValue(uint64_t instruction, uint64_t index, uint32_t currentValue)
        {
            uint64_t a = tagInt(instruction, makeDiversifier(0x12, index, currentValue));
            uint64_t b = tagInt(instruction, makeDiversifier(0x13, index, currentValue));
            return (a >> 39) ^ (b >> 23);
        }

        static ALWAYS_INLINE uint32_t bitsForDiversifier(void* diversifier)
        {
            return bitwise_cast<uintptr_t>(diversifier);
        }

        ALWAYS_INLINE uint32_t currentHash(uint32_t index, void* diversifier)
        {
            bool hashFieldIsTagged = index == 0;
            if (hashFieldIsTagged)
                return untagInt(m_hash, makeDiversifier(initializationNamespace, index, bitsForDiversifier(diversifier)));
            return m_hash;
        }

        ALWAYS_INLINE void setUpdatedHash(uint32_t value, uint32_t index, void* diversifier)
        {
            bool shouldTagHashField = index == 0;
            if (shouldTagHashField)
                m_hash = tagInt(static_cast<uint64_t>(value), makeDiversifier(initializationNamespace, index, bitsForDiversifier(diversifier)));
            else
                m_hash = value;
        }

        uint64_t m_hash;
    };
#endif // PLATFORM(MAC)
#endif // CPU(ARM64E)

    class AssemblerBuffer {
    public:
        AssemblerBuffer()
            : m_storage()
            , m_index(0)
#if CPU(ARM64E)
            , m_hash(this)
            , m_hashes()
#endif
        {
            auto& threadSpecificData = threadSpecificAssemblerData();
            m_storage.takeBufferIfLarger(WTFMove(*threadSpecificData));
#if CPU(ARM64E)
            auto& threadSpecificHashes = threadSpecificAssemblerHashes();
            m_hashes.takeBufferIfLarger(WTFMove(*threadSpecificHashes));
            ASSERT(m_storage.capacity() == m_hashes.capacity());
#endif
        }

        ~AssemblerBuffer()
        {
#if CPU(ARM64E)
            ASSERT(m_storage.capacity() == m_hashes.capacity());
            auto& threadSpecificHashes = threadSpecificAssemblerHashes();
            threadSpecificHashes->takeBufferIfLarger(WTFMove(m_hashes));
#endif
            auto& threadSpecificData = threadSpecificAssemblerData();
            threadSpecificData->takeBufferIfLarger(WTFMove(m_storage));
        }

        bool isAvailable(unsigned space)
        {
            return m_index + space <= m_storage.capacity();
        }

        void ensureSpace(unsigned space)
        {
            while (!isAvailable(space))
                outOfLineGrow();
        }

        bool isAligned(int alignment) const
        {
            return !(m_index & (alignment - 1));
        }

#if !CPU(ARM64)
        void putByteUnchecked(int8_t value) { putIntegralUnchecked(value); }
        void putByte(int8_t value) { putIntegral(value); }
        void putShortUnchecked(int16_t value) { putIntegralUnchecked(value); }
        void putShort(int16_t value) { putIntegral(value); }
        void putInt64Unchecked(int64_t value) { putIntegralUnchecked(value); }
        void putInt64(int64_t value) { putIntegral(value); }
#endif
        void putIntUnchecked(int32_t value) { putIntegralUnchecked(value); }
        void putInt(int32_t value) { putIntegral(value); }

        size_t codeSize() const
        {
            return m_index;
        }

#if !CPU(ARM64)
        void setCodeSize(size_t index)
        {
            // Warning: Only use this if you know exactly what you are doing.
            // For example, say you want 40 bytes of nops, it's ok to grow
            // and then fill 40 bytes of nops using bigger instructions.
            m_index = index;
            ASSERT(m_index <= m_storage.capacity());
        }
#endif

        AssemblerLabel label() const
        {
            return AssemblerLabel(m_index);
        }

        unsigned debugOffset() { return m_index; }

        AssemblerData&& releaseAssemblerData()
        {
            return WTFMove(m_storage);
        }

#if CPU(ARM64E)
        AssemblerData&& releaseAssemblerHashes()
        {
            return WTFMove(m_hashes);
        }
#endif

        // LocalWriter is a trick to keep the storage buffer and the index
        // in memory while issuing multiple Stores.
        // It is created in a block scope and its attribute can stay live
        // between writes.
        //
        // LocalWriter *CANNOT* be mixed with other types of access to AssemblerBuffer.
        // AssemblerBuffer cannot be used until its LocalWriter goes out of scope.
#if !CPU(ARM64) // If we ever need to use this on arm64e, we would need to make the checksum aware of this.
        class LocalWriter {
        public:
            LocalWriter(AssemblerBuffer& buffer, unsigned requiredSpace)
                : m_buffer(buffer)
            {
                buffer.ensureSpace(requiredSpace);
                m_storageBuffer = buffer.m_storage.buffer();
                m_index = buffer.m_index;
#if ASSERT_ENABLED
                m_initialIndex = m_index;
                m_requiredSpace = requiredSpace;
#endif
            }

            ~LocalWriter()
            {
                ASSERT(m_index - m_initialIndex <= m_requiredSpace);
                ASSERT(m_buffer.m_index == m_initialIndex);
                ASSERT(m_storageBuffer == m_buffer.m_storage.buffer());
                m_buffer.m_index = m_index;
            }

            void putByteUnchecked(int8_t value) { putIntegralUnchecked(value); }
            void putShortUnchecked(int16_t value) { putIntegralUnchecked(value); }
            void putIntUnchecked(int32_t value) { putIntegralUnchecked(value); }
            void putInt64Unchecked(int64_t value) { putIntegralUnchecked(value); }
        private:
            template<typename IntegralType>
            void putIntegralUnchecked(IntegralType value)
            {
                ASSERT(m_index + sizeof(IntegralType) <= m_buffer.m_storage.capacity());
                WTF::unalignedStore<IntegralType>(m_storageBuffer + m_index, value);
                m_index += sizeof(IntegralType);
            }
            AssemblerBuffer& m_buffer;
            char* m_storageBuffer;
            unsigned m_index;
#if ASSERT_ENABLED
            unsigned m_initialIndex;
            unsigned m_requiredSpace;
#endif
        };
#endif // !CPU(ARM64)

#if !CPU(ARM64) // If we were to define this on arm64e, we'd need a way to update the hash as we write directly into the buffer.
        void* data() const { return m_storage.buffer(); }
#endif

    protected:
        template<typename IntegralType>
        void putIntegral(IntegralType value)
        {
            unsigned nextIndex = m_index + sizeof(IntegralType);
            if (UNLIKELY(nextIndex > m_storage.capacity()))
                outOfLineGrow();
            putIntegralUnchecked<IntegralType>(value);
        }

        template<typename IntegralType>
        void putIntegralUnchecked(IntegralType value)
        {
#if CPU(ARM64)
            static_assert(sizeof(value) == 4, "");
#if CPU(ARM64E)
            uint32_t hash = m_hash.update(value, m_index / sizeof(IntegralType), this);
            WTF::unalignedStore<uint32_t>(m_hashes.buffer() + m_index, hash);
#endif
#endif
            ASSERT(isAvailable(sizeof(IntegralType)));
            WTF::unalignedStore<IntegralType>(m_storage.buffer() + m_index, value);
            m_index += sizeof(IntegralType);
        }

    private:
        void grow(int extraCapacity = 0)
        {
            m_storage.grow(extraCapacity);
#if CPU(ARM64E)
            m_hashes.grow(extraCapacity);
#endif
        }

        NEVER_INLINE void outOfLineGrow()
        {
            m_storage.grow();
#if CPU(ARM64E)
            m_hashes.grow();
#endif
        }

#if !CPU(ARM64)
        friend LocalWriter;
#endif
        friend LinkBuffer;

        AssemblerData m_storage;
        unsigned m_index;
#if CPU(ARM64E)
        ARM64EHash m_hash;
        AssemblerData m_hashes;
#endif
    };

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
