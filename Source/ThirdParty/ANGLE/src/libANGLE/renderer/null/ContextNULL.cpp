//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextNULL.cpp:
//    Implements the class methods for ContextNULL.
//

#include "libANGLE/renderer/null/ContextNULL.h"

#include "common/debug.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/null/BufferNULL.h"
#include "libANGLE/renderer/null/CompilerNULL.h"
#include "libANGLE/renderer/null/DisplayNULL.h"
#include "libANGLE/renderer/null/FenceNVNULL.h"
#include "libANGLE/renderer/null/FramebufferNULL.h"
#include "libANGLE/renderer/null/ImageNULL.h"
#include "libANGLE/renderer/null/PathNULL.h"
#include "libANGLE/renderer/null/ProgramNULL.h"
#include "libANGLE/renderer/null/ProgramPipelineNULL.h"
#include "libANGLE/renderer/null/QueryNULL.h"
#include "libANGLE/renderer/null/RenderbufferNULL.h"
#include "libANGLE/renderer/null/SamplerNULL.h"
#include "libANGLE/renderer/null/ShaderNULL.h"
#include "libANGLE/renderer/null/SyncNULL.h"
#include "libANGLE/renderer/null/TextureNULL.h"
#include "libANGLE/renderer/null/TransformFeedbackNULL.h"
#include "libANGLE/renderer/null/VertexArrayNULL.h"

namespace rx
{

AllocationTrackerNULL::AllocationTrackerNULL(size_t maxTotalAllocationSize)
    : mAllocatedBytes(0), mMaxBytes(maxTotalAllocationSize)
{}

AllocationTrackerNULL::~AllocationTrackerNULL()
{
    // ASSERT that all objects with the NULL renderer clean up after themselves
    ASSERT(mAllocatedBytes == 0);
}

bool AllocationTrackerNULL::updateMemoryAllocation(size_t oldSize, size_t newSize)
{
    ASSERT(mAllocatedBytes >= oldSize);

    size_t sizeAfterRelease    = mAllocatedBytes - oldSize;
    size_t sizeAfterReallocate = sizeAfterRelease + newSize;
    if (sizeAfterReallocate < sizeAfterRelease || sizeAfterReallocate > mMaxBytes)
    {
        // Overflow or allocation would be too large
        return false;
    }

    mAllocatedBytes = sizeAfterReallocate;
    return true;
}

ContextNULL::ContextNULL(const gl::State &state,
                         gl::ErrorSet *errorSet,
                         AllocationTrackerNULL *allocationTracker)
    : ContextImpl(state, errorSet), mAllocationTracker(allocationTracker)
{
    ASSERT(mAllocationTracker != nullptr);

    mExtensions                        = gl::Extensions();
    mExtensions.fence                  = true;
    mExtensions.instancedArraysANGLE   = true;
    mExtensions.instancedArraysEXT     = true;
    mExtensions.pixelBufferObject      = true;
    mExtensions.mapBuffer              = true;
    mExtensions.mapBufferRange         = true;
    mExtensions.copyTexture            = true;
    mExtensions.copyCompressedTexture  = true;
    mExtensions.textureRectangle       = true;
    mExtensions.textureUsage           = true;
    mExtensions.vertexArrayObject      = true;
    mExtensions.debugMarker            = true;
    mExtensions.translatedShaderSource = true;

    mExtensions.textureStorage             = true;
    mExtensions.rgb8rgba8                  = true;
    mExtensions.textureCompressionDXT1     = true;
    mExtensions.textureCompressionDXT3     = true;
    mExtensions.textureCompressionDXT5     = true;
    mExtensions.textureCompressionS3TCsRGB = true;
    mExtensions.textureCompressionASTCHDR  = true;
    mExtensions.textureCompressionASTCLDR  = true;
    mExtensions.compressedETC1RGB8Texture  = true;
    mExtensions.lossyETCDecode             = true;
    mExtensions.geometryShader             = true;

    mExtensions.eglImage                  = true;
    mExtensions.eglImageExternal          = true;
    mExtensions.eglImageExternalEssl3     = true;
    mExtensions.eglStreamConsumerExternal = true;

    const gl::Version maxClientVersion(3, 1);
    mCaps = GenerateMinimumCaps(maxClientVersion, mExtensions);

    InitMinimumTextureCapsMap(maxClientVersion, mExtensions, &mTextureCaps);
}

ContextNULL::~ContextNULL() {}

angle::Result ContextNULL::initialize()
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::flush(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::finish(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::drawArrays(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLint first,
                                      GLsizei count)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::drawArraysInstanced(const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               GLint first,
                                               GLsizei count,
                                               GLsizei instanceCount)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::drawElements(const gl::Context *context,
                                        gl::PrimitiveMode mode,
                                        GLsizei count,
                                        gl::DrawElementsType type,
                                        const void *indices)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::drawElementsInstanced(const gl::Context *context,
                                                 gl::PrimitiveMode mode,
                                                 GLsizei count,
                                                 gl::DrawElementsType type,
                                                 const void *indices,
                                                 GLsizei instances)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::drawRangeElements(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             GLuint start,
                                             GLuint end,
                                             GLsizei count,
                                             gl::DrawElementsType type,
                                             const void *indices)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::drawArraysIndirect(const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              const void *indirect)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::drawElementsIndirect(const gl::Context *context,
                                                gl::PrimitiveMode mode,
                                                gl::DrawElementsType type,
                                                const void *indirect)
{
    return angle::Result::Continue;
}

void ContextNULL::stencilFillPath(const gl::Path *path, GLenum fillMode, GLuint mask) {}

void ContextNULL::stencilStrokePath(const gl::Path *path, GLint reference, GLuint mask) {}

void ContextNULL::coverFillPath(const gl::Path *path, GLenum coverMode) {}

void ContextNULL::coverStrokePath(const gl::Path *path, GLenum coverMode) {}

void ContextNULL::stencilThenCoverFillPath(const gl::Path *path,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum coverMode)
{}

void ContextNULL::stencilThenCoverStrokePath(const gl::Path *path,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum coverMode)
{}

void ContextNULL::coverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                         GLenum coverMode,
                                         GLenum transformType,
                                         const GLfloat *transformValues)
{}

void ContextNULL::coverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                           GLenum coverMode,
                                           GLenum transformType,
                                           const GLfloat *transformValues)
{}

void ContextNULL::stencilFillPathInstanced(const std::vector<gl::Path *> &paths,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum transformType,
                                           const GLfloat *transformValues)
{}

void ContextNULL::stencilStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum transformType,
                                             const GLfloat *transformValues)
{}

