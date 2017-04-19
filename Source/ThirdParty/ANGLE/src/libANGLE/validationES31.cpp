//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES31.cpp: Validation functions for OpenGL ES 3.1 entry point parameters

#include "libANGLE/validationES31.h"

#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/validationES.h"
#include "libANGLE/validationES3.h"
#include "libANGLE/VertexArray.h"

#include "common/utilities.h"

using namespace angle;

namespace gl
{

namespace
{

bool ValidateNamedProgramInterface(GLenum programInterface)
{
    switch (programInterface)
    {
        case GL_UNIFORM:
        case GL_UNIFORM_BLOCK:
        case GL_PROGRAM_INPUT:
        case GL_PROGRAM_OUTPUT:
        case GL_TRANSFORM_FEEDBACK_VARYING:
        case GL_BUFFER_VARIABLE:
        case GL_SHADER_STORAGE_BLOCK:
            return true;
        default:
            return false;
    }
}

bool ValidateProgramResourceIndex(const Program *programObject,
                                  GLenum programInterface,
                                  GLuint index)
{
    switch (programInterface)
    {
        case GL_PROGRAM_INPUT:
            return (index < static_cast<GLuint>(programObject->getActiveAttributeCount()));

        case GL_PROGRAM_OUTPUT:
            return (index < static_cast<GLuint>(programObject->getOutputResourceCount()));

        // TODO(Jie): more interfaces.
        case GL_UNIFORM:
        case GL_UNIFORM_BLOCK:
        case GL_TRANSFORM_FEEDBACK_VARYING:
        case GL_BUFFER_VARIABLE:
        case GL_SHADER_STORAGE_BLOCK:
            UNIMPLEMENTED();
            return false;

        default:
            UNREACHABLE();
            return false;
    }
}

}  // anonymous namespace

bool ValidateGetBooleani_v(Context *context, GLenum target, GLuint index, GLboolean *data)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1"));
        return false;
    }

    if (!ValidateIndexedStateQuery(context, target, index, nullptr))
    {
        return false;
    }

    return true;
}

bool ValidateGetBooleani_vRobustANGLE(Context *context,
                                      GLenum target,
                                      GLuint index,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLboolean *data)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1"));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateIndexedStateQuery(context, target, index, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateDrawIndirectBase(Context *context, GLenum mode, const GLvoid *indirect)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1"));
        return false;
    }

    // Here the third parameter 1 is only to pass the count validation.
    if (!ValidateDrawBase(context, mode, 1))
    {
        return false;
    }

    const State &state = context->getGLState();

    // An INVALID_OPERATION error is generated if zero is bound to VERTEX_ARRAY_BINDING,
    // DRAW_INDIRECT_BUFFER or to any enabled vertex array.
    if (!state.getVertexArrayId())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "zero is bound to VERTEX_ARRAY_BINDING"));
        return false;
    }

    gl::Buffer *drawIndirectBuffer = state.getDrawIndirectBuffer();
    if (!drawIndirectBuffer)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "zero is bound to DRAW_INDIRECT_BUFFER"));
        return false;
    }

    // An INVALID_VALUE error is generated if indirect is not a multiple of the size, in basic
    // machine units, of uint.
    GLint64 offset = reinterpret_cast<GLint64>(indirect);
    if ((static_cast<GLuint>(offset) % sizeof(GLuint)) != 0)
    {
        context->handleError(
            Error(GL_INVALID_VALUE,
                  "indirect is not a multiple of the size, in basic machine units, of uint"));
        return false;
    }

    return true;
}

