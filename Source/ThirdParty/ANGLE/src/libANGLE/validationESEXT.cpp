//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// validationESEXT.cpp: Validation functions for OpenGL ES extension entry points.

#include "libANGLE/validationESEXT_autogen.h"

#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/MemoryObject.h"
#include "libANGLE/PixelLocalStorage.h"
#include "libANGLE/validationES.h"
#include "libANGLE/validationES2.h"
#include "libANGLE/validationES3.h"
#include "libANGLE/validationES31.h"
#include "libANGLE/validationES32.h"

namespace gl
{
using namespace err;

namespace
{
template <typename ObjectT>
bool ValidateGetImageFormatAndType(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   ObjectT *obj,
                                   GLenum format,
                                   GLenum type)
{
    GLenum implFormat = obj->getImplementationColorReadFormat(context);
    if (!ValidES3Format(format) && (format != implFormat || format == GL_NONE))
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidFormat);
        return false;
    }

    GLenum implType = obj->getImplementationColorReadType(context);
    if (!ValidES3Type(type) && (type != implType || type == GL_NONE))
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidType);
        return false;
    }

    // Format/type combinations are not yet validated.

    return true;
}

bool IsValidImageLayout(ImageLayout layout)
{
    switch (layout)
    {
        case ImageLayout::Undefined:
        case ImageLayout::General:
        case ImageLayout::ColorAttachment:
        case ImageLayout::DepthStencilAttachment:
        case ImageLayout::DepthStencilReadOnlyAttachment:
        case ImageLayout::ShaderReadOnly:
        case ImageLayout::TransferSrc:
        case ImageLayout::TransferDst:
        case ImageLayout::DepthReadOnlyStencilAttachment:
        case ImageLayout::DepthAttachmentStencilReadOnly:
            return true;

        default:
            return false;
    }
}

bool IsValidMemoryObjectParamater(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  GLenum pname)
{
    switch (pname)
    {
        case GL_DEDICATED_MEMORY_OBJECT_EXT:
            return true;

        case GL_PROTECTED_MEMORY_OBJECT_EXT:
            if (!context->getExtensions().protectedTexturesEXT)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
                return false;
            }
            return true;

        default:
            return false;
    }
}

bool ValidateObjectIdentifierAndName(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     GLenum identifier,
                                     GLuint name)
{
    bool isGLES11 = context->getClientVersion() == Version(1, 1);
    bool isGLES3  = context->getClientMajorVersion() >= 3;
    bool isGLES31 = context->getClientVersion() >= Version(3, 1);
    switch (identifier)
    {
        case GL_BUFFER_OBJECT_EXT:
            if (context->getBuffer({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidBufferName);
                return false;
            }
            return true;

        case GL_SHADER_OBJECT_EXT:
            if (isGLES11)
            {
                context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidType);
                return false;
            }
            if (context->getShader({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidShaderName);
                return false;
            }
            return true;

        case GL_PROGRAM_OBJECT_EXT:
            if (isGLES11)
            {
                context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidType);
                return false;
            }
            if (context->getProgramNoResolveLink({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidProgramName);
                return false;
            }
            return true;

        case GL_VERTEX_ARRAY_OBJECT_EXT:
            if (!isGLES3 && !context->getExtensions().vertexArrayObjectOES)
            {
                context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidType);
                return false;
            }
            if (context->getVertexArray({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidVertexArrayName);
                return false;
            }
            return true;

        case GL_QUERY_OBJECT_EXT:
            if (!isGLES3 && !context->getExtensions().occlusionQueryBooleanEXT)
            {
                context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidType);
                return false;
            }
            if (context->getQuery({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidQueryName);
                return false;
            }
            return true;

        case GL_TRANSFORM_FEEDBACK:
            if (!isGLES3)
            {
                context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidType);
                return false;
            }
            if (context->getTransformFeedback({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION,
                                         kInvalidTransformFeedbackName);
                return false;
            }
            return true;

        case GL_SAMPLER:
            if (!isGLES3)
            {
                context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidType);
                return false;
            }
            if (context->getSampler({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidSamplerName);
                return false;
            }
            return true;

        case GL_TEXTURE:
            if (context->getTexture({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidTextureName);
                return false;
            }
            return true;

        case GL_RENDERBUFFER:
            if (!context->isRenderbuffer({name}))
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION,
                                         kInvalidRenderbufferName);
                return false;
            }
            return true;

        case GL_FRAMEBUFFER:
            if (context->getFramebuffer({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidFramebufferName);
                return false;
            }
            return true;

        case GL_PROGRAM_PIPELINE_OBJECT_EXT:
            if (!isGLES31 && !context->getExtensions().separateShaderObjectsEXT)
            {
                context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidType);
                return false;
            }
            if (context->getProgramPipeline({name}) == nullptr)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION,
                                         kInvalidProgramPipelineName);
                return false;
            }
            return true;

        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidIndentifier);
            return false;
    }
}
}  // namespace

bool ValidateGetTexImage(const Context *context,
                         angle::EntryPoint entryPoint,
                         TextureTarget target,
                         GLint level)
{
    if (!context->getExtensions().getImageANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kGetImageExtensionNotEnabled);
        return false;
    }

    if (!ValidTexture2DDestinationTarget(context, target) &&
        !ValidTexture3DDestinationTarget(context, target))
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    if (level < 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kNegativeLevel);
        return false;
    }

    TextureType textureType = TextureTargetToType(target);
    if (!ValidMipLevel(context, textureType, level))
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidMipLevel);
        return false;
    }

    return true;
}

bool ValidateGetTexImageANGLE(const Context *context,
                              angle::EntryPoint entryPoint,
                              TextureTarget target,
                              GLint level,
                              GLenum format,
                              GLenum type,
                              const void *pixels)
{
    if (!ValidateGetTexImage(context, entryPoint, target, level))
    {
        return false;
    }

    Texture *texture = context->getTextureByTarget(target);

    if (!ValidateGetImageFormatAndType(context, entryPoint, texture, format, type))
    {
        return false;
    }

    GLsizei width  = static_cast<GLsizei>(texture->getWidth(target, level));
    GLsizei height = static_cast<GLsizei>(texture->getHeight(target, level));
    if (!ValidatePixelPack(context, entryPoint, format, type, 0, 0, width, height, -1, nullptr,
                           pixels))
    {
        return false;
    }

    if (texture->getFormat(target, level).info->compressed)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kGetImageCompressed);
        return false;
    }

    return true;
}

bool ValidateGetCompressedTexImageANGLE(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        TextureTarget target,
                                        GLint level,
                                        const void *pixels)
{
    if (!ValidateGetTexImage(context, entryPoint, target, level))
    {
        return false;
    }

    Texture *texture = context->getTextureByTarget(target);
    if (!texture->getFormat(target, level).info->compressed)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kGetImageNotCompressed);
        return false;
    }

    if (texture->isCompressedFormatEmulated(context, target, level))
    {
        // TODO (anglebug.com/7464): We can't currently read back from an emulated format
        context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidEmulatedFormat);
        return false;
    }

    return true;
}

bool ValidateGetRenderbufferImageANGLE(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       GLenum target,
                                       GLenum format,
                                       GLenum type,
                                       const void *pixels)
{
    if (!context->getExtensions().getImageANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kGetImageExtensionNotEnabled);
        return false;
    }

    if (target != GL_RENDERBUFFER)
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidRenderbufferTarget);
        return false;
    }

    Renderbuffer *renderbuffer = context->getState().getCurrentRenderbuffer();

    if (!ValidateGetImageFormatAndType(context, entryPoint, renderbuffer, format, type))
    {
        return false;
    }

    GLsizei width  = renderbuffer->getWidth();
    GLsizei height = renderbuffer->getHeight();
    if (!ValidatePixelPack(context, entryPoint, format, type, 0, 0, width, height, -1, nullptr,
                           pixels))
    {
        return false;
    }

    return true;
}

bool ValidateDrawElementsBaseVertexEXT(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       PrimitiveMode mode,
                                       GLsizei count,
                                       DrawElementsType type,
                                       const void *indices,
                                       GLint basevertex)
{
    if (!context->getExtensions().drawElementsBaseVertexAny())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawElementsCommon(context, entryPoint, mode, count, type, indices, 1);
}

bool ValidateDrawElementsInstancedBaseVertexEXT(const Context *context,
                                                angle::EntryPoint entryPoint,
                                                PrimitiveMode mode,
                                                GLsizei count,
                                                DrawElementsType type,
                                                const void *indices,
                                                GLsizei instancecount,
                                                GLint basevertex)
{
    if (!context->getExtensions().drawElementsBaseVertexAny())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawElementsInstancedBase(context, entryPoint, mode, count, type, indices,
                                             instancecount);
}

