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
#include "libANGLE/validationES.h"
#include "libANGLE/validationES2.h"
#include "libANGLE/validationES32.h"

namespace gl
{
using namespace err;

namespace
{
template <typename ObjectT>
bool ValidateGetImageFormatAndType(const Context *context, ObjectT *obj, GLenum format, GLenum type)
{
    GLenum implFormat = obj->getImplementationColorReadFormat(context);
    if (!ValidES3Format(format) && (format != implFormat || format == GL_NONE))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
        return false;
    }

    GLenum implType = obj->getImplementationColorReadType(context);
    if (!ValidES3Type(type) && (type != implType || type == GL_NONE))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidType);
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

bool IsValidMemoryObjectParamater(const Context *context, GLenum pname)
{
    switch (pname)
    {
        case GL_DEDICATED_MEMORY_OBJECT_EXT:
            return true;

        default:
            return false;
    }
}

}  // namespace

bool ValidateGetTexImageANGLE(const Context *context,
                              TextureTarget target,
                              GLint level,
                              GLenum format,
                              GLenum type,
                              const void *pixels)
{
    if (!context->getExtensions().getImageANGLE)
    {
        context->validationError(GL_INVALID_OPERATION, kGetImageExtensionNotEnabled);
        return false;
    }

    if (!ValidTexture2DDestinationTarget(context, target) &&
        !ValidTexture3DDestinationTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    if (level < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeLevel);
        return false;
    }

    TextureType textureType = TextureTargetToType(target);
    if (!ValidMipLevel(context, textureType, level))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
        return false;
    }

    Texture *texture = context->getTextureByTarget(target);

    if (!ValidateGetImageFormatAndType(context, texture, format, type))
    {
        return false;
    }

    GLsizei width  = static_cast<GLsizei>(texture->getWidth(target, level));
    GLsizei height = static_cast<GLsizei>(texture->getHeight(target, level));
    if (!ValidatePixelPack(context, format, type, 0, 0, width, height, -1, nullptr, pixels))
    {
        return false;
    }

    return true;
}

bool ValidateGetRenderbufferImageANGLE(const Context *context,
                                       GLenum target,
                                       GLenum format,
                                       GLenum type,
                                       const void *pixels)
{
    if (!context->getExtensions().getImageANGLE)
    {
        context->validationError(GL_INVALID_OPERATION, kGetImageExtensionNotEnabled);
        return false;
    }

    if (target != GL_RENDERBUFFER)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidRenderbufferTarget);
        return false;
    }

    Renderbuffer *renderbuffer = context->getState().getCurrentRenderbuffer();

    if (!ValidateGetImageFormatAndType(context, renderbuffer, format, type))
    {
        return false;
    }

    GLsizei width  = renderbuffer->getWidth();
    GLsizei height = renderbuffer->getHeight();
    if (!ValidatePixelPack(context, format, type, 0, 0, width, height, -1, nullptr, pixels))
    {
        return false;
    }

    return true;
}

bool ValidateDrawElementsBaseVertexEXT(const Context *context,
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

bool ValidateDrawElementsInstancedBaseVertexEXT(const Context *context,
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

bool ValidateDrawRangeElementsBaseVertexEXT(const Context *context,
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

bool ValidateMultiDrawElementsBaseVertexEXT(const Context *context,
                                            PrimitiveMode mode,
                                            const GLsizei *count,
                                            DrawElementsType type,
                                            const void *const *indices,
                                            GLsizei drawcount,
                                            const GLint *basevertex)
{
    return true;
}

bool ValidateDrawElementsBaseVertexOES(const Context *context,
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

bool ValidateDrawElementsInstancedBaseVertexOES(const Context *context,
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

bool ValidateDrawRangeElementsBaseVertexOES(const Context *context,
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

bool ValidateBlendEquationSeparateiEXT(const Context *context,
                                       GLuint buf,
                                       GLenum modeRGB,
                                       GLenum modeAlpha)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendEquationSeparatei(context, buf, modeRGB, modeAlpha);
}

bool ValidateBlendEquationiEXT(const Context *context, GLuint buf, GLenum mode)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendEquationi(context, buf, mode);
}

bool ValidateBlendFuncSeparateiEXT(const Context *context,
                                   GLuint buf,
                                   GLenum srcRGB,
                                   GLenum dstRGB,
                                   GLenum srcAlpha,
                                   GLenum dstAlpha)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendFuncSeparatei(context, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

bool ValidateBlendFunciEXT(const Context *context, GLuint buf, GLenum src, GLenum dst)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendFunci(context, buf, src, dst);
}

bool ValidateColorMaskiEXT(const Context *context,
                           GLuint index,
                           GLboolean r,
                           GLboolean g,
                           GLboolean b,
                           GLboolean a)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateColorMaski(context, index, r, g, b, a);
}

bool ValidateDisableiEXT(const Context *context, GLenum target, GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDisablei(context, target, index);
}

bool ValidateEnableiEXT(const Context *context, GLenum target, GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateEnablei(context, target, index);
}

bool ValidateIsEnablediEXT(const Context *context, GLenum target, GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedEXT)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateIsEnabledi(context, target, index);
}

bool ValidateBlendEquationSeparateiOES(const Context *context,
                                       GLuint buf,
                                       GLenum modeRGB,
                                       GLenum modeAlpha)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendEquationSeparatei(context, buf, modeRGB, modeAlpha);
}

bool ValidateBlendEquationiOES(const Context *context, GLuint buf, GLenum mode)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendEquationi(context, buf, mode);
}

