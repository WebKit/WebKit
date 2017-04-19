//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES.h: Validation functions for generic OpenGL ES entry point parameters

#include "libANGLE/validationES.h"

#include "libANGLE/validationES2.h"
#include "libANGLE/validationES3.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/Image.h"
#include "libANGLE/Query.h"
#include "libANGLE/Program.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexArray.h"

#include "common/mathutil.h"
#include "common/utilities.h"

using namespace angle;

namespace gl
{
const char *g_ExceedsMaxElementErrorMessage = "Element value exceeds maximum element index.";

namespace
{
bool ValidateDrawAttribs(ValidationContext *context,
                         GLint primcount,
                         GLint maxVertex,
                         GLint vertexCount)
{
    const gl::State &state     = context->getGLState();
    const gl::Program *program = state.getProgram();

    bool webglCompatibility = context->getExtensions().webglCompatibility;

    const VertexArray *vao    = state.getVertexArray();
    const auto &vertexAttribs = vao->getVertexAttributes();
    const auto &vertexBindings = vao->getVertexBindings();
    size_t maxEnabledAttrib   = vao->getMaxEnabledAttribute();
    for (size_t attributeIndex = 0; attributeIndex < maxEnabledAttrib; ++attributeIndex)
    {
        const VertexAttribute &attrib = vertexAttribs[attributeIndex];
        if (!program->isAttribLocationActive(attributeIndex) || !attrib.enabled)
        {
            continue;
        }

        const VertexBinding &binding = vertexBindings[attrib.bindingIndex];
        // If we have no buffer, then we either get an error, or there are no more checks to be done.
        gl::Buffer *buffer = binding.buffer.get();
        if (!buffer)
        {
            if (webglCompatibility || !state.areClientArraysEnabled())
            {
                // [WebGL 1.0] Section 6.5 Enabled Vertex Attributes and Range Checking
                // If a vertex attribute is enabled as an array via enableVertexAttribArray but
                // no buffer is bound to that attribute via bindBuffer and vertexAttribPointer,
                // then calls to drawArrays or drawElements will generate an INVALID_OPERATION
                // error.
                context->handleError(
                    Error(GL_INVALID_OPERATION, "An enabled vertex array has no buffer."));
                return false;
            }
            else if (attrib.pointer == nullptr)
            {
                // This is an application error that would normally result in a crash,
                // but we catch it and return an error
                context->handleError(
                    Error(GL_INVALID_OPERATION,
                          "An enabled vertex array has no buffer and no pointer."));
                return false;
            }
            continue;
        }

        // If we're drawing zero vertices, we have enough data.
        if (vertexCount <= 0 || primcount <= 0)
        {
            continue;
        }

        GLint maxVertexElement = 0;
        if (binding.divisor == 0)
        {
            maxVertexElement = maxVertex;
        }
        else
        {
            maxVertexElement = (primcount - 1) / binding.divisor;
        }

        // We do manual overflow checks here instead of using safe_math.h because it was
        // a bottleneck. Thanks to some properties of GL we know inequalities that can
        // help us make the overflow checks faster.

        // The max possible attribSize is 16 for a vector of 4 32 bit values.
        constexpr uint64_t kMaxAttribSize = 16;
        constexpr uint64_t kIntMax        = std::numeric_limits<int>::max();
        constexpr uint64_t kUint64Max     = std::numeric_limits<uint64_t>::max();

        // We know attribStride is given as a GLsizei which is typedefed to int.
        // We also know an upper bound for attribSize.
        static_assert(std::is_same<int, GLsizei>::value, "");
        uint64_t attribStride = ComputeVertexAttributeStride(attrib, binding);
        uint64_t attribSize   = ComputeVertexAttributeTypeSize(attrib);
        ASSERT(attribStride <= kIntMax && attribSize <= kMaxAttribSize);

        // Computing the max offset using uint64_t without attrib.offset is overflow
        // safe. Note: Last vertex element does not take the full stride!
        static_assert(kIntMax * kIntMax < kUint64Max - kMaxAttribSize, "");
        uint64_t attribDataSizeNoOffset = maxVertexElement * attribStride + attribSize;

        // An overflow can happen when adding the offset, check for it.
        uint64_t attribOffset = ComputeVertexAttributeOffset(attrib, binding);
        if (attribDataSizeNoOffset > kUint64Max - attribOffset)
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Integer overflow."));
            return false;
        }
        uint64_t attribDataSizeWithOffset = attribDataSizeNoOffset + attribOffset;

        // [OpenGL ES 3.0.2] section 2.9.4 page 40:
        // We can return INVALID_OPERATION if our vertex attribute does not have
        // enough backing data.
        if (attribDataSizeWithOffset > static_cast<uint64_t>(buffer->getSize()))
        {
            context->handleError(Error(GL_INVALID_OPERATION,
                                       "Vertex buffer is not big enough for the draw call"));
            return false;
        }
    }

    return true;
}

bool ValidReadPixelsFormatType(ValidationContext *context,
                               GLenum framebufferComponentType,
                               GLenum format,
                               GLenum type)
{
    switch (framebufferComponentType)
    {
        case GL_UNSIGNED_NORMALIZED:
            // TODO(geofflang): Don't accept BGRA here.  Some chrome internals appear to try to use
            // ReadPixels with BGRA even if the extension is not present
            return (format == GL_RGBA && type == GL_UNSIGNED_BYTE) ||
                   (context->getExtensions().readFormatBGRA && format == GL_BGRA_EXT &&
                    type == GL_UNSIGNED_BYTE);

        case GL_SIGNED_NORMALIZED:
            return (format == GL_RGBA && type == GL_UNSIGNED_BYTE);

        case GL_INT:
            return (format == GL_RGBA_INTEGER && type == GL_INT);

        case GL_UNSIGNED_INT:
            return (format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT);

        case GL_FLOAT:
            return (format == GL_RGBA && type == GL_FLOAT);

        default:
            UNREACHABLE();
            return false;
    }
}

bool ValidCap(const Context *context, GLenum cap, bool queryOnly)
{
    switch (cap)
    {
        // EXT_multisample_compatibility
        case GL_MULTISAMPLE_EXT:
        case GL_SAMPLE_ALPHA_TO_ONE_EXT:
            return context->getExtensions().multisampleCompatibility;

        case GL_CULL_FACE:
        case GL_POLYGON_OFFSET_FILL:
        case GL_SAMPLE_ALPHA_TO_COVERAGE:
        case GL_SAMPLE_COVERAGE:
        case GL_SCISSOR_TEST:
        case GL_STENCIL_TEST:
        case GL_DEPTH_TEST:
        case GL_BLEND:
        case GL_DITHER:
            return true;

        case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        case GL_RASTERIZER_DISCARD:
            return (context->getClientMajorVersion() >= 3);

        case GL_DEBUG_OUTPUT_SYNCHRONOUS:
        case GL_DEBUG_OUTPUT:
            return context->getExtensions().debug;

        case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
            return queryOnly && context->getExtensions().bindGeneratesResource;

        case GL_CLIENT_ARRAYS_ANGLE:
            return queryOnly && context->getExtensions().clientArrays;

        case GL_FRAMEBUFFER_SRGB_EXT:
            return context->getExtensions().sRGBWriteControl;

        case GL_SAMPLE_MASK:
            return context->getClientVersion() >= Version(3, 1);

        case GL_CONTEXT_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
            return queryOnly && context->getExtensions().robustResourceInitialization;

        default:
            return false;
    }
}

bool ValidateReadPixelsBase(ValidationContext *context,
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
                            GLvoid *pixels)
{
    if (length != nullptr)
    {
        *length = 0;
    }
    if (rows != nullptr)
    {
        *rows = 0;
    }
    if (columns != nullptr)
    {
        *columns = 0;
    }

    if (width < 0 || height < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "width and height must be positive"));
        return false;
    }

    auto readFramebuffer = context->getGLState().getReadFramebuffer();

    if (readFramebuffer->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    if (readFramebuffer->id() != 0 && readFramebuffer->getSamples(context) != 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const Framebuffer *framebuffer = context->getGLState().getReadFramebuffer();
    ASSERT(framebuffer);

    if (framebuffer->getReadBufferState() == GL_NONE)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Read buffer is GL_NONE"));
        return false;
    }

    const FramebufferAttachment *readBuffer = framebuffer->getReadColorbuffer();
    // WebGL 1.0 [Section 6.26] Reading From a Missing Attachment
    // In OpenGL ES it is undefined what happens when an operation tries to read from a missing
    // attachment and WebGL defines it to be an error. We do the check unconditionnaly as the
    // situation is an application error that would lead to a crash in ANGLE.
    if (readBuffer == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Missing read attachment"));
        return false;
    }

    GLenum currentFormat         = framebuffer->getImplementationColorReadFormat();
    GLenum currentType           = framebuffer->getImplementationColorReadType();
    GLenum currentInternalFormat = readBuffer->getFormat().asSized();

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(currentInternalFormat);
    bool validFormatTypeCombination =
        ValidReadPixelsFormatType(context, internalFormatInfo.componentType, format, type);

    if (!(currentFormat == format && currentType == type) && !validFormatTypeCombination)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // Check for pixel pack buffer related API errors
    gl::Buffer *pixelPackBuffer = context->getGLState().getTargetBuffer(GL_PIXEL_PACK_BUFFER);
    if (pixelPackBuffer != nullptr && pixelPackBuffer->isMapped())
    {
        // ...the buffer object's data store is currently mapped.
        context->handleError(Error(GL_INVALID_OPERATION, "Pixel pack buffer is mapped."));
        return false;
    }

    // ..  the data would be packed to the buffer object such that the memory writes required
    // would exceed the data store size.
    GLenum sizedInternalFormat       = GetSizedInternalFormat(format, type);
    const InternalFormat &formatInfo = GetInternalFormatInfo(sizedInternalFormat);
    const gl::Extents size(width, height, 1);
    const auto &pack = context->getGLState().getPackState();

    auto endByteOrErr = formatInfo.computePackUnpackEndByte(type, size, pack, false);
    if (endByteOrErr.isError())
    {
        context->handleError(endByteOrErr.getError());
        return false;
    }

    size_t endByte = endByteOrErr.getResult();
    if (bufSize >= 0)
    {
        if (pixelPackBuffer == nullptr && static_cast<size_t>(bufSize) < endByte)
        {
            context->handleError(
                Error(GL_INVALID_OPERATION, "bufSize must be at least %u bytes.", endByte));
            return false;
        }
    }

    if (pixelPackBuffer != nullptr)
    {
        CheckedNumeric<size_t> checkedEndByte(endByte);
        CheckedNumeric<size_t> checkedOffset(reinterpret_cast<size_t>(pixels));
        checkedEndByte += checkedOffset;

        if (checkedEndByte.ValueOrDie() > static_cast<size_t>(pixelPackBuffer->getSize()))
        {
            // Overflow past the end of the buffer
            context->handleError(
                Error(GL_INVALID_OPERATION, "Writes would overflow the pixel pack buffer."));
            return false;
        }
    }

    if (pixelPackBuffer == nullptr && length != nullptr)
    {
        if (endByte > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
        {
            context->handleError(
                Error(GL_INVALID_OPERATION, "length would overflow GLsizei.", endByte));
            return false;
        }

        *length = static_cast<GLsizei>(endByte);
    }

    auto getClippedExtent = [](GLint start, GLsizei length, int bufferSize) {
        angle::CheckedNumeric<int> clippedExtent(length);
        if (start < 0)
        {
            // "subtract" the area that is less than 0
            clippedExtent += start;
        }

        const int readExtent = start + length;
        if (readExtent > bufferSize)
        {
            // Subtract the region to the right of the read buffer
            clippedExtent -= (readExtent - bufferSize);
        }

        if (!clippedExtent.IsValid())
        {
            return 0;
        }

        return std::max(clippedExtent.ValueOrDie(), 0);
    };

    if (columns != nullptr)
    {
        *columns = getClippedExtent(x, width, readBuffer->getSize().width);
    }

    if (rows != nullptr)
    {
        *rows = getClippedExtent(y, height, readBuffer->getSize().height);
    }

    return true;
}

bool ValidateGetRenderbufferParameterivBase(Context *context,
                                            GLenum target,
                                            GLenum pname,
                                            GLsizei *length)
{
    if (length)
    {
        *length = 0;
    }

    if (target != GL_RENDERBUFFER)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid target."));
        return false;
    }

    Renderbuffer *renderbuffer = context->getGLState().getCurrentRenderbuffer();
    if (renderbuffer == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No renderbuffer bound."));
        return false;
    }

    switch (pname)
    {
        case GL_RENDERBUFFER_WIDTH:
        case GL_RENDERBUFFER_HEIGHT:
        case GL_RENDERBUFFER_INTERNAL_FORMAT:
        case GL_RENDERBUFFER_RED_SIZE:
        case GL_RENDERBUFFER_GREEN_SIZE:
        case GL_RENDERBUFFER_BLUE_SIZE:
        case GL_RENDERBUFFER_ALPHA_SIZE:
        case GL_RENDERBUFFER_DEPTH_SIZE:
        case GL_RENDERBUFFER_STENCIL_SIZE:
            break;

        case GL_RENDERBUFFER_SAMPLES_ANGLE:
            if (!context->getExtensions().framebufferMultisample)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_ANGLE_framebuffer_multisample is not enabled."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    if (length)
    {
        *length = 1;
    }
    return true;
}

bool ValidateGetShaderivBase(Context *context, GLuint shader, GLenum pname, GLsizei *length)
{
    if (length)
    {
        *length = 0;
    }

    if (GetValidShader(context, shader) == nullptr)
    {
        return false;
    }

    switch (pname)
    {
        case GL_SHADER_TYPE:
        case GL_DELETE_STATUS:
        case GL_COMPILE_STATUS:
        case GL_INFO_LOG_LENGTH:
        case GL_SHADER_SOURCE_LENGTH:
            break;

        case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
            if (!context->getExtensions().translatedShaderSource)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_ANGLE_translated_shader_source is not enabled."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    if (length)
    {
        *length = 1;
    }
    return true;
}

bool ValidateGetTexParameterBase(Context *context, GLenum target, GLenum pname, GLsizei *length)
{
    if (length)
    {
        *length = 0;
    }

    if (!ValidTextureTarget(context, target) && !ValidTextureExternalTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid texture target"));
        return false;
    }

    if (context->getTargetTexture(target) == nullptr)
    {
        // Should only be possible for external textures
        context->handleError(Error(GL_INVALID_ENUM, "No texture bound."));
        return false;
    }

    switch (pname)
    {
        case GL_TEXTURE_MAG_FILTER:
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
            break;

        case GL_TEXTURE_USAGE_ANGLE:
            if (!context->getExtensions().textureUsage)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_ANGLE_texture_usage is not enabled."));
                return false;
            }
            break;

        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (!context->getExtensions().textureFilterAnisotropic)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_EXT_texture_filter_anisotropic is not enabled."));
                return false;
            }
            break;

        case GL_TEXTURE_IMMUTABLE_FORMAT:
            if (context->getClientMajorVersion() < 3 && !context->getExtensions().textureStorage)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_EXT_texture_storage is not enabled."));
                return false;
            }
            break;

        case GL_TEXTURE_WRAP_R:
        case GL_TEXTURE_IMMUTABLE_LEVELS:
        case GL_TEXTURE_SWIZZLE_R:
        case GL_TEXTURE_SWIZZLE_G:
        case GL_TEXTURE_SWIZZLE_B:
        case GL_TEXTURE_SWIZZLE_A:
        case GL_TEXTURE_BASE_LEVEL:
        case GL_TEXTURE_MAX_LEVEL:
        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
        case GL_TEXTURE_COMPARE_MODE:
        case GL_TEXTURE_COMPARE_FUNC:
            if (context->getClientMajorVersion() < 3)
            {
                context->handleError(Error(GL_INVALID_ENUM, "pname requires OpenGL ES 3.0."));
                return false;
            }
            break;

        case GL_TEXTURE_SRGB_DECODE_EXT:
            if (!context->getExtensions().textureSRGBDecode)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_EXT_texture_sRGB_decode is not enabled."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    if (length)
    {
        *length = 1;
    }
    return true;
}

