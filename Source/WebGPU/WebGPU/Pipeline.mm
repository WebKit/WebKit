/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Pipeline.h"

#import "APIConversions.h"
#import "Device.h"
#import "Instance.h"
#import "ShaderModule.h"
#import "WGSLShaderModule.h"
#import <dispatch/dispatch.h>
#import <memory>
#import <wtf/BlockPtr.h>
#import <wtf/Borrow.h>
#import <wtf/Condition.h>
#import <wtf/Scope.h>
#import <wtf/darwin/DispatchExtras.h>

#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
#include <csignal>
#include <cstdlib>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#endif

namespace WebGPU::Metal {

#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
static bool enablePsoLogging();
#endif

std::optional<PreparedLibrary> prepareLibrary(const ShaderModule& shaderModule, PipelineLayout* pipelineLayout, const String& entryPoint, NSString *label, std::span<const WebGPU::ConstantEntry> constants, BufferBindingSizesForPipeline& mininumBufferSizes, NSError **error)
{
    HashMap<String, WGSL::ConstantValue> wgslConstantValues;

    if (!entryPoint.length() || !shaderModule.isValid())
        return std::nullopt;

    if (shaderModule.library() && pipelineLayout) {
        if (const RefPtr pipelineLayoutHint = shaderModule.pipelineLayoutHint(entryPoint)) {
            if (*pipelineLayoutHint == *pipelineLayout) {
                if (const auto* entryPointInformation = shaderModule.entryPointInformation(entryPoint))
                    return { { shaderModule.library(), { }, label, { }, *entryPointInformation, wgslConstantValues } };
            }
        }
    }

    auto ast = shaderModule.ast();
    if (!ast)
        return std::nullopt;

    std::optional<WGSL::PipelineLayout> wgslPipelineLayout { std::nullopt };
    if (pipelineLayout && !pipelineLayout->isAutoLayout())
        wgslPipelineLayout = ShaderModule::convertPipelineLayout(*pipelineLayout);

    auto prepareResult = WGSL::prepare(*ast, entryPoint, wgslPipelineLayout ? &*wgslPipelineLayout : nullptr);
    if (std::holds_alternative<WGSL::Error>(prepareResult)) {
        auto wgslError = std::get<WGSL::Error>(prepareResult);
        *error = [NSError errorWithDomain:@"WebGPU" code:1 userInfo:@{ NSLocalizedDescriptionKey: wgslError.message().createNSString().get() }];
        return std::nullopt;
    }

    auto& result = std::get<WGSL::PrepareResult>(prepareResult);
    auto iterator = result.entryPoints.find(entryPoint);
    if (iterator == result.entryPoints.end())
        return std::nullopt;

    const auto& entryPointInformation = iterator->value;

    for (const auto& entry : constants) {
        auto& keyEntry = entry.key;
        auto indexIterator = entryPointInformation.specializationConstants.find(keyEntry);
        if (indexIterator == entryPointInformation.specializationConstants.end()) {
            // Per WebGPU spec: providing a value for an override declared in the
            // shader module but not statically used by this entry point is valid
            // and the value is ignored. Returning an invalid pipeline only when
            // the identifier does not refer to any override in the module.
            if (ast->containsOverride(keyEntry))
                continue;
            return { };
        }

        const auto& specializationConstant = indexIterator->value;
        switch (specializationConstant.type) {
        case WGSL::Reflection::SpecializationConstantType::Boolean: {
            bool value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Float: {
            if (entry.value < std::numeric_limits<float>::lowest() || entry.value > std::numeric_limits<float>::max())
                return std::nullopt;
            float value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Int: {
            if (entry.value < std::numeric_limits<int32_t>::min() || entry.value > std::numeric_limits<int32_t>::max())
                return std::nullopt;
            int value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Unsigned: {
            if (entry.value < 0 || entry.value > std::numeric_limits<uint32_t>::max())
                return std::nullopt;
            unsigned value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Half: {
            constexpr double halfMax = 0x1.ffcp15;
            constexpr double halfLowest = -halfMax;
            if (entry.value < halfLowest || entry.value > halfMax)
                return std::nullopt;
            WGSL::half value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        }
    }

    if (pipelineLayout && !pipelineLayout->isAutoLayout()) {
        for (unsigned i = 0; i < pipelineLayout->numberOfBindGroupLayouts(); ++i) {
            auto& wgslBindGroupLayout = wgslPipelineLayout->bindGroupLayouts[i];
            auto it = mininumBufferSizes.add(i, BufferBindingSizesForBindGroup()).iterator;
            BufferBindingSizesForBindGroup& shaderBindingSizeForBuffer = it->value;
            for (unsigned i = 0; i < wgslBindGroupLayout.entries.size(); ++i) {
                auto& wgslBindGroupLayoutEntry = wgslBindGroupLayout.entries[i];
                auto* wgslBufferBinding = std::get_if<WGSL::BufferBindingLayout>(&wgslBindGroupLayoutEntry.bindingMember);
                if (wgslBufferBinding && wgslBufferBinding->minBindingSize) {
                    auto newValue = wgslBufferBinding->minBindingSize;
                    if (auto existingValueIt = shaderBindingSizeForBuffer.find(wgslBindGroupLayoutEntry.binding); existingValueIt != shaderBindingSizeForBuffer.end())
                        newValue = std::max(newValue, existingValueIt->value);
                    shaderBindingSizeForBuffer.set(wgslBindGroupLayoutEntry.binding, newValue);
                }
            }
        }
    }

    auto generationResult = WGSL::generate(*ast, result, wgslConstantValues, WGSL::DeviceState {
        .appleGPUFamily = shaderModule.device().appleGPUFamily(),
        .shaderValidationEnabled = shaderModule.device().isShaderValidationEnabled()
    });
    if (auto* generationError = std::get_if<WGSL::Error>(&generationResult)) {
        *error = [NSError errorWithDomain:@"WebGPU" code:1 userInfo:@{ NSLocalizedDescriptionKey: generationError->message().createNSString().get() }];
        return std::nullopt;
    }
    auto& msl = std::get<String>(generationResult);

    return { { nil, msl, label, WGSL::DeviceState {
        .appleGPUFamily = shaderModule.device().appleGPUFamily(),
        .shaderValidationEnabled = shaderModule.device().isShaderValidationEnabled(),
        .usesInvariant = entryPointInformation.usesInvariant
    }, entryPointInformation, wgslConstantValues } };
}

LibraryCompileRequest libraryCompileRequest(const PreparedLibrary& preparedLibrary)
{
    return {
        preparedLibrary.library,
        preparedLibrary.msl,
        preparedLibrary.label,
        preparedLibrary.deviceState
    };
}

struct LibraryCacheKey {
    uintptr_t devicePtr { 0 };
    uint32_t appleGPUFamily { 0 };
    uint8_t flags { 0 };
    String msl { };

    friend bool operator==(const LibraryCacheKey&, const LibraryCacheKey&) = default;
};

struct LibraryCacheKeyHash {
    static unsigned hash(const LibraryCacheKey& key)
    {
        return WTF::pairIntHash(WTF::pairIntHash(WTF::intHash(static_cast<uint64_t>(key.devicePtr)), WTF::intHash((key.appleGPUFamily << 8) | key.flags)), StringHash::hash(key.msl));
    }
    static bool equal(const LibraryCacheKey& a, const LibraryCacheKey& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct LibraryCacheKeyTraits : WTF::SimpleClassHashTraits<LibraryCacheKey> {
    static constexpr bool emptyValueIsZero = false;
    static LibraryCacheKey emptyValue() { return { }; }
    static void constructDeletedValue(LibraryCacheKey& slot)
    {
        new (NotNull, std::addressof(slot)) LibraryCacheKey { .devicePtr = std::numeric_limits<uintptr_t>::max() };
    }
    static bool isDeletedValue(const LibraryCacheKey& value) { return value.devicePtr == std::numeric_limits<uintptr_t>::max(); }
};

struct ComputePSOCacheKey {
    uintptr_t libraryPtr { 0 };
    uint32_t bindGroupCount { 0 };
    uint32_t validationState { 0 };
    String mangledName { };

    friend bool operator==(const ComputePSOCacheKey&, const ComputePSOCacheKey&) = default;
};

struct ComputePSOCacheKeyHash {
    static unsigned hash(const ComputePSOCacheKey& key)
    {
        return WTF::pairIntHash(WTF::pairIntHash(WTF::intHash(static_cast<uint64_t>(key.libraryPtr)), WTF::pairIntHash(WTF::intHash(key.bindGroupCount), WTF::intHash(key.validationState))), StringHash::hash(key.mangledName));
    }
    static bool equal(const ComputePSOCacheKey& a, const ComputePSOCacheKey& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct ComputePSOCacheKeyTraits : WTF::SimpleClassHashTraits<ComputePSOCacheKey> {
    static constexpr bool emptyValueIsZero = false;
    static ComputePSOCacheKey emptyValue() { return { }; }
    static void constructDeletedValue(ComputePSOCacheKey& slot)
    {
        new (NotNull, std::addressof(slot)) ComputePSOCacheKey { .libraryPtr = std::numeric_limits<uintptr_t>::max() };
    }
    static bool isDeletedValue(const ComputePSOCacheKey& value) { return value.libraryPtr == std::numeric_limits<uintptr_t>::max(); }
};

struct ComputePSOCacheEntry {
    RetainPtr<id<MTLLibrary>> library;
    RetainPtr<id<MTLComputePipelineState>> pso;
};

struct RenderPSOCacheKey {
    uintptr_t vertexLibraryPtr { 0 };
    uintptr_t fragmentLibraryPtr { 0 };
    String vertexMangledName { };
    String fragmentMangledName { };
    RenderPSODescriptorSignature signature { };

    friend bool operator==(const RenderPSOCacheKey&, const RenderPSOCacheKey&) = default;
};

struct RenderPSOCacheKeyHash {
    static unsigned hash(const RenderPSOCacheKey& key)
    {
        unsigned h = WTF::pairIntHash(WTF::intHash(static_cast<uint64_t>(key.vertexLibraryPtr)), WTF::intHash(static_cast<uint64_t>(key.fragmentLibraryPtr)));
        h = WTF::pairIntHash(h, StringHash::hash(key.vertexMangledName));
        if (!key.fragmentMangledName.isEmpty())
            h = WTF::pairIntHash(h, StringHash::hash(key.fragmentMangledName));
        for (uint64_t w : key.signature.words)
            h = WTF::pairIntHash(h, WTF::intHash(w));
        uint64_t packedMeta = (static_cast<uint64_t>(key.signature.descriptorSampleMask) << 32)
            | (static_cast<uint64_t>(key.signature.depthFormat) << 16)
            | (static_cast<uint64_t>(key.signature.stencilFormat) << 8)
            | (static_cast<uint64_t>(key.signature.rasterSampleCount) << 4)
            | static_cast<uint64_t>(key.signature.topologyAndFlags);
        h = WTF::pairIntHash(h, WTF::intHash(packedMeta));
        return h;
    }
    static bool equal(const RenderPSOCacheKey& a, const RenderPSOCacheKey& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct RenderPSOCacheKeyTraits : WTF::SimpleClassHashTraits<RenderPSOCacheKey> {
    static constexpr bool emptyValueIsZero = false;
    static RenderPSOCacheKey emptyValue() { return { }; }
    static void constructDeletedValue(RenderPSOCacheKey& slot)
    {
        new (NotNull, std::addressof(slot)) RenderPSOCacheKey { .vertexLibraryPtr = std::numeric_limits<uintptr_t>::max() };
    }
    static bool isDeletedValue(const RenderPSOCacheKey& value) { return value.vertexLibraryPtr == std::numeric_limits<uintptr_t>::max(); }
};

struct RenderPSOCacheEntry {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(RenderPSOCacheEntry);
    RetainPtr<id<MTLLibrary>> vertexLibrary;
    RetainPtr<id<MTLLibrary>> fragmentLibrary;
    RetainPtr<id<MTLFunction>> vertexFunction;
    RetainPtr<id<MTLFunction>> fragmentFunction;
    RetainPtr<id<MTLRenderPipelineState>> pso;
};

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
static constexpr size_t cacheGenerationCapacity = 4096;
#else
static constexpr size_t cacheGenerationCapacity = 128;
#endif

template<typename Key, typename Value, typename Hash, typename Traits>
struct TwoGenCache {
    using MapType = HashMap<Key, Value, Hash, Traits>;
    MapType hot;
    MapType cold;

    Value* find(const Key& lookupKey, MapType& evictedOut)
    {
        if (auto it = hot.find(lookupKey); it != hot.end())
            return &it->value;
        auto coldIt = cold.find(lookupKey);
        if (coldIt == cold.end())
            return nullptr;
        Key promotedKey = coldIt->key;
        Value promotedValue = cold.take(coldIt);
        if (hot.size() >= cacheGenerationCapacity)
            evictedOut = std::exchange(cold, std::exchange(hot, { }));
        auto addResult = hot.set(WTF::move(promotedKey), WTF::move(promotedValue));
        return &addResult.iterator->value;
    }

    void set(Key&& key, Value&& value, MapType& evictedOut)
    {
        cold.remove(key);
        if (hot.size() >= cacheGenerationCapacity && !hot.contains(key))
            evictedOut = std::exchange(cold, std::exchange(hot, { }));
        hot.set(WTF::move(key), WTF::move(value));
    }

    std::pair<MapType, MapType> clear()
    {
        return { std::exchange(hot, { }), std::exchange(cold, { }) };
    }
};

struct InFlightLibraryCompilation {
    WTF::Lock syncWaitLock;
    Condition syncWaitCondition;
    bool syncDone WTF_GUARDED_BY_LOCK(syncWaitLock) { false };
    RetainPtr<id<MTLLibrary>> syncLibrary;
    RetainPtr<NSError> syncError;
    Vector<Function<void(id<MTLLibrary>, NSError *)>, 2> asyncWaiters;
};

struct InFlightComputePSOCompilation {
    Vector<Function<void(id<MTLComputePipelineState>, NSString *, bool)>, 2> waiters;
};

struct InFlightRenderPSOCompilation {
    Vector<Function<void(id<MTLFunction>, id<MTLFunction>, id<MTLRenderPipelineState>, NSString *)>, 2> waiters;
};

using LibraryCacheMap = HashMap<LibraryCacheKey, RetainPtr<id<MTLLibrary>>, LibraryCacheKeyHash, LibraryCacheKeyTraits>;
using ComputePSOCacheMap = HashMap<ComputePSOCacheKey, ComputePSOCacheEntry, ComputePSOCacheKeyHash, ComputePSOCacheKeyTraits>;
using RenderPSOCacheMap = HashMap<RenderPSOCacheKey, std::unique_ptr<RenderPSOCacheEntry>, RenderPSOCacheKeyHash, RenderPSOCacheKeyTraits>;

struct MetalPipelineCaches {
    WTF::Lock lock;
    TwoGenCache<LibraryCacheKey, RetainPtr<id<MTLLibrary>>, LibraryCacheKeyHash, LibraryCacheKeyTraits> libraries;
    TwoGenCache<ComputePSOCacheKey, ComputePSOCacheEntry, ComputePSOCacheKeyHash, ComputePSOCacheKeyTraits> computePSOs;
    TwoGenCache<RenderPSOCacheKey, std::unique_ptr<RenderPSOCacheEntry>, RenderPSOCacheKeyHash, RenderPSOCacheKeyTraits> renderPSOs;
    HashMap<LibraryCacheKey, std::shared_ptr<InFlightLibraryCompilation>, LibraryCacheKeyHash, LibraryCacheKeyTraits> inFlightLibraries;
    HashMap<ComputePSOCacheKey, std::shared_ptr<InFlightComputePSOCompilation>, ComputePSOCacheKeyHash, ComputePSOCacheKeyTraits> inFlightComputePSOs;
    HashMap<RenderPSOCacheKey, std::shared_ptr<InFlightRenderPSOCompilation>, RenderPSOCacheKeyHash, RenderPSOCacheKeyTraits> inFlightRenderPSOs;
};

static MetalPipelineCaches& metalPipelineCaches()
{
    static LazyNeverDestroyed<MetalPipelineCaches> caches;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        caches.construct();
    });
    return caches.get();
}

static LibraryCacheKey makeLibraryCacheKey(id<MTLDevice> device, const LibraryCompileRequest& request)
{
    uint8_t flags = (request.deviceState.shaderValidationEnabled ? 1 : 0) | (request.deviceState.usesInvariant ? 2 : 0);
    return {
        .devicePtr = reinterpret_cast<uintptr_t>((__bridge void*)device),
        .appleGPUFamily = request.deviceState.appleGPUFamily,
        .flags = flags,
        .msl = request.mslKey.isolatedCopy(),
    };
}

static LibraryCacheKey makeLibraryLookupKey(id<MTLDevice> device, const LibraryCompileRequest& request)
{
    uint8_t flags = (request.deviceState.shaderValidationEnabled ? 1 : 0) | (request.deviceState.usesInvariant ? 2 : 0);
    return {
        .devicePtr = reinterpret_cast<uintptr_t>((__bridge void*)device),
        .appleGPUFamily = request.deviceState.appleGPUFamily,
        .flags = flags,
        .msl = request.mslKey,
    };
}

id<MTLLibrary> lookupCachedLibrary(id<MTLDevice> device, const LibraryCompileRequest& request)
{
    if (request.cachedLibrary)
        return request.cachedLibrary;
    if (!device || request.mslKey.isEmpty())
        return nil;

    LibraryCacheMap evicted;
    RetainPtr<id<MTLLibrary>> found;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        if (auto* entry = caches.libraries.find(makeLibraryLookupKey(device, request), evicted))
            found = *entry;
    }
    return found.get();
}

void cacheCompiledLibrary(id<MTLDevice> device, const LibraryCompileRequest& request, id<MTLLibrary> library)
{
    if (!device || !library || request.mslKey.isEmpty())
        return;

    LibraryCacheMap evicted;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        caches.libraries.set(makeLibraryCacheKey(device, request), RetainPtr<id<MTLLibrary>> { library }, evicted);
    }
}

id<MTLComputePipelineState> lookupCachedComputePipelineState(id<MTLLibrary> library, const String& mangledName, uint32_t bindGroupCount, uint32_t validationState)
{
    if (!library || mangledName.isEmpty())
        return nil;

    ComputePSOCacheMap evicted;
    RetainPtr<id<MTLComputePipelineState>> found;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        ComputePSOCacheKey lookupKey {
            .libraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)library),
            .bindGroupCount = bindGroupCount,
            .validationState = validationState,
            .mangledName = mangledName,
        };
        if (auto* entry = caches.computePSOs.find(lookupKey, evicted))
            found = entry->pso;
    }
    return found.get();
}

void cacheComputePipelineState(id<MTLLibrary> library, const String& mangledName, uint32_t bindGroupCount, uint32_t validationState, id<MTLComputePipelineState> pso)
{
    if (!library || !pso || mangledName.isEmpty())
        return;

    ComputePSOCacheMap evicted;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        ComputePSOCacheKey key {
            .libraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)library),
            .bindGroupCount = bindGroupCount,
            .validationState = validationState,
            .mangledName = mangledName.isolatedCopy(),
        };
        caches.computePSOs.set(WTF::move(key), ComputePSOCacheEntry { library, pso }, evicted);
    }
}

std::optional<CachedRenderPipelineState> lookupCachedRenderPipelineState(id<MTLLibrary> vertexLibrary, id<MTLLibrary> fragmentLibrary, const String& vertexMangledName, const String& fragmentMangledName, const RenderPSODescriptorSignature& signature)
{
    if (!vertexLibrary || vertexMangledName.isEmpty())
        return std::nullopt;

    RenderPSOCacheMap evicted;
    std::optional<CachedRenderPipelineState> found;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        RenderPSOCacheKey lookupKey {
            .vertexLibraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)vertexLibrary),
            .fragmentLibraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)fragmentLibrary),
            .vertexMangledName = vertexMangledName,
            .fragmentMangledName = fragmentMangledName,
            .signature = signature,
        };
        if (auto* entry = caches.renderPSOs.find(lookupKey, evicted))
            found = CachedRenderPipelineState { (*entry)->vertexFunction, (*entry)->fragmentFunction, (*entry)->pso };
    }
    return found;
}

