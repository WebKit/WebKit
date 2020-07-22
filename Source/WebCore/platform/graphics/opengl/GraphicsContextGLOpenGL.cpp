/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GraphicsContextGLOpenGL.h"

#if ENABLE(GRAPHICS_CONTEXT_GL)

#include "ExtensionsGL.h"
#include "FormatConverter.h"
#include "Image.h"
#include "ImageData.h"
#include "ImageObserver.h"
#include <wtf/UniqueArray.h>

namespace WebCore {

namespace {

GraphicsContextGL::DataFormat getDataFormat(GCGLenum destinationFormat, GCGLenum destinationType)
{
    GraphicsContextGL::DataFormat dstFormat = GraphicsContextGL::DataFormat::RGBA8;
    switch (destinationType) {
    case GraphicsContextGL::BYTE:
        switch (destinationFormat) {
        case GraphicsContextGL::RED:
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R8_S;
            break;
        case GraphicsContextGL::RG:
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG8_S;
            break;
        case GraphicsContextGL::RGB:
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB8_S;
            break;
        case GraphicsContextGL::RGBA:
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA8_S;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::UNSIGNED_BYTE:
        switch (destinationFormat) {
        case GraphicsContextGL::RGB:
        case GraphicsContextGL::RGB_INTEGER:
        case GraphicsContextGL::SRGB:
            dstFormat = GraphicsContextGL::GraphicsContextGL::DataFormat::RGB8;
            break;
        case GraphicsContextGL::RGBA:
        case GraphicsContextGL::RGBA_INTEGER:
        case GraphicsContextGL::SRGB_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA8;
            break;
        case GraphicsContextGL::ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::A8;
            break;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R8;
            break;
        case GraphicsContextGL::RG:
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG8;
            break;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RA8;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::SHORT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R16_S;
            break;
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG16_S;
            break;
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB16_S;
            break;
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA16_S;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::UNSIGNED_SHORT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R16;
            break;
        case GraphicsContextGL::DEPTH_COMPONENT:
            dstFormat = GraphicsContextGL::DataFormat::D16;
            break;
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG16;
            break;
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB16;
            break;
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA16;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::INT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R32_S;
            break;
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG32_S;
            break;
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB32_S;
            break;
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA32_S;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::UNSIGNED_INT:
        switch (destinationFormat) {
        case GraphicsContextGL::RED_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::R32;
            break;
        case GraphicsContextGL::DEPTH_COMPONENT:
            dstFormat = GraphicsContextGL::DataFormat::D32;
            break;
        case GraphicsContextGL::RG_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RG32;
            break;
        case GraphicsContextGL::RGB_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGB32;
            break;
        case GraphicsContextGL::RGBA_INTEGER:
            dstFormat = GraphicsContextGL::DataFormat::RGBA32;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
    case GraphicsContextGL::HALF_FLOAT:
        switch (destinationFormat) {
        case GraphicsContextGL::RGBA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA16F;
            break;
        case GraphicsContextGL::RGB:
            dstFormat = GraphicsContextGL::DataFormat::RGB16F;
            break;
        case GraphicsContextGL::RG:
            dstFormat = GraphicsContextGL::DataFormat::RG16F;
            break;
        case GraphicsContextGL::ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::A16F;
            break;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
            dstFormat = GraphicsContextGL::DataFormat::R16F;
            break;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RA16F;
            break;
        case GraphicsContextGL::SRGB:
            dstFormat = GraphicsContextGL::DataFormat::RGB16F;
            break;
        case GraphicsContextGL::SRGB_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA16F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::FLOAT: // OES_texture_float
        switch (destinationFormat) {
        case GraphicsContextGL::RGBA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA32F;
            break;
        case GraphicsContextGL::RGB:
            dstFormat = GraphicsContextGL::DataFormat::RGB32F;
            break;
        case GraphicsContextGL::RG:
            dstFormat = GraphicsContextGL::DataFormat::RG32F;
            break;
        case GraphicsContextGL::ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::A32F;
            break;
        case GraphicsContextGL::LUMINANCE:
        case GraphicsContextGL::RED:
            dstFormat = GraphicsContextGL::DataFormat::R32F;
            break;
        case GraphicsContextGL::DEPTH_COMPONENT:
            dstFormat = GraphicsContextGL::DataFormat::D32F;
            break;
        case GraphicsContextGL::LUMINANCE_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RA32F;
            break;
        case GraphicsContextGL::SRGB:
            dstFormat = GraphicsContextGL::DataFormat::RGB32F;
            break;
        case GraphicsContextGL::SRGB_ALPHA:
            dstFormat = GraphicsContextGL::DataFormat::RGBA32F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
        dstFormat = GraphicsContextGL::DataFormat::RGBA4444;
        break;
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        dstFormat = GraphicsContextGL::DataFormat::RGBA5551;
        break;
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
        dstFormat = GraphicsContextGL::DataFormat::RGB565;
        break;
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
        dstFormat = GraphicsContextGL::DataFormat::RGB5999;
        break;
    case GraphicsContextGL::UNSIGNED_INT_24_8:
        dstFormat = GraphicsContextGL::DataFormat::DS24_8;
        break;
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
        dstFormat = GraphicsContextGL::DataFormat::RGB10F11F11F;
        break;
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
        dstFormat = GraphicsContextGL::DataFormat::RGBA2_10_10_10;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return dstFormat;
}

} // anonymous namespace

void GraphicsContextGLOpenGL::resetBuffersToAutoClear()
{
    GCGLuint buffers = GraphicsContextGL::COLOR_BUFFER_BIT;
    // The GraphicsContextGL's attributes (as opposed to
    // WebGLRenderingContext's) indicate whether there is an
    // implicitly-allocated stencil buffer, for example.
    auto attrs = contextAttributes();
    if (attrs.depth)
        buffers |= GraphicsContextGL::DEPTH_BUFFER_BIT;
    if (attrs.stencil)
        buffers |= GraphicsContextGL::STENCIL_BUFFER_BIT;
    setBuffersToAutoClear(buffers);
}

void GraphicsContextGLOpenGL::setBuffersToAutoClear(GCGLbitfield buffers)
{
    auto attrs = contextAttributes();
    if (!attrs.preserveDrawingBuffer)
        m_buffersToAutoClear = buffers;
    else
        ASSERT(!m_buffersToAutoClear);
}

GCGLbitfield GraphicsContextGLOpenGL::getBuffersToAutoClear() const
{
    return m_buffersToAutoClear;
}

bool GraphicsContextGLOpenGL::texImage2DResourceSafe(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLint unpackAlignment)
{
    ASSERT(unpackAlignment == 1 || unpackAlignment == 2 || unpackAlignment == 4 || unpackAlignment == 8);
    UniqueArray<unsigned char> zero;
    if (width > 0 && height > 0) {
        unsigned size;
        GraphicsContextGLOpenGL::PixelStoreParams params;
        params.alignment = unpackAlignment;
        GCGLenum error = computeImageSizeInBytes(format, type, width, height, 1, params, &size, nullptr, nullptr);
        if (error != GraphicsContextGL::NO_ERROR) {
            synthesizeGLError(error);
            return false;
        }
        zero = makeUniqueArray<unsigned char>(size);
        if (!zero) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE);
            return false;
        }
        memset(zero.get(), 0, size);
    }
    return texImage2D(target, level, internalformat, width, height, border, format, type, zero.get());
}

bool GraphicsContextGLOpenGL::computeFormatAndTypeParameters(GCGLenum format, GCGLenum type, unsigned* componentsPerPixel, unsigned* bytesPerComponent)
{
    switch (format) {
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::RED:
    case GraphicsContextGL::RED_INTEGER:
    case GraphicsContextGL::DEPTH_COMPONENT:
    case GraphicsContextGL::DEPTH_STENCIL: // Treat it as one component.
        *componentsPerPixel = 1;
        break;
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::RG:
    case GraphicsContextGL::RG_INTEGER:
        *componentsPerPixel = 2;
        break;
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGB_INTEGER:
    case ExtensionsGL::SRGB_EXT:
        *componentsPerPixel = 3;
        break;
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::RGBA_INTEGER:
    case ExtensionsGL::BGRA_EXT: // GL_EXT_texture_format_BGRA8888
    case ExtensionsGL::SRGB_ALPHA_EXT:
        *componentsPerPixel = 4;
        break;
    default:
        return false;
    }

    switch (type) {
    case GraphicsContextGL::BYTE:
        *bytesPerComponent = sizeof(GCGLbyte);
        break;
    case GraphicsContextGL::UNSIGNED_BYTE:
        *bytesPerComponent = sizeof(GCGLubyte);
        break;
    case GraphicsContextGL::SHORT:
        *bytesPerComponent = sizeof(GCGLshort);
        break;
    case GraphicsContextGL::UNSIGNED_SHORT:
        *bytesPerComponent = sizeof(GCGLushort);
        break;
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GCGLushort);
        break;
    case GraphicsContextGL::INT:
        *bytesPerComponent = sizeof(GCGLint);
        break;
    case GraphicsContextGL::UNSIGNED_INT:
        *bytesPerComponent = sizeof(GCGLuint);
        break;
    case GraphicsContextGL::UNSIGNED_INT_24_8:
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
        *componentsPerPixel = 1;
        *bytesPerComponent = sizeof(GCGLuint);
        break;
    case GraphicsContextGL::FLOAT: // OES_texture_float
        *bytesPerComponent = sizeof(GCGLfloat);
        break;
    case GraphicsContextGL::HALF_FLOAT:
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
        *bytesPerComponent = sizeof(GCGLhalffloat);
        break;
    case GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV:
        *bytesPerComponent = sizeof(GCGLfloat) + sizeof(GCGLuint);
        break;
    default:
        return false;
    }
    return true;
}

#if !USE(ANGLE)
bool GraphicsContextGLOpenGL::possibleFormatAndTypeForInternalFormat(GCGLenum internalFormat, GCGLenum& format, GCGLenum& type)
{
#define POSSIBLE_FORMAT_TYPE_CASE(internalFormatMacro, formatMacro, typeMacro) case internalFormatMacro: \
        format = formatMacro; \
        type = GraphicsContextGLOpenGL::typeMacro; \
        break;

    switch (internalFormat) {
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB               , GraphicsContextGL::RGB            , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA              , GraphicsContextGL::RGBA           , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::LUMINANCE_ALPHA   , GraphicsContextGL::LUMINANCE_ALPHA, UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::LUMINANCE         , GraphicsContextGL::LUMINANCE      , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::ALPHA             , GraphicsContextGL::ALPHA          , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R8                , GraphicsContextGL::RED            , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R8_SNORM          , GraphicsContextGL::RED            , BYTE                          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R16F              , GraphicsContextGL::RED            , HALF_FLOAT                    );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R32F              , GraphicsContextGL::RED            , FLOAT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R8UI              , GraphicsContextGL::RED_INTEGER    , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R8I               , GraphicsContextGL::RED_INTEGER    , BYTE                          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R16UI             , GraphicsContextGL::RED_INTEGER    , UNSIGNED_SHORT                );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R16I              , GraphicsContextGL::RED_INTEGER    , SHORT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R32UI             , GraphicsContextGL::RED_INTEGER    , UNSIGNED_INT                  );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R32I              , GraphicsContextGL::RED_INTEGER    , INT                           );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG8               , GraphicsContextGL::RG             , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG8_SNORM         , GraphicsContextGL::RG             , BYTE                          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG16F             , GraphicsContextGL::RG             , HALF_FLOAT                    );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG32F             , GraphicsContextGL::RG             , FLOAT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG8UI             , GraphicsContextGL::RG_INTEGER     , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG8I              , GraphicsContextGL::RG_INTEGER     , BYTE                          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG16UI            , GraphicsContextGL::RG_INTEGER     , UNSIGNED_SHORT                );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG16I             , GraphicsContextGL::RG_INTEGER     , SHORT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG32UI            , GraphicsContextGL::RG_INTEGER     , UNSIGNED_INT                  );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RG32I             , GraphicsContextGL::RG_INTEGER     , INT                           );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB8              , GraphicsContextGL::RGB            , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::SRGB8             , GraphicsContextGL::RGB            , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB565            , GraphicsContextGL::RGB            , UNSIGNED_SHORT_5_6_5          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB8_SNORM        , GraphicsContextGL::RGB            , BYTE                          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::R11F_G11F_B10F    , GraphicsContextGL::RGB            , UNSIGNED_INT_10F_11F_11F_REV  );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB9_E5           , GraphicsContextGL::RGB            , UNSIGNED_INT_5_9_9_9_REV      );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB16F            , GraphicsContextGL::RGB            , HALF_FLOAT                    );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB32F            , GraphicsContextGL::RGB            , FLOAT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB8UI            , GraphicsContextGL::RGB_INTEGER    , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB8I             , GraphicsContextGL::RGB_INTEGER    , BYTE                          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB16UI           , GraphicsContextGL::RGB_INTEGER    , UNSIGNED_SHORT                );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB16I            , GraphicsContextGL::RGB_INTEGER    , SHORT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB32UI           , GraphicsContextGL::RGB_INTEGER    , UNSIGNED_INT                  );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB32I            , GraphicsContextGL::RGB_INTEGER    , INT                           );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA8             , GraphicsContextGL::RGBA           , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::SRGB8_ALPHA8      , GraphicsContextGL::RGBA           , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA8_SNORM       , GraphicsContextGL::RGBA           , BYTE                          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB5_A1           , GraphicsContextGL::RGBA           , UNSIGNED_SHORT_5_5_5_1        );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA4             , GraphicsContextGL::RGBA           , UNSIGNED_SHORT_4_4_4_4        );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB10_A2          , GraphicsContextGL::RGBA           , UNSIGNED_INT_2_10_10_10_REV   );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA16F           , GraphicsContextGL::RGBA           , HALF_FLOAT                    );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA32F           , GraphicsContextGL::RGBA           , FLOAT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA8UI           , GraphicsContextGL::RGBA_INTEGER   , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA8I            , GraphicsContextGL::RGBA_INTEGER   , BYTE                          );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGB10_A2UI        , GraphicsContextGL::RGBA_INTEGER   , UNSIGNED_INT_2_10_10_10_REV   );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA16UI          , GraphicsContextGL::RGBA_INTEGER   , UNSIGNED_SHORT                );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA16I           , GraphicsContextGL::RGBA_INTEGER   , SHORT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA32I           , GraphicsContextGL::RGBA_INTEGER   , INT                           );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::RGBA32UI          , GraphicsContextGL::RGBA_INTEGER   , UNSIGNED_INT                  );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::DEPTH_COMPONENT16 , GraphicsContextGL::DEPTH_COMPONENT, UNSIGNED_SHORT                );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::DEPTH_COMPONENT   , GraphicsContextGL::DEPTH_COMPONENT, UNSIGNED_SHORT                );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::DEPTH_COMPONENT24 , GraphicsContextGL::DEPTH_COMPONENT, UNSIGNED_INT                  );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::DEPTH_COMPONENT32F, GraphicsContextGL::DEPTH_COMPONENT, FLOAT                         );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::DEPTH_STENCIL     , GraphicsContextGL::DEPTH_STENCIL  , UNSIGNED_INT_24_8             );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::DEPTH24_STENCIL8  , GraphicsContextGL::DEPTH_STENCIL  , UNSIGNED_INT_24_8             );
    POSSIBLE_FORMAT_TYPE_CASE(GraphicsContextGL::DEPTH32F_STENCIL8 , GraphicsContextGL::DEPTH_STENCIL  , FLOAT_32_UNSIGNED_INT_24_8_REV);
    POSSIBLE_FORMAT_TYPE_CASE(ExtensionsGL::SRGB_EXT               , ExtensionsGL::SRGB_EXT            , UNSIGNED_BYTE                 );
    POSSIBLE_FORMAT_TYPE_CASE(ExtensionsGL::SRGB_ALPHA_EXT         , ExtensionsGL::SRGB_ALPHA_EXT      , UNSIGNED_BYTE                 );
    default:
        return false;
    }
