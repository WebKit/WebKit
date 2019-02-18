/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CachedTypes.h"

#include "BytecodeLivenessAnalysis.h"
#include "JSCast.h"
#include "JSImmutableButterfly.h"
#include "JSTemplateObjectDescriptor.h"
#include "ScopedArgumentsTable.h"
#include "SourceCodeKey.h"
#include "UnlinkedEvalCodeBlock.h"
#include "UnlinkedFunctionCodeBlock.h"
#include "UnlinkedMetadataTableInlines.h"
#include "UnlinkedModuleProgramCodeBlock.h"
#include "UnlinkedProgramCodeBlock.h"
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/text/AtomicStringImpl.h>

namespace JSC {

template <typename T, typename = void>
struct SourceTypeImpl {
    using type = T;
};

template<typename T>
struct SourceTypeImpl<T, std::enable_if_t<!std::is_fundamental<T>::value && !std::is_same<typename T::SourceType_, void>::value>> {
    using type = typename T::SourceType_;

};

template<typename T>
using SourceType = typename SourceTypeImpl<T>::type;

class Encoder {
    WTF_MAKE_NONCOPYABLE(Encoder);
    WTF_FORBID_HEAP_ALLOCATION;

public:
    class Allocation {
        friend class Encoder;

    public:
        uint8_t* buffer() const { return m_buffer; }
        ptrdiff_t offset() const { return m_offset; }

    private:
        Allocation(uint8_t* buffer, ptrdiff_t offset)
            : m_buffer(buffer)
            , m_offset(offset)
        {
        }

        uint8_t* m_buffer;
        ptrdiff_t m_offset;
    };

    Encoder(VM& vm)
        : m_vm(vm)
        , m_baseOffset(0)
        , m_currentPage(nullptr)
    {
        allocateNewPage();
    }

    VM& vm() { return m_vm; }

    Allocation malloc(unsigned size)
    {
        ASSERT(size);
        ptrdiff_t offset;
        if (m_currentPage->malloc(size, offset))
            return Allocation { m_currentPage->buffer() + offset, m_baseOffset + offset };
        allocateNewPage(size);
        return malloc(size);
    }

    template<typename T>
    T* malloc()
    {
        return reinterpret_cast<T*>(malloc(sizeof(T)).buffer());
    }

    ptrdiff_t offsetOf(const void* address)
    {
        ptrdiff_t offset;
        ptrdiff_t baseOffset = 0;
        for (const auto& page : m_pages) {
            if (page.getOffset(address, offset))
                return baseOffset + offset;
            baseOffset += page.size();
        }
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }

    void cachePtr(const void* ptr, ptrdiff_t offset)
    {
        m_ptrToOffsetMap.add(ptr, offset);
    }

    WTF::Optional<ptrdiff_t> offsetForPtr(const void* ptr)
    {
        auto it = m_ptrToOffsetMap.find(ptr);
        if (it == m_ptrToOffsetMap.end())
            return WTF::nullopt;
        return { it->value };
    }

    std::pair<MallocPtr<uint8_t>, size_t> release()
    {
        size_t size = m_baseOffset + m_currentPage->size();
        MallocPtr<uint8_t> buffer = MallocPtr<uint8_t>::malloc(size);
        unsigned offset = 0;
        for (const auto& page : m_pages) {
            memcpy(buffer.get() + offset, page.buffer(), page.size());
            offset += page.size();
        }
        RELEASE_ASSERT(offset == size);
        return { WTFMove(buffer), size };
    }

private:
    class Page {
    public:
        Page(size_t size)
            : m_offset(0)
            , m_capacity(size)
        {
            m_buffer = MallocPtr<uint8_t>::malloc(size);
        }

        bool malloc(size_t size, ptrdiff_t& result)
        {
            size_t alignment = std::min(alignof(std::max_align_t), static_cast<size_t>(WTF::roundUpToPowerOfTwo(size)));
            ptrdiff_t offset = WTF::roundUpToMultipleOf(alignment, m_offset);
            size = WTF::roundUpToMultipleOf(alignment, size);
            if (static_cast<size_t>(offset + size) > m_capacity)
                return false;

            result = offset;
            m_offset = offset + size;
            return true;
        }

        uint8_t* buffer() const { return m_buffer.get(); }
        size_t size() const { return static_cast<size_t>(m_offset); }

        bool getOffset(const void* address, ptrdiff_t& result) const
        {
            const uint8_t* addr = static_cast<const uint8_t*>(address);
            if (addr >= m_buffer.get() && addr < m_buffer.get() + m_offset) {
                result = addr - m_buffer.get();
                return true;
            }
            return false;
        }

    private:
        MallocPtr<uint8_t> m_buffer;
        ptrdiff_t m_offset;
        size_t m_capacity;
    };

    void allocateNewPage(size_t size = 0)
    {
        static size_t minPageSize = WTF::pageSize();
        if (m_currentPage)
            m_baseOffset += m_currentPage->size();
        if (size < minPageSize)
            size = minPageSize;
        else
            size = WTF::roundUpToMultipleOf(minPageSize, size);
        m_pages.append(Page { size });
        m_currentPage = &m_pages.last();
    }

    VM& m_vm;
    ptrdiff_t m_baseOffset;
    Page* m_currentPage;
    Vector<Page> m_pages;
    HashMap<const void*, ptrdiff_t> m_ptrToOffsetMap;
};

class Decoder {
    WTF_MAKE_NONCOPYABLE(Decoder);
    WTF_FORBID_HEAP_ALLOCATION;

public:
    Decoder(VM& vm, const void* baseAddress, size_t size)
        : m_vm(vm)
        , m_baseAddress(reinterpret_cast<const uint8_t*>(baseAddress))
#ifndef NDEBUG
        , m_size(size)
#endif
    {
        UNUSED_PARAM(size);
    }

    ~Decoder()
    {
        for (auto& pair : m_finalizers)
            pair.value();
    }

    VM& vm() { return m_vm; }

    ptrdiff_t offsetOf(const void* ptr)
    {
        const uint8_t* addr = static_cast<const uint8_t*>(ptr);
        ASSERT(addr >= m_baseAddress && addr < m_baseAddress + m_size);
        return addr - m_baseAddress;
    }

    void cacheOffset(ptrdiff_t offset, void* ptr)
    {
        m_offsetToPtrMap.add(offset, ptr);
    }

    WTF::Optional<void*> ptrForOffset(ptrdiff_t offset)
    {
        auto it = m_offsetToPtrMap.find(offset);
        if (it == m_offsetToPtrMap.end())
            return WTF::nullopt;
        return { it->value };
    }

    template<typename Functor>
    void addFinalizer(ptrdiff_t offset, const Functor& fn)
    {
        m_finalizers.add(offset, fn);
    }

private:
    VM& m_vm;
    const uint8_t* m_baseAddress;
#ifndef NDEBUG
    size_t m_size;
#endif
    HashMap<ptrdiff_t, void*> m_offsetToPtrMap;
    HashMap<ptrdiff_t, std::function<void()>> m_finalizers;
};

template<typename T>
static std::enable_if_t<std::is_same<T, SourceType<T>>::value> encode(Encoder&, T& dst, const SourceType<T>& src)
{
    dst = src;
}

template<typename T>
static std::enable_if_t<!std::is_same<T, SourceType<T>>::value> encode(Encoder& encoder, T& dst, const SourceType<T>& src)
{
    dst.encode(encoder, src);
}

template<typename T, typename... Args>
static std::enable_if_t<std::is_same<T, SourceType<T>>::value> decode(Decoder&, const T& src, SourceType<T>& dst, Args...)
{
    dst = src;
}

template<typename T, typename... Args>
static std::enable_if_t<!std::is_same<T, SourceType<T>>::value> decode(Decoder& decoder, const T& src, SourceType<T>& dst, Args... args)
{
    src.decode(decoder, dst, args...);
}

template<typename T>
static std::enable_if_t<std::is_same<T, SourceType<T>>::value, T> decode(Decoder&, T src)
{
    return src;
}

template<typename T>
static std::enable_if_t<!std::is_same<T, SourceType<T>>::value, SourceType<T>>&& decode(Decoder& decoder, const T& src)
{
    return src.decode(decoder);
}

template<typename Source>
class CachedObject {
    WTF_MAKE_NONCOPYABLE(CachedObject<Source>);

public:
    using SourceType_ = Source;

    CachedObject() = default;

    inline void* operator new(size_t, void* where) { return where; }

    // Copied from WTF_FORBID_HEAP_ALLOCATION, since we only want to allow placement new
    void* operator new[](size_t, void*) = delete;
    void* operator new(size_t) = delete;
    void operator delete(void*) = delete;
    void* operator new[](size_t size) = delete;
    void operator delete[](void*) = delete;
    void* operator new(size_t, NotNullTag, void* location) = delete;
};

template<typename Source>
class VariableLengthObject : public CachedObject<Source> {
    template<typename, typename>
    friend struct CachedPtr;

protected:
    const uint8_t* buffer() const
    {
        ASSERT(m_offset != s_invalidOffset);
        return reinterpret_cast<const uint8_t*>(this) + m_offset;
    }

    template<typename T>
    const T* buffer() const
    {
        return reinterpret_cast<const T*>(buffer());
    }

    uint8_t* allocate(Encoder& encoder, size_t size)
    {
        ptrdiff_t offsetOffset = encoder.offsetOf(&m_offset);
        auto result = encoder.malloc(size);
        m_offset = result.offset() - offsetOffset;
        return result.buffer();
    }

    template<typename T>
    T* allocate(Encoder& encoder, unsigned size = 1)
    {
        uint8_t* result = allocate(encoder, sizeof(T) * size);
        return new (result) T();
    }

private:
    constexpr static ptrdiff_t s_invalidOffset = std::numeric_limits<ptrdiff_t>::max();

