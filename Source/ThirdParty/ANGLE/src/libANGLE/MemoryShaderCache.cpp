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

#include "common/angle_version_info.h"
#include "common/utilities.h"
#include "libANGLE/BinaryStream.h"
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
void ComputeHash(const Context *context,
                 const Shader *shader,
                 const ShCompileOptions &compileOptions,
                 const ShCompilerInstance &compilerInstance,
                 egl::BlobCache::Key *hashOut)
{
    BinaryOutputStream hashStream;
    // Compute the shader hash. Start with the shader hashes and resource strings.
    hashStream.writeEnum(shader->getType());
    hashStream.writeString(shader->getSourceString());

    // Include the commit hash
    hashStream.writeString(angle::GetANGLECommitHash());

    hashStream.writeEnum(Compiler::SelectShaderSpec(context->getState()));
    hashStream.writeEnum(compilerInstance.getShaderOutputType());
    hashStream.writeBytes(reinterpret_cast<const uint8_t *>(&compileOptions),
                          sizeof(compileOptions));

    // Include the ShBuiltInResources, which represent the extensions and constants used by the
    // shader.
    const ShBuiltInResources resources = compilerInstance.getBuiltInResources();
    hashStream.writeBytes(reinterpret_cast<const uint8_t *>(&resources), sizeof(resources));

    // Call the secure SHA hashing function.
    const std::vector<uint8_t> &shaderKey = hashStream.getData();
    angle::base::SHA1HashBytes(shaderKey.data(), shaderKey.size(), hashOut->data());
}
}  // namespace

MemoryShaderCache::MemoryShaderCache(egl::BlobCache &blobCache) : mBlobCache(blobCache) {}

MemoryShaderCache::~MemoryShaderCache() {}

angle::Result MemoryShaderCache::getShader(const Context *context,
                                           Shader *shader,
                                           const ShCompileOptions &compileOptions,
                                           const ShCompilerInstance &compilerInstance,
                                           egl::BlobCache::Key *hashOut)
{
    // If caching is effectively disabled, don't bother calculating the hash.
    if (!mBlobCache.isCachingEnabled())
    {
        return angle::Result::Incomplete;
    }

    ComputeHash(context, shader, compileOptions, compilerInstance, hashOut);

    angle::MemoryBuffer uncompressedData;
    switch (mBlobCache.getAndDecompress(context->getScratchBuffer(), *hashOut, &uncompressedData))
    {
        case egl::BlobCache::GetAndDecompressResult::DecompressFailure:
            ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Error decompressing shader binary data from cache.");
            return angle::Result::Incomplete;

        case egl::BlobCache::GetAndDecompressResult::NotFound:
            return angle::Result::Incomplete;

        case egl::BlobCache::GetAndDecompressResult::GetSuccess:
            angle::Result result = shader->loadBinary(context, uncompressedData.data(),
                                                      static_cast<int>(uncompressedData.size()));

            {
                std::scoped_lock<std::mutex> lock(mHistogramMutex);
                ANGLE_HISTOGRAM_BOOLEAN("GPU.ANGLE.ShaderCache.LoadBinarySuccess",
                                        result == angle::Result::Continue);
            }
            ANGLE_TRY(result);

            if (result == angle::Result::Continue)
                return angle::Result::Continue;

            // Cache load failed, evict.
            ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Failed to load shader binary from cache.");
            mBlobCache.remove(*hashOut);
            return angle::Result::Incomplete;
    }

    UNREACHABLE();
    return angle::Result::Incomplete;
}

angle::Result MemoryShaderCache::putShader(const Context *context,
                                           const egl::BlobCache::Key &shaderHash,
                                           const Shader *shader)
{
    // If caching is effectively disabled, don't bother serializing the shader.
    if (!mBlobCache.isCachingEnabled())
    {
        return angle::Result::Incomplete;
    }

    angle::MemoryBuffer serializedShader;
    ANGLE_TRY(shader->serialize(nullptr, &serializedShader));

    size_t compressedSize;
    if (!mBlobCache.compressAndPut(shaderHash, std::move(serializedShader), &compressedSize))
    {
        ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Error compressing shader binary data for insertion into cache.");
        return angle::Result::Incomplete;
    }

    {
        std::scoped_lock<std::mutex> lock(mHistogramMutex);
        ANGLE_HISTOGRAM_COUNTS("GPU.ANGLE.ShaderCache.ShaderBinarySizeBytes",
                               static_cast<int>(compressedSize));
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
