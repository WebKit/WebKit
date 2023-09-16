//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// validationGL3.cpp: Validation functions for OpenGL 3.0 entry point parameters

#include "libANGLE/validationGL3_autogen.h"

namespace gl
{

bool ValidateBeginConditionalRender(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLuint id,
                                    GLenum mode)
{
    return true;
}

bool ValidateBindFragDataLocation(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID program,
                                  GLuint color,
                                  const GLchar *name)
{
    return true;
}

bool ValidateClampColor(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum target,
                        GLenum clamp)
{
    return true;
}

bool ValidateEndConditionalRender(const Context *context, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidateFramebufferTexture1D(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  GLenum target,
                                  GLenum attachment,
                                  TextureTarget textargetPacked,
                                  TextureID texture,
                                  GLint level)
{
    return true;
}

bool ValidateFramebufferTexture3D(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  GLenum target,
                                  GLenum attachment,
                                  TextureTarget textargetPacked,
                                  TextureID texture,
                                  GLint level,
                                  GLint zoffset)
{
    return true;
}

bool ValidateVertexAttribI1i(const PrivateState &state,
                             ErrorSet *errors,
                             angle::EntryPoint entryPoint,
                             GLuint index,
                             GLint x)
{
    return true;
}

bool ValidateVertexAttribI1iv(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLint *v)
{
    return true;
}

bool ValidateVertexAttribI1ui(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              GLuint x)
{
    return true;
}

bool ValidateVertexAttribI1uiv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               const GLuint *v)
{
    return true;
}

bool ValidateVertexAttribI2i(const PrivateState &state,
                             ErrorSet *errors,
                             angle::EntryPoint entryPoint,
                             GLuint index,
                             GLint x,
                             GLint y)
{
    return true;
}

bool ValidateVertexAttribI2iv(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLint *v)
{
    return true;
}

bool ValidateVertexAttribI2ui(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              GLuint x,
                              GLuint y)
{
    return true;
}

bool ValidateVertexAttribI2uiv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               const GLuint *v)
{
    return true;
}

bool ValidateVertexAttribI3i(const PrivateState &state,
                             ErrorSet *errors,
                             angle::EntryPoint entryPoint,
                             GLuint index,
                             GLint x,
                             GLint y,
                             GLint z)
{
    return true;
}

bool ValidateVertexAttribI3iv(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLint *v)
{
    return true;
}

bool ValidateVertexAttribI3ui(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              GLuint x,
                              GLuint y,
                              GLuint z)
{
    return true;
}

bool ValidateVertexAttribI3uiv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               const GLuint *v)
{
    return true;
}

bool ValidateVertexAttribI4bv(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLbyte *v)
{
    return true;
}

bool ValidateVertexAttribI4sv(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLshort *v)
{
    return true;
}

bool ValidateVertexAttribI4ubv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               const GLubyte *v)
{
    return true;
}

bool ValidateVertexAttribI4usv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               const GLushort *v)
{
    return true;
}

bool ValidateGetActiveUniformName(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID program,
                                  GLuint uniformIndex,
                                  GLsizei bufSize,
                                  const GLsizei *length,
                                  const GLchar *uniformName)
{
    return true;
}

bool ValidatePrimitiveRestartIndex(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLuint index)
{
    return true;
}

bool ValidateMultiDrawElementsBaseVertex(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         PrimitiveMode mode,
                                         const GLsizei *count,
                                         DrawElementsType type,
                                         const void *const *indices,
                                         GLsizei drawcount,
                                         const GLint *basevertex)
{
    return true;
}

bool ValidateProvokingVertex(const PrivateState &state,
                             ErrorSet *errors,
                             angle::EntryPoint entryPoint,
                             ProvokingVertexConvention modePacked)
{
    return true;
}

