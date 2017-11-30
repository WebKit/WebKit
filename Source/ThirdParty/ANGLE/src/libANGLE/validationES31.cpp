//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES31.cpp: Validation functions for OpenGL ES 3.1 entry point parameters

#include "libANGLE/validationES31.h"

#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/validationES.h"
#include "libANGLE/validationES2.h"
#include "libANGLE/validationES3.h"

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

bool ValidateLocationProgramInterface(GLenum programInterface)
{
    switch (programInterface)
    {
        case GL_UNIFORM:
        case GL_PROGRAM_INPUT:
        case GL_PROGRAM_OUTPUT:
            return true;
        default:
            return false;
    }
}

bool ValidateProgramInterface(GLenum programInterface)
{
    return (programInterface == GL_ATOMIC_COUNTER_BUFFER ||
            ValidateNamedProgramInterface(programInterface));
}

bool ValidateProgramResourceProperty(GLenum prop)
{
    switch (prop)
    {
        case GL_ACTIVE_VARIABLES:
        case GL_BUFFER_BINDING:
        case GL_NUM_ACTIVE_VARIABLES:

        case GL_ARRAY_SIZE:

        case GL_ARRAY_STRIDE:
        case GL_BLOCK_INDEX:
        case GL_IS_ROW_MAJOR:
        case GL_MATRIX_STRIDE:

        case GL_ATOMIC_COUNTER_BUFFER_INDEX:

        case GL_BUFFER_DATA_SIZE:

        case GL_LOCATION:

        case GL_NAME_LENGTH:

        case GL_OFFSET:

        case GL_REFERENCED_BY_VERTEX_SHADER:
        case GL_REFERENCED_BY_FRAGMENT_SHADER:
        case GL_REFERENCED_BY_COMPUTE_SHADER:

        case GL_TOP_LEVEL_ARRAY_SIZE:
        case GL_TOP_LEVEL_ARRAY_STRIDE:

        case GL_TYPE:
            return true;

        default:
            return false;
    }
}

// GLES 3.10 spec: Page 82 -- Table 7.2
bool ValidateProgramResourcePropertyByInterface(GLenum prop, GLenum programInterface)
{
    switch (prop)
    {
        case GL_ACTIVE_VARIABLES:
        case GL_BUFFER_BINDING:
        case GL_NUM_ACTIVE_VARIABLES:
        {
            switch (programInterface)
            {
                case GL_ATOMIC_COUNTER_BUFFER:
                case GL_SHADER_STORAGE_BLOCK:
                case GL_UNIFORM_BLOCK:
                    return true;
                default:
                    return false;
            }
        }

        case GL_ARRAY_SIZE:
        {
            switch (programInterface)
            {
                case GL_BUFFER_VARIABLE:
                case GL_PROGRAM_INPUT:
                case GL_PROGRAM_OUTPUT:
                case GL_TRANSFORM_FEEDBACK_VARYING:
                case GL_UNIFORM:
                    return true;
                default:
                    return false;
            }
        }

        case GL_ARRAY_STRIDE:
        case GL_BLOCK_INDEX:
        case GL_IS_ROW_MAJOR:
        case GL_MATRIX_STRIDE:
        {
            switch (programInterface)
            {
                case GL_BUFFER_VARIABLE:
                case GL_UNIFORM:
                    return true;
                default:
                    return false;
            }
        }

        case GL_ATOMIC_COUNTER_BUFFER_INDEX:
        {
            if (programInterface == GL_UNIFORM)
            {
                return true;
            }
            return false;
        }

        case GL_BUFFER_DATA_SIZE:
        {
            switch (programInterface)
            {
                case GL_ATOMIC_COUNTER_BUFFER:
                case GL_SHADER_STORAGE_BLOCK:
                case GL_UNIFORM_BLOCK:
                    return true;
                default:
                    return false;
            }
        }

        case GL_LOCATION:
        {
            return ValidateLocationProgramInterface(programInterface);
        }

        case GL_NAME_LENGTH:
        {
            return ValidateNamedProgramInterface(programInterface);
        }

        case GL_OFFSET:
        {
            switch (programInterface)
            {
                case GL_BUFFER_VARIABLE:
                case GL_UNIFORM:
                    return true;
                default:
                    return false;
            }
        }

        case GL_REFERENCED_BY_VERTEX_SHADER:
        case GL_REFERENCED_BY_FRAGMENT_SHADER:
        case GL_REFERENCED_BY_COMPUTE_SHADER:
        {
            switch (programInterface)
            {
                case GL_ATOMIC_COUNTER_BUFFER:
                case GL_BUFFER_VARIABLE:
                case GL_PROGRAM_INPUT:
                case GL_PROGRAM_OUTPUT:
                case GL_SHADER_STORAGE_BLOCK:
                case GL_UNIFORM:
                case GL_UNIFORM_BLOCK:
                    return true;
                default:
                    return false;
            }
        }

        case GL_TOP_LEVEL_ARRAY_SIZE:
        case GL_TOP_LEVEL_ARRAY_STRIDE:
        {
            if (programInterface == GL_BUFFER_VARIABLE)
            {
                return true;
            }
            return false;
        }

        case GL_TYPE:
        {
            switch (programInterface)
            {
                case GL_BUFFER_VARIABLE:
                case GL_PROGRAM_INPUT:
                case GL_PROGRAM_OUTPUT:
                case GL_TRANSFORM_FEEDBACK_VARYING:
                case GL_UNIFORM:
                    return true;
                default:
                    return false;
            }
        }

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

        case GL_UNIFORM:
            return (index < static_cast<GLuint>(programObject->getActiveUniformCount()));

        case GL_BUFFER_VARIABLE:
            return (index < static_cast<GLuint>(programObject->getActiveBufferVariableCount()));

        case GL_SHADER_STORAGE_BLOCK:
            return (index < static_cast<GLuint>(programObject->getActiveShaderStorageBlockCount()));

        case GL_UNIFORM_BLOCK:
            return (index < programObject->getActiveUniformBlockCount());

        case GL_ATOMIC_COUNTER_BUFFER:
            return (index < programObject->getActiveAtomicCounterBufferCount());

        // TODO(jie.a.chen@intel.com): more interfaces.
        case GL_TRANSFORM_FEEDBACK_VARYING:
            UNIMPLEMENTED();
            return false;

        default:
            UNREACHABLE();
            return false;
    }
}