bool ValidateDrawRangeElementsBaseVertexEXT(const Context *context,
                                            angle::EntryPoint entryPoint,
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
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (end < start)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidElementRange);
        return false;
    }

    if (!ValidateDrawElementsCommon(context, entryPoint, mode, count, type, indices, 0))
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
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExceedsElementRange);
        return false;
    }
    return true;
}

bool ValidateMultiDrawElementsBaseVertexEXT(const Context *context,
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

bool ValidateMultiDrawArraysIndirectEXT(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        PrimitiveMode modePacked,
                                        const void *indirect,
                                        GLsizei drawcount,
                                        GLsizei stride)
{
    if (!ValidateMultiDrawIndirectBase(context, entryPoint, drawcount, stride))
    {
        return false;
    }

    if (!ValidateDrawArraysIndirect(context, entryPoint, modePacked, indirect))
    {
        return false;
    }

    return true;
}

bool ValidateMultiDrawElementsIndirectEXT(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          PrimitiveMode modePacked,
                                          DrawElementsType typePacked,
                                          const void *indirect,
                                          GLsizei drawcount,
                                          GLsizei stride)
{
    if (!ValidateMultiDrawIndirectBase(context, entryPoint, drawcount, stride))
    {
        return false;
    }

    const State &state                      = context->getState();
    TransformFeedback *curTransformFeedback = state.getCurrentTransformFeedback();
    if (!ValidateDrawElementsIndirect(context, entryPoint, modePacked, typePacked, indirect))
    {
        return false;
    }

    if (curTransformFeedback && curTransformFeedback->isActive() &&
        !curTransformFeedback->isPaused())
    {
        // EXT_geometry_shader allows transform feedback to work with all draw commands.
        // [EXT_geometry_shader] Section 12.1, "Transform Feedback"
        if (context->getExtensions().geometryShaderAny() || context->getClientVersion() >= ES_3_2)
        {
            if (!ValidateTransformFeedbackPrimitiveMode(
                    context, entryPoint, curTransformFeedback->getPrimitiveMode(), modePacked))
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION,
                                         kInvalidDrawModeTransformFeedback);
                return false;
            }
        }
        else
        {
            // An INVALID_OPERATION error is generated if transform feedback is active and not
            // paused.
            context->validationError(entryPoint, GL_INVALID_OPERATION,
                                     kUnsupportedDrawModeForTransformFeedback);
            return false;
        }
    }

    return true;
}

bool ValidateDrawArraysInstancedBaseInstanceEXT(const Context *context,
                                                angle::EntryPoint entryPoint,
                                                PrimitiveMode mode,
                                                GLint first,
                                                GLsizei count,
                                                GLsizei instanceCount,
                                                GLuint baseInstance)
{
    if (!context->getExtensions().baseInstanceEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawArraysInstancedBase(context, entryPoint, mode, first, count, instanceCount);
}

bool ValidateDrawElementsInstancedBaseInstanceEXT(const Context *context,
                                                  angle::EntryPoint entryPoint,
                                                  PrimitiveMode mode,
                                                  GLsizei count,
                                                  DrawElementsType type,
                                                  void const *indices,
                                                  GLsizei instancecount,
                                                  GLuint baseinstance)
{
    if (!context->getExtensions().baseInstanceEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawElementsInstancedBase(context, entryPoint, mode, count, type, indices,
                                             instancecount);
}

bool ValidateDrawElementsInstancedBaseVertexBaseInstanceEXT(const Context *context,
                                                            angle::EntryPoint entryPoint,
                                                            PrimitiveMode mode,
                                                            GLsizei count,
                                                            DrawElementsType typePacked,
                                                            const void *indices,
                                                            GLsizei instancecount,
                                                            GLint basevertex,
                                                            GLuint baseinstance)
{
    if (!context->getExtensions().baseInstanceEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawElementsInstancedBase(context, entryPoint, mode, count, typePacked, indices,
                                             instancecount);
}

bool ValidateDrawElementsBaseVertexOES(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       PrimitiveMode mode,
                                       GLsizei count,
                                       DrawElementsType type,
                                       const void *indices,
                                       GLint basevertex)
{
    if (!context->getExtensions().drawElementsBaseVertexAny())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawElementsCommon(context, entryPoint, mode, count, type, indices, 1);
}

bool ValidateDrawElementsInstancedBaseVertexOES(const Context *context,
                                                angle::EntryPoint entryPoint,
                                                PrimitiveMode mode,
                                                GLsizei count,
                                                DrawElementsType type,
                                                const void *indices,
                                                GLsizei instancecount,
                                                GLint basevertex)
{
    if (!context->getExtensions().drawElementsBaseVertexAny())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDrawElementsInstancedBase(context, entryPoint, mode, count, type, indices,
                                             instancecount);
}

bool ValidateDrawRangeElementsBaseVertexOES(const Context *context,
                                            angle::EntryPoint entryPoint,
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
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (end < start)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidElementRange);
        return false;
    }

    if (!ValidateDrawElementsCommon(context, entryPoint, mode, count, type, indices, 0))
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
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExceedsElementRange);
        return false;
    }
    return true;
}

// GL_KHR_blend_equation_advanced
bool ValidateBlendBarrierKHR(const Context *context, angle::EntryPoint entryPoint)
{
    const Extensions &extensions = context->getExtensions();

    if (!extensions.blendEquationAdvancedKHR)
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kAdvancedBlendExtensionNotEnabled);
    }

    return true;
}

bool ValidateBlendEquationSeparateiEXT(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       GLuint buf,
                                       GLenum modeRGB,
                                       GLenum modeAlpha)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendEquationSeparatei(context, entryPoint, buf, modeRGB, modeAlpha);
}

bool ValidateBlendEquationiEXT(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLuint buf,
                               GLenum mode)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendEquationi(context, entryPoint, buf, mode);
}

bool ValidateBlendFuncSeparateiEXT(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLuint buf,
                                   GLenum srcRGB,
                                   GLenum dstRGB,
                                   GLenum srcAlpha,
                                   GLenum dstAlpha)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendFuncSeparatei(context, entryPoint, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

bool ValidateBlendFunciEXT(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLuint buf,
                           GLenum src,
                           GLenum dst)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendFunci(context, entryPoint, buf, src, dst);
}

bool ValidateColorMaskiEXT(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLuint index,
                           GLboolean r,
                           GLboolean g,
                           GLboolean b,
                           GLboolean a)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateColorMaski(context, entryPoint, index, r, g, b, a);
}

bool ValidateDisableiEXT(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLenum target,
                         GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDisablei(context, entryPoint, target, index);
}

bool ValidateEnableiEXT(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum target,
                        GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateEnablei(context, entryPoint, target, index);
}

bool ValidateIsEnablediEXT(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum target,
                           GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateIsEnabledi(context, entryPoint, target, index);
}

bool ValidateBlendEquationSeparateiOES(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       GLuint buf,
                                       GLenum modeRGB,
                                       GLenum modeAlpha)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendEquationSeparatei(context, entryPoint, buf, modeRGB, modeAlpha);
}

bool ValidateBlendEquationiOES(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLuint buf,
                               GLenum mode)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendEquationi(context, entryPoint, buf, mode);
}

bool ValidateBlendFuncSeparateiOES(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLuint buf,
                                   GLenum srcRGB,
                                   GLenum dstRGB,
                                   GLenum srcAlpha,
                                   GLenum dstAlpha)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendFuncSeparatei(context, entryPoint, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

bool ValidateBlendFunciOES(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLuint buf,
                           GLenum src,
                           GLenum dst)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendFunci(context, entryPoint, buf, src, dst);
}

bool ValidateColorMaskiOES(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLuint index,
                           GLboolean r,
                           GLboolean g,
                           GLboolean b,
                           GLboolean a)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateColorMaski(context, entryPoint, index, r, g, b, a);
}

bool ValidateDisableiOES(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLenum target,
                         GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDisablei(context, entryPoint, target, index);
}

bool ValidateEnableiOES(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum target,
                        GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateEnablei(context, entryPoint, target, index);
}

bool ValidateIsEnablediOES(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum target,
                           GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateIsEnabledi(context, entryPoint, target, index);
}

