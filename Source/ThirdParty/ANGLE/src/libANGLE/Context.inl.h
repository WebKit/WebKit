//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.inl.h: Defines inline functions of gl::Context class
// Has to be included after libANGLE/Context.h when using one
// of the defined functions

#ifndef LIBANGLE_CONTEXT_INL_H_
#define LIBANGLE_CONTEXT_INL_H_

#include "libANGLE/Context.h"
#include "libANGLE/GLES1Renderer.h"
#include "libANGLE/renderer/ContextImpl.h"

#define ANGLE_HANDLE_ERR(X) \
    (void)(X);              \
    return;
#define ANGLE_CONTEXT_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, static_cast<void>(0), ANGLE_HANDLE_ERR)

namespace gl
{
constexpr angle::PackedEnumMap<PrimitiveMode, GLsizei> kMinimumPrimitiveCounts = {{
    {PrimitiveMode::Points, 1},
    {PrimitiveMode::Lines, 2},
    {PrimitiveMode::LineLoop, 2},
    {PrimitiveMode::LineStrip, 2},
    {PrimitiveMode::Triangles, 3},
    {PrimitiveMode::TriangleStrip, 3},
    {PrimitiveMode::TriangleFan, 3},
    {PrimitiveMode::LinesAdjacency, 2},
    {PrimitiveMode::LineStripAdjacency, 2},
    {PrimitiveMode::TrianglesAdjacency, 3},
    {PrimitiveMode::TriangleStripAdjacency, 3},
}};

// All bits except |DIRTY_BIT_READ_FRAMEBUFFER_BINDING| because |mDrawDirtyObjects| does not contain
// |DIRTY_OBJECT_READ_FRAMEBUFFER|, to avoid synchronizing with invalid read framebuffer state.
constexpr state::DirtyBits kDrawDirtyBits =
    ~state::DirtyBits{state::DIRTY_BIT_READ_FRAMEBUFFER_BINDING};
constexpr state::ExtendedDirtyBits kDrawExtendedDirtyBits = state::ExtendedDirtyBits().set();

ANGLE_INLINE void MarkTransformFeedbackBufferUsage(const Context *context,
                                                   GLsizei count,
                                                   GLsizei instanceCount)
{
    if (context->getStateCache().isTransformFeedbackActiveUnpaused())
    {
        TransformFeedback *transformFeedback = context->getState().getCurrentTransformFeedback();
        transformFeedback->onVerticesDrawn(context, count, instanceCount);
    }
}

ANGLE_INLINE void MarkShaderStorageUsage(const Context *context)
{
    for (size_t index : context->getStateCache().getActiveShaderStorageBufferIndices())
    {
        Buffer *buffer = context->getState().getIndexedShaderStorageBuffer(index).get();
        if (buffer)
        {
            buffer->onDataChanged(context);
        }
    }

    for (size_t index : context->getStateCache().getActiveImageUnitIndices())
    {
        const ImageUnit &imageUnit = context->getState().getImageUnit(index);
        const Texture *texture     = imageUnit.texture.get();
        if (texture)
        {
            texture->onStateChange(angle::SubjectMessage::ContentsChanged);
        }
    }
}

// Return true if the draw is a no-op, else return false.
//  If there is no active program for the vertex or fragment shader stages, the results of vertex
//  and fragment shader execution will respectively be undefined. However, this is not
//  an error. ANGLE will treat this as a no-op.
//  A no-op draw occurs if the count of vertices is less than the minimum required to
//  have a valid primitive for this mode (0 for points, 0-1 for lines, 0-2 for tris).
ANGLE_INLINE bool Context::noopDrawProgram() const
{
    // Make sure any pending link is done before checking whether draw is allowed.
    mState.ensureNoPendingLink(this);

    // No-op when there is no active vertex shader
    return !mStateCache.getCanDraw();
}

ANGLE_INLINE bool Context::noopDraw(PrimitiveMode mode, GLsizei count) const
{
    if (ANGLE_UNLIKELY(count < kMinimumPrimitiveCounts[mode]))
    {
        return true;
    }

    return noopDrawProgram();
}

ANGLE_INLINE bool Context::noopDrawInstanced(PrimitiveMode mode,
                                             GLsizei count,
                                             GLsizei instanceCount) const
{
    if (ANGLE_UNLIKELY(instanceCount < 1))
    {
        return true;
    }

    return noopDraw(mode, count);
}

ANGLE_INLINE bool Context::noopMultiDraw(GLsizei drawcount) const
{
    if (ANGLE_UNLIKELY(drawcount < 1))
    {
        return true;
    }

    return noopDrawProgram();
}

ANGLE_INLINE angle::Result Context::syncDirtyBits(const state::DirtyBits bitMask,
                                                  const state::ExtendedDirtyBits extendedBitMask,
                                                  Command command)
{
    const state::DirtyBits dirtyBits = (mState.getDirtyBits() & bitMask);
    const state::ExtendedDirtyBits extendedDirtyBits =
        (mState.getExtendedDirtyBits() & extendedBitMask);
    ANGLE_TRY(mImplementation->syncState(this, dirtyBits, bitMask, extendedDirtyBits,
                                         extendedBitMask, command));
    mState.clearDirtyBits(dirtyBits);
    mState.clearExtendedDirtyBits(extendedDirtyBits);
    return angle::Result::Continue;
}

ANGLE_INLINE angle::Result Context::syncDirtyObjects(const state::DirtyObjects &objectMask,
                                                     Command command)
{
    return mState.syncDirtyObjects(this, objectMask, command);
}

ANGLE_INLINE angle::Result Context::prepareForDraw(PrimitiveMode mode)
{
    if (mGLES1Renderer)
    {
        ANGLE_TRY(mGLES1Renderer->prepareForDraw(mode, this, &mState, getMutableGLES1State()));
    }

    ANGLE_TRY(syncDirtyObjects(mDrawDirtyObjects, Command::Draw));
    ASSERT(!isRobustResourceInitEnabled() ||
           !mState.getDrawFramebuffer()->hasResourceThatNeedsInit());
    return syncDirtyBits(kDrawDirtyBits, kDrawExtendedDirtyBits, Command::Draw);
}

ANGLE_INLINE void Context::drawArrays(PrimitiveMode mode, GLint first, GLsizei count)
{
    // No-op if count draws no primitives for given mode
    if (noopDraw(mode, count))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawArrays(this, mode, first, count));
    MarkTransformFeedbackBufferUsage(this, count, 1);
}