bool ValidateProgramUniform(gl::Context *context,
                            GLenum valueType,
                            GLuint program,
                            GLint location,
                            GLsizei count)
{
    // Check for ES31 program uniform entry points
    if (context->getClientVersion() < Version(3, 1))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = GetValidProgram(context, program);
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniformValue(context, valueType, uniform->type);
}

bool ValidateProgramUniformMatrix(gl::Context *context,
                                  GLenum valueType,
                                  GLuint program,
                                  GLint location,
                                  GLsizei count,
                                  GLboolean transpose)
{
    // Check for ES31 program uniform entry points
    if (context->getClientVersion() < Version(3, 1))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = GetValidProgram(context, program);
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniformMatrixValue(context, valueType, uniform->type);
}

bool ValidateVertexAttribFormatCommon(ValidationContext *context,
                                      GLuint attribIndex,
                                      GLint size,
                                      GLenum type,
                                      GLuint relativeOffset,
                                      GLboolean pureInteger)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    const Caps &caps = context->getCaps();
    if (relativeOffset > static_cast<GLuint>(caps.maxVertexAttribRelativeOffset))
    {
        context->handleError(
            InvalidValue()
            << "relativeOffset cannot be greater than MAX_VERTEX_ATTRIB_RELATIVE_OFFSET.");
        return false;
    }

    // [OpenGL ES 3.1] Section 10.3.1 page 243:
    // An INVALID_OPERATION error is generated if the default vertex array object is bound.
    if (context->getGLState().getVertexArrayId() == 0)
    {
        context->handleError(InvalidOperation() << "Default vertex array object is bound.");
        return false;
    }

    return ValidateVertexFormatBase(context, attribIndex, size, type, pureInteger);
}

}  // anonymous namespace

bool ValidateGetBooleani_v(Context *context, GLenum target, GLuint index, GLboolean *data)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
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
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
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

bool ValidateDrawIndirectBase(Context *context, GLenum mode, const void *indirect)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
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
        context->handleError(InvalidOperation() << "zero is bound to VERTEX_ARRAY_BINDING");
        return false;
    }

    gl::Buffer *drawIndirectBuffer = state.getTargetBuffer(BufferBinding::DrawIndirect);
    if (!drawIndirectBuffer)
    {
        context->handleError(InvalidOperation() << "zero is bound to DRAW_INDIRECT_BUFFER");
        return false;
    }

    // An INVALID_VALUE error is generated if indirect is not a multiple of the size, in basic
    // machine units, of uint.
    GLint64 offset = reinterpret_cast<GLint64>(indirect);
    if ((static_cast<GLuint>(offset) % sizeof(GLuint)) != 0)
    {
        context->handleError(
            InvalidValue()
            << "indirect is not a multiple of the size, in basic machine units, of uint");
        return false;
    }

    // ANGLE_multiview spec, revision 1:
    // An INVALID_OPERATION is generated by DrawArraysIndirect and DrawElementsIndirect if the
    // number of views in the draw framebuffer is greater than 1.
    const Framebuffer *drawFramebuffer = context->getGLState().getDrawFramebuffer();
    ASSERT(drawFramebuffer != nullptr);
    if (drawFramebuffer->getNumViews() > 1)
    {
        context->handleError(
            InvalidOperation()
            << "The number of views in the active draw framebuffer is greater than 1.");
        return false;
    }

    return true;
}