template <typename ParamType>
bool ValidateTextureWrapModeValue(Context *context, ParamType *params, bool isExternalTextureTarget)
{
    switch (ConvertToGLenum(params[0]))
    {
        case GL_CLAMP_TO_EDGE:
            break;

        case GL_REPEAT:
        case GL_MIRRORED_REPEAT:
            if (isExternalTextureTarget)
            {
                // OES_EGL_image_external specifies this error.
                context->handleError(Error(
                    GL_INVALID_ENUM, "external textures only support CLAMP_TO_EDGE wrap mode"));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
            return false;
    }

    return true;
}

template <typename ParamType>
bool ValidateTextureMinFilterValue(Context *context,
                                   ParamType *params,
                                   bool isExternalTextureTarget)
{
    switch (ConvertToGLenum(params[0]))
    {
        case GL_NEAREST:
        case GL_LINEAR:
            break;

        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_LINEAR:
            if (isExternalTextureTarget)
            {
                // OES_EGL_image_external specifies this error.
                context->handleError(
                    Error(GL_INVALID_ENUM,
                          "external textures only support NEAREST and LINEAR filtering"));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
            return false;
    }

    return true;
}

template <typename ParamType>
bool ValidateTextureMagFilterValue(Context *context, ParamType *params)
{
    switch (ConvertToGLenum(params[0]))
    {
        case GL_NEAREST:
        case GL_LINEAR:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
            return false;
    }

    return true;
}

template <typename ParamType>
bool ValidateTextureCompareModeValue(Context *context, ParamType *params)
{
    // Acceptable mode parameters from GLES 3.0.2 spec, table 3.17
    switch (ConvertToGLenum(params[0]))
    {
        case GL_NONE:
        case GL_COMPARE_REF_TO_TEXTURE:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
            return false;
    }

    return true;
}

template <typename ParamType>
bool ValidateTextureCompareFuncValue(Context *context, ParamType *params)
{
    // Acceptable function parameters from GLES 3.0.2 spec, table 3.17
    switch (ConvertToGLenum(params[0]))
    {
        case GL_LEQUAL:
        case GL_GEQUAL:
        case GL_LESS:
        case GL_GREATER:
        case GL_EQUAL:
        case GL_NOTEQUAL:
        case GL_ALWAYS:
        case GL_NEVER:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
            return false;
    }

    return true;
}

template <typename ParamType>
bool ValidateTextureSRGBDecodeValue(Context *context, ParamType *params)
{
    if (!context->getExtensions().textureSRGBDecode)
    {
        context->handleError(Error(GL_INVALID_ENUM, "GL_EXT_texture_sRGB_decode is not enabled."));
        return false;
    }

    switch (ConvertToGLenum(params[0]))
    {
        case GL_DECODE_EXT:
        case GL_SKIP_DECODE_EXT:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
            return false;
    }

    return true;
}

template <typename ParamType>
bool ValidateTexParameterBase(Context *context,
                              GLenum target,
                              GLenum pname,
                              GLsizei bufSize,
                              ParamType *params)
{
    if (!ValidTextureTarget(context, target) && !ValidTextureExternalTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid texture target"));
        return false;
    }

    if (context->getTargetTexture(target) == nullptr)
    {
        // Should only be possible for external textures
        context->handleError(Error(GL_INVALID_ENUM, "No texture bound."));
        return false;
    }

    const GLsizei minBufSize = 1;
    if (bufSize >= 0 && bufSize < minBufSize)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "bufSize must be at least %i.", minBufSize));
        return false;
    }

    switch (pname)
    {
        case GL_TEXTURE_WRAP_R:
        case GL_TEXTURE_SWIZZLE_R:
        case GL_TEXTURE_SWIZZLE_G:
        case GL_TEXTURE_SWIZZLE_B:
        case GL_TEXTURE_SWIZZLE_A:
        case GL_TEXTURE_BASE_LEVEL:
        case GL_TEXTURE_MAX_LEVEL:
        case GL_TEXTURE_COMPARE_MODE:
        case GL_TEXTURE_COMPARE_FUNC:
        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
            if (context->getClientMajorVersion() < 3)
            {
                context->handleError(Error(GL_INVALID_ENUM, "pname requires OpenGL ES 3.0."));
                return false;
            }
            if (target == GL_TEXTURE_EXTERNAL_OES &&
                !context->getExtensions().eglImageExternalEssl3)
            {
                context->handleError(Error(GL_INVALID_ENUM,
                                           "ES3 texture parameters are not available without "
                                           "GL_OES_EGL_image_external_essl3."));
                return false;
            }
            break;

        default:
            break;
    }

    switch (pname)
    {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
            if (!ValidateTextureWrapModeValue(context, params, target == GL_TEXTURE_EXTERNAL_OES))
            {
                return false;
            }
            break;

        case GL_TEXTURE_MIN_FILTER:
            if (!ValidateTextureMinFilterValue(context, params, target == GL_TEXTURE_EXTERNAL_OES))
            {
                return false;
            }
            break;

        case GL_TEXTURE_MAG_FILTER:
            if (!ValidateTextureMagFilterValue(context, params))
            {
                return false;
            }
            break;

        case GL_TEXTURE_USAGE_ANGLE:
            switch (ConvertToGLenum(params[0]))
            {
                case GL_NONE:
                case GL_FRAMEBUFFER_ATTACHMENT_ANGLE:
                    break;

                default:
                    context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
                    return false;
            }
            break;

        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (!context->getExtensions().textureFilterAnisotropic)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_EXT_texture_anisotropic is not enabled."));
                return false;
            }

            // we assume the parameter passed to this validation method is truncated, not rounded
            if (params[0] < 1)
            {
                context->handleError(Error(GL_INVALID_VALUE, "Max anisotropy must be at least 1."));
                return false;
            }
            break;

        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
            // any value is permissible
            break;

        case GL_TEXTURE_COMPARE_MODE:
            if (!ValidateTextureCompareModeValue(context, params))
            {
                return false;
            }
            break;

        case GL_TEXTURE_COMPARE_FUNC:
            if (!ValidateTextureCompareFuncValue(context, params))
            {
                return false;
            }
            break;

        case GL_TEXTURE_SWIZZLE_R:
        case GL_TEXTURE_SWIZZLE_G:
        case GL_TEXTURE_SWIZZLE_B:
        case GL_TEXTURE_SWIZZLE_A:
            switch (ConvertToGLenum(params[0]))
            {
                case GL_RED:
                case GL_GREEN:
                case GL_BLUE:
                case GL_ALPHA:
                case GL_ZERO:
                case GL_ONE:
                    break;

                default:
                    context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
                    return false;
            }
            break;

        case GL_TEXTURE_BASE_LEVEL:
            if (params[0] < 0)
            {
                context->handleError(Error(GL_INVALID_VALUE, "Base level must be at least 0."));
                return false;
            }
            if (target == GL_TEXTURE_EXTERNAL_OES && static_cast<GLuint>(params[0]) != 0)
            {
                context->handleError(
                    Error(GL_INVALID_OPERATION, "Base level must be 0 for external textures."));
                return false;
            }
            break;

        case GL_TEXTURE_MAX_LEVEL:
            if (params[0] < 0)
            {
                context->handleError(Error(GL_INVALID_VALUE, "Max level must be at least 0."));
                return false;
            }
            break;

        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            if (context->getClientVersion() < Version(3, 1))
            {
                context->handleError(Error(GL_INVALID_ENUM, "pname requires OpenGL ES 3.1."));
                return false;
            }
            switch (ConvertToGLenum(params[0]))
            {
                case GL_DEPTH_COMPONENT:
                case GL_STENCIL_INDEX:
                    break;

                default:
                    context->handleError(Error(GL_INVALID_ENUM, "Unknown param value."));
                    return false;
            }
            break;

        case GL_TEXTURE_SRGB_DECODE_EXT:
            if (!ValidateTextureSRGBDecodeValue(context, params))
            {
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    return true;
}

template <typename ParamType>
bool ValidateSamplerParameterBase(Context *context,
                                  GLuint sampler,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  ParamType *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Context does not support OpenGL ES 3.0."));
        return false;
    }

    if (!context->isSampler(sampler))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Sampler is not valid."));
        return false;
    }

    const GLsizei minBufSize = 1;
    if (bufSize >= 0 && bufSize < minBufSize)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "bufSize must be at least %i.", minBufSize));
        return false;
    }

    switch (pname)
    {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
            if (!ValidateTextureWrapModeValue(context, params, false))
            {
                return false;
            }
            break;

        case GL_TEXTURE_MIN_FILTER:
            if (!ValidateTextureMinFilterValue(context, params, false))
            {
                return false;
            }
            break;

        case GL_TEXTURE_MAG_FILTER:
            if (!ValidateTextureMagFilterValue(context, params))
            {
                return false;
            }
            break;

        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
            // any value is permissible
            break;

        case GL_TEXTURE_COMPARE_MODE:
            if (!ValidateTextureCompareModeValue(context, params))
            {
                return false;
            }
            break;

        case GL_TEXTURE_COMPARE_FUNC:
            if (!ValidateTextureCompareFuncValue(context, params))
            {
                return false;
            }
            break;

        case GL_TEXTURE_SRGB_DECODE_EXT:
            if (!ValidateTextureSRGBDecodeValue(context, params))
            {
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    return true;
}

bool ValidateGetSamplerParameterBase(Context *context,
                                     GLuint sampler,
                                     GLenum pname,
                                     GLsizei *length)
{
    if (length)
    {
        *length = 0;
    }

    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Context does not support OpenGL ES 3.0."));
        return false;
    }

    if (!context->isSampler(sampler))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Sampler is not valid."));
        return false;
    }

    switch (pname)
    {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
        case GL_TEXTURE_COMPARE_MODE:
        case GL_TEXTURE_COMPARE_FUNC:
            break;

        case GL_TEXTURE_SRGB_DECODE_EXT:
            if (!context->getExtensions().textureSRGBDecode)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "GL_EXT_texture_sRGB_decode is not enabled."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    if (length)
    {
        *length = 1;
    }
    return true;
}

bool ValidateGetVertexAttribBase(Context *context,
                                 GLuint index,
                                 GLenum pname,
                                 GLsizei *length,
                                 bool pointer,
                                 bool pureIntegerEntryPoint)
{
    if (length)
    {
        *length = 0;
    }

    if (pureIntegerEntryPoint && context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Context does not support OpenGL ES 3.0."));
        return false;
    }

    if (index >= context->getCaps().maxVertexAttributes)
    {
        context->handleError(Error(
            GL_INVALID_VALUE, "index must be less than the value of GL_MAX_VERTEX_ATTRIBUTES."));
        return false;
    }

    if (pointer)
    {
        if (pname != GL_VERTEX_ATTRIB_ARRAY_POINTER)
        {
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
        }
    }
    else
    {
        switch (pname)
        {
            case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            case GL_CURRENT_VERTEX_ATTRIB:
                break;

            case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
                static_assert(
                    GL_VERTEX_ATTRIB_ARRAY_DIVISOR == GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE,
                    "ANGLE extension enums not equal to GL enums.");
                if (context->getClientMajorVersion() < 3 &&
                    !context->getExtensions().instancedArrays)
                {
                    context->handleError(Error(GL_INVALID_ENUM,
                                               "GL_VERTEX_ATTRIB_ARRAY_DIVISOR requires OpenGL ES "
                                               "3.0 or GL_ANGLE_instanced_arrays."));
                    return false;
                }
                break;

            case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
                if (context->getClientMajorVersion() < 3)
                {
                    context->handleError(Error(
                        GL_INVALID_ENUM, "GL_VERTEX_ATTRIB_ARRAY_INTEGER requires OpenGL ES 3.0."));
                    return false;
                }
                break;

            case GL_VERTEX_ATTRIB_BINDING:
            case GL_VERTEX_ATTRIB_RELATIVE_OFFSET:
                if (context->getClientVersion() < ES_3_1)
                {
                    context->handleError(
                        Error(GL_INVALID_ENUM, "Vertex Attrib Bindings require OpenGL ES 3.1."));
                    return false;
                }
                break;

            default:
                context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
                return false;
        }
    }

    if (length)
    {
        if (pname == GL_CURRENT_VERTEX_ATTRIB)
        {
            *length = 4;
        }
        else
        {
            *length = 1;
        }
    }

    return true;
}

bool ValidateGetActiveUniformBlockivBase(Context *context,
                                         GLuint program,
                                         GLuint uniformBlockIndex,
                                         GLenum pname,
                                         GLsizei *length)
{
    if (length)
    {
        *length = 0;
    }

    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Context does not support OpenGL ES 3.0."));
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    if (uniformBlockIndex >= programObject->getActiveUniformBlockCount())
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "uniformBlockIndex exceeds active uniform block count."));
        return false;
    }

    switch (pname)
    {
        case GL_UNIFORM_BLOCK_BINDING:
        case GL_UNIFORM_BLOCK_DATA_SIZE:
        case GL_UNIFORM_BLOCK_NAME_LENGTH:
        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
        case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
        case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    if (length)
    {
        if (pname == GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES)
        {
            const UniformBlock &uniformBlock =
                programObject->getUniformBlockByIndex(uniformBlockIndex);
            *length = static_cast<GLsizei>(uniformBlock.memberUniformIndexes.size());
        }
        else
        {
            *length = 1;
        }
    }

    return true;
}