bool ValidateBlendFuncSeparateiOES(const Context *context,
                                   GLuint buf,
                                   GLenum srcRGB,
                                   GLenum dstRGB,
                                   GLenum srcAlpha,
                                   GLenum dstAlpha)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendFuncSeparatei(context, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

bool ValidateBlendFunciOES(const Context *context, GLuint buf, GLenum src, GLenum dst)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateBlendFunci(context, buf, src, dst);
}

bool ValidateColorMaskiOES(const Context *context,
                           GLuint index,
                           GLboolean r,
                           GLboolean g,
                           GLboolean b,
                           GLboolean a)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateColorMaski(context, index, r, g, b, a);
}

bool ValidateDisableiOES(const Context *context, GLenum target, GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateDisablei(context, target, index);
}

bool ValidateEnableiOES(const Context *context, GLenum target, GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateEnablei(context, target, index);
}

bool ValidateIsEnablediOES(const Context *context, GLenum target, GLuint index)
{
    if (!context->getExtensions().drawBuffersIndexedOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateIsEnabledi(context, target, index);
}

bool ValidateGetInteger64vEXT(const Context *context, GLenum pname, const GLint64 *data)
{
    if (!context->getExtensions().disjointTimerQuery)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    GLenum nativeType      = GL_NONE;
    unsigned int numParams = 0;
    if (!ValidateStateQuery(context, pname, &nativeType, &numParams))
    {
        return false;
    }

    return true;
}

bool ValidateCopyImageSubDataEXT(const Context *context,
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
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateCopyImageSubDataBase(context, srcName, srcTarget, srcLevel, srcX, srcY, srcZ,
                                        dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth,
                                        srcHeight, srcDepth);
}

bool ValidateCopyImageSubDataOES(const Context *context,
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
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateCopyImageSubDataBase(context, srcName, srcTarget, srcLevel, srcX, srcY, srcZ,
                                        dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth,
                                        srcHeight, srcDepth);
}

bool ValidateBufferStorageMemEXT(const Context *context,
                                 TextureType target,
                                 GLsizeiptr size,
                                 MemoryObjectID memory,
                                 GLuint64 offset)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateCreateMemoryObjectsEXT(const Context *context,
                                    GLsizei n,
                                    const MemoryObjectID *memoryObjects)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteMemoryObjectsEXT(const Context *context,
                                    GLsizei n,
                                    const MemoryObjectID *memoryObjects)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGetMemoryObjectParameterivEXT(const Context *context,
                                           MemoryObjectID memoryObject,
                                           GLenum pname,
                                           const GLint *params)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const MemoryObject *memory = context->getMemoryObject(memoryObject);
    if (memory == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMemoryObject);
    }

    if (!IsValidMemoryObjectParamater(context, pname))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidMemoryObjectParameter);
        return false;
    }

    return true;
}

