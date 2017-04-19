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

#include "libANGLE/validationES.h"
#include "libANGLE/validationES31.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"

#include "common/debug.h"
#include "common/utilities.h"

namespace gl
{

void GL_APIENTRY DispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ)
{
    EVENT("(GLuint numGroupsX = %u, GLuint numGroupsY = %u, numGroupsZ = %u", numGroupsX,
          numGroupsY, numGroupsZ);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateDispatchCompute(context, numGroupsX, numGroupsY, numGroupsZ))
        {
            return;
        }

        context->dispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
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
        if (!context->skipValidation() && !ValidateDrawArraysIndirect(context, mode, indirect))
        {
            return;
        }

        context->drawArraysIndirect(mode, indirect);
    }
}

void GL_APIENTRY DrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
    EVENT("(GLenum mode = 0x%X, GLenum type = 0x%X, const void* indirect)", mode, type, indirect);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateDrawElementsIndirect(context, mode, type, indirect))
        {
            return;
        }

        context->drawElementsIndirect(mode, type, indirect);
    }
}

void GL_APIENTRY FramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint param = %d)", target, pname, param);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidationFramebufferParameteri(context, target, pname, param))
        {
            return;
        }

        context->setFramebufferParameteri(target, pname, param);
    }
}

void GL_APIENTRY GetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    EVENT("(GLenum target = 0x%X, GLenum pname = 0x%X, GLint* params = 0x%0.8p)", target, pname,
          params);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidationGetFramebufferParameteri(context, target, pname, params))
        {
            return;
        }

        context->getFramebufferParameteriv(target, pname, params);
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
        if (!context->skipValidation() &&
            !ValidateGetProgramResourceIndex(context, program, programInterface, name))
        {
            return GL_INVALID_INDEX;
        }
        return context->getProgramResourceIndex(program, programInterface, name);
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
        if (!context->skipValidation() &&
            !ValidateGetProgramResourceName(context, program, programInterface, index, bufSize,
                                            length, name))
        {
            return;
        }
        context->getProgramResourceName(program, programInterface, index, bufSize, length, name);
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
        if (pipeline != 0)
        {
            // Binding non-zero pipelines is not implemented yet.
            UNIMPLEMENTED();
        }
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
    ProgramUniform1iv(program, location, 1, &v0);
}

void GL_APIENTRY ProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1)
{
    GLint xy[2] = {v0, v1};
    ProgramUniform2iv(program, location, 1, xy);
}

void GL_APIENTRY ProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    GLint xyz[3] = {v0, v1, v2};
    ProgramUniform3iv(program, location, 1, xyz);
}

void GL_APIENTRY
ProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    GLint xyzw[4] = {v0, v1, v2, v3};
    ProgramUniform4iv(program, location, 1, xyzw);
}

void GL_APIENTRY ProgramUniform1ui(GLuint program, GLint location, GLuint v0)
{
    ProgramUniform1uiv(program, location, 1, &v0);
}

void GL_APIENTRY ProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1)
{
    GLuint xy[2] = {v0, v1};
    ProgramUniform2uiv(program, location, 1, xy);
}

void GL_APIENTRY ProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    GLuint xyz[3] = {v0, v1, v2};
    ProgramUniform3uiv(program, location, 1, xyz);
}

void GL_APIENTRY
ProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    GLuint xyzw[4] = {v0, v1, v2, v3};
    ProgramUniform4uiv(program, location, 1, xyzw);
}

void GL_APIENTRY ProgramUniform1f(GLuint program, GLint location, GLfloat v0)
{
    ProgramUniform1fv(program, location, 1, &v0);
}

void GL_APIENTRY ProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    GLfloat xy[2] = {v0, v1};
    ProgramUniform2fv(program, location, 1, xy);
}

void GL_APIENTRY
ProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    GLfloat xyz[3] = {v0, v1, v2};
    ProgramUniform3fv(program, location, 1, xyz);
}