void cacheRenderPipelineState(id<MTLLibrary> vertexLibrary, id<MTLLibrary> fragmentLibrary, const String& vertexMangledName, const String& fragmentMangledName, const RenderPSODescriptorSignature& signature, id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction, id<MTLRenderPipelineState> pso)
{
    if (!vertexLibrary || !vertexFunction || !pso || vertexMangledName.isEmpty())
        return;

    RenderPSOCacheMap evicted;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        RenderPSOCacheKey key {
            .vertexLibraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)vertexLibrary),
            .fragmentLibraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)fragmentLibrary),
            .vertexMangledName = vertexMangledName.isolatedCopy(),
            .fragmentMangledName = fragmentMangledName.isolatedCopy(),
            .signature = signature,
        };
        caches.renderPSOs.set(WTF::move(key), makeUnique<RenderPSOCacheEntry>(RenderPSOCacheEntry { vertexLibrary, fragmentLibrary, vertexFunction, fragmentFunction, pso }), evicted);
    }
}

static void completeInFlightLibrary(const LibraryCacheKey& ownedKey, const std::shared_ptr<InFlightLibraryCompilation>& inFlight, id<MTLLibrary> library, NSError *error, bool dispatchAsyncWaiters)
{
    LibraryCacheMap evicted;
    Vector<Function<void(id<MTLLibrary>, NSError *)>, 2> waiters;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        if (library) {
            LibraryCacheKey cacheKey {
                .devicePtr = ownedKey.devicePtr,
                .appleGPUFamily = ownedKey.appleGPUFamily,
                .flags = ownedKey.flags,
                .msl = ownedKey.msl.isolatedCopy(),
            };
            caches.libraries.set(WTF::move(cacheKey), RetainPtr<id<MTLLibrary>> { library }, evicted);
        }
        caches.inFlightLibraries.remove(ownedKey);
        waiters = std::exchange(inFlight->asyncWaiters, { });
    }
    {
        Locker waitLocker { inFlight->syncWaitLock };
        inFlight->syncLibrary = library;
        inFlight->syncError = error;
        inFlight->syncDone = true;
        inFlight->syncWaitCondition.notifyAll();
    }
    if (waiters.isEmpty())
        return;
    if (dispatchAsyncWaiters) {
        RetainPtr<id<MTLLibrary>> retainedLib = library;
        RetainPtr<NSError> retainedErr = error;
        @autoreleasepool {
            dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_USER_INITIATED, 0), makeBlockPtr([waiters = WTF::move(waiters), retainedLib = WTF::move(retainedLib), retainedErr = WTF::move(retainedErr)] mutable {
                for (auto& waiter : waiters)
                    waiter(retainedLib.get(), retainedErr.get());
            }).get());
        }
        return;
    }
    for (auto& waiter : waiters)
        waiter(library, error);
}

