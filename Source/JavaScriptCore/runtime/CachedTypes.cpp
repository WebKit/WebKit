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

#include "BytecodeCacheError.h"
#include "BytecodeCacheVersion.h"
#include "BytecodeLivenessAnalysis.h"
#include "JSCInlines.h"
#include "JSImmutableButterfly.h"
#include "JSTemplateObjectDescriptor.h"
#include "ScopedArgumentsTable.h"
#include "SourceCodeKey.h"
#include "SourceProvider.h"
#include "UnlinkedEvalCodeBlock.h"
#include "UnlinkedFunctionCodeBlock.h"
#include "UnlinkedMetadataTableInlines.h"
#include "UnlinkedModuleProgramCodeBlock.h"
#include "UnlinkedProgramCodeBlock.h"
#include <wtf/FastMalloc.h>
#include <wtf/Optional.h>
#include <wtf/UUID.h>
#include <wtf/text/AtomStringImpl.h>

namespace JSC {

namespace Yarr {
enum class Flags : uint8_t;
}

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

    Encoder(VM& vm, FileSystem::PlatformFileHandle fd = FileSystem::invalidPlatformFileHandle)
        : m_vm(vm)
        , m_fd(fd)
        , m_baseOffset(0)
        , m_currentPage(nullptr)
    {
        allocateNewPage();
    }

    VM& vm() { return m_vm; }

    Allocation malloc(unsigned size)
    {
        RELEASE_ASSERT(size);
        ptrdiff_t offset;
        if (m_currentPage->malloc(size, offset))
            return Allocation { m_currentPage->buffer() + offset, m_baseOffset + offset };
        allocateNewPage(size);
        return malloc(size);
    }

    template<typename T, typename... Args>
    T* malloc(Args&&... args)
    {
        return new (malloc(sizeof(T)).buffer()) T(std::forward<Args>(args)...);
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

    Optional<ptrdiff_t> cachedOffsetForPtr(const void* ptr)
    {
        auto it = m_ptrToOffsetMap.find(ptr);
        if (it == m_ptrToOffsetMap.end())
            return WTF::nullopt;
        return { it->value };
    }

    void addLeafExecutable(const UnlinkedFunctionExecutable* executable, ptrdiff_t offset)
    {
        m_leafExecutables.add(executable, offset);
    }

    RefPtr<CachedBytecode> release(BytecodeCacheError& error)
    {
        if (!m_currentPage)
            return nullptr;
        m_currentPage->alignEnd();

        if (FileSystem::isHandleValid(m_fd)) {
            return releaseMapped(error);
        }

        size_t size = m_baseOffset + m_currentPage->size();
        MallocPtr<uint8_t> buffer = MallocPtr<uint8_t>::malloc(size);
        unsigned offset = 0;
        for (const auto& page : m_pages) {
            memcpy(buffer.get() + offset, page.buffer(), page.size());
            offset += page.size();
        }
        RELEASE_ASSERT(offset == size);
        return CachedBytecode::create(WTFMove(buffer), size, WTFMove(m_leafExecutables));
    }

private:
    RefPtr<CachedBytecode> releaseMapped(BytecodeCacheError& error)
    {
        size_t size = m_baseOffset + m_currentPage->size();
        if (!FileSystem::truncateFile(m_fd, size)) {
            error = BytecodeCacheError::StandardError(errno);
            return nullptr;
        }

        for (const auto& page : m_pages) {
            int bytesWritten = FileSystem::writeToFile(m_fd, reinterpret_cast<char*>(page.buffer()), page.size());
            if (bytesWritten == -1) {
                error = BytecodeCacheError::StandardError(errno);
                return nullptr;
            }

            if (static_cast<size_t>(bytesWritten) != page.size()) {
                error = BytecodeCacheError::WriteError(bytesWritten, page.size());
                return nullptr;
            }
        }

        bool success;
        FileSystem::MappedFileData mappedFileData(m_fd, FileSystem::MappedFileMode::Private, success);
        if (!success) {
            error = BytecodeCacheError::StandardError(errno);
            return nullptr;
        }

        return CachedBytecode::create(WTFMove(mappedFileData), WTFMove(m_leafExecutables));
    }

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
            ptrdiff_t offset = roundUpToMultipleOf(alignment, m_offset);
            size = roundUpToMultipleOf(alignment, size);
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

        void alignEnd()
        {
            ptrdiff_t size = roundUpToMultipleOf(alignof(std::max_align_t), m_offset);
            if (size == m_offset)
                return;
            RELEASE_ASSERT(static_cast<size_t>(size) <= m_capacity);
            m_offset = size;
        }

    private:
        MallocPtr<uint8_t> m_buffer;
        ptrdiff_t m_offset;
        size_t m_capacity;
    };

    void allocateNewPage(size_t size = 0)
    {
        static size_t minPageSize = pageSize();
        if (m_currentPage) {
            m_currentPage->alignEnd();
            m_baseOffset += m_currentPage->size();
        }
        if (size < minPageSize)
            size = minPageSize;
        else
            size = roundUpToMultipleOf(minPageSize, size);
        m_pages.append(Page { size });
        m_currentPage = &m_pages.last();
    }

    VM& m_vm;
    FileSystem::PlatformFileHandle m_fd;
    ptrdiff_t m_baseOffset;
    Page* m_currentPage;
    Vector<Page> m_pages;
    HashMap<const void*, ptrdiff_t> m_ptrToOffsetMap;
    LeafExecutableMap m_leafExecutables;
};