bool ValidateDrawArraysIndirect(Context *context, GLenum mode, const GLvoid *indirect)
{
    const State &state                          = context->getGLState();
    gl::TransformFeedback *curTransformFeedback = state.getCurrentTransformFeedback();
    if (curTransformFeedback && curTransformFeedback->isActive() &&
        !curTransformFeedback->isPaused())
    {
        // An INVALID_OPERATION error is generated if transform feedback is active and not paused.
        context->handleError(
            Error(GL_INVALID_OPERATION, "transform feedback is active and not paused."));
        return false;
    }

    if (!ValidateDrawIndirectBase(context, mode, indirect))
        return false;

    gl::Buffer *drawIndirectBuffer = state.getDrawIndirectBuffer();
    CheckedNumeric<size_t> checkedOffset(reinterpret_cast<size_t>(indirect));
    // In OpenGL ES3.1 spec, session 10.5, it defines the struct of DrawArraysIndirectCommand
    // which's size is 4 * sizeof(uint).
    auto checkedSum = checkedOffset + 4 * sizeof(GLuint);
    if (!checkedSum.IsValid() ||
        checkedSum.ValueOrDie() > static_cast<size_t>(drawIndirectBuffer->getSize()))
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "the  command  would source data beyond the end of the buffer object."));
        return false;
    }

    return true;
}

bool ValidateDrawElementsIndirect(Context *context,
                                  GLenum mode,
                                  GLenum type,
                                  const GLvoid *indirect)
{
    if (!ValidateDrawElementsBase(context, type))
        return false;

    const State &state             = context->getGLState();
    const VertexArray *vao         = state.getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer().get();
    if (!elementArrayBuffer)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "zero is bound to ELEMENT_ARRAY_BUFFER"));
        return false;
    }

    if (!ValidateDrawIndirectBase(context, mode, indirect))
        return false;

    gl::Buffer *drawIndirectBuffer = state.getDrawIndirectBuffer();
    CheckedNumeric<size_t> checkedOffset(reinterpret_cast<size_t>(indirect));
    // In OpenGL ES3.1 spec, session 10.5, it defines the struct of DrawElementsIndirectCommand
    // which's size is 5 * sizeof(uint).
    auto checkedSum = checkedOffset + 5 * sizeof(GLuint);
    if (!checkedSum.IsValid() ||
        checkedSum.ValueOrDie() > static_cast<size_t>(drawIndirectBuffer->getSize()))
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "the  command  would source data beyond the end of the buffer object."));
        return false;
    }

    return true;
}

bool ValidateGetTexLevelParameterBase(Context *context,
                                      GLenum target,
                                      GLint level,
                                      GLenum pname,
                                      GLsizei *length)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1"));
        return false;
    }

    if (length)
    {
        *length = 0;
    }

    if (!ValidTexLevelDestinationTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid texture target"));
        return false;
    }

    if (context->getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target) ==
        nullptr)
    {
        context->handleError(Error(GL_INVALID_ENUM, "No texture bound."));
        return false;
    }

    if (!ValidMipLevel(context, target, level))
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    switch (pname)
    {
        case GL_TEXTURE_RED_TYPE:
        case GL_TEXTURE_GREEN_TYPE:
        case GL_TEXTURE_BLUE_TYPE:
        case GL_TEXTURE_ALPHA_TYPE:
        case GL_TEXTURE_DEPTH_TYPE:
            break;
        case GL_TEXTURE_RED_SIZE:
        case GL_TEXTURE_GREEN_SIZE:
        case GL_TEXTURE_BLUE_SIZE:
        case GL_TEXTURE_ALPHA_SIZE:
        case GL_TEXTURE_DEPTH_SIZE:
        case GL_TEXTURE_STENCIL_SIZE:
        case GL_TEXTURE_SHARED_SIZE:
            break;
        case GL_TEXTURE_INTERNAL_FORMAT:
        case GL_TEXTURE_WIDTH:
        case GL_TEXTURE_HEIGHT:
        case GL_TEXTURE_DEPTH:
            break;
        case GL_TEXTURE_SAMPLES:
        case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
            break;
        case GL_TEXTURE_COMPRESSED:
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    if (length)
    {
        *length = 1;
    }
    return true;
}