bool ValidateDrawArraysIndirect(Context *context, GLenum mode, const void *indirect)
{
    const State &state                          = context->getGLState();
    gl::TransformFeedback *curTransformFeedback = state.getCurrentTransformFeedback();
    if (curTransformFeedback && curTransformFeedback->isActive() &&
        !curTransformFeedback->isPaused())
    {
        // An INVALID_OPERATION error is generated if transform feedback is active and not paused.
        context->handleError(InvalidOperation() << "transform feedback is active and not paused.");
        return false;
    }

    if (!ValidateDrawIndirectBase(context, mode, indirect))
        return false;

    gl::Buffer *drawIndirectBuffer = state.getTargetBuffer(BufferBinding::DrawIndirect);
    CheckedNumeric<size_t> checkedOffset(reinterpret_cast<size_t>(indirect));
    // In OpenGL ES3.1 spec, session 10.5, it defines the struct of DrawArraysIndirectCommand
    // which's size is 4 * sizeof(uint).
    auto checkedSum = checkedOffset + 4 * sizeof(GLuint);
    if (!checkedSum.IsValid() ||
        checkedSum.ValueOrDie() > static_cast<size_t>(drawIndirectBuffer->getSize()))
    {
        context->handleError(
            InvalidOperation()
            << "the  command  would source data beyond the end of the buffer object.");
        return false;
    }

    return true;
}

bool ValidateDrawElementsIndirect(Context *context, GLenum mode, GLenum type, const void *indirect)
{
    if (!ValidateDrawElementsBase(context, type))
        return false;

    const State &state             = context->getGLState();
    const VertexArray *vao         = state.getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer().get();
    if (!elementArrayBuffer)
    {
        context->handleError(InvalidOperation() << "zero is bound to ELEMENT_ARRAY_BUFFER");
        return false;
    }

    if (!ValidateDrawIndirectBase(context, mode, indirect))
        return false;

    gl::Buffer *drawIndirectBuffer = state.getTargetBuffer(BufferBinding::DrawIndirect);
    CheckedNumeric<size_t> checkedOffset(reinterpret_cast<size_t>(indirect));
    // In OpenGL ES3.1 spec, session 10.5, it defines the struct of DrawElementsIndirectCommand
    // which's size is 5 * sizeof(uint).
    auto checkedSum = checkedOffset + 5 * sizeof(GLuint);
    if (!checkedSum.IsValid() ||
        checkedSum.ValueOrDie() > static_cast<size_t>(drawIndirectBuffer->getSize()))
    {
        context->handleError(
            InvalidOperation()
            << "the  command  would source data beyond the end of the buffer object.");
        return false;
    }

    return true;
}

bool ValidateProgramUniform1i(Context *context, GLuint program, GLint location, GLint v0)
{
    return ValidateProgramUniform1iv(context, program, location, 1, &v0);
}

bool ValidateProgramUniform2i(Context *context, GLuint program, GLint location, GLint v0, GLint v1)
{
    GLint xy[2] = {v0, v1};
    return ValidateProgramUniform2iv(context, program, location, 1, xy);
}

bool ValidateProgramUniform3i(Context *context,
                              GLuint program,
                              GLint location,
                              GLint v0,
                              GLint v1,
                              GLint v2)
{
    GLint xyz[3] = {v0, v1, v2};
    return ValidateProgramUniform3iv(context, program, location, 1, xyz);
}

bool ValidateProgramUniform4i(Context *context,
                              GLuint program,
                              GLint location,
                              GLint v0,
                              GLint v1,
                              GLint v2,
                              GLint v3)
{
    GLint xyzw[4] = {v0, v1, v2, v3};
    return ValidateProgramUniform4iv(context, program, location, 1, xyzw);
}