Decoder::Decoder(VM& vm, Ref<CachedBytecode> cachedBytecode, RefPtr<SourceProvider> provider)
    : m_vm(vm)
    , m_cachedBytecode(WTFMove(cachedBytecode))
    , m_provider(provider)
{
}

Decoder::~Decoder()
{
    for (auto& finalizer : m_finalizers)
        finalizer();
}

Ref<Decoder> Decoder::create(VM& vm, Ref<CachedBytecode> cachedBytecode, RefPtr<SourceProvider> provider)
{
    return adoptRef(*new Decoder(vm, WTFMove(cachedBytecode), WTFMove(provider)));
}

size_t Decoder::size() const
{
    return m_cachedBytecode->size();
}

ptrdiff_t Decoder::offsetOf(const void* ptr)
{
    const uint8_t* addr = static_cast<const uint8_t*>(ptr);
    ASSERT(addr >= m_cachedBytecode->data() && addr < m_cachedBytecode->data() + m_cachedBytecode->size());
    return addr - m_cachedBytecode->data();
}

void Decoder::cacheOffset(ptrdiff_t offset, void* ptr)
{
    m_offsetToPtrMap.add(offset, ptr);
}

WTF::Optional<void*> Decoder::cachedPtrForOffset(ptrdiff_t offset)
{
    auto it = m_offsetToPtrMap.find(offset);
    if (it == m_offsetToPtrMap.end())
        return WTF::nullopt;
    return { it->value };
}

const void* Decoder::ptrForOffsetFromBase(ptrdiff_t offset)
{
    ASSERT(offset > 0 && static_cast<size_t>(offset) < m_cachedBytecode->size());
    return m_cachedBytecode->data() + offset;
}

CompactVariableMap::Handle Decoder::handleForEnvironment(CompactVariableEnvironment* environment) const
{
    auto it = m_environmentToHandleMap.find(environment);
    RELEASE_ASSERT(it != m_environmentToHandleMap.end());
    return it->value;
}

void Decoder::setHandleForEnvironment(CompactVariableEnvironment* environment, const CompactVariableMap::Handle& handle)
{
    auto addResult = m_environmentToHandleMap.add(environment, handle);
    RELEASE_ASSERT(addResult.isNewEntry);
}

void Decoder::addLeafExecutable(const UnlinkedFunctionExecutable* executable, ptrdiff_t offset)
{
    m_cachedBytecode->leafExecutables().add(executable, offset);
}

template<typename Functor>
void Decoder::addFinalizer(const Functor& fn)
{
    m_finalizers.append(fn);
}

RefPtr<SourceProvider> Decoder::provider() const
{
    return m_provider;
}

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
    void* operator new[](size_t, void* where) { return where; }

    // Copied from WTF_FORBID_HEAP_ALLOCATION, since we only want to allow placement new
    void* operator new(size_t) = delete;
    void operator delete(void*) = delete;
    void* operator new[](size_t size) = delete;
    void operator delete[](void*) = delete;
    void* operator new(size_t, NotNullTag, void* location) = delete;
};

template<typename Source>
class VariableLengthObject : public CachedObject<Source>, VariableLengthObjectBase {
    template<typename, typename>
    friend class CachedPtr;
    friend struct CachedPtrOffsets;

public:
    VariableLengthObject()
        : VariableLengthObjectBase(s_invalidOffset)
    {
    }

    bool isEmpty() const
    {
        return m_offset == s_invalidOffset;
    }

protected:
    const uint8_t* buffer() const
    {
        ASSERT(!isEmpty());
        return bitwise_cast<const uint8_t*>(this) + m_offset;
    }

    template<typename T>
    const T* buffer() const
    {
        ASSERT(!(bitwise_cast<uintptr_t>(buffer()) % alignof(T)));
        return bitwise_cast<const T*>(buffer());
    }

    uint8_t* allocate(Encoder& encoder, size_t size)
    {
        ptrdiff_t offsetOffset = encoder.offsetOf(&m_offset);
        auto result = encoder.malloc(size);
        m_offset = result.offset() - offsetOffset;
        return result.buffer();
    }

    template<typename T>
#if CPU(ARM64) && CPU(ADDRESS32)
    // FIXME: Remove this once it's no longer needed and LLVM doesn't miscompile us:
    // <rdar://problem/49792205>
    __attribute__((optnone))
#endif
    T* allocate(Encoder& encoder, unsigned size = 1)
    {
        uint8_t* result = allocate(encoder, sizeof(T) * size);
        ASSERT(!(bitwise_cast<uintptr_t>(result) % alignof(T)));
        return new (result) T[size];
    }

private:
    constexpr static ptrdiff_t s_invalidOffset = std::numeric_limits<ptrdiff_t>::max();
};

template<typename T, typename Source = SourceType<T>>
class CachedPtr : public VariableLengthObject<Source*> {
    template<typename, typename>
    friend class CachedRefPtr;

    friend struct CachedPtrOffsets;

public:
    void encode(Encoder& encoder, const Source* src)
    {
        if (!src)
            return;

        if (Optional<ptrdiff_t> offset = encoder.cachedOffsetForPtr(src)) {
            this->m_offset = *offset - encoder.offsetOf(&this->m_offset);
            return;
        }

        T* cachedObject = this->template allocate<T>(encoder);
        cachedObject->encode(encoder, *src);
        encoder.cachePtr(src, encoder.offsetOf(cachedObject));
    }