bool ValidateGetUnsignedBytevEXT(const Context *context, GLenum pname, const GLubyte *data)
{
    if (!context->getExtensions().memoryObject && !context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateGetUnsignedBytei_vEXT(const Context *context,
                                   GLenum target,
                                   GLuint index,
                                   const GLubyte *data)
{
    if (!context->getExtensions().memoryObject && !context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateIsMemoryObjectEXT(const Context *context, MemoryObjectID memoryObject)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateMemoryObjectParameterivEXT(const Context *context,
                                        MemoryObjectID memoryObject,
                                        GLenum pname,
                                        const GLint *params)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    const MemoryObject *memory = context->getMemoryObject(memoryObject);
    if (memory == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMemoryObject);
        return false;
    }

    if (memory->isImmutable())
    {
        context->validationError(GL_INVALID_OPERATION, kImmutableMemoryObject);
        return false;
    }

    if (!IsValidMemoryObjectParamater(context, pname))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidMemoryObjectParameter);
        return false;
    }

    return true;
}

bool ValidateTexStorageMem2DEXT(const Context *context,
                                TextureType target,
                                GLsizei levels,
                                GLenum internalFormat,
                                GLsizei width,
                                GLsizei height,
                                MemoryObjectID memory,
                                GLuint64 offset)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2TexStorageParametersBase(context, target, levels, internalFormat, width,
                                                   height);
    }

    ASSERT(context->getClientMajorVersion() >= 3);
    return ValidateES3TexStorage2DParameters(context, target, levels, internalFormat, width, height,
                                             1);
}

bool ValidateTexStorageMem3DEXT(const Context *context,
                                TextureType target,
                                GLsizei levels,
                                GLenum internalFormat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                MemoryObjectID memory,
                                GLuint64 offset)
{
    if (!context->getExtensions().memoryObject)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateImportMemoryFdEXT(const Context *context,
                               MemoryObjectID memory,
                               GLuint64 size,
                               HandleType handleType,
                               GLint fd)
{
    if (!context->getExtensions().memoryObjectFd)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::OpaqueFd:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateImportMemoryZirconHandleANGLE(const Context *context,
                                           MemoryObjectID memory,
                                           GLuint64 size,
                                           HandleType handleType,
                                           GLuint handle)
{
    if (!context->getExtensions().memoryObjectFuchsiaANGLE)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::ZirconVmo:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateDeleteSemaphoresEXT(const Context *context, GLsizei n, const SemaphoreID *semaphores)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGenSemaphoresEXT(const Context *context, GLsizei n, const SemaphoreID *semaphores)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateGetSemaphoreParameterui64vEXT(const Context *context,
                                           SemaphoreID semaphore,
                                           GLenum pname,
                                           const GLuint64 *params)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateIsSemaphoreEXT(const Context *context, SemaphoreID semaphore)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return true;
}

bool ValidateSemaphoreParameterui64vEXT(const Context *context,
                                        SemaphoreID semaphore,
                                        GLenum pname,
                                        const GLuint64 *params)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    UNIMPLEMENTED();
    return false;
}

bool ValidateSignalSemaphoreEXT(const Context *context,
                                SemaphoreID semaphore,
                                GLuint numBufferBarriers,
                                const BufferID *buffers,
                                GLuint numTextureBarriers,
                                const TextureID *textures,
                                const GLenum *dstLayouts)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    for (GLuint i = 0; i < numTextureBarriers; ++i)
    {
        if (!IsValidImageLayout(FromGLenum<ImageLayout>(dstLayouts[i])))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidImageLayout);
            return false;
        }
    }

    return true;
}

bool ValidateWaitSemaphoreEXT(const Context *context,
                              SemaphoreID semaphore,
                              GLuint numBufferBarriers,
                              const BufferID *buffers,
                              GLuint numTextureBarriers,
                              const TextureID *textures,
                              const GLenum *srcLayouts)
{
    if (!context->getExtensions().semaphore)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    for (GLuint i = 0; i < numTextureBarriers; ++i)
    {
        if (!IsValidImageLayout(FromGLenum<ImageLayout>(srcLayouts[i])))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidImageLayout);
            return false;
        }
    }

    return true;
}

