//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES.h: Validation functions for generic OpenGL ES entry point parameters

#ifndef LIBANGLE_VALIDATION_ES_H_
#define LIBANGLE_VALIDATION_ES_H_

#include "common/PackedEnums.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/VertexArray.h"

#include <GLES2/gl2.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>

namespace egl
{
class Display;
class Image;
}  // namespace egl

namespace gl
{
class Context;
struct Format;
class Framebuffer;
struct LinkedUniform;
class Program;
class Shader;

void SetRobustLengthParam(const GLsizei *length, GLsizei value);
bool ValidTextureTarget(const Context *context, TextureType type);
bool ValidTexture2DTarget(const Context *context, TextureType type);
bool ValidTexture3DTarget(const Context *context, TextureType target);
bool ValidTextureExternalTarget(const Context *context, TextureType target);
bool ValidTextureExternalTarget(const Context *context, TextureTarget target);
bool ValidTexture2DDestinationTarget(const Context *context, TextureTarget target);
bool ValidTexture3DDestinationTarget(const Context *context, TextureTarget target);
bool ValidTexLevelDestinationTarget(const Context *context, TextureType type);
bool ValidFramebufferTarget(const Context *context, GLenum target);
bool ValidMipLevel(const Context *context, TextureType type, GLint level);
bool ValidImageSizeParameters(const Context *context,
                              TextureType target,
                              GLint level,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              bool isSubImage);
bool ValidCompressedImageSize(const Context *context,
                              GLenum internalFormat,
                              GLint level,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth);
bool ValidCompressedSubImageSize(const Context *context,
                                 GLenum internalFormat,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 size_t textureWidth,
                                 size_t textureHeight,
                                 size_t textureDepth);
bool ValidImageDataSize(const Context *context,
                        TextureType texType,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth,
                        GLenum format,
                        GLenum type,
                        const void *pixels,
                        GLsizei imageSize);

bool ValidQueryType(const Context *context, QueryType queryType);

bool ValidateWebGLVertexAttribPointer(const Context *context,
                                      VertexAttribType type,
                                      GLboolean normalized,
                                      GLsizei stride,
                                      const void *ptr,
                                      bool pureInteger);

// Returns valid program if id is a valid program name
// Errors INVALID_OPERATION if valid shader is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Program *GetValidProgram(const Context *context, ShaderProgramID id);

// Returns valid shader if id is a valid shader name
// Errors INVALID_OPERATION if valid program is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Shader *GetValidShader(const Context *context, ShaderProgramID id);

bool ValidateAttachmentTarget(const Context *context, GLenum attachment);
bool ValidateRenderbufferStorageParametersBase(const Context *context,
                                               GLenum target,
                                               GLsizei samples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height);
bool ValidateFramebufferRenderbufferParameters(const Context *context,
                                               GLenum target,
                                               GLenum attachment,
                                               GLenum renderbuffertarget,
                                               RenderbufferID renderbuffer);

bool ValidateBlitFramebufferParameters(const Context *context,
                                       GLint srcX0,
                                       GLint srcY0,
                                       GLint srcX1,
                                       GLint srcY1,
                                       GLint dstX0,
                                       GLint dstY0,
                                       GLint dstX1,
                                       GLint dstY1,
                                       GLbitfield mask,
                                       GLenum filter);

bool ValidatePixelPack(const Context *context,
                       GLenum format,
                       GLenum type,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height,
                       GLsizei bufSize,
                       GLsizei *length,
                       const void *pixels);

bool ValidateReadPixelsBase(const Context *context,
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
                            const void *pixels);
bool ValidateReadPixelsRobustANGLE(const Context *context,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   const GLsizei *length,
                                   const GLsizei *columns,
                                   const GLsizei *rows,
                                   const void *pixels);
bool ValidateReadnPixelsEXT(const Context *context,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            const void *pixels);
bool ValidateReadnPixelsRobustANGLE(const Context *context,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    GLsizei bufSize,
                                    const GLsizei *length,
                                    const GLsizei *columns,
                                    const GLsizei *rows,
                                    const void *data);

bool ValidateGenQueriesEXT(const Context *context, GLsizei n, const QueryID *ids);
bool ValidateDeleteQueriesEXT(const Context *context, GLsizei n, const QueryID *ids);
bool ValidateIsQueryEXT(const Context *context, QueryID id);
bool ValidateBeginQueryBase(const Context *context, QueryType target, QueryID id);
bool ValidateBeginQueryEXT(const Context *context, QueryType target, QueryID id);
bool ValidateEndQueryBase(const Context *context, QueryType target);
bool ValidateEndQueryEXT(const Context *context, QueryType target);
bool ValidateQueryCounterEXT(const Context *context, QueryID id, QueryType target);
bool ValidateGetQueryivBase(const Context *context,
                            QueryType target,
                            GLenum pname,
                            GLsizei *numParams);
bool ValidateGetQueryivEXT(const Context *context,
                           QueryType target,
                           GLenum pname,
                           const GLint *params);
bool ValidateGetQueryivRobustANGLE(const Context *context,
                                   QueryType target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLsizei *length,
                                   const GLint *params);
bool ValidateGetQueryObjectValueBase(const Context *context,
                                     QueryID id,
                                     GLenum pname,
                                     GLsizei *numParams);
bool ValidateGetQueryObjectivEXT(const Context *context,
                                 QueryID id,
                                 GLenum pname,
                                 const GLint *params);
bool ValidateGetQueryObjectivRobustANGLE(const Context *context,
                                         QueryID id,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         const GLsizei *length,
                                         const GLint *params);
bool ValidateGetQueryObjectuivEXT(const Context *context,
                                  QueryID id,
                                  GLenum pname,
                                  const GLuint *params);
bool ValidateGetQueryObjectuivRobustANGLE(const Context *context,
                                          QueryID id,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          const GLsizei *length,
                                          const GLuint *params);
bool ValidateGetQueryObjecti64vEXT(const Context *context,
                                   QueryID id,
                                   GLenum pname,
                                   GLint64 *params);
bool ValidateGetQueryObjecti64vRobustANGLE(const Context *context,
                                           QueryID id,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLsizei *length,
                                           GLint64 *params);
bool ValidateGetQueryObjectui64vEXT(const Context *context,
                                    QueryID id,
                                    GLenum pname,
                                    GLuint64 *params);
bool ValidateGetQueryObjectui64vRobustANGLE(const Context *context,
                                            QueryID id,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            const GLsizei *length,
                                            GLuint64 *params);

bool ValidateUniformCommonBase(const Context *context,
                               const Program *program,
                               UniformLocation location,
                               GLsizei count,
                               const LinkedUniform **uniformOut);
bool ValidateUniform1ivValue(const Context *context,
                             GLenum uniformType,
                             GLsizei count,
                             const GLint *value);

ANGLE_INLINE bool ValidateUniformValue(const Context *context, GLenum valueType, GLenum uniformType)
{
    // Check that the value type is compatible with uniform type.
    // Do the cheaper test first, for a little extra speed.
    if (valueType != uniformType && VariableBoolVectorType(valueType) != uniformType)
    {
        context->validationError(GL_INVALID_OPERATION, err::kUniformSizeMismatch);
        return false;
    }
    return true;
}

bool ValidateUniformMatrixValue(const Context *context, GLenum valueType, GLenum uniformType);
bool ValidateUniform(const Context *context,
                     GLenum uniformType,
                     UniformLocation location,
                     GLsizei count);
bool ValidateUniformMatrix(const Context *context,
                           GLenum matrixType,
                           UniformLocation location,
                           GLsizei count,
                           GLboolean transpose);
bool ValidateGetBooleanvRobustANGLE(const Context *context,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    const GLsizei *length,
                                    const GLboolean *params);
bool ValidateGetFloatvRobustANGLE(const Context *context,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  const GLsizei *length,
                                  const GLfloat *params);
bool ValidateStateQuery(const Context *context,
                        GLenum pname,
                        GLenum *nativeType,
                        unsigned int *numParams);
bool ValidateGetIntegervRobustANGLE(const Context *context,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    const GLsizei *length,
                                    const GLint *data);
bool ValidateGetInteger64vRobustANGLE(const Context *context,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      const GLsizei *length,
                                      GLint64 *data);
bool ValidateRobustStateQuery(const Context *context,
                              GLenum pname,
                              GLsizei bufSize,
                              GLenum *nativeType,
                              unsigned int *numParams);

bool ValidateCopyTexImageParametersBase(const Context *context,
                                        TextureTarget target,
                                        GLint level,
                                        GLenum internalformat,
                                        bool isSubImage,
                                        GLint xoffset,
                                        GLint yoffset,
                                        GLint zoffset,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border,
                                        Format *textureFormatOut);

void RecordDrawModeError(const Context *context, PrimitiveMode mode);
const char *ValidateDrawElementsStates(const Context *context);

ANGLE_INLINE bool ValidateDrawBase(const Context *context, PrimitiveMode mode)
{
    if (!context->getStateCache().isValidDrawMode(mode))
    {
        RecordDrawModeError(context, mode);
        return false;
    }

    intptr_t drawStatesError = context->getStateCache().getBasicDrawStatesError(context);
    if (drawStatesError)
    {
        const char *errorMessage = reinterpret_cast<const char *>(drawStatesError);

        // All errors from ValidateDrawStates should return INVALID_OPERATION except Framebuffer
        // Incomplete.
        bool isFramebufferIncomplete = strcmp(errorMessage, err::kDrawFramebufferIncomplete) == 0;
        GLenum errorCode =
            isFramebufferIncomplete ? GL_INVALID_FRAMEBUFFER_OPERATION : GL_INVALID_OPERATION;
        context->validationError(errorCode, errorMessage);
        return false;
    }

    return true;
}

bool ValidateDrawArraysInstancedBase(const Context *context,
                                     PrimitiveMode mode,
                                     GLint first,
                                     GLsizei count,
                                     GLsizei primcount);
bool ValidateDrawArraysInstancedANGLE(const Context *context,
                                      PrimitiveMode mode,
                                      GLint first,
                                      GLsizei count,
                                      GLsizei primcount);
bool ValidateDrawArraysInstancedEXT(const Context *context,
                                    PrimitiveMode mode,
                                    GLint first,
                                    GLsizei count,
                                    GLsizei primcount);

bool ValidateDrawElementsInstancedBase(const Context *context,
                                       PrimitiveMode mode,
                                       GLsizei count,
                                       DrawElementsType type,
                                       const void *indices,
                                       GLsizei primcount);
bool ValidateDrawElementsInstancedANGLE(const Context *context,
                                        PrimitiveMode mode,
                                        GLsizei count,
                                        DrawElementsType type,
                                        const void *indices,
                                        GLsizei primcount);
bool ValidateDrawElementsInstancedEXT(const Context *context,
                                      PrimitiveMode mode,
                                      GLsizei count,
                                      DrawElementsType type,
                                      const void *indices,
                                      GLsizei primcount);

bool ValidateDrawInstancedANGLE(const Context *context);

bool ValidateFramebufferTextureBase(const Context *context,
                                    GLenum target,
                                    GLenum attachment,
                                    TextureID texture,
                                    GLint level);

bool ValidateGetUniformBase(const Context *context,
                            ShaderProgramID program,
                            UniformLocation location);
bool ValidateGetnUniformfvEXT(const Context *context,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLsizei bufSize,
                              const GLfloat *params);
bool ValidateGetnUniformfvRobustANGLE(const Context *context,
                                      ShaderProgramID program,
                                      UniformLocation location,
                                      GLsizei bufSize,
                                      const GLsizei *length,
                                      const GLfloat *params);
bool ValidateGetnUniformivEXT(const Context *context,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLsizei bufSize,
                              const GLint *params);
bool ValidateGetnUniformivRobustANGLE(const Context *context,
                                      ShaderProgramID program,
                                      UniformLocation location,
                                      GLsizei bufSize,
                                      const GLsizei *length,
                                      const GLint *params);
bool ValidateGetnUniformuivRobustANGLE(const Context *context,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei bufSize,
                                       const GLsizei *length,
                                       const GLuint *params);
bool ValidateGetUniformfvRobustANGLE(const Context *context,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei bufSize,
                                     const GLsizei *length,
                                     const GLfloat *params);
bool ValidateGetUniformivRobustANGLE(const Context *context,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei bufSize,
                                     const GLsizei *length,
                                     const GLint *params);
bool ValidateGetUniformuivRobustANGLE(const Context *context,
                                      ShaderProgramID program,
                                      UniformLocation location,
                                      GLsizei bufSize,
                                      const GLsizei *length,
                                      const GLuint *params);

bool ValidateDiscardFramebufferBase(const Context *context,
                                    GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments,
                                    bool defaultFramebuffer);

bool ValidateInsertEventMarkerEXT(const Context *context, GLsizei length, const char *marker);
bool ValidatePushGroupMarkerEXT(const Context *context, GLsizei length, const char *marker);

bool ValidateEGLImageTargetTexture2DOES(const Context *context,
                                        TextureType type,
                                        GLeglImageOES image);
bool ValidateEGLImageTargetRenderbufferStorageOES(const Context *context,
                                                  GLenum target,
                                                  GLeglImageOES image);

bool ValidateBindVertexArrayBase(const Context *context, VertexArrayID array);

bool ValidateProgramBinaryBase(const Context *context,
                               ShaderProgramID program,
                               GLenum binaryFormat,
                               const void *binary,
                               GLint length);
bool ValidateGetProgramBinaryBase(const Context *context,
                                  ShaderProgramID program,
                                  GLsizei bufSize,
                                  const GLsizei *length,
                                  const GLenum *binaryFormat,
                                  const void *binary);

bool ValidateDrawBuffersBase(const Context *context, GLsizei n, const GLenum *bufs);

bool ValidateGetBufferPointervBase(const Context *context,
                                   BufferBinding target,
                                   GLenum pname,
                                   GLsizei *length,
                                   void *const *params);
bool ValidateUnmapBufferBase(const Context *context, BufferBinding target);
bool ValidateMapBufferRangeBase(const Context *context,
                                BufferBinding target,
                                GLintptr offset,
                                GLsizeiptr length,
                                GLbitfield access);
bool ValidateFlushMappedBufferRangeBase(const Context *context,
                                        BufferBinding target,
                                        GLintptr offset,
                                        GLsizeiptr length);

bool ValidateGenOrDelete(const Context *context, GLint n);

bool ValidateRobustEntryPoint(const Context *context, GLsizei bufSize);
bool ValidateRobustBufferSize(const Context *context, GLsizei bufSize, GLsizei numParams);

bool ValidateGetFramebufferAttachmentParameterivBase(const Context *context,
                                                     GLenum target,
                                                     GLenum attachment,
                                                     GLenum pname,
                                                     GLsizei *numParams);

bool ValidateGetBufferParameterBase(const Context *context,
                                    BufferBinding target,
                                    GLenum pname,
                                    bool pointerVersion,
                                    GLsizei *numParams);

bool ValidateGetProgramivBase(const Context *context,
                              ShaderProgramID program,
                              GLenum pname,
                              GLsizei *numParams);

bool ValidateGetRenderbufferParameterivBase(const Context *context,
                                            GLenum target,
                                            GLenum pname,
                                            GLsizei *length);

bool ValidateGetShaderivBase(const Context *context,
                             ShaderProgramID shader,
                             GLenum pname,
                             GLsizei *length);

bool ValidateGetTexParameterBase(const Context *context,
                                 TextureType target,
                                 GLenum pname,
                                 GLsizei *length);

template <typename ParamType>
bool ValidateTexParameterBase(const Context *context,
                              TextureType target,
                              GLenum pname,
                              GLsizei bufSize,
                              bool vectorParams,
                              const ParamType *params);

bool ValidateGetVertexAttribBase(const Context *context,
                                 GLuint index,
                                 GLenum pname,
                                 GLsizei *length,
                                 bool pointer,
                                 bool pureIntegerEntryPoint);

ANGLE_INLINE bool ValidateVertexFormat(const Context *context,
                                       GLuint index,
                                       GLint size,
                                       VertexAttribTypeCase validation)
{
    const Caps &caps = context->getCaps();
    if (index >= static_cast<GLuint>(caps.maxVertexAttributes))
    {
        context->validationError(GL_INVALID_VALUE, err::kIndexExceedsMaxVertexAttribute);
        return false;
    }

    switch (validation)
    {
        case VertexAttribTypeCase::Invalid:
            context->validationError(GL_INVALID_ENUM, err::kInvalidType);
            return false;
        case VertexAttribTypeCase::Valid:
            if (size < 1 || size > 4)
            {
                context->validationError(GL_INVALID_VALUE, err::kInvalidVertexAttrSize);
                return false;
            }
            break;
        case VertexAttribTypeCase::ValidSize4Only:
            if (size != 4)
            {
                context->validationError(GL_INVALID_OPERATION,
                                         err::kInvalidVertexAttribSize2101010);
                return false;
            }
            break;
        case VertexAttribTypeCase::ValidSize3or4:
            if (size != 3 && size != 4)
            {
                context->validationError(GL_INVALID_OPERATION,
                                         err::kInvalidVertexAttribSize1010102);
                return false;
            }
            break;
    }

    return true;
}

// Note: These byte, short, and int types are all converted to float for the shader.
ANGLE_INLINE bool ValidateFloatVertexFormat(const Context *context,
                                            GLuint index,
                                            GLint size,
                                            VertexAttribType type)
{
    return ValidateVertexFormat(context, index, size,
                                context->getStateCache().getVertexAttribTypeValidation(type));
}

ANGLE_INLINE bool ValidateIntegerVertexFormat(const Context *context,
                                              GLuint index,
                                              GLint size,
                                              VertexAttribType type)
{
    return ValidateVertexFormat(
        context, index, size, context->getStateCache().getIntegerVertexAttribTypeValidation(type));
}

bool ValidateWebGLFramebufferAttachmentClearType(const Context *context,
                                                 GLint drawbuffer,
                                                 const GLenum *validComponentTypes,
                                                 size_t validComponentTypeCount);

bool ValidateRobustCompressedTexImageBase(const Context *context,
                                          GLsizei imageSize,
                                          GLsizei dataSize);

bool ValidateVertexAttribIndex(const Context *context, GLuint index);

bool ValidateGetActiveUniformBlockivBase(const Context *context,
                                         ShaderProgramID program,
                                         GLuint uniformBlockIndex,
                                         GLenum pname,
                                         GLsizei *length);

bool ValidateGetSamplerParameterBase(const Context *context,
                                     SamplerID sampler,
                                     GLenum pname,
                                     GLsizei *length);

template <typename ParamType>
bool ValidateSamplerParameterBase(const Context *context,
                                  SamplerID sampler,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  bool vectorParams,
                                  const ParamType *params);

bool ValidateGetInternalFormativBase(const Context *context,
                                     GLenum target,
                                     GLenum internalformat,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *numParams);

bool ValidateFramebufferNotMultisampled(const Context *context,
                                        const Framebuffer *framebuffer,
                                        bool needResourceSamples);

bool ValidateMultitextureUnit(const Context *context, GLenum texture);

bool ValidateTransformFeedbackPrimitiveMode(const Context *context,
                                            PrimitiveMode transformFeedbackPrimitiveMode,
                                            PrimitiveMode renderPrimitiveMode);

// Common validation for 2D and 3D variants of TexStorage*Multisample.
bool ValidateTexStorageMultisample(const Context *context,
                                   TextureType target,
                                   GLsizei samples,
                                   GLint internalFormat,
                                   GLsizei width,
                                   GLsizei height);

bool ValidateTexStorage2DMultisampleBase(const Context *context,
                                         TextureType target,
                                         GLsizei samples,
                                         GLint internalFormat,
                                         GLsizei width,
                                         GLsizei height);

bool ValidateGetTexLevelParameterBase(const Context *context,
                                      TextureTarget target,
                                      GLint level,
                                      GLenum pname,
                                      GLsizei *length);

bool ValidateMapBufferBase(const Context *context, BufferBinding target);
bool ValidateIndexedStateQuery(const Context *context, GLenum pname, GLuint index, GLsizei *length);
bool ValidateES3TexImage2DParameters(const Context *context,
                                     TextureTarget target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     GLsizei imageSize,
                                     const void *pixels);
bool ValidateES3CopyTexImage2DParameters(const Context *context,
                                         TextureTarget target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border);
bool ValidateES3TexStorage2DParameters(const Context *context,
                                       TextureType target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth);
bool ValidateES3TexStorage3DParameters(const Context *context,
                                       TextureType target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth);

bool ValidateGetMultisamplefvBase(const Context *context,
                                  GLenum pname,
                                  GLuint index,
                                  const GLfloat *val);
bool ValidateSampleMaskiBase(const Context *context, GLuint maskNumber, GLbitfield mask);

// Utility macro for handling implementation methods inside Validation.
#define ANGLE_HANDLE_VALIDATION_ERR(X) \
    (void)(X);                         \
    return false
#define ANGLE_VALIDATION_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, ANGLE_HANDLE_VALIDATION_ERR)

// We should check with Khronos if returning INVALID_FRAMEBUFFER_OPERATION is OK when querying
// implementation format info for incomplete framebuffers. It seems like these queries are
// incongruent with the other errors.
// Inlined for speed.
template <GLenum ErrorCode = GL_INVALID_FRAMEBUFFER_OPERATION>
ANGLE_INLINE bool ValidateFramebufferComplete(const Context *context,
                                              const Framebuffer *framebuffer)
{
    if (!framebuffer->isComplete(context))
    {
        context->validationError(ErrorCode, err::kFramebufferIncomplete);
        return false;
    }

    return true;
}

const char *ValidateProgramDrawStates(const State &state,
                                      const Extensions &extensions,
                                      Program *program);
const char *ValidateProgramPipelineDrawStates(const State &state,
                                              const Extensions &extensions,
                                              ProgramPipeline *programPipeline);
const char *ValidateProgramPipelineAttachedPrograms(ProgramPipeline *programPipeline);
const char *ValidateDrawStates(const Context *context);

void RecordDrawAttribsError(const Context *context);

ANGLE_INLINE bool ValidateDrawAttribs(const Context *context, int64_t maxVertex)
{
    if (maxVertex > context->getStateCache().getNonInstancedVertexElementLimit())
    {
        RecordDrawAttribsError(context);
        return false;
    }

    return true;
}

ANGLE_INLINE bool ValidateDrawArraysAttribs(const Context *context, GLint first, GLsizei count)
{
    if (!context->isBufferAccessValidationEnabled())
    {
        return true;
    }

    // Check the computation of maxVertex doesn't overflow.
    // - first < 0 has been checked as an error condition.
    // - if count <= 0, skip validating no-op draw calls.
    // From this we know maxVertex will be positive, and only need to check if it overflows GLint.
    ASSERT(first >= 0);
    ASSERT(count > 0);
    int64_t maxVertex = static_cast<int64_t>(first) + static_cast<int64_t>(count) - 1;
    if (maxVertex > static_cast<int64_t>(std::numeric_limits<GLint>::max()))
    {
        context->validationError(GL_INVALID_OPERATION, err::kIntegerOverflow);
        return false;
    }

    return ValidateDrawAttribs(context, maxVertex);
}

ANGLE_INLINE bool ValidateDrawInstancedAttribs(const Context *context, GLint primcount)
{
    if (!context->isBufferAccessValidationEnabled())
    {
        return true;
    }

    if ((primcount - 1) > context->getStateCache().getInstancedVertexElementLimit())
    {
        RecordDrawAttribsError(context);
        return false;
    }

    return true;
}

ANGLE_INLINE bool ValidateDrawArraysCommon(const Context *context,
                                           PrimitiveMode mode,
                                           GLint first,
                                           GLsizei count,
                                           GLsizei primcount)
{
    if (first < 0)
    {
        context->validationError(GL_INVALID_VALUE, err::kNegativeStart);
        return false;
    }

    if (count <= 0)
    {
        if (count < 0)
        {
            context->validationError(GL_INVALID_VALUE, err::kNegativeCount);
            return false;
        }

        // Early exit.
        return ValidateDrawBase(context, mode);
    }

    if (!ValidateDrawBase(context, mode))
    {
        return false;
    }

    if (context->getStateCache().isTransformFeedbackActiveUnpaused())
    {
        const State &state                      = context->getState();
        TransformFeedback *curTransformFeedback = state.getCurrentTransformFeedback();
        if (!curTransformFeedback->checkBufferSpaceForDraw(count, primcount))
        {
            context->validationError(GL_INVALID_OPERATION, err::kTransformFeedbackBufferTooSmall);
            return false;
        }
    }

    return ValidateDrawArraysAttribs(context, first, count);
}

ANGLE_INLINE bool ValidateDrawElementsBase(const Context *context,
                                           PrimitiveMode mode,
                                           DrawElementsType type)
{
    if (!context->getStateCache().isValidDrawElementsType(type))
    {
        if (type == DrawElementsType::UnsignedInt)
        {
            context->validationError(GL_INVALID_ENUM, err::kTypeNotUnsignedShortByte);
            return false;
        }

        ASSERT(type == DrawElementsType::InvalidEnum);
        context->validationError(GL_INVALID_ENUM, err::kEnumNotSupported);
        return false;
    }

    intptr_t drawElementsError = context->getStateCache().getBasicDrawElementsError(context);
    if (drawElementsError)
    {
        // All errors from ValidateDrawElementsStates return INVALID_OPERATION.
        const char *errorMessage = reinterpret_cast<const char *>(drawElementsError);
        context->validationError(GL_INVALID_OPERATION, errorMessage);
        return false;
    }

    // Note that we are missing overflow checks for active transform feedback buffers.
    return true;
}

ANGLE_INLINE bool ValidateDrawElementsCommon(const Context *context,
                                             PrimitiveMode mode,
                                             GLsizei count,
                                             DrawElementsType type,
                                             const void *indices,
                                             GLsizei primcount)
{
    if (!ValidateDrawElementsBase(context, mode, type))
    {
        return false;
    }

    ASSERT(isPow2(GetDrawElementsTypeSize(type)) && GetDrawElementsTypeSize(type) > 0);

    if (context->getExtensions().webglCompatibility)
    {
        GLuint typeBytes = GetDrawElementsTypeSize(type);

        if ((reinterpret_cast<uintptr_t>(indices) & static_cast<uintptr_t>(typeBytes - 1)) != 0)
        {
            // [WebGL 1.0] Section 6.4 Buffer Offset and Stride Requirements
            // The offset arguments to drawElements and [...], must be a multiple of the size of the
            // data type passed to the call, or an INVALID_OPERATION error is generated.
            context->validationError(GL_INVALID_OPERATION, err::kOffsetMustBeMultipleOfType);
            return false;
        }

        // [WebGL 1.0] Section 6.4 Buffer Offset and Stride Requirements
        // In addition the offset argument to drawElements must be non-negative or an INVALID_VALUE
        // error is generated.
        if (reinterpret_cast<intptr_t>(indices) < 0)
        {
            context->validationError(GL_INVALID_VALUE, err::kNegativeOffset);
            return false;
        }
    }

    if (count <= 0)
    {
        if (count < 0)
        {
            context->validationError(GL_INVALID_VALUE, err::kNegativeCount);
            return false;
        }

        // Early exit.
        return ValidateDrawBase(context, mode);
    }

    if (!ValidateDrawBase(context, mode))
    {
        return false;
    }

    const State &state         = context->getState();
    const VertexArray *vao     = state.getVertexArray();
    Buffer *elementArrayBuffer = vao->getElementArrayBuffer();

    if (!elementArrayBuffer)
    {
        if (!indices)
        {
            // This is an application error that would normally result in a crash, but we catch
            // it and return an error
            context->validationError(GL_INVALID_OPERATION, err::kElementArrayNoBufferOrPointer);
            return false;
        }
    }
    else
    {
        // The max possible type size is 8 and count is on 32 bits so doing the multiplication
        // in a 64 bit integer is safe. Also we are guaranteed that here count > 0.
        static_assert(std::is_same<int, GLsizei>::value, "GLsizei isn't the expected type");
        constexpr uint64_t kMaxTypeSize = 8;
        constexpr uint64_t kIntMax      = std::numeric_limits<int>::max();
        constexpr uint64_t kUint64Max   = std::numeric_limits<uint64_t>::max();
        static_assert(kIntMax < kUint64Max / kMaxTypeSize, "");

        uint64_t elementCount = static_cast<uint64_t>(count);
        ASSERT(elementCount > 0 && GetDrawElementsTypeSize(type) <= kMaxTypeSize);

        // Doing the multiplication here is overflow-safe
        uint64_t elementDataSizeNoOffset = elementCount << GetDrawElementsTypeShift(type);

        // The offset can be any value, check for overflows
        uint64_t offset = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indices));
        uint64_t elementDataSizeWithOffset = elementDataSizeNoOffset + offset;
        if (elementDataSizeWithOffset < elementDataSizeNoOffset)
        {
            context->validationError(GL_INVALID_OPERATION, err::kIntegerOverflow);
            return false;
        }