void compileOrCoalesceLibraryAsync(id<MTLDevice> device, const LibraryCompileRequest& request, Function<void(id<MTLLibrary>, NSError *)>&& callback)
{
    if (request.cachedLibrary) {
        callback(request.cachedLibrary, nil);
        return;
    }
    if (!device || request.mslKey.isEmpty()) {
        callback(nil, nil);
        return;
    }

    LibraryCacheMap evicted;
    RetainPtr<id<MTLLibrary>> hitLibrary;
    LibraryCacheKey ownedKey;
    std::shared_ptr<InFlightLibraryCompilation> inFlight;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        auto lookupKey = makeLibraryLookupKey(device, request);
        if (auto* entry = caches.libraries.find(lookupKey, evicted))
            hitLibrary = *entry;
        else if (auto it = caches.inFlightLibraries.find(lookupKey); it != caches.inFlightLibraries.end()) {
            it->value->asyncWaiters.append(WTF::move(callback));
            return;
        } else {
            ownedKey = makeLibraryCacheKey(device, request);
            inFlight = std::make_shared<InFlightLibraryCompilation>();
            inFlight->asyncWaiters.append(WTF::move(callback));
            caches.inFlightLibraries.set(LibraryCacheKey { ownedKey.devicePtr, ownedKey.appleGPUFamily, ownedKey.flags, ownedKey.msl.isolatedCopy() }, inFlight);
        }
    }

    if (hitLibrary) {
        callback(hitLibrary.get(), nil);
        return;
    }

    ShaderModule::createLibraryAsync(device, request.mslString().get(), request.labelString().get(), request.deviceState, CompletionHandler<void(id<MTLLibrary>, NSError *)> { [ownedKey = WTF::move(ownedKey), inFlight = WTF::move(inFlight)](id<MTLLibrary> library, NSError *error) mutable {
        completeInFlightLibrary(ownedKey, inFlight, library, error, false);
    }, CompletionHandlerCallThread::AnyThread });
}

