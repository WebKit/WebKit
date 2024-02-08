//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextWgpu.cpp:
//    Implements the class methods for ContextWgpu.
//

#include "libANGLE/renderer/wgpu/ContextWgpu.h"

#include "common/debug.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/OverlayImpl.h"
#include "libANGLE/renderer/wgpu/BufferWgpu.h"
#include "libANGLE/renderer/wgpu/CompilerWgpu.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/FenceNVWgpu.h"
#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"
#include "libANGLE/renderer/wgpu/ImageWgpu.h"
#include "libANGLE/renderer/wgpu/ProgramExecutableWgpu.h"
#include "libANGLE/renderer/wgpu/ProgramPipelineWgpu.h"
#include "libANGLE/renderer/wgpu/ProgramWgpu.h"
#include "libANGLE/renderer/wgpu/QueryWgpu.h"
#include "libANGLE/renderer/wgpu/RenderbufferWgpu.h"
#include "libANGLE/renderer/wgpu/SamplerWgpu.h"
#include "libANGLE/renderer/wgpu/ShaderWgpu.h"
#include "libANGLE/renderer/wgpu/SyncWgpu.h"
#include "libANGLE/renderer/wgpu/TextureWgpu.h"
#include "libANGLE/renderer/wgpu/TransformFeedbackWgpu.h"
#include "libANGLE/renderer/wgpu/VertexArrayWgpu.h"