bool ValidateGetInteger64vEXT(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum pname,
                              const GLint64 *data)
{
    if (!context->getExtensions().disjointTimerQueryEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    GLenum nativeType      = GL_NONE;
    unsigned int numParams = 0;
    if (!ValidateStateQuery(context, entryPoint, pname, &nativeType, &numParams))
    {
        return false;
    }

    return true;
}

bool ValidateCopyImageSubDataEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
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
    if (!context->getExtensions().copyImageEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateCopyImageSubDataBase(context, entryPoint, srcName, srcTarget, srcLevel, srcX,
                                        srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ,
                                        srcWidth, srcHeight, srcDepth);
}

bool ValidateCopyImageSubDataOES(const Context *context,
                                 angle::EntryPoint entryPoint,
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
    if (!context->getExtensions().copyImageEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateCopyImageSubDataBase(context, entryPoint, srcName, srcTarget, srcLevel, srcX,
                                        srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ,
                                        srcWidth, srcHeight, srcDepth);
}

bool ValidateBufferStorageMemEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 TextureType target,
                                 GLsizeiptr size,
                                 MemoryObjectID memory,
                                 GLuint64 offset)
{
    if (!context->getExtensions().memoryObjectEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateCreateMemoryObjectsEXT(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLsizei n,
                                    const MemoryObjectID *memoryObjects)
{
    if (!context->getExtensions().memoryObjectEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, entryPoint, n);
}

bool ValidateDeleteMemoryObjectsEXT(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLsizei n,
                                    const MemoryObjectID *memoryObjects)
{
    if (!context->getExtensions().memoryObjectEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, entryPoint, n);
}

bool ValidateGetMemoryObjectParameterivEXT(const Context *context,
                                           angle::EntryPoint entryPoint,
                                           MemoryObjectID memoryObject,
                                           GLenum pname,
                                           const GLint *params)
{
    if (!context->getExtensions().memoryObjectEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const MemoryObject *memory = context->getMemoryObject(memoryObject);
    if (memory == nullptr)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidMemoryObject);
    }

    if (!IsValidMemoryObjectParamater(context, entryPoint, pname))
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidMemoryObjectParameter);
        return false;
    }

    return true;
}

bool ValidateGetUnsignedBytevEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 GLenum pname,
                                 const GLubyte *data)
{
    if (!context->getExtensions().memoryObjectEXT && !context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateGetUnsignedBytei_vEXT(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLenum target,
                                   GLuint index,
                                   const GLubyte *data)
{
    if (!context->getExtensions().memoryObjectEXT && !context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateIsMemoryObjectEXT(const Context *context,
                               angle::EntryPoint entryPoint,
                               MemoryObjectID memoryObject)
{
    if (!context->getExtensions().memoryObjectEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateMemoryObjectParameterivEXT(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        MemoryObjectID memoryObject,
                                        GLenum pname,
                                        const GLint *params)
{
    if (!context->getExtensions().memoryObjectEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const MemoryObject *memory = context->getMemoryObject(memoryObject);
    if (memory == nullptr)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidMemoryObject);
        return false;
    }

    if (memory->isImmutable())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kImmutableMemoryObject);
        return false;
    }

    if (!IsValidMemoryObjectParamater(context, entryPoint, pname))
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidMemoryObjectParameter);
        return false;
    }

    return true;
}

bool ValidateTexStorageMem2DEXT(const Context *context,
                                angle::EntryPoint entryPoint,
                                TextureType target,
                                GLsizei levels,
                                GLenum internalFormat,
                                GLsizei width,
                                GLsizei height,
                                MemoryObjectID memory,
                                GLuint64 offset)
{
    if (!context->getExtensions().memoryObjectEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2TexStorageParametersBase(context, entryPoint, target, levels,
                                                   internalFormat, width, height);
    }

    ASSERT(context->getClientMajorVersion() >= 3);
    return ValidateES3TexStorage2DParameters(context, entryPoint, target, levels, internalFormat,
                                             width, height, 1);
}

bool ValidateTexStorageMem3DEXT(const Context *context,
                                angle::EntryPoint entryPoint,
                                TextureType target,
                                GLsizei levels,
                                GLenum internalFormat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                MemoryObjectID memory,
                                GLuint64 offset)
{
    if (!context->getExtensions().memoryObjectEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateImportMemoryFdEXT(const Context *context,
                               angle::EntryPoint entryPoint,
                               MemoryObjectID memory,
                               GLuint64 size,
                               HandleType handleType,
                               GLint fd)
{
    if (!context->getExtensions().memoryObjectFdEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::OpaqueFd:
            break;
        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateImportMemoryZirconHandleANGLE(const Context *context,
                                           angle::EntryPoint entryPoint,
                                           MemoryObjectID memory,
                                           GLuint64 size,
                                           HandleType handleType,
                                           GLuint handle)
{
    if (!context->getExtensions().memoryObjectFuchsiaANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::ZirconVmo:
            break;
        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateDeleteSemaphoresEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 GLsizei n,
                                 const SemaphoreID *semaphores)
{
    if (!context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, entryPoint, n);
}

bool ValidateGenSemaphoresEXT(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLsizei n,
                              const SemaphoreID *semaphores)
{
    if (!context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, entryPoint, n);
}

bool ValidateGetSemaphoreParameterui64vEXT(const Context *context,
                                           angle::EntryPoint entryPoint,
                                           SemaphoreID semaphore,
                                           GLenum pname,
                                           const GLuint64 *params)
{
    if (!context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateIsSemaphoreEXT(const Context *context,
                            angle::EntryPoint entryPoint,
                            SemaphoreID semaphore)
{
    if (!context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateSemaphoreParameterui64vEXT(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        SemaphoreID semaphore,
                                        GLenum pname,
                                        const GLuint64 *params)
{
    if (!context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateSignalSemaphoreEXT(const Context *context,
                                angle::EntryPoint entryPoint,
                                SemaphoreID semaphore,
                                GLuint numBufferBarriers,
                                const BufferID *buffers,
                                GLuint numTextureBarriers,
                                const TextureID *textures,
                                const GLenum *dstLayouts)
{
    if (!context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    for (GLuint i = 0; i < numBufferBarriers; ++i)
    {
        if (!context->getBuffer(buffers[i]))
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidBufferName);
            return false;
        }
    }

    for (GLuint i = 0; i < numTextureBarriers; ++i)
    {
        if (!context->getTexture(textures[i]))
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidTextureName);
            return false;
        }
        if (!IsValidImageLayout(FromGLenum<ImageLayout>(dstLayouts[i])))
        {
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidImageLayout);
            return false;
        }
    }

    return true;
}

bool ValidateWaitSemaphoreEXT(const Context *context,
                              angle::EntryPoint entryPoint,
                              SemaphoreID semaphore,
                              GLuint numBufferBarriers,
                              const BufferID *buffers,
                              GLuint numTextureBarriers,
                              const TextureID *textures,
                              const GLenum *srcLayouts)
{
    if (!context->getExtensions().semaphoreEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    for (GLuint i = 0; i < numBufferBarriers; ++i)
    {
        if (!context->getBuffer(buffers[i]))
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidBufferName);
            return false;
        }
    }

    for (GLuint i = 0; i < numTextureBarriers; ++i)
    {
        if (!context->getTexture(textures[i]))
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidTextureName);
            return false;
        }
        if (!IsValidImageLayout(FromGLenum<ImageLayout>(srcLayouts[i])))
        {
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidImageLayout);
            return false;
        }
    }

    return true;
}

bool ValidateImportSemaphoreFdEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  SemaphoreID semaphore,
                                  HandleType handleType,
                                  GLint fd)
{
    if (!context->getExtensions().semaphoreFdEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::OpaqueFd:
            break;
        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateGetSamplerParameterIivEXT(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       SamplerID samplerPacked,
                                       GLenum pname,
                                       const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGetSamplerParameterBase(context, entryPoint, samplerPacked, pname, nullptr);
}

bool ValidateGetSamplerParameterIuivEXT(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        SamplerID samplerPacked,
                                        GLenum pname,
                                        const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGetSamplerParameterBase(context, entryPoint, samplerPacked, pname, nullptr);
}

bool ValidateGetTexParameterIivEXT(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   TextureType targetPacked,
                                   GLenum pname,
                                   const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGetTexParameterBase(context, entryPoint, targetPacked, pname, nullptr);
}

bool ValidateGetTexParameterIuivEXT(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    TextureType targetPacked,
                                    GLenum pname,
                                    const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGetTexParameterBase(context, entryPoint, targetPacked, pname, nullptr);
}

bool ValidateSamplerParameterIivEXT(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    SamplerID samplerPacked,
                                    GLenum pname,
                                    const GLint *param)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateSamplerParameterBase(context, entryPoint, samplerPacked, pname, -1, true, param);
}

bool ValidateSamplerParameterIuivEXT(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     SamplerID samplerPacked,
                                     GLenum pname,
                                     const GLuint *param)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateSamplerParameterBase(context, entryPoint, samplerPacked, pname, -1, true, param);
}

bool ValidateTexParameterIivEXT(const Context *context,
                                angle::EntryPoint entryPoint,
                                TextureType targetPacked,
                                GLenum pname,
                                const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateTexParameterBase(context, entryPoint, targetPacked, pname, -1, true, params);
}

bool ValidateTexParameterIuivEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 TextureType targetPacked,
                                 GLenum pname,
                                 const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateTexParameterBase(context, entryPoint, targetPacked, pname, -1, true, params);
}

bool ValidateImportSemaphoreZirconHandleANGLE(const Context *context,
                                              angle::EntryPoint entryPoint,
                                              SemaphoreID semaphore,
                                              HandleType handleType,
                                              GLuint handle)
{
    if (!context->getExtensions().semaphoreFuchsiaANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::ZirconEvent:
            break;
        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

namespace
{
enum class PLSExpectedStatus : bool
{
    Inactive,
    Active
};

bool ValidatePLSCommon(const Context *context,
                       angle::EntryPoint entryPoint,
                       PLSExpectedStatus expectedStatus)
{
    // Check that the pixel local storage extension is enabled at all.
    if (!context->getExtensions().shaderPixelLocalStorageANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kPLSExtensionNotEnabled);
        return false;
    }

    // INVALID_FRAMEBUFFER_OPERATION is generated if the default framebuffer object name 0 is
    // bound to DRAW_FRAMEBUFFER.
    if (context->getState().getDrawFramebuffer()->id().value == 0)
    {
        context->validationError(entryPoint, GL_INVALID_FRAMEBUFFER_OPERATION,
                                 kPLSDefaultFramebufferBound);
        return false;
    }

    if (expectedStatus == PLSExpectedStatus::Inactive)
    {
        // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE is TRUE.
        if (context->getState().getPixelLocalStorageActive())
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kPLSActive);
            return false;
        }
    }
    else
    {
        ASSERT(expectedStatus == PLSExpectedStatus::Active);

        // INVALID_OPERATION is generated if PIXEL_LOCAL_STORAGE_ACTIVE_ANGLE is FALSE.
        if (!context->getState().getPixelLocalStorageActive())
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kPLSInactive);
            return false;
        }
    }

    return true;
}

