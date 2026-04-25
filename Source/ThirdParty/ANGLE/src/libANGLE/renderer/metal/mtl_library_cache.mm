//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_library_cache.mm:
//    Defines classes for caching of mtl libraries
//

#include "libANGLE/renderer/metal/mtl_library_cache.h"

#include <stdio.h>

#include <limits>

#include "common/MemoryBuffer.h"
#include "common/angleutils.h"
#include "common/hash_utils.h"
#include "common/mathutil.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "platform/PlatformMethods.h"

namespace rx
{
namespace mtl
{

LibraryCache::LibraryCache() : mCache(kMaxCachedLibraries) {}

angle::ObjCPtr<id<MTLLibrary>> LibraryCache::get(const std::shared_ptr<const std::string> &source,
                                                 const std::map<std::string, std::string> &macros,
                                                 bool disableFastMath,
                                                 bool usesInvariance)
{
    ASSERT(source != nullptr);
    std::shared_ptr<LibraryCache::LibraryCacheEntry> entry =
        getCacheEntry(LibraryKey(source, macros, disableFastMath, usesInvariance));

    // Try to lock the entry and return the library if it exists. If we can't lock then it means
    // another thread is currently compiling.
    std::unique_lock<std::mutex> entryLockGuard(entry->lock, std::try_to_lock);
    if (entryLockGuard)
    {
        return entry->library;
    }
    else
    {
        return nil;
    }
}

angle::ObjCPtr<id<MTLLibrary>> LibraryCache::getOrCompileShaderLibrary(
    DisplayMtl *displayMtl,
    const std::shared_ptr<const std::string> &source,
    const std::map<std::string, std::string> &macros,
    bool disableFastMath,
    bool usesInvariance,
    angle::ObjCPtr<NSError> *errorOut)
{
    id<MTLDevice> metalDevice          = displayMtl->getMetalDevice();
    const angle::FeaturesMtl &features = displayMtl->getFeatures();
    if (!features.enableInMemoryMtlLibraryCache.enabled)
    {
        return CreateShaderLibrary(metalDevice, *source, macros, disableFastMath, usesInvariance,
                                   errorOut);
    }

    ASSERT(source != nullptr);
    std::shared_ptr<LibraryCache::LibraryCacheEntry> entry =
        getCacheEntry(LibraryKey(source, macros, disableFastMath, usesInvariance));

    // Lock this cache entry while compiling the shader. This causes other threads calling this
    // function to wait and not duplicate the compilation.
    std::lock_guard<std::mutex> entryLockGuard(entry->lock);
    if (entry->library)
    {
        return entry->library;
    }

    entry->library = CreateShaderLibrary(metalDevice, *source, macros, disableFastMath,
                                         usesInvariance, errorOut);
    return entry->library;
}

std::shared_ptr<LibraryCache::LibraryCacheEntry> LibraryCache::getCacheEntry(LibraryKey &&key)
{
    // Lock while searching or adding new items to the cache.
    std::lock_guard<std::mutex> cacheLockGuard(mCacheLock);

    auto iter = mCache.Get(key);
    if (iter != mCache.end())
    {
        return iter->second;
    }

    angle::TrimCache(kMaxCachedLibraries, kGCLimit, "metal library", &mCache);

    iter = mCache.Put(std::move(key), std::make_shared<LibraryCacheEntry>());
    return iter->second;
}

LibraryCache::LibraryKey::LibraryKey(const std::shared_ptr<const std::string> &sourceIn,
                                     const std::map<std::string, std::string> &macrosIn,
                                     bool disableFastMathIn,
                                     bool usesInvarianceIn)
    : source(sourceIn),
      macros(macrosIn),
      disableFastMath(disableFastMathIn),
      usesInvariance(usesInvarianceIn)
{}

bool LibraryCache::LibraryKey::operator==(const LibraryKey &other) const
{
    return std::tie(*source, macros, disableFastMath, usesInvariance) ==
           std::tie(*other.source, other.macros, other.disableFastMath, other.usesInvariance);
}

size_t LibraryCache::LibraryKeyHasher::operator()(const LibraryKey &k) const
{
    size_t hash = 0;
    angle::HashCombine(hash, *k.source);
    for (const auto &macro : k.macros)
    {
        angle::HashCombine(hash, macro.first);
        angle::HashCombine(hash, macro.second);
    }
    angle::HashCombine(hash, k.disableFastMath);
    angle::HashCombine(hash, k.usesInvariance);
    return hash;
}

LibraryCache::LibraryCacheEntry::~LibraryCacheEntry()
{
    // Lock the cache entry before deletion to ensure there is no other thread compiling and
    // preparing to write to the library. LibraryCacheEntry objects can only be deleted while the
    // mCacheLock is held so only one thread modifies mCache at a time.
    std::lock_guard<std::mutex> entryLockGuard(lock);
}

LibraryCache::LibraryCacheEntry::LibraryCacheEntry(LibraryCacheEntry &&moveFrom)
{
    // Lock the cache entry being moved from to make sure the library can be safely accessed.
    // Mutexes cannot be moved so a new one will be created in this entry
    std::lock_guard<std::mutex> entryLockGuard(moveFrom.lock);

    library          = std::move(moveFrom.library);
    moveFrom.library = nullptr;
}

}  // namespace mtl
}  // namespace rx
