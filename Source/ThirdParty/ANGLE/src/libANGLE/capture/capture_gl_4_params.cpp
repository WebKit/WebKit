//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_gl_2_params.cpp:
//   Pointer parameter capture functions for the OpenGL 2.x entry points.

#include "libANGLE/capture/capture_gl_4_autogen.h"

namespace gl
{
// GL 4.0
void CaptureGetActiveSubroutineName_length(const State &glState,
                                           bool isCallValid,
                                           ShaderProgramID programPacked,
                                           GLenum shadertype,
                                           GLuint index,
                                           GLsizei bufsize,
                                           GLsizei *length,
                                           GLchar *name,
                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetActiveSubroutineName_name(const State &glState,
                                         bool isCallValid,
                                         ShaderProgramID programPacked,
                                         GLenum shadertype,
                                         GLuint index,
                                         GLsizei bufsize,
                                         GLsizei *length,
                                         GLchar *name,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetActiveSubroutineUniformName_length(const State &glState,
                                                  bool isCallValid,
                                                  ShaderProgramID programPacked,
                                                  GLenum shadertype,
                                                  GLuint index,
                                                  GLsizei bufsize,
                                                  GLsizei *length,
                                                  GLchar *name,
                                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetActiveSubroutineUniformName_name(const State &glState,
                                                bool isCallValid,
                                                ShaderProgramID programPacked,
                                                GLenum shadertype,
                                                GLuint index,
                                                GLsizei bufsize,
                                                GLsizei *length,
                                                GLchar *name,
                                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetActiveSubroutineUniformiv_values(const State &glState,
                                                bool isCallValid,
                                                ShaderProgramID programPacked,
                                                GLenum shadertype,
                                                GLuint index,
                                                GLenum pname,
                                                GLint *values,
                                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetProgramStageiv_values(const State &glState,
                                     bool isCallValid,
                                     ShaderProgramID programPacked,
                                     GLenum shadertype,
                                     GLenum pname,
                                     GLint *values,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetQueryIndexediv_params(const State &glState,
                                     bool isCallValid,
                                     GLenum target,
                                     GLuint index,
                                     GLenum pname,
                                     GLint *params,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetSubroutineIndex_name(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID programPacked,
                                    GLenum shadertype,
                                    const GLchar *name,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetSubroutineUniformLocation_name(const State &glState,
                                              bool isCallValid,
                                              ShaderProgramID programPacked,
                                              GLenum shadertype,
                                              const GLchar *name,
                                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetUniformSubroutineuiv_params(const State &glState,
                                           bool isCallValid,
                                           GLenum shadertype,
                                           GLint location,
                                           GLuint *params,
                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetUniformdv_params(const State &glState,
                                bool isCallValid,
                                ShaderProgramID programPacked,
                                UniformLocation locationPacked,
                                GLdouble *params,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CapturePatchParameterfv_values(const State &glState,
                                    bool isCallValid,
                                    GLenum pname,
                                    const GLfloat *values,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniform1dv_value(const State &glState,
                             bool isCallValid,
                             UniformLocation locationPacked,
                             GLsizei count,
                             const GLdouble *value,
                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniform2dv_value(const State &glState,
                             bool isCallValid,
                             UniformLocation locationPacked,
                             GLsizei count,
                             const GLdouble *value,
                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniform3dv_value(const State &glState,
                             bool isCallValid,
                             UniformLocation locationPacked,
                             GLsizei count,
                             const GLdouble *value,
                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniform4dv_value(const State &glState,
                             bool isCallValid,
                             UniformLocation locationPacked,
                             GLsizei count,
                             const GLdouble *value,
                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix2dv_value(const State &glState,
                                   bool isCallValid,
                                   UniformLocation locationPacked,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLdouble *value,
                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix2x3dv_value(const State &glState,
                                     bool isCallValid,
                                     UniformLocation locationPacked,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix2x4dv_value(const State &glState,
                                     bool isCallValid,
                                     UniformLocation locationPacked,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix3dv_value(const State &glState,
                                   bool isCallValid,
                                   UniformLocation locationPacked,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLdouble *value,
                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix3x2dv_value(const State &glState,
                                     bool isCallValid,
                                     UniformLocation locationPacked,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix3x4dv_value(const State &glState,
                                     bool isCallValid,
                                     UniformLocation locationPacked,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix4dv_value(const State &glState,
                                   bool isCallValid,
                                   UniformLocation locationPacked,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLdouble *value,
                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix4x2dv_value(const State &glState,
                                     bool isCallValid,
                                     UniformLocation locationPacked,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformMatrix4x3dv_value(const State &glState,
                                     bool isCallValid,
                                     UniformLocation locationPacked,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureUniformSubroutinesuiv_indices(const State &glState,
                                          bool isCallValid,
                                          GLenum shadertype,
                                          GLsizei count,
                                          const GLuint *indices,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 4.1
void CaptureDepthRangeArrayv_v(const State &glState,
                               bool isCallValid,
                               GLuint first,
                               GLsizei count,
                               const GLdouble *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetDoublei_v_data(const State &glState,
                              bool isCallValid,
                              GLenum target,
                              GLuint index,
                              GLdouble *data,
                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetFloati_v_data(const State &glState,
                             bool isCallValid,
                             GLenum target,
                             GLuint index,
                             GLfloat *data,
                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetVertexAttribLdv_params(const State &glState,
                                      bool isCallValid,
                                      GLuint index,
                                      GLenum pname,
                                      GLdouble *params,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniform1dv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID programPacked,
                                    UniformLocation locationPacked,
                                    GLsizei count,
                                    const GLdouble *value,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniform2dv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID programPacked,
                                    UniformLocation locationPacked,
                                    GLsizei count,
                                    const GLdouble *value,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniform3dv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID programPacked,
                                    UniformLocation locationPacked,
                                    GLsizei count,
                                    const GLdouble *value,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniform4dv_value(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID programPacked,
                                    UniformLocation locationPacked,
                                    GLsizei count,
                                    const GLdouble *value,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix2dv_value(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLdouble *value,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix2x3dv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID programPacked,
                                            UniformLocation locationPacked,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLdouble *value,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix2x4dv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID programPacked,
                                            UniformLocation locationPacked,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLdouble *value,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix3dv_value(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLdouble *value,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix3x2dv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID programPacked,
                                            UniformLocation locationPacked,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLdouble *value,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix3x4dv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID programPacked,
                                            UniformLocation locationPacked,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLdouble *value,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix4dv_value(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLdouble *value,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix4x2dv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID programPacked,
                                            UniformLocation locationPacked,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLdouble *value,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureProgramUniformMatrix4x3dv_value(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID programPacked,
                                            UniformLocation locationPacked,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLdouble *value,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureScissorArrayv_v(const State &glState,
                            bool isCallValid,
                            GLuint first,
                            GLsizei count,
                            const GLint *v,
                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureScissorIndexedv_v(const State &glState,
                              bool isCallValid,
                              GLuint index,
                              const GLint *v,
                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribL1dv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLdouble *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribL2dv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLdouble *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribL3dv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLdouble *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribL4dv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLdouble *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribLPointer_pointer(const State &glState,
                                         bool isCallValid,
                                         GLuint index,
                                         GLint size,
                                         GLenum type,
                                         GLsizei stride,
                                         const void *pointer,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureViewportArrayv_v(const State &glState,
                             bool isCallValid,
                             GLuint first,
                             GLsizei count,
                             const GLfloat *v,
                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureViewportIndexedfv_v(const State &glState,
                                bool isCallValid,
                                GLuint index,
                                const GLfloat *v,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 4.2
void CaptureDrawElementsInstancedBaseInstance_indices(const State &glState,
                                                      bool isCallValid,
                                                      PrimitiveMode modePacked,
                                                      GLsizei count,
                                                      DrawElementsType typePacked,
                                                      const void *indices,
                                                      GLsizei instancecount,
                                                      GLuint baseinstance,
                                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureDrawElementsInstancedBaseVertexBaseInstance_indices(const State &glState,
                                                                bool isCallValid,
                                                                PrimitiveMode modePacked,
                                                                GLsizei count,
                                                                DrawElementsType typePacked,
                                                                const void *indices,
                                                                GLsizei instancecount,
                                                                GLint basevertex,
                                                                GLuint baseinstance,
                                                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetActiveAtomicCounterBufferiv_params(const State &glState,
                                                  bool isCallValid,
                                                  ShaderProgramID programPacked,
                                                  GLuint bufferIndex,
                                                  GLenum pname,
                                                  GLint *params,
                                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 4.3
void CaptureClearBufferData_data(const State &glState,
                                 bool isCallValid,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLenum format,
                                 GLenum type,
                                 const void *data,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureClearBufferSubData_data(const State &glState,
                                    bool isCallValid,
                                    GLenum target,
                                    GLenum internalformat,
                                    GLintptr offset,
                                    GLsizeiptr size,
                                    GLenum format,
                                    GLenum type,
                                    const void *data,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetInternalformati64v_params(const State &glState,
                                         bool isCallValid,
                                         GLenum target,
                                         GLenum internalformat,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLint64 *params,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetProgramResourceLocationIndex_name(const State &glState,
                                                 bool isCallValid,
                                                 ShaderProgramID programPacked,
                                                 GLenum programInterface,
                                                 const GLchar *name,
                                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiDrawArraysIndirect_indirect(const State &glState,
                                             bool isCallValid,
                                             PrimitiveMode modePacked,
                                             const void *indirect,
                                             GLsizei drawcount,
                                             GLsizei stride,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiDrawElementsIndirect_indirect(const State &glState,
                                               bool isCallValid,
                                               PrimitiveMode modePacked,
                                               DrawElementsType typePacked,
                                               const void *indirect,
                                               GLsizei drawcount,
                                               GLsizei stride,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 4.4
void CaptureBindBuffersBase_buffersPacked(const State &glState,
                                          bool isCallValid,
                                          GLenum target,
                                          GLuint first,
                                          GLsizei count,
                                          const BufferID *buffersPacked,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindBuffersRange_buffersPacked(const State &glState,
                                           bool isCallValid,
                                           GLenum target,
                                           GLuint first,
                                           GLsizei count,
                                           const BufferID *buffersPacked,
                                           const GLintptr *offsets,
                                           const GLsizeiptr *sizes,
                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindBuffersRange_offsets(const State &glState,
                                     bool isCallValid,
                                     GLenum target,
                                     GLuint first,
                                     GLsizei count,
                                     const BufferID *buffersPacked,
                                     const GLintptr *offsets,
                                     const GLsizeiptr *sizes,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindBuffersRange_sizes(const State &glState,
                                   bool isCallValid,
                                   GLenum target,
                                   GLuint first,
                                   GLsizei count,
                                   const BufferID *buffersPacked,
                                   const GLintptr *offsets,
                                   const GLsizeiptr *sizes,
                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindImageTextures_textures(const State &glState,
                                       bool isCallValid,
                                       GLuint first,
                                       GLsizei count,
                                       const GLuint *textures,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindSamplers_samplers(const State &glState,
                                  bool isCallValid,
                                  GLuint first,
                                  GLsizei count,
                                  const GLuint *samplers,
                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindTextures_textures(const State &glState,
                                  bool isCallValid,
                                  GLuint first,
                                  GLsizei count,
                                  const GLuint *textures,
                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindVertexBuffers_buffersPacked(const State &glState,
                                            bool isCallValid,
                                            GLuint first,
                                            GLsizei count,
                                            const BufferID *buffersPacked,
                                            const GLintptr *offsets,
                                            const GLsizei *strides,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindVertexBuffers_offsets(const State &glState,
                                      bool isCallValid,
                                      GLuint first,
                                      GLsizei count,
                                      const BufferID *buffersPacked,
                                      const GLintptr *offsets,
                                      const GLsizei *strides,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBindVertexBuffers_strides(const State &glState,
                                      bool isCallValid,
                                      GLuint first,
                                      GLsizei count,
                                      const BufferID *buffersPacked,
                                      const GLintptr *offsets,
                                      const GLsizei *strides,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureBufferStorage_data(const State &glState,
                               bool isCallValid,
                               BufferBinding targetPacked,
                               GLsizeiptr size,
                               const void *data,
                               GLbitfield flags,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureClearTexImage_data(const State &glState,
                               bool isCallValid,
                               TextureID texturePacked,
                               GLint level,
                               GLenum format,
                               GLenum type,
                               const void *data,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureClearTexSubImage_data(const State &glState,
                                  bool isCallValid,
                                  TextureID texturePacked,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLenum type,
                                  const void *data,
                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 4.5
void CaptureClearNamedBufferData_data(const State &glState,
                                      bool isCallValid,
                                      BufferID bufferPacked,
                                      GLenum internalformat,
                                      GLenum format,
                                      GLenum type,
                                      const void *data,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureClearNamedBufferSubData_data(const State &glState,
                                         bool isCallValid,
                                         BufferID bufferPacked,
                                         GLenum internalformat,
                                         GLintptr offset,
                                         GLsizeiptr size,
                                         GLenum format,
                                         GLenum type,
                                         const void *data,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureClearNamedFramebufferfv_value(const State &glState,
                                          bool isCallValid,
                                          FramebufferID framebufferPacked,
                                          GLenum buffer,
                                          GLint drawbuffer,
                                          const GLfloat *value,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureClearNamedFramebufferiv_value(const State &glState,
                                          bool isCallValid,
                                          FramebufferID framebufferPacked,
                                          GLenum buffer,
                                          GLint drawbuffer,
                                          const GLint *value,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureClearNamedFramebufferuiv_value(const State &glState,
                                           bool isCallValid,
                                           FramebufferID framebufferPacked,
                                           GLenum buffer,
                                           GLint drawbuffer,
                                           const GLuint *value,
                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCompressedTextureSubImage1D_data(const State &glState,
                                             bool isCallValid,
                                             TextureID texturePacked,
                                             GLint level,
                                             GLint xoffset,
                                             GLsizei width,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void *data,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCompressedTextureSubImage2D_data(const State &glState,
                                             bool isCallValid,
                                             TextureID texturePacked,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void *data,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCompressedTextureSubImage3D_data(const State &glState,
                                             bool isCallValid,
                                             TextureID texturePacked,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void *data,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateBuffers_buffersPacked(const State &glState,
                                        bool isCallValid,
                                        GLsizei n,
                                        BufferID *buffersPacked,
                                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateFramebuffers_framebuffers(const State &glState,
                                            bool isCallValid,
                                            GLsizei n,
                                            GLuint *framebuffers,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateProgramPipelines_pipelines(const State &glState,
                                             bool isCallValid,
                                             GLsizei n,
                                             GLuint *pipelines,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateQueries_ids(const State &glState,
                              bool isCallValid,
                              GLenum target,
                              GLsizei n,
                              GLuint *ids,
                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateRenderbuffers_renderbuffersPacked(const State &glState,
                                                    bool isCallValid,
                                                    GLsizei n,
                                                    RenderbufferID *renderbuffersPacked,
                                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateSamplers_samplers(const State &glState,
                                    bool isCallValid,
                                    GLsizei n,
                                    GLuint *samplers,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateTextures_textures(const State &glState,
                                    bool isCallValid,
                                    GLenum target,
                                    GLsizei n,
                                    GLuint *textures,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateTransformFeedbacks_ids(const State &glState,
                                         bool isCallValid,
                                         GLsizei n,
                                         GLuint *ids,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureCreateVertexArrays_arraysPacked(const State &glState,
                                            bool isCallValid,
                                            GLsizei n,
                                            VertexArrayID *arraysPacked,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetCompressedTextureImage_pixels(const State &glState,
                                             bool isCallValid,
                                             TextureID texturePacked,
                                             GLint level,
                                             GLsizei bufSize,
                                             void *pixels,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetCompressedTextureSubImage_pixels(const State &glState,
                                                bool isCallValid,
                                                TextureID texturePacked,
                                                GLint level,
                                                GLint xoffset,
                                                GLint yoffset,
                                                GLint zoffset,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLsizei bufSize,
                                                void *pixels,
                                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetNamedBufferParameteri64v_params(const State &glState,
                                               bool isCallValid,
                                               BufferID bufferPacked,
                                               GLenum pname,
                                               GLint64 *params,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetNamedBufferParameteriv_params(const State &glState,
                                             bool isCallValid,
                                             BufferID bufferPacked,
                                             GLenum pname,
                                             GLint *params,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetNamedBufferPointerv_params(const State &glState,
                                          bool isCallValid,
                                          BufferID bufferPacked,
                                          GLenum pname,
                                          void **params,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetNamedBufferSubData_data(const State &glState,
                                       bool isCallValid,
                                       BufferID bufferPacked,
                                       GLintptr offset,
                                       GLsizeiptr size,
                                       void *data,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetNamedFramebufferAttachmentParameteriv_params(const State &glState,
                                                            bool isCallValid,
                                                            FramebufferID framebufferPacked,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLint *params,
                                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetNamedFramebufferParameteriv_param(const State &glState,
                                                 bool isCallValid,
                                                 FramebufferID framebufferPacked,
                                                 GLenum pname,
                                                 GLint *param,
                                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetNamedRenderbufferParameteriv_params(const State &glState,
                                                   bool isCallValid,
                                                   RenderbufferID renderbufferPacked,
                                                   GLenum pname,
                                                   GLint *params,
                                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTextureImage_pixels(const State &glState,
                                   bool isCallValid,
                                   TextureID texturePacked,
                                   GLint level,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   void *pixels,
                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTextureLevelParameterfv_params(const State &glState,
                                              bool isCallValid,
                                              TextureID texturePacked,
                                              GLint level,
                                              GLenum pname,
                                              GLfloat *params,
                                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTextureLevelParameteriv_params(const State &glState,
                                              bool isCallValid,
                                              TextureID texturePacked,
                                              GLint level,
                                              GLenum pname,
                                              GLint *params,
                                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTextureParameterIiv_params(const State &glState,
                                          bool isCallValid,
                                          TextureID texturePacked,
                                          GLenum pname,
                                          GLint *params,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTextureParameterIuiv_params(const State &glState,
                                           bool isCallValid,
                                           TextureID texturePacked,
                                           GLenum pname,
                                           GLuint *params,
                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTextureParameterfv_params(const State &glState,
                                         bool isCallValid,
                                         TextureID texturePacked,
                                         GLenum pname,
                                         GLfloat *params,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTextureParameteriv_params(const State &glState,
                                         bool isCallValid,
                                         TextureID texturePacked,
                                         GLenum pname,
                                         GLint *params,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTextureSubImage_pixels(const State &glState,
                                      bool isCallValid,
                                      TextureID texturePacked,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei bufSize,
                                      void *pixels,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTransformFeedbacki64_v_param(const State &glState,
                                            bool isCallValid,
                                            GLuint xfb,
                                            GLenum pname,
                                            GLuint index,
                                            GLint64 *param,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTransformFeedbacki_v_param(const State &glState,
                                          bool isCallValid,
                                          GLuint xfb,
                                          GLenum pname,
                                          GLuint index,
                                          GLint *param,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetTransformFeedbackiv_param(const State &glState,
                                         bool isCallValid,
                                         GLuint xfb,
                                         GLenum pname,
                                         GLint *param,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetVertexArrayIndexed64iv_param(const State &glState,
                                            bool isCallValid,
                                            VertexArrayID vaobjPacked,
                                            GLuint index,
                                            GLenum pname,
                                            GLint64 *param,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetVertexArrayIndexediv_param(const State &glState,
                                          bool isCallValid,
                                          VertexArrayID vaobjPacked,
                                          GLuint index,
                                          GLenum pname,
                                          GLint *param,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetVertexArrayiv_param(const State &glState,
                                   bool isCallValid,
                                   VertexArrayID vaobjPacked,
                                   GLenum pname,
                                   GLint *param,
                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnColorTable_table(const State &glState,
                                 bool isCallValid,
                                 GLenum target,
                                 GLenum format,
                                 GLenum type,
                                 GLsizei bufSize,
                                 void *table,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnCompressedTexImage_pixels(const State &glState,
                                          bool isCallValid,
                                          GLenum target,
                                          GLint lod,
                                          GLsizei bufSize,
                                          void *pixels,
                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnConvolutionFilter_image(const State &glState,
                                        bool isCallValid,
                                        GLenum target,
                                        GLenum format,
                                        GLenum type,
                                        GLsizei bufSize,
                                        void *image,
                                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnHistogram_values(const State &glState,
                                 bool isCallValid,
                                 GLenum target,
                                 GLboolean reset,
                                 GLenum format,
                                 GLenum type,
                                 GLsizei bufSize,
                                 void *values,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnMapdv_v(const State &glState,
                        bool isCallValid,
                        GLenum target,
                        GLenum query,
                        GLsizei bufSize,
                        GLdouble *v,
                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnMapfv_v(const State &glState,
                        bool isCallValid,
                        GLenum target,
                        GLenum query,
                        GLsizei bufSize,
                        GLfloat *v,
                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnMapiv_v(const State &glState,
                        bool isCallValid,
                        GLenum target,
                        GLenum query,
                        GLsizei bufSize,
                        GLint *v,
                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnMinmax_values(const State &glState,
                              bool isCallValid,
                              GLenum target,
                              GLboolean reset,
                              GLenum format,
                              GLenum type,
                              GLsizei bufSize,
                              void *values,
                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnPixelMapfv_values(const State &glState,
                                  bool isCallValid,
                                  GLenum map,
                                  GLsizei bufSize,
                                  GLfloat *values,
                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnPixelMapuiv_values(const State &glState,
                                   bool isCallValid,
                                   GLenum map,
                                   GLsizei bufSize,
                                   GLuint *values,
                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnPixelMapusv_values(const State &glState,
                                   bool isCallValid,
                                   GLenum map,
                                   GLsizei bufSize,
                                   GLushort *values,
                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnPolygonStipple_pattern(const State &glState,
                                       bool isCallValid,
                                       GLsizei bufSize,
                                       GLubyte *pattern,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnSeparableFilter_row(const State &glState,
                                    bool isCallValid,
                                    GLenum target,
                                    GLenum format,
                                    GLenum type,
                                    GLsizei rowBufSize,
                                    void *row,
                                    GLsizei columnBufSize,
                                    void *column,
                                    void *span,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnSeparableFilter_column(const State &glState,
                                       bool isCallValid,
                                       GLenum target,
                                       GLenum format,
                                       GLenum type,
                                       GLsizei rowBufSize,
                                       void *row,
                                       GLsizei columnBufSize,
                                       void *column,
                                       void *span,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnSeparableFilter_span(const State &glState,
                                     bool isCallValid,
                                     GLenum target,
                                     GLenum format,
                                     GLenum type,
                                     GLsizei rowBufSize,
                                     void *row,
                                     GLsizei columnBufSize,
                                     void *column,
                                     void *span,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnTexImage_pixels(const State &glState,
                                bool isCallValid,
                                GLenum target,
                                GLint level,
                                GLenum format,
                                GLenum type,
                                GLsizei bufSize,
                                void *pixels,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetnUniformdv_params(const State &glState,
                                 bool isCallValid,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLsizei bufSize,
                                 GLdouble *params,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureInvalidateNamedFramebufferData_attachments(const State &glState,
                                                       bool isCallValid,
                                                       FramebufferID framebufferPacked,
                                                       GLsizei numAttachments,
                                                       const GLenum *attachments,
                                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureInvalidateNamedFramebufferSubData_attachments(const State &glState,
                                                          bool isCallValid,
                                                          FramebufferID framebufferPacked,
                                                          GLsizei numAttachments,
                                                          const GLenum *attachments,
                                                          GLint x,
                                                          GLint y,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureNamedBufferData_data(const State &glState,
                                 bool isCallValid,
                                 BufferID bufferPacked,
                                 GLsizeiptr size,
                                 const void *data,
                                 GLenum usage,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureNamedBufferStorage_data(const State &glState,
                                    bool isCallValid,
                                    BufferID bufferPacked,
                                    GLsizeiptr size,
                                    const void *data,
                                    GLbitfield flags,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureNamedBufferSubData_data(const State &glState,
                                    bool isCallValid,
                                    BufferID bufferPacked,
                                    GLintptr offset,
                                    GLsizeiptr size,
                                    const void *data,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureNamedFramebufferDrawBuffers_bufs(const State &glState,
                                             bool isCallValid,
                                             FramebufferID framebufferPacked,
                                             GLsizei n,
                                             const GLenum *bufs,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTextureParameterIiv_params(const State &glState,
                                       bool isCallValid,
                                       TextureID texturePacked,
                                       GLenum pname,
                                       const GLint *params,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTextureParameterIuiv_params(const State &glState,
                                        bool isCallValid,
                                        TextureID texturePacked,
                                        GLenum pname,
                                        const GLuint *params,
                                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTextureParameterfv_param(const State &glState,
                                     bool isCallValid,
                                     TextureID texturePacked,
                                     GLenum pname,
                                     const GLfloat *param,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTextureParameteriv_param(const State &glState,
                                     bool isCallValid,
                                     TextureID texturePacked,
                                     GLenum pname,
                                     const GLint *param,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTextureSubImage1D_pixels(const State &glState,
                                     bool isCallValid,
                                     TextureID texturePacked,
                                     GLint level,
                                     GLint xoffset,
                                     GLsizei width,
                                     GLenum format,
                                     GLenum type,
                                     const void *pixels,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTextureSubImage2D_pixels(const State &glState,
                                     bool isCallValid,
                                     TextureID texturePacked,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLenum format,
                                     GLenum type,
                                     const void *pixels,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTextureSubImage3D_pixels(const State &glState,
                                     bool isCallValid,
                                     TextureID texturePacked,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     GLenum type,
                                     const void *pixels,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexArrayVertexBuffers_buffersPacked(const State &glState,
                                                   bool isCallValid,
                                                   VertexArrayID vaobjPacked,
                                                   GLuint first,
                                                   GLsizei count,
                                                   const BufferID *buffersPacked,
                                                   const GLintptr *offsets,
                                                   const GLsizei *strides,
                                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexArrayVertexBuffers_offsets(const State &glState,
                                             bool isCallValid,
                                             VertexArrayID vaobjPacked,
                                             GLuint first,
                                             GLsizei count,
                                             const BufferID *buffersPacked,
                                             const GLintptr *offsets,
                                             const GLsizei *strides,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexArrayVertexBuffers_strides(const State &glState,
                                             bool isCallValid,
                                             VertexArrayID vaobjPacked,
                                             GLuint first,
                                             GLsizei count,
                                             const BufferID *buffersPacked,
                                             const GLintptr *offsets,
                                             const GLsizei *strides,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 4.6
void CaptureMultiDrawArraysIndirectCount_indirect(const State &glState,
                                                  bool isCallValid,
                                                  GLenum mode,
                                                  const void *indirect,
                                                  GLintptr drawcount,
                                                  GLsizei maxdrawcount,
                                                  GLsizei stride,
                                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiDrawElementsIndirectCount_indirect(const State &glState,
                                                    bool isCallValid,
                                                    GLenum mode,
                                                    GLenum type,
                                                    const void *indirect,
                                                    GLintptr drawcount,
                                                    GLsizei maxdrawcount,
                                                    GLsizei stride,
                                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureSpecializeShader_pEntryPoint(const State &glState,
                                         bool isCallValid,
                                         GLuint shader,
                                         const GLchar *pEntryPoint,
                                         GLuint numSpecializationConstants,
                                         const GLuint *pConstantIndex,
                                         const GLuint *pConstantValue,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureSpecializeShader_pConstantIndex(const State &glState,
                                            bool isCallValid,
                                            GLuint shader,
                                            const GLchar *pEntryPoint,
                                            GLuint numSpecializationConstants,
                                            const GLuint *pConstantIndex,
                                            const GLuint *pConstantValue,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureSpecializeShader_pConstantValue(const State &glState,
                                            bool isCallValid,
                                            GLuint shader,
                                            const GLchar *pEntryPoint,
                                            GLuint numSpecializationConstants,
                                            const GLuint *pConstantIndex,
                                            const GLuint *pConstantValue,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
}  // namespace gl