    template<typename... Args>
    Source* decode(Decoder& decoder, bool& isNewAllocation, Args&&... args) const
    {
        if (this->isEmpty()) {
            isNewAllocation = false;
            return nullptr;
        }

        ptrdiff_t bufferOffset = decoder.offsetOf(this->buffer());
        if (Optional<void*> ptr = decoder.cachedPtrForOffset(bufferOffset)) {
            isNewAllocation = false;
            return static_cast<Source*>(*ptr);
        }

        isNewAllocation = true;
        Source* ptr = get()->decode(decoder, std::forward<Args>(args)...);
        decoder.cacheOffset(bufferOffset, ptr);
        return ptr;
    }

    template<typename... Args>
    Source* decode(Decoder& decoder, Args&&... args) const
    {
        bool unusedIsNewAllocation;
        return decode(decoder, unusedIsNewAllocation, std::forward<Args>(args)...);
    }

    const T* operator->() const { return get(); }

private:
    const T* get() const
    {
        RELEASE_ASSERT(!this->isEmpty());
        return this->template buffer<T>();
    }
};

ptrdiff_t CachedPtrOffsets::offsetOffset()
{
    return OBJECT_OFFSETOF(CachedPtr<void>, m_offset);
}

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
        bool isNewAllocation;
        Source* decodedPtr = m_ptr.decode(decoder, isNewAllocation);
        if (!decodedPtr)
            return nullptr;
        if (isNewAllocation) {
            decoder.addFinalizer([=] {
                derefIfNotNull(decodedPtr);
            });
        }
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
    friend struct CachedWriteBarrierOffsets;

public:
    bool isEmpty() const { return m_ptr.isEmpty(); }

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

ptrdiff_t CachedWriteBarrierOffsets::ptrOffset()
{
    return OBJECT_OFFSETOF(CachedWriteBarrier<void>, m_ptr);
}

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

template<typename T>
class CachedUniquedStringImplBase : public VariableLengthObject<T> {
public:
    void encode(Encoder& encoder, const StringImpl& string)
    {
        m_isAtomic = string.isAtom();
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
                return AtomStringImpl::add(buffer, m_length).leakRef();

            Identifier ident = Identifier::fromString(decoder.vm(), buffer, m_length);
            String str = decoder.vm().propertyNames->lookUpPrivateName(ident);
            StringImpl* impl = str.releaseImpl().get();
            ASSERT(impl->isSymbol());
            return static_cast<UniquedStringImpl*>(impl);
        };

        if (!m_length) {
            if (m_isSymbol)
                return &SymbolImpl::createNullSymbol().leakRef();
            return AtomStringImpl::add("").leakRef();
        }

        if (m_is8Bit)
            return create(this->template buffer<LChar>());
        return create(this->template buffer<UChar>());
    }

private:
    bool m_is8Bit : 1;
    bool m_isSymbol : 1;
    bool m_isAtomic : 1;
    unsigned m_length;
};

class CachedUniquedStringImpl : public CachedUniquedStringImplBase<UniquedStringImpl> { };
class CachedStringImpl : public CachedUniquedStringImplBase<StringImpl> { };

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

        return Identifier::fromUid(decoder.vm(), (UniquedStringImpl*)str.impl());
    }

    void decode(Decoder& decoder, Identifier& ident) const
    {
        ident = decode(decoder);
    }

private:
    CachedString m_string;
};

template<typename T>
class CachedOptional : public VariableLengthObject<Optional<SourceType<T>>> {
public:
    void encode(Encoder& encoder, const Optional<SourceType<T>>& source)
    {
        if (!source)
            return;

        this->template allocate<T>(encoder)->encode(encoder, *source);
    }

    Optional<SourceType<T>> decode(Decoder& decoder) const
    {
        if (this->isEmpty())
            return WTF::nullopt;

        return { this->template buffer<T>()->decode(decoder) };
    }

    void decode(Decoder& decoder, Optional<SourceType<T>>& dst) const
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

class CachedCompactVariableEnvironment : public CachedObject<CompactVariableEnvironment> {
public:
    void encode(Encoder& encoder, const CompactVariableEnvironment& env)
    {
        m_variables.encode(encoder, env.m_variables);
        m_variableMetadata.encode(encoder, env.m_variableMetadata);
        m_hash = env.m_hash;
        m_isEverythingCaptured = env.m_isEverythingCaptured;
    }

    void decode(Decoder& decoder, CompactVariableEnvironment& env) const
    {
        m_variables.decode(decoder, env.m_variables);
        m_variableMetadata.decode(decoder, env.m_variableMetadata);
        env.m_hash = m_hash;
        env.m_isEverythingCaptured = m_isEverythingCaptured;
    }

    CompactVariableEnvironment* decode(Decoder& decoder) const
    {
        CompactVariableEnvironment* env = new CompactVariableEnvironment;
        decode(decoder, *env);
        return env;
    }

private:
    CachedVector<CachedRefPtr<CachedUniquedStringImpl>> m_variables;
    CachedVector<VariableEnvironmentEntry> m_variableMetadata;
    unsigned m_hash;
    bool m_isEverythingCaptured;
};

class CachedCompactVariableMapHandle : public CachedObject<CompactVariableMap::Handle> {
public:
    void encode(Encoder& encoder, const CompactVariableMap::Handle& handle)
    {
        m_environment.encode(encoder, handle.m_environment);
    }