    ptrdiff_t m_offset { s_invalidOffset };

};

template<typename T, typename Source = SourceType<T>>
class CachedPtr : public VariableLengthObject<Source*> {
    template<typename, typename>
    friend struct CachedRefPtr;

public:
    void encode(Encoder& encoder, const Source* src)
    {
        m_isEmpty = !src;
        if (m_isEmpty)
            return;

        if (WTF::Optional<ptrdiff_t> offset = encoder.offsetForPtr(src)) {
            this->m_offset = *offset - encoder.offsetOf(&this->m_offset);
            return;
        }

        T* cachedObject = this->template allocate<T>(encoder);
        cachedObject->encode(encoder, *src);
        encoder.cachePtr(src, encoder.offsetOf(cachedObject));
    }

    template<typename... Args>
    Source* decode(Decoder& decoder, Args... args) const
    {
        if (m_isEmpty)
            return nullptr;

        ptrdiff_t bufferOffset = decoder.offsetOf(this->buffer());
        if (WTF::Optional<void*> ptr = decoder.ptrForOffset(bufferOffset))
            return reinterpret_cast<Source*>(*ptr);

        Source* ptr = get()->decode(decoder, args...);
        decoder.cacheOffset(bufferOffset, ptr);
        return ptr;
    }

    const T* operator->() const { return get(); }

private:
    const T* get() const
    {
        if (m_isEmpty)
            return nullptr;
        return this->template buffer<T>();
    }

    bool m_isEmpty;

};

template<typename T, typename Source = SourceType<T>>
class CachedRefPtr : public CachedObject<RefPtr<Source>> {
public:
    void encode(Encoder& encoder, const Source* src)
    {
        m_ptr.encode(encoder, src);
    }

    void encode(Encoder& encoder, const RefPtr<Source> src)
    {
        encode(encoder, src.get());
    }

    RefPtr<Source> decode(Decoder& decoder) const
    {
        Source* decodedPtr = m_ptr.decode(decoder);
        if (!decodedPtr)
            return nullptr;
        decoder.addFinalizer(decoder.offsetOf(m_ptr.buffer()), [=] { derefIfNotNull(decodedPtr); });
        refIfNotNull(decodedPtr);
        return adoptRef(decodedPtr);
    }

    void decode(Decoder& decoder, RefPtr<Source>& src) const
    {
        src = decode(decoder);
    }

private:
    CachedPtr<T, Source> m_ptr;
};

template<typename T, typename Source = SourceType<T>>
class CachedWriteBarrier : public CachedObject<WriteBarrier<Source>> {
public:
    void encode(Encoder& encoder, const WriteBarrier<Source> src)
    {
        m_ptr.encode(encoder, src.get());
    }

    void decode(Decoder& decoder, WriteBarrier<Source>& src, const JSCell* owner) const
    {
        Source* decodedPtr = m_ptr.decode(decoder);
        if (decodedPtr)
            src.set(decoder.vm(), owner, decodedPtr);
    }

private:
    CachedPtr<T, Source> m_ptr;
};

template<typename T, size_t InlineCapacity = 0, typename OverflowHandler = CrashOnOverflow>
class CachedVector : public VariableLengthObject<Vector<SourceType<T>, InlineCapacity, OverflowHandler>> {
public:
    void encode(Encoder& encoder, const Vector<SourceType<T>, InlineCapacity, OverflowHandler>& vector)
    {
        m_size = vector.size();
        if (!m_size)
            return;
        T* buffer = this->template allocate<T>(encoder, m_size);
        for (unsigned i = 0; i < m_size; ++i)
            ::JSC::encode(encoder, buffer[i], vector[i]);
    }

    template<typename... Args>
    void decode(Decoder& decoder, Vector<SourceType<T>, InlineCapacity, OverflowHandler>& vector, Args... args) const
    {
        if (!m_size)
            return;
        vector.resizeToFit(m_size);
        const T* buffer = this->template buffer<T>();
        for (unsigned i = 0; i < m_size; ++i)
            ::JSC::decode(decoder, buffer[i], vector[i], args...);
    }

private:
    unsigned m_size;
};

template<typename First, typename Second>
class CachedPair : public CachedObject<std::pair<SourceType<First>, SourceType<Second>>> {
public:
    void encode(Encoder& encoder, const std::pair<SourceType<First>, SourceType<Second>>& pair)
    {
        ::JSC::encode(encoder, m_first, pair.first);
        ::JSC::encode(encoder, m_second, pair.second);
    }

    void decode(Decoder& decoder, std::pair<SourceType<First>, SourceType<Second>>& pair) const
    {
        ::JSC::decode(decoder, m_first, pair.first);
        ::JSC::decode(decoder, m_second, pair.second);
    }

private:
    First m_first;
    Second m_second;
};

template<typename Key, typename Value, typename HashArg = typename DefaultHash<SourceType<Key>>::Hash, typename KeyTraitsArg = HashTraits<SourceType<Key>>, typename MappedTraitsArg = HashTraits<SourceType<Value>>>
class CachedHashMap : public VariableLengthObject<HashMap<SourceType<Key>, SourceType<Value>, HashArg, KeyTraitsArg, MappedTraitsArg>> {
    template<typename K, typename V>
    using Map = HashMap<K, V, HashArg, KeyTraitsArg, MappedTraitsArg>;

public:
    void encode(Encoder& encoder, const Map<SourceType<Key>, SourceType<Value>>& map)
    {
        SourceType<decltype(m_entries)> entriesVector(map.size());
        unsigned i = 0;
        for (const auto& it : map)
            entriesVector[i++] = { it.key, it.value };
        m_entries.encode(encoder, entriesVector);
    }

    void decode(Decoder& decoder, Map<SourceType<Key>, SourceType<Value>>& map) const
    {
        SourceType<decltype(m_entries)> decodedEntries;
        m_entries.decode(decoder, decodedEntries);
        for (const auto& pair : decodedEntries)
            map.set(pair.first, pair.second);
    }

private:
    CachedVector<CachedPair<Key, Value>> m_entries;
};

class CachedUniquedStringImpl : public VariableLengthObject<UniquedStringImpl> {
public:
    void encode(Encoder& encoder, const StringImpl& string)
    {
        m_isAtomic = string.isAtomic();
        m_isSymbol = string.isSymbol();
        RefPtr<StringImpl> impl = const_cast<StringImpl*>(&string);

        if (m_isSymbol) {
            SymbolImpl* symbol = static_cast<SymbolImpl*>(impl.get());
            if (!symbol->isNullSymbol()) {
                // We have special handling for well-known symbols.
                if (!symbol->isPrivate())
                    impl = encoder.vm().propertyNames->getPublicName(encoder.vm(), symbol).impl();
            }
        }

        m_is8Bit = impl->is8Bit();
        m_length = impl->length();

        if (!m_length)
            return;

        unsigned size = m_length;
        const void* payload;
        if (m_is8Bit)
            payload = impl->characters8();
        else {
            payload = impl->characters16();
            size *= 2;
        }

        uint8_t* buffer = this->allocate(encoder, size);
        memcpy(buffer, payload, size);
    }

    UniquedStringImpl* decode(Decoder& decoder) const
    {
        auto create = [&](const auto* buffer) -> UniquedStringImpl* {
            if (!m_isSymbol)
                return AtomicStringImpl::add(buffer, m_length).leakRef();

            Identifier ident = Identifier::fromString(&decoder.vm(), buffer, m_length);
            String str = decoder.vm().propertyNames->lookUpPrivateName(ident);
            StringImpl* impl = str.releaseImpl().get();
            ASSERT(impl->isSymbol());
            return static_cast<UniquedStringImpl*>(impl);
        };

        if (!m_length) {
            if (m_isSymbol)
                return &SymbolImpl::createNullSymbol().leakRef();
            return AtomicStringImpl::add("").leakRef();
        }

        if (m_is8Bit)
            return create(this->buffer<LChar>());
        return create(this->buffer<UChar>());
    }

private:
    bool m_is8Bit : 1;
    bool m_isSymbol : 1;
    bool m_isAtomic : 1;
    unsigned m_length;
};

class CachedStringImpl : public VariableLengthObject<StringImpl> {
public:
    void encode(Encoder& encoder, const StringImpl& impl)
    {
        m_uniquedStringImpl.encode(encoder, impl);
    }

    StringImpl* decode(Decoder& decoder) const
    {
        return m_uniquedStringImpl.decode(decoder);
    }

private:
    CachedUniquedStringImpl m_uniquedStringImpl;
};

class CachedString : public VariableLengthObject<String> {
public:
    void encode(Encoder& encoder, const String& string)
    {
        m_impl.encode(encoder, static_cast<UniquedStringImpl*>(string.impl()));
    }

    String decode(Decoder& decoder) const
    {
        return String(static_cast<RefPtr<StringImpl>>(m_impl.decode(decoder)));
    }

    void decode(Decoder& decoder, String& dst) const
    {
        dst = decode(decoder);
    }

private:
    CachedRefPtr<CachedUniquedStringImpl> m_impl;
};

class CachedIdentifier : public VariableLengthObject<Identifier> {
public:
    void encode(Encoder& encoder, const Identifier& identifier)
    {
        m_string.encode(encoder, identifier.string());
    }

    Identifier decode(Decoder& decoder) const
    {
        String str = m_string.decode(decoder);
        if (str.isNull())
            return Identifier();

        return Identifier::fromUid(&decoder.vm(), (UniquedStringImpl*)str.impl());
    }

    void decode(Decoder& decoder, Identifier& ident) const
    {
        ident = decode(decoder);
    }

private:
    CachedString m_string;
};

template<typename T>
class CachedOptional : public VariableLengthObject<WTF::Optional<SourceType<T>>> {
public:
    void encode(Encoder& encoder, const WTF::Optional<SourceType<T>>& source)
    {
        m_isEmpty = !source;

        if (m_isEmpty)
            return;

        this->template allocate<T>(encoder)->encode(encoder, *source);
    }

    WTF::Optional<SourceType<T>> decode(Decoder& decoder) const
    {
        if (m_isEmpty)
            return WTF::nullopt;

        return { this->template buffer<T>()->decode(decoder) };
    }

    void decode(Decoder& decoder, WTF::Optional<SourceType<T>>& dst) const
    {
        dst = decode(decoder);
    }

