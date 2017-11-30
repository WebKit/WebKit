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
{
}

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

ContextNULL::ContextNULL(const gl::ContextState &state, AllocationTrackerNULL *allocationTracker)
    : ContextImpl(state), mAllocationTracker(allocationTracker)
{
    ASSERT(mAllocationTracker != nullptr);

    mExtensions                       = gl::Extensions();
    mExtensions.fence                 = true;
    mExtensions.instancedArrays       = true;
    mExtensions.pixelBufferObject     = true;
    mExtensions.mapBuffer             = true;
    mExtensions.mapBufferRange        = true;
    mExtensions.copyTexture           = true;
    mExtensions.copyCompressedTexture = true;
    mExtensions.textureRectangle      = true;
    mExtensions.textureUsage           = true;
    mExtensions.vertexArrayObject      = true;
    mExtensions.debugMarker            = true;
    mExtensions.translatedShaderSource = true;

    mExtensions.rgb8rgba8 = true;
    mExtensions.textureCompressionDXT1     = true;
    mExtensions.textureCompressionDXT3     = true;
    mExtensions.textureCompressionDXT5     = true;
    mExtensions.textureCompressionS3TCsRGB = true;
    mExtensions.textureCompressionASTCHDR  = true;
    mExtensions.textureCompressionASTCLDR  = true;
    mExtensions.compressedETC1RGB8Texture  = true;
    mExtensions.lossyETCDecode             = true;

    const gl::Version maxClientVersion(3, 1);
    mCaps = GenerateMinimumCaps(maxClientVersion, mExtensions);

    InitMinimumTextureCapsMap(maxClientVersion, mExtensions, &mTextureCaps);
}

ContextNULL::~ContextNULL()
{
}

gl::Error ContextNULL::initialize()
{
    return gl::NoError();
}

gl::Error ContextNULL::flush(const gl::Context *context)
{
    return gl::NoError();
}

gl::Error ContextNULL::finish(const gl::Context *context)
{
    return gl::NoError();
}

gl::Error ContextNULL::drawArrays(const gl::Context *context,
                                  GLenum mode,
                                  GLint first,
                                  GLsizei count)
{
    return gl::NoError();
}

gl::Error ContextNULL::drawArraysInstanced(const gl::Context *context,
                                           GLenum mode,
                                           GLint first,
                                           GLsizei count,
                                           GLsizei instanceCount)
{
    return gl::NoError();
}

gl::Error ContextNULL::drawElements(const gl::Context *context,
                                    GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const void *indices)
{
    return gl::NoError();
}

gl::Error ContextNULL::drawElementsInstanced(const gl::Context *context,
                                             GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             const void *indices,
                                             GLsizei instances)
{
    return gl::NoError();
}

gl::Error ContextNULL::drawRangeElements(const gl::Context *context,
                                         GLenum mode,
                                         GLuint start,
                                         GLuint end,
                                         GLsizei count,
                                         GLenum type,
                                         const void *indices)
{
    return gl::NoError();
}

gl::Error ContextNULL::drawArraysIndirect(const gl::Context *context,
                                          GLenum mode,
                                          const void *indirect)
{
    return gl::NoError();
}

gl::Error ContextNULL::drawElementsIndirect(const gl::Context *context,
                                            GLenum mode,
                                            GLenum type,
                                            const void *indirect)
{
    return gl::NoError();
}

void ContextNULL::stencilFillPath(const gl::Path *path, GLenum fillMode, GLuint mask)
{
}

void ContextNULL::stencilStrokePath(const gl::Path *path, GLint reference, GLuint mask)
{
}

void ContextNULL::coverFillPath(const gl::Path *path, GLenum coverMode)
{
}

void ContextNULL::coverStrokePath(const gl::Path *path, GLenum coverMode)
{
}

void ContextNULL::stencilThenCoverFillPath(const gl::Path *path,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum coverMode)
{
}

void ContextNULL::stencilThenCoverStrokePath(const gl::Path *path,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum coverMode)
{
}

void ContextNULL::coverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                         GLenum coverMode,
                                         GLenum transformType,
                                         const GLfloat *transformValues)
{
}

void ContextNULL::coverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                           GLenum coverMode,
                                           GLenum transformType,
                                           const GLfloat *transformValues)
{
}

void ContextNULL::stencilFillPathInstanced(const std::vector<gl::Path *> &paths,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum transformType,
                                           const GLfloat *transformValues)
{
}

void ContextNULL::stencilStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum transformType,
                                             const GLfloat *transformValues)
{
}

void ContextNULL::stencilThenCoverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                                    GLenum coverMode,
                                                    GLenum fillMode,
                                                    GLuint mask,
                                                    GLenum transformType,
                                                    const GLfloat *transformValues)
{
}

void ContextNULL::stencilThenCoverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                                      GLenum coverMode,
                                                      GLint reference,
                                                      GLuint mask,
                                                      GLenum transformType,
                                                      const GLfloat *transformValues)
{
}

GLenum ContextNULL::getResetStatus()
{
    return GL_NO_ERROR;
}

std::string ContextNULL::getVendorString() const
{
    return "NULL";
}

std::string ContextNULL::getRendererDescription() const
{
    return "NULL";
}

void ContextNULL::insertEventMarker(GLsizei length, const char *marker)
{
}

void ContextNULL::pushGroupMarker(GLsizei length, const char *marker)
{
}

void ContextNULL::popGroupMarker()
{
}

void ContextNULL::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message)
{
}

void ContextNULL::popDebugGroup()
{
}

void ContextNULL::syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits)
{
}

GLint ContextNULL::getGPUDisjoint()
{
    return 0;
}

GLint64 ContextNULL::getTimestamp()
{
    return 0;
}

void ContextNULL::onMakeCurrent(const gl::Context *context)
{
}

const gl::Caps &ContextNULL::getNativeCaps() const
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

RenderbufferImpl *ContextNULL::createRenderbuffer()
{
    return new RenderbufferNULL();
}

BufferImpl *ContextNULL::createBuffer(const gl::BufferState &state)
{
    return new BufferNULL(state, mAllocationTracker);
}

VertexArrayImpl *ContextNULL::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArrayNULL(data);
}

QueryImpl *ContextNULL::createQuery(GLenum type)
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

gl::Error ContextNULL::dispatchCompute(const gl::Context *context,
                                       GLuint numGroupsX,
                                       GLuint numGroupsY,
                                       GLuint numGroupsZ)
{
    return gl::NoError();
}

}  // namespace rx
