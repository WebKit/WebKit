/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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
#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/UnalignedAccess.h>

#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER) && CPU(ARM64E)
#include "SecureARM64EHashPinsInlines.h"
#include <wtf/PtrTag.h>
#endif

namespace JSC {
    enum class AssemblerDataType : uint8_t { Code, Hashes };
    template<AssemblerDataType>
    class AssemblerDataImpl;

    using AssemblerData = AssemblerDataImpl<AssemblerDataType::Code>;
    using ThreadSpecificAssemblerData = ThreadSpecific<AssemblerData, WTF::CanBeGCThread::True>;
    JS_EXPORT_PRIVATE ThreadSpecificAssemblerData& threadSpecificAssemblerData();

#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
    using AssemblerHashes = AssemblerDataImpl<AssemblerDataType::Hashes>;
    using ThreadSpecificAssemblerHashes = ThreadSpecific<AssemblerHashes, WTF::CanBeGCThread::True>;
    JS_EXPORT_PRIVATE ThreadSpecificAssemblerHashes& threadSpecificAssemblerHashes();
#endif

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
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER) && CPU(ARM64E)
            return static_cast<uint32_t>(untagInt(m_offset, bitwise_cast<PtrTag>(this)));
#else
            return m_offset;
#endif
        }

    private:
        inline void setOffset(uint32_t offset)
        {
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER) && CPU(ARM64E)
            m_offset = tagInt(static_cast<uint64_t>(offset), bitwise_cast<PtrTag>(this));
#else
            m_offset = offset;
#endif
        }

#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER) && CPU(ARM64E)
        uint64_t m_offset;
#else
        uint32_t m_offset;
