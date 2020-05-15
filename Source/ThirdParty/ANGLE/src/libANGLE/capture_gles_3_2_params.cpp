//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_gles32_params.cpp:
//   Pointer parameter capture functions for the OpenGL ES 3.2 entry points.

#include "libANGLE/capture_gles_3_2_autogen.h"

using namespace angle;

namespace gl
{

void CaptureDebugMessageCallback_userParam(const State &glState,
                                           bool isCallValid,
                                           GLDEBUGPROC callback,
                                           const void *userParam,
                                           ParamCapture *userParamParam)
{
    UNIMPLEMENTED();
}

void CaptureDebugMessageControl_ids(const State &glState,
                                    bool isCallValid,
                                    GLenum source,
                                    GLenum type,
                                    GLenum severity,
                                    GLsizei count,
                                    const GLuint *ids,
                                    GLboolean enabled,
                                    ParamCapture *idsParam)
{
    UNIMPLEMENTED();
}

void CaptureDebugMessageInsert_buf(const State &glState,
                                   bool isCallValid,
                                   GLenum source,
                                   GLenum type,
                                   GLuint id,
                                   GLenum severity,
                                   GLsizei length,
                                   const GLchar *buf,
                                   ParamCapture *bufParam)
{
    UNIMPLEMENTED();
}

void CaptureDrawElementsBaseVertex_indices(const State &glState,
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

void CaptureDrawElementsInstancedBaseVertex_indices(const State &glState,
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

void CaptureDrawRangeElementsBaseVertex_indices(const State &glState,
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

void CaptureGetDebugMessageLog_sources(const State &glState,
                                       bool isCallValid,
                                       GLuint count,
                                       GLsizei bufSize,
                                       GLenum *sources,
                                       GLenum *types,
                                       GLuint *ids,
                                       GLenum *severities,
                                       GLsizei *lengths,
                                       GLchar *messageLog,
                                       ParamCapture *sourcesParam)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLog_types(const State &glState,
                                     bool isCallValid,
                                     GLuint count,
                                     GLsizei bufSize,
                                     GLenum *sources,
                                     GLenum *types,
                                     GLuint *ids,
                                     GLenum *severities,
                                     GLsizei *lengths,
                                     GLchar *messageLog,
                                     ParamCapture *typesParam)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLog_ids(const State &glState,
                                   bool isCallValid,
                                   GLuint count,
                                   GLsizei bufSize,
                                   GLenum *sources,
                                   GLenum *types,
                                   GLuint *ids,
                                   GLenum *severities,
                                   GLsizei *lengths,
                                   GLchar *messageLog,
                                   ParamCapture *idsParam)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLog_severities(const State &glState,
                                          bool isCallValid,
                                          GLuint count,
                                          GLsizei bufSize,
                                          GLenum *sources,
                                          GLenum *types,
                                          GLuint *ids,
                                          GLenum *severities,
                                          GLsizei *lengths,
                                          GLchar *messageLog,
                                          ParamCapture *severitiesParam)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLog_lengths(const State &glState,
                                       bool isCallValid,
                                       GLuint count,
                                       GLsizei bufSize,
                                       GLenum *sources,
                                       GLenum *types,
                                       GLuint *ids,
                                       GLenum *severities,
                                       GLsizei *lengths,
                                       GLchar *messageLog,
                                       ParamCapture *lengthsParam)
{
    UNIMPLEMENTED();
}

void CaptureGetDebugMessageLog_messageLog(const State &glState,
                                          bool isCallValid,
                                          GLuint count,
                                          GLsizei bufSize,
                                          GLenum *sources,
                                          GLenum *types,
                                          GLuint *ids,
                                          GLenum *severities,
                                          GLsizei *lengths,
                                          GLchar *messageLog,
                                          ParamCapture *messageLogParam)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectLabel_length(const State &glState,
                                  bool isCallValid,
                                  GLenum identifier,
                                  GLuint name,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLchar *label,
                                  ParamCapture *lengthParam)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectLabel_label(const State &glState,
                                 bool isCallValid,
                                 GLenum identifier,
                                 GLuint name,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLchar *label,
                                 ParamCapture *labelParam)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectPtrLabel_ptr(const State &glState,
                                  bool isCallValid,
                                  const void *ptr,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLchar *label,
                                  ParamCapture *ptrParam)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectPtrLabel_length(const State &glState,
                                     bool isCallValid,
                                     const void *ptr,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *label,
                                     ParamCapture *lengthParam)
{
    UNIMPLEMENTED();
}

void CaptureGetObjectPtrLabel_label(const State &glState,
                                    bool isCallValid,
                                    const void *ptr,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLchar *label,
                                    ParamCapture *labelParam)
{
    UNIMPLEMENTED();
}

void CaptureGetPointerv_params(const State &glState,
                               bool isCallValid,
                               GLenum pname,
                               void **params,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIiv_params(const State &glState,
                                          bool isCallValid,
                                          SamplerID sampler,
                                          GLenum pname,
                                          GLint *params,
                                          ParamCapture *paramsParam)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIuiv_params(const State &glState,
                                           bool isCallValid,
                                           SamplerID sampler,
                                           GLenum pname,
                                           GLuint *params,
                                           ParamCapture *paramsParam)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIiv_params(const State &glState,
                                      bool isCallValid,
                                      TextureType targetPacked,
                                      GLenum pname,
                                      GLint *params,
                                      ParamCapture *paramsParam)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIuiv_params(const State &glState,
                                       bool isCallValid,
                                       TextureType targetPacked,
                                       GLenum pname,
                                       GLuint *params,
                                       ParamCapture *paramsParam)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformfv_params(const State &glState,
                                 bool isCallValid,
                                 ShaderProgramID program,
                                 UniformLocation location,
                                 GLsizei bufSize,
                                 GLfloat *params,
                                 ParamCapture *paramsParam)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformiv_params(const State &glState,
                                 bool isCallValid,
                                 ShaderProgramID program,
                                 UniformLocation location,
                                 GLsizei bufSize,
                                 GLint *params,
                                 ParamCapture *paramsParam)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformuiv_params(const State &glState,
                                  bool isCallValid,
                                  ShaderProgramID program,
                                  UniformLocation location,
                                  GLsizei bufSize,
                                  GLuint *params,
                                  ParamCapture *paramsParam)
{
    UNIMPLEMENTED();
}

void CaptureObjectLabel_label(const State &glState,
                              bool isCallValid,
                              GLenum identifier,
                              GLuint name,
                              GLsizei length,
                              const GLchar *label,
                              ParamCapture *labelParam)
{
    UNIMPLEMENTED();
}

void CaptureObjectPtrLabel_ptr(const State &glState,
                               bool isCallValid,
                               const void *ptr,
                               GLsizei length,
                               const GLchar *label,
                               ParamCapture *ptrParam)
{
    UNIMPLEMENTED();
}

void CaptureObjectPtrLabel_label(const State &glState,
                                 bool isCallValid,
                                 const void *ptr,
                                 GLsizei length,
                                 const GLchar *label,
                                 ParamCapture *labelParam)
{
    UNIMPLEMENTED();
}

void CapturePushDebugGroup_message(const State &glState,
                                   bool isCallValid,
                                   GLenum source,
                                   GLuint id,
                                   GLsizei length,
                                   const GLchar *message,
                                   ParamCapture *messageParam)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixels_data(const State &glState,
                             bool isCallValid,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLenum type,
                             GLsizei bufSize,
                             void *data,
                             ParamCapture *dataParam)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterIiv_param(const State &glState,
                                      bool isCallValid,
                                      SamplerID sampler,
                                      GLenum pname,
                                      const GLint *param,
                                      ParamCapture *paramParam)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterIuiv_param(const State &glState,
                                       bool isCallValid,
                                       SamplerID sampler,
                                       GLenum pname,
                                       const GLuint *param,
                                       ParamCapture *paramParam)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterIiv_params(const State &glState,
                                   bool isCallValid,
                                   TextureType targetPacked,
                                   GLenum pname,
                                   const GLint *params,
                                   ParamCapture *paramParam)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterIuiv_params(const State &glState,
                                    bool isCallValid,
                                    TextureType targetPacked,
                                    GLenum pname,
                                    const GLuint *params,
                                    ParamCapture *paramParam)
{
    UNIMPLEMENTED();
}

}  // namespace gl