#undef POSSIBLE_FORMAT_TYPE_CASE

    return true;
}
#endif // !USE(ANGLE)

GCGLenum GraphicsContextGLOpenGL::computeImageSizeInBytes(
    GCGLenum format, GCGLenum type, GCGLsizei width, GCGLsizei height, GCGLsizei depth, const GraphicsContextGLOpenGL::PixelStoreParams& params,
    unsigned* imageSizeInBytes, unsigned* paddingInBytes, unsigned* skipSizeInBytes)
{
    ASSERT(imageSizeInBytes);
    ASSERT(params.alignment == 1 || params.alignment == 2 || params.alignment == 4 || params.alignment == 8);
    ASSERT(params.rowLength >= 0);
    ASSERT(params.imageHeight >= 0);
    ASSERT(params.skipPixels >= 0);
    ASSERT(params.skipRows >= 0);
    ASSERT(params.skipImages >= 0);
    if (width < 0 || height < 0 || depth < 0)
        return GraphicsContextGL::INVALID_VALUE;
    if (!width || !height || !depth) {
        *imageSizeInBytes = 0;
        if (paddingInBytes)
            *paddingInBytes = 0;
        if (skipSizeInBytes)
            *skipSizeInBytes = 0;
        return GraphicsContextGL::NO_ERROR;
    }

    int rowLength = params.rowLength > 0 ? params.rowLength : width;
    int imageHeight = params.imageHeight > 0 ? params.imageHeight : height;

    unsigned bytesPerComponent, componentsPerPixel;
    if (!computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent))
        return GraphicsContextGL::INVALID_ENUM;
    unsigned bytesPerGroup = bytesPerComponent * componentsPerPixel;
    Checked<uint32_t, RecordOverflow> checkedValue = static_cast<uint32_t>(rowLength);
    checkedValue *= bytesPerGroup;
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;

    unsigned lastRowSize;
    if (params.rowLength > 0 && params.rowLength != width) {
        Checked<uint32_t, RecordOverflow> tmp = width;
        tmp *= bytesPerGroup;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        lastRowSize = tmp.unsafeGet();
    } else
        lastRowSize = checkedValue.unsafeGet();

    unsigned padding = 0;
    unsigned residual = checkedValue.unsafeGet() % params.alignment;
    if (residual) {
        padding = params.alignment - residual;
        checkedValue += padding;
    }
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    unsigned paddedRowSize = checkedValue.unsafeGet();

    Checked<uint32_t, RecordOverflow> rows = imageHeight;
    rows *= (depth - 1);
    // Last image is not affected by IMAGE_HEIGHT parameter.
    rows += height;
    if (rows.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    checkedValue *= (rows - 1);
    // Last row is not affected by ROW_LENGTH parameter.
    checkedValue += lastRowSize;
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    *imageSizeInBytes = checkedValue.unsafeGet();
    if (paddingInBytes)
        *paddingInBytes = padding;

    Checked<uint32_t, RecordOverflow> skipSize = 0;
    if (params.skipImages > 0) {
        Checked<uint32_t, RecordOverflow> tmp = paddedRowSize;
        tmp *= imageHeight;
        tmp *= params.skipImages;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp.unsafeGet();
    }
    if (params.skipRows > 0) {
        Checked<uint32_t, RecordOverflow> tmp = paddedRowSize;
        tmp *= params.skipRows;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp.unsafeGet();
    }
    if (params.skipPixels > 0) {
        Checked<uint32_t, RecordOverflow> tmp = bytesPerGroup;
        tmp *= params.skipPixels;
        if (tmp.hasOverflowed())
            return GraphicsContextGL::INVALID_VALUE;
        skipSize += tmp.unsafeGet();
    }
    if (skipSize.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    if (skipSizeInBytes)
        *skipSizeInBytes = skipSize.unsafeGet();

    checkedValue += skipSize.unsafeGet();
    if (checkedValue.hasOverflowed())
        return GraphicsContextGL::INVALID_VALUE;
    return GraphicsContextGL::NO_ERROR;
}

GraphicsContextGLOpenGL::PixelStoreParams::PixelStoreParams()
    : alignment(4)
    , rowLength(0)
    , imageHeight(0)
    , skipPixels(0)
    , skipRows(0)
    , skipImages(0)
{
}

GraphicsContextGLOpenGL::ImageExtractor::ImageExtractor(Image* image, DOMSource imageHtmlDomSource, bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    m_image = image;
    m_imageHtmlDomSource = imageHtmlDomSource;
    m_extractSucceeded = extractImage(premultiplyAlpha, ignoreGammaAndColorProfile);
}

bool GraphicsContextGLOpenGL::packImageData(Image* image, const void* pixels, GCGLenum format, GCGLenum type, bool flipY, AlphaOp alphaOp, DataFormat sourceFormat, unsigned sourceImageWidth, unsigned sourceImageHeight, const IntRect& sourceImageSubRectangle, int depth, unsigned sourceUnpackAlignment, int unpackImageHeight, Vector<uint8_t>& data)
{
    if (!image || !pixels)
        return false;

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    PixelStoreParams params;
    params.alignment = 1;
    if (computeImageSizeInBytes(format, type, sourceImageSubRectangle.width(), sourceImageSubRectangle.height(), depth, params, &packedSize, nullptr, nullptr) != GraphicsContextGL::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(reinterpret_cast<const uint8_t*>(pixels), sourceFormat, sourceImageWidth, sourceImageHeight, sourceImageSubRectangle, depth, sourceUnpackAlignment, unpackImageHeight, format, type, alphaOp, data.data(), flipY))
        return false;
    if (ImageObserver* observer = image->imageObserver())
        observer->didDraw(*image);
    return true;
}

bool GraphicsContextGLOpenGL::extractImageData(
    ImageData* imageData, DataFormat sourceDataFormat, const IntRect& sourceImageSubRectangle,
    int depth, int unpackImageHeight, GCGLenum format, GCGLenum type, bool flipY, bool premultiplyAlpha, Vector<uint8_t>& data)
{
    if (!imageData)
        return false;
    int width = imageData->width();
    int height = imageData->height();

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    PixelStoreParams params;
    params.alignment = 1;
    if (computeImageSizeInBytes(format, type, sourceImageSubRectangle.width(), sourceImageSubRectangle.height(), depth, params, &packedSize, nullptr, nullptr) != GraphicsContextGL::NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(imageData->data()->data(), sourceDataFormat, width, height, sourceImageSubRectangle, depth, 0, unpackImageHeight, format, type, premultiplyAlpha ? AlphaOp::DoPremultiply : AlphaOp::DoNothing, data.data(), flipY))
        return false;

    return true;
}

bool GraphicsContextGLOpenGL::extractTextureData(unsigned width, unsigned height, GCGLenum format, GCGLenum type, const PixelStoreParams& unpackParams, bool flipY, bool premultiplyAlpha, const void* pixels, Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    DataFormat sourceDataFormat = getDataFormat(format, type);

    // Resize the output buffer.
    unsigned componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent))
        return false;
    unsigned bytesPerPixel = componentsPerPixel * bytesPerComponent;
    data.resize(width * height * bytesPerPixel);

    unsigned imageSizeInBytes, skipSizeInBytes;
    computeImageSizeInBytes(format, type, width, height, 1, unpackParams, &imageSizeInBytes, nullptr, &skipSizeInBytes);
    const uint8_t* srcData = static_cast<const uint8_t*>(pixels);
    if (skipSizeInBytes)
        srcData += skipSizeInBytes;

    if (!packPixels(srcData, sourceDataFormat, unpackParams.rowLength ? unpackParams.rowLength : width, height, IntRect(0, 0, width, height), 1, unpackParams.alignment, 0, format, type, (premultiplyAlpha ? AlphaOp::DoPremultiply : AlphaOp::DoNothing), data.data(), flipY))
        return false;

    return true;
}