void GL_APIENTRY
ProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    GLfloat xyzw[4] = {v0, v1, v2, v3};
    ProgramUniform4fv(program, location, 1, xyzw);
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
        if (!ValidateProgramUniform1iv(context, program, location, count, value))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform1iv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_INT_VEC2, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform2iv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_INT_VEC3, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform3iv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_INT_VEC4, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform4iv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_UNSIGNED_INT, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform1uiv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_UNSIGNED_INT_VEC2, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform2uiv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_UNSIGNED_INT_VEC3, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform3uiv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_UNSIGNED_INT_VEC4, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform4uiv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_FLOAT, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform1fv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_FLOAT_VEC2, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform2fv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_FLOAT_VEC3, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform3fv(location, count, value);
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
        if (!ValidateProgramUniform(context, GL_FLOAT_VEC4, program, location, count))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniform4fv(location, count, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT2, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix2fv(location, count, transpose, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT3, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix3fv(location, count, transpose, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT4, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix4fv(location, count, transpose, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT2x3, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix2x3fv(location, count, transpose, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT3x2, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix3x2fv(location, count, transpose, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT2x4, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix2x4fv(location, count, transpose, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT4x2, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix4x2fv(location, count, transpose, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT3x4, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix3x4fv(location, count, transpose, value);
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
        if (!ValidateProgramUniformMatrix(context, GL_FLOAT_MAT4x3, program, location, count,
                                          transpose))
        {
            return;
        }

        Program *programObject = context->getProgram(program);
        ASSERT(programObject);
        programObject->setUniformMatrix4x3fv(location, count, transpose, value);
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
        if (texture != 0)
        {
            // Binding non-zero image textures is not implemented yet.
            UNIMPLEMENTED();
        }
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
        if (!context->skipValidation() &&
            !ValidateTexStorage2DMultiSample(context, target, samples, internalformat, width,
                                             height, fixedsamplelocations))
        {
            return;
        }
        context->texStorage2DMultisample(target, samples, internalformat, width, height,
                                         fixedsamplelocations);
    }
}

void GL_APIENTRY GetMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
    EVENT("(GLenum pname = 0x%X, GLuint index = %u, GLfloat* val = 0x%0.8p)", pname, index, val);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() && !ValidateGetMultisamplefv(context, pname, index, val))
        {
            return;
        }

        context->getMultisamplefv(pname, index, val);
    }
}

void GL_APIENTRY SampleMaski(GLuint maskNumber, GLbitfield mask)
{
    EVENT("(GLuint maskNumber = %u, GLbitfield mask = 0x%X)", maskNumber, mask);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (mask != ~GLbitfield(0))
        {
            // Setting a non-default sample mask is not implemented yet.
            UNIMPLEMENTED();
        }
    }
}

void GL_APIENTRY GetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
    EVENT("(GLenum target = 0x%X, GLint level = %d, GLenum pname = 0x%X, GLint* params = 0x%0.8p)",
          target, level, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetTexLevelParameteriv(context, target, level, pname, params))
        {
            return;
        }

        Texture *texture = context->getTargetTexture(
            IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
        QueryTexLevelParameteriv(texture, target, level, pname, params);
    }
}

void GL_APIENTRY GetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    EVENT(
        "(GLenum target = 0x%X, GLint level = %d, GLenum pname = 0x%X, GLfloat* params = 0x%0.8p)",
        target, level, pname, params);

    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateGetTexLevelParameterfv(context, target, level, pname, params))
        {
            return;
        }

        Texture *texture = context->getTargetTexture(
            IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
        QueryTexLevelParameterfv(texture, target, level, pname, params);
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
        if (!context->skipValidation() &&
            !ValidateBindVertexBuffer(context, bindingindex, buffer, offset, stride))
        {
            return;
        }

        context->bindVertexBuffer(bindingindex, buffer, offset, stride);
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
        if (!context->skipValidation() &&
            !ValidateVertexAttribFormat(context, attribindex, size, type, relativeoffset, false))
        {
            return;
        }

        context->vertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
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
        if (!context->skipValidation() &&
            !ValidateVertexAttribFormat(context, attribindex, size, type, relativeoffset, true))
        {
            return;
        }

        context->vertexAttribIFormat(attribindex, size, type, relativeoffset);
    }
}

void GL_APIENTRY VertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
    EVENT("(GLuint attribindex = %u, GLuint bindingindex = %u)", attribindex, bindingindex);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateVertexAttribBinding(context, attribindex, bindingindex))
        {
            return;
        }

        context->vertexAttribBinding(attribindex, bindingindex);
    }
}

void GL_APIENTRY VertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
    EVENT("(GLuint bindingindex = %u, GLuint divisor = %u)", bindingindex, divisor);
    Context *context = GetValidGlobalContext();
    if (context)
    {
        if (!context->skipValidation() &&
            !ValidateVertexBindingDivisor(context, bindingindex, divisor))
        {
            return;
        }

        context->setVertexBindingDivisor(bindingindex, divisor);
    }
}
}  // namespace gl