bool ValidateGetBufferParameterBase(ValidationContext *context,
                                    GLenum target,
                                    GLenum pname,
                                    bool pointerVersion,
                                    GLsizei *numParams)
{
    if (numParams)
    {
        *numParams = 0;
    }

    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid buffer target."));
        return false;
    }

    const Buffer *buffer = context->getGLState().getTargetBuffer(target);
    if (!buffer)
    {
        // A null buffer means that "0" is bound to the requested buffer target
        context->handleError(Error(GL_INVALID_OPERATION, "No buffer bound."));
        return false;
    }

    const Extensions &extensions = context->getExtensions();

    switch (pname)
    {
        case GL_BUFFER_USAGE:
        case GL_BUFFER_SIZE:
            break;

        case GL_BUFFER_ACCESS_OES:
            if (!extensions.mapBuffer)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "pname requires OpenGL ES 3.0 or GL_OES_mapbuffer."));
                return false;
            }
            break;

        case GL_BUFFER_MAPPED:
            static_assert(GL_BUFFER_MAPPED == GL_BUFFER_MAPPED_OES, "GL enums should be equal.");
            if (context->getClientMajorVersion() < 3 && !extensions.mapBuffer &&
                !extensions.mapBufferRange)
            {
                context->handleError(Error(
                    GL_INVALID_ENUM,
                    "pname requires OpenGL ES 3.0, GL_OES_mapbuffer or GL_EXT_map_buffer_range."));
                return false;
            }
            break;

        case GL_BUFFER_MAP_POINTER:
            if (!pointerVersion)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM,
                          "GL_BUFFER_MAP_POINTER can only be queried with GetBufferPointerv."));
                return false;
            }
            break;

        case GL_BUFFER_ACCESS_FLAGS:
        case GL_BUFFER_MAP_OFFSET:
        case GL_BUFFER_MAP_LENGTH:
            if (context->getClientMajorVersion() < 3 && !extensions.mapBufferRange)
            {
                context->handleError(Error(
                    GL_INVALID_ENUM, "pname requires OpenGL ES 3.0 or GL_EXT_map_buffer_range."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    // All buffer parameter queries return one value.
    if (numParams)
    {
        *numParams = 1;
    }

    return true;
}

bool ValidateGetInternalFormativBase(Context *context,
                                     GLenum target,
                                     GLenum internalformat,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *numParams)
{
    if (numParams)
    {
        *numParams = 0;
    }

    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Context does not support OpenGL ES 3.0."));
        return false;
    }

    const TextureCaps &formatCaps = context->getTextureCaps().get(internalformat);
    if (!formatCaps.renderable)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Internal format is not renderable."));
        return false;
    }

    switch (target)
    {
        case GL_RENDERBUFFER:
            break;

        case GL_TEXTURE_2D_MULTISAMPLE:
            if (context->getClientVersion() < ES_3_1)
            {
                context->handleError(
                    Error(GL_INVALID_OPERATION, "Texture target requires at least OpenGL ES 3.1."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid target."));
            return false;
    }

    if (bufSize < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "bufSize cannot be negative."));
        return false;
    }

    GLsizei maxWriteParams = 0;
    switch (pname)
    {
        case GL_NUM_SAMPLE_COUNTS:
            maxWriteParams = 1;
            break;

        case GL_SAMPLES:
            maxWriteParams = static_cast<GLsizei>(formatCaps.sampleCounts.size());
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    if (numParams)
    {
        // glGetInternalFormativ will not overflow bufSize
        *numParams = std::min(bufSize, maxWriteParams);
    }

    return true;
}

bool ValidateUniformCommonBase(ValidationContext *context,
                               gl::Program *program,
                               GLint location,
                               GLsizei count,
                               const LinkedUniform **uniformOut)
{
    // TODO(Jiajia): Add image uniform check in future.
    if (count < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (!program || !program->isLinked())
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (location == -1)
    {
        // Silently ignore the uniform command
        return false;
    }

    const auto &uniformLocations = program->getUniformLocations();
    size_t castedLocation = static_cast<size_t>(location);
    if (castedLocation >= uniformLocations.size())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Invalid uniform location"));
        return false;
    }

    const auto &uniformLocation = uniformLocations[castedLocation];
    if (uniformLocation.ignored)
    {
        // Silently ignore the uniform command
        return false;
    }

    if (!uniformLocation.used)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const auto &uniform = program->getUniformByIndex(uniformLocation.index);

    // attempting to write an array to a non-array uniform is an INVALID_OPERATION
    if (!uniform.isArray() && count > 1)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    *uniformOut = &uniform;
    return true;
}

bool ValidateUniform1ivValue(ValidationContext *context,
                             GLenum uniformType,
                             GLsizei count,
                             const GLint *value)
{
    // Value type is GL_INT, because we only get here from glUniform1i{v}.
    // It is compatible with INT or BOOL.
    // Do these cheap tests first, for a little extra speed.
    if (GL_INT == uniformType || GL_BOOL == uniformType)
    {
        return true;
    }

    if (IsSamplerType(uniformType))
    {
        // Check that the values are in range.
        const GLint max = context->getCaps().maxCombinedTextureImageUnits;
        for (GLsizei i = 0; i < count; ++i)
        {
            if (value[i] < 0 || value[i] >= max)
            {
                context->handleError(Error(GL_INVALID_VALUE, "sampler uniform value out of range"));
                return false;
            }
        }
        return true;
    }

    context->handleError(Error(GL_INVALID_OPERATION, "wrong type of value for uniform"));
    return false;
}

bool ValidateUniformValue(ValidationContext *context, GLenum valueType, GLenum uniformType)
{
    // Check that the value type is compatible with uniform type.
    // Do the cheaper test first, for a little extra speed.
    if (valueType == uniformType || VariableBoolVectorType(valueType) == uniformType)
    {
        return true;
    }

    context->handleError(Error(GL_INVALID_OPERATION, "wrong type of value for uniform"));
    return false;
}

bool ValidateUniformMatrixValue(ValidationContext *context, GLenum valueType, GLenum uniformType)
{
    // Check that the value type is compatible with uniform type.
    if (valueType == uniformType)
    {
        return true;
    }

    context->handleError(Error(GL_INVALID_OPERATION, "wrong type of value for uniform"));
    return false;
}

bool ValidateES2CopyTexImageParameters(ValidationContext *context,
                                       GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid texture target"));
        return false;
    }

    if (!ValidImageSizeParameters(context, target, level, width, height, 1, isSubImage))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid texture dimensions."));
        return false;
    }

    Format textureFormat = Format::Invalid();
    if (!ValidateCopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                            xoffset, yoffset, 0, x, y, width, height, border,
                                            &textureFormat))
    {
        return false;
    }

    const gl::Framebuffer *framebuffer = context->getGLState().getReadFramebuffer();
    GLenum colorbufferFormat           = framebuffer->getReadColorbuffer()->getFormat().asSized();
    const auto &formatInfo             = *textureFormat.info;

    // [OpenGL ES 2.0.24] table 3.9
    if (isSubImage)
    {
        switch (formatInfo.format)
        {
            case GL_ALPHA:
                if (colorbufferFormat != GL_ALPHA8_EXT && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_RGBA8_OES)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_LUMINANCE:
                if (colorbufferFormat != GL_R8_EXT && colorbufferFormat != GL_RG8_EXT &&
                    colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_RED_EXT:
                if (colorbufferFormat != GL_R8_EXT && colorbufferFormat != GL_RG8_EXT &&
                    colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_R32F &&
                    colorbufferFormat != GL_RG32F && colorbufferFormat != GL_RGB32F &&
                    colorbufferFormat != GL_RGBA32F)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_RG_EXT:
                if (colorbufferFormat != GL_RG8_EXT && colorbufferFormat != GL_RGB565 &&
                    colorbufferFormat != GL_RGB8_OES && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_RG32F && colorbufferFormat != GL_RGB32F &&
                    colorbufferFormat != GL_RGBA32F)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_RGB:
                if (colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_RGB32F &&
                    colorbufferFormat != GL_RGBA32F)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_LUMINANCE_ALPHA:
            case GL_RGBA:
                if (colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_RGBA32F)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
            case GL_ETC1_RGB8_OES:
            case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
            case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_STENCIL_OES:
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            default:
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
        }

        if (formatInfo.type == GL_FLOAT && !context->getExtensions().textureFloat)
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }
    else
    {
        switch (internalformat)
        {
            case GL_ALPHA:
                if (colorbufferFormat != GL_ALPHA8_EXT && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_LUMINANCE:
                if (colorbufferFormat != GL_R8_EXT && colorbufferFormat != GL_RG8_EXT &&
                    colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_RED_EXT:
                if (colorbufferFormat != GL_R8_EXT && colorbufferFormat != GL_RG8_EXT &&
                    colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_RG_EXT:
                if (colorbufferFormat != GL_RG8_EXT && colorbufferFormat != GL_RGB565 &&
                    colorbufferFormat != GL_RGB8_OES && colorbufferFormat != GL_RGBA4 &&
                    colorbufferFormat != GL_RGB5_A1 && colorbufferFormat != GL_BGRA8_EXT &&
                    colorbufferFormat != GL_RGBA8_OES && colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_RGB:
                if (colorbufferFormat != GL_RGB565 && colorbufferFormat != GL_RGB8_OES &&
                    colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_LUMINANCE_ALPHA:
            case GL_RGBA:
                if (colorbufferFormat != GL_RGBA4 && colorbufferFormat != GL_RGB5_A1 &&
                    colorbufferFormat != GL_BGRA8_EXT && colorbufferFormat != GL_RGBA8_OES &&
                    colorbufferFormat != GL_BGR5_A1_ANGLEX)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (context->getExtensions().textureCompressionDXT1)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
                if (context->getExtensions().textureCompressionDXT3)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
                if (context->getExtensions().textureCompressionDXT5)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_ETC1_RGB8_OES:
                if (context->getExtensions().compressedETC1RGB8Texture)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;
            case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
            case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
            case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
                if (context->getExtensions().lossyETCDecode)
                {
                    context->handleError(Error(GL_INVALID_OPERATION,
                                               "ETC lossy decode formats can't be copied to."));
                    return false;
                }
                else
                {
                    context->handleError(Error(
                        GL_INVALID_ENUM, "ANGLE_lossy_etc_decode extension is not supported."));
                    return false;
                }
                break;
            case GL_DEPTH_COMPONENT:
            case GL_DEPTH_COMPONENT16:
            case GL_DEPTH_COMPONENT32_OES:
            case GL_DEPTH_STENCIL_OES:
            case GL_DEPTH24_STENCIL8_OES:
                if (context->getExtensions().depthTextures)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
            default:
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
        }
    }

    // If width or height is zero, it is a no-op.  Return false without setting an error.
    return (width > 0 && height > 0);
}

}  // anonymous namespace

bool ValidTextureTarget(const ValidationContext *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
            return true;

        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            return (context->getClientMajorVersion() >= 3);

        case GL_TEXTURE_2D_MULTISAMPLE:
            return (context->getClientVersion() >= Version(3, 1));

        default:
            return false;
    }
}

bool ValidTexture2DTarget(const ValidationContext *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
            return true;

        default:
            return false;
    }
}

bool ValidTexture3DTarget(const ValidationContext *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            return (context->getClientMajorVersion() >= 3);

        default:
            return false;
    }
}

// Most texture GL calls are not compatible with external textures, so we have a separate validation
// function for use in the GL calls that do
bool ValidTextureExternalTarget(const ValidationContext *context, GLenum target)
{
    return (target == GL_TEXTURE_EXTERNAL_OES) &&
           (context->getExtensions().eglImageExternal ||
            context->getExtensions().eglStreamConsumerExternal);
}

// This function differs from ValidTextureTarget in that the target must be
// usable as the destination of a 2D operation-- so a cube face is valid, but
// GL_TEXTURE_CUBE_MAP is not.
// Note: duplicate of IsInternalTextureTarget
bool ValidTexture2DDestinationTarget(const ValidationContext *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            return true;
        default:
            return false;
    }
}

bool ValidTexture3DDestinationTarget(const ValidationContext *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            return true;
        default:
            return false;
    }
}

bool ValidTexLevelDestinationTarget(const ValidationContext *context, GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_2D_MULTISAMPLE:
            return true;
        default:
            return false;
    }
}

bool ValidFramebufferTarget(GLenum target)
{
    static_assert(GL_DRAW_FRAMEBUFFER_ANGLE == GL_DRAW_FRAMEBUFFER &&
                      GL_READ_FRAMEBUFFER_ANGLE == GL_READ_FRAMEBUFFER,
                  "ANGLE framebuffer enums must equal the ES3 framebuffer enums.");

    switch (target)
    {
        case GL_FRAMEBUFFER:
            return true;
        case GL_READ_FRAMEBUFFER:
            return true;
        case GL_DRAW_FRAMEBUFFER:
            return true;
        default:
            return false;
    }
}

bool ValidBufferTarget(const ValidationContext *context, GLenum target)
{
    switch (target)
    {
        case GL_ARRAY_BUFFER:
        case GL_ELEMENT_ARRAY_BUFFER:
            return true;

        case GL_PIXEL_PACK_BUFFER:
        case GL_PIXEL_UNPACK_BUFFER:
            return (context->getExtensions().pixelBufferObject ||
                    context->getClientMajorVersion() >= 3);

        case GL_COPY_READ_BUFFER:
        case GL_COPY_WRITE_BUFFER:
        case GL_TRANSFORM_FEEDBACK_BUFFER:
        case GL_UNIFORM_BUFFER:
            return (context->getClientMajorVersion() >= 3);

        case GL_ATOMIC_COUNTER_BUFFER:
        case GL_SHADER_STORAGE_BUFFER:
        case GL_DRAW_INDIRECT_BUFFER:
        case GL_DISPATCH_INDIRECT_BUFFER:
            return context->getClientVersion() >= Version(3, 1);

        default:
            return false;
    }
}

bool ValidMipLevel(const ValidationContext *context, GLenum target, GLint level)
{
    const auto &caps    = context->getCaps();
    size_t maxDimension = 0;
    switch (target)
    {
        case GL_TEXTURE_2D:
            maxDimension = caps.max2DTextureSize;
            break;
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            maxDimension = caps.maxCubeMapTextureSize;
            break;
        case GL_TEXTURE_3D:
            maxDimension = caps.max3DTextureSize;
            break;
        case GL_TEXTURE_2D_ARRAY:
            maxDimension = caps.max2DTextureSize;
            break;
        case GL_TEXTURE_2D_MULTISAMPLE:
            maxDimension = caps.max2DTextureSize;
            break;
        default:
            UNREACHABLE();
    }

    return level <= gl::log2(static_cast<int>(maxDimension));
}

bool ValidImageSizeParameters(const ValidationContext *context,
                              GLenum target,
                              GLint level,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              bool isSubImage)
{
    if (level < 0 || width < 0 || height < 0 || depth < 0)
    {
        return false;
    }

    // TexSubImage parameters can be NPOT without textureNPOT extension,
    // as long as the destination texture is POT.
    bool hasNPOTSupport =
        context->getExtensions().textureNPOT || context->getClientVersion() >= Version(3, 0);
    if (!isSubImage && !hasNPOTSupport &&
        (level != 0 && (!gl::isPow2(width) || !gl::isPow2(height) || !gl::isPow2(depth))))
    {
        return false;
    }

    if (!ValidMipLevel(context, target, level))
    {
        return false;
    }

    return true;
}

bool CompressedTextureFormatRequiresExactSize(GLenum internalFormat)
{
    // List of compressed format that require that the texture size is smaller than or a multiple of
    // the compressed block size.
    switch (internalFormat)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
        case GL_ETC1_RGB8_LOSSY_DECODE_ANGLE:
        case GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE:
        case GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE:
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
        case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE:
        case GL_COMPRESSED_RGBA8_LOSSY_DECODE_ETC2_EAC_ANGLE:
        case GL_COMPRESSED_SRGB8_ALPHA8_LOSSY_DECODE_ETC2_EAC_ANGLE:
            return true;

        default:
            return false;
    }
}

bool ValidCompressedImageSize(const ValidationContext *context,
                              GLenum internalFormat,
                              GLint xoffset,
                              GLint yoffset,
                              GLsizei width,
                              GLsizei height)
{
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalFormat);
    if (!formatInfo.compressed)
    {
        return false;
    }

    if (xoffset < 0 || yoffset < 0 || width < 0 || height < 0)
    {
        return false;
    }

    if (CompressedTextureFormatRequiresExactSize(internalFormat))
    {
        if (xoffset % formatInfo.compressedBlockWidth != 0 ||
            yoffset % formatInfo.compressedBlockHeight != 0 ||
            (static_cast<GLuint>(width) > formatInfo.compressedBlockWidth &&
             width % formatInfo.compressedBlockWidth != 0) ||
            (static_cast<GLuint>(height) > formatInfo.compressedBlockHeight &&
             height % formatInfo.compressedBlockHeight != 0))
        {
            return false;
        }
    }

    return true;
}

bool ValidImageDataSize(ValidationContext *context,
                        GLenum textureTarget,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth,
                        GLenum internalFormat,
                        GLenum type,
                        const GLvoid *pixels,
                        GLsizei imageSize)
{
    gl::Buffer *pixelUnpackBuffer = context->getGLState().getTargetBuffer(GL_PIXEL_UNPACK_BUFFER);
    if (pixelUnpackBuffer == nullptr && imageSize < 0)
    {
        // Checks are not required
        return true;
    }

    // ...the data would be unpacked from the buffer object such that the memory reads required
    // would exceed the data store size.
    GLenum sizedFormat                   = GetSizedInternalFormat(internalFormat, type);
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(sizedFormat);
    const gl::Extents size(width, height, depth);
    const auto &unpack = context->getGLState().getUnpackState();

    bool targetIs3D   = textureTarget == GL_TEXTURE_3D || textureTarget == GL_TEXTURE_2D_ARRAY;
    auto endByteOrErr = formatInfo.computePackUnpackEndByte(type, size, unpack, targetIs3D);
    if (endByteOrErr.isError())
    {
        context->handleError(endByteOrErr.getError());
        return false;
    }

    GLuint endByte = endByteOrErr.getResult();

    if (pixelUnpackBuffer)
    {
        CheckedNumeric<size_t> checkedEndByte(endByteOrErr.getResult());
        CheckedNumeric<size_t> checkedOffset(reinterpret_cast<size_t>(pixels));
        checkedEndByte += checkedOffset;

        if (!checkedEndByte.IsValid() ||
            (checkedEndByte.ValueOrDie() > static_cast<size_t>(pixelUnpackBuffer->getSize())))
        {
            // Overflow past the end of the buffer
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }
    else
    {
        ASSERT(imageSize >= 0);
        if (pixels == nullptr && imageSize != 0)
        {
            context->handleError(
                Error(GL_INVALID_OPERATION, "imageSize must be 0 if no texture data is provided."));
            return false;
        }

        if (pixels != nullptr && endByte > static_cast<GLuint>(imageSize))
        {
            context->handleError(
                Error(GL_INVALID_OPERATION, "imageSize must be at least %u.", endByte));
            return false;
        }
    }

    return true;
}

bool ValidQueryType(const Context *context, GLenum queryType)
{
    static_assert(GL_ANY_SAMPLES_PASSED == GL_ANY_SAMPLES_PASSED_EXT,
                  "GL extension enums not equal.");
    static_assert(GL_ANY_SAMPLES_PASSED_CONSERVATIVE == GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
                  "GL extension enums not equal.");

    switch (queryType)
    {
        case GL_ANY_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
            return true;
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            return (context->getClientMajorVersion() >= 3);
        case GL_TIME_ELAPSED_EXT:
            return context->getExtensions().disjointTimerQuery;
        case GL_COMMANDS_COMPLETED_CHROMIUM:
            return context->getExtensions().syncQuery;
        default:
            return false;
    }
}

bool ValidateWebGLVertexAttribPointer(ValidationContext *context,
                                      GLenum type,
                                      GLboolean normalized,
                                      GLsizei stride,
                                      const GLvoid *ptr,
                                      bool pureInteger)
{
    ASSERT(context->getExtensions().webglCompatibility);

    // WebGL 1.0 [Section 6.11] Vertex Attribute Data Stride
    // The WebGL API supports vertex attribute data strides up to 255 bytes. A call to
    // vertexAttribPointer will generate an INVALID_VALUE error if the value for the stride
    // parameter exceeds 255.
    constexpr GLsizei kMaxWebGLStride = 255;
    if (stride > kMaxWebGLStride)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Stride is over the maximum stride allowed by WebGL."));
        return false;
    }

    // WebGL 1.0 [Section 6.4] Buffer Offset and Stride Requirements
    // The offset arguments to drawElements and vertexAttribPointer, and the stride argument to
    // vertexAttribPointer, must be a multiple of the size of the data type passed to the call,
    // or an INVALID_OPERATION error is generated.
    VertexFormatType internalType = GetVertexFormatType(type, normalized, 1, pureInteger);
    size_t typeSize               = GetVertexFormatTypeSize(internalType);

    ASSERT(isPow2(typeSize) && typeSize > 0);
    size_t sizeMask = (typeSize - 1);
    if ((reinterpret_cast<intptr_t>(ptr) & sizeMask) != 0)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Offset is not a multiple of the type size."));
        return false;
    }

    if ((stride & sizeMask) != 0)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Stride is not a multiple of the type size."));
        return false;
    }

    return true;
}