void compileOrCoalesceComputePSOAsync(id<MTLDevice> device, id<MTLLibrary> library, const String& mangledName, NSString *label, uint32_t bindGroupCount, uint32_t validationStateRaw, Function<void(id<MTLComputePipelineState>, NSString *, bool)>&& callback)
{
    if (!device || !library || mangledName.isEmpty()) {
        callback(nil, @"createComputePipeline failed", false);
        return;
    }

    ComputePSOCacheMap evicted;
    RetainPtr<id<MTLComputePipelineState>> hitPSO;
    ComputePSOCacheKey ownedKey;
    std::shared_ptr<InFlightComputePSOCompilation> inFlight;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        ComputePSOCacheKey lookupKey {
            .libraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)library),
            .bindGroupCount = bindGroupCount,
            .validationState = validationStateRaw,
            .mangledName = mangledName,
        };
        if (auto* entry = caches.computePSOs.find(lookupKey, evicted))
            hitPSO = entry->pso;
        else if (auto it = caches.inFlightComputePSOs.find(lookupKey); it != caches.inFlightComputePSOs.end()) {
            it->value->waiters.append(WTF::move(callback));
            return;
        } else {
            ownedKey = ComputePSOCacheKey {
                .libraryPtr = lookupKey.libraryPtr,
                .bindGroupCount = bindGroupCount,
                .validationState = validationStateRaw,
                .mangledName = mangledName.isolatedCopy(),
            };
            inFlight = std::make_shared<InFlightComputePSOCompilation>();
            inFlight->waiters.append(WTF::move(callback));
            caches.inFlightComputePSOs.set(ComputePSOCacheKey { ownedKey.libraryPtr, ownedKey.bindGroupCount, ownedKey.validationState, ownedKey.mangledName.isolatedCopy() }, inFlight);
        }
    }

    if (hitPSO) {
        callback(hitPSO.get(), nil, false);
        return;
    }

    RetainPtr<id<MTLDevice>> retainedDevice = device;
    RetainPtr<id<MTLLibrary>> retainedLibrary = library;
    RetainPtr<NSString> retainedLabel = adoptNS([label copy]);
    RetainPtr<NSString> functionName = mangledName.createNSString();
    @autoreleasepool {
        dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_USER_INITIATED, 0), makeBlockPtr([retainedDevice = WTF::move(retainedDevice), retainedLibrary = WTF::move(retainedLibrary), retainedLabel = WTF::move(retainedLabel), functionName = WTF::move(functionName), bindGroupCount, validationStateRaw, ownedKey = WTF::move(ownedKey), inFlight = WTF::move(inFlight)] mutable {
            auto completeAll = [](const ComputePSOCacheKey& key, const std::shared_ptr<InFlightComputePSOCompilation>& state, id<MTLLibrary> lib, id<MTLComputePipelineState> pso, NSString *errMsg, bool isCompileFailure) {
                ComputePSOCacheMap localEvicted;
                Vector<Function<void(id<MTLComputePipelineState>, NSString *, bool)>, 2> waiters;
                {
                    auto& caches = metalPipelineCaches();
                    Locker locker { caches.lock };
                    if (pso) {
                        ComputePSOCacheKey cacheKey {
                            .libraryPtr = key.libraryPtr,
                            .bindGroupCount = key.bindGroupCount,
                            .validationState = key.validationState,
                            .mangledName = key.mangledName.isolatedCopy(),
                        };
                        caches.computePSOs.set(WTF::move(cacheKey), ComputePSOCacheEntry { lib, pso }, localEvicted);
                    }
                    caches.inFlightComputePSOs.remove(key);
                    waiters = std::exchange(state->waiters, { });
                }
                for (auto& waiter : waiters)
                    waiter(pso, errMsg, isCompileFailure);
            };

            auto function = createFunction(retainedLibrary.get(), functionName.get(), retainedLabel.get());
            if (!function || function.functionType != MTLFunctionTypeKernel) {
                completeAll(ownedKey, inFlight, retainedLibrary.get(), nil, @"createComputePipeline failed", false);
                return;
            }
            auto desc = [MTLComputePipelineDescriptor new];
#if ENABLE(WEBGPU_BY_DEFAULT)
            desc.shaderValidation = static_cast<GPUShaderValidation>(validationStateRaw);
#else
            UNUSED_PARAM(validationStateRaw);
#endif
            desc.computeFunction = function;
            for (uint32_t i = 0; i < bindGroupCount; ++i)
                desc.buffers[i].mutability = MTLMutabilityImmutable;
            desc.label = retainedLabel.get();

            @autoreleasepool {
                [retainedDevice.get() newComputePipelineStateWithDescriptor:desc options:MTLPipelineOptionNone completionHandler:makeBlockPtr([retainedLibrary = WTF::move(retainedLibrary), ownedKey = WTF::move(ownedKey), inFlight = WTF::move(inFlight), completeAll](id<MTLComputePipelineState> pso, MTLComputePipelineReflection *, NSError *error) mutable {
                    @autoreleasepool {
                        RetainPtr<id<MTLComputePipelineState>> retainedPSO = pso;
                        RetainPtr<NSString> errMsg = error.localizedDescription;
                        dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_USER_INITIATED, 0), makeBlockPtr([retainedLibrary = WTF::move(retainedLibrary), retainedPSO = WTF::move(retainedPSO), errMsg = WTF::move(errMsg), ownedKey = WTF::move(ownedKey), inFlight = WTF::move(inFlight), completeAll] mutable {
                            completeAll(ownedKey, inFlight, retainedLibrary.get(), retainedPSO.get(), errMsg.get() ?: @"GPUCompuePipeline could not compile", !retainedPSO);
                        }).get());
                    }
                }).get()];
            }
        }).get());
    }
}

