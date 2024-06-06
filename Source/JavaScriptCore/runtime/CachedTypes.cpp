/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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

#include "CachedTypesInlines.h"
#include "JSCBytecodeCacheVersion.h"

namespace JSC {

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

std::optional<void*> Decoder::cachedPtrForOffset(ptrdiff_t offset)
{
    auto it = m_offsetToPtrMap.find(offset);
    if (it == m_offsetToPtrMap.end())
        return std::nullopt;
    return { it->value };
}

const void* Decoder::ptrForOffsetFromBase(ptrdiff_t offset)
{
    ASSERT(offset > 0 && static_cast<size_t>(offset) < m_cachedBytecode->size());
    return m_cachedBytecode->data() + offset;
}

CompactTDZEnvironmentMap::Handle Decoder::handleForTDZEnvironment(CompactTDZEnvironment* environment) const
{
    auto it = m_environmentToHandleMap.find(environment);
    RELEASE_ASSERT(it != m_environmentToHandleMap.end());
    return it->value;
}

void Decoder::setHandleForTDZEnvironment(CompactTDZEnvironment* environment, const CompactTDZEnvironmentMap::Handle& handle)
{
    auto addResult = m_environmentToHandleMap.add(environment, handle);
    RELEASE_ASSERT(addResult.isNewEntry);
}

void Decoder::addLeafExecutable(const UnlinkedFunctionExecutable* executable, ptrdiff_t offset)
{
    m_cachedBytecode->leafExecutables().add(executable, offset);
}

RefPtr<SourceProvider> Decoder::provider() const
{
    return m_provider;
}

class GenericCacheEntry {
public:
    bool decode(Decoder&, std::pair<SourceCodeKey, UnlinkedCodeBlock*>&) const;
    bool isStillValid(Decoder&, const SourceCodeKey&, CachedCodeBlockTag) const;

protected:
    GenericCacheEntry(Encoder& encoder, CachedCodeBlockTag tag)
        : m_cacheVersion(computeJSCBytecodeCacheVersion())
        , m_tag(tag)
    {
        m_bootSessionUUID.encode(encoder, bootSessionUUIDString());
    }

    CachedCodeBlockTag tag() const { return m_tag; }

    bool isUpToDate(Decoder& decoder) const
    {
        if (m_cacheVersion != computeJSCBytecodeCacheVersion())
            return false;
        if (m_bootSessionUUID.decode(decoder) != bootSessionUUIDString())
            return false;
        return true;
    }

private:
    uint32_t m_cacheVersion;
    CachedString m_bootSessionUUID;
    CachedCodeBlockTag m_tag;
};

static_assert(alignof(GenericCacheEntry) <= alignof(std::max_align_t));

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

static_assert(alignof(CacheEntry<UnlinkedProgramCodeBlock>) <= alignof(std::max_align_t));
static_assert(alignof(CacheEntry<UnlinkedModuleProgramCodeBlock>) <= alignof(std::max_align_t));

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
    return false;
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
    const ClassInfo* classInfo = codeBlock->classInfo();

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
        DeferGC deferGC(vm);
        if (!cachedEntry->decode(decoder.get(), entry))
            return nullptr;
    }

    if (entry.first != key)
        return nullptr;
    return entry.second;
}

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