bool ValidatePLSCommon(const Context *context, angle::EntryPoint entryPoint, GLint plane)
{
    if (!ValidatePLSCommon(context, entryPoint, PLSExpectedStatus::Inactive))
    {
        return false;
    }

    // INVALID_VALUE is generated if <plane> < 0 or <plane> >= MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    if (plane < 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kPLSPlaneLessThanZero);
        return false;
    }
    if (plane >= static_cast<GLint>(context->getCaps().maxPixelLocalStoragePlanes))
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kPLSPlaneOutOfRange);
        return false;
    }

    return true;
}

bool ValidatePLSInternalformat(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum internalformat)
{
    // INVALID_ENUM is generated if <internalformat> is not one of the acceptable values in Table
    // X.2, or NONE.
    switch (internalformat)
    {
        case GL_RGBA8:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_R32F:
        case GL_R32UI:
            return true;
        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kPLSInvalidInternalformat);
            return false;
    }
}

bool ValidatePLSTextureType(const Context *context,
                            angle::EntryPoint entryPoint,
                            Texture *tex,
                            size_t *textureDepth)
{
    // INVALID_ENUM is generated if <backingtexture> is nonzero and not of type GL_TEXTURE_2D,
    // GL_TEXTURE_CUBE_MAP, GL_TEXTURE_2D_ARRAY, or GL_TEXTURE_3D.
    switch (tex->getType())
    {
        case TextureType::_2D:
            *textureDepth = 1;
            return true;
        case TextureType::CubeMap:
            *textureDepth = 6;
            return true;
        case TextureType::_2DArray:
            *textureDepth = tex->getDepth(TextureTarget::_2DArray, 0);
            return true;
        case TextureType::_3D:
            *textureDepth = tex->getDepth(TextureTarget::_3D, 0);
            return true;
        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kPLSInvalidTextureType);
            return false;
    }
}

bool ValidatePLSLoadOperation(const Context *context, angle::EntryPoint entryPoint, GLenum loadop)
{
    // INVALID_ENUM is generated if <loadops>[0..<planes>-1] is not one of the Load Operations
    // enumerated in Table X.1.
    switch (loadop)
    {
        case GL_ZERO:
        case GL_CLEAR_ANGLE:
        case GL_KEEP:
        case GL_DONT_CARE:
        case GL_DISABLE_ANGLE:
            return true;
        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kPLSInvalidLoadOperation);
            return false;
    }
}
}  // namespace

bool ValidateFramebufferMemorylessPixelLocalStorageANGLE(const Context *context,
                                                         angle::EntryPoint entryPoint,
                                                         GLint plane,
                                                         GLenum internalformat)
{
    if (!ValidatePLSCommon(context, entryPoint, plane))
    {
        return false;
    }

    // INVALID_ENUM is generated if <internalformat> is not one of the acceptable values in Table
    // X.2, or NONE.
    if (internalformat != GL_NONE)
    {
        if (!ValidatePLSInternalformat(context, entryPoint, internalformat))
        {
            return false;
        }
    }

    return true;
}

bool ValidateFramebufferTexturePixelLocalStorageANGLE(const Context *context,
                                                      angle::EntryPoint entryPoint,
                                                      GLint plane,
                                                      TextureID backingtexture,
                                                      GLint level,
                                                      GLint layer)
{
    if (!ValidatePLSCommon(context, entryPoint, plane))
    {
        return false;
    }

    if (backingtexture.value != 0)
    {
        Texture *tex = context->getTexture(backingtexture);

        // INVALID_OPERATION is generated if <backingtexture> is not the name of an existing
        // immutable texture object, or zero.
        if (!tex)
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidTextureName);
            return false;
        }
        if (!tex->getImmutableFormat())
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kTextureIsNotImmutable);
            return false;
        }

        // INVALID_ENUM is generated if <backingtexture> is nonzero and not of type GL_TEXTURE_2D,
        // GL_TEXTURE_CUBE_MAP, GL_TEXTURE_2D_ARRAY, or GL_TEXTURE_3D.
        size_t textureDepth;
        if (!ValidatePLSTextureType(context, entryPoint, tex, &textureDepth))
        {
            return false;
        }

        // INVALID_VALUE is generated if <backingtexture> is nonzero and <level> < 0.
        if (level < 0)
        {
            context->validationError(entryPoint, GL_INVALID_VALUE, kNegativeLevel);
            return false;
        }

        // INVALID_VALUE is generated if <backingtexture> is nonzero and <level> >= the
        // immutable number of mipmap levels in <backingtexture>.
        if (static_cast<GLuint>(level) >= tex->getImmutableLevels())
        {
            context->validationError(entryPoint, GL_INVALID_VALUE, kTextureLevelOutOfRange);
            return false;
        }

        // INVALID_VALUE is generated if <backingtexture> is nonzero and <layer> < 0.
        if (layer < 0)
        {
            context->validationError(entryPoint, GL_INVALID_VALUE, kNegativeLayer);
            return false;
        }

        // INVALID_VALUE is generated if <backingtexture> is nonzero and <layer> >= the immutable
        // number of texture layers in <backingtexture>.
        if ((size_t)layer >= textureDepth)
        {
            context->validationError(entryPoint, GL_INVALID_VALUE, kTextureLayerOutOfRange);
            return false;
        }

        // INVALID_ENUM is generated if <backingtexture> is nonzero and its internalformat is not
        // one of the acceptable values in Table X.2.
        ASSERT(tex->getImmutableFormat());
        GLenum internalformat = tex->getState().getBaseLevelDesc().format.info->internalFormat;
        if (!ValidatePLSInternalformat(context, entryPoint, internalformat))
        {
            return false;
        }
    }

    return true;
}