bool ValidateImportSemaphoreFdEXT(const Context *context,
                                  SemaphoreID semaphore,
                                  HandleType handleType,
                                  GLint fd)
{
    if (!context->getExtensions().semaphoreFd)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::OpaqueFd:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateImportSemaphoreZirconHandleANGLE(const Context *context,
                                              SemaphoreID semaphore,
                                              HandleType handleType,
                                              GLuint handle)
{
    if (!context->getExtensions().semaphoreFuchsiaANGLE)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    switch (handleType)
    {
        case HandleType::ZirconEvent:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidHandleType);
            return false;
    }

    return true;
}

bool ValidateTexStorageMemFlags2DANGLE(const Context *context,
                                       TextureType targetPacked,
                                       GLsizei levels,
                                       GLenum internalFormat,
                                       GLsizei width,
                                       GLsizei height,
                                       MemoryObjectID memoryPacked,
                                       GLuint64 offset,
                                       GLbitfield createFlags,
                                       GLbitfield usageFlags)
{
    if (!context->getExtensions().memoryObjectFlagsANGLE)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!ValidateTexStorageMem2DEXT(context, targetPacked, levels, internalFormat, width, height,
                                    memoryPacked, offset))
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
        context->validationError(GL_INVALID_VALUE, kInvalidExternalCreateFlags);
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
        context->validationError(GL_INVALID_VALUE, kInvalidExternalUsageFlags);
        return false;
    }

    return true;
}

bool ValidateTexStorageMemFlags2DMultisampleANGLE(const Context *context,
                                                  TextureType targetPacked,
                                                  GLsizei samples,
                                                  GLenum internalFormat,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLboolean fixedSampleLocations,
                                                  MemoryObjectID memoryPacked,
                                                  GLuint64 offset,
                                                  GLbitfield createFlags,
                                                  GLbitfield usageFlags)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateTexStorageMemFlags3DANGLE(const Context *context,
                                       TextureType targetPacked,
                                       GLsizei levels,
                                       GLenum internalFormat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       MemoryObjectID memoryPacked,
                                       GLuint64 offset,
                                       GLbitfield createFlags,
                                       GLbitfield usageFlags)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateTexStorageMemFlags3DMultisampleANGLE(const Context *context,
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
                                                  GLbitfield usageFlags)
{
    UNIMPLEMENTED();
    return false;
}

// GL_EXT_buffer_storage
bool ValidateBufferStorageEXT(const Context *context,
                              BufferBinding targetPacked,
                              GLsizeiptr size,
                              const void *data,
                              GLbitfield flags)
{
    if (!context->isValidBufferBinding(targetPacked))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBufferTypes);
        return false;
    }

    if (size < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeSize);
        return false;
    }

    constexpr GLbitfield kAllUsageFlags =
        (GL_DYNAMIC_STORAGE_BIT_EXT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT |
         GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_PERSISTENT_BIT_EXT | GL_CLIENT_STORAGE_BIT_EXT);
    if ((flags & ~kAllUsageFlags) != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidBufferUsageFlags);
        return false;
    }

    if (((flags & GL_MAP_PERSISTENT_BIT_EXT) != 0) &&
        ((flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == 0))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidBufferUsageFlags);
        return false;
    }

    if (((flags & GL_MAP_COHERENT_BIT_EXT) != 0) && ((flags & GL_MAP_PERSISTENT_BIT_EXT) == 0))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidBufferUsageFlags);
        return false;
    }

    Buffer *buffer = context->getState().getTargetBuffer(targetPacked);

    if (buffer == nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kBufferNotBound);
        return false;
    }

    if (buffer->isImmutable())
    {
        context->validationError(GL_INVALID_OPERATION, kBufferImmutable);
        return false;
    }

    return true;
}

// GL_EXT_external_buffer
bool ValidateBufferStorageExternalEXT(const Context *context,
                                      BufferBinding targetPacked,
                                      GLintptr offset,
                                      GLsizeiptr size,
                                      GLeglClientBufferEXT clientBuffer,
                                      GLbitfield flags)
{
    UNIMPLEMENTED();
    return false;
}

bool ValidateNamedBufferStorageExternalEXT(const Context *context,
                                           GLuint buffer,
                                           GLintptr offset,
                                           GLsizeiptr size,
                                           GLeglClientBufferEXT clientBuffer,
                                           GLbitfield flags)
{
    UNIMPLEMENTED();
    return false;
}
}  // namespace gl
