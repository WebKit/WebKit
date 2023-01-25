//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MemoryProgramCache: Stores compiled and linked programs in memory so they don't
//   always have to be re-compiled. Can be used in conjunction with the platform
//   layer to warm up the cache from disk.

// Include zlib first, otherwise FAR gets defined elsewhere.
#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

#include "libANGLE/MemoryProgramCache.h"

#include <GLSLANG/ShaderVars.h>
#include <anglebase/sha1.h>

#include "common/BinaryStream.h"
#include "common/angle_version_info.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/capture/FrameCapture.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "platform/PlatformMethods.h"

namespace gl
{

namespace
{

void WriteProgramBindings(BinaryOutputStream *stream, const ProgramBindings &bindings)
{
    for (const auto &binding : bindings.getStableIterationMap())
    {
        stream->writeString(binding.first);
        stream->writeInt(binding.second);
    }
}

void WriteProgramAliasedBindings(BinaryOutputStream *stream, const ProgramAliasedBindings &bindings)
{
    for (const auto &binding : bindings.getStableIterationMap())
    {
        stream->writeString(binding.first);
        stream->writeInt(binding.second.location);
    }
}

void WriteVariableLocations(BinaryOutputStream *stream,
                            const std::vector<gl::VariableLocation> &locations)
{
    for (const auto &loc : locations)
    {
        stream->writeInt(loc.index);
        stream->writeInt(loc.arrayIndex);
        stream->writeBool(loc.ignored);
    }
}

}  // anonymous namespace

MemoryProgramCache::MemoryProgramCache(egl::BlobCache &blobCache) : mBlobCache(blobCache) {}

MemoryProgramCache::~MemoryProgramCache() {}

void MemoryProgramCache::ComputeHash(const Context *context,
                                     const Program *program,
                                     egl::BlobCache::Key *hashOut)
{
    // Compute the program hash. Start with the shader hashes.
    BinaryOutputStream hashStream;
    for (ShaderType shaderType : AllShaderTypes())
    {
        Shader *shader = program->getAttachedShader(shaderType);
        if (shader)
        {
            shader->writeShaderKey(&hashStream);
        }
    }

    // Add some ANGLE metadata and Context properties, such as version and back-end.
    hashStream.writeString(angle::GetANGLEShaderProgramVersion());
    hashStream.writeInt(angle::GetANGLESHVersion());
    hashStream.writeInt(context->getClientMajorVersion());
    hashStream.writeInt(context->getClientMinorVersion());
    hashStream.writeString(reinterpret_cast<const char *>(context->getString(GL_RENDERER)));

    // Hash pre-link program properties.
    WriteProgramBindings(&hashStream, program->getAttributeBindings());
    WriteProgramAliasedBindings(&hashStream, program->getUniformLocationBindings());
    WriteProgramAliasedBindings(&hashStream, program->getFragmentOutputLocations());
    WriteProgramAliasedBindings(&hashStream, program->getFragmentOutputIndexes());
    for (const std::string &transformFeedbackVaryingName :
         program->getState().getTransformFeedbackVaryingNames())
    {
        hashStream.writeString(transformFeedbackVaryingName);
    }
    hashStream.writeInt(program->getState().getTransformFeedbackBufferMode());
    WriteVariableLocations(&hashStream, program->getState().getOutputLocations());
    WriteVariableLocations(&hashStream, program->getState().getSecondaryOutputLocations());

    // Include the status of FrameCapture, which adds source strings to the binary
    hashStream.writeBool(context->getShareGroup()->getFrameCaptureShared()->enabled());

    // Call the secure SHA hashing function.
    const std::vector<uint8_t> &programKey = hashStream.getData();
    angle::base::SHA1HashBytes(programKey.data(), programKey.size(), hashOut->data());
}

angle::Result MemoryProgramCache::getProgram(const Context *context,
                                             Program *program,
                                             egl::BlobCache::Key *hashOut)
{
    // If caching is effectively disabled, don't bother calculating the hash.
    if (!mBlobCache.isCachingEnabled())
    {
        return angle::Result::Incomplete;
    }

    ComputeHash(context, program, hashOut);

    angle::MemoryBuffer uncompressedData;
    switch (mBlobCache.getAndDecompress(context->getScratchBuffer(), *hashOut, &uncompressedData))
    {
        case egl::BlobCache::GetAndDecompressResult::NotFound:
            return angle::Result::Incomplete;

        case egl::BlobCache::GetAndDecompressResult::DecompressFailure:
            ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Error decompressing program binary data fetched from cache.");
            return angle::Result::Incomplete;

        case egl::BlobCache::GetAndDecompressResult::GetSuccess:
            angle::Result result =
                program->loadBinary(context, GL_PROGRAM_BINARY_ANGLE, uncompressedData.data(),
                                    static_cast<int>(uncompressedData.size()));
            ANGLE_TRY(result);

            if (result == angle::Result::Continue)
                return angle::Result::Continue;

            // Cache load failed, evict
            ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Failed to load program binary from cache.");
            remove(*hashOut);

            return angle::Result::Incomplete;
    }

    UNREACHABLE();
    return angle::Result::Incomplete;
}

bool MemoryProgramCache::getAt(size_t index,
                               const egl::BlobCache::Key **hashOut,
                               egl::BlobCache::Value *programOut)
{
    return mBlobCache.getAt(index, hashOut, programOut);
}

void MemoryProgramCache::remove(const egl::BlobCache::Key &programHash)
{
    mBlobCache.remove(programHash);
}

angle::Result MemoryProgramCache::putProgram(const egl::BlobCache::Key &programHash,
                                             const Context *context,
                                             const Program *program)
{
    // If caching is effectively disabled, don't bother serializing the program.
    if (!mBlobCache.isCachingEnabled())
    {
        return angle::Result::Incomplete;
    }

    angle::MemoryBuffer serializedProgram;
    ANGLE_TRY(program->serialize(context, &serializedProgram));

    angle::MemoryBuffer compressedData;
    if (!egl::CompressBlobCacheData(serializedProgram.size(), serializedProgram.data(),
                                    &compressedData))
    {
        ANGLE_PERF_WARNING(context->getState().getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Error compressing binary data.");
        return angle::Result::Incomplete;
    }

    {
        std::scoped_lock<std::mutex> lock(mBlobCache.getMutex());
        // TODO: http://anglebug.com/7568
        // This was a workaround for Chrome until it added support for EGL_ANDROID_blob_cache,
        // tracked by http://anglebug.com/2516. This issue has since been closed, but removing this
        // still causes a test failure.
        auto *platform = ANGLEPlatformCurrent();
        platform->cacheProgram(platform, programHash, compressedData.size(), compressedData.data());
    }

    mBlobCache.put(programHash, std::move(compressedData));
    return angle::Result::Continue;
}

angle::Result MemoryProgramCache::updateProgram(const Context *context, const Program *program)
{
    egl::BlobCache::Key programHash;
    ComputeHash(context, program, &programHash);
    return putProgram(programHash, context, program);
}

bool MemoryProgramCache::putBinary(const egl::BlobCache::Key &programHash,
                                   const uint8_t *binary,
                                   size_t length)
{
    // Copy the binary.
    angle::MemoryBuffer newEntry;
    if (!newEntry.resize(length))
    {
        return false;
    }
    memcpy(newEntry.data(), binary, length);

    // Store the binary.
    mBlobCache.populate(programHash, std::move(newEntry));

    return true;
}

void MemoryProgramCache::clear()
{
    mBlobCache.clear();
}

void MemoryProgramCache::resize(size_t maxCacheSizeBytes)
{
    mBlobCache.resize(maxCacheSizeBytes);
}

size_t MemoryProgramCache::entryCount() const
{
    return mBlobCache.entryCount();
}

size_t MemoryProgramCache::trim(size_t limit)
{
    return mBlobCache.trim(limit);
}

size_t MemoryProgramCache::size() const
{
    return mBlobCache.size();
}

size_t MemoryProgramCache::maxSize() const
{
    return mBlobCache.maxSize();
}

}  // namespace gl