bool ValidateBeginPixelLocalStorageANGLE(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         GLsizei planes,
                                         const GLenum loadops[],
                                         const void *cleardata)
{
    if (!ValidatePLSCommon(context, entryPoint, PLSExpectedStatus::Inactive))
    {
        return false;
    }

    const State &state             = context->getState();
    const Framebuffer *framebuffer = state.getDrawFramebuffer();

    // INVALID_OPERATION is generated if the value of SAMPLE_BUFFERS is 1 (i.e., if rendering to a
    // multisampled framebuffer).
    if (framebuffer->getSamples(context) != 0)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kPLSMultisamplingEnabled);
        return false;
    }

    // INVALID_OPERATION is generated if DITHER is enabled.
    if (state.isDitherEnabled())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kPLSDitherEnabled);
        return false;
    }

    // INVALID_OPERATION is generated if RASTERIZER_DISCARD is enabled.
    if (state.isRasterizerDiscardEnabled())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kPLSRasterizerDiscardEnabled);
        return false;
    }

    // INVALID_OPERATION is generated if SAMPLE_ALPHA_TO_COVERAGE is enabled.
    if (state.isSampleAlphaToCoverageEnabled())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION,
                                 kPLSSampleAlphaToCoverageEnabled);
        return false;
    }

    // INVALID_OPERATION is generated if SAMPLE_COVERAGE is enabled.
    if (state.isSampleCoverageEnabled())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kPLSSampleCoverageEnabled);
        return false;
    }

    // INVALID_VALUE is generated if <planes> < 1 or <planes> >
    // MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.
    if (planes < 1)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kPLSPlanesLessThanOne);
        return false;
    }
    if (planes > static_cast<GLsizei>(context->getCaps().maxPixelLocalStoragePlanes))
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kPLSPlanesOutOfRange);
        return false;
    }

    // INVALID_FRAMEBUFFER_OPERATION is generated if the draw framebuffer has an image attached to
    // any color attachment point on or after:
    //
    //   COLOR_ATTACHMENT0 +
    //   MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE
    //
    const Caps &caps = context->getCaps();
    for (int i = caps.maxColorAttachmentsWithActivePixelLocalStorage; i < caps.maxColorAttachments;
         ++i)
    {
        if (framebuffer->getColorAttachment(i))
        {
            context->validationError(entryPoint, GL_INVALID_FRAMEBUFFER_OPERATION,
                                     kPLSMaxColorAttachmentsExceded);
            return false;
        }
    }

    // INVALID_FRAMEBUFFER_OPERATION is generated if the draw framebuffer has an image attached to
    // any color attachment point on or after:
    //
    //   COLOR_ATTACHMENT0 +
    //   MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE -
    //   <planes>
    //
    for (GLuint i = caps.maxCombinedDrawBuffersAndPixelLocalStoragePlanes - planes;
         i < caps.maxColorAttachmentsWithActivePixelLocalStorage; ++i)
    {
        if (framebuffer->getColorAttachment(i))
        {
            context->validationError(entryPoint, GL_INVALID_FRAMEBUFFER_OPERATION,
                                     kPLSMaxCombinedDrawBuffersAndPlanesExceded);
            return false;
        }
    }

    // INVALID_VALUE is generated if <loadops> is NULL.
    if (!loadops)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kPLSNullLoadOps);
        return false;
    }

    const PixelLocalStorage *pls   = framebuffer->peekPixelLocalStorage();
    bool hasTextureBackedPLSPlanes = false;
    Extents textureBackedPLSExtents{};

    for (int i = 0; i < planes; ++i)
    {
        // INVALID_ENUM is generated if <loadops>[0..<planes>-1] is not one of the Load
        // Operations enumerated in Table X.1.
        if (!ValidatePLSLoadOperation(context, entryPoint, loadops[i]))
        {
            return false;
        }

        if (loadops[i] == GL_DISABLE_ANGLE)
        {
            continue;
        }

        // INVALID_VALUE is generated if <loadops>[0..<planes>-1] is CLEAR_ANGLE and <cleardata> is
        // NULL.
        if (loadops[i] == GL_CLEAR_ANGLE && !cleardata)
        {
            context->validationError(entryPoint, GL_INVALID_VALUE, kPLSNullClearData);
            return false;
        }

        // INVALID_OPERATION is generated if <loadops>[0..<planes>-1] is not DISABLE_ANGLE, and
        // the pixel local storage plane at that same index is is in a deinitialized state.
        if (pls == nullptr || pls->getPlane(i).isDeinitialized())
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION,
                                     kPLSEnablingDeinitializedPlane);
            return false;
        }

        // [ANGLE_shader_pixel_local_storage] Section 4.4.2.X "Configuring Pixel Local Storage
        // on a Framebuffer": When a texture object is deleted, any pixel local storage plane to
        // which it was bound is automatically converted to a memoryless plane of matching
        // internalformat.
        const PixelLocalStoragePlane &plane = pls->getPlane(i);

        Extents textureExtents;
        if (plane.getTextureImageExtents(context, &textureExtents))
        {
            // INVALID_OPERATION is generated if all enabled, texture-backed pixel local storage
            // planes do not have the same width and height.
            if (!hasTextureBackedPLSPlanes)
            {
                textureBackedPLSExtents   = textureExtents;
                hasTextureBackedPLSPlanes = true;
            }
            else if (textureExtents != textureBackedPLSExtents)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION,
                                         kPLSMismatchedBackingTextureSizes);
                return false;
            }
        }
        else
        {
            // INVALID_OPERATION is generated if <loadops>[0..<planes>-1] is KEEP and the pixel
            // local storage plane at that same index is memoryless.
            if (loadops[i] == GL_KEEP)
            {
                context->validationError(entryPoint, GL_INVALID_OPERATION,
                                         kPLSKeepingMemorylessPlane);
                return false;
            }
        }
    }

    const FramebufferAttachment *firstAttachment =
        framebuffer->getState().getFirstNonNullAttachment();
    if (firstAttachment)
    {
        // INVALID_OPERATION is generated if the draw framebuffer has other attachments, and its
        // enabled, texture-backed pixel local storage planes do not have identical dimensions
        // with the rendering area.
        if (hasTextureBackedPLSPlanes &&
            textureBackedPLSExtents != framebuffer->getState().getAttachmentExtentsIntersection())
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION,
                                     kPLSDimensionsDontMatchRenderingArea);
            return false;
        }
    }
    else
    {
        // INVALID_OPERATION is generated if the draw framebuffer has no attachments and no
        // enabled, texture-backed pixel local storage planes.
        if (!hasTextureBackedPLSPlanes)
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION,
                                     kPLSNoAttachmentsNoTextureBacked);
            return false;
        }
    }

    // INVALID_OPERATION is generated if a single texture image is bound to more than one pixel
    // local storage plane.
    //
    //   TODO(anglebug.com/7279): Block feedback loops
    //

    // INVALID_OPERATION is generated if a single texture image is simultaneously bound to a pixel
    // local storage plane and attached to the draw framebuffer.
    //
    //   TODO(anglebug.com/7279): Block feedback loops
    //

    return true;
}

bool ValidateEndPixelLocalStorageANGLE(const Context *context, angle::EntryPoint entryPoint)
{
    return ValidatePLSCommon(context, entryPoint, PLSExpectedStatus::Active);
}

bool ValidatePixelLocalStorageBarrierANGLE(const Context *context, angle::EntryPoint entryPoint)
{
    return ValidatePLSCommon(context, entryPoint, PLSExpectedStatus::Active);
}

bool ValidateFramebufferFetchBarrierEXT(const Context *context, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidatePatchParameteriEXT(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLenum pname,
                                GLint value)
{
    if (!context->getExtensions().tessellationShaderEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION,
                                 kTessellationShaderExtensionNotEnabled);
        return false;
    }

    if (pname != GL_PATCH_VERTICES)
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidPname);
        return false;
    }

    if (value <= 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidValueNonPositive);
        return false;
    }

    if (value > context->getCaps().maxPatchVertices)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidValueExceedsMaxPatchSize);
        return false;
    }

    return true;
}