#endif
    };

    template<AssemblerDataType type>
    class AssemblerDataImpl {
        WTF_MAKE_NONCOPYABLE(AssemblerDataImpl);
        static constexpr size_t InlineCapacity = 128;
    public:
        AssemblerDataImpl()
            : m_buffer(m_inlineBuffer)
            , m_capacity(InlineCapacity)
        {
            if constexpr (type == AssemblerDataType::Code)
                takeBufferIfLarger(*threadSpecificAssemblerData());
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
            if constexpr (type == AssemblerDataType::Hashes)
                takeBufferIfLarger(*threadSpecificAssemblerHashes());
#else
            static_assert(type != AssemblerDataType::Hashes);
#endif
        }

        AssemblerDataImpl(AssemblerDataImpl&& other)
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

        AssemblerDataImpl& operator=(AssemblerDataImpl&& other)
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

        void takeBufferIfLarger(AssemblerDataImpl& other)
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

        ~AssemblerDataImpl()
        {
            if constexpr (type == AssemblerDataType::Code)
                threadSpecificAssemblerData()->takeBufferIfLarger(*this);
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
            if constexpr (type == AssemblerDataType::Hashes)
                threadSpecificAssemblerHashes()->takeBufferIfLarger(*this);
#else
            static_assert(type != AssemblerDataType::Hashes);
#endif
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

#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
    enum class ShouldSign : bool { No, Yes };
    enum class HashType { Insecure, Pac };

#if CPU(ARM64E)
    template <ShouldSign shouldSign, HashType hashType = HashType::Pac>
#else
    template <ShouldSign shouldSign, HashType hashType = HashType::Insecure>
#endif
    class BufferChecksum;

    template <ShouldSign shouldSign>
    class BufferChecksum<shouldSign, HashType::Pac> {
        WTF_MAKE_NONCOPYABLE(BufferChecksum);
    public:
        BufferChecksum()
        {
            allocatePinForCurrentThreadAndInitializeHash();
        }

        ~BufferChecksum()
        {
            deallocatePinForCurrentThread();
        }

        ALWAYS_INLINE uint32_t update(uint32_t instruction, uint32_t byteIdx)
        {
            uint32_t instrIdx = byteIdx / sizeof(instruction);
            uint32_t currentHash = this->currentHash(instrIdx);
            uint64_t nextIdx = instrIdx + 1;
            uint32_t output = nextWordHash(instruction, nextIdx, currentHash);
            setUpdatedHash(output, nextIdx);
            return output;
        }

        void earlyCleanup()
        {
            deallocatePinForCurrentThread();
        }

    private:
        static constexpr uint8_t initializationNamespace = 0x11;

        ALWAYS_INLINE void allocatePinForCurrentThreadAndInitializeHash()
        {
            if constexpr (shouldSign == ShouldSign::Yes) {
                m_initializedPin = true;
#if CPU(ARM64E)
                g_jscConfig.arm64eHashPins.allocatePinForCurrentThread();
#endif
                setUpdatedHash(0, 0);
            } else
                m_hash = 0;
        }

        void deallocatePinForCurrentThread()
        {
            if (m_initializedPin) {
#if CPU(ARM64E)
                g_jscConfig.arm64eHashPins.deallocatePinForCurrentThread();
#endif
                m_initializedPin = false;
            }
        }

        static ALWAYS_INLINE PtrTag makeDiversifier(uint8_t namespaceTag, uint64_t instrIdx, uint32_t value)
        {
            // <namespaceTag:8><instrIdx:24><value:32>
            return static_cast<PtrTag>((static_cast<uint64_t>(namespaceTag) << 56) + ((instrIdx & 0xFFFFFF) << 32) + value);
        }

        static ALWAYS_INLINE uint32_t nextWordHash(uint64_t instruction, uint64_t instrIdx, uint32_t currentValue)
        {
            uint64_t a = tagInt<PACKeyType::ProcessIndependent>(instruction, makeDiversifier(0x12, instrIdx, currentValue));
            uint64_t b = tagInt<PACKeyType::ProcessIndependent>(instruction, makeDiversifier(0x13, instrIdx, currentValue));
            return (a >> 39) ^ (b >> 23);
        }

        static ALWAYS_INLINE uint32_t pin()
        {
#if CPU(ARM64E)
            return g_jscConfig.arm64eHashPins.pinForCurrentThread();
#else
            return 1;
#endif
        }

        ALWAYS_INLINE uint32_t currentHash(uint32_t instrIdx)
        {
            if constexpr (shouldSign == ShouldSign::Yes)
                return untagInt<PACKeyType::ProcessIndependent>(m_hash, makeDiversifier(initializationNamespace, instrIdx, pin()));
            return m_hash;
        }

        ALWAYS_INLINE void setUpdatedHash(uint32_t value, uint32_t instrIdx)
        {
            if constexpr (shouldSign == ShouldSign::Yes)
                m_hash = tagInt<PACKeyType::ProcessIndependent>(static_cast<uint64_t>(value), makeDiversifier(initializationNamespace, instrIdx, pin()));
            else
                m_hash = static_cast<uint64_t>(value);
        }

        uint64_t m_hash;
        bool m_initializedPin { false };
    };

    template <ShouldSign shouldSign>
    class BufferChecksum<shouldSign, HashType::Insecure> {
        WTF_MAKE_NONCOPYABLE(BufferChecksum);
    public:
        BufferChecksum() = default;

        template<typename IntegralType>
        ALWAYS_INLINE IntegralType update(IntegralType instruction, uint32_t byteIdx)
        {
            IntegralType result { 0 };
            for (size_t i = 0; i < sizeof(instruction); i++) {
                uint8_t currentInstrByte = (instruction >> (8 * i)) & 0xFF;

                byteIdx++;
                m_hash = nextByteHash(currentInstrByte, byteIdx, m_hash);
                result = (result << 8) | m_hash;
            }
            return result;
        }

        void earlyCleanup() { }

    private:
        static ALWAYS_INLINE uint8_t nextByteHash(uint8_t byte, uint32_t byteIdx, uint8_t currentValue)
        {
            // ignore top 16b of index
            uint8_t idx_hash = (byteIdx ^ (byteIdx >> 8)) & 0xFF;
            return currentValue ^ byte ^ idx_hash;
        }

        uint8_t m_hash { 0 };
    };
#endif // ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)

    class AssemblerBuffer {
    public:
        AssemblerBuffer()
            : m_storage()
            , m_index(0)
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
            , m_hash()
            , m_hashes()
#endif
        {
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
            ASSERT(m_storage.capacity() == m_hashes.capacity());
#endif
        }

        ~AssemblerBuffer()
        {
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
            ASSERT(m_storage.capacity() == m_hashes.capacity());
#endif
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

#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
        AssemblerHashes&& releaseAssemblerHashes()
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

#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
        BufferChecksum<ShouldSign::Yes>& checksum() { return m_hash; }
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
            static_assert(sizeof(value) == 4);
#endif
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
            auto hash = m_hash.update(value, m_index);
            WTF::unalignedStore<decltype(hash)>(m_hashes.buffer() + m_index, hash);
#endif
            ASSERT(isAvailable(sizeof(IntegralType)));
            WTF::unalignedStore<IntegralType>(m_storage.buffer() + m_index, value);
            m_index += sizeof(IntegralType);
        }

    private:
        void grow(int extraCapacity = 0)
        {
            m_storage.grow(extraCapacity);
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
            m_hashes.grow(extraCapacity);
#endif
        }

        NEVER_INLINE void outOfLineGrow()
        {
            m_storage.grow();
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
            m_hashes.grow();
#endif
        }

#if !CPU(ARM64)
        friend LocalWriter;
#endif
        friend LinkBuffer;

        AssemblerData m_storage;
        unsigned m_index;
#if ENABLE(JIT_CHECKSUM_ASSEMBLER_BUFFER)
        BufferChecksum<ShouldSign::Yes> m_hash;
        AssemblerHashes m_hashes;
#endif
    };

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