Program *GetValidProgram(ValidationContext *context, GLuint id)
{
    // ES3 spec (section 2.11.1) -- "Commands that accept shader or program object names will
    // generate the error INVALID_VALUE if the provided name is not the name of either a shader
    // or program object and INVALID_OPERATION if the provided name identifies an object
    // that is not the expected type."

    Program *validProgram = context->getProgram(id);

    if (!validProgram)
    {
        if (context->getShader(id))
        {
            context->handleError(
                Error(GL_INVALID_OPERATION, "Expected a program name, but found a shader name"));
        }
        else
        {
            context->handleError(Error(GL_INVALID_VALUE, "Program name is not valid"));
        }
    }

    return validProgram;
}

Shader *GetValidShader(ValidationContext *context, GLuint id)
{
    // See ValidProgram for spec details.

    Shader *validShader = context->getShader(id);

    if (!validShader)
    {
        if (context->getProgram(id))
        {
            context->handleError(
                Error(GL_INVALID_OPERATION, "Expected a shader name, but found a program name"));
        }
        else
        {
            context->handleError(Error(GL_INVALID_VALUE, "Shader name is invalid"));
        }
    }

    return validShader;
}

bool ValidateAttachmentTarget(gl::Context *context, GLenum attachment)
{
    if (attachment >= GL_COLOR_ATTACHMENT0_EXT && attachment <= GL_COLOR_ATTACHMENT15_EXT)
    {
        const unsigned int colorAttachment = (attachment - GL_COLOR_ATTACHMENT0_EXT);

        if (colorAttachment >= context->getCaps().maxColorAttachments)
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }
    else
    {
        switch (attachment)
        {
            case GL_DEPTH_ATTACHMENT:
            case GL_STENCIL_ATTACHMENT:
                break;

            case GL_DEPTH_STENCIL_ATTACHMENT:
                if (!context->getExtensions().webglCompatibility &&
                    context->getClientMajorVersion() < 3)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;

            default:
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
        }
    }

    return true;
}

bool ValidateRenderbufferStorageParametersBase(ValidationContext *context,
                                               GLenum target,
                                               GLsizei samples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height)
{
    switch (target)
    {
        case GL_RENDERBUFFER:
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    if (width < 0 || height < 0 || samples < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum convertedInternalFormat = context->getConvertedRenderbufferFormat(internalformat);

    const TextureCaps &formatCaps = context->getTextureCaps().get(convertedInternalFormat);
    if (!formatCaps.renderable)
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    // ANGLE_framebuffer_multisample does not explicitly state that the internal format must be
    // sized but it does state that the format must be in the ES2.0 spec table 4.5 which contains
    // only sized internal formats.
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(convertedInternalFormat);
    if (formatInfo.pixelBytes == 0)
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    if (static_cast<GLuint>(std::max(width, height)) > context->getCaps().maxRenderbufferSize)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    GLuint handle = context->getGLState().getRenderbufferId();
    if (handle == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return true;
}

bool ValidateFramebufferRenderbufferParameters(gl::Context *context,
                                               GLenum target,
                                               GLenum attachment,
                                               GLenum renderbuffertarget,
                                               GLuint renderbuffer)
{
    if (!ValidFramebufferTarget(target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    gl::Framebuffer *framebuffer = context->getGLState().getTargetFramebuffer(target);

    ASSERT(framebuffer);
    if (framebuffer->id() == 0)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Cannot change default FBO's attachments"));
        return false;
    }

    if (!ValidateAttachmentTarget(context, attachment))
    {
        return false;
    }

    // [OpenGL ES 2.0.25] Section 4.4.3 page 112
    // [OpenGL ES 3.0.2] Section 4.4.2 page 201
    // 'renderbuffer' must be either zero or the name of an existing renderbuffer object of
    // type 'renderbuffertarget', otherwise an INVALID_OPERATION error is generated.
    if (renderbuffer != 0)
    {
        if (!context->getRenderbuffer(renderbuffer))
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    return true;
}

bool ValidateBlitFramebufferParameters(ValidationContext *context,
                                       GLint srcX0,
                                       GLint srcY0,
                                       GLint srcX1,
                                       GLint srcY1,
                                       GLint dstX0,
                                       GLint dstY0,
                                       GLint dstX1,
                                       GLint dstY1,
                                       GLbitfield mask,
                                       GLenum filter)
{
    switch (filter)
    {
        case GL_NEAREST:
            break;
        case GL_LINEAR:
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    if ((mask & ~(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)) != 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (mask == 0)
    {
        // ES3.0 spec, section 4.3.2 specifies that a mask of zero is valid and no
        // buffers are copied.
        return false;
    }

    // ES3.0 spec, section 4.3.2 states that linear filtering is only available for the
    // color buffer, leaving only nearest being unfiltered from above
    if ((mask & ~GL_COLOR_BUFFER_BIT) != 0 && filter != GL_NEAREST)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const auto &glState              = context->getGLState();
    gl::Framebuffer *readFramebuffer = glState.getReadFramebuffer();
    gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();

    if (!readFramebuffer || !drawFramebuffer)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    if (readFramebuffer->id() == drawFramebuffer->id())
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (readFramebuffer->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    if (drawFramebuffer->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    if (drawFramebuffer->getSamples(context) != 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    bool sameBounds = srcX0 == dstX0 && srcY0 == dstY0 && srcX1 == dstX1 && srcY1 == dstY1;

    if (mask & GL_COLOR_BUFFER_BIT)
    {
        const gl::FramebufferAttachment *readColorBuffer = readFramebuffer->getReadColorbuffer();
        const Extensions &extensions                     = context->getExtensions();

        if (readColorBuffer)
        {
            const Format &readFormat = readColorBuffer->getFormat();

            for (size_t drawbufferIdx = 0;
                 drawbufferIdx < drawFramebuffer->getDrawbufferStateCount(); ++drawbufferIdx)
            {
                const FramebufferAttachment *attachment =
                    drawFramebuffer->getDrawBuffer(drawbufferIdx);
                if (attachment)
                {
                    const Format &drawFormat = attachment->getFormat();

                    // The GL ES 3.0.2 spec (pg 193) states that:
                    // 1) If the read buffer is fixed point format, the draw buffer must be as well
                    // 2) If the read buffer is an unsigned integer format, the draw buffer must be
                    // as well
                    // 3) If the read buffer is a signed integer format, the draw buffer must be as
                    // well
                    // Changes with EXT_color_buffer_float:
                    // Case 1) is changed to fixed point OR floating point
                    GLenum readComponentType = readFormat.info->componentType;
                    GLenum drawComponentType = drawFormat.info->componentType;
                    bool readFixedPoint      = (readComponentType == GL_UNSIGNED_NORMALIZED ||
                                           readComponentType == GL_SIGNED_NORMALIZED);
                    bool drawFixedPoint = (drawComponentType == GL_UNSIGNED_NORMALIZED ||
                                           drawComponentType == GL_SIGNED_NORMALIZED);

                    if (extensions.colorBufferFloat)
                    {
                        bool readFixedOrFloat = (readFixedPoint || readComponentType == GL_FLOAT);
                        bool drawFixedOrFloat = (drawFixedPoint || drawComponentType == GL_FLOAT);

                        if (readFixedOrFloat != drawFixedOrFloat)
                        {
                            context->handleError(Error(GL_INVALID_OPERATION,
                                                       "If the read buffer contains fixed-point or "
                                                       "floating-point values, the draw buffer "
                                                       "must as well."));
                            return false;
                        }
                    }
                    else if (readFixedPoint != drawFixedPoint)
                    {
                        context->handleError(Error(GL_INVALID_OPERATION,
                                                   "If the read buffer contains fixed-point "
                                                   "values, the draw buffer must as well."));
                        return false;
                    }

                    if (readComponentType == GL_UNSIGNED_INT &&
                        drawComponentType != GL_UNSIGNED_INT)
                    {
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                    }

                    if (readComponentType == GL_INT && drawComponentType != GL_INT)
                    {
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                    }

                    if (readColorBuffer->getSamples() > 0 &&
                        (!Format::SameSized(readFormat, drawFormat) || !sameBounds))
                    {
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                    }
                }
            }

            if ((readFormat.info->componentType == GL_INT ||
                 readFormat.info->componentType == GL_UNSIGNED_INT) &&
                filter == GL_LINEAR)
            {
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            }
        }
        // WebGL 2.0 BlitFramebuffer when blitting from a missing attachment
        // In OpenGL ES it is undefined what happens when an operation tries to blit from a missing
        // attachment and WebGL defines it to be an error. We do the check unconditionally as the
        // situation is an application error that would lead to a crash in ANGLE.
        else if (drawFramebuffer->hasEnabledDrawBuffer())
        {
            context->handleError(Error(
                GL_INVALID_OPERATION,
                "Attempt to read from a missing color attachment of a complete framebuffer."));
            return false;
        }
    }

    GLenum masks[]       = {GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT};
    GLenum attachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    for (size_t i = 0; i < 2; i++)
    {
        if (mask & masks[i])
        {
            const gl::FramebufferAttachment *readBuffer =
                readFramebuffer->getAttachment(attachments[i]);
            const gl::FramebufferAttachment *drawBuffer =
                drawFramebuffer->getAttachment(attachments[i]);

            if (readBuffer && drawBuffer)
            {
                if (!Format::SameSized(readBuffer->getFormat(), drawBuffer->getFormat()))
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }

                if (readBuffer->getSamples() > 0 && !sameBounds)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
            }
            // WebGL 2.0 BlitFramebuffer when blitting from a missing attachment
            else if (drawBuffer)
            {
                context->handleError(Error(GL_INVALID_OPERATION,
                                           "Attempt to read from a missing depth/stencil "
                                           "attachment of a complete framebuffer."));
                return false;
            }
        }
    }

    return true;
}

bool ValidateReadPixels(ValidationContext *context,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        GLvoid *pixels)
{
    return ValidateReadPixelsBase(context, x, y, width, height, format, type, -1, nullptr, nullptr,
                                  nullptr, pixels);
}

bool ValidateReadPixelsRobustANGLE(ValidationContext *context,
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
                                   GLvoid *pixels)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateReadPixelsBase(context, x, y, width, height, format, type, bufSize, length,
                                columns, rows, pixels))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateReadnPixelsEXT(Context *context,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            GLvoid *pixels)
{
    if (bufSize < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "bufSize must be a positive number"));
        return false;
    }

    return ValidateReadPixelsBase(context, x, y, width, height, format, type, bufSize, nullptr,
                                  nullptr, nullptr, pixels);
}

bool ValidateReadnPixelsRobustANGLE(ValidationContext *context,
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
                                    GLvoid *data)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateReadPixelsBase(context, x, y, width, height, format, type, bufSize, length,
                                columns, rows, data))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGenQueriesEXT(gl::Context *context, GLsizei n)
{
    if (!context->getExtensions().occlusionQueryBoolean &&
        !context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query extension not enabled"));
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteQueriesEXT(gl::Context *context, GLsizei n)
{
    if (!context->getExtensions().occlusionQueryBoolean &&
        !context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query extension not enabled"));
        return false;
    }

    return ValidateGenOrDelete(context, n);
}

bool ValidateBeginQueryBase(gl::Context *context, GLenum target, GLuint id)
{
    if (!ValidQueryType(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid query target"));
        return false;
    }

    if (id == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query id is 0"));
        return false;
    }

    // From EXT_occlusion_query_boolean: If BeginQueryEXT is called with an <id>
    // of zero, if the active query object name for <target> is non-zero (for the
    // targets ANY_SAMPLES_PASSED_EXT and ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, if
    // the active query for either target is non-zero), if <id> is the name of an
    // existing query object whose type does not match <target>, or if <id> is the
    // active query object name for any query type, the error INVALID_OPERATION is
    // generated.

    // Ensure no other queries are active
    // NOTE: If other queries than occlusion are supported, we will need to check
    // separately that:
    //    a) The query ID passed is not the current active query for any target/type
    //    b) There are no active queries for the requested target (and in the case
    //       of GL_ANY_SAMPLES_PASSED_EXT and GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
    //       no query may be active for either if glBeginQuery targets either.

    if (context->getGLState().isQueryActive(target))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Other query is active"));
        return false;
    }

    Query *queryObject = context->getQuery(id, true, target);

    // check that name was obtained with glGenQueries
    if (!queryObject)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Invalid query id"));
        return false;
    }

    // check for type mismatch
    if (queryObject->getType() != target)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query type does not match target"));
        return false;
    }

    return true;
}

bool ValidateBeginQueryEXT(gl::Context *context, GLenum target, GLuint id)
{
    if (!context->getExtensions().occlusionQueryBoolean &&
        !context->getExtensions().disjointTimerQuery && !context->getExtensions().syncQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query extension not enabled"));
        return false;
    }

    return ValidateBeginQueryBase(context, target, id);
}

bool ValidateEndQueryBase(gl::Context *context, GLenum target)
{
    if (!ValidQueryType(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid query target"));
        return false;
    }

    const Query *queryObject = context->getGLState().getActiveQuery(target);

    if (queryObject == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query target not active"));
        return false;
    }

    return true;
}

bool ValidateEndQueryEXT(gl::Context *context, GLenum target)
{
    if (!context->getExtensions().occlusionQueryBoolean &&
        !context->getExtensions().disjointTimerQuery && !context->getExtensions().syncQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query extension not enabled"));
        return false;
    }

    return ValidateEndQueryBase(context, target);
}

bool ValidateQueryCounterEXT(Context *context, GLuint id, GLenum target)
{
    if (!context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Disjoint timer query not enabled"));
        return false;
    }

    if (target != GL_TIMESTAMP_EXT)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid query target"));
        return false;
    }

    Query *queryObject = context->getQuery(id, true, target);
    if (queryObject == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Invalid query id"));
        return false;
    }

    if (context->getGLState().isQueryActive(queryObject))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query is active"));
        return false;
    }

    return true;
}

bool ValidateGetQueryivBase(Context *context, GLenum target, GLenum pname, GLsizei *numParams)
{
    if (numParams)
    {
        *numParams = 0;
    }

    if (!ValidQueryType(context, target) && target != GL_TIMESTAMP_EXT)
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid query type"));
        return false;
    }

    switch (pname)
    {
        case GL_CURRENT_QUERY_EXT:
            if (target == GL_TIMESTAMP_EXT)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "Cannot use current query for timestamp"));
                return false;
            }
            break;
        case GL_QUERY_COUNTER_BITS_EXT:
            if (!context->getExtensions().disjointTimerQuery ||
                (target != GL_TIMESTAMP_EXT && target != GL_TIME_ELAPSED_EXT))
            {
                context->handleError(Error(GL_INVALID_ENUM, "Invalid pname"));
                return false;
            }
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid pname"));
            return false;
    }

    if (numParams)
    {
        // All queries return only one value
        *numParams = 1;
    }

    return true;
}

bool ValidateGetQueryivEXT(Context *context, GLenum target, GLenum pname, GLint *params)
{
    if (!context->getExtensions().occlusionQueryBoolean &&
        !context->getExtensions().disjointTimerQuery && !context->getExtensions().syncQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query extension not enabled"));
        return false;
    }

    return ValidateGetQueryivBase(context, target, pname, nullptr);
}

