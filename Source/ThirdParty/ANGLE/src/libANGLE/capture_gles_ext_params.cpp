//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_glesext_params.cpp:
//   Pointer parameter capture functions for the OpenGL ES extension entry points.

#include "libANGLE/capture_gles_ext_autogen.h"

#include "libANGLE/capture_gles_2_0_autogen.h"
#include "libANGLE/capture_gles_3_0_autogen.h"
#include "libANGLE/capture_gles_3_2_autogen.h"

using namespace angle;

namespace gl
{
void CaptureDrawElementsInstancedBaseVertexBaseInstanceANGLE_indices(
    const State &glState,
    bool isCallValid,
    PrimitiveMode modePacked,
    GLsizei count,
    DrawElementsType typePacked,
    const GLvoid *indices,
    GLsizei instanceCounts,
    GLint baseVertex,
    GLuint baseInstance,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedBaseInstanceANGLE_firsts(const State &glState,
                                                             bool isCallValid,
                                                             PrimitiveMode modePacked,
                                                             const GLint *firsts,
                                                             const GLsizei *counts,
                                                             const GLsizei *instanceCounts,
                                                             const GLuint *baseInstances,
                                                             GLsizei drawcount,
                                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}  // namespace gl

void CaptureMultiDrawArraysInstancedBaseInstanceANGLE_counts(const State &glState,
                                                             bool isCallValid,
                                                             PrimitiveMode modePacked,
                                                             const GLint *firsts,
                                                             const GLsizei *counts,
                                                             const GLsizei *instanceCounts,
                                                             const GLuint *baseInstances,
                                                             GLsizei drawcount,
                                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedBaseInstanceANGLE_instanceCounts(
    const State &glState,
    bool isCallValid,
    PrimitiveMode modePacked,
    const GLint *firsts,
    const GLsizei *counts,
    const GLsizei *instanceCounts,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedBaseInstanceANGLE_baseInstances(
    const State &glState,
    bool isCallValid,
    PrimitiveMode modePacked,
    const GLint *firsts,
    const GLsizei *counts,
    const GLsizei *instanceCounts,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_counts(
    const State &glState,
    bool isCallValid,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_indices(
    const State &glState,
    bool isCallValid,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_instanceCounts(
    const State &glState,
    bool isCallValid,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_baseVertices(
    const State &glState,
    bool isCallValid,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_baseInstances(
    const State &glState,
    bool isCallValid,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawElementsInstancedANGLE_indices(const State &glState,
                                               bool isCallValid,
                                               PrimitiveMode modePacked,
                                               GLsizei count,
                                               DrawElementsType typePacked,
                                               const void *indices,
                                               GLsizei primcount,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawElementsBaseVertexEXT_indices(const State &glState,
                                              bool isCallValid,
                                              PrimitiveMode modePacked,
                                              GLsizei count,
                                              DrawElementsType typePacked,
                                              const void *indices,
                                              GLint basevertex,
                                              ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureDrawElementsInstancedBaseVertexEXT_indices(const State &glState,
                                                       bool isCallValid,
                                                       PrimitiveMode modePacked,
                                                       GLsizei count,
                                                       DrawElementsType typePacked,
                                                       const void *indices,
                                                       GLsizei instancecount,
                                                       GLint basevertex,
                                                       ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureDrawRangeElementsBaseVertexEXT_indices(const State &glState,
                                                   bool isCallValid,
                                                   PrimitiveMode modePacked,
                                                   GLuint start,
                                                   GLuint end,
                                                   GLsizei count,
                                                   DrawElementsType typePacked,
                                                   const void *indices,
                                                   GLint basevertex,
                                                   ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsBaseVertexEXT_count(const State &glState,
                                                 bool isCallValid,
                                                 PrimitiveMode modePacked,
                                                 const GLsizei *count,
                                                 DrawElementsType typePacked,
                                                 const void *const *indices,
                                                 GLsizei drawcount,
                                                 const GLint *basevertex,
                                                 ParamCapture *countParam)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsBaseVertexEXT_indices(const State &glState,
                                                   bool isCallValid,
                                                   PrimitiveMode modePacked,
                                                   const GLsizei *count,
                                                   DrawElementsType typePacked,
                                                   const void *const *indices,
                                                   GLsizei drawcount,
                                                   const GLint *basevertex,
                                                   ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsBaseVertexEXT_basevertex(const State &glState,
                                                      bool isCallValid,
                                                      PrimitiveMode modePacked,
                                                      const GLsizei *count,
                                                      DrawElementsType typePacked,
                                                      const void *const *indices,
                                                      GLsizei drawcount,
                                                      const GLint *basevertex,
                                                      ParamCapture *basevertexParam)
{
    UNIMPLEMENTED();
}

void CaptureDrawElementsBaseVertexOES_indices(const State &glState,
                                              bool isCallValid,
                                              PrimitiveMode modePacked,
                                              GLsizei count,
                                              DrawElementsType typePacked,
                                              const void *indices,
                                              GLint basevertex,
                                              ParamCapture *indicesParam)
{
    CaptureDrawElements_indices(glState, isCallValid, modePacked, count, typePacked, indices,
                                indicesParam);
}

void CaptureDrawElementsInstancedBaseVertexOES_indices(const State &glState,
                                                       bool isCallValid,
                                                       PrimitiveMode modePacked,
                                                       GLsizei count,
                                                       DrawElementsType typePacked,
                                                       const void *indices,
                                                       GLsizei instancecount,
                                                       GLint basevertex,
                                                       ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureDrawRangeElementsBaseVertexOES_indices(const State &glState,
                                                   bool isCallValid,
                                                   PrimitiveMode modePacked,
                                                   GLuint start,
                                                   GLuint end,
                                                   GLsizei count,
                                                   DrawElementsType typePacked,
                                                   const void *indices,
                                                   GLint basevertex,
                                                   ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysANGLE_firsts(const State &glState,
                                        bool isCallValid,
                                        PrimitiveMode modePacked,
                                        const GLint *firsts,
                                        const GLsizei *counts,
                                        GLsizei drawcount,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysANGLE_counts(const State &glState,
                                        bool isCallValid,
                                        PrimitiveMode modePacked,
                                        const GLint *firsts,
                                        const GLsizei *counts,
                                        GLsizei drawcount,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedANGLE_firsts(const State &glState,
                                                 bool isCallValid,
                                                 PrimitiveMode modePacked,
                                                 const GLint *firsts,
                                                 const GLsizei *counts,
                                                 const GLsizei *instanceCounts,
                                                 GLsizei drawcount,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedANGLE_counts(const State &glState,
                                                 bool isCallValid,
                                                 PrimitiveMode modePacked,
                                                 const GLint *firsts,
                                                 const GLsizei *counts,
                                                 const GLsizei *instanceCounts,
                                                 GLsizei drawcount,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedANGLE_instanceCounts(const State &glState,
                                                         bool isCallValid,
                                                         PrimitiveMode modePacked,
                                                         const GLint *firsts,
                                                         const GLsizei *counts,
                                                         const GLsizei *instanceCounts,
                                                         GLsizei drawcount,
                                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsANGLE_counts(const State &glState,
                                          bool isCallValid,
                                          PrimitiveMode modePacked,
                                          const GLsizei *counts,
                                          DrawElementsType typePacked,
                                          const GLvoid *const *indices,
                                          GLsizei drawcount,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsANGLE_indices(const State &glState,
                                           bool isCallValid,
                                           PrimitiveMode modePacked,
                                           const GLsizei *counts,
                                           DrawElementsType typePacked,
                                           const GLvoid *const *indices,
                                           GLsizei drawcount,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedANGLE_counts(const State &glState,
                                                   bool isCallValid,
                                                   PrimitiveMode modePacked,
                                                   const GLsizei *counts,
                                                   DrawElementsType typePacked,
                                                   const GLvoid *const *indices,
                                                   const GLsizei *instanceCounts,
                                                   GLsizei drawcount,
                                                   ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedANGLE_indices(const State &glState,
                                                    bool isCallValid,
                                                    PrimitiveMode modePacked,
                                                    const GLsizei *counts,
                                                    DrawElementsType typePacked,
                                                    const GLvoid *const *indices,
                                                    const GLsizei *instanceCounts,
                                                    GLsizei drawcount,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedANGLE_instanceCounts(const State &glState,
                                                           bool isCallValid,
                                                           PrimitiveMode modePacked,
                                                           const GLsizei *counts,
                                                           DrawElementsType typePacked,
                                                           const GLvoid *const *indices,
                                                           const GLsizei *instanceCounts,
                                                           GLsizei drawcount,
                                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureRequestExtensionANGLE_name(const State &glState,
                                       bool isCallValid,
                                       const GLchar *name,
                                       ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureDisableExtensionANGLE_name(const State &glState,
                                       bool isCallValid,
                                       const GLchar *name,
                                       ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureGetBooleanvRobustANGLE_length(const State &glState,
                                          bool isCallValid,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLboolean *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBooleanvRobustANGLE_params(const State &glState,
                                          bool isCallValid,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLboolean *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferParameterivRobustANGLE_length(const State &glState,
                                                   bool isCallValid,
                                                   BufferBinding targetPacked,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params,
                                                   ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferParameterivRobustANGLE_params(const State &glState,
                                                   bool isCallValid,
                                                   BufferBinding targetPacked,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params,
                                                   ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFloatvRobustANGLE_length(const State &glState,
                                        bool isCallValid,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLfloat *params,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFloatvRobustANGLE_params(const State &glState,
                                        bool isCallValid,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLfloat *params,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferAttachmentParameterivRobustANGLE_length(const State &glState,
                                                                  bool isCallValid,
                                                                  GLenum target,
                                                                  GLenum attachment,
                                                                  GLenum pname,
                                                                  GLsizei bufSize,
                                                                  GLsizei *length,
                                                                  GLint *params,
                                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferAttachmentParameterivRobustANGLE_params(const State &glState,
                                                                  bool isCallValid,
                                                                  GLenum target,
                                                                  GLenum attachment,
                                                                  GLenum pname,
                                                                  GLsizei bufSize,
                                                                  GLsizei *length,
                                                                  GLint *params,
                                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetIntegervRobustANGLE_length(const State &glState,
                                          bool isCallValid,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *data,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetIntegervRobustANGLE_data(const State &glState,
                                        bool isCallValid,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint *data,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramivRobustANGLE_length(const State &glState,
                                           bool isCallValid,
                                           ShaderProgramID program,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramivRobustANGLE_params(const State &glState,
                                           bool isCallValid,
                                           ShaderProgramID program,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetRenderbufferParameterivRobustANGLE_length(const State &glState,
                                                         bool isCallValid,
                                                         GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei *length,
                                                         GLint *params,
                                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetRenderbufferParameterivRobustANGLE_params(const State &glState,
                                                         bool isCallValid,
                                                         GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei *length,
                                                         GLint *params,
                                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetShaderivRobustANGLE_length(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID shader,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetShaderivRobustANGLE_params(const State &glState,
                                          bool isCallValid,
                                          ShaderProgramID shader,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterfvRobustANGLE_length(const State &glState,
                                                bool isCallValid,
                                                TextureType targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLfloat *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterfvRobustANGLE_params(const State &glState,
                                                bool isCallValid,
                                                TextureType targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLfloat *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterivRobustANGLE_length(const State &glState,
                                                bool isCallValid,
                                                TextureType targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterivRobustANGLE_params(const State &glState,
                                                bool isCallValid,
                                                TextureType targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformfvRobustANGLE_length(const State &glState,
                                           bool isCallValid,
                                           ShaderProgramID program,
                                           UniformLocation location,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLfloat *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformfvRobustANGLE_params(const State &glState,
                                           bool isCallValid,
                                           ShaderProgramID program,
                                           UniformLocation location,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLfloat *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformivRobustANGLE_length(const State &glState,
                                           bool isCallValid,
                                           ShaderProgramID program,
                                           UniformLocation location,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformivRobustANGLE_params(const State &glState,
                                           bool isCallValid,
                                           ShaderProgramID program,
                                           UniformLocation location,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribfvRobustANGLE_length(const State &glState,
                                                bool isCallValid,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLfloat *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribfvRobustANGLE_params(const State &glState,
                                                bool isCallValid,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLfloat *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribivRobustANGLE_length(const State &glState,
                                                bool isCallValid,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribivRobustANGLE_params(const State &glState,
                                                bool isCallValid,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribPointervRobustANGLE_length(const State &glState,
                                                      bool isCallValid,
                                                      GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      void **pointer,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribPointervRobustANGLE_pointer(const State &glState,
                                                       bool isCallValid,
                                                       GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       void **pointer,
                                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadPixelsRobustANGLE_length(const State &glState,
                                         bool isCallValid,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLsizei *columns,
                                         GLsizei *rows,
                                         void *pixels,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadPixelsRobustANGLE_columns(const State &glState,
                                          bool isCallValid,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLsizei *columns,
                                          GLsizei *rows,
                                          void *pixels,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadPixelsRobustANGLE_rows(const State &glState,
                                       bool isCallValid,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLenum type,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLsizei *columns,
                                       GLsizei *rows,
                                       void *pixels,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadPixelsRobustANGLE_pixels(const State &glState,
                                         bool isCallValid,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLsizei *columns,
                                         GLsizei *rows,
                                         void *pixels,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexImage2DRobustANGLE_pixels(const State &glState,
                                         bool isCallValid,
                                         TextureTarget targetPacked,
                                         GLint level,
                                         GLint internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         const void *pixels,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterfvRobustANGLE_params(const State &glState,
                                             bool isCallValid,
                                             TextureType targetPacked,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLfloat *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterivRobustANGLE_params(const State &glState,
                                             bool isCallValid,
                                             TextureType targetPacked,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLint *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexSubImage2DRobustANGLE_pixels(const State &glState,
                                            bool isCallValid,
                                            TextureTarget targetPacked,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            const void *pixels,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexImage3DRobustANGLE_pixels(const State &glState,
                                         bool isCallValid,
                                         TextureTarget targetPacked,
                                         GLint level,
                                         GLint internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         const void *pixels,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexSubImage3DRobustANGLE_pixels(const State &glState,
                                            bool isCallValid,
                                            TextureTarget targetPacked,
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
                                            const void *pixels,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCompressedTexImage2DRobustANGLE_data(const State &glState,
                                                 bool isCallValid,
                                                 TextureTarget targetPacked,
                                                 GLint level,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLint border,
                                                 GLsizei imageSize,
                                                 GLsizei dataSize,
                                                 const GLvoid *data,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCompressedTexSubImage2DRobustANGLE_data(const State &glState,
                                                    bool isCallValid,
                                                    TextureTarget targetPacked,
                                                    GLint level,
                                                    GLsizei xoffset,
                                                    GLsizei yoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLenum format,
                                                    GLsizei imageSize,
                                                    GLsizei dataSize,
                                                    const GLvoid *data,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCompressedTexImage3DRobustANGLE_data(const State &glState,
                                                 bool isCallValid,
                                                 TextureTarget targetPacked,
                                                 GLint level,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLint border,
                                                 GLsizei imageSize,
                                                 GLsizei dataSize,
                                                 const GLvoid *data,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCompressedTexSubImage3DRobustANGLE_data(const State &glState,
                                                    bool isCallValid,
                                                    TextureTarget targetPacked,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLint zoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLsizei depth,
                                                    GLenum format,
                                                    GLsizei imageSize,
                                                    GLsizei dataSize,
                                                    const GLvoid *data,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryivRobustANGLE_length(const State &glState,
                                         bool isCallValid,
                                         QueryType targetPacked,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryivRobustANGLE_params(const State &glState,
                                         bool isCallValid,
                                         QueryType targetPacked,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectuivRobustANGLE_length(const State &glState,
                                                bool isCallValid,
                                                QueryID id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLuint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectuivRobustANGLE_params(const State &glState,
                                                bool isCallValid,
                                                QueryID id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLuint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferPointervRobustANGLE_length(const State &glState,
                                                bool isCallValid,
                                                BufferBinding targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                void **params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferPointervRobustANGLE_params(const State &glState,
                                                bool isCallValid,
                                                BufferBinding targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                void **params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetIntegeri_vRobustANGLE_length(const State &glState,
                                            bool isCallValid,
                                            GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *data,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetIntegeri_vRobustANGLE_data(const State &glState,
                                          bool isCallValid,
                                          GLenum target,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *data,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInternalformativRobustANGLE_length(const State &glState,
                                                  bool isCallValid,
                                                  GLenum target,
                                                  GLenum internalformat,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInternalformativRobustANGLE_params(const State &glState,
                                                  bool isCallValid,
                                                  GLenum target,
                                                  GLenum internalformat,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribIivRobustANGLE_length(const State &glState,
                                                 bool isCallValid,
                                                 GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribIivRobustANGLE_params(const State &glState,
                                                 bool isCallValid,
                                                 GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribIuivRobustANGLE_length(const State &glState,
                                                  bool isCallValid,
                                                  GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribIuivRobustANGLE_params(const State &glState,
                                                  bool isCallValid,
                                                  GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformuivRobustANGLE_length(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformuivRobustANGLE_params(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetActiveUniformBlockivRobustANGLE_length(const State &glState,
                                                      bool isCallValid,
                                                      ShaderProgramID program,
                                                      GLuint uniformBlockIndex,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetActiveUniformBlockivRobustANGLE_params(const State &glState,
                                                      bool isCallValid,
                                                      ShaderProgramID program,
                                                      GLuint uniformBlockIndex,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInteger64vRobustANGLE_length(const State &glState,
                                            bool isCallValid,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint64 *data,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInteger64vRobustANGLE_data(const State &glState,
                                          bool isCallValid,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint64 *data,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInteger64i_vRobustANGLE_length(const State &glState,
                                              bool isCallValid,
                                              GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              GLint64 *data,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInteger64i_vRobustANGLE_data(const State &glState,
                                            bool isCallValid,
                                            GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint64 *data,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferParameteri64vRobustANGLE_length(const State &glState,
                                                     bool isCallValid,
                                                     BufferBinding targetPacked,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint64 *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferParameteri64vRobustANGLE_params(const State &glState,
                                                     bool isCallValid,
                                                     BufferBinding targetPacked,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint64 *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterivRobustANGLE_param(const State &glState,
                                                bool isCallValid,
                                                SamplerID sampler,
                                                GLuint pname,
                                                GLsizei bufSize,
                                                const GLint *param,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterfvRobustANGLE_param(const State &glState,
                                                bool isCallValid,
                                                SamplerID sampler,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLfloat *param,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterivRobustANGLE_length(const State &glState,
                                                    bool isCallValid,
                                                    SamplerID sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLint *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterivRobustANGLE_params(const State &glState,
                                                    bool isCallValid,
                                                    SamplerID sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLint *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterfvRobustANGLE_length(const State &glState,
                                                    bool isCallValid,
                                                    SamplerID sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLfloat *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterfvRobustANGLE_params(const State &glState,
                                                    bool isCallValid,
                                                    SamplerID sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLfloat *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferParameterivRobustANGLE_length(const State &glState,
                                                        bool isCallValid,
                                                        GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLint *params,
                                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferParameterivRobustANGLE_params(const State &glState,
                                                        bool isCallValid,
                                                        GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLint *params,
                                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramInterfaceivRobustANGLE_length(const State &glState,
                                                    bool isCallValid,
                                                    ShaderProgramID program,
                                                    GLenum programInterface,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLint *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramInterfaceivRobustANGLE_params(const State &glState,
                                                    bool isCallValid,
                                                    ShaderProgramID program,
                                                    GLenum programInterface,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLint *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBooleani_vRobustANGLE_length(const State &glState,
                                            bool isCallValid,
                                            GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLboolean *data,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBooleani_vRobustANGLE_data(const State &glState,
                                          bool isCallValid,
                                          GLenum target,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLboolean *data,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetMultisamplefvRobustANGLE_length(const State &glState,
                                               bool isCallValid,
                                               GLenum pname,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLfloat *val,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetMultisamplefvRobustANGLE_val(const State &glState,
                                            bool isCallValid,
                                            GLenum pname,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLfloat *val,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterivRobustANGLE_length(const State &glState,
                                                     bool isCallValid,
                                                     TextureTarget targetPacked,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterivRobustANGLE_params(const State &glState,
                                                     bool isCallValid,
                                                     TextureTarget targetPacked,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterfvRobustANGLE_length(const State &glState,
                                                     bool isCallValid,
                                                     TextureTarget targetPacked,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLfloat *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterfvRobustANGLE_params(const State &glState,
                                                     bool isCallValid,
                                                     TextureTarget targetPacked,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLfloat *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPointervRobustANGLERobustANGLE_length(const State &glState,
                                                     bool isCallValid,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     void **params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPointervRobustANGLERobustANGLE_params(const State &glState,
                                                     bool isCallValid,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     void **params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsRobustANGLE_length(const State &glState,
                                          bool isCallValid,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLsizei *columns,
                                          GLsizei *rows,
                                          void *data,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsRobustANGLE_columns(const State &glState,
                                           bool isCallValid,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLsizei *columns,
                                           GLsizei *rows,
                                           void *data,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsRobustANGLE_rows(const State &glState,
                                        bool isCallValid,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height,
                                        GLenum format,
                                        GLenum type,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLsizei *columns,
                                        GLsizei *rows,
                                        void *data,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsRobustANGLE_data(const State &glState,
                                        bool isCallValid,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height,
                                        GLenum format,
                                        GLenum type,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLsizei *columns,
                                        GLsizei *rows,
                                        void *data,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformfvRobustANGLE_length(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLfloat *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformfvRobustANGLE_params(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLfloat *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformivRobustANGLE_length(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformivRobustANGLE_params(const State &glState,
                                            bool isCallValid,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformuivRobustANGLE_length(const State &glState,
                                             bool isCallValid,
                                             ShaderProgramID program,
                                             UniformLocation location,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLuint *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformuivRobustANGLE_params(const State &glState,
                                             bool isCallValid,
                                             ShaderProgramID program,
                                             UniformLocation location,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLuint *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterIivRobustANGLE_params(const State &glState,
                                              bool isCallValid,
                                              TextureType targetPacked,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLint *params,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterIuivRobustANGLE_params(const State &glState,
                                               bool isCallValid,
                                               TextureType targetPacked,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLuint *params,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIivRobustANGLE_length(const State &glState,
                                                 bool isCallValid,
                                                 TextureType targetPacked,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIivRobustANGLE_params(const State &glState,
                                                 bool isCallValid,
                                                 TextureType targetPacked,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIuivRobustANGLE_length(const State &glState,
                                                  bool isCallValid,
                                                  TextureType targetPacked,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIuivRobustANGLE_params(const State &glState,
                                                  bool isCallValid,
                                                  TextureType targetPacked,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterIivRobustANGLE_param(const State &glState,
                                                 bool isCallValid,
                                                 SamplerID sampler,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLint *param,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterIuivRobustANGLE_param(const State &glState,
                                                  bool isCallValid,
                                                  SamplerID sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLuint *param,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIivRobustANGLE_length(const State &glState,
                                                     bool isCallValid,
                                                     SamplerID sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIivRobustANGLE_params(const State &glState,
                                                     bool isCallValid,
                                                     SamplerID sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIuivRobustANGLE_length(const State &glState,
                                                      bool isCallValid,
                                                      SamplerID sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLuint *params,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIuivRobustANGLE_params(const State &glState,
                                                      bool isCallValid,
                                                      SamplerID sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLuint *params,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectivRobustANGLE_length(const State &glState,
                                               bool isCallValid,
                                               QueryID id,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint *params,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectivRobustANGLE_params(const State &glState,
                                               bool isCallValid,
                                               QueryID id,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint *params,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjecti64vRobustANGLE_length(const State &glState,
                                                 bool isCallValid,
                                                 QueryID id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint64 *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjecti64vRobustANGLE_params(const State &glState,
                                                 bool isCallValid,
                                                 QueryID id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint64 *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectui64vRobustANGLE_length(const State &glState,
                                                  bool isCallValid,
                                                  QueryID id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint64 *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectui64vRobustANGLE_params(const State &glState,
                                                  bool isCallValid,
                                                  QueryID id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint64 *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterivANGLE_params(const State &glState,
                                               bool isCallValid,
                                               TextureTarget targetPacked,
                                               GLint level,
                                               GLenum pname,
                                               GLint *params,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterfvANGLE_params(const State &glState,
                                               bool isCallValid,
                                               TextureTarget targetPacked,
                                               GLint level,
                                               GLenum pname,
                                               GLfloat *params,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetMultisamplefvANGLE_val(const State &glState,
                                      bool isCallValid,
                                      GLenum pname,
                                      GLuint index,
                                      GLfloat *val,
                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTranslatedShaderSourceANGLE_length(const State &glState,
                                                  bool isCallValid,
                                                  ShaderProgramID shader,
                                                  GLsizei bufsize,
                                                  GLsizei *length,
                                                  GLchar *source,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTranslatedShaderSourceANGLE_source(const State &glState,
                                                  bool isCallValid,
                                                  ShaderProgramID shader,
                                                  GLsizei bufsize,
                                                  GLsizei *length,
                                                  GLchar *source,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureBindUniformLocationCHROMIUM_name(const State &glState,
                                             bool isCallValid,
                                             ShaderProgramID program,
                                             UniformLocation location,
                                             const GLchar *name,
                                             ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureMatrixLoadfCHROMIUM_matrix(const State &glState,
                                       bool isCallValid,
                                       GLenum matrixMode,
                                       const GLfloat *matrix,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CapturePathCommandsCHROMIUM_commands(const State &glState,
                                          bool isCallValid,
                                          PathID path,
                                          GLsizei numCommands,
                                          const GLubyte *commands,
                                          GLsizei numCoords,
                                          GLenum coordType,
                                          const void *coords,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CapturePathCommandsCHROMIUM_coords(const State &glState,
                                        bool isCallValid,
                                        PathID path,
                                        GLsizei numCommands,
                                        const GLubyte *commands,
                                        GLsizei numCoords,
                                        GLenum coordType,
                                        const void *coords,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPathParameterfvCHROMIUM_value(const State &glState,
                                             bool isCallValid,
                                             PathID path,
                                             GLenum pname,
                                             GLfloat *value,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPathParameterivCHROMIUM_value(const State &glState,
                                             bool isCallValid,
                                             PathID path,
                                             GLenum pname,
                                             GLint *value,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCoverFillPathInstancedCHROMIUM_paths(const State &glState,
                                                 bool isCallValid,
                                                 GLsizei numPath,
                                                 GLenum pathNameType,
                                                 const void *paths,
                                                 PathID pathBase,
                                                 GLenum coverMode,
                                                 GLenum transformType,
                                                 const GLfloat *transformValues,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCoverFillPathInstancedCHROMIUM_transformValues(const State &glState,
                                                           bool isCallValid,
                                                           GLsizei numPath,
                                                           GLenum pathNameType,
                                                           const void *paths,
                                                           PathID pathBase,
                                                           GLenum coverMode,
                                                           GLenum transformType,
                                                           const GLfloat *transformValues,
                                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCoverStrokePathInstancedCHROMIUM_paths(const State &glState,
                                                   bool isCallValid,
                                                   GLsizei numPath,
                                                   GLenum pathNameType,
                                                   const void *paths,
                                                   PathID pathBase,
                                                   GLenum coverMode,
                                                   GLenum transformType,
                                                   const GLfloat *transformValues,
                                                   ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCoverStrokePathInstancedCHROMIUM_transformValues(const State &glState,
                                                             bool isCallValid,
                                                             GLsizei numPath,
                                                             GLenum pathNameType,
                                                             const void *paths,
                                                             PathID pathBase,
                                                             GLenum coverMode,
                                                             GLenum transformType,
                                                             const GLfloat *transformValues,
                                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureStencilStrokePathInstancedCHROMIUM_paths(const State &glState,
                                                     bool isCallValid,
                                                     GLsizei numPath,
                                                     GLenum pathNameType,
                                                     const void *paths,
                                                     PathID pathBase,
                                                     GLint reference,
                                                     GLuint mask,
                                                     GLenum transformType,
                                                     const GLfloat *transformValues,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureStencilStrokePathInstancedCHROMIUM_transformValues(const State &glState,
                                                               bool isCallValid,
                                                               GLsizei numPath,
                                                               GLenum pathNameType,
                                                               const void *paths,
                                                               PathID pathBase,
                                                               GLint reference,
                                                               GLuint mask,
                                                               GLenum transformType,
                                                               const GLfloat *transformValues,
                                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureStencilFillPathInstancedCHROMIUM_paths(const State &glState,
                                                   bool isCallValid,
                                                   GLsizei numPaths,
                                                   GLenum pathNameType,
                                                   const void *paths,
                                                   PathID pathBase,
                                                   GLenum fillMode,
                                                   GLuint mask,
                                                   GLenum transformType,
                                                   const GLfloat *transformValues,
                                                   ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureStencilFillPathInstancedCHROMIUM_transformValues(const State &glState,
                                                             bool isCallValid,
                                                             GLsizei numPaths,
                                                             GLenum pathNameType,
                                                             const void *paths,
                                                             PathID pathBase,
                                                             GLenum fillMode,
                                                             GLuint mask,
                                                             GLenum transformType,
                                                             const GLfloat *transformValues,
                                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureStencilThenCoverFillPathInstancedCHROMIUM_paths(const State &glState,
                                                            bool isCallValid,
                                                            GLsizei numPaths,
                                                            GLenum pathNameType,
                                                            const void *paths,
                                                            PathID pathBase,
                                                            GLenum fillMode,
                                                            GLuint mask,
                                                            GLenum coverMode,
                                                            GLenum transformType,
                                                            const GLfloat *transformValues,
                                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureStencilThenCoverFillPathInstancedCHROMIUM_transformValues(
    const State &glState,
    bool isCallValid,
    GLsizei numPaths,
    GLenum pathNameType,
    const void *paths,
    PathID pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat *transformValues,
    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureStencilThenCoverStrokePathInstancedCHROMIUM_paths(const State &glState,
                                                              bool isCallValid,
                                                              GLsizei numPaths,
                                                              GLenum pathNameType,
                                                              const void *paths,
                                                              PathID pathBase,
                                                              GLint reference,
                                                              GLuint mask,
                                                              GLenum coverMode,
                                                              GLenum transformType,
                                                              const GLfloat *transformValues,
                                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureStencilThenCoverStrokePathInstancedCHROMIUM_transformValues(
    const State &glState,
    bool isCallValid,
    GLsizei numPaths,
    GLenum pathNameType,
    const void *paths,
    PathID pathBase,
    GLint reference,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat *transformValues,
    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureBindFragmentInputLocationCHROMIUM_name(const State &glState,
                                                   bool isCallValid,
                                                   ShaderProgramID programs,
                                                   GLint location,
                                                   const GLchar *name,
                                                   ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureProgramPathFragmentInputGenCHROMIUM_coeffs(const State &glState,
                                                       bool isCallValid,
                                                       ShaderProgramID program,
                                                       GLint location,
                                                       GLenum genMode,
                                                       GLint components,
                                                       const GLfloat *coeffs,
                                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureBindFragDataLocationEXT_name(const State &glState,
                                         bool isCallValid,
                                         ShaderProgramID program,
                                         GLuint color,
                                         const GLchar *name,
                                         ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureBindFragDataLocationIndexedEXT_name(const State &glState,
                                                bool isCallValid,
                                                ShaderProgramID program,
                                                GLuint colorNumber,
                                                GLuint index,
                                                const GLchar *name,
                                                ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureGetFragDataIndexEXT_name(const State &glState,
                                     bool isCallValid,
                                     ShaderProgramID program,
                                     const GLchar *name,
                                     ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureGetProgramResourceLocationIndexEXT_name(const State &glState,
                                                    bool isCallValid,
                                                    ShaderProgramID program,
                                                    GLenum programInterface,
                                                    const GLchar *name,
                                                    ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureInsertEventMarkerEXT_marker(const State &glState,
                                        bool isCallValid,
                                        GLsizei length,
                                        const GLchar *marker,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CapturePushGroupMarkerEXT_marker(const State &glState,
                                      bool isCallValid,
                                      GLsizei length,
                                      const GLchar *marker,
                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDiscardFramebufferEXT_attachments(const State &glState,
                                              bool isCallValid,
                                              GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum *attachments,
                                              ParamCapture *paramCapture)
{
    CaptureMemory(attachments, sizeof(GLenum) * numAttachments, paramCapture);
}

void CaptureDeleteQueriesEXT_idsPacked(const State &glState,
                                       bool isCallValid,
                                       GLsizei n,
                                       const QueryID *ids,
                                       ParamCapture *paramCapture)
{
    CaptureMemory(ids, sizeof(QueryID) * n, paramCapture);
}

void CaptureGenQueriesEXT_idsPacked(const State &glState,
                                    bool isCallValid,
                                    GLsizei n,
                                    QueryID *ids,
                                    ParamCapture *paramCapture)
{
    CaptureGenHandles(n, ids, paramCapture);
}

void CaptureGetQueryObjecti64vEXT_params(const State &glState,
                                         bool isCallValid,
                                         QueryID id,
                                         GLenum pname,
                                         GLint64 *params,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInteger64vEXT_data(const State &glState,
                                  bool isCallValid,
                                  GLenum pname,
                                  GLint64 *data,
                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectivEXT_params(const State &glState,
                                       bool isCallValid,
                                       QueryID id,
                                       GLenum pname,
                                       GLint *params,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectui64vEXT_params(const State &glState,
                                          bool isCallValid,
                                          QueryID id,
                                          GLenum pname,
                                          GLuint64 *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectuivEXT_params(const State &glState,
                                        bool isCallValid,
                                        QueryID id,
                                        GLenum pname,
                                        GLuint *params,
                                        ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLuint);
}

void CaptureGetQueryivEXT_params(const State &glState,
                                 bool isCallValid,
                                 QueryType targetPacked,
                                 GLenum pname,
                                 GLint *params,
                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawBuffersEXT_bufs(const State &glState,
                                bool isCallValid,
                                GLsizei n,
                                const GLenum *bufs,
                                ParamCapture *paramCapture)
{
    CaptureDrawBuffers_bufs(glState, isCallValid, n, bufs, paramCapture);
}

void CaptureDrawElementsInstancedEXT_indices(const State &glState,
                                             bool isCallValid,
                                             PrimitiveMode modePacked,
                                             GLsizei count,
                                             DrawElementsType typePacked,
                                             const void *indices,
                                             GLsizei primcount,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCreateMemoryObjectsEXT_memoryObjectsPacked(const State &glState,
                                                       bool isCallValid,
                                                       GLsizei n,
                                                       MemoryObjectID *memoryObjects,
                                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDeleteMemoryObjectsEXT_memoryObjectsPacked(const State &glState,
                                                       bool isCallValid,
                                                       GLsizei n,
                                                       const MemoryObjectID *memoryObjects,
                                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetMemoryObjectParameterivEXT_params(const State &glState,
                                                 bool isCallValid,
                                                 MemoryObjectID memoryObject,
                                                 GLenum pname,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUnsignedBytevEXT_data(const State &glState,
                                     bool isCallValid,
                                     GLenum pname,
                                     GLubyte *data,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUnsignedBytei_vEXT_data(const State &glState,
                                       bool isCallValid,
                                       GLenum target,
                                       GLuint index,
                                       GLubyte *data,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMemoryObjectParameterivEXT_params(const State &glState,
                                              bool isCallValid,
                                              MemoryObjectID memoryObject,
                                              GLenum pname,
                                              const GLint *params,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformfvEXT_params(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei bufSize,
                                    GLfloat *params,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformivEXT_params(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei bufSize,
                                    GLint *params,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsEXT_data(const State &glState,
                                bool isCallValid,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                GLsizei bufSize,
                                void *data,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDeleteSemaphoresEXT_semaphoresPacked(const State &glState,
                                                 bool isCallValid,
                                                 GLsizei n,
                                                 const SemaphoreID *semaphores,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGenSemaphoresEXT_semaphoresPacked(const State &glState,
                                              bool isCallValid,
                                              GLsizei n,
                                              SemaphoreID *semaphores,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSemaphoreParameterui64vEXT_params(const State &glState,
                                                 bool isCallValid,
                                                 SemaphoreID semaphore,
                                                 GLenum pname,
                                                 GLuint64 *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSemaphoreParameterui64vEXT_params(const State &glState,
                                              bool isCallValid,
                                              SemaphoreID semaphore,
                                              GLenum pname,
                                              const GLuint64 *params,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSignalSemaphoreEXT_buffersPacked(const State &glState,
                                             bool isCallValid,
                                             SemaphoreID semaphore,
                                             GLuint numBufferBarriers,
                                             const BufferID *buffers,
                                             GLuint numTextureBarriers,
                                             const TextureID *textures,
                                             const GLenum *dstLayouts,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSignalSemaphoreEXT_texturesPacked(const State &glState,
                                              bool isCallValid,
                                              SemaphoreID semaphore,
                                              GLuint numBufferBarriers,
                                              const BufferID *buffers,
                                              GLuint numTextureBarriers,
                                              const TextureID *textures,
                                              const GLenum *dstLayouts,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSignalSemaphoreEXT_dstLayouts(const State &glState,
                                          bool isCallValid,
                                          SemaphoreID semaphore,
                                          GLuint numBufferBarriers,
                                          const BufferID *buffers,
                                          GLuint numTextureBarriers,
                                          const TextureID *textures,
                                          const GLenum *dstLayouts,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureWaitSemaphoreEXT_buffersPacked(const State &glState,
                                           bool isCallValid,
                                           SemaphoreID semaphore,
                                           GLuint numBufferBarriers,
                                           const BufferID *buffers,
                                           GLuint numTextureBarriers,
                                           const TextureID *textures,
                                           const GLenum *srcLayouts,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureWaitSemaphoreEXT_texturesPacked(const State &glState,
                                            bool isCallValid,
                                            SemaphoreID semaphore,
                                            GLuint numBufferBarriers,
                                            const BufferID *buffers,
                                            GLuint numTextureBarriers,
                                            const TextureID *textures,
                                            const GLenum *srcLayouts,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureWaitSemaphoreEXT_srcLayouts(const State &glState,
                                        bool isCallValid,
                                        SemaphoreID semaphore,
                                        GLuint numBufferBarriers,
                                        const BufferID *buffers,
                                        GLuint numTextureBarriers,
                                        const TextureID *textures,
                                        const GLenum *srcLayouts,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDebugMessageCallbackKHR_userParam(const State &glState,
                                              bool isCallValid,
                                              GLDEBUGPROCKHR callback,
                                              const void *userParam,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDebugMessageControlKHR_ids(const State &glState,
                                       bool isCallValid,
                                       GLenum source,
                                       GLenum type,
                                       GLenum severity,
                                       GLsizei count,
                                       const GLuint *ids,
                                       GLboolean enabled,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDebugMessageInsertKHR_buf(const State &glState,
                                      bool isCallValid,
                                      GLenum source,
                                      GLenum type,
                                      GLuint id,
                                      GLenum severity,
                                      GLsizei length,
                                      const GLchar *buf,
                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLogKHR_sources(const State &glState,
                                          bool isCallValid,
                                          GLuint count,
                                          GLsizei bufSize,
                                          GLenum *sources,
                                          GLenum *types,
                                          GLuint *ids,
                                          GLenum *severities,
                                          GLsizei *lengths,
                                          GLchar *messageLog,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLogKHR_types(const State &glState,
                                        bool isCallValid,
                                        GLuint count,
                                        GLsizei bufSize,
                                        GLenum *sources,
                                        GLenum *types,
                                        GLuint *ids,
                                        GLenum *severities,
                                        GLsizei *lengths,
                                        GLchar *messageLog,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLogKHR_ids(const State &glState,
                                      bool isCallValid,
                                      GLuint count,
                                      GLsizei bufSize,
                                      GLenum *sources,
                                      GLenum *types,
                                      GLuint *ids,
                                      GLenum *severities,
                                      GLsizei *lengths,
                                      GLchar *messageLog,
                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLogKHR_severities(const State &glState,
                                             bool isCallValid,
                                             GLuint count,
                                             GLsizei bufSize,
                                             GLenum *sources,
                                             GLenum *types,
                                             GLuint *ids,
                                             GLenum *severities,
                                             GLsizei *lengths,
                                             GLchar *messageLog,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLogKHR_lengths(const State &glState,
                                          bool isCallValid,
                                          GLuint count,
                                          GLsizei bufSize,
                                          GLenum *sources,
                                          GLenum *types,
                                          GLuint *ids,
                                          GLenum *severities,
                                          GLsizei *lengths,
                                          GLchar *messageLog,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLogKHR_messageLog(const State &glState,
                                             bool isCallValid,
                                             GLuint count,
                                             GLsizei bufSize,
                                             GLenum *sources,
                                             GLenum *types,
                                             GLuint *ids,
                                             GLenum *severities,
                                             GLsizei *lengths,
                                             GLchar *messageLog,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectLabelKHR_length(const State &glState,
                                     bool isCallValid,
                                     GLenum identifier,
                                     GLuint name,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *label,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectLabelKHR_label(const State &glState,
                                    bool isCallValid,
                                    GLenum identifier,
                                    GLuint name,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLchar *label,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectPtrLabelKHR_ptr(const State &glState,
                                     bool isCallValid,
                                     const void *ptr,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *label,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectPtrLabelKHR_length(const State &glState,
                                        bool isCallValid,
                                        const void *ptr,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *label,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectPtrLabelKHR_label(const State &glState,
                                       bool isCallValid,
                                       const void *ptr,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLchar *label,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPointervKHR_params(const State &glState,
                                  bool isCallValid,
                                  GLenum pname,
                                  void **params,
                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureObjectLabelKHR_label(const State &glState,
                                 bool isCallValid,
                                 GLenum identifier,
                                 GLuint name,
                                 GLsizei length,
                                 const GLchar *label,
                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureObjectPtrLabelKHR_ptr(const State &glState,
                                  bool isCallValid,
                                  const void *ptr,
                                  GLsizei length,
                                  const GLchar *label,
                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureObjectPtrLabelKHR_label(const State &glState,
                                    bool isCallValid,
                                    const void *ptr,
                                    GLsizei length,
                                    const GLchar *label,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CapturePushDebugGroupKHR_message(const State &glState,
                                      bool isCallValid,
                                      GLenum source,
                                      GLuint id,
                                      GLsizei length,
                                      const GLchar *message,
                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDeleteFencesNV_fencesPacked(const State &glState,
                                        bool isCallValid,
                                        GLsizei n,
                                        const FenceNVID *fences,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGenFencesNV_fencesPacked(const State &glState,
                                     bool isCallValid,
                                     GLsizei n,
                                     FenceNVID *fences,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFenceivNV_params(const State &glState,
                                bool isCallValid,
                                FenceNVID fence,
                                GLenum pname,
                                GLint *params,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawTexfvOES_coords(const State &glState,
                                bool isCallValid,
                                const GLfloat *coords,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawTexivOES_coords(const State &glState,
                                bool isCallValid,
                                const GLint *coords,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawTexsvOES_coords(const State &glState,
                                bool isCallValid,
                                const GLshort *coords,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawTexxvOES_coords(const State &glState,
                                bool isCallValid,
                                const GLfixed *coords,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDeleteFramebuffersOES_framebuffersPacked(const State &glState,
                                                     bool isCallValid,
                                                     GLsizei n,
                                                     const FramebufferID *framebuffers,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDeleteRenderbuffersOES_renderbuffersPacked(const State &glState,
                                                       bool isCallValid,
                                                       GLsizei n,
                                                       const RenderbufferID *renderbuffers,
                                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGenFramebuffersOES_framebuffersPacked(const State &glState,
                                                  bool isCallValid,
                                                  GLsizei n,
                                                  FramebufferID *framebuffers,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGenRenderbuffersOES_renderbuffersPacked(const State &glState,
                                                    bool isCallValid,
                                                    GLsizei n,
                                                    RenderbufferID *renderbuffers,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferAttachmentParameterivOES_params(const State &glState,
                                                          bool isCallValid,
                                                          GLenum target,
                                                          GLenum attachment,
                                                          GLenum pname,
                                                          GLint *params,
                                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetRenderbufferParameterivOES_params(const State &glState,
                                                 bool isCallValid,
                                                 GLenum target,
                                                 GLenum pname,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramBinaryOES_length(const State &glState,
                                       bool isCallValid,
                                       ShaderProgramID program,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLenum *binaryFormat,
                                       void *binary,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramBinaryOES_binaryFormat(const State &glState,
                                             bool isCallValid,
                                             ShaderProgramID program,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLenum *binaryFormat,
                                             void *binary,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramBinaryOES_binary(const State &glState,
                                       bool isCallValid,
                                       ShaderProgramID program,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLenum *binaryFormat,
                                       void *binary,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramBinaryOES_binary(const State &glState,
                                    bool isCallValid,
                                    ShaderProgramID program,
                                    GLenum binaryFormat,
                                    const void *binary,
                                    GLint length,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferPointervOES_params(const State &glState,
                                        bool isCallValid,
                                        BufferBinding targetPacked,
                                        GLenum pname,
                                        void **params,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMatrixIndexPointerOES_pointer(const State &glState,
                                          bool isCallValid,
                                          GLint size,
                                          GLenum type,
                                          GLsizei stride,
                                          const void *pointer,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureWeightPointerOES_pointer(const State &glState,
                                     bool isCallValid,
                                     GLint size,
                                     GLenum type,
                                     GLsizei stride,
                                     const void *pointer,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CapturePointSizePointerOES_pointer(const State &glState,
                                        bool isCallValid,
                                        VertexAttribType typePacked,
                                        GLsizei stride,
                                        const void *pointer,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureQueryMatrixxOES_mantissa(const State &glState,
                                     bool isCallValid,
                                     GLfixed *mantissa,
                                     GLint *exponent,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureQueryMatrixxOES_exponent(const State &glState,
                                     bool isCallValid,
                                     GLfixed *mantissa,
                                     GLint *exponent,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCompressedTexImage3DOES_data(const State &glState,
                                         bool isCallValid,
                                         TextureTarget targetPacked,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLsizei imageSize,
                                         const void *data,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureCompressedTexSubImage3DOES_data(const State &glState,
                                            bool isCallValid,
                                            TextureTarget targetPacked,
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
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexImage3DOES_pixels(const State &glState,
                                 bool isCallValid,
                                 TextureTarget targetPacked,
                                 GLint level,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLint border,
                                 GLenum format,
                                 GLenum type,
                                 const void *pixels,
                                 ParamCapture *paramCapture)
{
    CaptureTexImage3D_pixels(glState, isCallValid, targetPacked, level, internalformat, width,
                             height, depth, border, format, type, pixels, paramCapture);
}

void CaptureTexSubImage3DOES_pixels(const State &glState,
                                    bool isCallValid,
                                    TextureTarget targetPacked,
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
                                    ParamCapture *paramCapture)
{
    CaptureTexSubImage3D_pixels(glState, isCallValid, targetPacked, level, xoffset, yoffset,
                                zoffset, width, height, depth, format, type, pixels, paramCapture);
}

void CaptureGetSamplerParameterIivOES_params(const State &glState,
                                             bool isCallValid,
                                             SamplerID sampler,
                                             GLenum pname,
                                             GLint *params,
                                             ParamCapture *paramCapture)
{
    CaptureGetSamplerParameterIiv_params(glState, isCallValid, sampler, pname, params,
                                         paramCapture);
}

void CaptureGetSamplerParameterIuivOES_params(const State &glState,
                                              bool isCallValid,
                                              SamplerID sampler,
                                              GLenum pname,
                                              GLuint *params,
                                              ParamCapture *paramCapture)
{
    CaptureGetSamplerParameterIuiv_params(glState, isCallValid, sampler, pname, params,
                                          paramCapture);
}

void CaptureGetTexParameterIivOES_params(const State &glState,
                                         bool isCallValid,
                                         TextureType targetPacked,
                                         GLenum pname,
                                         GLint *params,
                                         ParamCapture *paramCapture)
{
    CaptureGetTexParameterIiv_params(glState, isCallValid, targetPacked, pname, params,
                                     paramCapture);
}

void CaptureGetTexParameterIuivOES_params(const State &glState,
                                          bool isCallValid,
                                          TextureType targetPacked,
                                          GLenum pname,
                                          GLuint *params,
                                          ParamCapture *paramCapture)
{
    CaptureGetTexParameterIuiv_params(glState, isCallValid, targetPacked, pname, params,
                                      paramCapture);
}

void CaptureSamplerParameterIivOES_param(const State &glState,
                                         bool isCallValid,
                                         SamplerID sampler,
                                         GLenum pname,
                                         const GLint *param,
                                         ParamCapture *paramCapture)
{
    CaptureSamplerParameterIiv_param(glState, isCallValid, sampler, pname, param, paramCapture);
}

void CaptureSamplerParameterIuivOES_param(const State &glState,
                                          bool isCallValid,
                                          SamplerID sampler,
                                          GLenum pname,
                                          const GLuint *param,
                                          ParamCapture *paramCapture)
{
    CaptureSamplerParameterIuiv_param(glState, isCallValid, sampler, pname, param, paramCapture);
}

void CaptureTexParameterIivOES_params(const State &glState,
                                      bool isCallValid,
                                      TextureType targetPacked,
                                      GLenum pname,
                                      const GLint *params,
                                      ParamCapture *paramCapture)
{
    CaptureTexParameterIiv_params(glState, isCallValid, targetPacked, pname, params, paramCapture);
}

void CaptureTexParameterIuivOES_params(const State &glState,
                                       bool isCallValid,
                                       TextureType targetPacked,
                                       GLenum pname,
                                       const GLuint *params,
                                       ParamCapture *paramCapture)
{
    CaptureTexParameterIuiv_params(glState, isCallValid, targetPacked, pname, params, paramCapture);
}

void CaptureGetTexGenfvOES_params(const State &glState,
                                  bool isCallValid,
                                  GLenum coord,
                                  GLenum pname,
                                  GLfloat *params,
                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexGenivOES_params(const State &glState,
                                  bool isCallValid,
                                  GLenum coord,
                                  GLenum pname,
                                  GLint *params,
                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexGenxvOES_params(const State &glState,
                                  bool isCallValid,
                                  GLenum coord,
                                  GLenum pname,
                                  GLfixed *params,
                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexGenfvOES_params(const State &glState,
                               bool isCallValid,
                               GLenum coord,
                               GLenum pname,
                               const GLfloat *params,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexGenivOES_params(const State &glState,
                               bool isCallValid,
                               GLenum coord,
                               GLenum pname,
                               const GLint *params,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexGenxvOES_params(const State &glState,
                               bool isCallValid,
                               GLenum coord,
                               GLenum pname,
                               const GLfixed *params,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDeleteVertexArraysOES_arraysPacked(const State &glState,
                                               bool isCallValid,
                                               GLsizei n,
                                               const VertexArrayID *arrays,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGenVertexArraysOES_arraysPacked(const State &glState,
                                            bool isCallValid,
                                            GLsizei n,
                                            VertexArrayID *arrays,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexImageANGLE_pixels(const State &glState,
                                    bool isCallValid,
                                    TextureTarget target,
                                    GLint level,
                                    GLenum format,
                                    GLenum type,
                                    void *pixels,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetRenderbufferImageANGLE_pixels(const State &glState,
                                             bool isCallValid,
                                             GLenum target,
                                             GLenum format,
                                             GLenum type,
                                             void *pixels,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
}  // namespace gl