namespace rx
{

ContextWgpu::ContextWgpu(const gl::State &state, gl::ErrorSet *errorSet)
    : ContextImpl(state, errorSet)
{
    mExtensions                               = gl::Extensions();
    mExtensions.blendEquationAdvancedKHR      = true;
    mExtensions.blendFuncExtendedEXT          = true;
    mExtensions.copyCompressedTextureCHROMIUM = true;
    mExtensions.copyTextureCHROMIUM           = true;
    mExtensions.debugMarkerEXT                = true;
    mExtensions.drawBuffersIndexedOES         = true;
    mExtensions.fenceNV                       = true;
    mExtensions.framebufferBlitANGLE          = true;
    mExtensions.framebufferBlitNV             = true;
    mExtensions.instancedArraysANGLE          = true;
    mExtensions.instancedArraysEXT            = true;
    mExtensions.mapBufferRangeEXT             = true;
    mExtensions.mapbufferOES                  = true;
    mExtensions.pixelBufferObjectNV           = true;
    mExtensions.shaderPixelLocalStorageANGLE  = state.getClientVersion() >= gl::Version(3, 0);
    mExtensions.shaderPixelLocalStorageCoherentANGLE = mExtensions.shaderPixelLocalStorageANGLE;
    mExtensions.textureRectangleANGLE                = true;
    mExtensions.textureUsageANGLE                    = true;
    mExtensions.translatedShaderSourceANGLE          = true;
    mExtensions.vertexArrayObjectOES                 = true;

    mExtensions.textureStorageEXT               = true;
    mExtensions.rgb8Rgba8OES                    = true;
    mExtensions.textureCompressionDxt1EXT       = true;
    mExtensions.textureCompressionDxt3ANGLE     = true;
    mExtensions.textureCompressionDxt5ANGLE     = true;
    mExtensions.textureCompressionS3tcSrgbEXT   = true;
    mExtensions.textureCompressionAstcHdrKHR    = true;
    mExtensions.textureCompressionAstcLdrKHR    = true;
    mExtensions.textureCompressionAstcOES       = true;
    mExtensions.compressedETC1RGB8TextureOES    = true;
    mExtensions.compressedETC1RGB8SubTextureEXT = true;
    mExtensions.lossyEtcDecodeANGLE             = true;
    mExtensions.geometryShaderEXT               = true;
    mExtensions.geometryShaderOES               = true;
    mExtensions.multiDrawIndirectEXT            = true;

    mExtensions.EGLImageOES                 = true;
    mExtensions.EGLImageExternalOES         = true;
    mExtensions.EGLImageExternalEssl3OES    = true;
    mExtensions.EGLImageArrayEXT            = true;
    mExtensions.EGLStreamConsumerExternalNV = true;

    const gl::Version maxClientVersion(3, 1);
    mCaps = GenerateMinimumCaps(maxClientVersion, mExtensions);

    InitMinimumTextureCapsMap(maxClientVersion, mExtensions, &mTextureCaps);

    if (mExtensions.shaderPixelLocalStorageANGLE)
    {
        mPLSOptions.type             = ShPixelLocalStorageType::FramebufferFetch;
        mPLSOptions.fragmentSyncType = ShFragmentSynchronizationType::Automatic;
    }
}

ContextWgpu::~ContextWgpu() {}

angle::Result ContextWgpu::initialize()
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::flush(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::finish(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArrays(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLint first,
                                      GLsizei count)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArraysInstanced(const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               GLint first,
                                               GLsizei count,
                                               GLsizei instanceCount)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawArraysInstancedBaseInstance(const gl::Context *context,
                                                           gl::PrimitiveMode mode,
                                                           GLint first,
                                                           GLsizei count,
                                                           GLsizei instanceCount,
                                                           GLuint baseInstance)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElements(const gl::Context *context,
                                        gl::PrimitiveMode mode,
                                        GLsizei count,
                                        gl::DrawElementsType type,
                                        const void *indices)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsBaseVertex(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  GLsizei count,
                                                  gl::DrawElementsType type,
                                                  const void *indices,
                                                  GLint baseVertex)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsInstanced(const gl::Context *context,
                                                 gl::PrimitiveMode mode,
                                                 GLsizei count,
                                                 gl::DrawElementsType type,
                                                 const void *indices,
                                                 GLsizei instances)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsInstancedBaseVertex(const gl::Context *context,
                                                           gl::PrimitiveMode mode,
                                                           GLsizei count,
                                                           gl::DrawElementsType type,
                                                           const void *indices,
                                                           GLsizei instances,
                                                           GLint baseVertex)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsInstancedBaseVertexBaseInstance(const gl::Context *context,
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

angle::Result ContextWgpu::drawRangeElements(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             GLuint start,
                                             GLuint end,
                                             GLsizei count,
                                             gl::DrawElementsType type,
                                             const void *indices)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawRangeElementsBaseVertex(const gl::Context *context,
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

angle::Result ContextWgpu::drawArraysIndirect(const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              const void *indirect)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::drawElementsIndirect(const gl::Context *context,
                                                gl::PrimitiveMode mode,
                                                gl::DrawElementsType type,
                                                const void *indirect)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawArrays(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           const GLint *firsts,
                                           const GLsizei *counts,
                                           GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawArraysInstanced(const gl::Context *context,
                                                    gl::PrimitiveMode mode,
                                                    const GLint *firsts,
                                                    const GLsizei *counts,
                                                    const GLsizei *instanceCounts,
                                                    GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawArraysIndirect(const gl::Context *context,
                                                   gl::PrimitiveMode mode,
                                                   const void *indirect,
                                                   GLsizei drawcount,
                                                   GLsizei stride)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawElements(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             const GLsizei *counts,
                                             gl::DrawElementsType type,
                                             const GLvoid *const *indices,
                                             GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawElementsInstanced(const gl::Context *context,
                                                      gl::PrimitiveMode mode,
                                                      const GLsizei *counts,
                                                      gl::DrawElementsType type,
                                                      const GLvoid *const *indices,
                                                      const GLsizei *instanceCounts,
                                                      GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawElementsIndirect(const gl::Context *context,
                                                     gl::PrimitiveMode mode,
                                                     gl::DrawElementsType type,
                                                     const void *indirect,
                                                     GLsizei drawcount,
                                                     GLsizei stride)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawArraysInstancedBaseInstance(const gl::Context *context,
                                                                gl::PrimitiveMode mode,
                                                                const GLint *firsts,
                                                                const GLsizei *counts,
                                                                const GLsizei *instanceCounts,
                                                                const GLuint *baseInstances,
                                                                GLsizei drawcount)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::multiDrawElementsInstancedBaseVertexBaseInstance(
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

gl::GraphicsResetStatus ContextWgpu::getResetStatus()
{
    return gl::GraphicsResetStatus::NoError;
}

angle::Result ContextWgpu::insertEventMarker(GLsizei length, const char *marker)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::pushGroupMarker(GLsizei length, const char *marker)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::popGroupMarker()
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::pushDebugGroup(const gl::Context *context,
                                          GLenum source,
                                          GLuint id,
                                          const std::string &message)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::popDebugGroup(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::syncState(const gl::Context *context,
                                     const gl::state::DirtyBits dirtyBits,
                                     const gl::state::DirtyBits bitMask,
                                     const gl::state::ExtendedDirtyBits extendedDirtyBits,
                                     const gl::state::ExtendedDirtyBits extendedBitMask,
                                     gl::Command command)
{
    return angle::Result::Continue;
}

GLint ContextWgpu::getGPUDisjoint()
{
    return 0;
}

GLint64 ContextWgpu::getTimestamp()
{
    return 0;
}

angle::Result ContextWgpu::onMakeCurrent(const gl::Context *context)
{
    return angle::Result::Continue;
}

gl::Caps ContextWgpu::getNativeCaps() const
{
    return mCaps;
}

const gl::TextureCapsMap &ContextWgpu::getNativeTextureCaps() const
{
    return mTextureCaps;
}

const gl::Extensions &ContextWgpu::getNativeExtensions() const
{
    return mExtensions;
}

const gl::Limitations &ContextWgpu::getNativeLimitations() const
{
    return mLimitations;
}

const ShPixelLocalStorageOptions &ContextWgpu::getNativePixelLocalStorageOptions() const
{
    return mPLSOptions;
}

CompilerImpl *ContextWgpu::createCompiler()
{
    return new CompilerWgpu();
}

ShaderImpl *ContextWgpu::createShader(const gl::ShaderState &data)
{
    return new ShaderWgpu(data);
}

ProgramImpl *ContextWgpu::createProgram(const gl::ProgramState &data)
{
    return new ProgramWgpu(data);
}

ProgramExecutableImpl *ContextWgpu::createProgramExecutable(const gl::ProgramExecutable *executable)
{
    return new ProgramExecutableWgpu(executable);
}

FramebufferImpl *ContextWgpu::createFramebuffer(const gl::FramebufferState &data)
{
    return new FramebufferWgpu(data);
}

TextureImpl *ContextWgpu::createTexture(const gl::TextureState &state)
{
    return new TextureWgpu(state);
}

RenderbufferImpl *ContextWgpu::createRenderbuffer(const gl::RenderbufferState &state)
{
    return new RenderbufferWgpu(state);
}

BufferImpl *ContextWgpu::createBuffer(const gl::BufferState &state)
{
    return new BufferWgpu(state);
}

VertexArrayImpl *ContextWgpu::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArrayWgpu(data);
}

QueryImpl *ContextWgpu::createQuery(gl::QueryType type)
{
    return new QueryWgpu(type);
}

FenceNVImpl *ContextWgpu::createFenceNV()
{
    return new FenceNVWgpu();
}

SyncImpl *ContextWgpu::createSync()
{
    return new SyncWgpu();
}

TransformFeedbackImpl *ContextWgpu::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedbackWgpu(state);
}

SamplerImpl *ContextWgpu::createSampler(const gl::SamplerState &state)
{
    return new SamplerWgpu(state);
}

ProgramPipelineImpl *ContextWgpu::createProgramPipeline(const gl::ProgramPipelineState &state)
{
    return new ProgramPipelineWgpu(state);
}

MemoryObjectImpl *ContextWgpu::createMemoryObject()
{
    UNREACHABLE();
    return nullptr;
}

SemaphoreImpl *ContextWgpu::createSemaphore()
{
    UNREACHABLE();
    return nullptr;
}

OverlayImpl *ContextWgpu::createOverlay(const gl::OverlayState &state)
{
    return new OverlayImpl(state);
}

angle::Result ContextWgpu::dispatchCompute(const gl::Context *context,
                                           GLuint numGroupsX,
                                           GLuint numGroupsY,
                                           GLuint numGroupsZ)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::memoryBarrier(const gl::Context *context, GLbitfield barriers)
{
    return angle::Result::Continue;
}

angle::Result ContextWgpu::memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers)
{
    return angle::Result::Continue;
}

void ContextWgpu::handleError(GLenum errorCode,
                              const char *message,
                              const char *file,
                              const char *function,
                              unsigned int line)
{
    std::stringstream errorStream;
    errorStream << "Internal Wgpu back-end error: " << message << ".";
    mErrors->handleError(errorCode, errorStream.str().c_str(), file, function, line);
}
}  // namespace rx
