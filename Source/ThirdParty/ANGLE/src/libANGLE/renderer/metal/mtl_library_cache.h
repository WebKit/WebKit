//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_library_cache.h:
//    Defines classes for caching of mtl libraries
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_LIBRARY_CACHE_H_
#define LIBANGLE_RENDERER_METAL_MTL_LIBRARY_CACHE_H_

#include "libANGLE/renderer/metal/mtl_utils.h"

#include <type_traits>

namespace rx
{
namespace mtl
{

class LibraryCache : angle::NonCopyable
{
  public:
    LibraryCache()  = default;
    ~LibraryCache() = default;

    AutoObjCPtr<id<MTLLibrary>> get(const std::shared_ptr<const std::string> &source,
                                    const std::map<std::string, std::string> &macros,
                                    bool enableFastMath);
    AutoObjCPtr<id<MTLLibrary>> getOrCompileShaderLibrary(
        ContextMtl *context,
        const std::shared_ptr<const std::string> &source,
        const std::map<std::string, std::string> &macros,
        bool enableFastMath,
        AutoObjCPtr<NSError *> *errorOut);

  private:
    struct LibraryKey
    {
        LibraryKey() = default;
        LibraryKey(const std::shared_ptr<const std::string> &source,
                   const std::map<std::string, std::string> &macros,
                   bool enableFastMath);

        std::shared_ptr<const std::string> source;
        std::map<std::string, std::string> macros;
        bool enableFastMath;

        bool operator==(const LibraryKey &other) const;
    };

    struct LibraryKeyHasher
    {
        // Hash function
        size_t operator()(const LibraryKey &k) const;

        // Comparison
        bool operator()(const LibraryKey &a, const LibraryKey &b) const;
    };

    struct LibraryCacheEntry
    {
        LibraryCacheEntry() = default;

        // library can only go from the null -> not null state. It is safe to check if the library
        // already exists without locking.
        AutoObjCPtr<id<MTLLibrary>> library;

        // Lock for this specific library to avoid multiple threads compiling the same shader at
        // once.
        std::mutex lock;
    };

    LibraryCacheEntry &getCacheEntry(LibraryKey &&key);

    // Lock for searching and adding new entries to the cache
    std::mutex mCacheLock;

    using CacheMap = std::unordered_map<LibraryKey, LibraryCacheEntry, LibraryKeyHasher>;
    CacheMap mCache;
};

}  // namespace mtl
}  // namespace rx

#endif  // LIBANGLE_RENDERER_METAL_MTL_LIBRARY_CACHE_H_