ANGLE_INLINE void Context::drawElements(PrimitiveMode mode,
                                        GLsizei count,
                                        DrawElementsType type,
                                        const void *indices)
{
    // No-op if count draws no primitives for given mode
    if (noopDraw(mode, count))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawElements(this, mode, count, type, indices));
}

ANGLE_INLINE void Context::bindBuffer(BufferBinding target, BufferID buffer)
{
    Buffer *bufferObject =
        mState.mBufferManager->checkBufferAllocation(mImplementation.get(), buffer);

    // If there is a shared context, buffer may have been modified by other context. bindBuffer
    // supposedly should pick up the changes other contexts have made and update current vertex
    // array.
    if (bufferObject != nullptr && isSharedContext())
    {
        VertexArrayBufferBindingMask bindingMask = bufferObject->getVertexArrayBinding(this);
        if (bindingMask.any())
        {
            ASSERT(mState.mVertexArray != nullptr);
            // Update vertex array only if buffer is attached to current vertex array.
            mState.mVertexArray->onSharedBufferBind(this, bufferObject, bindingMask);
            mState.setObjectDirty(GL_VERTEX_ARRAY);
            mPrivateStateCache.onVertexArrayBufferContentsChange();
        }
    }

    // Early return if rebinding the same buffer
    if (bufferObject == mState.getTargetBuffer(target))
    {
        return;
    }

    mState.setBufferBinding(this, target, bufferObject);
    mPrivateStateCache.onBufferBindingChange();

    if (bufferObject && isWebGL())
    {
        bufferObject->onBind(this, target);
    }
}

ANGLE_INLINE void Context::uniform1f(UniformLocation location, GLfloat x)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform1fv(location, 1, &x);
}

ANGLE_INLINE void Context::uniform1fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform1fv(location, count, v);
}

ANGLE_INLINE void Context::setUniform1iImpl(Program *program,
                                            UniformLocation location,
                                            GLsizei count,
                                            const GLint *v)
{
    program->getExecutable().setUniform1iv(this, location, count, v);
}

ANGLE_INLINE void Context::uniform1i(UniformLocation location, GLint x)
{
    Program *program = getActiveLinkedProgram();
    setUniform1iImpl(program, location, 1, &x);
}

ANGLE_INLINE void Context::uniform1iv(UniformLocation location, GLsizei count, const GLint *v)
{
    Program *program = getActiveLinkedProgram();
    setUniform1iImpl(program, location, count, v);
}

ANGLE_INLINE void Context::uniform2f(UniformLocation location, GLfloat x, GLfloat y)
{
    GLfloat xy[2]    = {x, y};
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform2fv(location, 1, xy);
}

ANGLE_INLINE void Context::uniform2fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform2fv(location, count, v);
}

ANGLE_INLINE void Context::uniform2i(UniformLocation location, GLint x, GLint y)
{
    GLint xy[2]      = {x, y};
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform2iv(location, 1, xy);
}

ANGLE_INLINE void Context::uniform2iv(UniformLocation location, GLsizei count, const GLint *v)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform2iv(location, count, v);
}

