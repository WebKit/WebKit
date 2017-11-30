//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextGL:
//   OpenGL-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/gl/ContextGL.h"

#include "libANGLE/renderer/gl/BufferGL.h"
#include "libANGLE/renderer/gl/CompilerGL.h"
#include "libANGLE/renderer/gl/FenceNVGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/PathGL.h"
#include "libANGLE/renderer/gl/ProgramGL.h"
#include "libANGLE/renderer/gl/ProgramPipelineGL.h"
#include "libANGLE/renderer/gl/QueryGL.h"
#include "libANGLE/renderer/gl/RenderbufferGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/SamplerGL.h"
#include "libANGLE/renderer/gl/ShaderGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/SyncGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "libANGLE/renderer/gl/TransformFeedbackGL.h"
#include "libANGLE/renderer/gl/VertexArrayGL.h"

namespace rx
{

ContextGL::ContextGL(const gl::ContextState &state, RendererGL *renderer)
    : ContextImpl(state), mRenderer(renderer)
{
}

ContextGL::~ContextGL()
{
}

gl::Error ContextGL::initialize()
{
    return gl::NoError();
}

CompilerImpl *ContextGL::createCompiler()
{
    return new CompilerGL(getFunctions());
}

ShaderImpl *ContextGL::createShader(const gl::ShaderState &data)
{
    return new ShaderGL(data, getFunctions(), getWorkaroundsGL(),
                        getExtensions().webglCompatibility,
                        mRenderer->getMultiviewImplementationType());
}

ProgramImpl *ContextGL::createProgram(const gl::ProgramState &data)
{
    return new ProgramGL(data, getFunctions(), getWorkaroundsGL(), getStateManager(),
                         getExtensions().pathRendering);
}

FramebufferImpl *ContextGL::createFramebuffer(const gl::FramebufferState &data)
{
    return new FramebufferGL(data, getFunctions(), getStateManager(), getWorkaroundsGL(),
                             mRenderer->getBlitter(), mRenderer->getMultiviewClearer(), false);
}

TextureImpl *ContextGL::createTexture(const gl::TextureState &state)
{
    return new TextureGL(state, getFunctions(), getWorkaroundsGL(), getStateManager(),
                         mRenderer->getBlitter());
}

RenderbufferImpl *ContextGL::createRenderbuffer()
{
    return new RenderbufferGL(getFunctions(), getWorkaroundsGL(), getStateManager(),
                              getNativeTextureCaps());
}

BufferImpl *ContextGL::createBuffer(const gl::BufferState &state)
{
    return new BufferGL(state, getFunctions(), getStateManager());
}

VertexArrayImpl *ContextGL::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArrayGL(data, getFunctions(), getStateManager());
}

QueryImpl *ContextGL::createQuery(GLenum type)
{
    switch (type)
    {
        case GL_COMMANDS_COMPLETED_CHROMIUM:
            return new SyncQueryGL(type, getFunctions(), getStateManager());

        default:
            return new StandardQueryGL(type, getFunctions(), getStateManager());
    }
}

FenceNVImpl *ContextGL::createFenceNV()
{
    return new FenceNVGL(getFunctions());
}

SyncImpl *ContextGL::createSync()
{
    return new SyncGL(getFunctions());
}

TransformFeedbackImpl *ContextGL::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedbackGL(state, getFunctions(), getStateManager());
}

SamplerImpl *ContextGL::createSampler(const gl::SamplerState &state)
{
    return new SamplerGL(state, getFunctions(), getStateManager());
}

ProgramPipelineImpl *ContextGL::createProgramPipeline(const gl::ProgramPipelineState &data)
{
    return new ProgramPipelineGL(data, getFunctions());
}