ALWAYS_INLINE unsigned TexelBytesForFormat(GraphicsContextGL::DataFormat format)
{
    switch (format) {
    case GraphicsContextGL::DataFormat::R8:
    case GraphicsContextGL::DataFormat::R8_S:
    case GraphicsContextGL::DataFormat::A8:
        return 1;
    case GraphicsContextGL::DataFormat::RG8:
    case GraphicsContextGL::DataFormat::RG8_S:
    case GraphicsContextGL::DataFormat::RA8:
    case GraphicsContextGL::DataFormat::AR8:
    case GraphicsContextGL::DataFormat::RGBA5551:
    case GraphicsContextGL::DataFormat::RGBA4444:
    case GraphicsContextGL::DataFormat::RGB565:
    case GraphicsContextGL::DataFormat::A16F:
    case GraphicsContextGL::DataFormat::R16:
    case GraphicsContextGL::DataFormat::R16_S:
    case GraphicsContextGL::DataFormat::R16F:
    case GraphicsContextGL::DataFormat::D16:
        return 2;
    case GraphicsContextGL::DataFormat::RGB8:
    case GraphicsContextGL::DataFormat::RGB8_S:
    case GraphicsContextGL::DataFormat::BGR8:
        return 3;
    case GraphicsContextGL::DataFormat::RGBA8:
    case GraphicsContextGL::DataFormat::RGBA8_S:
    case GraphicsContextGL::DataFormat::ARGB8:
    case GraphicsContextGL::DataFormat::ABGR8:
    case GraphicsContextGL::DataFormat::BGRA8:
    case GraphicsContextGL::DataFormat::R32:
    case GraphicsContextGL::DataFormat::R32_S:
    case GraphicsContextGL::DataFormat::R32F:
    case GraphicsContextGL::DataFormat::A32F:
    case GraphicsContextGL::DataFormat::RA16F:
    case GraphicsContextGL::DataFormat::RGBA2_10_10_10:
    case GraphicsContextGL::DataFormat::RGB10F11F11F:
    case GraphicsContextGL::DataFormat::RGB5999:
    case GraphicsContextGL::DataFormat::RG16:
    case GraphicsContextGL::DataFormat::RG16_S:
    case GraphicsContextGL::DataFormat::RG16F:
    case GraphicsContextGL::DataFormat::D32:
    case GraphicsContextGL::DataFormat::D32F:
    case GraphicsContextGL::DataFormat::DS24_8:
        return 4;
    case GraphicsContextGL::DataFormat::RGB16:
    case GraphicsContextGL::DataFormat::RGB16_S:
    case GraphicsContextGL::DataFormat::RGB16F:
        return 6;
    case GraphicsContextGL::DataFormat::RGBA16:
    case GraphicsContextGL::DataFormat::RGBA16_S:
    case GraphicsContextGL::DataFormat::RA32F:
    case GraphicsContextGL::DataFormat::RGBA16F:
    case GraphicsContextGL::DataFormat::RG32:
    case GraphicsContextGL::DataFormat::RG32_S:
    case GraphicsContextGL::DataFormat::RG32F:
        return 8;
    case GraphicsContextGL::DataFormat::RGB32:
    case GraphicsContextGL::DataFormat::RGB32_S:
    case GraphicsContextGL::DataFormat::RGB32F:
        return 12;
    case GraphicsContextGL::DataFormat::RGBA32:
    case GraphicsContextGL::DataFormat::RGBA32_S:
    case GraphicsContextGL::DataFormat::RGBA32F:
        return 16;
    default:
        return 0;
    }
}