bool ValidateGetQueryivRobustANGLE(Context *context,
                                   GLenum target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetQueryivBase(context, target, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetQueryObjectValueBase(Context *context, GLuint id, GLenum pname, GLsizei *numParams)
{
    if (numParams)
    {
        *numParams = 0;
    }

    Query *queryObject = context->getQuery(id, false, GL_NONE);

    if (!queryObject)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query does not exist"));
        return false;
    }

    if (context->getGLState().isQueryActive(queryObject))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query currently active"));
        return false;
    }

    switch (pname)
    {
        case GL_QUERY_RESULT_EXT:
        case GL_QUERY_RESULT_AVAILABLE_EXT:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid pname enum"));
            return false;
    }

    if (numParams)
    {
        *numParams = 1;
    }

    return true;
}

bool ValidateGetQueryObjectivEXT(Context *context, GLuint id, GLenum pname, GLint *params)
{
    if (!context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Timer query extension not enabled"));
        return false;
    }
    return ValidateGetQueryObjectValueBase(context, id, pname, nullptr);
}

bool ValidateGetQueryObjectivRobustANGLE(Context *context,
                                         GLuint id,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params)
{
    if (!context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Timer query extension not enabled"));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetQueryObjectValueBase(context, id, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetQueryObjectuivEXT(Context *context, GLuint id, GLenum pname, GLuint *params)
{
    if (!context->getExtensions().disjointTimerQuery &&
        !context->getExtensions().occlusionQueryBoolean && !context->getExtensions().syncQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query extension not enabled"));
        return false;
    }
    return ValidateGetQueryObjectValueBase(context, id, pname, nullptr);
}

bool ValidateGetQueryObjectuivRobustANGLE(Context *context,
                                          GLuint id,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLuint *params)
{
    if (!context->getExtensions().disjointTimerQuery &&
        !context->getExtensions().occlusionQueryBoolean && !context->getExtensions().syncQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Query extension not enabled"));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetQueryObjectValueBase(context, id, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetQueryObjecti64vEXT(Context *context, GLuint id, GLenum pname, GLint64 *params)
{
    if (!context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Timer query extension not enabled"));
        return false;
    }
    return ValidateGetQueryObjectValueBase(context, id, pname, nullptr);
}

bool ValidateGetQueryObjecti64vRobustANGLE(Context *context,
                                           GLuint id,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint64 *params)
{
    if (!context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Timer query extension not enabled"));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetQueryObjectValueBase(context, id, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetQueryObjectui64vEXT(Context *context, GLuint id, GLenum pname, GLuint64 *params)
{
    if (!context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Timer query extension not enabled"));
        return false;
    }
    return ValidateGetQueryObjectValueBase(context, id, pname, nullptr);
}

bool ValidateGetQueryObjectui64vRobustANGLE(Context *context,
                                            GLuint id,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint64 *params)
{
    if (!context->getExtensions().disjointTimerQuery)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Timer query extension not enabled"));
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetQueryObjectValueBase(context, id, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateProgramUniform(gl::Context *context,
                            GLenum valueType,
                            GLuint program,
                            GLint location,
                            GLsizei count)
{
    // Check for ES31 program uniform entry points
    if (context->getClientVersion() < Version(3, 1))
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = GetValidProgram(context, program);
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniformValue(context, valueType, uniform->type);
}

bool ValidateProgramUniform1iv(gl::Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value)
{
    // Check for ES31 program uniform entry points
    if (context->getClientVersion() < Version(3, 1))
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = GetValidProgram(context, program);
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniform1ivValue(context, uniform->type, count, value);
}

bool ValidateProgramUniformMatrix(gl::Context *context,
                                  GLenum valueType,
                                  GLuint program,
                                  GLint location,
                                  GLsizei count,
                                  GLboolean transpose)
{
    // Check for ES31 program uniform entry points
    if (context->getClientVersion() < Version(3, 1))
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = GetValidProgram(context, program);
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniformMatrixValue(context, valueType, uniform->type);
}

bool ValidateUniform(ValidationContext *context, GLenum valueType, GLint location, GLsizei count)
{
    // Check for ES3 uniform entry points
    if (VariableComponentType(valueType) == GL_UNSIGNED_INT && context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = context->getGLState().getProgram();
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniformValue(context, valueType, uniform->type);
}

bool ValidateUniform1iv(gl::Context *context, GLint location, GLsizei count, const GLint *value)
{
    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = context->getGLState().getProgram();
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniform1ivValue(context, uniform->type, count, value);
}

bool ValidateUniformMatrix(ValidationContext *context,
                           GLenum valueType,
                           GLint location,
                           GLsizei count,
                           GLboolean transpose)
{
    // Check for ES3 uniform entry points
    int rows = VariableRowCount(valueType);
    int cols = VariableColumnCount(valueType);
    if (rows != cols && context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (transpose != GL_FALSE && context->getClientMajorVersion() < 3)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    const LinkedUniform *uniform = nullptr;
    gl::Program *programObject   = context->getGLState().getProgram();
    return ValidateUniformCommonBase(context, programObject, location, count, &uniform) &&
           ValidateUniformMatrixValue(context, valueType, uniform->type);
}

bool ValidateStateQuery(ValidationContext *context,
                        GLenum pname,
                        GLenum *nativeType,
                        unsigned int *numParams)
{
    if (!context->getQueryParameterInfo(pname, nativeType, numParams))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    const Caps &caps = context->getCaps();

    if (pname >= GL_DRAW_BUFFER0 && pname <= GL_DRAW_BUFFER15)
    {
        unsigned int colorAttachment = (pname - GL_DRAW_BUFFER0);

        if (colorAttachment >= caps.maxDrawBuffers)
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    switch (pname)
    {
        case GL_TEXTURE_BINDING_2D:
        case GL_TEXTURE_BINDING_CUBE_MAP:
        case GL_TEXTURE_BINDING_3D:
        case GL_TEXTURE_BINDING_2D_ARRAY:
            break;
        case GL_TEXTURE_BINDING_EXTERNAL_OES:
            if (!context->getExtensions().eglStreamConsumerExternal &&
                !context->getExtensions().eglImageExternal)
            {
                context->handleError(Error(GL_INVALID_ENUM,
                                           "Neither NV_EGL_stream_consumer_external nor "
                                           "GL_OES_EGL_image_external extensions enabled"));
                return false;
            }
            break;

        case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        {
            if (context->getGLState().getReadFramebuffer()->checkStatus(context) !=
                GL_FRAMEBUFFER_COMPLETE)
            {
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            }

            const Framebuffer *framebuffer = context->getGLState().getReadFramebuffer();
            ASSERT(framebuffer);

            if (framebuffer->getReadBufferState() == GL_NONE)
            {
                context->handleError(Error(GL_INVALID_OPERATION, "Read buffer is GL_NONE"));
                return false;
            }

            const FramebufferAttachment *attachment = framebuffer->getReadColorbuffer();
            if (!attachment)
            {
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
            }
        }
        break;

        default:
            break;
    }

    // pname is valid, but there are no parameters to return
    if (*numParams == 0)
    {
        return false;
    }

    return true;
}

bool ValidateRobustStateQuery(ValidationContext *context,
                              GLenum pname,
                              GLsizei bufSize,
                              GLenum *nativeType,
                              unsigned int *numParams)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateStateQuery(context, pname, nativeType, numParams))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *numParams))
    {
        return false;
    }

    return true;
}

bool ValidateCopyTexImageParametersBase(ValidationContext *context,
                                        GLenum target,
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
                                        Format *textureFormatOut)
{
    if (level < 0 || xoffset < 0 || yoffset < 0 || zoffset < 0 || width < 0 || height < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (std::numeric_limits<GLsizei>::max() - xoffset < width ||
        std::numeric_limits<GLsizei>::max() - yoffset < height)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (border != 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (!ValidMipLevel(context, target, level))
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    const auto &state    = context->getGLState();
    auto readFramebuffer = state.getReadFramebuffer();
    if (readFramebuffer->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    if (readFramebuffer->id() != 0 && readFramebuffer->getSamples(context) != 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (readFramebuffer->getReadBufferState() == GL_NONE)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Read buffer is GL_NONE"));
        return false;
    }

    // WebGL 1.0 [Section 6.26] Reading From a Missing Attachment
    // In OpenGL ES it is undefined what happens when an operation tries to read from a missing
    // attachment and WebGL defines it to be an error. We do the check unconditionally as the
    // situation is an application error that would lead to a crash in ANGLE.
    if (readFramebuffer->getReadColorbuffer() == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Missing read attachment"));
        return false;
    }

    const gl::Caps &caps = context->getCaps();

    GLuint maxDimension = 0;
    switch (target)
    {
        case GL_TEXTURE_2D:
            maxDimension = caps.max2DTextureSize;
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            maxDimension = caps.maxCubeMapTextureSize;
            break;

        case GL_TEXTURE_2D_ARRAY:
            maxDimension = caps.max2DTextureSize;
            break;

        case GL_TEXTURE_3D:
            maxDimension = caps.max3DTextureSize;
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    gl::Texture *texture =
        state.getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    if (!texture)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (texture->getImmutableFormat() && !isSubImage)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalformat);

    if (formatInfo.depthBits > 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (formatInfo.compressed &&
        !ValidCompressedImageSize(context, internalformat, xoffset, yoffset, width, height))
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (isSubImage)
    {
        if (static_cast<size_t>(xoffset + width) > texture->getWidth(target, level) ||
            static_cast<size_t>(yoffset + height) > texture->getHeight(target, level) ||
            static_cast<size_t>(zoffset) >= texture->getDepth(target, level))
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }
    else
    {
        if (IsCubeMapTextureTarget(target) && width != height)
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }

        if (!formatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
        {
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
        }

        int maxLevelDimension = (maxDimension >> level);
        if (static_cast<int>(width) > maxLevelDimension ||
            static_cast<int>(height) > maxLevelDimension)
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }

    if (textureFormatOut)
    {
        *textureFormatOut = texture->getFormat(target, level);
    }

    // Detect texture copying feedback loops for WebGL.
    if (context->getExtensions().webglCompatibility)
    {
        if (readFramebuffer->formsCopyingFeedbackLoopWith(texture->id(), level, zoffset))
        {
            context->handleError(Error(GL_INVALID_OPERATION,
                                       "Texture copying feedback loop formed between Framebuffer "
                                       "and specified Texture level."));
            return false;
        }
    }

    return true;
}

bool ValidateDrawBase(ValidationContext *context, GLenum mode, GLsizei count)
{
    switch (mode)
    {
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    if (count < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    const State &state = context->getGLState();

    // Check for mapped buffers
    if (state.hasMappedBuffer(GL_ARRAY_BUFFER))
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // Note: these separate values are not supported in WebGL, due to D3D's limitations. See
    // Section 6.10 of the WebGL 1.0 spec.
    Framebuffer *framebuffer = state.getDrawFramebuffer();
    if (context->getLimitations().noSeparateStencilRefsAndMasks ||
        context->getExtensions().webglCompatibility)
    {
        const FramebufferAttachment *dsAttachment =
            framebuffer->getStencilOrDepthStencilAttachment();
        GLuint stencilBits                = dsAttachment ? dsAttachment->getStencilSize() : 0;
        GLuint minimumRequiredStencilMask = (1 << stencilBits) - 1;
        const DepthStencilState &depthStencilState = state.getDepthStencilState();

        bool differentRefs = state.getStencilRef() != state.getStencilBackRef();
        bool differentWritemasks =
            (depthStencilState.stencilWritemask & minimumRequiredStencilMask) !=
            (depthStencilState.stencilBackWritemask & minimumRequiredStencilMask);
        bool differentMasks = (depthStencilState.stencilMask & minimumRequiredStencilMask) !=
                              (depthStencilState.stencilBackMask & minimumRequiredStencilMask);

        if (differentRefs || differentWritemasks || differentMasks)
        {
            if (!context->getExtensions().webglCompatibility)
            {
                ERR() << "This ANGLE implementation does not support separate front/back stencil "
                         "writemasks, reference values, or stencil mask values.";
            }
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    if (framebuffer->checkStatus(context) != GL_FRAMEBUFFER_COMPLETE)
    {
        context->handleError(Error(GL_INVALID_FRAMEBUFFER_OPERATION));
        return false;
    }

    gl::Program *program = state.getProgram();
    if (!program)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (!program->validateSamplers(NULL, context->getCaps()))
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // Uniform buffer validation
    for (unsigned int uniformBlockIndex = 0;
         uniformBlockIndex < program->getActiveUniformBlockCount(); uniformBlockIndex++)
    {
        const gl::UniformBlock &uniformBlock = program->getUniformBlockByIndex(uniformBlockIndex);
        GLuint blockBinding                  = program->getUniformBlockBinding(uniformBlockIndex);
        const OffsetBindingPointer<Buffer> &uniformBuffer =
            state.getIndexedUniformBuffer(blockBinding);

        if (uniformBuffer.get() == nullptr)
        {
            // undefined behaviour
            context->handleError(
                Error(GL_INVALID_OPERATION,
                      "It is undefined behaviour to have a used but unbound uniform buffer."));
            return false;
        }

        size_t uniformBufferSize = uniformBuffer.getSize();
        if (uniformBufferSize == 0)
        {
            // Bind the whole buffer.
            uniformBufferSize = static_cast<size_t>(uniformBuffer->getSize());
        }

        if (uniformBufferSize < uniformBlock.dataSize)
        {
            // undefined behaviour
            context->handleError(
                Error(GL_INVALID_OPERATION,
                      "It is undefined behaviour to use a uniform buffer that is too small."));
            return false;
        }
    }

    // Detect rendering feedback loops for WebGL.
    if (context->getExtensions().webglCompatibility)
    {
        if (framebuffer->formsRenderingFeedbackLoopWith(state))
        {
            context->handleError(
                Error(GL_INVALID_OPERATION,
                      "Rendering feedback loop formed between Framebuffer and active Texture."));
            return false;
        }
    }

    // No-op if zero count
    return (count > 0);
}

bool ValidateDrawArraysCommon(ValidationContext *context,
                              GLenum mode,
                              GLint first,
                              GLsizei count,
                              GLsizei primcount)
{
    if (first < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    const State &state                          = context->getGLState();
    gl::TransformFeedback *curTransformFeedback = state.getCurrentTransformFeedback();
    if (curTransformFeedback && curTransformFeedback->isActive() &&
        !curTransformFeedback->isPaused() && curTransformFeedback->getPrimitiveMode() != mode)
    {
        // It is an invalid operation to call DrawArrays or DrawArraysInstanced with a draw mode
        // that does not match the current transform feedback object's draw mode (if transform
        // feedback
        // is active), (3.0.2, section 2.14, pg 86)
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (!ValidateDrawBase(context, mode, count))
    {
        return false;
    }

    // Check the computation of maxVertex doesn't overflow.
    // - first < 0 or count < 0 have been checked as an error condition
    // - count > 0 has been checked in ValidateDrawBase as it makes the call a noop
    // From this we know maxVertex will be positive, and only need to check if it overflows GLint.
    ASSERT(count > 0 && first >= 0);
    int64_t maxVertex = static_cast<int64_t>(first) + static_cast<int64_t>(count) - 1;
    if (maxVertex > static_cast<int64_t>(std::numeric_limits<GLint>::max()))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Integer overflow."));
        return false;
    }

    if (!ValidateDrawAttribs(context, primcount, static_cast<GLint>(maxVertex), count))
    {
        return false;
    }

    return true;
}

bool ValidateDrawArraysInstanced(Context *context,
                                 GLenum mode,
                                 GLint first,
                                 GLsizei count,
                                 GLsizei primcount)
{
    if (primcount < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (!ValidateDrawArraysCommon(context, mode, first, count, primcount))
    {
        return false;
    }

    // No-op if zero primitive count
    return (primcount > 0);
}

static bool ValidateDrawInstancedANGLE(Context *context)
{
    // Verify there is at least one active attribute with a divisor of zero
    const gl::State &state = context->getGLState();

    gl::Program *program = state.getProgram();

    const auto &attribs  = state.getVertexArray()->getVertexAttributes();
    const auto &bindings = state.getVertexArray()->getVertexBindings();
    for (size_t attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        const VertexAttribute &attrib = attribs[attributeIndex];
        const VertexBinding &binding  = bindings[attrib.bindingIndex];
        if (program->isAttribLocationActive(attributeIndex) && binding.divisor == 0)
        {
            return true;
        }
    }

    context->handleError(Error(GL_INVALID_OPERATION,
                               "ANGLE_instanced_arrays requires that at least one active attribute"
                               "has a divisor of zero."));
    return false;
}

bool ValidateDrawArraysInstancedANGLE(Context *context,
                                      GLenum mode,
                                      GLint first,
                                      GLsizei count,
                                      GLsizei primcount)
{
    if (!ValidateDrawInstancedANGLE(context))
    {
        return false;
    }

    return ValidateDrawArraysInstanced(context, mode, first, count, primcount);
}

bool ValidateDrawElementsBase(ValidationContext *context, GLenum type)
{
    switch (type)
    {
        case GL_UNSIGNED_BYTE:
        case GL_UNSIGNED_SHORT:
            break;
        case GL_UNSIGNED_INT:
            if (context->getClientMajorVersion() < 3 && !context->getExtensions().elementIndexUint)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;
        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    const State &state = context->getGLState();

    gl::TransformFeedback *curTransformFeedback = state.getCurrentTransformFeedback();
    if (curTransformFeedback && curTransformFeedback->isActive() &&
        !curTransformFeedback->isPaused())
    {
        // It is an invalid operation to call DrawElements, DrawRangeElements or
        // DrawElementsInstanced
        // while transform feedback is active, (3.0.2, section 2.14, pg 86)
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return true;
}

bool ValidateDrawElements(ValidationContext *context,
                          GLenum mode,
                          GLsizei count,
                          GLenum type,
                          const GLvoid *indices,
                          GLsizei primcount,
                          IndexRange *indexRangeOut)
{
    if (!ValidateDrawElementsBase(context, type))
        return false;

    const State &state = context->getGLState();

    // Check for mapped buffers
    if (state.hasMappedBuffer(GL_ELEMENT_ARRAY_BUFFER))
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Index buffer is mapped."));
        return false;
    }

    const gl::VertexArray *vao     = state.getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer().get();

    GLuint typeBytes = gl::GetTypeInfo(type).bytes;

    if (context->getExtensions().webglCompatibility)
    {
        ASSERT(isPow2(typeBytes) && typeBytes > 0);
        if ((reinterpret_cast<uintptr_t>(indices) & static_cast<uintptr_t>(typeBytes - 1)) != 0)
        {
            // [WebGL 1.0] Section 6.4 Buffer Offset and Stride Requirements
            // The offset arguments to drawElements and [...], must be a multiple of the size of the
            // data type passed to the call, or an INVALID_OPERATION error is generated.
            context->handleError(Error(GL_INVALID_OPERATION,
                                       "indices must be a multiple of the element type size."));
            return false;
        }

        // [WebGL 1.0] Section 6.4 Buffer Offset and Stride Requirements
        // In addition the offset argument to drawElements must be non-negative or an INVALID_VALUE
        // error is generated.
        if (reinterpret_cast<intptr_t>(indices) < 0)
        {
            context->handleError(Error(GL_INVALID_VALUE, "Offset < 0."));
            return false;
        }
    }

    if (context->getExtensions().webglCompatibility ||
        !context->getGLState().areClientArraysEnabled())
    {
        if (!elementArrayBuffer && count > 0)
        {
            // [WebGL 1.0] Section 6.2 No Client Side Arrays
            // If drawElements is called with a count greater than zero, and no WebGLBuffer is bound
            // to the ELEMENT_ARRAY_BUFFER binding point, an INVALID_OPERATION error is generated.
            context->handleError(Error(GL_INVALID_OPERATION,
                                       "There is no element array buffer bound and count > 0."));
            return false;
        }
    }

    if (count > 0)
    {
        if (elementArrayBuffer)
        {
            // The max possible type size is 8 and count is on 32 bits so doing the multiplication
            // in a 64 bit integer is safe. Also we are guaranteed that here count > 0.
            static_assert(std::is_same<int, GLsizei>::value, "GLsizei isn't the expected type");
            constexpr uint64_t kMaxTypeSize = 8;
            constexpr uint64_t kIntMax      = std::numeric_limits<int>::max();
            constexpr uint64_t kUint64Max   = std::numeric_limits<uint64_t>::max();
            static_assert(kIntMax < kUint64Max / kMaxTypeSize, "");

            uint64_t typeSize     = typeBytes;
            uint64_t elementCount = static_cast<uint64_t>(count);
            ASSERT(elementCount > 0 && typeSize <= kMaxTypeSize);

            // Doing the multiplication here is overflow-safe
            uint64_t elementDataSizeNoOffset = typeSize * elementCount;

            // The offset can be any value, check for overflows
            uint64_t offset = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indices));
            if (elementDataSizeNoOffset > kUint64Max - offset)
            {
                context->handleError(Error(GL_INVALID_OPERATION, "Integer overflow."));
                return false;
            }

            uint64_t elementDataSizeWithOffset = elementDataSizeNoOffset + offset;
            if (elementDataSizeWithOffset > static_cast<uint64_t>(elementArrayBuffer->getSize()))
            {
                context->handleError(
                    Error(GL_INVALID_OPERATION, "Index buffer is not big enough for the draw."));
                return false;
            }
        }
        else if (!indices)
        {
            // This is an application error that would normally result in a crash,
            // but we catch it and return an error
            context->handleError(
                Error(GL_INVALID_OPERATION, "No element array buffer and no pointer."));
            return false;
        }
    }

    if (!ValidateDrawBase(context, mode, count))
    {
        return false;
    }

    // Use max index to validate if our vertex buffers are large enough for the pull.
    // TODO: offer fast path, with disabled index validation.
    // TODO: also disable index checking on back-ends that are robust to out-of-range accesses.
    if (elementArrayBuffer)
    {
        uintptr_t offset = reinterpret_cast<uintptr_t>(indices);
        Error error =
            elementArrayBuffer->getIndexRange(type, static_cast<size_t>(offset), count,
                                              state.isPrimitiveRestartEnabled(), indexRangeOut);
        if (error.isError())
        {
            context->handleError(error);
            return false;
        }
    }
    else
    {
        *indexRangeOut = ComputeIndexRange(type, indices, count, state.isPrimitiveRestartEnabled());
    }

    // If we use an index greater than our maximum supported index range, return an error.
    // The ES3 spec does not specify behaviour here, it is undefined, but ANGLE should always
    // return an error if possible here.
    if (static_cast<GLuint64>(indexRangeOut->end) >= context->getCaps().maxElementIndex)
    {
        context->handleError(Error(GL_INVALID_OPERATION, g_ExceedsMaxElementErrorMessage));
        return false;
    }

    if (!ValidateDrawAttribs(context, primcount, static_cast<GLint>(indexRangeOut->end),
                             static_cast<GLint>(indexRangeOut->vertexCount())))
    {
        return false;
    }

    // No op if there are no real indices in the index data (all are primitive restart).
    return (indexRangeOut->vertexIndexCount > 0);
}

bool ValidateDrawElementsInstanced(Context *context,
                                   GLenum mode,
                                   GLsizei count,
                                   GLenum type,
                                   const GLvoid *indices,
                                   GLsizei primcount,
                                   IndexRange *indexRangeOut)
{
    if (primcount < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (!ValidateDrawElements(context, mode, count, type, indices, primcount, indexRangeOut))
    {
        return false;
    }

    // No-op zero primitive count
    return (primcount > 0);
}

bool ValidateDrawElementsInstancedANGLE(Context *context,
                                        GLenum mode,
                                        GLsizei count,
                                        GLenum type,
                                        const GLvoid *indices,
                                        GLsizei primcount,
                                        IndexRange *indexRangeOut)
{
    if (!ValidateDrawInstancedANGLE(context))
    {
        return false;
    }

    return ValidateDrawElementsInstanced(context, mode, count, type, indices, primcount,
                                         indexRangeOut);
}

bool ValidateFramebufferTextureBase(Context *context,
                                    GLenum target,
                                    GLenum attachment,
                                    GLuint texture,
                                    GLint level)
{
    if (!ValidFramebufferTarget(target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    if (!ValidateAttachmentTarget(context, attachment))
    {
        return false;
    }

    if (texture != 0)
    {
        gl::Texture *tex = context->getTexture(texture);

        if (tex == NULL)
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }

        if (level < 0)
        {
            context->handleError(Error(GL_INVALID_VALUE));
            return false;
        }
    }

    const gl::Framebuffer *framebuffer = context->getGLState().getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->id() == 0)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Cannot change default FBO's attachments"));
        return false;
    }

    return true;
}

bool ValidateFramebufferTexture2D(Context *context,
                                  GLenum target,
                                  GLenum attachment,
                                  GLenum textarget,
                                  GLuint texture,
                                  GLint level)
{
    // Attachments are required to be bound to level 0 without ES3 or the GL_OES_fbo_render_mipmap
    // extension
    if (context->getClientMajorVersion() < 3 && !context->getExtensions().fboRenderMipmap &&
        level != 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    if (!ValidateFramebufferTextureBase(context, target, attachment, texture, level))
    {
        return false;
    }

    if (texture != 0)
    {
        gl::Texture *tex = context->getTexture(texture);
        ASSERT(tex);

        const gl::Caps &caps = context->getCaps();

        switch (textarget)
        {
            case GL_TEXTURE_2D:
            {
                if (level > gl::log2(caps.max2DTextureSize))
                {
                    context->handleError(Error(GL_INVALID_VALUE));
                    return false;
                }
                if (tex->getTarget() != GL_TEXTURE_2D)
                {
                    context->handleError(Error(GL_INVALID_OPERATION,
                                               "Textarget must match the texture target type."));
                    return false;
                }
            }
            break;

            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            {
                if (level > gl::log2(caps.maxCubeMapTextureSize))
                {
                    context->handleError(Error(GL_INVALID_VALUE));
                    return false;
                }
                if (tex->getTarget() != GL_TEXTURE_CUBE_MAP)
                {
                    context->handleError(Error(GL_INVALID_OPERATION,
                                               "Textarget must match the texture target type."));
                    return false;
                }
            }
            break;

            case GL_TEXTURE_2D_MULTISAMPLE:
            {
                if (context->getClientVersion() < ES_3_1)
                {
                    context->handleError(Error(GL_INVALID_OPERATION,
                                               "Texture target requires at least OpenGL ES 3.1."));
                    return false;
                }

                if (level != 0)
                {
                    context->handleError(
                        Error(GL_INVALID_VALUE, "Level must be 0 for TEXTURE_2D_MULTISAMPLE."));
                    return false;
                }
                if (tex->getTarget() != GL_TEXTURE_2D_MULTISAMPLE)
                {
                    context->handleError(Error(GL_INVALID_OPERATION,
                                               "Textarget must match the texture target type."));
                    return false;
                }
            }
            break;

            default:
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
        }

        const Format &format = tex->getFormat(textarget, level);
        if (format.info->compressed)
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }
    }

    return true;
}

bool ValidateGetUniformBase(Context *context, GLuint program, GLint location)
{
    if (program == 0)
    {
        context->handleError(Error(GL_INVALID_VALUE));
        return false;
    }

    gl::Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    if (!programObject || !programObject->isLinked())
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    if (!programObject->isValidUniformLocation(location))
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return true;
}

bool ValidateGetUniformfv(Context *context, GLuint program, GLint location, GLfloat *params)
{
    return ValidateGetUniformBase(context, program, location);
}

bool ValidateGetUniformiv(Context *context, GLuint program, GLint location, GLint *params)
{
    return ValidateGetUniformBase(context, program, location);
}

static bool ValidateSizedGetUniform(Context *context,
                                    GLuint program,
                                    GLint location,
                                    GLsizei bufSize,
                                    GLsizei *length)
{
    if (length)
    {
        *length = 0;
    }

    if (!ValidateGetUniformBase(context, program, location))
    {
        return false;
    }

    if (bufSize < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "bufSize cannot be negative."));
        return false;
    }

    gl::Program *programObject = context->getProgram(program);
    ASSERT(programObject);

    // sized queries -- ensure the provided buffer is large enough
    const LinkedUniform &uniform = programObject->getUniformByLocation(location);
    size_t requiredBytes         = VariableExternalSize(uniform.type);
    if (static_cast<size_t>(bufSize) < requiredBytes)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "bufSize of at least %u is required.", requiredBytes));
        return false;
    }

    if (length)
    {
        *length = VariableComponentCount(uniform.type);
    }

    return true;
}

bool ValidateGetnUniformfvEXT(Context *context,
                              GLuint program,
                              GLint location,
                              GLsizei bufSize,
                              GLfloat *params)
{
    return ValidateSizedGetUniform(context, program, location, bufSize, nullptr);
}

bool ValidateGetnUniformivEXT(Context *context,
                              GLuint program,
                              GLint location,
                              GLsizei bufSize,
                              GLint *params)
{
    return ValidateSizedGetUniform(context, program, location, bufSize, nullptr);
}

bool ValidateGetUniformfvRobustANGLE(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLfloat *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    // bufSize is validated in ValidateSizedGetUniform
    return ValidateSizedGetUniform(context, program, location, bufSize, length);
}

bool ValidateGetUniformivRobustANGLE(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    // bufSize is validated in ValidateSizedGetUniform
    return ValidateSizedGetUniform(context, program, location, bufSize, length);
}

bool ValidateGetUniformuivRobustANGLE(Context *context,
                                      GLuint program,
                                      GLint location,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLuint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (context->getClientMajorVersion() < 3)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Entry point requires at least OpenGL ES 3.0."));
        return false;
    }

    // bufSize is validated in ValidateSizedGetUniform
    return ValidateSizedGetUniform(context, program, location, bufSize, length);
}

bool ValidateDiscardFramebufferBase(Context *context,
                                    GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments,
                                    bool defaultFramebuffer)
{
    if (numAttachments < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "numAttachments must not be less than zero"));
        return false;
    }

    for (GLsizei i = 0; i < numAttachments; ++i)
    {
        if (attachments[i] >= GL_COLOR_ATTACHMENT0 && attachments[i] <= GL_COLOR_ATTACHMENT31)
        {
            if (defaultFramebuffer)
            {
                context->handleError(Error(
                    GL_INVALID_ENUM, "Invalid attachment when the default framebuffer is bound"));
                return false;
            }

            if (attachments[i] >= GL_COLOR_ATTACHMENT0 + context->getCaps().maxColorAttachments)
            {
                context->handleError(Error(GL_INVALID_OPERATION,
                                           "Requested color attachment is greater than the maximum "
                                           "supported color attachments"));
                return false;
            }
        }
        else
        {
            switch (attachments[i])
            {
                case GL_DEPTH_ATTACHMENT:
                case GL_STENCIL_ATTACHMENT:
                case GL_DEPTH_STENCIL_ATTACHMENT:
                    if (defaultFramebuffer)
                    {
                        context->handleError(
                            Error(GL_INVALID_ENUM,
                                  "Invalid attachment when the default framebuffer is bound"));
                        return false;
                    }
                    break;
                case GL_COLOR:
                case GL_DEPTH:
                case GL_STENCIL:
                    if (!defaultFramebuffer)
                    {
                        context->handleError(
                            Error(GL_INVALID_ENUM,
                                  "Invalid attachment when the default framebuffer is not bound"));
                        return false;
                    }
                    break;
                default:
                    context->handleError(Error(GL_INVALID_ENUM, "Invalid attachment"));
                    return false;
            }
        }
    }

    return true;
}

bool ValidateInsertEventMarkerEXT(Context *context, GLsizei length, const char *marker)
{
    // Note that debug marker calls must not set error state

    if (length < 0)
    {
        return false;
    }

    if (marker == nullptr)
    {
        return false;
    }

    return true;
}

bool ValidatePushGroupMarkerEXT(Context *context, GLsizei length, const char *marker)
{
    // Note that debug marker calls must not set error state

    if (length < 0)
    {
        return false;
    }

    if (length > 0 && marker == nullptr)
    {
        return false;
    }

    return true;
}

bool ValidateEGLImageTargetTexture2DOES(Context *context,
                                        egl::Display *display,
                                        GLenum target,
                                        egl::Image *image)
{
    if (!context->getExtensions().eglImage && !context->getExtensions().eglImageExternal)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    switch (target)
    {
        case GL_TEXTURE_2D:
            if (!context->getExtensions().eglImage)
            {
                context->handleError(Error(
                    GL_INVALID_ENUM, "GL_TEXTURE_2D texture target requires GL_OES_EGL_image."));
            }
            break;

        case GL_TEXTURE_EXTERNAL_OES:
            if (!context->getExtensions().eglImageExternal)
            {
                context->handleError(Error(
                    GL_INVALID_ENUM,
                    "GL_TEXTURE_EXTERNAL_OES texture target requires GL_OES_EGL_image_external."));
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "invalid texture target."));
            return false;
    }

    if (!display->isValidImage(image))
    {
        context->handleError(Error(GL_INVALID_VALUE, "EGL image is not valid."));
        return false;
    }

    if (image->getSamples() > 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION,
                                   "cannot create a 2D texture from a multisampled EGL image."));
        return false;
    }

    const TextureCaps &textureCaps = context->getTextureCaps().get(image->getFormat().asSized());
    if (!textureCaps.texturable)
    {
        context->handleError(Error(GL_INVALID_OPERATION,
                                   "EGL image internal format is not supported as a texture."));
        return false;
    }

    return true;
}

bool ValidateEGLImageTargetRenderbufferStorageOES(Context *context,
                                                  egl::Display *display,
                                                  GLenum target,
                                                  egl::Image *image)
{
    if (!context->getExtensions().eglImage)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    switch (target)
    {
        case GL_RENDERBUFFER:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "invalid renderbuffer target."));
            return false;
    }

    if (!display->isValidImage(image))
    {
        context->handleError(Error(GL_INVALID_VALUE, "EGL image is not valid."));
        return false;
    }

    const TextureCaps &textureCaps = context->getTextureCaps().get(image->getFormat().asSized());
    if (!textureCaps.renderable)
    {
        context->handleError(Error(
            GL_INVALID_OPERATION, "EGL image internal format is not supported as a renderbuffer."));
        return false;
    }

    return true;
}

bool ValidateBindVertexArrayBase(Context *context, GLuint array)
{
    if (!context->isVertexArrayGenerated(array))
    {
        // The default VAO should always exist
        ASSERT(array != 0);
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return true;
}

bool ValidateLinkProgram(Context *context, GLuint program)
{
    if (context->hasActiveTransformFeedback(program))
    {
        // ES 3.0.4 section 2.15 page 91
        context->handleError(Error(GL_INVALID_OPERATION,
                                   "Cannot link program while program is associated with an active "
                                   "transform feedback object."));
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateProgramBinaryBase(Context *context,
                               GLuint program,
                               GLenum binaryFormat,
                               const void *binary,
                               GLint length)
{
    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }

    const std::vector<GLenum> &programBinaryFormats = context->getCaps().programBinaryFormats;
    if (std::find(programBinaryFormats.begin(), programBinaryFormats.end(), binaryFormat) ==
        programBinaryFormats.end())
    {
        context->handleError(Error(GL_INVALID_ENUM, "Program binary format is not valid."));
        return false;
    }

    if (context->hasActiveTransformFeedback(program))
    {
        // ES 3.0.4 section 2.15 page 91
        context->handleError(Error(GL_INVALID_OPERATION,
                                   "Cannot change program binary while program is associated with "
                                   "an active transform feedback object."));
        return false;
    }

    return true;
}

bool ValidateGetProgramBinaryBase(Context *context,
                                  GLuint program,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLenum *binaryFormat,
                                  void *binary)
{
    Program *programObject = GetValidProgram(context, program);
    if (programObject == nullptr)
    {
        return false;
    }

    if (!programObject->isLinked())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Program is not linked."));
        return false;
    }

    if (context->getCaps().programBinaryFormats.empty())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "No program binary formats supported."));
        return false;
    }

    return true;
}

bool ValidateUseProgram(Context *context, GLuint program)
{
    if (program != 0)
    {
        Program *programObject = context->getProgram(program);
        if (!programObject)
        {
            // ES 3.1.0 section 7.3 page 72
            if (context->getShader(program))
            {
                context->handleError(
                    Error(GL_INVALID_OPERATION,
                          "Attempted to use a single shader instead of a shader program."));
                return false;
            }
            else
            {
                context->handleError(Error(GL_INVALID_VALUE, "Program invalid."));
                return false;
            }
        }
        if (!programObject->isLinked())
        {
            context->handleError(Error(GL_INVALID_OPERATION, "Program not linked."));
            return false;
        }
    }
    if (context->getGLState().isTransformFeedbackActiveUnpaused())
    {
        // ES 3.0.4 section 2.15 page 91
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "Cannot change active program while transform feedback is unpaused."));
        return false;
    }

    return true;
}