bool ValidateGetTexLevelParameterfv(Context *context,
                                    GLenum target,
                                    GLint level,
                                    GLenum pname,
                                    GLfloat *params)
{
    return ValidateGetTexLevelParameterBase(context, target, level, pname, nullptr);
}

bool ValidateGetTexLevelParameteriv(Context *context,
                                    GLenum target,
                                    GLint level,
                                    GLenum pname,
                                    GLint *params)
{
    return ValidateGetTexLevelParameterBase(context, target, level, pname, nullptr);
}

bool ValidateTexStorage2DMultiSample(Context *context,
                                     GLenum target,
                                     GLsizei samples,
                                     GLint internalFormat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLboolean fixedSampleLocations)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    if (target != GL_TEXTURE_2D_MULTISAMPLE)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Target must be TEXTURE_2D_MULTISAMPLE."));
        return false;
    }

    if (width < 1 || height < 1)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Width and height must be positive."));
        return false;
    }

    const Caps &caps = context->getCaps();
    if (static_cast<GLuint>(width) > caps.max2DTextureSize ||
        static_cast<GLuint>(height) > caps.max2DTextureSize)
    {
        context->handleError(
            Error(GL_INVALID_VALUE,
                  "Width and height must be less than or equal to GL_MAX_TEXTURE_SIZE."));
        return false;
    }

    if (samples == 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Samples may not be zero."));
        return false;
    }

    const TextureCaps &formatCaps = context->getTextureCaps().get(internalFormat);
    if (!formatCaps.renderable)
    {
        context->handleError(
            Error(GL_INVALID_ENUM,
                  "SizedInternalformat must be color-renderable, depth-renderable, "
                  "or stencil-renderable."));
        return false;
    }

    // The ES3.1 spec(section 8.8) states that an INVALID_ENUM error is generated if internalformat
    // is one of the unsized base internalformats listed in table 8.11.
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalFormat);
    if (formatInfo.pixelBytes == 0)
    {
        context->handleError(
            Error(GL_INVALID_ENUM,
                  "Internalformat is one of the unsupported unsized base internalformats."));
        return false;
    }

    if (static_cast<GLuint>(samples) > formatCaps.getMaxSamples())
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "Samples must not be greater than maximum supported value for the format."));
        return false;
    }

    Texture *texture = context->getTargetTexture(target);
    if (!texture || texture->id() == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Zero is bound to target."));
        return false;
    }

    if (texture->getImmutableFormat())
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "The value of TEXTURE_IMMUTABLE_FORMAT for the texture "
                  "currently bound to target on the active texture unit is true."));
        return false;
    }

    return true;
}

bool ValidateGetMultisamplefv(Context *context, GLenum pname, GLuint index, GLfloat *val)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    if (pname != GL_SAMPLE_POSITION)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Pname must be SAMPLE_POSITION."));
        return false;
    }

    GLint maxSamples = context->getCaps().maxSamples;
    if (index >= static_cast<GLuint>(maxSamples))
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Index must be less than the value of SAMPLES."));
        return false;
    }

    return true;
}

bool ValidationFramebufferParameteri(Context *context, GLenum target, GLenum pname, GLint param)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    if (!ValidFramebufferTarget(target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid framebuffer target."));
        return false;
    }

    switch (pname)
    {
        case GL_FRAMEBUFFER_DEFAULT_WIDTH:
        {
            GLint maxWidth = context->getCaps().maxFramebufferWidth;
            if (param < 0 || param > maxWidth)
            {
                context->handleError(
                    Error(GL_INVALID_VALUE,
                          "Params less than 0 or greater than GL_MAX_FRAMEBUFFER_WIDTH."));
                return false;
            }
            break;
        }
        case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
        {
            GLint maxHeight = context->getCaps().maxFramebufferHeight;
            if (param < 0 || param > maxHeight)
            {
                context->handleError(
                    Error(GL_INVALID_VALUE,
                          "Params less than 0 or greater than GL_MAX_FRAMEBUFFER_HEIGHT."));
                return false;
            }
            break;
        }
        case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
        {
            GLint maxSamples = context->getCaps().maxFramebufferSamples;
            if (param < 0 || param > maxSamples)
            {
                context->handleError(
                    Error(GL_INVALID_VALUE,
                          "Params less than 0 or greater than GL_MAX_FRAMEBUFFER_SAMPLES."));
                return false;
            }
            break;
        }
        case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:
        {
            break;
        }
        default:
        {
            context->handleError(Error(GL_INVALID_ENUM, "Invalid pname: 0x%X", pname));
            return false;
        }
    }

    const Framebuffer *framebuffer = context->getGLState().getTargetFramebuffer(target);
    ASSERT(framebuffer);
    if (framebuffer->id() == 0)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Default framebuffer is bound to target."));
        return false;
    }
    return true;
}