std::vector<PathImpl *> ContextGL::createPaths(GLsizei range)
{
    const FunctionsGL *funcs = getFunctions();

    std::vector<PathImpl *> ret;
    ret.reserve(range);

    const GLuint first = funcs->genPathsNV(range);
    if (first == 0)
        return ret;

    for (GLsizei i = 0; i < range; ++i)
    {
        const auto id = first + i;
        ret.push_back(new PathGL(funcs, id));
    }

    return ret;
}

gl::Error ContextGL::flush(const gl::Context *context)
{
    return mRenderer->flush();
}

gl::Error ContextGL::finish(const gl::Context *context)
{
    return mRenderer->finish();
}

gl::Error ContextGL::drawArrays(const gl::Context *context, GLenum mode, GLint first, GLsizei count)
{
    return mRenderer->drawArrays(context, mode, first, count);
}

gl::Error ContextGL::drawArraysInstanced(const gl::Context *context,
                                         GLenum mode,
                                         GLint first,
                                         GLsizei count,
                                         GLsizei instanceCount)
{
    return mRenderer->drawArraysInstanced(context, mode, first, count, instanceCount);
}

gl::Error ContextGL::drawElements(const gl::Context *context,
                                  GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void *indices)
{
    return mRenderer->drawElements(context, mode, count, type, indices);
}

gl::Error ContextGL::drawElementsInstanced(const gl::Context *context,
                                           GLenum mode,
                                           GLsizei count,
                                           GLenum type,
                                           const void *indices,
                                           GLsizei instances)
{
    return mRenderer->drawElementsInstanced(context, mode, count, type, indices, instances);
}

gl::Error ContextGL::drawRangeElements(const gl::Context *context,
                                       GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void *indices)
{
    return mRenderer->drawRangeElements(context, mode, start, end, count, type, indices);
}

gl::Error ContextGL::drawArraysIndirect(const gl::Context *context,
                                        GLenum mode,
                                        const void *indirect)
{
    return mRenderer->drawArraysIndirect(context, mode, indirect);
}

gl::Error ContextGL::drawElementsIndirect(const gl::Context *context,
                                          GLenum mode,
                                          GLenum type,
                                          const void *indirect)
{
    return mRenderer->drawElementsIndirect(context, mode, type, indirect);
}

void ContextGL::stencilFillPath(const gl::Path *path, GLenum fillMode, GLuint mask)
{
    mRenderer->stencilFillPath(mState, path, fillMode, mask);
}

void ContextGL::stencilStrokePath(const gl::Path *path, GLint reference, GLuint mask)
{
    mRenderer->stencilStrokePath(mState, path, reference, mask);
}

void ContextGL::coverFillPath(const gl::Path *path, GLenum coverMode)
{
    mRenderer->coverFillPath(mState, path, coverMode);
}

void ContextGL::coverStrokePath(const gl::Path *path, GLenum coverMode)
{
    mRenderer->coverStrokePath(mState, path, coverMode);
}

void ContextGL::stencilThenCoverFillPath(const gl::Path *path,
                                         GLenum fillMode,
                                         GLuint mask,
                                         GLenum coverMode)
{
    mRenderer->stencilThenCoverFillPath(mState, path, fillMode, mask, coverMode);
}

void ContextGL::stencilThenCoverStrokePath(const gl::Path *path,
                                           GLint reference,
                                           GLuint mask,
                                           GLenum coverMode)
{
    mRenderer->stencilThenCoverStrokePath(mState, path, reference, mask, coverMode);
}

void ContextGL::coverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                       GLenum coverMode,
                                       GLenum transformType,
                                       const GLfloat *transformValues)
{
    mRenderer->coverFillPathInstanced(mState, paths, coverMode, transformType, transformValues);
}

void ContextGL::coverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                         GLenum coverMode,
                                         GLenum transformType,
                                         const GLfloat *transformValues)
{
    mRenderer->coverStrokePathInstanced(mState, paths, coverMode, transformType, transformValues);
}