bool ValidateCopyTexImage2D(ValidationContext *context,
                            GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLint border)
{
    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2CopyTexImageParameters(context, target, level, internalformat, false, 0,
                                                 0, x, y, width, height, border);
    }

    ASSERT(context->getClientMajorVersion() == 3);
    return ValidateES3CopyTexImage2DParameters(context, target, level, internalformat, false, 0, 0,
                                               0, x, y, width, height, border);
}

bool ValidateFramebufferRenderbuffer(Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     GLenum renderbuffertarget,
                                     GLuint renderbuffer)
{
    if (!ValidFramebufferTarget(target) ||
        (renderbuffertarget != GL_RENDERBUFFER && renderbuffer != 0))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    return ValidateFramebufferRenderbufferParameters(context, target, attachment,
                                                     renderbuffertarget, renderbuffer);
}

bool ValidateDrawBuffersBase(ValidationContext *context, GLsizei n, const GLenum *bufs)
{
    // INVALID_VALUE is generated if n is negative or greater than value of MAX_DRAW_BUFFERS
    if (n < 0 || static_cast<GLuint>(n) > context->getCaps().maxDrawBuffers)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "n must be non-negative and no greater than MAX_DRAW_BUFFERS"));
        return false;
    }

    ASSERT(context->getGLState().getDrawFramebuffer());
    GLuint frameBufferId      = context->getGLState().getDrawFramebuffer()->id();
    GLuint maxColorAttachment = GL_COLOR_ATTACHMENT0_EXT + context->getCaps().maxColorAttachments;

    // This should come first before the check for the default frame buffer
    // because when we switch to ES3.1+, invalid enums will return INVALID_ENUM
    // rather than INVALID_OPERATION
    for (int colorAttachment = 0; colorAttachment < n; colorAttachment++)
    {
        const GLenum attachment = GL_COLOR_ATTACHMENT0_EXT + colorAttachment;

        if (bufs[colorAttachment] != GL_NONE && bufs[colorAttachment] != GL_BACK &&
            (bufs[colorAttachment] < GL_COLOR_ATTACHMENT0 ||
             bufs[colorAttachment] > GL_COLOR_ATTACHMENT31))
        {
            // Value in bufs is not NONE, BACK, or GL_COLOR_ATTACHMENTi
            // The 3.0.4 spec says to generate GL_INVALID_OPERATION here, but this
            // was changed to GL_INVALID_ENUM in 3.1, which dEQP also expects.
            // 3.1 is still a bit ambiguous about the error, but future specs are
            // expected to clarify that GL_INVALID_ENUM is the correct error.
            context->handleError(Error(GL_INVALID_ENUM, "Invalid buffer value"));
            return false;
        }
        else if (bufs[colorAttachment] >= maxColorAttachment)
        {
            context->handleError(
                Error(GL_INVALID_OPERATION, "Buffer value is greater than MAX_DRAW_BUFFERS"));
            return false;
        }
        else if (bufs[colorAttachment] != GL_NONE && bufs[colorAttachment] != attachment &&
                 frameBufferId != 0)
        {
            // INVALID_OPERATION-GL is bound to buffer and ith argument
            // is not COLOR_ATTACHMENTi or NONE
            context->handleError(
                Error(GL_INVALID_OPERATION, "Ith value does not match COLOR_ATTACHMENTi or NONE"));
            return false;
        }
    }

    // INVALID_OPERATION is generated if GL is bound to the default framebuffer
    // and n is not 1 or bufs is bound to value other than BACK and NONE
    if (frameBufferId == 0)
    {
        if (n != 1)
        {
            context->handleError(Error(GL_INVALID_OPERATION,
                                       "n must be 1 when GL is bound to the default framebuffer"));
            return false;
        }

        if (bufs[0] != GL_NONE && bufs[0] != GL_BACK)
        {
            context->handleError(Error(
                GL_INVALID_OPERATION,
                "Only NONE or BACK are valid values when drawing to the default framebuffer"));
            return false;
        }
    }

    return true;
}