bool ValidationGetFramebufferParameteri(Context *context,
                                        GLenum target,
                                        GLenum pname,
                                        GLint *params)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    if (!ValidFramebufferTarget(target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid framebuffer target."));
        return false;
    }

    switch (pname)
    {
        case GL_FRAMEBUFFER_DEFAULT_WIDTH:
        case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
        case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
        case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid pname: 0x%X", pname));
            return false;
    }

    const Framebuffer *framebuffer = context->getGLState().getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->id() == 0)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Default framebuffer is bound to target."));
        return false;
    }
    return true;
}

bool ValidateGetProgramResourceIndex(Context *context,
                                     GLuint program,
                                     GLenum programInterface,
                                     const GLchar *name)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES 3.1."));
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }

    if (!ValidateNamedProgramInterface(programInterface))
    {
        context->handleError(
            Error(GL_INVALID_ENUM, "Invalid program interface: 0x%X", programInterface));
        return false;
    }

    return true;
}

bool ValidateBindVertexBuffer(ValidationContext *context,
                              GLuint bindingIndex,
                              GLuint buffer,
                              GLintptr offset,
                              GLsizei stride)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    if (!context->isBufferGenerated(buffer))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Buffer is not generated."));
        return false;
    }

    const Caps &caps = context->getCaps();
    if (bindingIndex >= caps.maxVertexAttribBindings)
    {
        context->handleError(Error(
            GL_INVALID_VALUE, "bindingindex must be smaller than MAX_VERTEX_ATTRIB_BINDINGS."));
        return false;
    }

    if (offset < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "offset cannot be negative."));
        return false;
    }

    if (stride < 0 || stride > caps.maxVertexAttribStride)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "stride must be between 0 and MAX_VERTEX_ATTRIB_STRIDE."));
        return false;
    }

    // [OpenGL ES 3.1] Section 10.3.1 page 244:
    // An INVALID_OPERATION error is generated if the default vertex array object is bound.
    if (context->getGLState().getVertexArrayId() == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Default vertex array buffer is bound."));
        return false;
    }

    return true;
}

bool ValidateVertexBindingDivisor(ValidationContext *context, GLuint bindingIndex, GLuint divisor)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    const Caps &caps = context->getCaps();
    if (bindingIndex >= caps.maxVertexAttribBindings)
    {
        context->handleError(Error(
            GL_INVALID_VALUE, "bindingindex must be smaller than MAX_VERTEX_ATTRIB_BINDINGS."));
        return false;
    }

    // [OpenGL ES 3.1] Section 10.3.1 page 243:
    // An INVALID_OPERATION error is generated if the default vertex array object is bound.
    if (context->getGLState().getVertexArrayId() == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Default vertex array object is bound."));
        return false;
    }

    return true;
}