void ContextGL::stencilFillPathInstanced(const std::vector<gl::Path *> &paths,
                                         GLenum fillMode,
                                         GLuint mask,
                                         GLenum transformType,
                                         const GLfloat *transformValues)
{
    mRenderer->stencilFillPathInstanced(mState, paths, fillMode, mask, transformType,
                                        transformValues);
}

void ContextGL::stencilStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                           GLint reference,
                                           GLuint mask,
                                           GLenum transformType,
                                           const GLfloat *transformValues)
{
    mRenderer->stencilStrokePathInstanced(mState, paths, reference, mask, transformType,
                                          transformValues);
}

void ContextGL::stencilThenCoverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                                  GLenum coverMode,
                                                  GLenum fillMode,
                                                  GLuint mask,
                                                  GLenum transformType,
                                                  const GLfloat *transformValues)
{
    mRenderer->stencilThenCoverFillPathInstanced(mState, paths, coverMode, fillMode, mask,
                                                 transformType, transformValues);
}

void ContextGL::stencilThenCoverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                                    GLenum coverMode,
                                                    GLint reference,
                                                    GLuint mask,
                                                    GLenum transformType,
                                                    const GLfloat *transformValues)
{
    mRenderer->stencilThenCoverStrokePathInstanced(mState, paths, coverMode, reference, mask,
                                                   transformType, transformValues);
}

GLenum ContextGL::getResetStatus()
{
    return mRenderer->getResetStatus();
}

std::string ContextGL::getVendorString() const
{
    return mRenderer->getVendorString();
}

std::string ContextGL::getRendererDescription() const
{
    return mRenderer->getRendererDescription();
}

void ContextGL::insertEventMarker(GLsizei length, const char *marker)
{
    mRenderer->insertEventMarker(length, marker);
}

void ContextGL::pushGroupMarker(GLsizei length, const char *marker)
{
    mRenderer->pushGroupMarker(length, marker);
}

void ContextGL::popGroupMarker()
{
    mRenderer->popGroupMarker();
}

void ContextGL::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message)
{
    mRenderer->pushDebugGroup(source, id, length, message);
}

void ContextGL::popDebugGroup()
{
    mRenderer->popDebugGroup();
}

void ContextGL::syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits)
{
    mRenderer->getStateManager()->syncState(context, dirtyBits);
}

GLint ContextGL::getGPUDisjoint()
{
    return mRenderer->getGPUDisjoint();
}

GLint64 ContextGL::getTimestamp()
{
    return mRenderer->getTimestamp();
}

void ContextGL::onMakeCurrent(const gl::Context *context)
{
    // Queries need to be paused/resumed on context switches
    ANGLE_SWALLOW_ERR(mRenderer->getStateManager()->onMakeCurrent(context));
}

const gl::Caps &ContextGL::getNativeCaps() const
{
    return mRenderer->getNativeCaps();
}

const gl::TextureCapsMap &ContextGL::getNativeTextureCaps() const
{
    return mRenderer->getNativeTextureCaps();
}

const gl::Extensions &ContextGL::getNativeExtensions() const
{
    return mRenderer->getNativeExtensions();
}

const gl::Limitations &ContextGL::getNativeLimitations() const
{
    return mRenderer->getNativeLimitations();
}

void ContextGL::applyNativeWorkarounds(gl::Workarounds *workarounds) const
{
    return mRenderer->applyNativeWorkarounds(workarounds);
}

const FunctionsGL *ContextGL::getFunctions() const
{
    return mRenderer->getFunctions();
}

StateManagerGL *ContextGL::getStateManager()
{
    return mRenderer->getStateManager();
}

const WorkaroundsGL &ContextGL::getWorkaroundsGL() const
{
    return mRenderer->getWorkarounds();
}

gl::Error ContextGL::dispatchCompute(const gl::Context *context,
                                     GLuint numGroupsX,
                                     GLuint numGroupsY,
                                     GLuint numGroupsZ)
{
    return mRenderer->dispatchCompute(context, numGroupsX, numGroupsY, numGroupsZ);
}

}  // namespace rx
