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

bool ValidateBlendBarrier(const Context *context)
{
    return true;
}

bool ValidateBlendEquationSeparatei(const Context *context,
                                    GLuint buf,
                                    GLenum modeRGB,
                                    GLenum modeAlpha)
{
    if (buf >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
    {
        context->validationError(GL_INVALID_VALUE, kExceedsMaxDrawBuffers);
        return false;
    }

    if (!ValidateBlendEquationSeparate(context, modeRGB, modeAlpha))
    {
        // error already generated
        return false;
    }

    return true;
}

bool ValidateBlendEquationi(const Context *context, GLuint buf, GLenum mode)
{
    if (buf >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
    {
        context->validationError(GL_INVALID_VALUE, kExceedsMaxDrawBuffers);
        return false;
    }

    if (!ValidateBlendEquation(context, mode))
    {
        // error already generated
        return false;
    }

    return true;
}

bool ValidateBlendFuncSeparatei(const Context *context,
                                GLuint buf,
                                GLenum srcRGB,
                                GLenum dstRGB,
                                GLenum srcAlpha,
                                GLenum dstAlpha)
{
    if (buf >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
    {
        context->validationError(GL_INVALID_VALUE, kExceedsMaxDrawBuffers);
        return false;
    }

    if (!ValidateBlendFuncSeparate(context, srcRGB, dstRGB, srcAlpha, dstAlpha))
    {
        // error already generated
        return false;
    }

    return true;
}

bool ValidateBlendFunci(const Context *context, GLuint buf, GLenum src, GLenum dst)
{
    if (buf >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
    {
        context->validationError(GL_INVALID_VALUE, kExceedsMaxDrawBuffers);
        return false;
    }

    if (!ValidateBlendFunc(context, src, dst))
    {
        // error already generated
        return false;
    }

    return true;
}

bool ValidateColorMaski(const Context *context,
                        GLuint index,
                        GLboolean r,
                        GLboolean g,
                        GLboolean b,
                        GLboolean a)
{
    if (index >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxDrawBuffer);
        return false;
    }

    return true;
}

bool ValidateCopyImageSubData(const Context *context,
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

bool ValidateDebugMessageCallback(const Context *context,
                                  GLDEBUGPROC callback,
                                  const void *userParam)
{
    return true;
}

bool ValidateDebugMessageControl(const Context *context,
                                 GLenum source,
                                 GLenum type,
                                 GLenum severity,
                                 GLsizei count,
                                 const GLuint *ids,
                                 GLboolean enabled)
{
    return true;
}

bool ValidateDebugMessageInsert(const Context *context,
                                GLenum source,
                                GLenum type,
                                GLuint id,
                                GLenum severity,
                                GLsizei length,
                                const GLchar *buf)
{
    return true;
}

bool ValidateDisablei(const Context *context, GLenum target, GLuint index)
{
    switch (target)
    {
        case GL_BLEND:
            if (index >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxDrawBuffer);
                return false;
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }
    return true;
}

bool ValidateDrawElementsBaseVertex(const Context *context,
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

bool ValidateDrawElementsInstancedBaseVertex(const Context *context,
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

bool ValidateDrawRangeElementsBaseVertex(const Context *context,
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

    return true;
}

bool ValidateEnablei(const Context *context, GLenum target, GLuint index)
{
    switch (target)
    {
        case GL_BLEND:
            if (index >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxDrawBuffer);
                return false;
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }
    return true;
}

bool ValidateFramebufferTexture(const Context *context,
                                GLenum target,
                                GLenum attachment,
                                TextureID texture,
                                GLint level)
{
    return true;
}

bool ValidateGetDebugMessageLog(const Context *context,
                                GLuint count,
                                GLsizei bufSize,
                                const GLenum *sources,
                                const GLenum *types,
                                const GLuint *ids,
                                const GLenum *severities,
                                const GLsizei *lengths,
                                const GLchar *messageLog)
{
    return true;
}

bool ValidateGetGraphicsResetStatus(const Context *context)
{
    return true;
}

bool ValidateGetObjectLabel(const Context *context,
                            GLenum identifier,
                            GLuint name,
                            GLsizei bufSize,
                            const GLsizei *length,
                            const GLchar *label)
{
    return true;
}

bool ValidateGetObjectPtrLabel(const Context *context,
                               const void *ptr,
                               GLsizei bufSize,
                               const GLsizei *length,
                               const GLchar *label)
{
    return true;
}

bool ValidateGetPointerv(const Context *context, GLenum pname, void *const *params)
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

bool ValidateGetSamplerParameterIiv(const Context *context,
                                    SamplerID sampler,
                                    GLenum pname,
                                    const GLint *params)
{
    return true;
}

bool ValidateGetSamplerParameterIuiv(const Context *context,
                                     SamplerID sampler,
                                     GLenum pname,
                                     const GLuint *params)
{
    return true;
}

bool ValidateGetTexParameterIiv(const Context *context,
                                TextureType targetPacked,
                                GLenum pname,
                                const GLint *params)
{
    return true;
}

bool ValidateGetTexParameterIuiv(const Context *context,
                                 TextureType targetPacked,
                                 GLenum pname,
                                 const GLuint *params)
{
    return true;
}

bool ValidateGetnUniformfv(const Context *context,
                           ShaderProgramID program,
                           UniformLocation location,
                           GLsizei bufSize,
                           const GLfloat *params)
{
    return true;
}

bool ValidateGetnUniformiv(const Context *context,
                           ShaderProgramID program,
                           UniformLocation location,
                           GLsizei bufSize,
                           const GLint *params)
{
    return true;
}

bool ValidateGetnUniformuiv(const Context *context,
                            ShaderProgramID program,
                            UniformLocation location,
                            GLsizei bufSize,
                            const GLuint *params)
{
    return true;
}

bool ValidateIsEnabledi(const Context *context, GLenum target, GLuint index)
{
    switch (target)
    {
        case GL_BLEND:
            if (index >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxDrawBuffer);
                return false;
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }
    return true;
}

bool ValidateMinSampleShading(const Context *context, GLfloat value)
{
    return true;
}

bool ValidateObjectLabel(const Context *context,
                         GLenum identifier,
                         GLuint name,
                         GLsizei length,
                         const GLchar *label)
{
    return true;
}

bool ValidateObjectPtrLabel(const Context *context,
                            const void *ptr,
                            GLsizei length,
                            const GLchar *label)
{
    return true;
}

bool ValidatePatchParameteri(const Context *context, GLenum pname, GLint value)
{
    return true;
}

bool ValidatePopDebugGroup(const Context *context)
{
    return true;
}

bool ValidatePrimitiveBoundingBox(const Context *context,
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

bool ValidatePushDebugGroup(const Context *context,
                            GLenum source,
                            GLuint id,
                            GLsizei length,
                            const GLchar *message)
{
    return true;
}

bool ValidateReadnPixels(const Context *context,
                         GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         GLsizei bufSize,
                         const void *data)
{
    return true;
}

bool ValidateSamplerParameterIiv(const Context *context,
                                 SamplerID sampler,
                                 GLenum pname,
                                 const GLint *param)
{
    return true;
}

bool ValidateSamplerParameterIuiv(const Context *context,
                                  SamplerID sampler,
                                  GLenum pname,
                                  const GLuint *param)
{
    return true;
}

bool ValidateTexBuffer(const Context *context,
                       GLenum target,
                       GLenum internalformat,
                       BufferID buffer)
{
    return true;
}

bool ValidateTexBufferRange(const Context *context,
                            GLenum target,
                            GLenum internalformat,
                            BufferID buffer,
                            GLintptr offset,
                            GLsizeiptr size)
{
    return true;
}

bool ValidateTexParameterIiv(const Context *context,
                             TextureType targetPacked,
                             GLenum pname,
                             const GLint *params)
{
    return true;
}

bool ValidateTexParameterIuiv(const Context *context,
                              TextureType targetPacked,
                              GLenum pname,
                              const GLuint *params)
{
    return true;
}

bool ValidateTexStorage3DMultisample(const Context *context,
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