    void encode(Encoder& encoder, const std::unique_ptr<SourceType<T>>& source)
    {
        if (!source)
            encode(encoder, WTF::nullopt);
        else
            encode(encoder, { *source });
    }

    SourceType<T>* decodeAsPtr(Decoder& decoder) const
    {
        if (m_isEmpty)
            return nullptr;

        return this->template buffer<T>()->decode(decoder);
    }

private:
    bool m_isEmpty;
};

class CachedSimpleJumpTable : public CachedObject<UnlinkedSimpleJumpTable> {
public:
    void encode(Encoder& encoder, const UnlinkedSimpleJumpTable& jumpTable)
    {
        m_min = jumpTable.min;
        m_branchOffsets.encode(encoder, jumpTable.branchOffsets);
    }

    void decode(Decoder& decoder, UnlinkedSimpleJumpTable& jumpTable) const
    {
        jumpTable.min = m_min;
        m_branchOffsets.decode(decoder, jumpTable.branchOffsets);
    }

private:
    int32_t m_min;
    CachedVector<int32_t> m_branchOffsets;
};

class CachedStringJumpTable : public CachedObject<UnlinkedStringJumpTable> {
public:
    void encode(Encoder& encoder, const UnlinkedStringJumpTable& jumpTable)
    {
        m_offsetTable.encode(encoder, jumpTable.offsetTable);
    }

    void decode(Decoder& decoder, UnlinkedStringJumpTable& jumpTable) const
    {
        m_offsetTable.decode(decoder, jumpTable.offsetTable);
    }

private:
    CachedHashMap<CachedRefPtr<CachedStringImpl>, UnlinkedStringJumpTable:: OffsetLocation> m_offsetTable;
};

class CachedBitVector : public VariableLengthObject<BitVector> {
public:
    void encode(Encoder& encoder, const BitVector& bitVector)
    {
        m_numBits = bitVector.size();
        if (!m_numBits)
            return;
        size_t sizeInBytes = BitVector::byteCount(m_numBits);
        uint8_t* buffer = this->allocate(encoder, sizeInBytes);
        memcpy(buffer, bitVector.bits(), sizeInBytes);
    }

    void decode(Decoder&, BitVector& bitVector) const
    {
        if (!m_numBits)
            return;
        bitVector.ensureSize(m_numBits);
        size_t sizeInBytes = BitVector::byteCount(m_numBits);
        memcpy(bitVector.bits(), this->buffer(), sizeInBytes);
    }

private:
    size_t m_numBits;
};

template<typename T, typename HashArg = typename DefaultHash<T>::Hash>
class CachedHashSet : public CachedObject<HashSet<SourceType<T>, HashArg>> {
public:
    void encode(Encoder& encoder, const HashSet<SourceType<T>, HashArg>& set)
    {
        SourceType<decltype(m_entries)> entriesVector(set.size());
        unsigned i = 0;
        for (const auto& item : set)
            entriesVector[i++] = item;
        m_entries.encode(encoder, entriesVector);
    }

    void decode(Decoder& decoder, HashSet<SourceType<T>, HashArg>& set) const
    {
        SourceType<decltype(m_entries)> entriesVector;
        m_entries.decode(decoder, entriesVector);
        for (const auto& item : entriesVector)
            set.add(item);
    }

private:
    CachedVector<T> m_entries;
};

class CachedConstantIdentifierSetEntry : public VariableLengthObject<ConstantIdentifierSetEntry> {
public:
    void encode(Encoder& encoder, const ConstantIdentifierSetEntry& entry)
    {
        m_constant = entry.second;
        m_set.encode(encoder, entry.first);
    }

    void decode(Decoder& decoder, ConstantIdentifierSetEntry& entry) const
    {
        entry.second = m_constant;
        m_set.decode(decoder, entry.first);
    }

private:
    unsigned m_constant;
    CachedHashSet<CachedRefPtr<CachedUniquedStringImpl>, IdentifierRepHash> m_set;
};

class CachedCodeBlockRareData : public CachedObject<UnlinkedCodeBlock::RareData> {
public:
    void encode(Encoder& encoder, const UnlinkedCodeBlock::RareData& rareData)
    {
        m_exceptionHandlers.encode(encoder, rareData.m_exceptionHandlers);
        m_switchJumpTables.encode(encoder, rareData.m_switchJumpTables);
        m_stringSwitchJumpTables.encode(encoder, rareData.m_stringSwitchJumpTables);
        m_expressionInfoFatPositions.encode(encoder, rareData.m_expressionInfoFatPositions);
        m_typeProfilerInfoMap.encode(encoder, rareData.m_typeProfilerInfoMap);
        m_opProfileControlFlowBytecodeOffsets.encode(encoder, rareData.m_opProfileControlFlowBytecodeOffsets);
        m_bitVectors.encode(encoder, rareData.m_bitVectors);
        m_constantIdentifierSets.encode(encoder, rareData.m_constantIdentifierSets);
    }

    UnlinkedCodeBlock::RareData* decode(Decoder& decoder) const
    {
        UnlinkedCodeBlock::RareData* rareData = new UnlinkedCodeBlock::RareData { };
        m_exceptionHandlers.decode(decoder, rareData->m_exceptionHandlers);
        m_switchJumpTables.decode(decoder, rareData->m_switchJumpTables);
        m_stringSwitchJumpTables.decode(decoder, rareData->m_stringSwitchJumpTables);
        m_expressionInfoFatPositions.decode(decoder, rareData->m_expressionInfoFatPositions);
        m_typeProfilerInfoMap.decode(decoder, rareData->m_typeProfilerInfoMap);
        m_opProfileControlFlowBytecodeOffsets.decode(decoder, rareData->m_opProfileControlFlowBytecodeOffsets);
        m_bitVectors.decode(decoder, rareData->m_bitVectors);
        m_constantIdentifierSets.decode(decoder, rareData->m_constantIdentifierSets);
        return rareData;
    }

private:
    CachedVector<UnlinkedHandlerInfo> m_exceptionHandlers;
    CachedVector<CachedSimpleJumpTable> m_switchJumpTables;
    CachedVector<CachedStringJumpTable> m_stringSwitchJumpTables;
    CachedVector<ExpressionRangeInfo::FatPosition> m_expressionInfoFatPositions;
    CachedHashMap<unsigned, UnlinkedCodeBlock::RareData::TypeProfilerExpressionRange> m_typeProfilerInfoMap;
    CachedVector<InstructionStream::Offset> m_opProfileControlFlowBytecodeOffsets;
    CachedVector<CachedBitVector> m_bitVectors;
    CachedVector<CachedConstantIdentifierSetEntry> m_constantIdentifierSets;
};

class CachedVariableEnvironment : public CachedObject<VariableEnvironment> {
public:
    void encode(Encoder& encoder, const VariableEnvironment& env)
    {
        m_isEverythingCaptured = env.m_isEverythingCaptured;
        m_map.encode(encoder, env.m_map);
    }

    void decode(Decoder& decoder, VariableEnvironment& env) const
    {
        env.m_isEverythingCaptured = m_isEverythingCaptured;
        m_map.decode(decoder, env.m_map);
    }

private:
    bool m_isEverythingCaptured;
    CachedHashMap<CachedRefPtr<CachedUniquedStringImpl>, VariableEnvironmentEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, VariableEnvironmentEntryHashTraits> m_map;
};

template<typename T, typename Source = SourceType<T>>
class CachedArray : public VariableLengthObject<Source*> {
public:
    void encode(Encoder& encoder, const Source* array, unsigned size)
    {
        if (!size)
            return;
        T* dst = this->template allocate<T>(encoder, size);
        for (unsigned i = 0; i < size; ++i)
            ::JSC::encode(encoder, dst[i], array[i]);
    }

    template<typename... Args>
    void decode(Decoder& decoder, Source* array, unsigned size, Args... args) const
    {
        if (!size)
            return;
        const T* buffer = this->template buffer<T>();
        for (unsigned i = 0; i < size; ++i)
            ::JSC::decode(decoder, buffer[i], array[i], args...);
    }
};

class CachedScopedArgumentsTable : public CachedObject<ScopedArgumentsTable> {
public:
    void encode(Encoder& encoder, const ScopedArgumentsTable& scopedArgumentsTable)
    {
        m_length = scopedArgumentsTable.m_length;
        m_arguments.encode(encoder, scopedArgumentsTable.m_arguments.get(), m_length);
    }

    ScopedArgumentsTable* decode(Decoder& decoder) const
    {
        ScopedArgumentsTable* scopedArgumentsTable = ScopedArgumentsTable::create(decoder.vm(), m_length);
        m_arguments.decode(decoder, scopedArgumentsTable->m_arguments.get(), m_length);
        return scopedArgumentsTable;
    }

private:
    uint32_t m_length;
    CachedArray<ScopeOffset> m_arguments;
};

class CachedSymbolTableEntry : public CachedObject<SymbolTableEntry> {
public:
    void encode(Encoder&, const SymbolTableEntry& symbolTableEntry)
    {
        m_bits = symbolTableEntry.m_bits | SymbolTableEntry::SlimFlag;
    }

    void decode(Decoder&, SymbolTableEntry& symbolTableEntry) const
    {
        symbolTableEntry.m_bits = m_bits;
    }

private:
    intptr_t m_bits;
};

class CachedSymbolTable : public CachedObject<SymbolTable> {
public:
    void encode(Encoder& encoder, const SymbolTable& symbolTable)
    {
        m_map.encode(encoder, symbolTable.m_map);
        m_maxScopeOffset = symbolTable.m_maxScopeOffset;
        m_usesNonStrictEval = symbolTable.m_usesNonStrictEval;
        m_nestedLexicalScope = symbolTable.m_nestedLexicalScope;
        m_scopeType = symbolTable.m_scopeType;
        m_arguments.encode(encoder, symbolTable.m_arguments.get());
    }