bool ValidateTexStorageMemFlags2DANGLE(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       TextureType targetPacked,
                                       GLsizei levels,
                                       GLenum internalFormat,
                                       GLsizei width,
                                       GLsizei height,
                                       MemoryObjectID memoryPacked,
                                       GLuint64 offset,
                                       GLbitfield createFlags,
                                       GLbitfield usageFlags,
                                       const void *imageCreateInfoPNext)
{
    if (!context->getExtensions().memoryObjectFlagsANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!ValidateTexStorageMem2DEXT(context, entryPoint, targetPacked, levels, internalFormat,
                                    width, height, memoryPacked, offset))
    {
        return false;
    }

    // |createFlags| and |usageFlags| must only have bits specified by the extension.
    constexpr GLbitfield kAllCreateFlags =
        GL_CREATE_SPARSE_BINDING_BIT_ANGLE | GL_CREATE_SPARSE_RESIDENCY_BIT_ANGLE |
        GL_CREATE_SPARSE_ALIASED_BIT_ANGLE | GL_CREATE_MUTABLE_FORMAT_BIT_ANGLE |
        GL_CREATE_CUBE_COMPATIBLE_BIT_ANGLE | GL_CREATE_ALIAS_BIT_ANGLE |
        GL_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_ANGLE | GL_CREATE_2D_ARRAY_COMPATIBLE_BIT_ANGLE |
        GL_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT_ANGLE | GL_CREATE_EXTENDED_USAGE_BIT_ANGLE |
        GL_CREATE_PROTECTED_BIT_ANGLE | GL_CREATE_DISJOINT_BIT_ANGLE |
        GL_CREATE_CORNER_SAMPLED_BIT_ANGLE | GL_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_ANGLE |
        GL_CREATE_SUBSAMPLED_BIT_ANGLE;

    if ((createFlags & ~kAllCreateFlags) != 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidExternalCreateFlags);
        return false;
    }

    constexpr GLbitfield kAllUsageFlags =
        GL_USAGE_TRANSFER_SRC_BIT_ANGLE | GL_USAGE_TRANSFER_DST_BIT_ANGLE |
        GL_USAGE_SAMPLED_BIT_ANGLE | GL_USAGE_STORAGE_BIT_ANGLE |
        GL_USAGE_COLOR_ATTACHMENT_BIT_ANGLE | GL_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT_ANGLE |
        GL_USAGE_TRANSIENT_ATTACHMENT_BIT_ANGLE | GL_USAGE_INPUT_ATTACHMENT_BIT_ANGLE |
        GL_USAGE_SHADING_RATE_IMAGE_BIT_ANGLE | GL_USAGE_FRAGMENT_DENSITY_MAP_BIT_ANGLE;

    if ((usageFlags & ~kAllUsageFlags) != 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidExternalUsageFlags);
        return false;
    }

    return true;
}

bool ValidateTexStorageMemFlags2DMultisampleANGLE(const Context *context,
                                                  angle::EntryPoint entryPoint,
                                                  TextureType targetPacked,
                                                  GLsizei samples,
                                                  GLenum internalFormat,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLboolean fixedSampleLocations,
                                                  MemoryObjectID memoryPacked,
                                                  GLuint64 offset,
                                                  GLbitfield createFlags,
                                                  GLbitfield usageFlags,
                                                  const void *imageCreateInfoPNext)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateTexStorageMemFlags3DANGLE(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       TextureType targetPacked,
                                       GLsizei levels,
                                       GLenum internalFormat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       MemoryObjectID memoryPacked,
                                       GLuint64 offset,
                                       GLbitfield createFlags,
                                       GLbitfield usageFlags,
                                       const void *imageCreateInfoPNext)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateTexStorageMemFlags3DMultisampleANGLE(const Context *context,
                                                  angle::EntryPoint entryPoint,
                                                  TextureType targetPacked,
                                                  GLsizei samples,
                                                  GLenum internalFormat,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth,
                                                  GLboolean fixedSampleLocations,
                                                  MemoryObjectID memoryPacked,
                                                  GLuint64 offset,
                                                  GLbitfield createFlags,
                                                  GLbitfield usageFlags,
                                                  const void *imageCreateInfoPNext)
{
    UNIMPLEMENTED();
    return false;
}

// GL_EXT_buffer_storage
bool ValidateBufferStorageEXT(const Context *context,
                              angle::EntryPoint entryPoint,
                              BufferBinding targetPacked,
                              GLsizeiptr size,
                              const void *data,
                              GLbitfield flags)
{
    if (!context->isValidBufferBinding(targetPacked))
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidBufferTypes);
        return false;
    }

    if (size <= 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kNonPositiveSize);
        return false;
    }

    constexpr GLbitfield kAllUsageFlags =
        (GL_DYNAMIC_STORAGE_BIT_EXT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT |
         GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT | GL_CLIENT_STORAGE_BIT_EXT);
    if ((flags & ~kAllUsageFlags) != 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidBufferUsageFlags);
        return false;
    }

    if (((flags & GL_MAP_PERSISTENT_BIT_EXT) != 0) &&
        ((flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == 0))
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidBufferUsageFlags);
        return false;
    }

    if (((flags & GL_MAP_COHERENT_BIT_EXT) != 0) && ((flags & GL_MAP_PERSISTENT_BIT_EXT) == 0))
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidBufferUsageFlags);
        return false;
    }

    Buffer *buffer = context->getState().getTargetBuffer(targetPacked);

    if (buffer == nullptr)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kBufferNotBound);
        return false;
    }

    if (buffer->isImmutable())
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kBufferImmutable);
        return false;
    }

    return true;
}

// GL_EXT_clip_control
bool ValidateClipControlEXT(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLenum origin,
                            GLenum depth)
{
    if ((origin != GL_LOWER_LEFT_EXT) && (origin != GL_UPPER_LEFT_EXT))
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidOriginEnum);
        return false;
    }

    if ((depth != GL_NEGATIVE_ONE_TO_ONE_EXT) && (depth != GL_ZERO_TO_ONE_EXT))
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidDepthEnum);
        return false;
    }

    return true;
}

// GL_EXT_external_buffer
bool ValidateBufferStorageExternalEXT(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      BufferBinding targetPacked,
                                      GLintptr offset,
                                      GLsizeiptr size,
                                      GLeglClientBufferEXT clientBuffer,
                                      GLbitfield flags)
{
    if (!ValidateBufferStorageEXT(context, entryPoint, targetPacked, size, nullptr, flags))
    {
        return false;
    }

    if (offset != 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kExternalBufferInvalidOffset);
        return false;
    }

    if (clientBuffer == nullptr && size > 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kClientBufferInvalid);
        return false;
    }

    return true;
}

bool ValidateNamedBufferStorageExternalEXT(const Context *context,
                                           angle::EntryPoint entryPoint,
                                           GLuint buffer,
                                           GLintptr offset,
                                           GLsizeiptr size,
                                           GLeglClientBufferEXT clientBuffer,
                                           GLbitfield flags)
{
    UNIMPLEMENTED();
    return false;
}

// GL_EXT_primitive_bounding_box
bool ValidatePrimitiveBoundingBoxEXT(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     GLfloat minX,
                                     GLfloat minY,
                                     GLfloat minZ,
                                     GLfloat minW,
                                     GLfloat maxX,
                                     GLfloat maxY,
                                     GLfloat maxZ,
                                     GLfloat maxW)
{
    if (!context->getExtensions().primitiveBoundingBoxEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

// GL_OES_primitive_bounding_box
bool ValidatePrimitiveBoundingBoxOES(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     GLfloat minX,
                                     GLfloat minY,
                                     GLfloat minZ,
                                     GLfloat minW,
                                     GLfloat maxX,
                                     GLfloat maxY,
                                     GLfloat maxZ,
                                     GLfloat maxW)
{
    if (!context->getExtensions().primitiveBoundingBoxOES)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

// GL_EXT_separate_shader_objects
bool ValidateActiveShaderProgramEXT(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    ProgramPipelineID pipelinePacked,
                                    ShaderProgramID programPacked)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateActiveShaderProgramBase(context, entryPoint, pipelinePacked, programPacked);
}

bool ValidateBindProgramPipelineEXT(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    ProgramPipelineID pipelinePacked)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBindProgramPipelineBase(context, entryPoint, pipelinePacked);
}

bool ValidateCreateShaderProgramvEXT(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     ShaderType typePacked,
                                     GLsizei count,
                                     const GLchar **strings)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateCreateShaderProgramvBase(context, entryPoint, typePacked, count, strings);
}

bool ValidateDeleteProgramPipelinesEXT(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       GLsizei n,
                                       const ProgramPipelineID *pipelinesPacked)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDeleteProgramPipelinesBase(context, entryPoint, n, pipelinesPacked);
}

bool ValidateGenProgramPipelinesEXT(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLsizei n,
                                    const ProgramPipelineID *pipelinesPacked)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenProgramPipelinesBase(context, entryPoint, n, pipelinesPacked);
}

bool ValidateGetProgramPipelineInfoLogEXT(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ProgramPipelineID pipelinePacked,
                                          GLsizei bufSize,
                                          const GLsizei *length,
                                          const GLchar *infoLog)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGetProgramPipelineInfoLogBase(context, entryPoint, pipelinePacked, bufSize,
                                                 length, infoLog);
}

bool ValidateGetProgramPipelineivEXT(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     ProgramPipelineID pipelinePacked,
                                     GLenum pname,
                                     const GLint *params)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGetProgramPipelineivBase(context, entryPoint, pipelinePacked, pname, params);
}

bool ValidateIsProgramPipelineEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ProgramPipelineID pipelinePacked)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateIsProgramPipelineBase(context, entryPoint, pipelinePacked);
}

bool ValidateProgramParameteriEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  GLenum pname,
                                  GLint value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramParameteriBase(context, entryPoint, programPacked, pname, value);
}

bool ValidateProgramUniform1fEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLfloat v0)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform1fBase(context, entryPoint, programPacked, locationPacked, v0);
}