bool ValidateCopyTexSubImage2D(Context *context,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height)
{
    if (context->getClientMajorVersion() < 3)
    {
        return ValidateES2CopyTexImageParameters(context, target, level, GL_NONE, true, xoffset,
                                                 yoffset, x, y, width, height, 0);
    }

    return ValidateES3CopyTexImage2DParameters(context, target, level, GL_NONE, true, xoffset,
                                               yoffset, 0, x, y, width, height, 0);
}

bool ValidateGetBufferPointervBase(Context *context,
                                   GLenum target,
                                   GLenum pname,
                                   GLsizei *length,
                                   void **params)
{
    if (length)
    {
        *length = 0;
    }

    if (context->getClientMajorVersion() < 3 && !context->getExtensions().mapBuffer)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION,
                  "Context does not support OpenGL ES 3.0 or GL_OES_mapbuffer is not enabled."));
        return false;
    }

    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Buffer target not valid: 0x%X", target));
        return false;
    }

    switch (pname)
    {
        case GL_BUFFER_MAP_POINTER:
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown pname."));
            return false;
    }

    // GLES 3.0 section 2.10.1: "Attempts to attempts to modify or query buffer object state for a
    // target bound to zero generate an INVALID_OPERATION error."
    // GLES 3.1 section 6.6 explicitly specifies this error.
    if (context->getGLState().getTargetBuffer(target) == nullptr)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Can not get pointer for reserved buffer name zero."));
        return false;
    }

    if (length)
    {
        *length = 1;
    }

    return true;
}

bool ValidateUnmapBufferBase(Context *context, GLenum target)
{
    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid buffer target."));
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (buffer == nullptr || !buffer->isMapped())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Buffer not mapped."));
        return false;
    }

    return true;
}

bool ValidateMapBufferRangeBase(Context *context,
                                GLenum target,
                                GLintptr offset,
                                GLsizeiptr length,
                                GLbitfield access)
{
    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid buffer target."));
        return false;
    }

    if (offset < 0 || length < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid offset or length."));
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (!buffer)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Attempted to map buffer object zero."));
        return false;
    }

    // Check for buffer overflow
    CheckedNumeric<size_t> checkedOffset(offset);
    auto checkedSize = checkedOffset + length;

    if (!checkedSize.IsValid() || checkedSize.ValueOrDie() > static_cast<size_t>(buffer->getSize()))
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Mapped range does not fit into buffer dimensions."));
        return false;
    }

    // Check for invalid bits in the mask
    GLbitfield allAccessBits = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT |
                               GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
                               GL_MAP_UNSYNCHRONIZED_BIT;

    if (access & ~(allAccessBits))
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid access bits: 0x%X.", access));
        return false;
    }

    if (length == 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Buffer mapping length is zero."));
        return false;
    }

    if (buffer->isMapped())
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Buffer is already mapped."));
        return false;
    }

    // Check for invalid bit combinations
    if ((access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == 0)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "Need to map buffer for either reading or writing."));
        return false;
    }

    GLbitfield writeOnlyBits =
        GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT;

    if ((access & GL_MAP_READ_BIT) != 0 && (access & writeOnlyBits) != 0)
    {
        context->handleError(Error(GL_INVALID_OPERATION,
                                   "Invalid access bits when mapping buffer for reading: 0x%X.",
                                   access));
        return false;
    }

    if ((access & GL_MAP_WRITE_BIT) == 0 && (access & GL_MAP_FLUSH_EXPLICIT_BIT) != 0)
    {
        context->handleError(Error(
            GL_INVALID_OPERATION,
            "The explicit flushing bit may only be set if the buffer is mapped for writing."));
        return false;
    }
    return true;
}

bool ValidateFlushMappedBufferRangeBase(Context *context,
                                        GLenum target,
                                        GLintptr offset,
                                        GLsizeiptr length)
{
    if (offset < 0 || length < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "Invalid offset/length parameters."));
        return false;
    }

    if (!ValidBufferTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid buffer target."));
        return false;
    }

    Buffer *buffer = context->getGLState().getTargetBuffer(target);

    if (buffer == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION, "Attempted to flush buffer object zero."));
        return false;
    }

    if (!buffer->isMapped() || (buffer->getAccessFlags() & GL_MAP_FLUSH_EXPLICIT_BIT) == 0)
    {
        context->handleError(Error(
            GL_INVALID_OPERATION, "Attempted to flush a buffer not mapped for explicit flushing."));
        return false;
    }

    // Check for buffer overflow
    CheckedNumeric<size_t> checkedOffset(offset);
    auto checkedSize = checkedOffset + length;

    if (!checkedSize.IsValid() ||
        checkedSize.ValueOrDie() > static_cast<size_t>(buffer->getMapLength()))
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "Flushed range does not fit into buffer mapping dimensions."));
        return false;
    }

    return true;
}