    SymbolTable* decode(Decoder& decoder) const
    {
        SymbolTable* symbolTable = SymbolTable::create(decoder.vm());
        m_map.decode(decoder, symbolTable->m_map);
        symbolTable->m_maxScopeOffset = m_maxScopeOffset;
        symbolTable->m_usesNonStrictEval = m_usesNonStrictEval;
        symbolTable->m_nestedLexicalScope = m_nestedLexicalScope;
        symbolTable->m_scopeType = m_scopeType;
        ScopedArgumentsTable* scopedArgumentsTable = m_arguments.decode(decoder);
        if (scopedArgumentsTable)
            symbolTable->m_arguments.set(decoder.vm(), symbolTable, scopedArgumentsTable);
        return symbolTable;
    }

private:
    CachedHashMap<CachedRefPtr<CachedUniquedStringImpl>, CachedSymbolTableEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, SymbolTableIndexHashTraits> m_map;
    ScopeOffset m_maxScopeOffset;
    unsigned m_usesNonStrictEval : 1;
    unsigned m_nestedLexicalScope : 1;
    unsigned m_scopeType : 3;
    CachedPtr<CachedScopedArgumentsTable> m_arguments;
};

class CachedJSValue;
class CachedImmutableButterfly : public CachedObject<JSImmutableButterfly> {
public:
    CachedImmutableButterfly()
        : m_cachedDoubles()
    {
    }

    void encode(Encoder& encoder, JSImmutableButterfly& immutableButterfly)
    {
        m_length = immutableButterfly.length();
        m_indexingType = immutableButterfly.indexingTypeAndMisc();
        if (hasDouble(m_indexingType))
            m_cachedDoubles.encode(encoder, immutableButterfly.toButterfly()->contiguousDouble().data(), m_length);
        else
            m_cachedValues.encode(encoder, immutableButterfly.toButterfly()->contiguous().data(), m_length);
    }

    JSImmutableButterfly* decode(Decoder& decoder) const
    {
        JSImmutableButterfly* immutableButterfly = JSImmutableButterfly::create(decoder.vm(), m_indexingType, m_length);
        if (hasDouble(m_indexingType))
            m_cachedDoubles.decode(decoder, immutableButterfly->toButterfly()->contiguousDouble().data(), m_length, immutableButterfly);
        else
            m_cachedValues.decode(decoder, immutableButterfly->toButterfly()->contiguous().data(), m_length, immutableButterfly);
        return immutableButterfly;
    }

private:
    IndexingType m_indexingType;
    unsigned m_length;
    union {
        CachedArray<double> m_cachedDoubles;
        CachedArray<CachedJSValue, WriteBarrier<Unknown>> m_cachedValues;
    };
};

class CachedRegExp : public CachedObject<RegExp> {
public:
    void encode(Encoder& encoder, const RegExp& regExp)
    {
        m_patternString.encode(encoder, regExp.m_patternString);
        m_flags = regExp.m_flags;
    }

    RegExp* decode(Decoder& decoder) const
    {
        String pattern { m_patternString.decode(decoder) };
        return RegExp::create(decoder.vm(), pattern, m_flags);
    }

private:
    CachedString m_patternString;
    RegExpFlags m_flags;
};

class CachedTemplateObjectDescriptor : public CachedObject<TemplateObjectDescriptor> {
public:
    void encode(Encoder& encoder, const TemplateObjectDescriptor& templateObjectDescriptor)
    {
        m_rawStrings.encode(encoder, templateObjectDescriptor.rawStrings());
        m_cookedStrings.encode(encoder, templateObjectDescriptor.cookedStrings());
    }

    Ref<TemplateObjectDescriptor> decode(Decoder& decoder) const
    {
        TemplateObjectDescriptor::StringVector decodedRawStrings;
        TemplateObjectDescriptor::OptionalStringVector decodedCookedStrings;
        m_rawStrings.decode(decoder, decodedRawStrings);
        m_cookedStrings.decode(decoder, decodedCookedStrings);
        return TemplateObjectDescriptor::create(WTFMove(decodedRawStrings), WTFMove(decodedCookedStrings));
    }

private:
    CachedVector<CachedString, 4> m_rawStrings;
    CachedVector<CachedOptional<CachedString>, 4> m_cookedStrings;
};

class CachedBigInt : public VariableLengthObject<JSBigInt> {
public:
    void encode(Encoder& encoder, JSBigInt& bigInt)
    {
        m_length = bigInt.length();
        m_sign = bigInt.sign();

        if (!m_length)
            return;

        unsigned size = sizeof(JSBigInt::Digit) * m_length;
        uint8_t* buffer = this->allocate(encoder, size);
        memcpy(buffer, bigInt.dataStorage(), size);
    }