bool ValidateProgramUniform1ui(Context *context, GLuint program, GLint location, GLuint v0)
{
    return ValidateProgramUniform1uiv(context, program, location, 1, &v0);
}

bool ValidateProgramUniform2ui(Context *context,
                               GLuint program,
                               GLint location,
                               GLuint v0,
                               GLuint v1)
{
    GLuint xy[2] = {v0, v1};
    return ValidateProgramUniform2uiv(context, program, location, 1, xy);
}

bool ValidateProgramUniform3ui(Context *context,
                               GLuint program,
                               GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2)
{
    GLuint xyz[3] = {v0, v1, v2};
    return ValidateProgramUniform3uiv(context, program, location, 1, xyz);
}

bool ValidateProgramUniform4ui(Context *context,
                               GLuint program,
                               GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2,
                               GLuint v3)
{
    GLuint xyzw[4] = {v0, v1, v2, v3};
    return ValidateProgramUniform4uiv(context, program, location, 1, xyzw);
}

bool ValidateProgramUniform1f(Context *context, GLuint program, GLint location, GLfloat v0)
{
    return ValidateProgramUniform1fv(context, program, location, 1, &v0);
}

bool ValidateProgramUniform2f(Context *context,
                              GLuint program,
                              GLint location,
                              GLfloat v0,
                              GLfloat v1)
{
    GLfloat xy[2] = {v0, v1};
    return ValidateProgramUniform2fv(context, program, location, 1, xy);
}

bool ValidateProgramUniform3f(Context *context,
                              GLuint program,
                              GLint location,
                              GLfloat v0,
                              GLfloat v1,
                              GLfloat v2)
{
    GLfloat xyz[3] = {v0, v1, v2};
    return ValidateProgramUniform3fv(context, program, location, 1, xyz);
}

bool ValidateProgramUniform4f(Context *context,
                              GLuint program,
                              GLint location,
                              GLfloat v0,
                              GLfloat v1,
                              GLfloat v2,
                              GLfloat v3)
{
    GLfloat xyzw[4] = {v0, v1, v2, v3};
    return ValidateProgramUniform4fv(context, program, location, 1, xyzw);
}

bool ValidateProgramUniform1iv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value)
{
    // Check for ES31 program uniform entry points
    if (context->getClientVersion() < Version(3, 1))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = GetValidProgram(context, program);
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniform1ivValue(context, uniform->type, count, value);
}

bool ValidateProgramUniform2iv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value)
{
    return ValidateProgramUniform(context, GL_INT_VEC2, program, location, count);
}

bool ValidateProgramUniform3iv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value)
{
    return ValidateProgramUniform(context, GL_INT_VEC3, program, location, count);
}

bool ValidateProgramUniform4iv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value)
{
    return ValidateProgramUniform(context, GL_INT_VEC4, program, location, count);
}

bool ValidateProgramUniform1uiv(Context *context,
                                GLuint program,
                                GLint location,
                                GLsizei count,
                                const GLuint *value)
{
    return ValidateProgramUniform(context, GL_UNSIGNED_INT, program, location, count);
}

bool ValidateProgramUniform2uiv(Context *context,
                                GLuint program,
                                GLint location,
                                GLsizei count,
                                const GLuint *value)
{
    return ValidateProgramUniform(context, GL_UNSIGNED_INT_VEC2, program, location, count);
}

bool ValidateProgramUniform3uiv(Context *context,
                                GLuint program,
                                GLint location,
                                GLsizei count,
                                const GLuint *value)
{
    return ValidateProgramUniform(context, GL_UNSIGNED_INT_VEC3, program, location, count);
}

bool ValidateProgramUniform4uiv(Context *context,
                                GLuint program,
                                GLint location,
                                GLsizei count,
                                const GLuint *value)
{
    return ValidateProgramUniform(context, GL_UNSIGNED_INT_VEC4, program, location, count);
}

bool ValidateProgramUniform1fv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLfloat *value)
{
    return ValidateProgramUniform(context, GL_FLOAT, program, location, count);
}

bool ValidateProgramUniform2fv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLfloat *value)
{
    return ValidateProgramUniform(context, GL_FLOAT_VEC2, program, location, count);
}

bool ValidateProgramUniform3fv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLfloat *value)
{
    return ValidateProgramUniform(context, GL_FLOAT_VEC3, program, location, count);
}

bool ValidateProgramUniform4fv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLfloat *value)
{
    return ValidateProgramUniform(context, GL_FLOAT_VEC4, program, location, count);
}

bool ValidateProgramUniformMatrix2fv(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT2, program, location, count,
                                        transpose);
}

