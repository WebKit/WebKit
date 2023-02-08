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
#if defined(__cpp_lib_generic_unordered_lookup) && __cpp_lib_generic_unordered_lookup >= 201811L
    auto key = std::tie(source, macros, enableFastMath);
#else
    Key key(source, macros, enableFastMath);
#endif

    auto iter = mCache.find(key);
    if (iter != mCache.end())
    {
        return iter->second;
    }

    AutoObjCPtr<id<MTLLibrary>> library =
        CreateShaderLibrary(context->getMetalDevice(), source, macros, enableFastMath, errorOut);
    if (library)
    {
        mCache.insert_or_assign(std::move(key), library);
    }

    return library;
}

template<typename K>
size_t LibraryCache::KeyCompare::operator()(K &&k) const
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

template<typename K1, typename K2>
bool LibraryCache::KeyCompare::operator()(K1 &&a, K2 &&b) const
{
    return a == b;
}

}  // namespace mtl
}  // namespace rx
