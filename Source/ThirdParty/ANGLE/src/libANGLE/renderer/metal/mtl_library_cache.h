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

    AutoObjCPtr<id<MTLLibrary>> getOrCompileShaderLibrary(
        ContextMtl *context,
        const std::string &source,
        const std::map<std::string, std::string> &macros,
        bool enableFastMath,
        AutoObjCPtr<NSError *> *errorOut);

  private:
    using Key = std::tuple<std::string, std::map<std::string, std::string>, bool>;

    struct KeyCompare
    {
        // Mark this comparator as transparent. This allows types that are not Key to be used
        // as a key for unordered_map::find. We can avoid the construction of a Key (which
        // copies std::strings) when searching the cache.
        using is_transparent = void;

        template<typename K>
        size_t operator()(K &&k) const;

        template<typename K1, typename K2>
        bool operator()(K1&& a, K2 &&b) const;
    };

    angle::HashMap<Key, AutoObjCPtr<id<MTLLibrary>>, KeyCompare, KeyCompare> mCache;
};

}  // namespace mtl
}  // namespace rx

#endif  // LIBANGLE_RENDERER_METAL_MTL_LIBRARY_CACHE_H_