void compileOrCoalesceRenderPSOAsync(id<MTLDevice> device, id<MTLLibrary> vertexLibrary, id<MTLLibrary> fragmentLibrary, const String& vertexMangledName, const String& fragmentMangledName, const RenderPSODescriptorSignature& signature, MTLRenderPipelineDescriptor *descriptor, NSString *label, Function<void(id<MTLFunction>, id<MTLFunction>, id<MTLRenderPipelineState>, NSString *)>&& callback)
{
    if (!device || !vertexLibrary || vertexMangledName.isEmpty()) {
        callback(nil, nil, nil, @"Vertex function could not be created");
        return;
    }

    RenderPSOCacheMap evicted;
    std::optional<CachedRenderPipelineState> hitRenderPSO;
    RenderPSOCacheKey ownedKey;
    std::shared_ptr<InFlightRenderPSOCompilation> inFlight;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        RenderPSOCacheKey lookupKey {
            .vertexLibraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)vertexLibrary),
            .fragmentLibraryPtr = reinterpret_cast<uintptr_t>((__bridge void*)fragmentLibrary),
            .vertexMangledName = vertexMangledName,
            .fragmentMangledName = fragmentMangledName,
            .signature = signature,
        };
        if (auto* entry = caches.renderPSOs.find(lookupKey, evicted))
            hitRenderPSO = CachedRenderPipelineState { (*entry)->vertexFunction, (*entry)->fragmentFunction, (*entry)->pso };
        else if (auto it = caches.inFlightRenderPSOs.find(lookupKey); it != caches.inFlightRenderPSOs.end()) {
            it->value->waiters.append(WTF::move(callback));
            return;
        } else {
            ownedKey = RenderPSOCacheKey {
                .vertexLibraryPtr = lookupKey.vertexLibraryPtr,
                .fragmentLibraryPtr = lookupKey.fragmentLibraryPtr,
                .vertexMangledName = vertexMangledName.isolatedCopy(),
                .fragmentMangledName = fragmentMangledName.isolatedCopy(),
                .signature = signature,
            };
            inFlight = std::make_shared<InFlightRenderPSOCompilation>();
            inFlight->waiters.append(WTF::move(callback));
            caches.inFlightRenderPSOs.set(RenderPSOCacheKey { ownedKey.vertexLibraryPtr, ownedKey.fragmentLibraryPtr, ownedKey.vertexMangledName.isolatedCopy(), ownedKey.fragmentMangledName.isolatedCopy(), ownedKey.signature }, inFlight);
        }
    }

    if (hitRenderPSO) {
        callback(hitRenderPSO->vertexFunction.get(), hitRenderPSO->fragmentFunction.get(), hitRenderPSO->pso.get(), nil);
        return;
    }

    RetainPtr<id<MTLDevice>> retainedDevice = device;
    RetainPtr<id<MTLLibrary>> retainedVLib = vertexLibrary;
    RetainPtr<id<MTLLibrary>> retainedFLib = fragmentLibrary;
    RetainPtr<MTLRenderPipelineDescriptor> bgDescriptor = adoptNS([descriptor copy]);
    RetainPtr<NSString> retainedLabel = adoptNS([label copy]);
    RetainPtr<NSString> vertexFunctionName = vertexMangledName.createNSString();
    RetainPtr<NSString> fragmentFunctionName = fragmentLibrary ? fragmentMangledName.createNSString() : nil;

    @autoreleasepool {
        dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_USER_INITIATED, 0), makeBlockPtr([retainedDevice = WTF::move(retainedDevice), retainedVLib = WTF::move(retainedVLib), retainedFLib = WTF::move(retainedFLib), bgDescriptor = WTF::move(bgDescriptor), retainedLabel = WTF::move(retainedLabel), vertexFunctionName = WTF::move(vertexFunctionName), fragmentFunctionName = WTF::move(fragmentFunctionName), ownedKey = WTF::move(ownedKey), inFlight = WTF::move(inFlight)] mutable {
            auto completeAll = [](const RenderPSOCacheKey& key, const std::shared_ptr<InFlightRenderPSOCompilation>& state, id<MTLLibrary> vLib, id<MTLLibrary> fLib, id<MTLFunction> vFunc, id<MTLFunction> fFunc, id<MTLRenderPipelineState> pso, NSString *funcErr) {
                RenderPSOCacheMap localEvicted;
                Vector<Function<void(id<MTLFunction>, id<MTLFunction>, id<MTLRenderPipelineState>, NSString *)>, 2> waiters;
                {
                    auto& caches = metalPipelineCaches();
                    Locker locker { caches.lock };
                    if (pso && vFunc) {
                        RenderPSOCacheKey cacheKey {
                            .vertexLibraryPtr = key.vertexLibraryPtr,
                            .fragmentLibraryPtr = key.fragmentLibraryPtr,
                            .vertexMangledName = key.vertexMangledName.isolatedCopy(),
                            .fragmentMangledName = key.fragmentMangledName.isolatedCopy(),
                            .signature = key.signature,
                        };
                        caches.renderPSOs.set(WTF::move(cacheKey), makeUnique<RenderPSOCacheEntry>(RenderPSOCacheEntry { vLib, fLib, vFunc, fFunc, pso }), localEvicted);
                    }
                    caches.inFlightRenderPSOs.remove(key);
                    waiters = std::exchange(state->waiters, { });
                }
                for (auto& waiter : waiters)
                    waiter(vFunc, fFunc, pso, funcErr);
            };

            id<MTLFunction> vFunc = createFunction(retainedVLib.get(), vertexFunctionName.get(), retainedLabel.get());
            if (!vFunc || vFunc.functionType != MTLFunctionTypeVertex) {
                completeAll(ownedKey, inFlight, retainedVLib.get(), retainedFLib.get(), nil, nil, nil, @"Vertex function could not be created");
                return;
            }
            bgDescriptor.get().vertexFunction = vFunc;

            id<MTLFunction> fFunc = nil;
            if (retainedFLib) {
                fFunc = createFunction(retainedFLib.get(), fragmentFunctionName.get(), retainedLabel.get());
                if (!fFunc || fFunc.functionType != MTLFunctionTypeFragment) {
                    completeAll(ownedKey, inFlight, retainedVLib.get(), retainedFLib.get(), nil, nil, nil, @"Fragment function failed creation");
                    return;
                }
                bgDescriptor.get().fragmentFunction = fFunc;
            }

            RetainPtr<id<MTLFunction>> retainedVFunc = vFunc;
            RetainPtr<id<MTLFunction>> retainedFFunc = fFunc;
            @autoreleasepool {
                [retainedDevice.get() newRenderPipelineStateWithDescriptor:bgDescriptor.get() completionHandler:makeBlockPtr([retainedVLib = WTF::move(retainedVLib), retainedFLib = WTF::move(retainedFLib), retainedVFunc = WTF::move(retainedVFunc), retainedFFunc = WTF::move(retainedFFunc), ownedKey = WTF::move(ownedKey), inFlight = WTF::move(inFlight), completeAll](id<MTLRenderPipelineState> pso, NSError *) mutable {
                    @autoreleasepool {
                        RetainPtr<id<MTLRenderPipelineState>> retainedPSO = pso;
                        dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_USER_INITIATED, 0), makeBlockPtr([retainedVLib = WTF::move(retainedVLib), retainedFLib = WTF::move(retainedFLib), retainedVFunc = WTF::move(retainedVFunc), retainedFFunc = WTF::move(retainedFFunc), retainedPSO = WTF::move(retainedPSO), ownedKey = WTF::move(ownedKey), inFlight = WTF::move(inFlight), completeAll] mutable {
                            completeAll(ownedKey, inFlight, retainedVLib.get(), retainedFLib.get(), retainedVFunc.get(), retainedFFunc.get(), retainedPSO.get(), nil);
                        }).get());
                    }
                }).get()];
            }
        }).get());
    }
}

void purgeMetalPipelineCaches()
{
    std::pair<RenderPSOCacheMap, RenderPSOCacheMap> evictedRenderPSOs;
    std::pair<ComputePSOCacheMap, ComputePSOCacheMap> evictedPSOs;
    std::pair<LibraryCacheMap, LibraryCacheMap> evictedLibraries;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        evictedRenderPSOs = caches.renderPSOs.clear();
        evictedPSOs = caches.computePSOs.clear();
        evictedLibraries = caches.libraries.clear();
    }
}

id<MTLLibrary> compileLibrary(id<MTLDevice> device, const LibraryCompileRequest& request, NSError **error)
{
    if (request.cachedLibrary)
        return request.cachedLibrary;
    if (!device || request.mslKey.isEmpty())
        return nil;

    LibraryCacheMap evicted;
    RetainPtr<id<MTLLibrary>> hitLibrary;
    std::shared_ptr<InFlightLibraryCompilation> existingInFlight;
    std::shared_ptr<InFlightLibraryCompilation> ownedInFlight;
    LibraryCacheKey ownedKey;
    {
        auto& caches = metalPipelineCaches();
        Locker locker { caches.lock };
        auto lookupKey = makeLibraryLookupKey(device, request);
        if (auto* entry = caches.libraries.find(lookupKey, evicted))
            hitLibrary = *entry;
        else if (auto it = caches.inFlightLibraries.find(lookupKey); it != caches.inFlightLibraries.end())
            existingInFlight = it->value;
        else {
            ownedKey = makeLibraryCacheKey(device, request);
            ownedInFlight = std::make_shared<InFlightLibraryCompilation>();
            caches.inFlightLibraries.set(LibraryCacheKey { ownedKey.devicePtr, ownedKey.appleGPUFamily, ownedKey.flags, ownedKey.msl.isolatedCopy() }, ownedInFlight);
        }
    }

    if (hitLibrary)
        return hitLibrary.get();

    if (existingInFlight) {
        Locker waitLocker { existingInFlight->syncWaitLock };
        while (!existingInFlight->syncDone)
            existingInFlight->syncWaitCondition.wait(existingInFlight->syncWaitLock);
        if (error)
            *error = existingInFlight->syncError.get();
        return existingInFlight->syncLibrary.get();
    }

    NSError *localError = nil;
    id<MTLLibrary> library = ShaderModule::createLibrary(device, request.mslString().get(), request.labelString().get(), &localError, request.deviceState);
    if (error)
        *error = localError;
    completeInFlightLibrary(ownedKey, ownedInFlight, library, localError, true);
    return library;
}