    JSBigInt* decode(Decoder& decoder) const
    {
        JSBigInt* bigInt = JSBigInt::createWithLengthUnchecked(decoder.vm(), m_length);
        bigInt->setSign(m_sign);
        if (m_length)
            memcpy(bigInt->dataStorage(), this->buffer(), sizeof(JSBigInt::Digit) * m_length);
        return bigInt;
    }

private:
    unsigned m_length;
    bool m_sign;
};

class CachedJSValue : public VariableLengthObject<WriteBarrier<Unknown>> {
public:
    void encode(Encoder& encoder, const WriteBarrier<Unknown> value)
    {
        JSValue v = value.get();

        if (!v.isCell() || v.isEmpty()) {
            m_type = EncodedType::JSValue;
            *this->allocate<EncodedJSValue>(encoder) = JSValue::encode(v);
            return;
        }

        JSCell* cell = v.asCell();
        VM& vm = encoder.vm();

        if (auto* symbolTable = jsDynamicCast<SymbolTable*>(vm, cell)) {
            m_type = EncodedType::SymbolTable;
            this->allocate<CachedSymbolTable>(encoder)->encode(encoder, *symbolTable);
            return;
        }

        if (auto* string = jsDynamicCast<JSString*>(vm, cell)) {
            m_type = EncodedType::String;
            StringImpl* impl = string->tryGetValue().impl();
            this->allocate<CachedUniquedStringImpl>(encoder)->encode(encoder, *impl);
            return;
        }

        if (auto* immutableButterfly = jsDynamicCast<JSImmutableButterfly*>(vm, cell)) {
            m_type = EncodedType::ImmutableButterfly;
            this->allocate<CachedImmutableButterfly>(encoder)->encode(encoder, *immutableButterfly);
            return;
        }

        if (auto* regexp = jsDynamicCast<RegExp*>(vm, cell)) {
            m_type = EncodedType::RegExp;
            this->allocate<CachedRegExp>(encoder)->encode(encoder, *regexp);
            return;
        }

        if (auto* templateObjectDescriptor = jsDynamicCast<JSTemplateObjectDescriptor*>(vm, cell)) {
            m_type = EncodedType::TemplateObjectDescriptor;
            this->allocate<CachedTemplateObjectDescriptor>(encoder)->encode(encoder, templateObjectDescriptor->descriptor());
            return;
        }

        if (auto* bigInt = jsDynamicCast<JSBigInt*>(vm, cell)) {
            m_type = EncodedType::BigInt;
            this->allocate<CachedBigInt>(encoder)->encode(encoder, *bigInt);
            return;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    void decode(Decoder& decoder, WriteBarrier<Unknown>& value, const JSCell* owner) const
    {
        JSValue v;
        switch (m_type) {
        case EncodedType::JSValue:
            v = JSValue::decode(*this->buffer<EncodedJSValue>());
            break;
        case EncodedType::SymbolTable:
            v = this->buffer<CachedSymbolTable>()->decode(decoder);
            break;
        case EncodedType::String: {
            StringImpl* impl = this->buffer<CachedUniquedStringImpl>()->decode(decoder);
            v = jsString(&decoder.vm(), adoptRef(*impl));
            break;
        }
        case EncodedType::ImmutableButterfly:
            v = this->buffer<CachedImmutableButterfly>()->decode(decoder);
            break;
        case EncodedType::RegExp:
            v = this->buffer<CachedRegExp>()->decode(decoder);
            break;
        case EncodedType::TemplateObjectDescriptor:
            v = JSTemplateObjectDescriptor::create(decoder.vm(), this->buffer<CachedTemplateObjectDescriptor>()->decode(decoder));
            break;
        case EncodedType::BigInt:
            v = this->buffer<CachedBigInt>()->decode(decoder);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        value.set(decoder.vm(), owner, v);
    }

private:
    enum class EncodedType : uint8_t {
        JSValue,
        SymbolTable,
        String,
        ImmutableButterfly,
        RegExp,
        TemplateObjectDescriptor,
        BigInt,
    };

    EncodedType m_type;
};

class CachedInstructionStream : public CachedObject<InstructionStream> {
public:
    void encode(Encoder& encoder, const InstructionStream& stream)
    {
        m_instructions.encode(encoder, stream.m_instructions);
    }

    InstructionStream* decode(Decoder& decoder) const
    {
        Vector<uint8_t, 0, UnsafeVectorOverflow> instructionsVector;
        m_instructions.decode(decoder, instructionsVector);
        return new InstructionStream(WTFMove(instructionsVector));
    }

private:
    CachedVector<uint8_t, 0, UnsafeVectorOverflow> m_instructions;
};

class CachedMetadataTable : public CachedObject<UnlinkedMetadataTable> {
public:
    void encode(Encoder&, const UnlinkedMetadataTable& metadataTable)
    {
        ASSERT(metadataTable.m_isFinalized);
        m_hasMetadata = metadataTable.m_hasMetadata;
        if (!m_hasMetadata)
            return;
        for (unsigned i = UnlinkedMetadataTable::s_offsetTableEntries; i--;)
            m_metadata[i] = metadataTable.buffer()[i];
    }

    Ref<UnlinkedMetadataTable> decode(Decoder&) const
    {
        Ref<UnlinkedMetadataTable> metadataTable = UnlinkedMetadataTable::create();
        metadataTable->m_isFinalized = true;
        metadataTable->m_isLinked = false;
        metadataTable->m_hasMetadata = m_hasMetadata;
        for (unsigned i = UnlinkedMetadataTable::s_offsetTableEntries; i--;)
            metadataTable->buffer()[i] = m_metadata[i];
        return metadataTable;
    }

private:
    bool m_hasMetadata;
    std::array<unsigned, UnlinkedMetadataTable::s_offsetTableEntries> m_metadata;
};

class CachedSourceOrigin : public CachedObject<SourceOrigin> {
public:
    void encode(Encoder& encoder, const SourceOrigin& sourceOrigin)
    {
        m_string.encode(encoder, sourceOrigin.string());
    }

    SourceOrigin decode(Decoder& decoder) const
    {
        return SourceOrigin { m_string.decode(decoder) };
    }

private:
    CachedString m_string;
};

class CachedTextPosition : public CachedObject<TextPosition> {
public:
    void encode(Encoder&, TextPosition textPosition)
    {
        m_line = textPosition.m_line.zeroBasedInt();
        m_column = textPosition.m_column.zeroBasedInt();
    }

    TextPosition decode(Decoder&) const
    {
        return TextPosition { OrdinalNumber::fromZeroBasedInt(m_line), OrdinalNumber::fromZeroBasedInt(m_column) };
    }

private:
    int m_line;
    int m_column;
};

template <typename Source, typename CachedType>
class CachedSourceProviderShape : public CachedObject<Source> {
public:
    void encode(Encoder& encoder, const SourceProvider& sourceProvider)
    {
        m_sourceOrigin.encode(encoder, sourceProvider.sourceOrigin());
        m_url.encode(encoder, sourceProvider.url());
        m_sourceURLDirective.encode(encoder, sourceProvider.sourceURLDirective());
        m_sourceMappingURLDirective.encode(encoder, sourceProvider.sourceMappingURLDirective());
        m_startPosition.encode(encoder, sourceProvider.startPosition());
    }

    void decode(Decoder& decoder, SourceProvider& sourceProvider) const
    {
        sourceProvider.setSourceURLDirective(m_sourceURLDirective.decode(decoder));
        sourceProvider.setSourceMappingURLDirective(m_sourceMappingURLDirective.decode(decoder));
    }

protected:
    CachedSourceOrigin m_sourceOrigin;
    CachedString m_url;
    CachedString m_sourceURLDirective;
    CachedString m_sourceMappingURLDirective;
    CachedTextPosition m_startPosition;
};

class CachedStringSourceProvider : public CachedSourceProviderShape<StringSourceProvider, CachedStringSourceProvider> {
    using Base = CachedSourceProviderShape<StringSourceProvider, CachedStringSourceProvider>;

public:
    void encode(Encoder& encoder, const StringSourceProvider& sourceProvider)
    {
        Base::encode(encoder, sourceProvider);
        m_source.encode(encoder, sourceProvider.source().toString());
    }

    StringSourceProvider* decode(Decoder& decoder, SourceProviderSourceType sourceType) const
    {
        String decodedSource = m_source.decode(decoder);
        SourceOrigin decodedSourceOrigin = m_sourceOrigin.decode(decoder);
        String decodedURL = m_url.decode(decoder);
        TextPosition decodedStartPosition = m_startPosition.decode(decoder);

        Ref<StringSourceProvider> sourceProvider = StringSourceProvider::create(decodedSource, decodedSourceOrigin, URL(URL(), decodedURL), decodedStartPosition, sourceType);
        Base::decode(decoder, sourceProvider.get());
        return &sourceProvider.leakRef();
    }

private:
    CachedString m_source;
};

#if ENABLE(WEBASSEMBLY)
class CachedWebAssemblySourceProvider : public CachedSourceProviderShape<WebAssemblySourceProvider, CachedWebAssemblySourceProvider> {
    using Base = CachedSourceProviderShape<WebAssemblySourceProvider, CachedWebAssemblySourceProvider>;

public:
    void encode(Encoder& encoder, const WebAssemblySourceProvider& sourceProvider)
    {
        Base::encode(encoder, sourceProvider);
        m_data.encode(encoder, sourceProvider.data());
    }

    WebAssemblySourceProvider* decode(Decoder& decoder) const
    {
        Vector<uint8_t> decodedData;
        SourceOrigin decodedSourceOrigin = m_sourceOrigin.decode(decoder);
        String decodedURL = m_url.decode(decoder);

        m_data.decode(decoder, decodedData);

        Ref<WebAssemblySourceProvider> sourceProvider = WebAssemblySourceProvider::create(WTFMove(decodedData), decodedSourceOrigin, URL(URL(), decodedURL));
        Base::decode(decoder, sourceProvider.get());

        return &sourceProvider.leakRef();
    }

private:
    CachedVector<uint8_t> m_data;
};
#endif

class CachedSourceProvider : public VariableLengthObject<SourceProvider> {
public:
    void encode(Encoder& encoder, const SourceProvider& sourceProvider)
    {
        m_sourceType = sourceProvider.sourceType();
        switch (m_sourceType) {
        case SourceProviderSourceType::Program:
        case SourceProviderSourceType::Module:
            this->allocate<CachedStringSourceProvider>(encoder)->encode(encoder, reinterpret_cast<const StringSourceProvider&>(sourceProvider));
            break;
#if ENABLE(WEBASSEMBLY)
        case SourceProviderSourceType::WebAssembly:
            this->allocate<CachedWebAssemblySourceProvider>(encoder)->encode(encoder, reinterpret_cast<const WebAssemblySourceProvider&>(sourceProvider));
            break;
#endif
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    SourceProvider* decode(Decoder& decoder) const
    {
        switch (m_sourceType) {
        case SourceProviderSourceType::Program:
        case SourceProviderSourceType::Module:
            return this->buffer<CachedStringSourceProvider>()->decode(decoder, m_sourceType);
#if ENABLE(WEBASSEMBLY)
        case SourceProviderSourceType::WebAssembly:
            return this->buffer<CachedWebAssemblySourceProvider>()->decode(decoder);
#endif
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

private:
    SourceProviderSourceType m_sourceType;
};

template<typename Source>
class CachedUnlinkedSourceCodeShape : public CachedObject<Source> {
public:
    void encode(Encoder& encoder, const UnlinkedSourceCode& sourceCode)
    {
        m_provider.encode(encoder, sourceCode.m_provider.get());
        m_startOffset = sourceCode.startOffset();
        m_endOffset = sourceCode.endOffset();
    }

    void decode(Decoder& decoder, UnlinkedSourceCode& sourceCode) const
    {
        sourceCode.m_provider = m_provider.decode(decoder);
        sourceCode.m_startOffset = m_startOffset;
        sourceCode.m_endOffset = m_endOffset;
    }

private:
    CachedPtr<CachedSourceProvider> m_provider;
    int m_startOffset;
    int m_endOffset;
};


class CachedUnlinkedSourceCode : public CachedUnlinkedSourceCodeShape<UnlinkedSourceCode> { };

class CachedSourceCode : public CachedUnlinkedSourceCodeShape<SourceCode> {
    using Base = CachedUnlinkedSourceCodeShape<SourceCode>;

public:
    void encode(Encoder& encoder, const SourceCode& sourceCode)
    {
        Base::encode(encoder, sourceCode);
        m_firstLine = sourceCode.firstLine().zeroBasedInt();
        m_startColumn = sourceCode.startColumn().zeroBasedInt();
    }

    void decode(Decoder& decoder, SourceCode& sourceCode) const
    {
        Base::decode(decoder, sourceCode);
        sourceCode.m_firstLine = OrdinalNumber::fromZeroBasedInt(m_firstLine);
        sourceCode.m_startColumn = OrdinalNumber::fromZeroBasedInt(m_startColumn);
    }

private:
    int m_firstLine;
    int m_startColumn;
};

class CachedFunctionExecutableRareData : public CachedObject<UnlinkedFunctionExecutable::RareData> {
public:
    void encode(Encoder& encoder, const UnlinkedFunctionExecutable::RareData& rareData)
    {
        m_classSource.encode(encoder, rareData.m_classSource);
    }

    UnlinkedFunctionExecutable::RareData* decode(Decoder& decoder) const
    {
        UnlinkedFunctionExecutable::RareData* rareData = new UnlinkedFunctionExecutable::RareData { };
        m_classSource.decode(decoder, rareData->m_classSource);
        return rareData;
    }

private:
    CachedSourceCode m_classSource;
};

class CachedFunctionExecutable : public CachedObject<UnlinkedFunctionExecutable> {
public:
    void encode(Encoder&, const UnlinkedFunctionExecutable&);
    UnlinkedFunctionExecutable* decode(Decoder&) const;

    unsigned firstLineOffset() const { return m_firstLineOffset; }
    unsigned lineCount() const { return m_lineCount; }
    unsigned unlinkedFunctionNameStart() const { return m_unlinkedFunctionNameStart; }
    unsigned unlinkedBodyStartColumn() const { return m_unlinkedBodyStartColumn; }
    unsigned unlinkedBodyEndColumn() const { return m_unlinkedBodyEndColumn; }
    unsigned startOffset() const { return m_startOffset; }
    unsigned sourceLength() const { return m_sourceLength; }
    unsigned parametersStartOffset() const { return m_parametersStartOffset; }
    unsigned typeProfilingStartOffset() const { return m_typeProfilingStartOffset; }
    unsigned typeProfilingEndOffset() const { return m_typeProfilingEndOffset; }
    unsigned parameterCount() const { return m_parameterCount; }

    CodeFeatures features() const { return m_features; }
    SourceParseMode sourceParseMode() const { return m_sourceParseMode; }

    unsigned isInStrictContext() const { return m_isInStrictContext; }
    unsigned hasCapturedVariables() const { return m_hasCapturedVariables; }
    unsigned isBuiltinFunction() const { return m_isBuiltinFunction; }
    unsigned isBuiltinDefaultClassConstructor() const { return m_isBuiltinDefaultClassConstructor; }
    unsigned constructAbility() const { return m_constructAbility; }
    unsigned constructorKind() const { return m_constructorKind; }
    unsigned functionMode() const { return m_functionMode; }
    unsigned scriptMode() const { return m_scriptMode; }
    unsigned superBinding() const { return m_superBinding; }
    unsigned derivedContextType() const { return m_derivedContextType; }

    Identifier name(Decoder& decoder) const { return m_name.decode(decoder); }
    Identifier ecmaName(Decoder& decoder) const { return m_ecmaName.decode(decoder); }
    Identifier inferredName(Decoder& decoder) const { return m_inferredName.decode(decoder); }

    UnlinkedFunctionExecutable::RareData* rareData(Decoder& decoder) const { return m_rareData.decodeAsPtr(decoder); }

private:
    unsigned m_firstLineOffset;
    unsigned m_lineCount;
    unsigned m_unlinkedFunctionNameStart;
    unsigned m_unlinkedBodyStartColumn;
    unsigned m_unlinkedBodyEndColumn;
    unsigned m_startOffset;
    unsigned m_sourceLength;
    unsigned m_parametersStartOffset;
    unsigned m_typeProfilingStartOffset;
    unsigned m_typeProfilingEndOffset;
    unsigned m_parameterCount;
    CodeFeatures m_features;
    SourceParseMode m_sourceParseMode;
    unsigned m_isInStrictContext : 1;
    unsigned m_hasCapturedVariables : 1;
    unsigned m_isBuiltinFunction : 1;
    unsigned m_isBuiltinDefaultClassConstructor : 1;
    unsigned m_constructAbility: 1;
    unsigned m_constructorKind : 2;
    unsigned m_functionMode : 2; // FunctionMode
    unsigned m_scriptMode: 1; // JSParserScriptMode
    unsigned m_superBinding : 1;
    unsigned m_derivedContextType: 2;

    CachedOptional<CachedFunctionExecutableRareData> m_rareData;

    CachedIdentifier m_name;
    CachedIdentifier m_ecmaName;
    CachedIdentifier m_inferredName;

    CachedVariableEnvironment m_parentScopeTDZVariables;

    CachedWriteBarrier<CachedFunctionCodeBlock, UnlinkedFunctionCodeBlock> m_unlinkedCodeBlockForCall;
    CachedWriteBarrier<CachedFunctionCodeBlock, UnlinkedFunctionCodeBlock> m_unlinkedCodeBlockForConstruct;
};

template<typename CodeBlockType>
class CachedCodeBlock : public CachedObject<CodeBlockType> {
public:
    void encode(Encoder&, const UnlinkedCodeBlock&);
    void decode(Decoder&, UnlinkedCodeBlock&) const;

    InstructionStream* instructions(Decoder& decoder) const { return m_instructions.decode(decoder); }

    VirtualRegister thisRegister() const { return m_thisRegister; }
    VirtualRegister scopeRegister() const { return m_scopeRegister; }

    String sourceURLDirective(Decoder& decoder) const { return m_sourceURLDirective.decode(decoder); }
    String sourceMappingURLDirective(Decoder& decoder) const { return m_sourceMappingURLDirective.decode(decoder); }

    Ref<UnlinkedMetadataTable> metadata(Decoder& decoder) const { return m_metadata.decode(decoder); }

    unsigned usesEval() const { return m_usesEval; }
    unsigned isStrictMode() const { return m_isStrictMode; }
    unsigned isConstructor() const { return m_isConstructor; }
    unsigned hasCapturedVariables() const { return m_hasCapturedVariables; }
    unsigned isBuiltinFunction() const { return m_isBuiltinFunction; }
    unsigned superBinding() const { return m_superBinding; }
    unsigned scriptMode() const { return m_scriptMode; }
    unsigned isArrowFunctionContext() const { return m_isArrowFunctionContext; }
    unsigned isClassContext() const { return m_isClassContext; }
    unsigned wasCompiledWithDebuggingOpcodes() const { return m_wasCompiledWithDebuggingOpcodes; }
    unsigned constructorKind() const { return m_constructorKind; }
    unsigned derivedContextType() const { return m_derivedContextType; }
    unsigned evalContextType() const { return m_evalContextType; }
    unsigned hasTailCalls() const { return m_hasTailCalls; }
    unsigned lineCount() const { return m_lineCount; }
    unsigned endColumn() const { return m_endColumn; }

    int numVars() const { return m_numVars; }
    int numCalleeLocals() const { return m_numCalleeLocals; }
    int numParameters() const { return m_numParameters; }

    CodeFeatures features() const { return m_features; }
    SourceParseMode parseMode() const { return m_parseMode; }
    unsigned codeType() const { return m_codeType; }

    UnlinkedCodeBlock::RareData* rareData(Decoder& decoder) const { return m_rareData.decodeAsPtr(decoder); }

private:
    VirtualRegister m_thisRegister;
    VirtualRegister m_scopeRegister;
    std::array<unsigned, LinkTimeConstantCount> m_linkTimeConstants;

    unsigned m_usesEval : 1;
    unsigned m_isStrictMode : 1;
    unsigned m_isConstructor : 1;
    unsigned m_hasCapturedVariables : 1;
    unsigned m_isBuiltinFunction : 1;
    unsigned m_superBinding : 1;
    unsigned m_scriptMode: 1;
    unsigned m_isArrowFunctionContext : 1;
    unsigned m_isClassContext : 1;
    unsigned m_wasCompiledWithDebuggingOpcodes : 1;
    unsigned m_constructorKind : 2;
    unsigned m_derivedContextType : 2;
    unsigned m_evalContextType : 2;
    unsigned m_hasTailCalls : 1;
    unsigned m_codeType : 2;

    CodeFeatures m_features;
    SourceParseMode m_parseMode;

    unsigned m_lineCount;
    unsigned m_endColumn;

    int m_numVars;
    int m_numCalleeLocals;
    int m_numParameters;

    CachedMetadataTable m_metadata;

    CachedOptional<CachedCodeBlockRareData> m_rareData;

    CachedString m_sourceURLDirective;
    CachedString m_sourceMappingURLDirective;

    CachedPtr<CachedInstructionStream> m_instructions;
    CachedVector<InstructionStream::Offset> m_jumpTargets;
    CachedVector<InstructionStream::Offset> m_propertyAccessInstructions;
    CachedVector<CachedJSValue> m_constantRegisters;
    CachedVector<SourceCodeRepresentation> m_constantsSourceCodeRepresentation;
    CachedVector<ExpressionRangeInfo> m_expressionInfo;
    CachedHashMap<InstructionStream::Offset, int> m_outOfLineJumpTargets;

    CachedVector<CachedIdentifier> m_identifiers;
    CachedVector<CachedWriteBarrier<CachedFunctionExecutable>> m_functionDecls;
    CachedVector<CachedWriteBarrier<CachedFunctionExecutable>> m_functionExprs;
};

class CachedProgramCodeBlock : public CachedCodeBlock<UnlinkedProgramCodeBlock> {
    using Base = CachedCodeBlock<UnlinkedProgramCodeBlock>;

public:
    void encode(Encoder& encoder, const UnlinkedProgramCodeBlock& codeBlock)
    {
        Base::encode(encoder, codeBlock);
        m_varDeclarations.encode(encoder, codeBlock.m_varDeclarations);
        m_lexicalDeclarations.encode(encoder, codeBlock.m_lexicalDeclarations);
    }

    UnlinkedProgramCodeBlock* decode(Decoder& decoder) const
    {
        UnlinkedProgramCodeBlock* codeBlock = new (NotNull, allocateCell<UnlinkedProgramCodeBlock>(decoder.vm().heap)) UnlinkedProgramCodeBlock(decoder, *this);
        codeBlock->finishCreation(decoder.vm());
        Base::decode(decoder, *codeBlock);
        m_varDeclarations.decode(decoder, codeBlock->m_varDeclarations);
        m_lexicalDeclarations.decode(decoder, codeBlock->m_lexicalDeclarations);
        return codeBlock;
    }

private:
    CachedVariableEnvironment m_varDeclarations;
    CachedVariableEnvironment m_lexicalDeclarations;
};

class CachedModuleCodeBlock : public CachedCodeBlock<UnlinkedModuleProgramCodeBlock> {
    using Base = CachedCodeBlock<UnlinkedModuleProgramCodeBlock>;

public:
    void encode(Encoder& encoder, const UnlinkedModuleProgramCodeBlock& codeBlock)
    {
        Base::encode(encoder, codeBlock);
        m_moduleEnvironmentSymbolTableConstantRegisterOffset = codeBlock.m_moduleEnvironmentSymbolTableConstantRegisterOffset;
    }

    UnlinkedModuleProgramCodeBlock* decode(Decoder& decoder) const
    {
        UnlinkedModuleProgramCodeBlock* codeBlock = new (NotNull, allocateCell<UnlinkedModuleProgramCodeBlock>(decoder.vm().heap)) UnlinkedModuleProgramCodeBlock(decoder, *this);
        codeBlock->finishCreation(decoder.vm());
        Base::decode(decoder, *codeBlock);
        codeBlock->m_moduleEnvironmentSymbolTableConstantRegisterOffset = m_moduleEnvironmentSymbolTableConstantRegisterOffset;
        return codeBlock;
    }

private:
    int m_moduleEnvironmentSymbolTableConstantRegisterOffset;
};

class CachedEvalCodeBlock : public CachedCodeBlock<UnlinkedEvalCodeBlock> {
    using Base = CachedCodeBlock<UnlinkedEvalCodeBlock>;

public:
    void encode(Encoder& encoder, const UnlinkedEvalCodeBlock& codeBlock)
    {
        Base::encode(encoder, codeBlock);
        m_variables.encode(encoder, codeBlock.m_variables);
        m_functionHoistingCandidates.encode(encoder, codeBlock.m_functionHoistingCandidates);
    }

    UnlinkedEvalCodeBlock* decode(Decoder& decoder) const
    {
        UnlinkedEvalCodeBlock* codeBlock = new (NotNull, allocateCell<UnlinkedEvalCodeBlock>(decoder.vm().heap)) UnlinkedEvalCodeBlock(decoder, *this);
        codeBlock->finishCreation(decoder.vm());
        Base::decode(decoder, *codeBlock);
        m_variables.decode(decoder, codeBlock->m_variables);
        m_functionHoistingCandidates.decode(decoder, codeBlock->m_functionHoistingCandidates);
        return codeBlock;
    }

private:
    CachedVector<CachedIdentifier, 0, UnsafeVectorOverflow> m_variables;
    CachedVector<CachedIdentifier, 0, UnsafeVectorOverflow> m_functionHoistingCandidates;
};

class CachedFunctionCodeBlock : public CachedCodeBlock<UnlinkedFunctionCodeBlock> {
    using Base = CachedCodeBlock<UnlinkedFunctionCodeBlock>;

public:
    void encode(Encoder& encoder, const UnlinkedFunctionCodeBlock& codeBlock)
    {
        Base::encode(encoder, codeBlock);
    }

    UnlinkedFunctionCodeBlock* decode(Decoder& decoder) const
    {
        UnlinkedFunctionCodeBlock* codeBlock = new (NotNull, allocateCell<UnlinkedFunctionCodeBlock>(decoder.vm().heap)) UnlinkedFunctionCodeBlock(decoder, *this);
        codeBlock->finishCreation(decoder.vm());
        Base::decode(decoder, *codeBlock);
        return codeBlock;
    }
};

ALWAYS_INLINE UnlinkedFunctionCodeBlock::UnlinkedFunctionCodeBlock(Decoder& decoder, const CachedFunctionCodeBlock& cachedCodeBlock)
    : Base(decoder, decoder.vm().unlinkedFunctionCodeBlockStructure.get(), cachedCodeBlock)
{
}

template<typename T>
struct CachedCodeBlockTypeImpl;

enum CachedCodeBlockTag {
    CachedProgramCodeBlockTag,
    CachedModuleCodeBlockTag,
    CachedEvalCodeBlockTag,
};

template<>
struct CachedCodeBlockTypeImpl<UnlinkedProgramCodeBlock> {
    using type = CachedProgramCodeBlock;
    static constexpr CachedCodeBlockTag tag = CachedProgramCodeBlockTag;
};

template<>
struct CachedCodeBlockTypeImpl<UnlinkedModuleProgramCodeBlock> {
    using type = CachedModuleCodeBlock;
    static constexpr CachedCodeBlockTag tag = CachedModuleCodeBlockTag;
};

template<>
struct CachedCodeBlockTypeImpl<UnlinkedEvalCodeBlock> {
    using type = CachedEvalCodeBlock;
    static constexpr CachedCodeBlockTag tag = CachedEvalCodeBlockTag;
};

template<typename T>
using CachedCodeBlockType = typename CachedCodeBlockTypeImpl<T>::type;

template<typename CodeBlockType>
ALWAYS_INLINE UnlinkedCodeBlock::UnlinkedCodeBlock(Decoder& decoder, Structure* structure, const CachedCodeBlock<CodeBlockType>& cachedCodeBlock)
    : Base(decoder.vm(), structure)
    , m_thisRegister(cachedCodeBlock.thisRegister())
    , m_scopeRegister(cachedCodeBlock.scopeRegister())

    , m_usesEval(cachedCodeBlock.usesEval())
    , m_isStrictMode(cachedCodeBlock.isStrictMode())
    , m_isConstructor(cachedCodeBlock.isConstructor())
    , m_hasCapturedVariables(cachedCodeBlock.hasCapturedVariables())
    , m_isBuiltinFunction(cachedCodeBlock.isBuiltinFunction())
    , m_superBinding(cachedCodeBlock.superBinding())
    , m_scriptMode(cachedCodeBlock.scriptMode())
    , m_isArrowFunctionContext(cachedCodeBlock.isArrowFunctionContext())
    , m_isClassContext(cachedCodeBlock.isClassContext())
    , m_wasCompiledWithDebuggingOpcodes(cachedCodeBlock.wasCompiledWithDebuggingOpcodes())
    , m_constructorKind(cachedCodeBlock.constructorKind())
    , m_derivedContextType(cachedCodeBlock.derivedContextType())
    , m_evalContextType(cachedCodeBlock.evalContextType())
    , m_hasTailCalls(cachedCodeBlock.hasTailCalls())
    , m_codeType(cachedCodeBlock.codeType())

    , m_features(cachedCodeBlock.features())
    , m_parseMode(cachedCodeBlock.parseMode())

    , m_lineCount(cachedCodeBlock.lineCount())
    , m_endColumn(cachedCodeBlock.endColumn())
    , m_numVars(cachedCodeBlock.numVars())
    , m_numCalleeLocals(cachedCodeBlock.numCalleeLocals())
    , m_numParameters(cachedCodeBlock.numParameters())

    , m_sourceURLDirective(cachedCodeBlock.sourceURLDirective(decoder))
    , m_sourceMappingURLDirective(cachedCodeBlock.sourceMappingURLDirective(decoder))

    , m_metadata(cachedCodeBlock.metadata(decoder))
    , m_instructions(cachedCodeBlock.instructions(decoder))

    , m_rareData(cachedCodeBlock.rareData(decoder))
{
}

template<typename CodeBlockType>
ALWAYS_INLINE void CachedCodeBlock<CodeBlockType>::decode(Decoder& decoder, UnlinkedCodeBlock& codeBlock) const
{
    for (unsigned i = LinkTimeConstantCount; i--;)
        codeBlock.m_linkTimeConstants[i] = m_linkTimeConstants[i];

    m_propertyAccessInstructions.decode(decoder, codeBlock.m_propertyAccessInstructions);
    m_constantRegisters.decode(decoder, codeBlock.m_constantRegisters, &codeBlock);
    m_constantsSourceCodeRepresentation.decode(decoder, codeBlock.m_constantsSourceCodeRepresentation);
    m_expressionInfo.decode(decoder, codeBlock.m_expressionInfo);
    m_outOfLineJumpTargets.decode(decoder, codeBlock.m_outOfLineJumpTargets);
    m_jumpTargets.decode(decoder, codeBlock.m_jumpTargets);
    m_identifiers.decode(decoder, codeBlock.m_identifiers);
    m_functionDecls.decode(decoder, codeBlock.m_functionDecls, &codeBlock);
    m_functionExprs.decode(decoder, codeBlock.m_functionExprs, &codeBlock);
}

ALWAYS_INLINE UnlinkedProgramCodeBlock::UnlinkedProgramCodeBlock(Decoder& decoder, const CachedProgramCodeBlock& cachedCodeBlock)
    : Base(decoder, decoder.vm().unlinkedProgramCodeBlockStructure.get(), cachedCodeBlock)
{
}

ALWAYS_INLINE UnlinkedModuleProgramCodeBlock::UnlinkedModuleProgramCodeBlock(Decoder& decoder, const CachedModuleCodeBlock& cachedCodeBlock)
    : Base(decoder, decoder.vm().unlinkedModuleProgramCodeBlockStructure.get(), cachedCodeBlock)
{
}

ALWAYS_INLINE UnlinkedEvalCodeBlock::UnlinkedEvalCodeBlock(Decoder& decoder, const CachedEvalCodeBlock& cachedCodeBlock)
    : Base(decoder, decoder.vm().unlinkedEvalCodeBlockStructure.get(), cachedCodeBlock)
{
}

ALWAYS_INLINE void CachedFunctionExecutable::encode(Encoder& encoder, const UnlinkedFunctionExecutable& executable)
{
    m_firstLineOffset = executable.m_firstLineOffset;
    m_lineCount = executable.m_lineCount;
    m_unlinkedFunctionNameStart = executable.m_unlinkedFunctionNameStart;
    m_unlinkedBodyStartColumn = executable.m_unlinkedBodyStartColumn;
    m_unlinkedBodyEndColumn = executable.m_unlinkedBodyEndColumn;
    m_startOffset = executable.m_startOffset;
    m_sourceLength = executable.m_sourceLength;
    m_parametersStartOffset = executable.m_parametersStartOffset;
    m_typeProfilingStartOffset = executable.m_typeProfilingStartOffset;
    m_typeProfilingEndOffset = executable.m_typeProfilingEndOffset;
    m_parameterCount = executable.m_parameterCount;

    m_features = executable.m_features;
    m_sourceParseMode = executable.m_sourceParseMode;

    m_isInStrictContext = executable.m_isInStrictContext;
    m_hasCapturedVariables = executable.m_hasCapturedVariables;
    m_isBuiltinFunction = executable.m_isBuiltinFunction;
    m_isBuiltinDefaultClassConstructor = executable.m_isBuiltinDefaultClassConstructor;
    m_constructAbility = executable.m_constructAbility;
    m_constructorKind = executable.m_constructorKind;
    m_functionMode = executable.m_functionMode;
    m_scriptMode = executable.m_scriptMode;
    m_superBinding = executable.m_superBinding;
    m_derivedContextType = executable.m_derivedContextType;

    m_rareData.encode(encoder, executable.m_rareData);

    m_name.encode(encoder, executable.name());
    m_ecmaName.encode(encoder, executable.ecmaName());
    m_inferredName.encode(encoder, executable.inferredName());

    m_parentScopeTDZVariables.encode(encoder, executable.parentScopeTDZVariables());

    m_unlinkedCodeBlockForCall.encode(encoder, executable.m_unlinkedCodeBlockForCall);
    m_unlinkedCodeBlockForConstruct.encode(encoder, executable.m_unlinkedCodeBlockForConstruct);
}

ALWAYS_INLINE UnlinkedFunctionExecutable* CachedFunctionExecutable::decode(Decoder& decoder) const
{
    VariableEnvironment env;
    m_parentScopeTDZVariables.decode(decoder, env);

    UnlinkedFunctionExecutable* executable = new (NotNull, allocateCell<UnlinkedFunctionExecutable>(decoder.vm().heap)) UnlinkedFunctionExecutable(decoder, env, *this);
    executable->finishCreation(decoder.vm());

    m_unlinkedCodeBlockForCall.decode(decoder, executable->m_unlinkedCodeBlockForCall, executable);
    m_unlinkedCodeBlockForConstruct.decode(decoder, executable->m_unlinkedCodeBlockForConstruct, executable);

    return executable;
}

ALWAYS_INLINE UnlinkedFunctionExecutable::UnlinkedFunctionExecutable(Decoder& decoder, VariableEnvironment& parentScopeTDZVariables, const CachedFunctionExecutable& cachedExecutable)
    : Base(decoder.vm(), decoder.vm().unlinkedFunctionExecutableStructure.get())
    , m_firstLineOffset(cachedExecutable.firstLineOffset())
    , m_lineCount(cachedExecutable.lineCount())
    , m_unlinkedFunctionNameStart(cachedExecutable.unlinkedFunctionNameStart())
    , m_unlinkedBodyStartColumn(cachedExecutable.unlinkedBodyStartColumn())
    , m_unlinkedBodyEndColumn(cachedExecutable.unlinkedBodyEndColumn())
    , m_startOffset(cachedExecutable.startOffset())
    , m_sourceLength(cachedExecutable.sourceLength())
    , m_parametersStartOffset(cachedExecutable.parametersStartOffset())
    , m_typeProfilingStartOffset(cachedExecutable.typeProfilingStartOffset())
    , m_typeProfilingEndOffset(cachedExecutable.typeProfilingEndOffset())
    , m_parameterCount(cachedExecutable.parameterCount())
    , m_features(cachedExecutable.features())
    , m_sourceParseMode(cachedExecutable.sourceParseMode())
    , m_isInStrictContext(cachedExecutable.isInStrictContext())
    , m_hasCapturedVariables(cachedExecutable.hasCapturedVariables())
    , m_isBuiltinFunction(cachedExecutable.isBuiltinFunction())
    , m_isBuiltinDefaultClassConstructor(cachedExecutable.isBuiltinDefaultClassConstructor())
    , m_constructAbility(cachedExecutable.constructAbility())
    , m_constructorKind(cachedExecutable.constructorKind())
    , m_functionMode(cachedExecutable.functionMode())
    , m_scriptMode(cachedExecutable.scriptMode())
    , m_superBinding(cachedExecutable.superBinding())
    , m_derivedContextType(cachedExecutable.derivedContextType())

    , m_name(cachedExecutable.name(decoder))
    , m_ecmaName(cachedExecutable.ecmaName(decoder))
    , m_inferredName(cachedExecutable.inferredName(decoder))

    , m_parentScopeTDZVariables(decoder.vm().m_compactVariableMap->get(parentScopeTDZVariables))

    , m_rareData(cachedExecutable.rareData(decoder))
{
}

template<typename CodeBlockType>
ALWAYS_INLINE void CachedCodeBlock<CodeBlockType>::encode(Encoder& encoder, const UnlinkedCodeBlock& codeBlock)
{
    m_thisRegister = codeBlock.m_thisRegister;
    m_scopeRegister = codeBlock.m_scopeRegister;
    m_usesEval = codeBlock.m_usesEval;
    m_isStrictMode = codeBlock.m_isStrictMode;
    m_isConstructor = codeBlock.m_isConstructor;
    m_hasCapturedVariables = codeBlock.m_hasCapturedVariables;
    m_isBuiltinFunction = codeBlock.m_isBuiltinFunction;
    m_superBinding = codeBlock.m_superBinding;
    m_scriptMode = codeBlock.m_scriptMode;
    m_isArrowFunctionContext = codeBlock.m_isArrowFunctionContext;
    m_isClassContext = codeBlock.m_isClassContext;
    m_wasCompiledWithDebuggingOpcodes = codeBlock.m_wasCompiledWithDebuggingOpcodes;
    m_constructorKind = codeBlock.m_constructorKind;
    m_derivedContextType = codeBlock.m_derivedContextType;
    m_evalContextType = codeBlock.m_evalContextType;
    m_hasTailCalls = codeBlock.m_hasTailCalls;
    m_lineCount = codeBlock.m_lineCount;
    m_endColumn = codeBlock.m_endColumn;
    m_numVars = codeBlock.m_numVars;
    m_numCalleeLocals = codeBlock.m_numCalleeLocals;
    m_numParameters = codeBlock.m_numParameters;
    m_features = codeBlock.m_features;
    m_parseMode = codeBlock.m_parseMode;
    m_codeType = codeBlock.m_codeType;

    for (unsigned i = LinkTimeConstantCount; i--;)
        m_linkTimeConstants[i] = codeBlock.m_linkTimeConstants[i];

    m_metadata.encode(encoder, codeBlock.m_metadata.get());
    m_rareData.encode(encoder, codeBlock.m_rareData);

    m_sourceURLDirective.encode(encoder, codeBlock.m_sourceURLDirective.impl());
    m_sourceMappingURLDirective.encode(encoder, codeBlock.m_sourceURLDirective.impl());

    m_instructions.encode(encoder, codeBlock.m_instructions.get());
    m_propertyAccessInstructions.encode(encoder, codeBlock.m_propertyAccessInstructions);
    m_constantRegisters.encode(encoder, codeBlock.m_constantRegisters);
    m_constantsSourceCodeRepresentation.encode(encoder, codeBlock.m_constantsSourceCodeRepresentation);
    m_expressionInfo.encode(encoder, codeBlock.m_expressionInfo);
    m_jumpTargets.encode(encoder, codeBlock.m_jumpTargets);
    m_outOfLineJumpTargets.encode(encoder, codeBlock.m_outOfLineJumpTargets);

    m_identifiers.encode(encoder, codeBlock.m_identifiers);
    m_functionDecls.encode(encoder, codeBlock.m_functionDecls);
    m_functionExprs.encode(encoder, codeBlock.m_functionExprs);
}

class CachedSourceCodeKey : public CachedObject<SourceCodeKey> {
public:
    void encode(Encoder& encoder, const SourceCodeKey& key)
    {
        m_sourceCode.encode(encoder, key.m_sourceCode);
        m_name.encode(encoder, key.m_name);
        m_flags = key.m_flags.m_flags;
        m_hash = key.hash();
        m_functionConstructorParametersEndPosition = key.m_functionConstructorParametersEndPosition;
    }

    void decode(Decoder& decoder, SourceCodeKey& key) const
    {
        m_sourceCode.decode(decoder, key.m_sourceCode);
        m_name.decode(decoder, key.m_name);
        key.m_flags.m_flags = m_flags;
        key.m_hash = m_hash;
        key.m_functionConstructorParametersEndPosition = m_functionConstructorParametersEndPosition;
    }

private:
    CachedUnlinkedSourceCode m_sourceCode;
    CachedString m_name;
    unsigned m_flags;
    unsigned m_hash;
    int m_functionConstructorParametersEndPosition;
};

class GenericCacheEntry {
public:
    std::pair<SourceCodeKey, UnlinkedCodeBlock*> decode(Decoder&) const;

protected:
    CachedCodeBlockTag m_tag;
};

template<typename UnlinkedCodeBlockType>
class CacheEntry : public GenericCacheEntry {
public:
    void encode(Encoder& encoder, std::pair<SourceCodeKey, const UnlinkedCodeBlockType*> pair)
    {
        m_tag = CachedCodeBlockTypeImpl<UnlinkedCodeBlockType>::tag;
        m_key.encode(encoder, pair.first);
        m_codeBlock.encode(encoder, pair.second);
    }

private:
    friend GenericCacheEntry;

    std::pair<SourceCodeKey, UnlinkedCodeBlockType*> decode(Decoder& decoder) const
    {
        ASSERT(m_tag == CachedCodeBlockTypeImpl<UnlinkedCodeBlockType>::tag);
        SourceCodeKey decodedKey;
        m_key.decode(decoder, decodedKey);
        return { WTFMove(decodedKey), m_codeBlock.decode(decoder) };
    }

    CachedSourceCodeKey m_key;
    CachedPtr<CachedCodeBlockType<UnlinkedCodeBlockType>> m_codeBlock;
};

std::pair<SourceCodeKey, UnlinkedCodeBlock*> GenericCacheEntry::decode(Decoder& decoder) const
{
    switch (m_tag) {
    case CachedProgramCodeBlockTag:
        return reinterpret_cast<const CacheEntry<UnlinkedProgramCodeBlock>*>(this)->decode(decoder);
    case CachedModuleCodeBlockTag:
        return reinterpret_cast<const CacheEntry<UnlinkedModuleProgramCodeBlock>*>(this)->decode(decoder);
    case CachedEvalCodeBlockTag:
        // We do not cache eval code blocks
        RELEASE_ASSERT_NOT_REACHED();
    }
    RELEASE_ASSERT_NOT_REACHED();
#if COMPILER(MSVC)
    // Without this, MSVC will complain that this path does not return a value.
    return reinterpret_cast<const CacheEntry<UnlinkedEvalCodeBlock>*>(this)->decode(decoder);
#endif
}

template<typename UnlinkedCodeBlockType>
void encodeCodeBlock(Encoder& encoder, const SourceCodeKey& key, const UnlinkedCodeBlock* codeBlock)
{
    auto* entry = encoder.template malloc<CacheEntry<UnlinkedCodeBlockType>>();
    entry->encode(encoder,  { key, jsCast<const UnlinkedCodeBlockType*>(codeBlock) });
}

std::pair<MallocPtr<uint8_t>, size_t> encodeCodeBlock(VM& vm, const SourceCodeKey& key, const UnlinkedCodeBlock* codeBlock)
{
    const ClassInfo* classInfo = codeBlock->classInfo(vm);

    Encoder encoder(vm);
    if (classInfo == UnlinkedProgramCodeBlock::info())
        encodeCodeBlock<UnlinkedProgramCodeBlock>(encoder, key, codeBlock);
    else if (classInfo == UnlinkedModuleProgramCodeBlock::info())
        encodeCodeBlock<UnlinkedModuleProgramCodeBlock>(encoder, key, codeBlock);
    else
        ASSERT(classInfo == UnlinkedEvalCodeBlock::info());

    return encoder.release();
}

UnlinkedCodeBlock* decodeCodeBlockImpl(VM& vm, const SourceCodeKey& key, const void* buffer, size_t size)
{
    const auto* cachedEntry = reinterpret_cast<const GenericCacheEntry*>(buffer);
    Decoder decoder(vm, buffer, size);
    std::pair<SourceCodeKey, UnlinkedCodeBlock*> entry;
    {
        DeferGC deferGC(vm.heap);
        entry = cachedEntry->decode(decoder);
    }

    if (entry.first != key)
        return nullptr;
    return entry.second;
}

} // namespace JSC