bool ValidateProgramUniformMatrix3fv(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT3, program, location, count,
                                        transpose);
}

bool ValidateProgramUniformMatrix4fv(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT4, program, location, count,
                                        transpose);
}

bool ValidateProgramUniformMatrix2x3fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT2x3, program, location, count,
                                        transpose);
}

bool ValidateProgramUniformMatrix3x2fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT3x2, program, location, count,
                                        transpose);
}

bool ValidateProgramUniformMatrix2x4fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT2x4, program, location, count,
                                        transpose);
}

bool ValidateProgramUniformMatrix4x2fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT4x2, program, location, count,
                                        transpose);
}

bool ValidateProgramUniformMatrix3x4fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT3x4, program, location, count,
                                        transpose);
}

bool ValidateProgramUniformMatrix4x3fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value)
{
    return ValidateProgramUniformMatrix(context, GL_FLOAT_MAT4x3, program, location, count,
                                        transpose);
}

bool ValidateGetTexLevelParameterBase(Context *context,
                                      GLenum target,
                                      GLint level,
                                      GLenum pname,
                                      GLsizei *length)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    if (length)
    {
        *length = 0;
    }

    if (!ValidTexLevelDestinationTarget(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidTextureTarget);
        return false;
    }

    if (context->getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target) ==
        nullptr)
    {
        context->handleError(InvalidEnum() << "No texture bound.");
        return false;
    }

    if (!ValidMipLevel(context, target, level))
    {
        context->handleError(InvalidValue());
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
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidPname);
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

bool ValidateTexStorage2DMultisample(Context *context,
                                     GLenum target,
                                     GLsizei samples,
                                     GLint internalFormat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLboolean fixedSampleLocations)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    if (target != GL_TEXTURE_2D_MULTISAMPLE)
    {
        context->handleError(InvalidEnum() << "Target must be TEXTURE_2D_MULTISAMPLE.");
        return false;
    }

    if (width < 1 || height < 1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeSize);
        return false;
    }

    const Caps &caps = context->getCaps();
    if (static_cast<GLuint>(width) > caps.max2DTextureSize ||
        static_cast<GLuint>(height) > caps.max2DTextureSize)
    {
        context
            ->handleError(InvalidValue()
                          << "Width and height must be less than or equal to GL_MAX_TEXTURE_SIZE.");
        return false;
    }

    if (samples == 0)
    {
        context->handleError(InvalidValue() << "Samples may not be zero.");
        return false;
    }

    const TextureCaps &formatCaps = context->getTextureCaps().get(internalFormat);
    if (!formatCaps.renderable)
    {
        context->handleError(InvalidEnum() << "SizedInternalformat must be color-renderable, "
                                              "depth-renderable, or stencil-renderable.");
        return false;
    }

    // The ES3.1 spec(section 8.8) states that an INVALID_ENUM error is generated if internalformat
    // is one of the unsized base internalformats listed in table 8.11.
    const InternalFormat &formatInfo = GetSizedInternalFormatInfo(internalFormat);
    if (formatInfo.internalFormat == GL_NONE)
    {
        context->handleError(
            InvalidEnum()
            << "Internalformat is one of the unsupported unsized base internalformats.");
        return false;
    }

    if (static_cast<GLuint>(samples) > formatCaps.getMaxSamples())
    {
        context->handleError(
            InvalidOperation()
            << "Samples must not be greater than maximum supported value for the format.");
        return false;
    }

    Texture *texture = context->getTargetTexture(target);
    if (!texture || texture->id() == 0)
    {
        context->handleError(InvalidOperation() << "Zero is bound to target.");
        return false;
    }

    if (texture->getImmutableFormat())
    {
        context->handleError(InvalidOperation() << "The value of TEXTURE_IMMUTABLE_FORMAT for "
                                                   "the texture currently bound to target on "
                                                   "the active texture unit is true.");
        return false;
    }

    return true;
}

bool ValidateGetMultisamplefv(Context *context, GLenum pname, GLuint index, GLfloat *val)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    if (pname != GL_SAMPLE_POSITION)
    {
        context->handleError(InvalidEnum() << "Pname must be SAMPLE_POSITION.");
        return false;
    }

    Framebuffer *framebuffer = context->getGLState().getDrawFramebuffer();

    if (index >= static_cast<GLuint>(framebuffer->getSamples(context)))
    {
        context->handleError(InvalidValue() << "Index must be less than the value of SAMPLES.");
        return false;
    }

    return true;
}

