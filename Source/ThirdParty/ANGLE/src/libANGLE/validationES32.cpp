//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES32.cpp: Validation functions for OpenGL ES 3.2 entry point parameters

#include "libANGLE/validationES32_autogen.h"

#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/validationES.h"
#include "libANGLE/validationES2_autogen.h"
#include "libANGLE/validationES31_autogen.h"
#include "libANGLE/validationES3_autogen.h"

#include "common/utilities.h"

using namespace angle;

namespace gl
{
using namespace err;

bool ValidateBlendBarrier(Context *context)
{
    return true;
}

bool ValidateBlendEquationSeparatei(Context *context, GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
    return true;
}

bool ValidateBlendEquationi(Context *context, GLuint buf, GLenum mode)
{
    return true;
}

bool ValidateBlendFuncSeparatei(Context *context,
                                GLuint buf,
                                GLenum srcRGB,
                                GLenum dstRGB,
                                GLenum srcAlpha,
                                GLenum dstAlpha)
{
    return true;
}

bool ValidateBlendFunci(Context *context, GLuint buf, GLenum src, GLenum dst)
{
    return true;
}

bool ValidateColorMaski(Context *context,
                        GLuint index,
                        GLboolean r,
                        GLboolean g,
                        GLboolean b,
                        GLboolean a)
{
    return true;
}

bool ValidateCopyImageSubData(Context *context,
                              GLuint srcName,
                              GLenum srcTarget,
                              GLint srcLevel,
                              GLint srcX,
                              GLint srcY,
                              GLint srcZ,
                              GLuint dstName,
                              GLenum dstTarget,
                              GLint dstLevel,
                              GLint dstX,
                              GLint dstY,
                              GLint dstZ,
                              GLsizei srcWidth,
                              GLsizei srcHeight,
                              GLsizei srcDepth)
{
    return true;
}

bool ValidateDebugMessageCallback(Context *context, GLDEBUGPROC callback, const void *userParam)
{
    return true;
}

bool ValidateDebugMessageControl(Context *context,
                                 GLenum source,
                                 GLenum type,
                                 GLenum severity,
                                 GLsizei count,
                                 const GLuint *ids,
                                 GLboolean enabled)
{
    return true;
}

bool ValidateDebugMessageInsert(Context *context,
                                GLenum source,
                                GLenum type,
                                GLuint id,
                                GLenum severity,
                                GLsizei length,
                                const GLchar *buf)
{
    return true;
}

bool ValidateDisablei(Context *context, GLenum target, GLuint index)
{
    return true;
}

bool ValidateDrawElementsBaseVertex(Context *context,
                                    PrimitiveMode mode,
                                    GLsizei count,
                                    DrawElementsType type,
                                    const void *indices,
                                    GLint basevertex)
{
    if (!context->getExtensions().drawElementsBaseVertexAny())
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawElementsCommon(context, mode, count, type, indices, 1);
}

bool ValidateDrawElementsInstancedBaseVertex(Context *context,
                                             PrimitiveMode mode,
                                             GLsizei count,
                                             DrawElementsType type,
                                             const void *indices,
                                             GLsizei instancecount,
                                             GLint basevertex)
{
    if (!context->getExtensions().drawElementsBaseVertexAny())
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawElementsInstancedBase(context, mode, count, type, indices, instancecount);
}

bool ValidateDrawRangeElementsBaseVertex(Context *context,
                                         PrimitiveMode mode,
                                         GLuint start,
                                         GLuint end,
                                         GLsizei count,
                                         DrawElementsType type,
                                         const void *indices,
                                         GLint basevertex)
{
    if (!context->getExtensions().drawElementsBaseVertexAny())
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (end < start)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidElementRange);
        return false;
    }

    if (!ValidateDrawElementsCommon(context, mode, count, type, indices, 0))
    {
        return false;
    }

    // Skip range checks for no-op calls.
    if (count <= 0)
    {
        return true;
    }

    // Note that resolving the index range is a bit slow. We should probably optimize this.
    IndexRange indexRange;
    ANGLE_VALIDATION_TRY(context->getState().getVertexArray()->getIndexRange(context, type, count,
                                                                             indices, &indexRange));

    if (indexRange.end > end || indexRange.start < start)
    {
        // GL spec says that behavior in this case is undefined - generating an error is fine.
        context->validationError(GL_INVALID_OPERATION, kExceedsElementRange);
        return false;
    }
    return true;
}

bool ValidateEnablei(Context *context, GLenum target, GLuint index)
{
    return true;
}

bool ValidateFramebufferTexture(Context *context,
                                GLenum target,
                                GLenum attachment,
                                TextureID texture,
                                GLint level)
{
    return true;
}

bool ValidateGetDebugMessageLog(Context *context,
                                GLuint count,
                                GLsizei bufSize,
                                GLenum *sources,
                                GLenum *types,
                                GLuint *ids,
                                GLenum *severities,
                                GLsizei *lengths,
                                GLchar *messageLog)
{
    return true;
}

bool ValidateGetGraphicsResetStatus(Context *context)
{
    return true;
}