bool ValidateVertexAttribFormat(ValidationContext *context,
                                GLuint attribIndex,
                                GLint size,
                                GLenum type,
                                GLuint relativeOffset,
                                GLboolean pureInteger)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    const Caps &caps = context->getCaps();
    if (relativeOffset > static_cast<GLuint>(caps.maxVertexAttribRelativeOffset))
    {
        context->handleError(
            Error(GL_INVALID_VALUE,
                  "relativeOffset cannot be greater than MAX_VERTEX_ATTRIB_RELATIVE_OFFSET."));
        return false;
    }

    // [OpenGL ES 3.1] Section 10.3.1 page 243:
    // An INVALID_OPERATION error is generated if the default vertex array object is bound.
    if (context->getGLState().getVertexArrayId() == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Default vertex array object is bound."));
        return false;
    }

    return ValidateVertexFormatBase(context, attribIndex, size, type, pureInteger);
}

bool ValidateVertexAttribBinding(ValidationContext *context,
                                 GLuint attribIndex,
                                 GLuint bindingIndex)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    // [OpenGL ES 3.1] Section 10.3.1 page 243:
    // An INVALID_OPERATION error is generated if the default vertex array object is bound.
    if (context->getGLState().getVertexArrayId() == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Default vertex array object is bound."));
        return false;
    }

    const Caps &caps = context->getCaps();
    if (attribIndex >= caps.maxVertexAttributes)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "attribindex must be smaller than MAX_VERTEX_ATTRIBS."));
        return false;
    }

    if (bindingIndex >= caps.maxVertexAttribBindings)
    {
        context->handleError(Error(GL_INVALID_VALUE,
                                   "bindingindex must be smaller than MAX_VERTEX_ATTRIB_BINDINGS"));
        return false;
    }

    return true;
}

bool ValidateGetProgramResourceName(Context *context,
                                    GLuint program,
                                    GLenum programInterface,
                                    GLuint index,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLchar *name)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }

    if (!ValidateNamedProgramInterface(programInterface))
    {
        context->handleError(
            Error(GL_INVALID_ENUM, "Invalid program interface: 0x%X", programInterface));
        return false;
    }

    if (!ValidateProgramResourceIndex(programObject, programInterface, index))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid index: %d", index));
        return false;
    }

    if (bufSize < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid bufSize: %d", bufSize));
        return false;
    }

    return true;
}

bool ValidateDispatchCompute(Context *context,
                             GLuint numGroupsX,
                             GLuint numGroupsY,
                             GLuint numGroupsZ)
{
    if (context->getClientVersion() < ES_3_1)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Context does not support GLES3.1."));
        return false;
    }

    const State &state = context->getGLState();
    Program *program   = state.getProgram();

    if (program == nullptr)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "No active program object for the compute shader stage."));
        return false;
    }

    if (program->isLinked() == false || program->getAttachedComputeShader() == nullptr)
    {
        context->handleError(Error(
            GL_INVALID_OPERATION,
            "Program has not been successfully linked, or program contains no compute shaders."));
        return false;
    }

    const Caps &caps = context->getCaps();
    if (numGroupsX > caps.maxComputeWorkGroupCount[0])
    {
        context->handleError(
            Error(GL_INVALID_VALUE,
                  "num_groups_x cannot be greater than MAX_COMPUTE_WORK_GROUP_COUNT[0](%u).",
                  caps.maxComputeWorkGroupCount[0]));
        return false;
    }
    if (numGroupsY > caps.maxComputeWorkGroupCount[1])
    {
        context->handleError(
            Error(GL_INVALID_VALUE,
                  "num_groups_y cannot be greater than MAX_COMPUTE_WORK_GROUP_COUNT[1](%u).",
                  caps.maxComputeWorkGroupCount[1]));
        return false;
    }
    if (numGroupsZ > caps.maxComputeWorkGroupCount[2])
    {
        context->handleError(
            Error(GL_INVALID_VALUE,
                  "num_groups_z cannot be greater than MAX_COMPUTE_WORK_GROUP_COUNT[2](%u).",
                  caps.maxComputeWorkGroupCount[2]));
        return false;
    }

    return true;
}

}  // namespace gl