bool ValidateFramebufferParameteri(Context *context, GLenum target, GLenum pname, GLint param)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    if (!ValidFramebufferTarget(context, target))
    {
        context->handleError(InvalidEnum() << "Invalid framebuffer target.");
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
                    InvalidValue()
                    << "Params less than 0 or greater than GL_MAX_FRAMEBUFFER_WIDTH.");
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
                    InvalidValue()
                    << "Params less than 0 or greater than GL_MAX_FRAMEBUFFER_HEIGHT.");
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
                    InvalidValue()
                    << "Params less than 0 or greater than GL_MAX_FRAMEBUFFER_SAMPLES.");
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
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidPname);
            return false;
        }
    }

    const Framebuffer *framebuffer = context->getGLState().getTargetFramebuffer(target);
    ASSERT(framebuffer);
    if (framebuffer->id() == 0)
    {
        context->handleError(InvalidOperation() << "Default framebuffer is bound to target.");
        return false;
    }
    return true;
}

bool ValidateGetFramebufferParameteriv(Context *context, GLenum target, GLenum pname, GLint *params)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    if (!ValidFramebufferTarget(context, target))
    {
        ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidFramebufferTarget);
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
            ANGLE_VALIDATION_ERR(context, InvalidEnum(), InvalidPname);
            return false;
    }

    const Framebuffer *framebuffer = context->getGLState().getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->id() == 0)
    {
        context->handleError(InvalidOperation() << "Default framebuffer is bound to target.");
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
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }

    if (!ValidateNamedProgramInterface(programInterface))
    {
        context->handleError(InvalidEnum() << "Invalid program interface: 0x" << std::hex
                                           << std::uppercase << programInterface);
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
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    if (!context->isBufferGenerated(buffer))
    {
        context->handleError(InvalidOperation() << "Buffer is not generated.");
        return false;
    }

    const Caps &caps = context->getCaps();
    if (bindingIndex >= caps.maxVertexAttribBindings)
    {
        context->handleError(InvalidValue()
                             << "bindingindex must be smaller than MAX_VERTEX_ATTRIB_BINDINGS.");
        return false;
    }

    if (offset < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeOffset);
        return false;
    }

    if (stride < 0 || stride > caps.maxVertexAttribStride)
    {
        context->handleError(InvalidValue()
                             << "stride must be between 0 and MAX_VERTEX_ATTRIB_STRIDE.");
        return false;
    }

    // [OpenGL ES 3.1] Section 10.3.1 page 244:
    // An INVALID_OPERATION error is generated if the default vertex array object is bound.
    if (context->getGLState().getVertexArrayId() == 0)
    {
        context->handleError(InvalidOperation() << "Default vertex array buffer is bound.");
        return false;
    }

    return true;
}

bool ValidateVertexBindingDivisor(ValidationContext *context, GLuint bindingIndex, GLuint divisor)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    const Caps &caps = context->getCaps();
    if (bindingIndex >= caps.maxVertexAttribBindings)
    {
        context->handleError(InvalidValue()
                             << "bindingindex must be smaller than MAX_VERTEX_ATTRIB_BINDINGS.");
        return false;
    }

    // [OpenGL ES 3.1] Section 10.3.1 page 243:
    // An INVALID_OPERATION error is generated if the default vertex array object is bound.
    if (context->getGLState().getVertexArrayId() == 0)
    {
        context->handleError(InvalidOperation() << "Default vertex array object is bound.");
        return false;
    }

    return true;
}

bool ValidateVertexAttribFormat(ValidationContext *context,
                                GLuint attribindex,
                                GLint size,
                                GLenum type,
                                GLboolean normalized,
                                GLuint relativeoffset)
{
    return ValidateVertexAttribFormatCommon(context, attribindex, size, type, relativeoffset,
                                            false);
}

bool ValidateVertexAttribIFormat(ValidationContext *context,
                                 GLuint attribindex,
                                 GLint size,
                                 GLenum type,
                                 GLuint relativeoffset)
{
    return ValidateVertexAttribFormatCommon(context, attribindex, size, type, relativeoffset, true);
}

