//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Context11:
//   D3D11-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/d3d/d3d11/Context11.h"

#include "common/string_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/MemoryProgramCache.h"
#include "libANGLE/renderer/d3d/CompilerD3D.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/SamplerD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Fence11.h"
#include "libANGLE/renderer/d3d/d3d11/Framebuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/ProgramPipeline11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/StateManager11.h"
#include "libANGLE/renderer/d3d/d3d11/TransformFeedback11.h"
#include "libANGLE/renderer/d3d/d3d11/VertexArray11.h"

namespace rx
{

Context11::Context11(const gl::ContextState &state, Renderer11 *renderer)
    : ContextImpl(state), mRenderer(renderer)
{
}

Context11::~Context11()
{
}

gl::Error Context11::initialize()
{
    return gl::NoError();
}

CompilerImpl *Context11::createCompiler()
{
    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        return new CompilerD3D(SH_HLSL_4_0_FL9_3_OUTPUT);
    }
    else
    {
        return new CompilerD3D(SH_HLSL_4_1_OUTPUT);
    }
}

ShaderImpl *Context11::createShader(const gl::ShaderState &data)
{
    return new ShaderD3D(data, mRenderer->getWorkarounds(), mRenderer->getNativeExtensions());
}

ProgramImpl *Context11::createProgram(const gl::ProgramState &data)
{
    return new ProgramD3D(data, mRenderer);
}

FramebufferImpl *Context11::createFramebuffer(const gl::FramebufferState &data)
{
    return new Framebuffer11(data, mRenderer);
}

TextureImpl *Context11::createTexture(const gl::TextureState &state)
{
    switch (state.getTarget())
    {
        case GL_TEXTURE_2D:
            return new TextureD3D_2D(state, mRenderer);
        case GL_TEXTURE_CUBE_MAP:
            return new TextureD3D_Cube(state, mRenderer);
        case GL_TEXTURE_3D:
            return new TextureD3D_3D(state, mRenderer);
        case GL_TEXTURE_2D_ARRAY:
            return new TextureD3D_2DArray(state, mRenderer);
        case GL_TEXTURE_EXTERNAL_OES:
            return new TextureD3D_External(state, mRenderer);
        case GL_TEXTURE_2D_MULTISAMPLE:
            return new TextureD3D_2DMultisample(state, mRenderer);
            break;
        default:
            UNREACHABLE();
    }

    return nullptr;
}

RenderbufferImpl *Context11::createRenderbuffer()
{
    return new RenderbufferD3D(mRenderer);
}

BufferImpl *Context11::createBuffer(const gl::BufferState &state)
{
    Buffer11 *buffer = new Buffer11(state, mRenderer);
    mRenderer->onBufferCreate(buffer);
    return buffer;
}

VertexArrayImpl *Context11::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArray11(data);
}

QueryImpl *Context11::createQuery(GLenum type)
{
    return new Query11(mRenderer, type);
}

FenceNVImpl *Context11::createFenceNV()
{
    return new FenceNV11(mRenderer);
}

SyncImpl *Context11::createSync()
{
    return new Sync11(mRenderer);
}

TransformFeedbackImpl *Context11::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedback11(state, mRenderer);
}

SamplerImpl *Context11::createSampler(const gl::SamplerState &state)
{
    return new SamplerD3D(state);
}

ProgramPipelineImpl *Context11::createProgramPipeline(const gl::ProgramPipelineState &data)
{
    return new ProgramPipeline11(data);
}

std::vector<PathImpl *> Context11::createPaths(GLsizei)
{
    return std::vector<PathImpl *>();
}

gl::Error Context11::flush(const gl::Context *context)
{
    return mRenderer->flush();
}

gl::Error Context11::finish(const gl::Context *context)
{
    return mRenderer->finish();
}

gl::Error Context11::drawArrays(const gl::Context *context, GLenum mode, GLint first, GLsizei count)
{
    ANGLE_TRY(prepareForDrawCall(context, mode));
    return mRenderer->drawArrays(context, mode, first, count, 0);
}

gl::Error Context11::drawArraysInstanced(const gl::Context *context,
                                         GLenum mode,
                                         GLint first,
                                         GLsizei count,
                                         GLsizei instanceCount)
{
    ANGLE_TRY(prepareForDrawCall(context, mode));
    return mRenderer->drawArrays(context, mode, first, count, instanceCount);
}

gl::Error Context11::drawElements(const gl::Context *context,
                                  GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void *indices)
{
    ANGLE_TRY(prepareForDrawCall(context, mode));
    return mRenderer->drawElements(context, mode, count, type, indices, 0);
}

gl::Error Context11::drawElementsInstanced(const gl::Context *context,
                                           GLenum mode,
                                           GLsizei count,
                                           GLenum type,
                                           const void *indices,
                                           GLsizei instances)
{
    ANGLE_TRY(prepareForDrawCall(context, mode));
    return mRenderer->drawElements(context, mode, count, type, indices, instances);
}

gl::Error Context11::drawRangeElements(const gl::Context *context,
                                       GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void *indices)
{
    ANGLE_TRY(prepareForDrawCall(context, mode));
    return mRenderer->drawElements(context, mode, count, type, indices, 0);
}

gl::Error Context11::drawArraysIndirect(const gl::Context *context,
                                        GLenum mode,
                                        const void *indirect)
{
    ANGLE_TRY(prepareForDrawCall(context, mode));
    return mRenderer->drawArraysIndirect(context, mode, indirect);
}