template<typename Result>
static CompletionHandler<void(Result, NSError *)> callOnWebGPUThread(Instance& instance, CompletionHandler<void(Result, NSError *)>&& callback)
{
    using Callback = CompletionHandler<void(Result, NSError *)>;

    auto* callbackToHopBack = new Callback(WTF::move(callback));
    // Metal runs its completion handlers on its own queues, so the returned handler is called on an
    // arbitrary thread. It is the hop below which returns to the thread `callback` was created on.
    return Callback { [asyncCompileInstance = AsyncCompileInstance { instance }, callbackToHopBack](Result result, NSError *error) mutable {
        auto localInstance = WTF::move(asyncCompileInstance);
        if (RefPtr instance = localInstance.take()) {
            instance->scheduleWork([callbackToHopBack, result, error] mutable {
                std::unique_ptr<Callback> callback(callbackToHopBack);
                (*callback)(result, error);
            });
        }
    }, CompletionHandlerCallThread::AnyThread };
}

void compileLibraryAsync(id<MTLDevice> device, Instance& instance, const LibraryCompileRequest& request, CompletionHandler<void(id<MTLLibrary>, NSError *)>&& callback)
{
    // A library which was already compiled needs no hop: this is still running on the WebGPU thread.
    if (id<MTLLibrary> cached = lookupCachedLibrary(device, request)) {
        callback(cached, nil);
        return;
    }

    // nil device here means the device is either lost or destroyed, all which report no error
    if (!device) {
        callback(nil, nil);
        return;
    }

    compileOrCoalesceLibraryAsync(device, request, [hopBack = callOnWebGPUThread<id<MTLLibrary>>(instance, WTF::move(callback))](id<MTLLibrary> library, NSError *error) mutable {
        hopBack(library, error);
    });
}

void createComputePipelineStateAsync(id<MTLDevice> device, Instance& instance, MTLComputePipelineDescriptor *descriptor, CompletionHandler<void(id<MTLComputePipelineState>, NSError *)>&& callback)
{
    // See the comment in compileLibraryAsync(): a lost device is nil and would never call back.
    if (!device) {
        callback(nil, nil);
        return;
    }

    [device newComputePipelineStateWithDescriptor:descriptor options:MTLPipelineOptionNone completionHandler:makeBlockPtr([hopBack = callOnWebGPUThread<id<MTLComputePipelineState>>(instance, WTF::move(callback))](id<MTLComputePipelineState> computePipelineState, MTLComputePipelineReflection *, NSError *error) mutable {
        hopBack(computePipelineState, error);
    }).get()];
}

void createRenderPipelineStateAsync(id<MTLDevice> device, Instance& instance, MTLRenderPipelineDescriptor *descriptor, CompletionHandler<void(id<MTLRenderPipelineState>, NSError *)>&& callback)
{
    // See the comment in compileLibraryAsync(): a lost device is nil and would never call back.
    if (!device) {
        callback(nil, nil);
        return;
    }

    [device newRenderPipelineStateWithDescriptor:descriptor completionHandler:makeBlockPtr([hopBack = callOnWebGPUThread<id<MTLRenderPipelineState>>(instance, WTF::move(callback))](id<MTLRenderPipelineState> renderPipelineState, NSError *error) mutable {
        hopBack(renderPipelineState, error);
    }).get()];
}

ScopeExit<Function<void()>> scopedErrorReporting(Device& device, bool suppressErrors)
{
    bool wasErrorReportingPaused = device.pauseErrorReporting(suppressErrors);
    return makeScopeExit(Function<void()> { [protectedDevice = protect(device), wasErrorReportingPaused] {
        protectedDevice->pauseErrorReporting(wasErrorReportingPaused);
    } });
}

Device::LibraryCompilation Device::asynchronousIfPossible()
{
#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
    if (enablePsoLogging())
        return LibraryCompilation::Synchronous;
#endif
    return LibraryCompilation::Asynchronous;
}

void Device::compileLibrary(const LibraryCompileRequest& request, LibraryCompilation libraryCompilation, CompletionHandler<void(id<MTLLibrary>, NSError *)>&& callback)
{
    if (libraryCompilation == LibraryCompilation::Asynchronous) {
        if (RefPtr protectedInstance = weakInstance()) {
            compileLibraryAsync(m_device, *protectedInstance, request, WTF::move(callback));
            return;
        }
    }

    NSError *error = nil;
    id<MTLLibrary> library = WebGPU::Metal::compileLibrary(m_device, request, &error);
    callback(library, error);
}

void Device::createComputePipelineState(MTLComputePipelineDescriptor *descriptor, String&& shaderSource, LibraryCompilation libraryCompilation, CompletionHandler<void(id<MTLComputePipelineState>)>&& callback)
{
    auto reportError = [](NSError *error) {
        if (error)
            WTFLogAlways("Pipeline state creation error: %@", error); // NOLINT
    };

    if (libraryCompilation == LibraryCompilation::Asynchronous) {
        if (RefPtr protectedInstance = weakInstance()) {
            createComputePipelineStateAsync(m_device, *protectedInstance, descriptor, [reportError, callback = WTF::move(callback)](id<MTLComputePipelineState> computePipelineState, NSError *error) mutable {
                reportError(error);
                callback(computePipelineState);
            });
            return;
        }
    }

    NSError *error = nil;
#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
    dumpMetalReproCaseComputePSO(WTF::move(shaderSource), descriptor.computeFunction.name);
#else
    UNUSED_PARAM(shaderSource);
#endif
    id<MTLComputePipelineState> computePipelineState = [m_device newComputePipelineStateWithDescriptor:descriptor options:MTLPipelineOptionNone reflection:nil error:&error];
#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
    clearMetalPSORepro();
#endif
    reportError(error);
    callback(computePipelineState);
}

id<MTLFunction> createFunction(id<MTLLibrary> library, NSString *functionName, NSString *label)
{
    auto functionDescriptor = [MTLFunctionDescriptor new];
    functionDescriptor.name = functionName;
    NSError *error = nil;
    id<MTLFunction> function = [library newFunctionWithDescriptor:functionDescriptor error:&error];
    if (error)
        WTFLogAlways("Function creation error: %@", error);
    function.label = label;
    return function;
}

id<MTLFunction> createFunction(id<MTLLibrary> library, const WGSL::Reflection::EntryPointInformation& entryPointInformation, NSString *label)
{
    return createFunction(library, entryPointInformation.mangledName.createNSString().get(), label);
}

