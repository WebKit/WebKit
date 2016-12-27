//
// Copyright(c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_gles_3_1.cpp : Implements the GLES 3.1 Entry point.

#include "libGLESv2/entry_points_gles_3_1.h"
#include "libGLESv2/global_state.h"

#include "libANGLE/Context.h"
#include "libANGLE/Error.h"

#include "libANGLE/validationES31.h"

#include "common/debug.h"

namespace gl
{

void GL_APIENTRY DispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ)
{
    EVENT("(GLuint numGroupsX = %u, GLuint numGroupsY = %u, numGroupsZ = %u", numGroupsX,
          numGroupsY, numGroupsZ);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY DispatchComputeIndirect(GLintptr indirect)
{
    EVENT("(GLintptr indirect = %d)", indirect);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY DrawArraysIndirect(GLenum mode, const void *indirect)
{
    EVENT("(GLenum mode = 0x%X, const void* indirect)", mode, indirect);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY DrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
    EVENT("(GLenum mode = 0x%X, GLenum type = 0x%X, const void* indirect)", mode, type, indirect);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY FramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint param = %d)", target, pname, param);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY GetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname,
          params);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY GetProgramInterfaceiv(GLuint program,
                                       GLenum programInterface,
                                       GLenum pname,
                                       GLint *params)
{
    EVENT(
        "(GLuint program = %u, GLenum programInterface = 0x%X, GLenum pname = 0x%X, GLint* params "
        "= 0x%0.8p)",
        program, programInterface, pname, params);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

GLuint GL_APIENTRY GetProgramResourceIndex(GLuint program,
                                           GLenum programInterface,
                                           const GLchar *name)
{
    EVENT("(GLuint program = %u, GLenum programInterface = 0x%X, const GLchar* name = 0x%0.8p)",
          program, programInterface, name);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
    return 0u;
}

void GL_APIENTRY GetProgramResourceName(GLuint program,
                                        GLenum programInterface,
                                        GLuint index,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *name)
{
    EVENT(
        "(GLuint program = %u, GLenum programInterface = 0x%X, GLuint index = %u, GLsizei bufSize "
        "= %d, GLsizei* length = 0x%0.8p, GLchar* name = 0x%0.8p)",
        program, programInterface, index, bufSize, length, name);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY GetProgramResourceiv(GLuint program,
                                      GLenum programInterface,
                                      GLuint index,
                                      GLsizei propCount,
                                      const GLenum *props,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *params)
{
    EVENT(
        "(GLuint program = %u, GLenum programInterface = 0x%X, GLuint index = %u, GLsizei "
        "propCount = %d, const GLenum* props = 0x%0.8p, GLsizei bufSize = %d, GLsizei* length = "
        "0x%0.8p, GLint* params = 0x%0.8p)",
        program, programInterface, index, propCount, props, bufSize, length, params);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

GLint GL_APIENTRY GetProgramResourceLocation(GLuint program,
                                             GLenum programInterface,
                                             const GLchar *name)
{
    EVENT("(GLuint program = %u, GLenum programInterface = 0x%X, const GLchar* name = 0x%0.8p)",
          program, programInterface, name);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
    return 0;
}

void GL_APIENTRY UseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
    EVENT("(GLuint pipeline = %u, GLbitfield stages = 0x%X, GLuint program = %u)", pipeline, stages,
          program);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ActiveShaderProgram(GLuint pipeline, GLuint program)
{
    EVENT("(GLuint pipeline = %u, GLuint program = %u)", pipeline, program);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

GLuint GL_APIENTRY CreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strings)
{
    EVENT("(GLenum type = %0x%X, GLsizei count = %d, const GLchar *const* = 0x%0.8p)", type, count,
          strings);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
    return 0u;
}

void GL_APIENTRY BindProgramPipeline(GLuint pipeline)
{
    EVENT("(GLuint pipeline = %u)", pipeline);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY DeleteProgramPipelines(GLsizei n, const GLuint *pipelines)
{
    EVENT("(GLsizei n = %d, const GLuint* pipelines = 0x%0.8p)", n, pipelines);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY GenProgramPipelines(GLsizei n, GLuint *pipelines)
{
    EVENT("(GLsizei n = %d, GLuint* pipelines = 0x%0.8p)", n, pipelines);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

GLboolean GL_APIENTRY IsProgramPipeline(GLuint pipeline)
{
    EVENT("(GLuint pipeline = %u)", pipeline);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
    return false;
}

void GL_APIENTRY GetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
    EVENT("(GLuint pipeline = %u, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", pipeline, pname,
          params);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform1i(GLuint program, GLint location, GLint v0)
{
    EVENT("(GLuint program = %u, GLint location = %d, GLint v0 = %d)", program, location, v0);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1)
{
    EVENT("(GLuint program = %u, GLint location = %d, GLint v0 = %d, GLint v1 = %d)", program,
          location, v0, v1);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    EVENT("(GLuint program = %u, GLint location = %d, GLint v0 = %d, GLint v1 = %d, GLint v2 = %d)",
          program, location, v0, v1, v2);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY
ProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLint v0 = %d, GLint v1 = %d, GLint v2 = %d, "
        "GLint v3 = %d)",
        program, location, v0, v1, v2, v3);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform1ui(GLuint program, GLint location, GLuint v0)
{
    EVENT("(GLuint program = %u, GLint location = %d, GLuint v0 = %u)", program, location, v0);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1)
{
    EVENT("(GLuint program = %u, GLint location = %d, GLuint v0 = %u, GLuint v1 = %u)", program,
          location, v0, v1);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLuint v0 = %u, GLuint v1 = %u, GLuint v2 = "
        "%u)",
        program, location, v0, v1, v2);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY
ProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLuint v0 = %u, GLuint v1 = %u, GLuint v2 = "
        "%u, GLuint v3 = %u)",
        program, location, v0, v1, v2, v3);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform1f(GLuint program, GLint location, GLfloat v0)
{
    EVENT("(GLuint program = %u, GLint location = %d, GLfloat v0 = %g)", program, location, v0);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    EVENT("(GLuint program = %u, GLint location = %d, GLfloat v0 = %g, GLfloat v1 = %g)", program,
          location, v0, v1);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY
ProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLfloat v0 = %g, GLfloat v1 = %g, GLfloat v2 = "
        "%g)",
        program, location, v0, v1, v2);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY
ProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLfloat v0 = %g, GLfloat v1 = %g, GLfloat v2 = "
        "%g, GLfloat v3 = %g)",
        program, location, v0, v1, v2, v3);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform1iv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   const GLint *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLint* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform2iv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   const GLint *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLint* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform3iv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   const GLint *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLint* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform4iv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   const GLint *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLint* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform1uiv(GLuint program,
                                    GLint location,
                                    GLsizei count,
                                    const GLuint *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLuint* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform2uiv(GLuint program,
                                    GLint location,
                                    GLsizei count,
                                    const GLuint *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLuint* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform3uiv(GLuint program,
                                    GLint location,
                                    GLsizei count,
                                    const GLuint *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLuint* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform4uiv(GLuint program,
                                    GLint location,
                                    GLsizei count,
                                    const GLuint *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLuint* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform1fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLfloat* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform2fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLfloat* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform3fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLfloat* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniform4fv(GLuint program,
                                   GLint location,
                                   GLsizei count,
                                   const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, const GLfloat* value = "
        "0x%0.8p)",
        program, location, count, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix2fv(GLuint program,
                                         GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix3fv(GLuint program,
                                         GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix4fv(GLuint program,
                                         GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix2x3fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix3x2fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix2x4fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix4x2fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix3x4fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ProgramUniformMatrix4x3fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    EVENT(
        "(GLuint program = %u, GLint location = %d, GLsizei count = %d, GLboolean transpose = %u, "
        "const GLfloat* value = 0x%0.8p)",
        program, location, count, transpose, value);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY ValidateProgramPipeline(GLuint pipeline)
{
    EVENT("(GLuint pipeline = %u)", pipeline);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY GetProgramPipelineInfoLog(GLuint pipeline,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLchar *infoLog)
{
    EVENT(
        "(GLuint pipeline = %u, GLsizei bufSize = %d, GLsizei* length = 0x%0.8p, GLchar* infoLog = "
        "0x%0.8p)",
        pipeline, bufSize, length, infoLog);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY BindImageTexture(GLuint unit,
                                  GLuint texture,
                                  GLint level,
                                  GLboolean layered,
                                  GLint layer,
                                  GLenum access,
                                  GLenum format)
{
    EVENT(
        "(GLuint unit = %u, GLuint texture = %u, GLint level = %d, GLboolean layered = %u, GLint "
        "layer = %d, GLenum access = 0x%X, GLenum format = 0x%X)",
        unit, texture, level, layered, layer, access, format);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY GetBooleani_v(GLenum target, GLuint index, GLboolean *data)
{
    EVENT("(GLenum target = 0x%X, GLuint index = %u, GLboolean* data = 0x%0.8p)", target, index,
          data);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetBooleani_v(context, target, index, data))
        {
            return;
        }
        context->getBooleani_v(target, index, data);
    }
}

void GL_APIENTRY MemoryBarrier(GLbitfield barriers)
{
    EVENT("(GLbitfield barriers = 0x%X)", barriers);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY MemoryBarrierByRegion(GLbitfield barriers)
{
    EVENT("(GLbitfield barriers = 0x%X)", barriers);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY TexStorage2DMultisample(GLenum target,
                                         GLsizei samples,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLboolean fixedsamplelocations)
{
    EVENT(
        "(GLenum target = 0x%X, GLsizei samples = %d, GLenum internalformat = 0x%X, GLsizei width "
        "= %d, GLsizei height = %d, GLboolean fixedsamplelocations = %u)",
        target, samples, internalformat, width, height, fixedsamplelocations);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY GetMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
    EVENT("(GLenum pname = 0x%X, GLuint index = %u, GLfloat* val = 0x%0.8p)", pname, index, val);
    UNIMPLEMENTED();
}

void GL_APIENTRY SampleMaski(GLuint maskNumber, GLbitfield mask)
{
    EVENT("(GLuint maskNumber = %u, GLbitfield mask = 0x%X)", maskNumber, mask);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY GetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLenum pname = 0x%X, GLint* params = 0x%0.8p)",
          target, level, pname, params);
    UNIMPLEMENTED();
}

void GL_APIENTRY GetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)",
        target, level, pname, params);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY BindVertexBuffer(GLuint bindingindex,
                                  GLuint buffer,
                                  GLintptr offset,
                                  GLsizei stride)
{
    EVENT(
        "(GLuint bindingindex = %u, GLuint buffer = %u, GLintptr offset = %d, GLsizei stride = %d)",
        bindingindex, buffer, offset, stride);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY VertexAttribFormat(GLuint attribindex,
                                    GLint size,
                                    GLenum type,
                                    GLboolean normalized,
                                    GLuint relativeoffset)
{
    EVENT(
        "(GLuint attribindex = %u, GLint size = %d, GLenum type = 0x%X, GLboolean normalized = %u, "
        "GLuint relativeoffset = %u)",
        attribindex, size, type, normalized, relativeoffset);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY VertexAttribIFormat(GLuint attribindex,
                                     GLint size,
                                     GLenum type,
                                     GLuint relativeoffset)
{
    EVENT(
        "(GLuint attribindex = %u, GLint size = %d, GLenum type = 0x%X, GLuint relativeoffset = "
        "%u)",
        attribindex, size, type, relativeoffset);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY VertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
    EVENT("(GLuint attribindex = %u, GLuint bindingindex = %u)", attribindex, bindingindex);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}

void GL_APIENTRY VertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
    EVENT("(GLuint bindingindex = %u, GLuint divisor = %u)", bindingindex, divisor);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Entry point not implemented"));
        }
        UNIMPLEMENTED();
    }
}
}  // namespace gl
