//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// validationES2.h:
//  Inlined validation functions for OpenGL ES 2.0 entry points.

#ifndef LIBANGLE_VALIDATION_ES2_H_
#define LIBANGLE_VALIDATION_ES2_H_

#include "libANGLE/ErrorStrings.h"
#include "libANGLE/validationES.h"
#include "libANGLE/validationES2_autogen.h"

namespace gl
{
ANGLE_INLINE bool ValidateDrawArrays(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     PrimitiveMode mode,
                                     GLint first,
                                     GLsizei count)
{
    return ValidateDrawArraysCommon(context, entryPoint, mode, first, count, 1);
}

ANGLE_INLINE bool ValidateUniform2f(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    UniformLocation location,
                                    GLfloat x,
                                    GLfloat y)
{
    return ValidateUniform(context, entryPoint, GL_FLOAT_VEC2, location, 1);
}

ANGLE_INLINE bool ValidateBindBuffer(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     BufferBinding target,
                                     BufferID buffer)
{
    if (!context->isValidBufferBinding(target))
    {
        ANGLE_VALIDATION_ERROR(GL_INVALID_ENUM, err::kInvalidBufferTypes);
        return false;
    }

    if (!context->getState().isBindGeneratesResourceEnabled() &&
        !context->isBufferGenerated(buffer))
    {
        ANGLE_VALIDATION_ERROR(GL_INVALID_OPERATION, err::kObjectNotGenerated);
        return false;
    }

    return true;
}

ANGLE_INLINE bool ValidateDrawElements(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       PrimitiveMode mode,
                                       GLsizei count,
                                       DrawElementsType type,
                                       const void *indices)
{
    return ValidateDrawElementsCommon(context, entryPoint, mode, count, type, indices, 1);
}

ANGLE_INLINE bool ValidateVertexAttribPointer(const Context *context,
                                              angle::EntryPoint entryPoint,
                                              GLuint index,
                                              GLint size,
                                              VertexAttribType type,
                                              GLboolean normalized,
                                              GLsizei stride,
                                              const void *ptr)
{
    if (!ValidateFloatVertexFormat(context, entryPoint, index, size, type))
    {
        return false;
    }

    if (stride < 0)
    {
        ANGLE_VALIDATION_ERROR(GL_INVALID_VALUE, err::kNegativeStride);
        return false;
    }

    if (context->getClientVersion() >= ES_3_1)
    {
        const Caps &caps = context->getCaps();
        if (stride > caps.maxVertexAttribStride)
        {
            ANGLE_VALIDATION_ERROR(GL_INVALID_VALUE, err::kExceedsMaxVertexAttribStride);
            return false;
        }

        if (index >= static_cast<GLuint>(caps.maxVertexAttribBindings))
        {
            ANGLE_VALIDATION_ERROR(GL_INVALID_VALUE, err::kExceedsMaxVertexAttribBindings);
            return false;
        }
    }

    // [OpenGL ES 3.0.2] Section 2.8 page 24:
    // An INVALID_OPERATION error is generated when a non-zero vertex array object
    // is bound, zero is bound to the ARRAY_BUFFER buffer object binding point,
    // and the pointer argument is not NULL.
    bool nullBufferAllowed = context->getState().areClientArraysEnabled() &&
                             context->getState().getVertexArray()->id().value == 0;
    if (!nullBufferAllowed && context->getState().getTargetBuffer(BufferBinding::Array) == 0 &&
        ptr != nullptr)
    {
        ANGLE_VALIDATION_ERROR(GL_INVALID_OPERATION, err::kClientDataInVertexArray);
        return false;
    }

    if (context->isWebGL())
    {
        // WebGL 1.0 [Section 6.14] Fixed point support
        // The WebGL API does not support the GL_FIXED data type.
        if (type == VertexAttribType::Fixed)
        {
            ANGLE_VALIDATION_ERROR(GL_INVALID_ENUM, err::kFixedNotInWebGL);
            return false;
        }

        if (!ValidateWebGLVertexAttribPointer(context, entryPoint, type, normalized, stride, ptr,
                                              false))
        {
            return false;
        }
    }

    return true;
}

void RecordBindTextureTypeError(const Context *context,
                                angle::EntryPoint entryPoint,
                                TextureType target);

ANGLE_INLINE bool ValidateBindTexture(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      TextureType target,
                                      TextureID texture)
{
    if (!context->getStateCache().isValidBindTextureType(target))
    {
        RecordBindTextureTypeError(context, entryPoint, target);
        return false;
    }

    if (texture.value == 0)
    {
        return true;
    }

    Texture *textureObject = context->getTexture(texture);
    if (textureObject && textureObject->getType() != target)
    {
        ANGLE_VALIDATION_ERRORF(GL_INVALID_OPERATION, err::kTextureTargetMismatchWithLabel,
                                static_cast<uint8_t>(target),
                                static_cast<uint8_t>(textureObject->getType()),
                                textureObject->getLabel().c_str());
        return false;
    }

    if (!context->getState().isBindGeneratesResourceEnabled() &&
        !context->isTextureGenerated(texture))
    {
        ANGLE_VALIDATION_ERROR(GL_INVALID_OPERATION, err::kObjectNotGenerated);
        return false;
    }

    return true;
}

// Validation of all Tex[Sub]Image2D parameters except TextureTarget.
bool ValidateES2TexImageParametersBase(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       TextureTarget target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isCompressed,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       GLsizei imageSize,
                                       const void *pixels);

// Validation of TexStorage*2DEXT
bool ValidateES2TexStorageParametersBase(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         TextureType target,
                                         GLsizei levels,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height);

// Validation of [Push,Pop]DebugGroup
bool ValidatePushDebugGroupBase(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLenum source,
                                GLuint id,
                                GLsizei length,
                                const GLchar *message);
bool ValidatePopDebugGroupBase(const Context *context, angle::EntryPoint entryPoint);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES2_H_