bool ValidateTexImage2DMultisample(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLenum target,
                                   GLsizei samples,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateTexImage3DMultisample(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLenum target,
                                   GLsizei samples,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateBindFragDataLocationIndexed(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         ShaderProgramID program,
                                         GLuint colorNumber,
                                         GLuint index,
                                         const GLchar *name)
{
    return true;
}

bool ValidateColorP3ui(const Context *context,
                       angle::EntryPoint entryPoint,
                       GLenum type,
                       GLuint color)
{
    return true;
}

bool ValidateColorP3uiv(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum type,
                        const GLuint *color)
{
    return true;
}

bool ValidateColorP4ui(const Context *context,
                       angle::EntryPoint entryPoint,
                       GLenum type,
                       GLuint color)
{
    return true;
}

bool ValidateColorP4uiv(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum type,
                        const GLuint *color)
{
    return true;
}

bool ValidateGetFragDataIndex(const Context *context,
                              angle::EntryPoint entryPoint,
                              ShaderProgramID program,
                              const GLchar *name)
{
    return true;
}

bool ValidateGetQueryObjecti64v(const Context *context,
                                angle::EntryPoint entryPoint,
                                QueryID id,
                                GLenum pname,
                                const GLint64 *params)
{
    return true;
}

bool ValidateGetQueryObjectui64v(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 QueryID id,
                                 GLenum pname,
                                 const GLuint64 *params)
{
    return true;
}

bool ValidateMultiTexCoordP1ui(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum texture,
                               GLenum type,
                               GLuint coords)
{
    return true;
}

bool ValidateMultiTexCoordP1uiv(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLenum texture,
                                GLenum type,
                                const GLuint *coords)
{
    return true;
}

bool ValidateMultiTexCoordP2ui(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum texture,
                               GLenum type,
                               GLuint coords)
{
    return true;
}

bool ValidateMultiTexCoordP2uiv(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLenum texture,
                                GLenum type,
                                const GLuint *coords)
{
    return true;
}

bool ValidateMultiTexCoordP3ui(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum texture,
                               GLenum type,
                               GLuint coords)
{
    return true;
}

bool ValidateMultiTexCoordP3uiv(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLenum texture,
                                GLenum type,
                                const GLuint *coords)
{
    return true;
}

bool ValidateMultiTexCoordP4ui(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum texture,
                               GLenum type,
                               GLuint coords)
{
    return true;
}

bool ValidateMultiTexCoordP4uiv(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLenum texture,
                                GLenum type,
                                const GLuint *coords)
{
    return true;
}

bool ValidateNormalP3ui(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum type,
                        GLuint coords)
{
    return true;
}

bool ValidateNormalP3uiv(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLenum type,
                         const GLuint *coords)
{
    return true;
}

bool ValidateQueryCounter(const Context *context,
                          angle::EntryPoint entryPoint,
                          QueryID id,
                          QueryType targetPacked)
{
    return true;
}

bool ValidateSecondaryColorP3ui(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLenum type,
                                GLuint color)
{
    return true;
}

bool ValidateSecondaryColorP3uiv(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 GLenum type,
                                 const GLuint *color)
{
    return true;
}

bool ValidateTexCoordP1ui(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLenum type,
                          GLuint coords)
{
    return true;
}

bool ValidateTexCoordP1uiv(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum type,
                           const GLuint *coords)
{
    return true;
}

bool ValidateTexCoordP2ui(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLenum type,
                          GLuint coords)
{
    return true;
}

bool ValidateTexCoordP2uiv(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum type,
                           const GLuint *coords)
{
    return true;
}

bool ValidateTexCoordP3ui(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLenum type,
                          GLuint coords)
{
    return true;
}

bool ValidateTexCoordP3uiv(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum type,
                           const GLuint *coords)
{
    return true;
}

bool ValidateTexCoordP4ui(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLenum type,
                          GLuint coords)
{
    return true;
}

bool ValidateTexCoordP4uiv(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum type,
                           const GLuint *coords)
{
    return true;
}

bool ValidateVertexAttribP1ui(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              GLenum type,
                              GLboolean normalized,
                              GLuint value)
{
    return true;
}

bool ValidateVertexAttribP1uiv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               GLenum type,
                               GLboolean normalized,
                               const GLuint *value)
{
    return true;
}

bool ValidateVertexAttribP2ui(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              GLenum type,
                              GLboolean normalized,
                              GLuint value)
{
    return true;
}

bool ValidateVertexAttribP2uiv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               GLenum type,
                               GLboolean normalized,
                               const GLuint *value)
{
    return true;
}

bool ValidateVertexAttribP3ui(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              GLenum type,
                              GLboolean normalized,
                              GLuint value)
{
    return true;
}

bool ValidateVertexAttribP3uiv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               GLenum type,
                               GLboolean normalized,
                               const GLuint *value)
{
    return true;
}

bool ValidateVertexAttribP4ui(const PrivateState &state,
                              ErrorSet *errors,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              GLenum type,
                              GLboolean normalized,
                              GLuint value)
{
    return true;
}

bool ValidateVertexAttribP4uiv(const PrivateState &state,
                               ErrorSet *errors,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               GLenum type,
                               GLboolean normalized,
                               const GLuint *value)
{
    return true;
}

bool ValidateVertexP2ui(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum type,
                        GLuint value)
{
    return true;
}

bool ValidateVertexP2uiv(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLenum type,
                         const GLuint *value)
{
    return true;
}

bool ValidateVertexP3ui(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum type,
                        GLuint value)
{
    return true;
}

bool ValidateVertexP3uiv(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLenum type,
                         const GLuint *value)
{
    return true;
}

bool ValidateVertexP4ui(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum type,
                        GLuint value)
{
    return true;
}

bool ValidateVertexP4uiv(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLenum type,
                         const GLuint *value)
{
    return true;
}

}  // namespace gl
