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
#include "libANGLE/renderer/OverlayImpl.h"
#include "libANGLE/renderer/null/BufferNULL.h"
#include "libANGLE/renderer/null/CompilerNULL.h"
#include "libANGLE/renderer/null/DisplayNULL.h"
#include "libANGLE/renderer/null/FenceNVNULL.h"
#include "libANGLE/renderer/null/FramebufferNULL.h"
#include "libANGLE/renderer/null/ImageNULL.h"
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
    mExtensions.fenceNV                = true;
    mExtensions.framebufferBlit        = true;
    mExtensions.instancedArraysANGLE   = true;
    mExtensions.instancedArraysEXT     = true;
    mExtensions.pixelBufferObjectNV    = true;
    mExtensions.mapBufferOES           = true;
    mExtensions.mapBufferRange         = true;
    mExtensions.copyTexture            = true;
    mExtensions.copyCompressedTexture  = true;
    mExtensions.textureRectangle       = true;
    mExtensions.textureUsage           = true;
    mExtensions.vertexArrayObjectOES   = true;
    mExtensions.debugMarker            = true;
    mExtensions.translatedShaderSource = true;

    mExtensions.textureStorage               = true;
    mExtensions.rgb8rgba8OES                 = true;
    mExtensions.textureCompressionDXT1       = true;
    mExtensions.textureCompressionDXT3       = true;
    mExtensions.textureCompressionDXT5       = true;
    mExtensions.textureCompressionS3TCsRGB   = true;
    mExtensions.textureCompressionASTCHDRKHR = true;
    mExtensions.textureCompressionASTCLDRKHR = true;
    mExtensions.textureCompressionASTCOES    = true;
    mExtensions.compressedETC1RGB8TextureOES = true;
    mExtensions.compressedETC1RGB8SubTexture = true;
    mExtensions.lossyETCDecode               = true;
    mExtensions.geometryShader               = true;

    mExtensions.eglImageOES                 = true;
    mExtensions.eglImageExternalOES         = true;
    mExtensions.eglImageExternalEssl3OES    = true;
    mExtensions.eglImageArray               = true;
    mExtensions.eglStreamConsumerExternalNV = true;

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

angle::Result ContextNULL::drawArraysInstancedBaseInstance(const gl::Context *context,
                                                           gl::PrimitiveMode mode,
                                                           GLint first,
                                                           GLsizei count,
                                                           GLsizei instanceCount,
                                                           GLuint baseInstance)
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

angle::Result ContextNULL::drawElementsBaseVertex(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  GLsizei count,
                                                  gl::DrawElementsType type,
                                                  const void *indices,
                                                  GLint baseVertex)
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

angle::Result ContextNULL::drawElementsInstancedBaseVertex(const gl::Context *context,
                                                           gl::PrimitiveMode mode,
                                                           GLsizei count,
                                                           gl::DrawElementsType type,
                                                           const void *indices,
                                                           GLsizei instances,
                                                           GLint baseVertex)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::drawElementsInstancedBaseVertexBaseInstance(const gl::Context *context,
                                                                       gl::PrimitiveMode mode,
                                                                       GLsizei count,
                                                                       gl::DrawElementsType type,
                                                                       const void *indices,
                                                                       GLsizei instances,
                                                                       GLint baseVertex,
                                                                       GLuint baseInstance)
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

angle::Result ContextNULL::drawRangeElementsBaseVertex(const gl::Context *context,
                                                       gl::PrimitiveMode mode,
                                                       GLuint start,
                                                       GLuint end,
                                                       GLsizei count,
                                                       gl::DrawElementsType type,
                                                       const void *indices,
                                                       GLint baseVertex)
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

angle::Result ContextNULL::multiDrawArrays(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           const GLint *firsts,
                                           const GLsizei *counts,
                                           GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::multiDrawArraysInstanced(const gl::Context *context,
                                                    gl::PrimitiveMode mode,
                                                    const GLint *firsts,
                                                    const GLsizei *counts,
                                                    const GLsizei *instanceCounts,
                                                    GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::multiDrawElements(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             const GLsizei *counts,
                                             gl::DrawElementsType type,
                                             const GLvoid *const *indices,
                                             GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::multiDrawElementsInstanced(const gl::Context *context,
                                                      gl::PrimitiveMode mode,
                                                      const GLsizei *counts,
                                                      gl::DrawElementsType type,
                                                      const GLvoid *const *indices,
                                                      const GLsizei *instanceCounts,
                                                      GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::multiDrawArraysInstancedBaseInstance(const gl::Context *context,
                                                                gl::PrimitiveMode mode,
                                                                const GLint *firsts,
                                                                const GLsizei *counts,
                                                                const GLsizei *instanceCounts,
                                                                const GLuint *baseInstances,
                                                                GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::multiDrawElementsInstancedBaseVertexBaseInstance(
    const gl::Context *context,
    gl::PrimitiveMode mode,
    const GLsizei *counts,
    gl::DrawElementsType type,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount)
{
    return angle::Result::Continue;
}

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

angle::Result ContextNULL::insertEventMarker(GLsizei length, const char *marker)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::pushGroupMarker(GLsizei length, const char *marker)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::popGroupMarker()
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::pushDebugGroup(const gl::Context *context,
                                          GLenum source,
                                          GLuint id,
                                          const std::string &message)
{
    return angle::Result::Continue;
}

angle::Result ContextNULL::popDebugGroup(const gl::Context *context)
{
    return angle::Result::Continue;
}

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

MemoryObjectImpl *ContextNULL::createMemoryObject()
{
    UNREACHABLE();
    return nullptr;
}

SemaphoreImpl *ContextNULL::createSemaphore()
{
    UNREACHABLE();
    return nullptr;
}

OverlayImpl *ContextNULL::createOverlay(const gl::OverlayState &state)
{
    return new OverlayImpl(state);
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
