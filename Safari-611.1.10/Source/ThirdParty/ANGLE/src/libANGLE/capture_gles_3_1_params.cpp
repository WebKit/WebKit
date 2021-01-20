//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_gles31_params.cpp:
//   Pointer parameter capture functions for the OpenGL ES 3.1 entry points.

#include "libANGLE/capture_gles_3_1_autogen.h"

using namespace angle;

namespace gl
{

void CaptureCreateShaderProgramv_strings(const State &glState,
                                         bool isCallValid,
                                         ShaderType typePacked,
                                         GLsizei count,
                                         const GLchar *const *strings,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDeleteProgramPipelines_pipelinesPacked(const State &glState,
                                                   bool isCallValid,
                                                   GLsizei n,
                                                   const ProgramPipelineID *pipelines,
                                                   ParamCapture *paramCapture)
{
    CaptureMemory(pipelines, sizeof(ProgramPipelineID) * n, paramCapture);
}

void CaptureDrawArraysIndirect_indirect(const State &glState,
                                        bool isCallValid,
                                        PrimitiveMode modePacked,
                                        const void *indirect,
                                        ParamCapture *paramCapture)
{
    // DrawArraysIndirect requires that all data sourced for the command,
    // including the DrawArraysIndirectCommand structure, be in buffer objects,
    // and may not be called when the default vertex array object is bound.
    // Indirect pointer is automatically captured in capture_gles_3_1_autogen.cpp
    assert(!isCallValid || glState.getTargetBuffer(gl::BufferBinding::DrawIndirect));
}

void CaptureDrawElementsIndirect_indirect(const State &glState,
                                          bool isCallValid,
                                          PrimitiveMode modePacked,
                                          DrawElementsType typePacked,
                                          const void *indirect,
                                          ParamCapture *paramCapture)
{
    // DrawElementsIndirect requires that all data sourced for the command,
    // including the DrawElementsIndirectCommand structure, be in buffer objects,
    // and may not be called when the default vertex array object is bound
    // Indirect pointer is automatically captured in capture_gles_3_1_autogen.cpp
    assert(!isCallValid || glState.getTargetBuffer(gl::BufferBinding::DrawIndirect));
}

void CaptureGenProgramPipelines_pipelinesPacked(const State &glState,
                                                bool isCallValid,
                                                GLsizei n,
                                                ProgramPipelineID *pipelines,
                                                ParamCapture *paramCapture)
{
    CaptureGenHandles(n, pipelines, paramCapture);
}

void CaptureGetBooleani_v_data(const State &glState,
                               bool isCallValid,
                               GLenum target,
                               GLuint index,
                               GLboolean *data,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferParameteriv_params(const State &glState,
                                             bool isCallValid,
                                             GLenum target,
                                             GLenum pname,
                                             GLint *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetMultisamplefv_val(const State &glState,
                                 bool isCallValid,
                                 GLenum pname,
                                 GLuint index,
                                 GLfloat *val,
                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramInterfaceiv_params(const State &glState,
                                         bool isCallValid,
                                         ShaderProgramID program,
                                         GLenum programInterface,
                                         GLenum pname,
                                         GLint *params,
                                         ParamCapture *paramCapture)
{
    CaptureMemory(params, sizeof(GLint), paramCapture);
}

void CaptureGetProgramPipelineInfoLog_length(const State &glState,
                                             bool isCallValid,
                                             ProgramPipelineID pipeline,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLchar *infoLog,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramPipelineInfoLog_infoLog(const State &glState,
                                              bool isCallValid,
                                              ProgramPipelineID pipeline,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              GLchar *infoLog,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramPipelineiv_params(const State &glState,
                                        bool isCallValid,
                                        ProgramPipelineID pipeline,
                                        GLenum pname,
                                        GLint *params,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramResourceIndex_name(const State &glState,
                                         bool isCallValid,
                                         ShaderProgramID program,
                                         GLenum programInterface,
                                         const GLchar *name,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramResourceLocation_name(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            GLenum programInterface,
                                            const GLchar *name,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramResourceName_length(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID program,
                                          GLenum programInterface,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLchar *name,
                                          ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetProgramResourceName_name(const State &glState,
                                        bool isCallValid,
                                        ShaderProgramID program,
                                        GLenum programInterface,
                                        GLuint index,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *name,
                                        ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureGetProgramResourceiv_props(const State &glState,
                                       bool isCallValid,
                                       ShaderProgramID program,
                                       GLenum programInterface,
                                       GLuint index,
                                       GLsizei propCount,
                                       const GLenum *props,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint *params,
                                       ParamCapture *paramCapture)
{
    CaptureMemory(props, sizeof(GLenum) * propCount, paramCapture);
}

void CaptureGetProgramResourceiv_length(const State &glState,
                                        bool isCallValid,
                                        ShaderProgramID program,
                                        GLenum programInterface,
                                        GLuint index,
                                        GLsizei propCount,
                                        const GLenum *props,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint *params,
                                        ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetProgramResourceiv_params(const State &glState,
                                        bool isCallValid,
                                        ShaderProgramID program,
                                        GLenum programInterface,
                                        GLuint index,
                                        GLsizei propCount,
                                        const GLenum *props,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint *params,
                                        ParamCapture *paramCapture)
{
    // See QueryProgramResourceiv for details on how these are handled
    for (int i = 0; i < propCount; ++i)
    {
        if (props[i] == GL_ACTIVE_VARIABLES)
        {
            // This appears to be the only property that isn't a single integer
            UNIMPLEMENTED();
            return;
        }
    }

    CaptureMemory(props, sizeof(GLint) * propCount, paramCapture);
}

void CaptureGetTexLevelParameterfv_params(const State &glState,
                                          bool isCallValid,
                                          TextureTarget targetPacked,
                                          GLint level,
                                          GLenum pname,
                                          GLfloat *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameteriv_params(const State &glState,
                                          bool isCallValid,
                                          TextureTarget targetPacked,
                                          GLint level,
                                          GLenum pname,
                                          GLint *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform1fv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei count,
                                    const GLfloat *value,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform1iv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei count,
                                    const GLint *value,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform1uiv_value(const State &glState,
                                     bool isCallValid,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     const GLuint *value,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform2fv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei count,
                                    const GLfloat *value,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform2iv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei count,
                                    const GLint *value,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform2uiv_value(const State &glState,
                                     bool isCallValid,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     const GLuint *value,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform3fv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei count,
                                    const GLfloat *value,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform3iv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei count,
                                    const GLint *value,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform3uiv_value(const State &glState,
                                     bool isCallValid,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     const GLuint *value,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform4fv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei count,
                                    const GLfloat *value,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform4iv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei count,
                                    const GLint *value,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform4uiv_value(const State &glState,
                                     bool isCallValid,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     const GLuint *value,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix2fv_value(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID program,
                                          UniformLocation location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix2x3fv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat *value,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix2x4fv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat *value,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix3fv_value(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID program,
                                          UniformLocation location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix3x2fv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat *value,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix3x4fv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat *value,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix4fv_value(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID program,
                                          UniformLocation location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix4x2fv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat *value,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix4x3fv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat *value,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

}  // namespace gl
