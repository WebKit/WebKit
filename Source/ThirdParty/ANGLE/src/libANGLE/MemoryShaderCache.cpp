//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MemoryShaderCache: Stores compiled shader in memory so they don't
//   always have to be re-compiled. Can be used in conjunction with the platform
//   layer to warm up the cache from disk.

#include "libANGLE/MemoryShaderCache.h"

#include <GLSLANG/ShaderVars.h>
#include <anglebase/sha1.h>

#include "common/BinaryStream.h"
#include "common/utilities.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Context.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/ShaderImpl.h"
#include "platform/PlatformMethods.h"

namespace gl
{

namespace
{
// Limit decompressed programs to 5MB. If they're larger then this there is a good chance the data
// is not what we expect. This limits the amount of memory we will allocate based on a binary blob
// we believe is compressed data.
static constexpr size_t kMaxUncompressedShaderSize = 5 * 1024 * 1024;
}  // namespace

MemoryShaderCache::MemoryShaderCache(egl::BlobCache &blobCache) : mBlobCache(blobCache) {}

MemoryShaderCache::~MemoryShaderCache() {}

bool MemoryShaderCache::getShader(const Context *context,
                                  Shader *shader,
                                  const egl::BlobCache::Key &shaderHash)
{
    // If caching is effectively disabled, don't bother calculating the hash.
    if (!mBlobCache.isCachingEnabled())
    {
        return false;
    }

    angle::MemoryBuffer uncompressedData;
    switch (mBlobCache.getAndDecompress(context->getScratchBuffer(), shaderHash,
                                        kMaxUncompressedShaderSize, &uncompressedData))
    {
        case egl::BlobCache::GetAndDecompressResult::DecompressFailure:
            ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Error decompressing shader binary data from cache.");
            return false;

        case egl::BlobCache::GetAndDecompressResult::NotFound:
            return false;

        case egl::BlobCache::GetAndDecompressResult::GetSuccess:
            if (shader->loadBinary(context, uncompressedData.data(),
                                   static_cast<int>(uncompressedData.size())))
            {
                return true;
            }

            // Cache load failed, evict.
            ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Failed to load shader binary from cache.");
            mBlobCache.remove(shaderHash);
            return false;
    }

    UNREACHABLE();
    return false;
}

angle::Result MemoryShaderCache::putShader(const Context *context,
                                           const egl::BlobCache::Key &shaderHash,
                                           const Shader *shader)
{
    // If caching is effectively disabled, don't bother serializing the shader.
    if (!mBlobCache.isCachingEnabled())
    {
        return angle::Result::Continue;
    }

    angle::MemoryBuffer serializedShader;
    ANGLE_TRY(shader->serialize(nullptr, &serializedShader));

    size_t compressedSize;
    if (!mBlobCache.compressAndPut(shaderHash, std::move(serializedShader), &compressedSize))
    {
        ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Error compressing shader binary data for insertion into cache.");
        return angle::Result::Continue;
    }

    return angle::Result::Continue;
}

void MemoryShaderCache::clear()
{
    mBlobCache.clear();
}

size_t MemoryShaderCache::maxSize() const
{
    return mBlobCache.maxSize();
}

}  // namespace gl
