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

#include "common/angle_version_info.h"
#include "common/utilities.h"
#include "libANGLE/BinaryStream.h"
#include "libANGLE/Context.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/capture/FrameCapture.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "platform/PlatformMethods.h"

namespace gl
{

namespace
{
constexpr unsigned int kWarningLimit = 3;

class HashStream final : angle::NonCopyable
{
  public:
    std::string str() { return mStringStream.str(); }

    template <typename T>
    HashStream &operator<<(T value)
    {
        mStringStream << value << kSeparator;
        return *this;
    }

  private:
    static constexpr char kSeparator = ':';
    std::ostringstream mStringStream;
};

HashStream &operator<<(HashStream &stream, Shader *shader)
{
    if (shader)
    {
        stream << shader->getSourceString().c_str() << shader->getSourceString().length()
               << shader->getCompilerResourcesString().c_str();
    }
    return stream;
}

HashStream &operator<<(HashStream &stream, const ProgramBindings &bindings)
{
    for (const auto &binding : bindings.getStableIterationMap())
    {
        stream << binding.first << binding.second;
    }
    return stream;
}

HashStream &operator<<(HashStream &stream, const ProgramAliasedBindings &bindings)
{
    for (const auto &binding : bindings.getStableIterationMap())
    {
        stream << binding.first << binding.second.location;
    }
    return stream;
}

HashStream &operator<<(HashStream &stream, const std::vector<std::string> &strings)
{
    for (const auto &str : strings)
    {
        stream << str;
    }
    return stream;
}

HashStream &operator<<(HashStream &stream, const std::vector<gl::VariableLocation> &locations)
{
    for (const auto &loc : locations)
    {
        stream << loc.index << loc.arrayIndex << loc.ignored;
    }
    return stream;
}

}  // anonymous namespace

MemoryProgramCache::MemoryProgramCache(egl::BlobCache &blobCache)
    : mBlobCache(blobCache), mIssuedWarnings(0)
{}

MemoryProgramCache::~MemoryProgramCache() {}

void MemoryProgramCache::ComputeHash(const Context *context,
                                     const Program *program,
                                     egl::BlobCache::Key *hashOut)
{
    // Compute the program hash. Start with the shader hashes and resource strings.
    HashStream hashStream;
    for (ShaderType shaderType : AllShaderTypes())
    {
        hashStream << program->getAttachedShader(shaderType);
    }

    // Add some ANGLE metadata and Context properties, such as version and back-end.
    hashStream << angle::GetANGLECommitHash() << context->getClientMajorVersion()
               << context->getClientMinorVersion() << context->getString(GL_RENDERER);

    // Hash pre-link program properties.
    hashStream << program->getAttributeBindings() << program->getUniformLocationBindings()
               << program->getFragmentOutputLocations() << program->getFragmentOutputIndexes()
               << program->getState().getTransformFeedbackVaryingNames()
               << program->getState().getTransformFeedbackBufferMode()
               << program->getState().getOutputLocations()
               << program->getState().getSecondaryOutputLocations();

    // Include the status of FrameCapture, which adds source strings to the binary
    hashStream << context->getShareGroup()->getFrameCaptureShared()->enabled();

    // Call the secure SHA hashing function.
    const std::string &programKey = hashStream.str();
    angle::base::SHA1HashBytes(reinterpret_cast<const unsigned char *>(programKey.c_str()),
                               programKey.length(), hashOut->data());
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
    egl::BlobCache::Value binaryProgram;
    size_t programSize = 0;
    if (get(context, *hashOut, &binaryProgram, &programSize))
    {
        angle::MemoryBuffer uncompressedData;
        if (!egl::DecompressBlobCacheData(binaryProgram.data(), programSize, &uncompressedData))
        {
            ERR() << "Error decompressing binary data.";
            return angle::Result::Incomplete;
        }

        angle::Result result =
            program->loadBinary(context, GL_PROGRAM_BINARY_ANGLE, uncompressedData.data(),
                                static_cast<int>(uncompressedData.size()));
        ANGLE_HISTOGRAM_BOOLEAN("GPU.ANGLE.ProgramCache.LoadBinarySuccess",
                                result == angle::Result::Continue);
        ANGLE_TRY(result);

        if (result == angle::Result::Continue)
            return angle::Result::Continue;

        // Cache load failed, evict.
        if (mIssuedWarnings++ < kWarningLimit)
        {
            WARN() << "Failed to load binary from cache.";

            if (mIssuedWarnings == kWarningLimit)
            {
                WARN() << "Reaching warning limit for cache load failures, silencing "
                          "subsequent warnings.";
            }
        }
        remove(*hashOut);
    }
    return angle::Result::Incomplete;
}

bool MemoryProgramCache::get(const Context *context,
                             const egl::BlobCache::Key &programHash,
                             egl::BlobCache::Value *programOut,
                             size_t *programSizeOut)
{
    return mBlobCache.get(context->getScratchBuffer(), programHash, programOut, programSizeOut);
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
        ERR() << "Error compressing binary data.";
        return angle::Result::Incomplete;
    }

    ANGLE_HISTOGRAM_COUNTS("GPU.ANGLE.ProgramCache.ProgramBinarySizeBytes",
                           static_cast<int>(compressedData.size()));

    // TODO(syoussefi): to be removed.  Compatibility for Chrome until it supports
    // EGL_ANDROID_blob_cache. http://anglebug.com/2516
    auto *platform = ANGLEPlatformCurrent();
    platform->cacheProgram(platform, programHash, compressedData.size(), compressedData.data());

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
    mIssuedWarnings = 0;
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