    CompactVariableMap::Handle decode(Decoder& decoder) const
    {
        bool isNewAllocation;
        CompactVariableEnvironment* environment = m_environment.decode(decoder, isNewAllocation);
        if (!environment) {
            ASSERT(!isNewAllocation);
            return CompactVariableMap::Handle();
        }

        if (!isNewAllocation)
            return decoder.handleForEnvironment(environment);
        bool isNewEntry;
        CompactVariableMap::Handle handle = decoder.vm().m_compactVariableMap->get(environment, isNewEntry);
        if (!isNewEntry) {
            decoder.addFinalizer([=] {
                delete environment;
            });
        }
        decoder.setHandleForEnvironment(environment, handle);
        return handle;
    }

private:
    CachedPtr<CachedCompactVariableEnvironment> m_environment;
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
        m_arguments.encode(encoder, scopedArgumentsTable.m_arguments.get(m_length), m_length);
    }

    ScopedArgumentsTable* decode(Decoder& decoder) const
    {
        ScopedArgumentsTable* scopedArgumentsTable = ScopedArgumentsTable::create(decoder.vm(), m_length);
        m_arguments.decode(decoder, scopedArgumentsTable->m_arguments.get(m_length), m_length);
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
    OptionSet<Yarr::Flags> m_flags;
};

class CachedTemplateObjectDescriptor : public CachedObject<TemplateObjectDescriptor> {
public:
    void encode(Encoder& encoder, const JSTemplateObjectDescriptor& descriptor)
    {
        m_rawStrings.encode(encoder, descriptor.descriptor().rawStrings());
        m_cookedStrings.encode(encoder, descriptor.descriptor().cookedStrings());
        m_endOffset = descriptor.endOffset();
    }

    JSTemplateObjectDescriptor* decode(Decoder& decoder) const
    {
        TemplateObjectDescriptor::StringVector decodedRawStrings;
        TemplateObjectDescriptor::OptionalStringVector decodedCookedStrings;
        m_rawStrings.decode(decoder, decodedRawStrings);
        m_cookedStrings.decode(decoder, decodedCookedStrings);
        return JSTemplateObjectDescriptor::create(decoder.vm(), TemplateObjectDescriptor::create(WTFMove(decodedRawStrings), WTFMove(decodedCookedStrings)), m_endOffset);
    }

private:
    CachedVector<CachedString, 4> m_rawStrings;
    CachedVector<CachedOptional<CachedString>, 4> m_cookedStrings;
    int m_endOffset;
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
            this->allocate<CachedTemplateObjectDescriptor>(encoder)->encode(encoder, *templateObjectDescriptor);
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
            v = jsString(decoder.vm(), adoptRef(*impl));
            break;
        }
        case EncodedType::ImmutableButterfly:
            v = this->buffer<CachedImmutableButterfly>()->decode(decoder);
            break;
        case EncodedType::RegExp:
            v = this->buffer<CachedRegExp>()->decode(decoder);
            break;
        case EncodedType::TemplateObjectDescriptor:
            v = this->buffer<CachedTemplateObjectDescriptor>()->decode(decoder);
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
        m_is32Bit = metadataTable.m_is32Bit;
        if (m_is32Bit) {
            for (unsigned i = UnlinkedMetadataTable::s_offsetTableEntries; i--;)
                m_metadata[i] = metadataTable.offsetTable32()[i];
        } else {
            for (unsigned i = UnlinkedMetadataTable::s_offsetTableEntries; i--;)
                m_metadata[i] = metadataTable.offsetTable16()[i];
        }
    }

    Ref<UnlinkedMetadataTable> decode(Decoder&) const
    {
        if (!m_hasMetadata)
            return UnlinkedMetadataTable::empty();

        Ref<UnlinkedMetadataTable> metadataTable = UnlinkedMetadataTable::create(m_is32Bit);
        metadataTable->m_isFinalized = true;
        metadataTable->m_isLinked = false;
        metadataTable->m_hasMetadata = m_hasMetadata;
        if (m_is32Bit) {
            for (unsigned i = UnlinkedMetadataTable::s_offsetTableEntries; i--;)
                metadataTable->offsetTable32()[i] = m_metadata[i];
        } else {
            for (unsigned i = UnlinkedMetadataTable::s_offsetTableEntries; i--;)
                metadataTable->offsetTable16()[i] = m_metadata[i];
        }
        return metadataTable;
    }

private:
    bool m_hasMetadata;
    bool m_is32Bit;
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
        m_provider.encode(encoder, sourceCode.m_provider);
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
    CachedRefPtr<CachedSourceProvider> m_provider;
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

class CachedSourceCodeWithoutProvider : public CachedObject<SourceCode> {
public:
    void encode(Encoder&, const SourceCode& sourceCode)
    {
        m_hasProvider = !!sourceCode.provider();
        m_startOffset = sourceCode.startOffset();
        m_endOffset = sourceCode.endOffset();
        m_firstLine = sourceCode.firstLine().zeroBasedInt();
        m_startColumn = sourceCode.startColumn().zeroBasedInt();
    }