gl::Error Context11::drawElementsIndirect(const gl::Context *context,
                                          GLenum mode,
                                          GLenum type,
                                          const void *indirect)
{
    ANGLE_TRY(prepareForDrawCall(context, mode));
    return mRenderer->drawElementsIndirect(context, mode, type, indirect);
}

GLenum Context11::getResetStatus()
{
    return mRenderer->getResetStatus();
}

std::string Context11::getVendorString() const
{
    return mRenderer->getVendorString();
}

std::string Context11::getRendererDescription() const
{
    return mRenderer->getRendererDescription();
}

void Context11::insertEventMarker(GLsizei length, const char *marker)
{
    auto optionalString = angle::WidenString(static_cast<size_t>(length), marker);
    if (optionalString.valid())
    {
        mRenderer->getAnnotator()->setMarker(optionalString.value().data());
    }
}

void Context11::pushGroupMarker(GLsizei length, const char *marker)
{
    auto optionalString = angle::WidenString(static_cast<size_t>(length), marker);
    if (optionalString.valid())
    {
        mRenderer->getAnnotator()->beginEvent(optionalString.value().data());
    }
}

void Context11::popGroupMarker()
{
    mRenderer->getAnnotator()->endEvent();
}

void Context11::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message)
{
    // Fall through to the EXT_debug_marker functions
    pushGroupMarker(length, message);
}

void Context11::popDebugGroup()
{
    // Fall through to the EXT_debug_marker functions
    popGroupMarker();
}

void Context11::syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits)
{
    mRenderer->getStateManager()->syncState(context, dirtyBits);
}

GLint Context11::getGPUDisjoint()
{
    return mRenderer->getGPUDisjoint();
}

GLint64 Context11::getTimestamp()
{
    return mRenderer->getTimestamp();
}

void Context11::onMakeCurrent(const gl::Context *context)
{
    ANGLE_SWALLOW_ERR(mRenderer->getStateManager()->onMakeCurrent(context));
}

const gl::Caps &Context11::getNativeCaps() const
{
    return mRenderer->getNativeCaps();
}

const gl::TextureCapsMap &Context11::getNativeTextureCaps() const
{
    return mRenderer->getNativeTextureCaps();
}

const gl::Extensions &Context11::getNativeExtensions() const
{
    return mRenderer->getNativeExtensions();
}

const gl::Limitations &Context11::getNativeLimitations() const
{
    return mRenderer->getNativeLimitations();
}

gl::Error Context11::dispatchCompute(const gl::Context *context,
                                     GLuint numGroupsX,
                                     GLuint numGroupsY,
                                     GLuint numGroupsZ)
{
    return mRenderer->dispatchCompute(context, numGroupsX, numGroupsY, numGroupsZ);
}

gl::Error Context11::triggerDrawCallProgramRecompilation(const gl::Context *context,
                                                         GLenum drawMode)
{
    const auto &glState    = context->getGLState();
    const auto *va11       = GetImplAs<VertexArray11>(glState.getVertexArray());
    const auto *drawFBO    = glState.getDrawFramebuffer();
    gl::Program *program   = glState.getProgram();
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);

    programD3D->updateCachedInputLayout(va11->getCurrentStateSerial(), glState);
    programD3D->updateCachedOutputLayout(context, drawFBO);

    bool recompileVS = !programD3D->hasVertexExecutableForCachedInputLayout();
    bool recompileGS = !programD3D->hasGeometryExecutableForPrimitiveType(drawMode);
    bool recompilePS = !programD3D->hasPixelExecutableForCachedOutputLayout();

    if (!recompileVS && !recompileGS && !recompilePS)
    {
        return gl::NoError();
    }

    // Load the compiler if necessary and recompile the programs.
    ANGLE_TRY(mRenderer->ensureHLSLCompilerInitialized());

    gl::InfoLog infoLog;

    if (recompileVS)
    {
        ShaderExecutableD3D *vertexExe = nullptr;
        ANGLE_TRY(programD3D->getVertexExecutableForCachedInputLayout(&vertexExe, &infoLog));
        if (!programD3D->hasVertexExecutableForCachedInputLayout())
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Dynamic recompilation error log: " << infoLog.str();
            return gl::InternalError()
                   << "Error compiling dynamic vertex executable:" << infoLog.str();
        }
    }

    if (recompileGS)
    {
        ShaderExecutableD3D *geometryExe = nullptr;
        ANGLE_TRY(programD3D->getGeometryExecutableForPrimitiveType(context, drawMode, &geometryExe,
                                                                    &infoLog));
        if (!programD3D->hasGeometryExecutableForPrimitiveType(drawMode))
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Dynamic recompilation error log: " << infoLog.str();
            return gl::InternalError()
                   << "Error compiling dynamic geometry executable:" << infoLog.str();
        }
    }

    if (recompilePS)
    {
        ShaderExecutableD3D *pixelExe = nullptr;
        ANGLE_TRY(programD3D->getPixelExecutableForCachedOutputLayout(&pixelExe, &infoLog));
        if (!programD3D->hasPixelExecutableForCachedOutputLayout())
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Dynamic recompilation error log: " << infoLog.str();
            return gl::InternalError()
                   << "Error compiling dynamic pixel executable:" << infoLog.str();
        }
    }

    // Refresh the program cache entry.
    if (mMemoryProgramCache)
    {
        mMemoryProgramCache->updateProgram(context, program);
    }

    return gl::NoError();
}

gl::Error Context11::prepareForDrawCall(const gl::Context *context, GLenum drawMode)
{
    ANGLE_TRY(mRenderer->getStateManager()->updateState(context, drawMode));
    return gl::NoError();
}

}  // namespace rx