bool ValidateVertexAttribBinding(ValidationContext *context,
                                 GLuint attribIndex,
                                 GLuint bindingIndex)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    // [OpenGL ES 3.1] Section 10.3.1 page 243:
    // An INVALID_OPERATION error is generated if the default vertex array object is bound.
    if (context->getGLState().getVertexArrayId() == 0)
    {
        context->handleError(InvalidOperation() << "Default vertex array object is bound.");
        return false;
    }

    const Caps &caps = context->getCaps();
    if (attribIndex >= caps.maxVertexAttributes)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), IndexExceedsMaxVertexAttribute);
        return false;
    }

    if (bindingIndex >= caps.maxVertexAttribBindings)
    {
        context->handleError(InvalidValue()
                             << "bindingindex must be smaller than MAX_VERTEX_ATTRIB_BINDINGS");
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
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }

    if (!ValidateNamedProgramInterface(programInterface))
    {
        context->handleError(InvalidEnum() << "Invalid program interface: 0x" << std::hex
                                           << std::uppercase << programInterface);
        return false;
    }

    if (!ValidateProgramResourceIndex(programObject, programInterface, index))
    {
        context->handleError(InvalidValue() << "Invalid index: " << index);
        return false;
    }

    if (bufSize < 0)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), NegativeBufferSize);
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
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    const State &state = context->getGLState();
    Program *program   = state.getProgram();

    if (program == nullptr)
    {
        context->handleError(InvalidOperation()
                             << "No active program object for the compute shader stage.");
        return false;
    }

    if (!program->isLinked() || !program->hasLinkedComputeShader())
    {
        context->handleError(
            InvalidOperation()
            << "Program has not been successfully linked, or program contains no compute shaders.");
        return false;
    }

    const Caps &caps = context->getCaps();
    if (numGroupsX > caps.maxComputeWorkGroupCount[0])
    {
        context->handleError(
            InvalidValue() << "num_groups_x cannot be greater than MAX_COMPUTE_WORK_GROUP_COUNT[0]="
                           << caps.maxComputeWorkGroupCount[0]);
        return false;
    }
    if (numGroupsY > caps.maxComputeWorkGroupCount[1])
    {
        context->handleError(
            InvalidValue() << "num_groups_y cannot be greater than MAX_COMPUTE_WORK_GROUP_COUNT[1]="
                           << caps.maxComputeWorkGroupCount[1]);
        return false;
    }
    if (numGroupsZ > caps.maxComputeWorkGroupCount[2])
    {
        context->handleError(
            InvalidValue() << "num_groups_z cannot be greater than MAX_COMPUTE_WORK_GROUP_COUNT[2]="
                           << caps.maxComputeWorkGroupCount[2]);
        return false;
    }

    return true;
}

bool ValidateDispatchComputeIndirect(Context *context, GLintptr indirect)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateBindImageTexture(Context *context,
                              GLuint unit,
                              GLuint texture,
                              GLint level,
                              GLboolean layered,
                              GLint layer,
                              GLenum access,
                              GLenum format)
{
    GLuint maxImageUnits = context->getCaps().maxImageUnits;
    if (unit >= maxImageUnits)
    {
        context->handleError(InvalidValue()
                             << "unit cannot be greater than or equal than MAX_IMAGE_UNITS = "
                             << maxImageUnits);
        return false;
    }

    if (level < 0)
    {
        context->handleError(InvalidValue() << "level is negative.");
        return false;
    }

    if (layer < 0)
    {
        context->handleError(InvalidValue() << "layer is negative.");
        return false;
    }

    if (access != GL_READ_ONLY && access != GL_WRITE_ONLY && access != GL_READ_WRITE)
    {
        context->handleError(InvalidEnum() << "access is not one of the supported tokens.");
        return false;
    }

    switch (format)
    {
        case GL_RGBA32F:
        case GL_RGBA16F:
        case GL_R32F:
        case GL_RGBA32UI:
        case GL_RGBA16UI:
        case GL_RGBA8UI:
        case GL_R32UI:
        case GL_RGBA32I:
        case GL_RGBA16I:
        case GL_RGBA8I:
        case GL_R32I:
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
            break;
        default:
            context->handleError(InvalidValue()
                                 << "format is not one of supported image unit formats.");
            return false;
    }

    if (texture != 0)
    {
        Texture *tex = context->getTexture(texture);

        if (tex == nullptr)
        {
            context->handleError(InvalidValue()
                                 << "texture is not the name of an existing texture object.");
            return false;
        }

        if (!tex->getImmutableFormat())
        {
            context->handleError(InvalidOperation()
                                 << "texture is not the name of an immutable texture object.");
            return false;
        }
    }

    return true;
}

bool ValidateGetProgramResourceLocation(Context *context,
                                        GLuint program,
                                        GLenum programInterface,
                                        const GLchar *name)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }

    if (!programObject->isLinked())
    {
        context->handleError(InvalidOperation() << "Program is not successfully linked.");
        return false;
    }

    if (!ValidateLocationProgramInterface(programInterface))
    {
        context->handleError(InvalidEnum() << "Invalid program interface.");
        return false;
    }
    return true;
}