    void decode(Decoder& decoder, SourceCode& sourceCode) const
    {
        if (m_hasProvider)
            sourceCode.m_provider = decoder.provider();
        sourceCode.m_startOffset = m_startOffset;
        sourceCode.m_endOffset = m_endOffset;
        sourceCode.m_firstLine = OrdinalNumber::fromZeroBasedInt(m_firstLine);
        sourceCode.m_startColumn = OrdinalNumber::fromZeroBasedInt(m_startColumn);
    }

private:
    bool m_hasProvider;
    int m_startOffset;
    int m_endOffset;
    int m_firstLine;
    int m_startColumn;
};

class CachedFunctionExecutableRareData : public CachedObject<UnlinkedFunctionExecutable::RareData> {
public:
    void encode(Encoder& encoder, const UnlinkedFunctionExecutable::RareData& rareData)
    {
        m_classSource.encode(encoder, rareData.m_classSource);
        m_parentScopeTDZVariables.encode(encoder, rareData.m_parentScopeTDZVariables);
    }

    UnlinkedFunctionExecutable::RareData* decode(Decoder& decoder) const
    {
        UnlinkedFunctionExecutable::RareData* rareData = new UnlinkedFunctionExecutable::RareData { };
        m_classSource.decode(decoder, rareData->m_classSource);
        auto parentScopeTDZVariables = m_parentScopeTDZVariables.decode(decoder);
        rareData->m_parentScopeTDZVariables = WTFMove(parentScopeTDZVariables);
        return rareData;
    }

private:
    CachedSourceCodeWithoutProvider m_classSource;
    CachedCompactVariableMapHandle m_parentScopeTDZVariables;
};

class CachedFunctionExecutable : public CachedObject<UnlinkedFunctionExecutable> {
    friend struct CachedFunctionExecutableOffsets;

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

    CodeFeatures features() const { return m_mutableMetadata.m_features; }
    SourceParseMode sourceParseMode() const { return m_sourceParseMode; }

    unsigned isInStrictContext() const { return m_isInStrictContext; }
    unsigned hasCapturedVariables() const { return m_mutableMetadata.m_hasCapturedVariables; }
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

    UnlinkedFunctionExecutable::RareData* rareData(Decoder& decoder) const { return m_rareData.decode(decoder); }

    const CachedWriteBarrier<CachedFunctionCodeBlock, UnlinkedFunctionCodeBlock>& unlinkedCodeBlockForCall() const { return m_unlinkedCodeBlockForCall; }
    const CachedWriteBarrier<CachedFunctionCodeBlock, UnlinkedFunctionCodeBlock>& unlinkedCodeBlockForConstruct() const { return m_unlinkedCodeBlockForConstruct; }

private:
    CachedFunctionExecutableMetadata m_mutableMetadata;

    unsigned m_firstLineOffset : 31;
    unsigned m_isInStrictContext : 1;
    unsigned m_lineCount : 31;
    unsigned m_isBuiltinFunction : 1;
    unsigned m_unlinkedFunctionNameStart : 31;
    unsigned m_isBuiltinDefaultClassConstructor : 1;
    unsigned m_unlinkedBodyStartColumn : 31;
    unsigned m_constructAbility: 1;
    unsigned m_unlinkedBodyEndColumn : 31;
    unsigned m_startOffset : 31;
    unsigned m_scriptMode: 1; // JSParserScriptMode
    unsigned m_sourceLength : 31;
    unsigned m_superBinding : 1;
    unsigned m_parametersStartOffset : 31;
    unsigned m_typeProfilingStartOffset;
    unsigned m_typeProfilingEndOffset;
    unsigned m_parameterCount;
    SourceParseMode m_sourceParseMode;
    unsigned m_constructorKind : 2;
    unsigned m_functionMode : 2; // FunctionMode
    unsigned m_derivedContextType: 2;

    CachedPtr<CachedFunctionExecutableRareData> m_rareData;

    CachedIdentifier m_name;
    CachedIdentifier m_ecmaName;

    CachedWriteBarrier<CachedFunctionCodeBlock, UnlinkedFunctionCodeBlock> m_unlinkedCodeBlockForCall;
    CachedWriteBarrier<CachedFunctionCodeBlock, UnlinkedFunctionCodeBlock> m_unlinkedCodeBlockForConstruct;
};

ptrdiff_t CachedFunctionExecutableOffsets::codeBlockForCallOffset()
{
    return OBJECT_OFFSETOF(CachedFunctionExecutable, m_unlinkedCodeBlockForCall);
}

ptrdiff_t CachedFunctionExecutableOffsets::codeBlockForConstructOffset()
{
    return OBJECT_OFFSETOF(CachedFunctionExecutable, m_unlinkedCodeBlockForConstruct);
}

ptrdiff_t CachedFunctionExecutableOffsets::metadataOffset()
{
    return OBJECT_OFFSETOF(CachedFunctionExecutable, m_mutableMetadata);
}

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
    OptionSet<CodeGenerationMode> codeGenerationMode() const { return m_codeGenerationMode; }
    unsigned codeType() const { return m_codeType; }

    UnlinkedCodeBlock::RareData* rareData(Decoder& decoder) const { return m_rareData.decode(decoder); }

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
    unsigned m_constructorKind : 2;
    unsigned m_derivedContextType : 2;
    unsigned m_evalContextType : 2;
    unsigned m_hasTailCalls : 1;
    unsigned m_codeType : 2;

    CodeFeatures m_features;
    SourceParseMode m_parseMode;
    OptionSet<CodeGenerationMode> m_codeGenerationMode;

    unsigned m_lineCount;
    unsigned m_endColumn;

    int m_numVars;
    int m_numCalleeLocals;
    int m_numParameters;

    CachedMetadataTable m_metadata;

    CachedPtr<CachedCodeBlockRareData> m_rareData;

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