bool GraphicsContextGLOpenGL::packPixels(const uint8_t* sourceData, DataFormat sourceDataFormat, unsigned sourceDataWidth, unsigned sourceDataHeight, const IntRect& sourceDataSubRectangle, int depth, unsigned sourceUnpackAlignment, int unpackImageHeight, unsigned destinationFormat, unsigned destinationType, AlphaOp alphaOp, void* destinationData, bool flipY)
{
    ASSERT(depth >= 1);
    UNUSED_PARAM(sourceDataHeight); // Derived from sourceDataSubRectangle.height().
    if (!unpackImageHeight)
        unpackImageHeight = sourceDataSubRectangle.height();
    int validSrc = sourceDataWidth * TexelBytesForFormat(sourceDataFormat);
    int remainder = sourceUnpackAlignment ? (validSrc % sourceUnpackAlignment) : 0;
    int srcStride = remainder ? (validSrc + sourceUnpackAlignment - remainder) : validSrc;
    int srcRowOffset = sourceDataSubRectangle.x() * TexelBytesForFormat(sourceDataFormat);

    DataFormat dstDataFormat = getDataFormat(destinationFormat, destinationType);
    int dstStride = sourceDataSubRectangle.width() * TexelBytesForFormat(dstDataFormat);
    if (flipY) {
        destinationData = static_cast<uint8_t*>(destinationData) + dstStride * ((depth * sourceDataSubRectangle.height()) - 1);
        dstStride = -dstStride;
    }
    if (!hasAlpha(sourceDataFormat) || !hasColor(sourceDataFormat) || !hasColor(dstDataFormat))
        alphaOp = AlphaOp::DoNothing;

    if (sourceDataFormat == dstDataFormat && alphaOp == AlphaOp::DoNothing) {
        const uint8_t* basePtr = sourceData + srcStride * sourceDataSubRectangle.y();
        const uint8_t* baseEnd = sourceData + srcStride * sourceDataSubRectangle.maxY();

        // If packing multiple images into a 3D texture, and flipY is true,
        // then the sub-rectangle is pointing at the start of the
        // "bottommost" of those images. Since the source pointer strides in
        // the positive direction, we need to back it up to point at the
        // last, or "topmost", of these images.
        if (flipY && depth > 1) {
            const ptrdiff_t distanceToTopImage = (depth - 1) * srcStride * unpackImageHeight;
            basePtr -= distanceToTopImage;
            baseEnd -= distanceToTopImage;
        }

        unsigned rowSize = (dstStride > 0) ? dstStride: -dstStride;
        uint8_t* dst = static_cast<uint8_t*>(destinationData);

        for (int i = 0; i < depth; ++i) {
            const uint8_t* ptr = basePtr;
            const uint8_t* ptrEnd = baseEnd;
            while (ptr < ptrEnd) {
                memcpy(dst, ptr + srcRowOffset, rowSize);
                ptr += srcStride;
                dst += dstStride;
            }
            basePtr += unpackImageHeight * srcStride;
            baseEnd += unpackImageHeight * srcStride;
        }
        return true;
    }

    FormatConverter converter(
        sourceDataSubRectangle, depth,
        unpackImageHeight, sourceData, destinationData,
        srcStride, srcRowOffset, dstStride);
    converter.convert(sourceDataFormat, dstDataFormat, alphaOp);
    if (!converter.success())
        return false;
    return true;
}

#if !(PLATFORM(COCOA) && (USE(OPENGL) || USE(ANGLE)))
void GraphicsContextGLOpenGL::setContextVisibility(bool)
{
}
#endif

#if !PLATFORM(COCOA)
void GraphicsContextGLOpenGL::simulateContextChanged()
{
}

void GraphicsContextGLOpenGL::prepareForDisplay()
{
}
#endif

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_GL)