ANGLE_INLINE void Context::uniform3f(UniformLocation location, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat xyz[3]   = {x, y, z};
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform3fv(location, 1, xyz);
}

ANGLE_INLINE void Context::uniform3fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform3fv(location, count, v);
}

ANGLE_INLINE void Context::uniform3i(UniformLocation location, GLint x, GLint y, GLint z)
{
    GLint xyz[3]     = {x, y, z};
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform3iv(location, 1, xyz);
}

ANGLE_INLINE void Context::uniform3iv(UniformLocation location, GLsizei count, const GLint *v)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform3iv(location, count, v);
}

ANGLE_INLINE void Context::uniform4f(UniformLocation location,
                                     GLfloat x,
                                     GLfloat y,
                                     GLfloat z,
                                     GLfloat w)
{
    GLfloat xyzw[4]  = {x, y, z, w};
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform4fv(location, 1, xyzw);
}

ANGLE_INLINE void Context::uniform4fv(UniformLocation location, GLsizei count, const GLfloat *v)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform4fv(location, count, v);
}

ANGLE_INLINE void Context::uniform4i(UniformLocation location, GLint x, GLint y, GLint z, GLint w)
{
    GLint xyzw[4]    = {x, y, z, w};
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform4iv(location, 1, xyzw);
}

ANGLE_INLINE void Context::uniform4iv(UniformLocation location, GLsizei count, const GLint *v)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform4iv(location, count, v);
}

ANGLE_INLINE void Context::uniform1ui(UniformLocation location, GLuint v0)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform1uiv(location, 1, &v0);
}

ANGLE_INLINE void Context::uniform2ui(UniformLocation location, GLuint v0, GLuint v1)
{
    Program *program  = getActiveLinkedProgram();
    const GLuint xy[] = {v0, v1};
    program->getExecutable().setUniform2uiv(location, 1, xy);
}

ANGLE_INLINE void Context::uniform3ui(UniformLocation location, GLuint v0, GLuint v1, GLuint v2)
{
    Program *program   = getActiveLinkedProgram();
    const GLuint xyz[] = {v0, v1, v2};
    program->getExecutable().setUniform3uiv(location, 1, xyz);
}

ANGLE_INLINE void Context::uniform4ui(UniformLocation location,
                                      GLuint v0,
                                      GLuint v1,
                                      GLuint v2,
                                      GLuint v3)
{
    Program *program    = getActiveLinkedProgram();
    const GLuint xyzw[] = {v0, v1, v2, v3};
    program->getExecutable().setUniform4uiv(location, 1, xyzw);
}

ANGLE_INLINE void Context::uniform1uiv(UniformLocation location, GLsizei count, const GLuint *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform1uiv(location, count, value);
}

ANGLE_INLINE void Context::uniform2uiv(UniformLocation location, GLsizei count, const GLuint *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform2uiv(location, count, value);
}

ANGLE_INLINE void Context::uniform3uiv(UniformLocation location, GLsizei count, const GLuint *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform3uiv(location, count, value);
}

ANGLE_INLINE void Context::uniform4uiv(UniformLocation location, GLsizei count, const GLuint *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniform4uiv(location, count, value);
}

ANGLE_INLINE void Context::uniformMatrix2x3fv(UniformLocation location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniformMatrix2x3fv(location, count, transpose, value);
}

ANGLE_INLINE void Context::uniformMatrix3x2fv(UniformLocation location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniformMatrix3x2fv(location, count, transpose, value);
}

ANGLE_INLINE void Context::uniformMatrix2x4fv(UniformLocation location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniformMatrix2x4fv(location, count, transpose, value);
}

ANGLE_INLINE void Context::uniformMatrix4x2fv(UniformLocation location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniformMatrix4x2fv(location, count, transpose, value);
}

ANGLE_INLINE void Context::uniformMatrix3x4fv(UniformLocation location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniformMatrix3x4fv(location, count, transpose, value);
}

ANGLE_INLINE void Context::uniformMatrix4x3fv(UniformLocation location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    Program *program = getActiveLinkedProgram();
    program->getExecutable().setUniformMatrix4x3fv(location, count, transpose, value);
}

ANGLE_INLINE void Context::vertexAttribPointer(GLuint index,
                                               GLint size,
                                               VertexAttribType type,
                                               GLboolean normalized,
                                               GLsizei stride,
                                               const void *ptr)
{
    bool vertexAttribDirty = false;
    mState.setVertexAttribPointer(this, index, mState.getTargetBuffer(BufferBinding::Array), size,
                                  type, normalized != GL_FALSE, stride, ptr, &vertexAttribDirty);
    if (vertexAttribDirty)
    {
        mPrivateStateCache.onVertexArrayStateChange();
    }
}

}  // namespace gl

#endif  // LIBANGLE_CONTEXT_INL_H_