static CachedCodeBlockTag tagFromSourceCodeType(SourceCodeType type)
{
    switch (type) {
    case SourceCodeType::ProgramType:
        return CachedProgramCodeBlockTag;
    case SourceCodeType::EvalType:
        return CachedEvalCodeBlockTag;
    case SourceCodeType::ModuleType:
        return CachedModuleCodeBlockTag;
    case SourceCodeType::FunctionType:
        break;
    }
    ASSERT_NOT_REACHED();
    return static_cast<CachedCodeBlockTag>(-1);
}

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
    , m_hasTailCalls(cachedCodeBlock.hasTailCalls())
    , m_constructorKind(cachedCodeBlock.constructorKind())
    , m_derivedContextType(cachedCodeBlock.derivedContextType())
    , m_evalContextType(cachedCodeBlock.evalContextType())
    , m_codeType(cachedCodeBlock.codeType())

    , m_didOptimize(static_cast<unsigned>(MixedTriState))
    , m_age(0)

    , m_features(cachedCodeBlock.features())
    , m_parseMode(cachedCodeBlock.parseMode())
    , m_codeGenerationMode(cachedCodeBlock.codeGenerationMode())

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
    m_mutableMetadata.m_features = executable.m_features;
    m_mutableMetadata.m_hasCapturedVariables = executable.m_hasCapturedVariables;

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

    m_sourceParseMode = executable.m_sourceParseMode;

    m_isInStrictContext = executable.m_isInStrictContext;
    m_isBuiltinFunction = executable.m_isBuiltinFunction;
    m_isBuiltinDefaultClassConstructor = executable.m_isBuiltinDefaultClassConstructor;
    m_constructAbility = executable.m_constructAbility;
    m_constructorKind = executable.m_constructorKind;
    m_functionMode = executable.m_functionMode;
    m_scriptMode = executable.m_scriptMode;
    m_superBinding = executable.m_superBinding;
    m_derivedContextType = executable.m_derivedContextType;

    m_rareData.encode(encoder, executable.m_rareData.get());

    m_name.encode(encoder, executable.name());
    m_ecmaName.encode(encoder, executable.ecmaName());

    m_unlinkedCodeBlockForCall.encode(encoder, executable.m_unlinkedCodeBlockForCall);
    m_unlinkedCodeBlockForConstruct.encode(encoder, executable.m_unlinkedCodeBlockForConstruct);

    if (!executable.m_unlinkedCodeBlockForCall || !executable.m_unlinkedCodeBlockForConstruct)
        encoder.addLeafExecutable(&executable, encoder.offsetOf(this));
}

ALWAYS_INLINE UnlinkedFunctionExecutable* CachedFunctionExecutable::decode(Decoder& decoder) const
{
    UnlinkedFunctionExecutable* executable = new (NotNull, allocateCell<UnlinkedFunctionExecutable>(decoder.vm().heap)) UnlinkedFunctionExecutable(decoder, *this);
    executable->finishCreation(decoder.vm());
    return executable;
}

