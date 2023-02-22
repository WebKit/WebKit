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

class LibraryCache
{
  public:
    LibraryCache()  = default;
    ~LibraryCache() = default;

    AutoObjCPtr<id<MTLLibrary>> get(const std::string &source,
                                    const std::map<std::string, std::string> &macros,
                                    bool enableFastMath);
    AutoObjCPtr<id<MTLLibrary>> getOrCompileShaderLibrary(
        ContextMtl *context,
        const std::string &source,
        const std::map<std::string, std::string> &macros,
        bool enableFastMath,
        AutoObjCPtr<NSError *> *errorOut);

  private:
    struct LibraryKey
    {
        std::string source;
        std::map<std::string, std::string> macros;
        bool enableFastMath;

        using LValueTuple = decltype(std::tie(std::as_const(source),
                                              std::as_const(macros),
                                              std::as_const(enableFastMath)));

        LibraryKey() = default;
        explicit LibraryKey(const LValueTuple &fromTuple);

        LValueTuple tie() const;
    };

    struct LibraryKeyCompare
    {
        // Mark this comparator as transparent. This allows types that are not LibraryKey to be used
        // as a key for unordered_map::find. We can avoid the construction of a LibraryKey (which
        // copies std::strings) when searching the cache.
        using is_transparent = void;

        // Hash functions for keys and lvalue tuples of keys
        size_t operator()(const LibraryKey::LValueTuple &k) const;
        size_t operator()(const LibraryKey &k) const;

        // Comparison operators for all key/lvalue combinations need by a map
        bool operator()(const LibraryKey &a, const LibraryKey &b) const;
        bool operator()(const LibraryKey &a, const LibraryKey::LValueTuple &b) const;
        bool operator()(const LibraryKey::LValueTuple &a, const LibraryKey &b) const;
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

    LibraryCacheEntry &getCacheEntry(const LibraryKey::LValueTuple &lValueKey);

    // Lock for searching and adding new entries to the cache
    std::mutex mCacheLock;

    using CacheMap =
        std::unordered_map<LibraryKey, LibraryCacheEntry, LibraryKeyCompare, LibraryKeyCompare>;
    CacheMap mCache;
};

}  // namespace mtl
}  // namespace rx

#endif  // LIBANGLE_RENDERER_METAL_MTL_LIBRARY_CACHE_H_