bool ValidateProgramUniform1fvEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLsizei count,
                                  const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform1fvBase(context, entryPoint, programPacked, locationPacked, count,
                                         value);
}

bool ValidateProgramUniform1iEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLint v0)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform1iBase(context, entryPoint, programPacked, locationPacked, v0);
}

bool ValidateProgramUniform1ivEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLsizei count,
                                  const GLint *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform1ivBase(context, entryPoint, programPacked, locationPacked, count,
                                         value);
}

bool ValidateProgramUniform1uiEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLuint v0)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform1uiBase(context, entryPoint, programPacked, locationPacked, v0);
}

bool ValidateProgramUniform1uivEXT(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   ShaderProgramID programPacked,
                                   UniformLocation locationPacked,
                                   GLsizei count,
                                   const GLuint *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform1uivBase(context, entryPoint, programPacked, locationPacked, count,
                                          value);
}

bool ValidateProgramUniform2fEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLfloat v0,
                                 GLfloat v1)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform2fBase(context, entryPoint, programPacked, locationPacked, v0, v1);
}

bool ValidateProgramUniform2fvEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLsizei count,
                                  const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform2fvBase(context, entryPoint, programPacked, locationPacked, count,
                                         value);
}

bool ValidateProgramUniform2iEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLint v0,
                                 GLint v1)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform2iBase(context, entryPoint, programPacked, locationPacked, v0, v1);
}

bool ValidateProgramUniform2ivEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLsizei count,
                                  const GLint *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform2ivBase(context, entryPoint, programPacked, locationPacked, count,
                                         value);
}

bool ValidateProgramUniform2uiEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLuint v0,
                                  GLuint v1)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform2uiBase(context, entryPoint, programPacked, locationPacked, v0,
                                         v1);
}

bool ValidateProgramUniform2uivEXT(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   ShaderProgramID programPacked,
                                   UniformLocation locationPacked,
                                   GLsizei count,
                                   const GLuint *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform2uivBase(context, entryPoint, programPacked, locationPacked, count,
                                          value);
}

bool ValidateProgramUniform3fEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLfloat v0,
                                 GLfloat v1,
                                 GLfloat v2)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform3fBase(context, entryPoint, programPacked, locationPacked, v0, v1,
                                        v2);
}

bool ValidateProgramUniform3fvEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLsizei count,
                                  const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform3fvBase(context, entryPoint, programPacked, locationPacked, count,
                                         value);
}

bool ValidateProgramUniform3iEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLint v0,
                                 GLint v1,
                                 GLint v2)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform3iBase(context, entryPoint, programPacked, locationPacked, v0, v1,
                                        v2);
}

bool ValidateProgramUniform3ivEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLsizei count,
                                  const GLint *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform3ivBase(context, entryPoint, programPacked, locationPacked, count,
                                         value);
}

bool ValidateProgramUniform3uiEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLuint v0,
                                  GLuint v1,
                                  GLuint v2)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform3uiBase(context, entryPoint, programPacked, locationPacked, v0, v1,
                                         v2);
}

bool ValidateProgramUniform3uivEXT(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   ShaderProgramID programPacked,
                                   UniformLocation locationPacked,
                                   GLsizei count,
                                   const GLuint *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform3uivBase(context, entryPoint, programPacked, locationPacked, count,
                                          value);
}

bool ValidateProgramUniform4fEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLfloat v0,
                                 GLfloat v1,
                                 GLfloat v2,
                                 GLfloat v3)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform4fBase(context, entryPoint, programPacked, locationPacked, v0, v1,
                                        v2, v3);
}

bool ValidateProgramUniform4fvEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLsizei count,
                                  const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform4fvBase(context, entryPoint, programPacked, locationPacked, count,
                                         value);
}

bool ValidateProgramUniform4iEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ShaderProgramID programPacked,
                                 UniformLocation locationPacked,
                                 GLint v0,
                                 GLint v1,
                                 GLint v2,
                                 GLint v3)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform4iBase(context, entryPoint, programPacked, locationPacked, v0, v1,
                                        v2, v3);
}

bool ValidateProgramUniform4ivEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLsizei count,
                                  const GLint *value)
{
    return ValidateProgramUniform4ivBase(context, entryPoint, programPacked, locationPacked, count,
                                         value);
}

bool ValidateProgramUniform4uiEXT(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  ShaderProgramID programPacked,
                                  UniformLocation locationPacked,
                                  GLuint v0,
                                  GLuint v1,
                                  GLuint v2,
                                  GLuint v3)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniform4uiBase(context, entryPoint, programPacked, locationPacked, v0, v1,
                                         v2, v3);
}

bool ValidateProgramUniform4uivEXT(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   ShaderProgramID programPacked,
                                   UniformLocation locationPacked,
                                   GLsizei count,
                                   const GLuint *value)
{
    return ValidateProgramUniform4uivBase(context, entryPoint, programPacked, locationPacked, count,
                                          value);
}

bool ValidateProgramUniformMatrix2fvEXT(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        ShaderProgramID programPacked,
                                        UniformLocation locationPacked,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix2fvBase(context, entryPoint, programPacked, locationPacked,
                                               count, transpose, value);
}

bool ValidateProgramUniformMatrix2x3fvEXT(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix2x3fvBase(context, entryPoint, programPacked, locationPacked,
                                                 count, transpose, value);
}

bool ValidateProgramUniformMatrix2x4fvEXT(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix2x4fvBase(context, entryPoint, programPacked, locationPacked,
                                                 count, transpose, value);
}

bool ValidateProgramUniformMatrix3fvEXT(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        ShaderProgramID programPacked,
                                        UniformLocation locationPacked,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix3fvBase(context, entryPoint, programPacked, locationPacked,
                                               count, transpose, value);
}

bool ValidateProgramUniformMatrix3x2fvEXT(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix3x2fvBase(context, entryPoint, programPacked, locationPacked,
                                                 count, transpose, value);
}

bool ValidateProgramUniformMatrix3x4fvEXT(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix3x4fvBase(context, entryPoint, programPacked, locationPacked,
                                                 count, transpose, value);
}

bool ValidateProgramUniformMatrix4fvEXT(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        ShaderProgramID programPacked,
                                        UniformLocation locationPacked,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix4fvBase(context, entryPoint, programPacked, locationPacked,
                                               count, transpose, value);
}

bool ValidateProgramUniformMatrix4x2fvEXT(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix4x2fvBase(context, entryPoint, programPacked, locationPacked,
                                                 count, transpose, value);
}

bool ValidateProgramUniformMatrix4x3fvEXT(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ShaderProgramID programPacked,
                                          UniformLocation locationPacked,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat *value)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateProgramUniformMatrix4x3fvBase(context, entryPoint, programPacked, locationPacked,
                                                 count, transpose, value);
}

bool ValidateUseProgramStagesEXT(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 ProgramPipelineID pipelinePacked,
                                 GLbitfield stages,
                                 ShaderProgramID programPacked)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateUseProgramStagesBase(context, entryPoint, pipelinePacked, stages, programPacked);
}