bool ValidateGetObjectLabel(Context *context,
                            GLenum identifier,
                            GLuint name,
                            GLsizei bufSize,
                            GLsizei *length,
                            GLchar *label)
{
    return true;
}

bool ValidateGetObjectPtrLabel(Context *context,
                               const void *ptr,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLchar *label)
{
    return true;
}

bool ValidateGetPointerv(Context *context, GLenum pname, void **params)
{
    Version clientVersion = context->getClientVersion();

    if ((clientVersion == ES_1_0) || (clientVersion == ES_1_1))
    {
        switch (pname)
        {
            case GL_VERTEX_ARRAY_POINTER:
            case GL_NORMAL_ARRAY_POINTER:
            case GL_COLOR_ARRAY_POINTER:
            case GL_TEXTURE_COORD_ARRAY_POINTER:
            case GL_POINT_SIZE_ARRAY_POINTER_OES:
                return true;
            default:
                context->validationError(GL_INVALID_ENUM, kInvalidPointerQuery);
                return false;
        }
    }
    else if (clientVersion == ES_3_2)
    {
        switch (pname)
        {
            case GL_DEBUG_CALLBACK_FUNCTION:
            case GL_DEBUG_CALLBACK_USER_PARAM:
                return true;
            default:
                context->validationError(GL_INVALID_ENUM, kInvalidPointerQuery);
                return false;
        }
    }
    else
    {
        context->validationError(GL_INVALID_OPERATION, kES1or32Required);
        return false;
    }
}

bool ValidateGetSamplerParameterIiv(Context *context,
                                    SamplerID sampler,
                                    GLenum pname,
                                    GLint *params)
{
    return true;
}

bool ValidateGetSamplerParameterIuiv(Context *context,
                                     SamplerID sampler,
                                     GLenum pname,
                                     GLuint *params)
{
    return true;
}

bool ValidateGetTexParameterIiv(Context *context,
                                TextureType targetPacked,
                                GLenum pname,
                                GLint *params)
{
    return true;
}

bool ValidateGetTexParameterIuiv(Context *context,
                                 TextureType targetPacked,
                                 GLenum pname,
                                 GLuint *params)
{
    return true;
}

bool ValidateGetnUniformfv(Context *context,
                           ShaderProgramID program,
                           GLint location,
                           GLsizei bufSize,
                           GLfloat *params)
{
    return true;
}

bool ValidateGetnUniformiv(Context *context,
                           ShaderProgramID program,
                           GLint location,
                           GLsizei bufSize,
                           GLint *params)
{
    return true;
}

bool ValidateGetnUniformuiv(Context *context,
                            ShaderProgramID program,
                            GLint location,
                            GLsizei bufSize,
                            GLuint *params)
{
    return true;
}

bool ValidateIsEnabledi(Context *context, GLenum target, GLuint index)
{
    return true;
}

bool ValidateMinSampleShading(Context *context, GLfloat value)
{
    return true;
}

bool ValidateObjectLabel(Context *context,
                         GLenum identifier,
                         GLuint name,
                         GLsizei length,
                         const GLchar *label)
{
    return true;
}

bool ValidateObjectPtrLabel(Context *context, const void *ptr, GLsizei length, const GLchar *label)
{
    return true;
}

bool ValidatePatchParameteri(Context *context, GLenum pname, GLint value)
{
    return true;
}

bool ValidatePopDebugGroup(Context *context)
{
    return true;
}

bool ValidatePrimitiveBoundingBox(Context *context,
                                  GLfloat minX,
                                  GLfloat minY,
                                  GLfloat minZ,
                                  GLfloat minW,
                                  GLfloat maxX,
                                  GLfloat maxY,
                                  GLfloat maxZ,
                                  GLfloat maxW)
{
    return true;
}

bool ValidatePushDebugGroup(Context *context,
                            GLenum source,
                            GLuint id,
                            GLsizei length,
                            const GLchar *message)
{
    return true;
}

bool ValidateReadnPixels(Context *context,
                         GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         GLsizei bufSize,
                         void *data)
{
    return true;
}

bool ValidateSamplerParameterIiv(Context *context,
                                 SamplerID sampler,
                                 GLenum pname,
                                 const GLint *param)
{
    return true;
}

bool ValidateSamplerParameterIuiv(Context *context,
                                  SamplerID sampler,
                                  GLenum pname,
                                  const GLuint *param)
{
    return true;
}

bool ValidateTexBuffer(Context *context, GLenum target, GLenum internalformat, BufferID buffer)
{
    return true;
}

bool ValidateTexBufferRange(Context *context,
                            GLenum target,
                            GLenum internalformat,
                            BufferID buffer,
                            GLintptr offset,
                            GLsizeiptr size)
{
    return true;
}

bool ValidateTexParameterIiv(Context *context,
                             TextureType targetPacked,
                             GLenum pname,
                             const GLint *params)
{
    return true;
}

bool ValidateTexParameterIuiv(Context *context,
                              TextureType targetPacked,
                              GLenum pname,
                              const GLuint *params)
{
    return true;
}

bool ValidateTexStorage3DMultisample(Context *context,
                                     TextureType targetPacked,
                                     GLsizei samples,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLboolean fixedsamplelocations)
{
    return true;
}

}  // namespace gl