ALWAYS_INLINE UnlinkedFunctionExecutable::UnlinkedFunctionExecutable(Decoder& decoder, const CachedFunctionExecutable& cachedExecutable)
    : Base(decoder.vm(), decoder.vm().unlinkedFunctionExecutableStructure.get())
    , m_firstLineOffset(cachedExecutable.firstLineOffset())
    , m_isInStrictContext(cachedExecutable.isInStrictContext())
    , m_lineCount(cachedExecutable.lineCount())
    , m_hasCapturedVariables(cachedExecutable.hasCapturedVariables())
    , m_unlinkedFunctionNameStart(cachedExecutable.unlinkedFunctionNameStart())
    , m_isBuiltinFunction(cachedExecutable.isBuiltinFunction())
    , m_unlinkedBodyStartColumn(cachedExecutable.unlinkedBodyStartColumn())
    , m_isBuiltinDefaultClassConstructor(cachedExecutable.isBuiltinDefaultClassConstructor())
    , m_unlinkedBodyEndColumn(cachedExecutable.unlinkedBodyEndColumn())
    , m_constructAbility(cachedExecutable.constructAbility())
    , m_startOffset(cachedExecutable.startOffset())
    , m_scriptMode(cachedExecutable.scriptMode())
    , m_sourceLength(cachedExecutable.sourceLength())
    , m_superBinding(cachedExecutable.superBinding())
    , m_parametersStartOffset(cachedExecutable.parametersStartOffset())
    , m_isCached(false)
    , m_typeProfilingStartOffset(cachedExecutable.typeProfilingStartOffset())
    , m_typeProfilingEndOffset(cachedExecutable.typeProfilingEndOffset())
    , m_parameterCount(cachedExecutable.parameterCount())
    , m_features(cachedExecutable.features())
    , m_sourceParseMode(cachedExecutable.sourceParseMode())
    , m_constructorKind(cachedExecutable.constructorKind())
    , m_functionMode(cachedExecutable.functionMode())
    , m_derivedContextType(cachedExecutable.derivedContextType())
    , m_isGeneratedFromCache(true)
    , m_unlinkedCodeBlockForCall()
    , m_unlinkedCodeBlockForConstruct()

    , m_name(cachedExecutable.name(decoder))
    , m_ecmaName(cachedExecutable.ecmaName(decoder))

    , m_rareData(cachedExecutable.rareData(decoder))
{

    uint32_t leafExecutables = 2;
    auto checkBounds = [&](int32_t& codeBlockOffset, auto& cachedPtr) {
        if (!cachedPtr.isEmpty()) {
            ptrdiff_t offset = decoder.offsetOf(&cachedPtr);
            if (static_cast<size_t>(offset) < decoder.size()) {
                codeBlockOffset = offset;
                m_isCached = true;
                leafExecutables--;
                return;
            }
        }

        codeBlockOffset = 0;
    };

    if (!cachedExecutable.unlinkedCodeBlockForCall().isEmpty() || !cachedExecutable.unlinkedCodeBlockForConstruct().isEmpty()) {
        checkBounds(m_cachedCodeBlockForCallOffset, cachedExecutable.unlinkedCodeBlockForCall());
        checkBounds(m_cachedCodeBlockForConstructOffset, cachedExecutable.unlinkedCodeBlockForConstruct());
        if (m_isCached)
            m_decoder = &decoder;
        else
            m_decoder = nullptr;
    }

    if (leafExecutables)
        decoder.addLeafExecutable(this, decoder.offsetOf(&cachedExecutable));
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
    m_hasTailCalls = codeBlock.m_hasTailCalls;
    m_constructorKind = codeBlock.m_constructorKind;
    m_derivedContextType = codeBlock.m_derivedContextType;
    m_evalContextType = codeBlock.m_evalContextType;
    m_lineCount = codeBlock.m_lineCount;
    m_endColumn = codeBlock.m_endColumn;
    m_numVars = codeBlock.m_numVars;
    m_numCalleeLocals = codeBlock.m_numCalleeLocals;
    m_numParameters = codeBlock.m_numParameters;
    m_features = codeBlock.m_features;
    m_parseMode = codeBlock.m_parseMode;
    m_codeGenerationMode = codeBlock.m_codeGenerationMode;
    m_codeType = codeBlock.m_codeType;

    for (unsigned i = LinkTimeConstantCount; i--;)
        m_linkTimeConstants[i] = codeBlock.m_linkTimeConstants[i];

    m_metadata.encode(encoder, codeBlock.m_metadata.get());
    m_rareData.encode(encoder, codeBlock.m_rareData.get());

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
    bool decode(Decoder&, std::pair<SourceCodeKey, UnlinkedCodeBlock*>&) const;
    bool isStillValid(Decoder&, const SourceCodeKey&, CachedCodeBlockTag) const;

protected:
    GenericCacheEntry(Encoder& encoder, CachedCodeBlockTag tag)
        : m_tag(tag)
    {
        m_bootSessionUUID.encode(encoder, bootSessionUUIDString());
    }

    CachedCodeBlockTag tag() const { return m_tag; }

    bool isUpToDate(Decoder& decoder) const
    {
        if (m_cacheVersion != JSC_BYTECODE_CACHE_VERSION)
            return false;
        if (m_bootSessionUUID.decode(decoder) != bootSessionUUIDString())
            return false;
        return true;
    }

private:
    uint32_t m_cacheVersion { JSC_BYTECODE_CACHE_VERSION };
    CachedString m_bootSessionUUID;
    CachedCodeBlockTag m_tag;
};

template<typename UnlinkedCodeBlockType>
class CacheEntry : public GenericCacheEntry {
public:
    CacheEntry(Encoder& encoder)
        : GenericCacheEntry(encoder, CachedCodeBlockTypeImpl<UnlinkedCodeBlockType>::tag)
    {
    }

    void encode(Encoder& encoder, std::pair<SourceCodeKey, const UnlinkedCodeBlockType*> pair)
    {
        m_key.encode(encoder, pair.first);
        m_codeBlock.encode(encoder, pair.second);
    }

private:
    friend GenericCacheEntry;

    bool isStillValid(Decoder& decoder, const SourceCodeKey& key) const
    {
        SourceCodeKey decodedKey;
        m_key.decode(decoder, decodedKey);
        return decodedKey == key;
    }

    bool decode(Decoder& decoder, std::pair<SourceCodeKey, UnlinkedCodeBlockType*>& result) const
    {
        ASSERT(tag() == CachedCodeBlockTypeImpl<UnlinkedCodeBlockType>::tag);
        SourceCodeKey decodedKey;
        m_key.decode(decoder, decodedKey);
        result = { WTFMove(decodedKey), m_codeBlock.decode(decoder) };
        return true;
    }

    CachedSourceCodeKey m_key;
    CachedPtr<CachedCodeBlockType<UnlinkedCodeBlockType>> m_codeBlock;
};