bool ValidateGetProgramResourceiv(Context *context,
                                  GLuint program,
                                  GLenum programInterface,
                                  GLuint index,
                                  GLsizei propCount,
                                  const GLenum *props,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *params)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }
    if (!ValidateProgramInterface(programInterface))
    {
        context->handleError(InvalidEnum() << "Invalid program interface.");
        return false;
    }
    if (propCount <= 0)
    {
        context->handleError(InvalidValue() << "Invalid propCount.");
        return false;
    }
    if (bufSize < 0)
    {
        context->handleError(InvalidValue() << "Invalid bufSize.");
        return false;
    }
    if (!ValidateProgramResourceIndex(programObject, programInterface, index))
    {
        context->handleError(InvalidValue() << "Invalid index: " << index);
        return false;
    }
    for (GLsizei i = 0; i < propCount; i++)
    {
        if (!ValidateProgramResourceProperty(props[i]))
        {
            context->handleError(InvalidEnum() << "Invalid prop.");
            return false;
        }
        if (!ValidateProgramResourcePropertyByInterface(props[i], programInterface))
        {
            context->handleError(InvalidOperation() << "Not an allowed prop for interface");
            return false;
        }
    }
    return true;
}

bool ValidateGetProgramInterfaceiv(Context *context,
                                   GLuint program,
                                   GLenum programInterface,
                                   GLenum pname,
                                   GLint *params)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }

    if (!ValidateProgramInterface(programInterface))
    {
        context->handleError(InvalidEnum() << "Invalid program interface.");
        return false;
    }

    switch (pname)
    {
        case GL_ACTIVE_RESOURCES:
        case GL_MAX_NAME_LENGTH:
        case GL_MAX_NUM_ACTIVE_VARIABLES:
            break;

        default:
            context->handleError(InvalidEnum() << "Unknown property of program interface.");
            return false;
    }

    if (pname == GL_MAX_NAME_LENGTH && programInterface == GL_ATOMIC_COUNTER_BUFFER)
    {
        context->handleError(InvalidOperation()
                             << "Active atomic counter resources are not assigned name strings.");
        return false;
    }

    if (pname == GL_MAX_NUM_ACTIVE_VARIABLES)
    {
        switch (programInterface)
        {
            case GL_ATOMIC_COUNTER_BUFFER:
            case GL_SHADER_STORAGE_BLOCK:
            case GL_UNIFORM_BLOCK:
                break;

            default:
                context->handleError(
                    InvalidOperation()
                    << "MAX_NUM_ACTIVE_VARIABLES requires a buffer or block interface.");
                return false;
        }
    }

    return true;
}

static bool ValidateGenOrDeleteES31(Context *context, GLint n)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGenProgramPipelines(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDeleteES31(context, n);
}

bool ValidateDeleteProgramPipelines(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDeleteES31(context, n);
}

bool ValidateBindProgramPipeline(Context *context, GLuint pipeline)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    if (!context->isProgramPipelineGenerated(pipeline))
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ObjectNotGenerated);
        return false;
    }

    return true;
}

bool ValidateIsProgramPipeline(Context *context, GLuint pipeline)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    return true;
}

bool ValidateUseProgramStages(Context *context, GLuint pipeline, GLbitfield stages, GLuint program)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateActiveShaderProgram(Context *context, GLuint pipeline, GLuint program)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateCreateShaderProgramv(Context *context,
                                  GLenum type,
                                  GLsizei count,
                                  const GLchar *const *strings)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateGetProgramPipelineiv(Context *context, GLuint pipeline, GLenum pname, GLint *params)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateValidateProgramPipeline(Context *context, GLuint pipeline)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateGetProgramPipelineInfoLog(Context *context,
                                       GLuint pipeline,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLchar *infoLog)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateMemoryBarrier(Context *context, GLbitfield barriers)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateMemoryBarrierByRegion(Context *context, GLbitfield barriers)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateSampleMaski(Context *context, GLuint maskNumber, GLbitfield mask)
{
    if (context->getClientVersion() < ES_3_1)
    {
        ANGLE_VALIDATION_ERR(context, InvalidOperation(), ES31Required);
        return false;
    }

    if (maskNumber >= context->getCaps().maxSampleMaskWords)
    {
        ANGLE_VALIDATION_ERR(context, InvalidValue(), InvalidSampleMaskNumber);
        return false;
    }

    return true;
}

}  // namespace gl