NSString* errorValidatingBindGroup(const BindGroup& bindGroup, const BufferBindingSizesForBindGroup* mininumBufferSizes, const Vector<uint32_t>* dynamicOffsets)
{
    RefPtr bindGroupLayout = bindGroup.bindGroupLayout();
    if (!bindGroupLayout)
        return nil;

    auto& bindGroupLayoutEntries = bindGroupLayout->entries();
    for (const auto& resourceVector : bindGroup.resources()) {
        for (const auto& resource : borrow(resourceVector.resourceUsages)) {
            auto bindingIndex = resource.binding;
            auto* buffer = get_if<RefPtr<Buffer>>(&resource.resource);
            if (!buffer)
                continue;

            auto it = bindGroupLayoutEntries.find(bindingIndex);
            if (it == bindGroupLayoutEntries.end())
                return [NSString stringWithFormat:@"Buffer size is missing for binding at index %u bind group", bindingIndex];

            uint64_t bufferSize = 0;
            if (auto* bufferBinding = get_if<BindGroupLayout::BufferBindingLayout>(&it->value.bindingLayout))
                bufferSize = bufferBinding->minBindingSize;
            if (mininumBufferSizes) {
                if (auto bufferSizeIt = mininumBufferSizes->find(it->value.binding); bufferSizeIt != mininumBufferSizes->end()) {
                    if (bufferSize && bufferSizeIt->value > bufferSize)
                        return [NSString stringWithFormat:@"buffer size from WGSL shader(%llu) is less than the binding buffer size(%llu)", bufferSizeIt->value, bufferSize];
                    bufferSize = std::max(bufferSize, bufferSizeIt->value);
                }
            }

            if (bufferSize && buffer->get()) {
                auto dynamicOffset = bindGroup.dynamicOffset(bindingIndex, dynamicOffsets);
                auto checkedTotalOffset = checkedSum<uint64_t>(resource.entryOffset, dynamicOffset);
                if (checkedTotalOffset.hasOverflowed())
                    return [NSString stringWithFormat:@"resourceOffset(%llu) + dynamicOffset(%u) overflows uint64_t", resource.entryOffset, dynamicOffset];
                auto totalOffset = checkedTotalOffset.value();
                auto mtlBufferLength = buffer->get()->currentSize();
                if (totalOffset > mtlBufferLength || (mtlBufferLength - totalOffset) < bufferSize || bufferSize > resource.entrySize)
                    return [NSString stringWithFormat:@"buffer length(%llu) minus offset(%llu), (resourceOffset(%llu) + dynamicOffset(%u)), is less than required bufferSize(%llu)", mtlBufferLength, totalOffset, resource.entryOffset, dynamicOffset, bufferSize];
            }
        }
    }
    return nil;
}

#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
static bool enablePsoLogging()
{
#if defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED
    return true;
#else
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitWebGPULogPSOState"];
#endif
}

static StringBuilder& psoReproStringBuilder()
{
    __attribute__((no_destroy)) static thread_local StringBuilder sb;
    return sb;
}

static Vector<void (*)(int)>& existingSigabortHandlers()
{
    __attribute__((no_destroy)) static Vector<void (*)(int)> handlers;
    return handlers;
}

static String psoPreamble()
{
    return R"(
    // Compile and run with:
    //
    //  clang++ -fobjc-arc -O3 psoRepro.mm -o repro -framework Metal -fsanitize=address -framework Cocoa -std=c++20 && AGC_ENABLE_STATUS_FILE=1 FS_CACHE_SIZE=0 MTL_MONOLITHIC_COMPILER=1 ./repro
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

    static uint32_t computeAppleGPUFamily(id<MTLDevice> device)
    {
        if ([device supportsFamily:MTLGPUFamilyApple9])
            return 9;
        if ([device supportsFamily:MTLGPUFamilyApple8])
            return 8;
        if ([device supportsFamily:MTLGPUFamilyApple7])
            return 7;
        if ([device supportsFamily:MTLGPUFamilyApple6])
            return 6;
        if ([device supportsFamily:MTLGPUFamilyApple5])
            return 5;
        if ([device supportsFamily:MTLGPUFamilyApple4])
            return 4;
        return 0xFF;
    }
)"_s;
}

static void printPsoOnProgramExit(int sig)
{
    if (psoReproStringBuilder().length())
        /* NOLINT */ SAFE_WTFLOGALWAYS("// ------------------------------------------------------------------------\n// Dumping Metal repro case:\n// ------------------------------------------------------------------------\n%s\n// ------------------------------------------------------------------------\n// End of repro case\n// ------------------------------------------------------------------------", makeUTF8CString(psoReproStringBuilder().toString()));

    if (existingSigabortHandlers()[sig])
        existingSigabortHandlers()[sig](sig);
}

static bool registerPsoExitHandlers()
{
    static std::once_flag onceFlag;
    static bool handlersRegistered = false;
    std::call_once(onceFlag, [&] {
        handlersRegistered = true;
        auto sigCodes = { SIGINT, SIGILL, SIGTRAP, SIGABRT, SIGBUS, SIGSEGV };
        existingSigabortHandlers().resize(32);
        for (auto sig : sigCodes) {
            void (*prev)(int) = signal(sig, SIG_DFL);
            RELEASE_ASSERT(static_cast<size_t>(sig) < existingSigabortHandlers().size());
            existingSigabortHandlers()[sig] = prev;
            errno = 0;
            if (signal(sig, printPsoOnProgramExit) == SIG_ERR) {
                WTFLogAlways("Error: Unable to install signal handler for %d - errno %d", sig, errno); // NOLINT
                handlersRegistered = false;
            }
        }
    });

    return handlersRegistered;
}

static void printToFileForPsoRepro()
{
    NSError* error;
#if PLATFORM(IOS_FAMILY)
    NSURL* defaultURL = [NSURL fileURLWithPath:@"/tmp/" isDirectory:YES];
#else
    NSURL* defaultURL = [NSFileManager.defaultManager temporaryDirectory];
#endif
    NSURL* outputURL = [defaultURL URLByAppendingPathComponent:@"com.apple.WebKit"];
    BOOL directoryCreated = [NSFileManager.defaultManager createDirectoryAtURL:outputURL withIntermediateDirectories:YES attributes:nil error:&error];
    if (!directoryCreated || error) {
        WTFLogAlways("Error saving repro to %@ - %@", outputURL, error); // NOLINT
        return;
    }

    outputURL = [outputURL URLByAppendingPathComponent:@"psoRepro.mm"];
    if (FileSystem::overwriteEntireFile(outputURL.path, byteCast<uint8_t>(psoReproStringBuilder().span<Latin1Character>())))
        WTFLogAlways("Sucessfully saved repro to %@", outputURL); // NOLINT
    else
        WTFLogAlways("Error saving repro to %@ - %@", outputURL, error); // NOLINT
}

void dumpMetalReproCaseComputePSO(String&& shaderSource, String&& functionName)
{
    if (!enablePsoLogging())
        return;

    bool printToFile = !registerPsoExitHandlers();

    auto& sb = psoReproStringBuilder();
    sb.clear();
    sb.append(psoPreamble());
    /* NOLINT */ sb.append(R"(
    int main(int argc, const char * argv[])
    {
        @autoreleasepool {
        NSError *error = nil;

        // 1. Create a Metal device.
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            NSLog(@"Metal is not supported on this device.");
            return 1;
        }

        MTLCompileOptions* options = [MTLCompileOptions new];
        options.mathMode = MTLMathModeRelaxed;
        options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsFast;
        options.preprocessorMacros = @{ @"__wgslMetalAppleGPUFamily" : [NSString stringWithFormat:@"%u", computeAppleGPUFamily(device)] };

        // 2. Load the shader source.
        NSString *shaderSource = @R"(
    )"_s); /* NOLINT */
    sb.append(shaderSource);
    sb.append(")\";"_s);
    /* NOLINT */ sb.append(R"(
    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:options error:&error];
    if (!library || error) {
        NSLog(@"Failed to create library");
        return 1;
    }

    // 4. Create a Metal compute function.
    id<MTLFunction> computeFunction = [library newFunctionWithName:
    )"_s); /* NOLINT */
    sb.append(functionName);
    sb.append(")];"_s);
    /* NOLINT */ sb.append(R"(
    if (!computeFunction) {
        NSLog(@"Failed to create compute function.");
        return 1;
    }

    // 5. Create a Metal compute pipeline state.
    auto computePipelineDescriptor = [MTLComputePipelineDescriptor new];
    computePipelineDescriptor.computeFunction = computeFunction;
    computePipelineDescriptor.supportIndirectCommandBuffers = YES;
    id<MTLComputePipelineState> computePipelineState = [device newComputePipelineStateWithDescriptor:computePipelineDescriptor options:MTLPipelineOptionNone reflection:nil error:&error];

    if (computePipelineState)
        NSLog(@"SUCCESS");
    else
        NSLog(@"FAILED");
    }
    return 0;
    }
    )"_s); /* NOLINT */

    if (printToFile)
        printToFileForPsoRepro();
}