bool GenericCacheEntry::decode(Decoder& decoder, std::pair<SourceCodeKey, UnlinkedCodeBlock*>& result) const
{
    if (!isUpToDate(decoder))
        return false;

    switch (m_tag) {
    case CachedProgramCodeBlockTag:
        return bitwise_cast<const CacheEntry<UnlinkedProgramCodeBlock>*>(this)->decode(decoder, reinterpret_cast<std::pair<SourceCodeKey, UnlinkedProgramCodeBlock*>&>(result));
    case CachedModuleCodeBlockTag:
        return bitwise_cast<const CacheEntry<UnlinkedModuleProgramCodeBlock>*>(this)->decode(decoder, reinterpret_cast<std::pair<SourceCodeKey, UnlinkedModuleProgramCodeBlock*>&>(result));
    case CachedEvalCodeBlockTag:
        // We do not cache eval code blocks
        RELEASE_ASSERT_NOT_REACHED();
    }
    RELEASE_ASSERT_NOT_REACHED();
#if COMPILER(MSVC)
    // Without this, MSVC will complain that this path does not return a value.
    return false;
#endif
}

bool GenericCacheEntry::isStillValid(Decoder& decoder, const SourceCodeKey& key, CachedCodeBlockTag tag) const
{
    if (!isUpToDate(decoder))
        return false;

    switch (tag) {
    case CachedProgramCodeBlockTag:
        return bitwise_cast<const CacheEntry<UnlinkedProgramCodeBlock>*>(this)->isStillValid(decoder, key);
    case CachedModuleCodeBlockTag:
        return bitwise_cast<const CacheEntry<UnlinkedModuleProgramCodeBlock>*>(this)->isStillValid(decoder, key);
    case CachedEvalCodeBlockTag:
        // We do not cache eval code blocks
        RELEASE_ASSERT_NOT_REACHED();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

template<typename UnlinkedCodeBlockType>
void encodeCodeBlock(Encoder& encoder, const SourceCodeKey& key, const UnlinkedCodeBlock* codeBlock)
{
    auto* entry = encoder.template malloc<CacheEntry<UnlinkedCodeBlockType>>(encoder);
    entry->encode(encoder, { key, jsCast<const UnlinkedCodeBlockType*>(codeBlock) });
}

RefPtr<CachedBytecode> encodeCodeBlock(VM& vm, const SourceCodeKey& key, const UnlinkedCodeBlock* codeBlock, FileSystem::PlatformFileHandle fd, BytecodeCacheError& error)
{
    const ClassInfo* classInfo = codeBlock->classInfo(vm);

    Encoder encoder(vm, fd);
    if (classInfo == UnlinkedProgramCodeBlock::info())
        encodeCodeBlock<UnlinkedProgramCodeBlock>(encoder, key, codeBlock);
    else if (classInfo == UnlinkedModuleProgramCodeBlock::info())
        encodeCodeBlock<UnlinkedModuleProgramCodeBlock>(encoder, key, codeBlock);
    else
        ASSERT(classInfo == UnlinkedEvalCodeBlock::info());

    return encoder.release(error);
}

RefPtr<CachedBytecode> encodeCodeBlock(VM& vm, const SourceCodeKey& key, const UnlinkedCodeBlock* codeBlock)
{
    BytecodeCacheError error;
    return encodeCodeBlock(vm, key, codeBlock, FileSystem::invalidPlatformFileHandle, error);
}

RefPtr<CachedBytecode> encodeFunctionCodeBlock(VM& vm, const UnlinkedFunctionCodeBlock* codeBlock, BytecodeCacheError& error)
{
    Encoder encoder(vm);
    encoder.malloc<CachedFunctionCodeBlock>()->encode(encoder, *codeBlock);
    return encoder.release(error);
}

UnlinkedCodeBlock* decodeCodeBlockImpl(VM& vm, const SourceCodeKey& key, Ref<CachedBytecode> cachedBytecode)
{
    const auto* cachedEntry = bitwise_cast<const GenericCacheEntry*>(cachedBytecode->data());
    Ref<Decoder> decoder = Decoder::create(vm, WTFMove(cachedBytecode), &key.source().provider());
    std::pair<SourceCodeKey, UnlinkedCodeBlock*> entry;
    {
        DeferGC deferGC(vm.heap);
        if (!cachedEntry->decode(decoder.get(), entry))
            return nullptr;
    }

    if (entry.first != key)
        return nullptr;
    return entry.second;
}

bool isCachedBytecodeStillValid(VM& vm, Ref<CachedBytecode> cachedBytecode, const SourceCodeKey& key, SourceCodeType type)
{
    const void* buffer = cachedBytecode->data();
    size_t size = cachedBytecode->size();
    if (!size)
        return false;
    const auto* cachedEntry = bitwise_cast<const GenericCacheEntry*>(buffer);
    Ref<Decoder> decoder = Decoder::create(vm, WTFMove(cachedBytecode));
    return cachedEntry->isStillValid(decoder.get(), key, tagFromSourceCodeType(type));
}

void decodeFunctionCodeBlock(Decoder& decoder, int32_t cachedFunctionCodeBlockOffset, WriteBarrier<UnlinkedFunctionCodeBlock>& codeBlock, const JSCell* owner)
{
    ASSERT(decoder.vm().heap.isDeferred());
    auto* cachedCodeBlock = static_cast<const CachedWriteBarrier<CachedFunctionCodeBlock, UnlinkedFunctionCodeBlock>*>(decoder.ptrForOffsetFromBase(cachedFunctionCodeBlockOffset));
    cachedCodeBlock->decode(decoder, codeBlock, owner);
}

} // namespace JSC