void ContextNULL::stencilThenCoverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                                    GLenum coverMode,
                                                    GLenum fillMode,
                                                    GLuint mask,
                                                    GLenum transformType,
                                                    const GLfloat *transformValues)
{}

void ContextNULL::stencilThenCoverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                                      GLenum coverMode,
                                                      GLint reference,
                                                      GLuint mask,
                                                      GLenum transformType,
                                                      const GLfloat *transformValues)
{}

gl::GraphicsResetStatus ContextNULL::getResetStatus()
{
    return gl::GraphicsResetStatus::NoError;
}

std::string ContextNULL::getVendorString() const
{
    return "NULL";
}

std::string ContextNULL::getRendererDescription() const
{
    return "NULL";
}

void ContextNULL::insertEventMarker(GLsizei length, const char *marker) {}

void ContextNULL::pushGroupMarker(GLsizei length, const char *marker) {}

void ContextNULL::popGroupMarker() {}

void ContextNULL::pushDebugGroup(GLenum source, GLuint id, const std::string &message) {}

void ContextNULL::popDebugGroup() {}

angle::Result ContextNULL::syncState(const gl::Context *context,
                                     const gl::State::DirtyBits &dirtyBits,
                                     const gl::State::DirtyBits &bitMask)
{
    return angle::Result::Continue;
}

GLint ContextNULL::getGPUDisjoint()
{
    return 0;
}

GLint64 ContextNULL::getTimestamp()
{
    return 0;
}

angle::Result ContextNULL::onMakeCurrent(const gl::Context *context)
{
    return angle::Result::Continue;
}

gl::Caps ContextNULL::getNativeCaps() const
{
    return mCaps;
}

const gl::TextureCapsMap &ContextNULL::getNativeTextureCaps() const
{
    return mTextureCaps;
}

const gl::Extensions &ContextNULL::getNativeExtensions() const
{
    return mExtensions;
}

const gl::Limitations &ContextNULL::getNativeLimitations() const
{
    return mLimitations;
}

CompilerImpl *ContextNULL::createCompiler()
{
    return new CompilerNULL();
}

ShaderImpl *ContextNULL::createShader(const gl::ShaderState &data)
{
    return new ShaderNULL(data);
}

ProgramImpl *ContextNULL::createProgram(const gl::ProgramState &data)
{
    return new ProgramNULL(data);
}

FramebufferImpl *ContextNULL::createFramebuffer(const gl::FramebufferState &data)
{
    return new FramebufferNULL(data);
}

TextureImpl *ContextNULL::createTexture(const gl::TextureState &state)
{
    return new TextureNULL(state);
}

RenderbufferImpl *ContextNULL::createRenderbuffer(const gl::RenderbufferState &state)
{
    return new RenderbufferNULL(state);
}

BufferImpl *ContextNULL::createBuffer(const gl::BufferState &state)
{
    return new BufferNULL(state, mAllocationTracker);
}

VertexArrayImpl *ContextNULL::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArrayNULL(data);
}

QueryImpl *ContextNULL::createQuery(gl::QueryType type)
{
    return new QueryNULL(type);
}

FenceNVImpl *ContextNULL::createFenceNV()
{
    return new FenceNVNULL();
}

SyncImpl *ContextNULL::createSync()
{
    return new SyncNULL();
}

TransformFeedbackImpl *ContextNULL::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedbackNULL(state);
}

SamplerImpl *ContextNULL::createSampler(const gl::SamplerState &state)
{
    return new SamplerNULL(state);
}

ProgramPipelineImpl *ContextNULL::createProgramPipeline(const gl::ProgramPipelineState &state)
{
    return new ProgramPipelineNULL(state);
}

std::vector<PathImpl *> ContextNULL::createPaths(GLsizei range)
{
    std::vector<PathImpl *> result(range);
    for (GLsizei idx = 0; idx < range; idx++)
    {
        result[idx] = new PathNULL();
    }
    return result;
}

MemoryObjectImpl *ContextNULL::createMemoryObject()
{
    UNREACHABLE();
    return nullptr;
}

angle::Result ContextNULL::dispatchCompute(const gl::Context *context,
                                           GLuint numGroupsX,
                                           GLuint numGroupsY,
                                           GLuint numGroupsZ)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::memoryBarrier(const gl::Context *context, GLbitfield barriers)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers)
{
    return angle::Result::Continue;
}

void ContextNULL::handleError(GLenum errorCode,
                              const char *message,
                              const char *file,
                              const char *function,
                              unsigned int line)
{
    std::stringstream errorStream;
    errorStream << "Internal NULL back-end error: " << message << ".";
    mErrors->handleError(errorCode, errorStream.str().c_str(), file, function, line);
}
}  // namespace rx