        // Related to possible test bug: https://github.com/KhronosGroup/WebGL/issues/3064
        if ((elementDataSizeWithOffset > static_cast<uint64_t>(elementArrayBuffer->getSize())) &&
            (primcount > 0))
        {
            context->validationError(GL_INVALID_OPERATION, err::kInsufficientBufferSize);
            return false;
        }
    }

    if (context->isBufferAccessValidationEnabled() && primcount > 0)
    {
        // Use the parameter buffer to retrieve and cache the index range.
        IndexRange indexRange{IndexRange::Undefined()};
        ANGLE_VALIDATION_TRY(vao->getIndexRange(context, type, count, indices, &indexRange));

        // If we use an index greater than our maximum supported index range, return an error.
        // The ES3 spec does not specify behaviour here, it is undefined, but ANGLE should
        // always return an error if possible here.
        if (static_cast<GLint64>(indexRange.end) >= context->getCaps().maxElementIndex)
        {
            context->validationError(GL_INVALID_OPERATION, err::kExceedsMaxElement);
            return false;
        }

        if (!ValidateDrawAttribs(context, static_cast<GLint>(indexRange.end)))
        {
            return false;
        }

        // No op if there are no real indices in the index data (all are primitive restart).
        return (indexRange.vertexIndexCount > 0);
    }

    return true;
}
}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES_H_
