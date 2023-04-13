//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_library_cache.mm:
//    Defines classes for caching of mtl libraries
//

#include "libANGLE/renderer/metal/mtl_library_cache.h"

#include "common/hash_utils.h"
#include "common/mathutil.h"
#include "libANGLE/renderer/metal/ContextMtl.h"

namespace rx
{
namespace mtl
{

AutoObjCPtr<id<MTLLibrary>> LibraryCache::get(const std::string &source,
                                              const std::map<std::string, std::string> &macros,
                                              bool enableFastMath)
{
    LibraryKey::LValueTuple key            = std::tie(source, macros, enableFastMath);
    LibraryCache::LibraryCacheEntry &entry = getCacheEntry(key);

    // Try to lock the entry and return the library if it exists. If we can't lock then it means
    // another thread is currently compiling.
    std::unique_lock<std::mutex> entryLockGuard(entry.lock, std::try_to_lock);
    if (entryLockGuard)
    {
        return entry.library;
    }
    else
    {
        return nil;
    }
}

AutoObjCPtr<id<MTLLibrary>> LibraryCache::getOrCompileShaderLibrary(
    ContextMtl *context,
    const std::string &source,
    const std::map<std::string, std::string> &macros,
    bool enableFastMath,
    AutoObjCPtr<NSError *> *errorOut)
{
    const angle::FeaturesMtl &features = context->getDisplay()->getFeatures();
    if (!features.enableInMemoryMtlLibraryCache.enabled)
    {
        return CreateShaderLibrary(context->getMetalDevice(), source, macros, enableFastMath,
                                   errorOut);
    }

    LibraryKey::LValueTuple key            = std::tie(source, macros, enableFastMath);
    LibraryCache::LibraryCacheEntry &entry = getCacheEntry(key);

    // Lock this cache entry while compiling the shader. This causes other threads calling this
    // function to wait and not duplicate the compilation.
    std::lock_guard<std::mutex> entryLockGuard(entry.lock);

    if (!entry.library)
    {
        entry.library = CreateShaderLibrary(context->getMetalDevice(), source, macros,
                                            enableFastMath, errorOut);
    }

    return entry.library;
}

LibraryCache::LibraryCacheEntry &LibraryCache::getCacheEntry(
    const LibraryKey::LValueTuple &lValueKey)
{
    // Lock while searching or adding new items to the cache.
    std::lock_guard<std::mutex> cacheLockGuard(mCacheLock);

#if ANGLE_HAS_HASH_MAP_GENERIC_LOOKUP
    // Fast-path that can search the cache with only lvalues instead of making a copy of the key
    auto iter = mCache.find(lValueKey);
    if (iter != mCache.end())
    {
        return iter->second;
    }
#endif

    LibraryKey key(lValueKey);
    return mCache[std::move(key)];
}

LibraryCache::LibraryKey::LibraryKey(const LValueTuple &fromTuple)
{
    source         = std::get<0>(fromTuple);
    macros         = std::get<1>(fromTuple);
    enableFastMath = std::get<2>(fromTuple);
}

LibraryCache::LibraryKey::LValueTuple LibraryCache::LibraryKey::tie() const
{
    return std::tie(source, macros, enableFastMath);
}

size_t LibraryCache::LibraryKeyCompare::operator()(const LibraryKey::LValueTuple &k) const
{
    size_t hash = std::hash<std::string>()(std::get<0>(k));
    for (const auto &macro : std::get<1>(k))
    {
        hash =
            hash ^ std::hash<std::string>()(macro.first) ^ std::hash<std::string>()(macro.second);
    }
    hash = hash ^ std::hash<bool>()(std::get<2>(k));
    return hash;
}

size_t LibraryCache::LibraryKeyCompare::operator()(const LibraryKey &k) const
{
    return operator()(k.tie());
}

bool LibraryCache::LibraryKeyCompare::operator()(const LibraryKey &a, const LibraryKey &b) const
{
    return a.tie() == b.tie();
}

bool LibraryCache::LibraryKeyCompare::operator()(const LibraryKey &a,
                                                 const LibraryKey::LValueTuple &b) const
{
    return a.tie() == b;
}

bool LibraryCache::LibraryKeyCompare::operator()(const LibraryKey::LValueTuple &a,
                                                 const LibraryKey &b) const
{
    return a == b.tie();
}

}  // namespace mtl
}  // namespace rx