bool dumpMetalReproCaseRenderPSO(String&& vertexShaderSource, String&& vertexFunctionName, String&& fragmentShaderSource, String&& fragmentFunctionName, MTLRenderPipelineDescriptor* descriptor, ShaderModule::VertexStageIn& shaderLocations, const Device& device)
{
    if (!enablePsoLogging())
        return false;

    bool printToFile = !registerPsoExitHandlers();

    const auto maxVertexBuffers = device.limits().maxVertexBuffers;

    auto& sb = psoReproStringBuilder();
    sb.append(psoPreamble());
    /* NOLINT */ sb.append(R"(
    static id<MTLRenderPipelineState> makePso(id<MTLDevice> device)
    {
        MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.mathMode = MTLMathModeRelaxed;
        options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsFast;
        options.preprocessorMacros = @{ @"__wgslMetalAppleGPUFamily" : [NSString stringWithFormat:@"%u", computeAppleGPUFamily(device)] };
        const auto& mtlColorAttachment = mtlRenderPipelineDescriptor.colorAttachments[0];

        id<MTLFunction> functionVS = nil;
        id<MTLFunction> functionFS = nil;
        NSError *error = nil;
        NSString *vertexShaderSource = @R"(
    )"_s); /* NOLINT */
    sb.append(vertexShaderSource);
    sb.append(")\";\nNSString *fragmentShaderSource = @R\"("_s);
    sb.append(fragmentShaderSource);
    sb.append(")\";"_s);
    /* NOLINT */ sb.append(R"(
    id<MTLLibrary> vertexLibrary = [device newLibraryWithSource:vertexShaderSource options:options error:&error];
    if (error)
        NSLog(@"error compiling vertex shader");

    id<MTLLibrary> fragmentLibrary = [device newLibraryWithSource:fragmentShaderSource options:options error:&error];
    if (error)
        NSLog(@"error compiling fragment shader");

    functionVS = [vertexLibrary newFunctionWithName:@")"_s); /* NOLINT */
    sb.append(vertexFunctionName);
    sb.append("\"]; functionFS = [fragmentLibrary newFunctionWithName:@\""_s);
    sb.append(fragmentFunctionName);
    /* NOLINT */ sb.append(R"("];

    mtlRenderPipelineDescriptor.vertexFunction = functionVS;
    mtlRenderPipelineDescriptor.fragmentFunction = functionFS;

    mtlRenderPipelineDescriptor.rasterSampleCount = )"_s); /* NOLINT */
    sb.append(makeString(descriptor.rasterSampleCount));
    sb.append(";\n mtlRenderPipelineDescriptor.alphaToCoverageEnabled = "_s);
    sb.append(makeString(descriptor.alphaToCoverageEnabled));
    sb.append(";\n mtlRenderPipelineDescriptor.alphaToOneEnabled = "_s);
    sb.append(makeString((int)descriptor.alphaToOneEnabled));
    sb.append(";\n mtlRenderPipelineDescriptor.rasterizationEnabled = "_s);
    sb.append(makeString((int)descriptor.rasterizationEnabled));
    sb.append(";\n mtlRenderPipelineDescriptor.maxVertexAmplificationCount = "_s);
    sb.append(makeString((unsigned)descriptor.maxVertexAmplificationCount));
    sb.append(";\n mtlRenderPipelineDescriptor.depthAttachmentPixelFormat = (MTLPixelFormat)"_s);
    sb.append(makeString((unsigned)descriptor.depthAttachmentPixelFormat));
    sb.append(";\n mtlRenderPipelineDescriptor.stencilAttachmentPixelFormat = (MTLPixelFormat)"_s);
    sb.append(makeString((unsigned)descriptor.stencilAttachmentPixelFormat));
    sb.append(";\n mtlRenderPipelineDescriptor.inputPrimitiveTopology = (MTLPrimitiveTopologyClass)"_s);
    sb.append(makeString((unsigned)descriptor.inputPrimitiveTopology));
    sb.append(";\n mtlRenderPipelineDescriptor.supportIndirectCommandBuffers = YES;\n MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor new];"_s);

    for (size_t i = 0; i < maxVertexBuffers; ++i) {
        if (MTLVertexBufferLayoutDescriptor* d = descriptor.vertexDescriptor.layouts[i]) {
            sb.append("{uint32_t location = "_s);
            sb.append(makeString(i));
            sb.append("; vertexDescriptor.layouts[location].stride = "_s);
            sb.append(makeString(d.stride));
            sb.append("; vertexDescriptor.layouts[location].stepFunction = (MTLVertexStepFunction)"_s);
            sb.append(d.stepFunction);
            sb.append("; vertexDescriptor.layouts[location].stepRate = "_s);
            sb.append(d.stepRate);
            sb.append(";} mtlRenderPipelineDescriptor.vertexDescriptor = vertexDescriptor; \n"_s);
        }
    }

    for (auto shaderLocation : shaderLocations.keys()) {
        if (MTLVertexAttributeDescriptor* a = descriptor.vertexDescriptor.attributes[shaderLocation]) {
            sb.append("{uint32_t shaderLocation = "_s);
            sb.append(makeString(shaderLocation));
            sb.append("; vertexDescriptor.attributes[shaderLocation].format = (MTLVertexFormat)"_s);
            sb.append(makeString((unsigned)a.format));
            sb.append("; vertexDescriptor.attributes[shaderLocation].bufferIndex = "_s);
            sb.append(makeString(a.bufferIndex));
            sb.append("; vertexDescriptor.attributes[shaderLocation].offset = "_s);
            sb.append(makeString(a.offset));
            sb.append(";}\n"_s);
        }
    }

    for (size_t i = 0; i < 8; ++i) {
        if (MTLRenderPipelineColorAttachmentDescriptor* c = descriptor.colorAttachments[i]) {
            sb.append("{uint32_t i = "_s);
            sb.append(makeString(i));
            sb.append("; MTLRenderPipelineColorAttachmentDescriptor* c = mtlRenderPipelineDescriptor.colorAttachments[i]; c.pixelFormat = (MTLPixelFormat)"_s);
            sb.append(makeString((unsigned)c.pixelFormat));
            sb.append("; c.blendingEnabled = (BOOL)"_s);
            sb.append(makeString(c.blendingEnabled));
            sb.append("; c.sourceRGBBlendFactor = (MTLBlendFactor)"_s);
            sb.append(makeString(c.sourceRGBBlendFactor));
            sb.append("; c.destinationRGBBlendFactor = (MTLBlendFactor)"_s);
            sb.append(makeString(c.destinationRGBBlendFactor));
            sb.append("; c.rgbBlendOperation = (MTLBlendOperation)"_s);
            sb.append(makeString(c.rgbBlendOperation));
            sb.append("; c.sourceAlphaBlendFactor = (MTLBlendFactor)"_s);
            sb.append(makeString(c.sourceAlphaBlendFactor));
            sb.append("; c.destinationAlphaBlendFactor = (MTLBlendFactor)"_s);
            sb.append(makeString(c.destinationAlphaBlendFactor));
            sb.append("; c.alphaBlendOperation = (MTLBlendOperation)"_s);
            sb.append(makeString(c.alphaBlendOperation));
            sb.append("; c.writeMask = (MTLColorWriteMask)"_s);
            sb.append(makeString(c.writeMask));
            sb.append(";}\n"_s);
        }
    }

    /* NOLINT */ sb.append(R"(mtlRenderPipelineDescriptor.vertexDescriptor = vertexDescriptor;
    return [device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    }

    int main()
    {
        auto device = MTLCreateSystemDefaultDevice();
        NSLog(@"Device is %@", [device name]);

        id<MTLRenderPipelineState> pso = makePso(device);
        if (pso)
            NSLog(@"PASS");
        else
            NSLog(@"FAIL");

        return 0;
    }
    )"_s); /* NOLINT */

    if (printToFile)
        printToFileForPsoRepro();

    return true;
}

void clearMetalPSORepro()
{
    psoReproStringBuilder().clear();
}

#endif

} // namespace WebGPU::Metal
