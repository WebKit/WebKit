//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_gl_3_params.cpp:
//   Pointer parameter capture functions for the OpenGL 3.x entry points.

#include "libANGLE/capture/capture_gl_3_autogen.h"

namespace gl
{
// GL 3.0
void CaptureBindFragDataLocation_name(const State &glState,
                                      bool isCallValid,
                                      ShaderProgramID programPacked,
                                      GLuint color,
                                      const GLchar *name,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI1iv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLint *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI1uiv_v(const State &glState,
                                bool isCallValid,
                                GLuint index,
                                const GLuint *v,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI2iv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLint *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI2uiv_v(const State &glState,
                                bool isCallValid,
                                GLuint index,
                                const GLuint *v,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI3iv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLint *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI3uiv_v(const State &glState,
                                bool isCallValid,
                                GLuint index,
                                const GLuint *v,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI4bv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLbyte *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI4sv_v(const State &glState,
                               bool isCallValid,
                               GLuint index,
                               const GLshort *v,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI4ubv_v(const State &glState,
                                bool isCallValid,
                                GLuint index,
                                const GLubyte *v,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribI4usv_v(const State &glState,
                                bool isCallValid,
                                GLuint index,
                                const GLushort *v,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 3.1
void CaptureGetActiveUniformName_length(const State &glState,
                                        bool isCallValid,
                                        ShaderProgramID programPacked,
                                        GLuint uniformIndex,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *uniformName,
                                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetActiveUniformName_uniformName(const State &glState,
                                             bool isCallValid,
                                             ShaderProgramID programPacked,
                                             GLuint uniformIndex,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLchar *uniformName,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 3.2
void CaptureMultiDrawElementsBaseVertex_count(const State &glState,
                                              bool isCallValid,
                                              PrimitiveMode modePacked,
                                              const GLsizei *count,
                                              DrawElementsType typePacked,
                                              const void *const *indices,
                                              GLsizei drawcount,
                                              const GLint *basevertex,
                                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiDrawElementsBaseVertex_indices(const State &glState,
                                                bool isCallValid,
                                                PrimitiveMode modePacked,
                                                const GLsizei *count,
                                                DrawElementsType typePacked,
                                                const void *const *indices,
                                                GLsizei drawcount,
                                                const GLint *basevertex,
                                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiDrawElementsBaseVertex_basevertex(const State &glState,
                                                   bool isCallValid,
                                                   PrimitiveMode modePacked,
                                                   const GLsizei *count,
                                                   DrawElementsType typePacked,
                                                   const void *const *indices,
                                                   GLsizei drawcount,
                                                   const GLint *basevertex,
                                                   angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL 3.3
void CaptureBindFragDataLocationIndexed_name(const State &glState,
                                             bool isCallValid,
                                             ShaderProgramID programPacked,
                                             GLuint colorNumber,
                                             GLuint index,
                                             const GLchar *name,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureColorP3uiv_color(const State &glState,
                             bool isCallValid,
                             GLenum type,
                             const GLuint *color,
                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureColorP4uiv_color(const State &glState,
                             bool isCallValid,
                             GLenum type,
                             const GLuint *color,
                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetFragDataIndex_name(const State &glState,
                                  bool isCallValid,
                                  ShaderProgramID programPacked,
                                  const GLchar *name,
                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetQueryObjecti64v_params(const State &glState,
                                      bool isCallValid,
                                      QueryID idPacked,
                                      GLenum pname,
                                      GLint64 *params,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetQueryObjectui64v_params(const State &glState,
                                       bool isCallValid,
                                       QueryID idPacked,
                                       GLenum pname,
                                       GLuint64 *params,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiTexCoordP1uiv_coords(const State &glState,
                                      bool isCallValid,
                                      GLenum texture,
                                      GLenum type,
                                      const GLuint *coords,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiTexCoordP2uiv_coords(const State &glState,
                                      bool isCallValid,
                                      GLenum texture,
                                      GLenum type,
                                      const GLuint *coords,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiTexCoordP3uiv_coords(const State &glState,
                                      bool isCallValid,
                                      GLenum texture,
                                      GLenum type,
                                      const GLuint *coords,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureMultiTexCoordP4uiv_coords(const State &glState,
                                      bool isCallValid,
                                      GLenum texture,
                                      GLenum type,
                                      const GLuint *coords,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureNormalP3uiv_coords(const State &glState,
                               bool isCallValid,
                               GLenum type,
                               const GLuint *coords,
                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureSecondaryColorP3uiv_color(const State &glState,
                                      bool isCallValid,
                                      GLenum type,
                                      const GLuint *color,
                                      angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTexCoordP1uiv_coords(const State &glState,
                                 bool isCallValid,
                                 GLenum type,
                                 const GLuint *coords,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTexCoordP2uiv_coords(const State &glState,
                                 bool isCallValid,
                                 GLenum type,
                                 const GLuint *coords,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTexCoordP3uiv_coords(const State &glState,
                                 bool isCallValid,
                                 GLenum type,
                                 const GLuint *coords,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureTexCoordP4uiv_coords(const State &glState,
                                 bool isCallValid,
                                 GLenum type,
                                 const GLuint *coords,
                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribP1uiv_value(const State &glState,
                                    bool isCallValid,
                                    GLuint index,
                                    GLenum type,
                                    GLboolean normalized,
                                    const GLuint *value,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribP2uiv_value(const State &glState,
                                    bool isCallValid,
                                    GLuint index,
                                    GLenum type,
                                    GLboolean normalized,
                                    const GLuint *value,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribP3uiv_value(const State &glState,
                                    bool isCallValid,
                                    GLuint index,
                                    GLenum type,
                                    GLboolean normalized,
                                    const GLuint *value,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexAttribP4uiv_value(const State &glState,
                                    bool isCallValid,
                                    GLuint index,
                                    GLenum type,
                                    GLboolean normalized,
                                    const GLuint *value,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexP2uiv_value(const State &glState,
                              bool isCallValid,
                              GLenum type,
                              const GLuint *value,
                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexP3uiv_value(const State &glState,
                              bool isCallValid,
                              GLenum type,
                              const GLuint *value,
                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureVertexP4uiv_value(const State &glState,
                              bool isCallValid,
                              GLenum type,
                              const GLuint *value,
                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
}  // namespace gl