bool ValidateGenerateMipmap(Context *context, GLenum target)
{
    if (!ValidTextureTarget(context, target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    Texture *texture = context->getTargetTexture(target);

    if (texture == nullptr)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    const GLuint effectiveBaseLevel = texture->getTextureState().getEffectiveBaseLevel();

    // This error isn't spelled out in the spec in a very explicit way, but we interpret the spec so
    // that out-of-range base level has a non-color-renderable / non-texture-filterable format.
    if (effectiveBaseLevel >= gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    GLenum baseTarget  = (target == GL_TEXTURE_CUBE_MAP) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : target;
    const auto &format = texture->getFormat(baseTarget, effectiveBaseLevel);
    const TextureCaps &formatCaps = context->getTextureCaps().get(format.asSized());

    // GenerateMipmap should not generate an INVALID_OPERATION for textures created with
    // unsized formats or that are color renderable and filterable.  Since we do not track if
    // the texture was created with sized or unsized format (only sized formats are stored),
    // it is not possible to make sure the the LUMA formats can generate mipmaps (they should
    // be able to) because they aren't color renderable.  Simply do a special case for LUMA
    // textures since they're the only texture format that can be created with unsized formats
    // that is not color renderable.  New unsized formats are unlikely to be added, since ES2
    // was the last version to use add them.
    if (format.info->depthBits > 0 || format.info->stencilBits > 0 || !formatCaps.filterable ||
        (!formatCaps.renderable && !format.info->isLUMA()) || format.info->compressed)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // GL_EXT_sRGB does not support mipmap generation on sRGB textures
    if (context->getClientMajorVersion() == 2 && format.info->colorEncoding == GL_SRGB)
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // Non-power of 2 ES2 check
    if (context->getClientVersion() < Version(3, 0) && !context->getExtensions().textureNPOT &&
        (!isPow2(static_cast<int>(texture->getWidth(baseTarget, 0))) ||
         !isPow2(static_cast<int>(texture->getHeight(baseTarget, 0)))))
    {
        ASSERT(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP);
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    // Cube completeness check
    if (target == GL_TEXTURE_CUBE_MAP && !texture->getTextureState().isCubeComplete())
    {
        context->handleError(Error(GL_INVALID_OPERATION));
        return false;
    }

    return true;
}

bool ValidateGenBuffers(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteBuffers(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenFramebuffers(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteFramebuffers(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenRenderbuffers(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteRenderbuffers(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenTextures(Context *context, GLint n, GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateDeleteTextures(Context *context, GLint n, const GLuint *)
{
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenOrDelete(Context *context, GLint n)
{
    if (n < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "n < 0"));
        return false;
    }
    return true;
}

bool ValidateEnable(Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, false))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid cap."));
        return false;
    }

    if (context->getLimitations().noSampleAlphaToCoverageSupport &&
        cap == GL_SAMPLE_ALPHA_TO_COVERAGE)
    {
        const char *errorMessage = "Current renderer doesn't support alpha-to-coverage";
        context->handleError(Error(GL_INVALID_OPERATION, errorMessage));

        // We also output an error message to the debugger window if tracing is active, so that
        // developers can see the error message.
        ERR() << errorMessage;
        return false;
    }

    return true;
}

bool ValidateDisable(Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, false))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid cap."));
        return false;
    }

    return true;
}

bool ValidateIsEnabled(Context *context, GLenum cap)
{
    if (!ValidCap(context, cap, true))
    {
        context->handleError(Error(GL_INVALID_ENUM, "Invalid cap."));
        return false;
    }

    return true;
}

bool ValidateRobustEntryPoint(ValidationContext *context, GLsizei bufSize)
{
    if (!context->getExtensions().robustClientMemory)
    {
        context->handleError(
            Error(GL_INVALID_OPERATION, "GL_ANGLE_robust_client_memory is not available."));
        return false;
    }

    if (bufSize < 0)
    {
        context->handleError(Error(GL_INVALID_VALUE, "bufSize cannot be negative."));
        return false;
    }

    return true;
}

bool ValidateRobustBufferSize(ValidationContext *context, GLsizei bufSize, GLsizei numParams)
{
    if (bufSize < numParams)
    {
        context->handleError(Error(GL_INVALID_OPERATION,
                                   "%u parameters are required but %i were provided.", numParams,
                                   bufSize));
        return false;
    }

    return true;
}

bool ValidateGetFramebufferAttachmentParameteriv(ValidationContext *context,
                                                 GLenum target,
                                                 GLenum attachment,
                                                 GLenum pname,
                                                 GLsizei *numParams)
{
    // Only one parameter is returned from glGetFramebufferAttachmentParameteriv
    *numParams = 1;

    if (!ValidFramebufferTarget(target))
    {
        context->handleError(Error(GL_INVALID_ENUM));
        return false;
    }

    int clientVersion = context->getClientMajorVersion();

    switch (pname)
    {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
            if (clientVersion < 3 && !context->getExtensions().sRGB)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
            if (clientVersion < 3)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM));
            return false;
    }

    // Determine if the attachment is a valid enum
    switch (attachment)
    {
        case GL_BACK:
        case GL_FRONT:
        case GL_DEPTH:
        case GL_STENCIL:
        case GL_DEPTH_STENCIL_ATTACHMENT:
            if (clientVersion < 3)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;

        case GL_DEPTH_ATTACHMENT:
        case GL_STENCIL_ATTACHMENT:
            break;

        default:
            if (attachment < GL_COLOR_ATTACHMENT0_EXT ||
                (attachment - GL_COLOR_ATTACHMENT0_EXT) >= context->getCaps().maxColorAttachments)
            {
                context->handleError(Error(GL_INVALID_ENUM));
                return false;
            }
            break;
    }

    const Framebuffer *framebuffer = context->getGLState().getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->id() == 0)
    {
        if (clientVersion < 3)
        {
            context->handleError(Error(GL_INVALID_OPERATION));
            return false;
        }

        switch (attachment)
        {
            case GL_BACK:
            case GL_DEPTH:
            case GL_STENCIL:
                break;

            default:
                context->handleError(Error(GL_INVALID_OPERATION));
                return false;
        }
    }
    else
    {
        if (attachment >= GL_COLOR_ATTACHMENT0_EXT && attachment <= GL_COLOR_ATTACHMENT15_EXT)
        {
            // Valid attachment query
        }
        else
        {
            switch (attachment)
            {
                case GL_DEPTH_ATTACHMENT:
                case GL_STENCIL_ATTACHMENT:
                    break;

                case GL_DEPTH_STENCIL_ATTACHMENT:
                    if (!framebuffer->hasValidDepthStencil())
                    {
                        context->handleError(Error(GL_INVALID_OPERATION));
                        return false;
                    }
                    break;

                default:
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
            }
        }
    }

    const FramebufferAttachment *attachmentObject = framebuffer->getAttachment(attachment);
    if (attachmentObject)
    {
        ASSERT(attachmentObject->type() == GL_RENDERBUFFER ||
               attachmentObject->type() == GL_TEXTURE ||
               attachmentObject->type() == GL_FRAMEBUFFER_DEFAULT);

        switch (pname)
        {
            case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                if (attachmentObject->type() != GL_RENDERBUFFER &&
                    attachmentObject->type() != GL_TEXTURE)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;

            case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
                if (attachmentObject->type() != GL_TEXTURE)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;

            case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
                if (attachmentObject->type() != GL_TEXTURE)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;

            case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
                if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
                break;

            case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
                if (attachmentObject->type() != GL_TEXTURE)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;

            default:
                break;
        }
    }
    else
    {
        // ES 2.0.25 spec pg 127 states that if the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE
        // is NONE, then querying any other pname will generate INVALID_ENUM.

        // ES 3.0.2 spec pg 235 states that if the attachment type is none,
        // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME will return zero and be an
        // INVALID_OPERATION for all other pnames

        switch (pname)
        {
            case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                break;

            case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                if (clientVersion < 3)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                break;

            default:
                if (clientVersion < 3)
                {
                    context->handleError(Error(GL_INVALID_ENUM));
                    return false;
                }
                else
                {
                    context->handleError(Error(GL_INVALID_OPERATION));
                    return false;
                }
        }
    }

    return true;
}

bool ValidateGetFramebufferAttachmentParameterivRobustANGLE(ValidationContext *context,
                                                            GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *numParams)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetFramebufferAttachmentParameteriv(context, target, attachment, pname, numParams))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *numParams))
    {
        return false;
    }

    return true;
}

bool ValidateGetBufferParameteriv(ValidationContext *context,
                                  GLenum target,
                                  GLenum pname,
                                  GLint *params)
{
    return ValidateGetBufferParameterBase(context, target, pname, false, nullptr);
}

bool ValidateGetBufferParameterivRobustANGLE(ValidationContext *context,
                                             GLenum target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetBufferParameterBase(context, target, pname, false, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetBufferParameteri64v(ValidationContext *context,
                                    GLenum target,
                                    GLenum pname,
                                    GLint64 *params)
{
    return ValidateGetBufferParameterBase(context, target, pname, false, nullptr);
}

bool ValidateGetBufferParameteri64vRobustANGLE(ValidationContext *context,
                                               GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint64 *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetBufferParameterBase(context, target, pname, false, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetProgramiv(Context *context, GLuint program, GLenum pname, GLsizei *numParams)
{
    // Currently, all GetProgramiv queries return 1 parameter
    *numParams = 1;

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    switch (pname)
    {
        case GL_DELETE_STATUS:
        case GL_LINK_STATUS:
        case GL_VALIDATE_STATUS:
        case GL_INFO_LOG_LENGTH:
        case GL_ATTACHED_SHADERS:
        case GL_ACTIVE_ATTRIBUTES:
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
        case GL_ACTIVE_UNIFORMS:
        case GL_ACTIVE_UNIFORM_MAX_LENGTH:
            break;

        case GL_PROGRAM_BINARY_LENGTH:
            if (context->getClientMajorVersion() < 3 && !context->getExtensions().getProgramBinary)
            {
                context->handleError(Error(GL_INVALID_ENUM,
                                           "Querying GL_PROGRAM_BINARY_LENGTH requires "
                                           "GL_OES_get_program_binary or ES 3.0."));
                return false;
            }
            break;

        case GL_ACTIVE_UNIFORM_BLOCKS:
        case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
        case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
        case GL_TRANSFORM_FEEDBACK_VARYINGS:
        case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH:
        case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:
            if (context->getClientMajorVersion() < 3)
            {
                context->handleError(Error(GL_INVALID_ENUM, "Querying requires at least ES 3.0."));
                return false;
            }
            break;

        case GL_PROGRAM_SEPARABLE:
            if (context->getClientVersion() < Version(3, 1))
            {
                context->handleError(Error(GL_INVALID_ENUM, "Querying requires at least ES 3.1."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Unknown parameter name."));
            return false;
    }

    return true;
}

bool ValidateGetProgramivRobustANGLE(Context *context,
                                     GLuint program,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *numParams)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetProgramiv(context, program, pname, numParams))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *numParams))
    {
        return false;
    }

    return true;
}

bool ValidateGetRenderbufferParameteriv(Context *context,
                                        GLenum target,
                                        GLenum pname,
                                        GLint *params)
{
    return ValidateGetRenderbufferParameterivBase(context, target, pname, nullptr);
}

bool ValidateGetRenderbufferParameterivRobustANGLE(Context *context,
                                                   GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetRenderbufferParameterivBase(context, target, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetShaderiv(Context *context, GLuint shader, GLenum pname, GLint *params)
{
    return ValidateGetShaderivBase(context, shader, pname, nullptr);
}

bool ValidateGetShaderivRobustANGLE(Context *context,
                                    GLuint shader,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetShaderivBase(context, shader, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetTexParameterfv(Context *context, GLenum target, GLenum pname, GLfloat *params)
{
    return ValidateGetTexParameterBase(context, target, pname, nullptr);
}

bool ValidateGetTexParameterfvRobustANGLE(Context *context,
                                          GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetTexParameterBase(context, target, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetTexParameteriv(Context *context, GLenum target, GLenum pname, GLint *params)
{
    return ValidateGetTexParameterBase(context, target, pname, nullptr);
}

bool ValidateGetTexParameterivRobustANGLE(Context *context,
                                          GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetTexParameterBase(context, target, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateTexParameterf(Context *context, GLenum target, GLenum pname, GLfloat param)
{
    return ValidateTexParameterBase(context, target, pname, -1, &param);
}

bool ValidateTexParameterfv(Context *context, GLenum target, GLenum pname, const GLfloat *params)
{
    return ValidateTexParameterBase(context, target, pname, -1, params);
}

bool ValidateTexParameterfvRobustANGLE(Context *context,
                                       GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLfloat *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    return ValidateTexParameterBase(context, target, pname, bufSize, params);
}

bool ValidateTexParameteri(Context *context, GLenum target, GLenum pname, GLint param)
{
    return ValidateTexParameterBase(context, target, pname, -1, &param);
}

bool ValidateTexParameteriv(Context *context, GLenum target, GLenum pname, const GLint *params)
{
    return ValidateTexParameterBase(context, target, pname, -1, params);
}

bool ValidateTexParameterivRobustANGLE(Context *context,
                                       GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    return ValidateTexParameterBase(context, target, pname, bufSize, params);
}

bool ValidateGetSamplerParameterfv(Context *context, GLuint sampler, GLenum pname, GLfloat *params)
{
    return ValidateGetSamplerParameterBase(context, sampler, pname, nullptr);
}

bool ValidateGetSamplerParameterfvRobustANGLE(Context *context,
                                              GLuint sampler,
                                              GLenum pname,
                                              GLuint bufSize,
                                              GLsizei *length,
                                              GLfloat *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetSamplerParameterBase(context, sampler, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetSamplerParameteriv(Context *context, GLuint sampler, GLenum pname, GLint *params)
{
    return ValidateGetSamplerParameterBase(context, sampler, pname, nullptr);
}

bool ValidateGetSamplerParameterivRobustANGLE(Context *context,
                                              GLuint sampler,
                                              GLenum pname,
                                              GLuint bufSize,
                                              GLsizei *length,
                                              GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetSamplerParameterBase(context, sampler, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateSamplerParameterf(Context *context, GLuint sampler, GLenum pname, GLfloat param)
{
    return ValidateSamplerParameterBase(context, sampler, pname, -1, &param);
}

bool ValidateSamplerParameterfv(Context *context,
                                GLuint sampler,
                                GLenum pname,
                                const GLfloat *params)
{
    return ValidateSamplerParameterBase(context, sampler, pname, -1, params);
}

bool ValidateSamplerParameterfvRobustANGLE(Context *context,
                                           GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLfloat *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    return ValidateSamplerParameterBase(context, sampler, pname, bufSize, params);
}

bool ValidateSamplerParameteri(Context *context, GLuint sampler, GLenum pname, GLint param)
{
    return ValidateSamplerParameterBase(context, sampler, pname, -1, &param);
}

bool ValidateSamplerParameteriv(Context *context, GLuint sampler, GLenum pname, const GLint *params)
{
    return ValidateSamplerParameterBase(context, sampler, pname, -1, params);
}

bool ValidateSamplerParameterivRobustANGLE(Context *context,
                                           GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    return ValidateSamplerParameterBase(context, sampler, pname, bufSize, params);
}

bool ValidateGetVertexAttribfv(Context *context, GLuint index, GLenum pname, GLfloat *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, false);
}

bool ValidateGetVertexAttribfvRobustANGLE(Context *context,
                                          GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetVertexAttribBase(context, index, pname, length, false, false))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetVertexAttribiv(Context *context, GLuint index, GLenum pname, GLint *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, false);
}

bool ValidateGetVertexAttribivRobustANGLE(Context *context,
                                          GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetVertexAttribBase(context, index, pname, length, false, false))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetVertexAttribPointerv(Context *context, GLuint index, GLenum pname, void **pointer)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, true, false);
}

bool ValidateGetVertexAttribPointervRobustANGLE(Context *context,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                void **pointer)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetVertexAttribBase(context, index, pname, length, true, false))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetVertexAttribIiv(Context *context, GLuint index, GLenum pname, GLint *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, true);
}

bool ValidateGetVertexAttribIivRobustANGLE(Context *context,
                                           GLuint index,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetVertexAttribBase(context, index, pname, length, false, true))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetVertexAttribIuiv(Context *context, GLuint index, GLenum pname, GLuint *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, true);
}

bool ValidateGetVertexAttribIuivRobustANGLE(Context *context,
                                            GLuint index,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetVertexAttribBase(context, index, pname, length, false, true))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetActiveUniformBlockiv(Context *context,
                                     GLuint program,
                                     GLuint uniformBlockIndex,
                                     GLenum pname,
                                     GLint *params)
{
    return ValidateGetActiveUniformBlockivBase(context, program, uniformBlockIndex, pname, nullptr);
}

bool ValidateGetActiveUniformBlockivRobustANGLE(Context *context,
                                                GLuint program,
                                                GLuint uniformBlockIndex,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetActiveUniformBlockivBase(context, program, uniformBlockIndex, pname, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateGetInternalFormativ(Context *context,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLint *params)
{
    return ValidateGetInternalFormativBase(context, target, internalformat, pname, bufSize,
                                           nullptr);
}

bool ValidateGetInternalFormativRobustANGLE(Context *context,
                                            GLenum target,
                                            GLenum internalformat,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    if (!ValidateGetInternalFormativBase(context, target, internalformat, pname, bufSize, length))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, *length))
    {
        return false;
    }

    return true;
}

bool ValidateVertexFormatBase(ValidationContext *context,
                              GLuint attribIndex,
                              GLint size,
                              GLenum type,
                              GLboolean pureInteger)
{
    const Caps &caps = context->getCaps();
    if (attribIndex >= caps.maxVertexAttributes)
    {
        context->handleError(
            Error(GL_INVALID_VALUE, "attribindex must be smaller than MAX_VERTEX_ATTRIBS."));
        return false;
    }

    if (size < 1 || size > 4)
    {
        context->handleError(Error(GL_INVALID_VALUE, "size must be between one and four."));
    }

    switch (type)
    {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            break;

        case GL_INT:
        case GL_UNSIGNED_INT:
            if (context->getClientMajorVersion() < 3)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "Vertex type not supported before OpenGL ES 3.0."));
                return false;
            }
            break;

        case GL_FIXED:
        case GL_FLOAT:
            if (pureInteger)
            {
                context->handleError(Error(GL_INVALID_ENUM, "Type is not integer."));
                return false;
            }
            break;

        case GL_HALF_FLOAT:
            if (context->getClientMajorVersion() < 3)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "Vertex type not supported before OpenGL ES 3.0."));
                return false;
            }
            if (pureInteger)
            {
                context->handleError(Error(GL_INVALID_ENUM, "Type is not integer."));
                return false;
            }
            break;

        case GL_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            if (context->getClientMajorVersion() < 3)
            {
                context->handleError(
                    Error(GL_INVALID_ENUM, "Vertex type not supported before OpenGL ES 3.0."));
                return false;
            }
            if (pureInteger)
            {
                context->handleError(Error(GL_INVALID_ENUM, "Type is not integer."));
                return false;
            }
            if (size != 4)
            {
                context->handleError(Error(GL_INVALID_OPERATION,
                                           "Type is INT_2_10_10_10_REV or "
                                           "UNSIGNED_INT_2_10_10_10_REV and size is not 4."));
                return false;
            }
            break;

        default:
            context->handleError(Error(GL_INVALID_ENUM, "Invalid vertex type."));
            return false;
    }

    return true;
}

// Perform validation from WebGL 2 section 5.10 "Invalid Clears":
// In the WebGL 2 API, trying to perform a clear when there is a mismatch between the type of the
// specified clear value and the type of a buffer that is being cleared generates an
// INVALID_OPERATION error instead of producing undefined results
bool ValidateWebGLFramebufferAttachmentClearType(ValidationContext *context,
                                                 GLint drawbuffer,
                                                 const GLenum *validComponentTypes,
                                                 size_t validComponentTypeCount)
{
    const FramebufferAttachment *attachment =
        context->getGLState().getDrawFramebuffer()->getDrawBuffer(drawbuffer);
    if (attachment)
    {
        GLenum componentType = attachment->getFormat().info->componentType;
        const GLenum *end    = validComponentTypes + validComponentTypeCount;
        if (std::find(validComponentTypes, end, componentType) == end)
        {
            context->handleError(
                Error(GL_INVALID_OPERATION,
                      "No defined conversion between clear value and attachment format."));
            return false;
        }
    }

    return true;
}

}  // namespace gl