bool ValidateValidateProgramPipelineEXT(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        ProgramPipelineID pipelinePacked)
{
    if (!context->getExtensions().separateShaderObjectsEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateValidateProgramPipelineBase(context, entryPoint, pipelinePacked);
}

// GL_EXT_debug_label
bool ValidateGetObjectLabelEXT(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum type,
                               GLuint object,
                               GLsizei bufSize,
                               const GLsizei *length,
                               const GLchar *label)
{
    if (!context->getExtensions().debugLabelEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (bufSize < 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    return ValidateObjectIdentifierAndName(context, entryPoint, type, object);
}

bool ValidateLabelObjectEXT(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLenum type,
                            GLuint object,
                            GLsizei length,
                            const GLchar *label)
{
    if (!context->getExtensions().debugLabelEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (length < 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kNegativeLength);
        return false;
    }

    return ValidateObjectIdentifierAndName(context, entryPoint, type, object);
}

bool ValidateEGLImageTargetTextureStorageEXT(const Context *context,
                                             angle::EntryPoint entryPoint,
                                             GLuint texture,
                                             GLeglImageOES image,
                                             const GLint *attrib_list)
{
    UNREACHABLE();
    return false;
}

bool ValidateEGLImageTargetTexStorageEXT(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         GLenum target,
                                         GLeglImageOES image,
                                         const GLint *attrib_list)
{
    if (!context->getExtensions().EGLImageStorageEXT)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    gl::TextureType targetType = FromGLenum<TextureType>(target);
    switch (targetType)
    {
        case TextureType::External:
            if (!context->getExtensions().EGLImageExternalOES)
            {
                context->validationErrorF(entryPoint, GL_INVALID_ENUM, kEnumNotSupported,
                                          ToGLenum(targetType));
            }
            break;
        case TextureType::CubeMapArray:
            if (!context->getExtensions().textureCubeMapArrayAny())
            {
                context->validationErrorF(entryPoint, GL_INVALID_ENUM, kEnumNotSupported,
                                          ToGLenum(targetType));
            }
            break;
        case TextureType::_2D:
        case TextureType::_2DArray:
        case TextureType::_3D:
        case TextureType::CubeMap:
            break;
        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidTextureTarget);
            return false;
    }

    // Validate egl source image is valid
    egl::Image *imageObject = static_cast<egl::Image *>(image);
    if (!ValidateEGLImageObject(context, entryPoint, targetType, image))
    {
        return false;
    }

    // attrib list validation
    if (attrib_list != nullptr && attrib_list[0] != GL_NONE)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kAttributeListNotNull);
        return false;
    }

    GLsizei levelCount    = imageObject->getLevelCount();
    Extents size          = imageObject->getExtents();
    GLsizei width         = static_cast<GLsizei>(size.width);
    GLsizei height        = static_cast<GLsizei>(size.height);
    GLsizei depth         = static_cast<GLsizei>(size.depth);
    GLenum internalformat = imageObject->getFormat().info->sizedInternalFormat;

    if (width < 1 || height < 1 || depth < 1 || levelCount < 1)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kTextureSizeTooSmall);
        return false;
    }

    if (!ValidateES3TexStorageParametersLevel(context, entryPoint, targetType, levelCount, width,
                                              height, depth))
    {
        // Error already generated.
        return false;
    }

    if (targetType == TextureType::External)
    {
        const Caps &caps = context->getCaps();
        if (width > caps.max2DTextureSize || height > caps.max2DTextureSize)
        {
            context->validationError(entryPoint, GL_INVALID_VALUE, kResourceMaxTextureSize);
            return false;
        }
    }
    else if (!ValidateES3TexStorageParametersExtent(context, entryPoint, targetType, levelCount,
                                                    width, height, depth))
    {
        // Error already generated.
        return false;
    }

    if (!ValidateES3TexStorageParametersTexObject(context, entryPoint, targetType))
    {
        // Error already generated.
        return false;
    }

    if (!ValidateES3TexStorageParametersFormat(context, entryPoint, targetType, levelCount,
                                               internalformat, width, height, depth))
    {
        // Error already generated.
        return false;
    }

    return true;
}

bool ValidateAcquireTexturesANGLE(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  GLuint numTextures,
                                  const TextureID *textures,
                                  const GLenum *layouts)
{
    if (!context->getExtensions().vulkanImageANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    for (GLuint i = 0; i < numTextures; ++i)
    {
        if (!context->getTexture(textures[i]))
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidTextureName);
            return false;
        }
        if (!IsValidImageLayout(FromGLenum<ImageLayout>(layouts[i])))
        {
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidImageLayout);
            return false;
        }
    }

    return true;
}

bool ValidateReleaseTexturesANGLE(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  GLuint numTextures,
                                  const TextureID *textures,
                                  const GLenum *layouts)
{
    if (!context->getExtensions().vulkanImageANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    for (GLuint i = 0; i < numTextures; ++i)
    {
        if (!context->getTexture(textures[i]))
        {
            context->validationError(entryPoint, GL_INVALID_OPERATION, kInvalidTextureName);
            return false;
        }
    }

    return true;
}

bool ValidateFramebufferParameteriMESA(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       GLenum target,
                                       GLenum pname,
                                       GLint param)
{
    if (pname != GL_FRAMEBUFFER_FLIP_Y_MESA)
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidPname);
        return false;
    }
    return ValidateFramebufferParameteriBase(context, entryPoint, target, pname, param);
}

bool ValidateGetFramebufferParameterivMESA(const Context *context,
                                           angle::EntryPoint entryPoint,
                                           GLenum target,
                                           GLenum pname,
                                           const GLint *params)
{
    if (pname != GL_FRAMEBUFFER_FLIP_Y_MESA)
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidPname);
        return false;
    }
    return ValidateGetFramebufferParameterivBase(context, entryPoint, target, pname, params);
}

// GL_AMD_performance_monitor
bool ValidateBeginPerfMonitorAMD(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 GLuint monitor)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateDeletePerfMonitorsAMD(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLsizei n,
                                   const GLuint *monitors)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateEndPerfMonitorAMD(const Context *context, angle::EntryPoint entryPoint, GLuint monitor)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateGenPerfMonitorsAMD(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLsizei n,
                                const GLuint *monitors)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateGetPerfMonitorCounterDataAMD(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          GLuint monitor,
                                          GLenum pname,
                                          GLsizei dataSize,
                                          const GLuint *data,
                                          const GLint *bytesWritten)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (monitor != 0)
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidPerfMonitor);
        return false;
    }

    switch (pname)
    {
        case GL_PERFMON_RESULT_AVAILABLE_AMD:
        case GL_PERFMON_RESULT_SIZE_AMD:
        case GL_PERFMON_RESULT_AMD:
            break;

        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidPname);
            return false;
    }

    return true;
}

bool ValidateGetPerfMonitorCounterInfoAMD(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          GLuint group,
                                          GLuint counter,
                                          GLenum pname,
                                          const void *data)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const angle::PerfMonitorCounterGroups &groups = context->getPerfMonitorCounterGroups();

    if (group >= groups.size())
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidPerfMonitorGroup);
        return false;
    }

    if (counter >= groups[group].counters.size())
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidPerfMonitorCounter);
        return false;
    }

    switch (pname)
    {
        case GL_COUNTER_TYPE_AMD:
        case GL_COUNTER_RANGE_AMD:
            break;

        default:
            context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidPname);
            return false;
    }

    return true;
}

bool ValidateGetPerfMonitorCounterStringAMD(const Context *context,
                                            angle::EntryPoint entryPoint,
                                            GLuint group,
                                            GLuint counter,
                                            GLsizei bufSize,
                                            const GLsizei *length,
                                            const GLchar *counterString)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const angle::PerfMonitorCounterGroups &groups = context->getPerfMonitorCounterGroups();

    if (group >= groups.size())
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidPerfMonitorGroup);
        return false;
    }

    if (counter >= groups[group].counters.size())
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidPerfMonitorCounter);
        return false;
    }

    return true;
}

bool ValidateGetPerfMonitorCountersAMD(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       GLuint group,
                                       const GLint *numCounters,
                                       const GLint *maxActiveCounters,
                                       GLsizei counterSize,
                                       const GLuint *counters)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const angle::PerfMonitorCounterGroups &groups = context->getPerfMonitorCounterGroups();

    if (group >= groups.size())
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidPerfMonitorGroup);
        return false;
    }

    return true;
}

bool ValidateGetPerfMonitorGroupStringAMD(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          GLuint group,
                                          GLsizei bufSize,
                                          const GLsizei *length,
                                          const GLchar *groupString)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const angle::PerfMonitorCounterGroups &groups = context->getPerfMonitorCounterGroups();

    if (group >= groups.size())
    {
        context->validationError(entryPoint, GL_INVALID_VALUE, kInvalidPerfMonitorGroup);
        return false;
    }

    return true;
}

bool ValidateGetPerfMonitorGroupsAMD(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     const GLint *numGroups,
                                     GLsizei groupsSize,
                                     const GLuint *groups)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateSelectPerfMonitorCountersAMD(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          GLuint monitor,
                                          GLboolean enable,
                                          GLuint group,
                                          GLint numCounters,
                                          const GLuint *counterList)
{
    if (!context->getExtensions().performanceMonitorAMD)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateShadingRateQCOM(const Context *context, angle::EntryPoint entryPoint, GLenum rate)
{
    if (!context->getExtensions().shadingRateQCOM)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    gl::ShadingRate shadingRate = gl::FromGLenum<gl::ShadingRate>(rate);
    if (shadingRate == gl::ShadingRate::Undefined || shadingRate == gl::ShadingRate::InvalidEnum)
    {
        context->validationError(entryPoint, GL_INVALID_ENUM, kInvalidShadingRate);
        return false;
    }

    return true;
}

bool ValidateLogicOpANGLE(const Context *context,
                          angle::EntryPoint entryPoint,
                          LogicalOperation opcodePacked)
{
    if (!context->getExtensions().logicOpANGLE)
    {
        context->validationError(entryPoint, GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateLogicOpCommon(context, entryPoint, opcodePacked);
}
}  // namespace gl
