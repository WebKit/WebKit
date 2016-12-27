//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils.cpp: Queries for GL image formats.

#include "common/mathutil.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"

using namespace angle;

namespace gl
{

// ES2 requires that format is equal to internal format at all glTex*Image2D entry points and the implementation
// can decide the true, sized, internal format. The ES2FormatMap determines the internal format for all valid
// format and type combinations.
GLenum GetSizedFormatInternal(GLenum format, GLenum type);

namespace
{
typedef std::pair<GLenum, InternalFormat> InternalFormatInfoPair;
typedef std::map<GLenum, InternalFormat> InternalFormatInfoMap;

}  // anonymous namespace

FormatType::FormatType() : format(GL_NONE), type(GL_NONE)
{
}

FormatType::FormatType(GLenum format_, GLenum type_) : format(format_), type(type_)
{
}

bool FormatType::operator<(const FormatType &other) const
{
    if (format != other.format)
        return format < other.format;
    return type < other.type;
}

Type::Type()
    : bytes(0),
      bytesShift(0),
      specialInterpretation(false)
{
}

static Type GenTypeInfo(GLuint bytes, bool specialInterpretation)
{
    Type info;
    info.bytes = bytes;
    GLuint i = 0;
    while ((1u << i) < bytes)
    {
        ++i;
    }
    info.bytesShift = i;
    ASSERT((1u << info.bytesShift) == bytes);
    info.specialInterpretation = specialInterpretation;
    return info;
}

bool operator<(const Type& a, const Type& b)
{
    return memcmp(&a, &b, sizeof(Type)) < 0;
}

// Information about internal formats
static bool AlwaysSupported(GLuint, const Extensions &)
{
    return true;
}

static bool NeverSupported(GLuint, const Extensions &)
{
    return false;
}

template <GLuint minCoreGLVersion>
static bool RequireES(GLuint clientVersion, const Extensions &)
{
    return clientVersion >= minCoreGLVersion;
}

// Pointer to a boolean memeber of the Extensions struct
typedef bool(Extensions::*ExtensionBool);

// Check support for a single extension
template <ExtensionBool bool1>
static bool RequireExt(GLuint, const Extensions & extensions)
{
    return extensions.*bool1;
}

// Check for a minimum client version or a single extension
template <GLuint minCoreGLVersion, ExtensionBool bool1>
static bool RequireESOrExt(GLuint clientVersion, const Extensions &extensions)
{
    return clientVersion >= minCoreGLVersion || extensions.*bool1;
}

// Check for a minimum client version or two extensions
template <GLuint minCoreGLVersion, ExtensionBool bool1, ExtensionBool bool2>
static bool RequireESOrExtAndExt(GLuint clientVersion, const Extensions &extensions)
{
    return clientVersion >= minCoreGLVersion || (extensions.*bool1 && extensions.*bool2);
}

// Check for a minimum client version or at least one of two extensions
template <GLuint minCoreGLVersion, ExtensionBool bool1, ExtensionBool bool2>
static bool RequireESOrExtOrExt(GLuint clientVersion, const Extensions &extensions)
{
    return clientVersion >= minCoreGLVersion || extensions.*bool1 || extensions.*bool2;
}

// Check support for two extensions
template <ExtensionBool bool1, ExtensionBool bool2>
static bool RequireExtAndExt(GLuint, const Extensions &extensions)
{
    return extensions.*bool1 && extensions.*bool2;
}

// Check support for either of two extensions
template <ExtensionBool bool1, ExtensionBool bool2>
static bool RequireExtOrExt(GLuint, const Extensions &extensions)
{
    return extensions.*bool1 || extensions.*bool2;
}

// Special function for half float formats with three or four channels.
static bool HalfFloatSupport(GLuint clientVersion, const Extensions &extensions)
{
    return clientVersion >= 3 || extensions.textureHalfFloat;
}

static bool HalfFloatRenderableSupport(GLuint clientVersion, const Extensions &extensions)
{
    return HalfFloatSupport(clientVersion, extensions) && extensions.colorBufferHalfFloat;
}

// Special function for half float formats with one or two channels.
static bool HalfFloatSupportRG(GLuint clientVersion, const Extensions &extensions)
{
    return clientVersion >= 3 || (extensions.textureHalfFloat && extensions.textureRG);
}

static bool HalfFloatRenderableSupportRG(GLuint clientVersion, const Extensions &extensions)
{
    return HalfFloatSupportRG(clientVersion, extensions) && extensions.colorBufferHalfFloat;
}

// Special function for float formats with three or four channels.
static bool FloatSupport(GLuint clientVersion, const Extensions &extensions)
{
    return clientVersion >= 3 || extensions.textureFloat;
}

static bool FloatRenderableSupport(GLuint clientVersion, const Extensions &extensions)
{
    // We don't expose colorBufferFloat in ES2, but we silently support rendering to float.
    return FloatSupport(clientVersion, extensions) &&
           (extensions.colorBufferFloat || clientVersion == 2);
}

// Special function for float formats with one or two channels.
static bool FloatSupportRG(GLuint clientVersion, const Extensions &extensions)
{
    return clientVersion >= 3 || (extensions.textureFloat && extensions.textureRG);
}

static bool FloatRenderableSupportRG(GLuint clientVersion, const Extensions &extensions)
{
    // We don't expose colorBufferFloat in ES2, but we silently support rendering to float.
    return FloatSupportRG(clientVersion, extensions) &&
           (extensions.colorBufferFloat || clientVersion == 2);
}

InternalFormat::InternalFormat()
    : internalFormat(GL_NONE),
      redBits(0),
      greenBits(0),
      blueBits(0),
      luminanceBits(0),
      alphaBits(0),
      sharedBits(0),
      depthBits(0),
      stencilBits(0),
      pixelBytes(0),
      componentCount(0),
      compressed(false),
      compressedBlockWidth(0),
      compressedBlockHeight(0),
      format(GL_NONE),
      type(GL_NONE),
      componentType(GL_NONE),
      colorEncoding(GL_NONE),
      textureSupport(NeverSupported),
      renderSupport(NeverSupported),
      filterSupport(NeverSupported)
{
}

bool InternalFormat::isLUMA() const
{
    return ((redBits + greenBits + blueBits + depthBits + stencilBits) == 0 &&
            (luminanceBits + alphaBits) > 0);
}

GLenum InternalFormat::getReadPixelsFormat() const
{
    return format;
}

GLenum InternalFormat::getReadPixelsType() const
{
    switch (type)
    {
        case GL_HALF_FLOAT:
            // The internal format may have a type of GL_HALF_FLOAT but when exposing this type as
            // the IMPLEMENTATION_READ_TYPE, only HALF_FLOAT_OES is allowed by
            // OES_texture_half_float
            return GL_HALF_FLOAT_OES;

        default:
            return type;
    }
}

Format::Format(GLenum internalFormat) : Format(GetInternalFormatInfo(internalFormat))
{
}

Format::Format(const InternalFormat &internalFormat)
    : info(&internalFormat), format(info->format), type(info->type), sized(true)
{
    ASSERT((info->pixelBytes > 0 && format != GL_NONE && type != GL_NONE) ||
           internalFormat.format == GL_NONE);
}

Format::Format(GLenum internalFormat, GLenum format, GLenum type)
    : info(nullptr), format(format), type(type), sized(false)
{
    const auto &plainInfo = GetInternalFormatInfo(internalFormat);
    sized                 = plainInfo.pixelBytes > 0;
    info = (sized ? &plainInfo : &GetInternalFormatInfo(GetSizedFormatInternal(format, type)));
    ASSERT(format == GL_NONE || info->pixelBytes > 0);
}

Format::Format(const Format &other) = default;
Format &Format::operator=(const Format &other) = default;

GLenum Format::asSized() const
{
    return sized ? info->internalFormat : GetSizedFormatInternal(format, type);
}

bool Format::valid() const
{
    return info->format != GL_NONE;
}

// static
bool Format::SameSized(const Format &a, const Format &b)
{
    return (a.info == b.info);
}

// static
Format Format::Invalid()
{
    static Format invalid(GL_NONE, GL_NONE, GL_NONE);
    return invalid;
}

bool InternalFormat::operator==(const InternalFormat &other) const
{
    // We assume there are no duplicates.
    ASSERT((this == &other) == (internalFormat == other.internalFormat));
    return internalFormat == other.internalFormat;
}

bool InternalFormat::operator!=(const InternalFormat &other) const
{
    // We assume there are no duplicates.
    ASSERT((this != &other) == (internalFormat != other.internalFormat));
    return internalFormat != other.internalFormat;
}

static void AddUnsizedFormat(InternalFormatInfoMap *map,
                             GLenum internalFormat,
                             GLenum format,
                             InternalFormat::SupportCheckFunction textureSupport,
                             InternalFormat::SupportCheckFunction renderSupport,
                             InternalFormat::SupportCheckFunction filterSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat = internalFormat;
    formatInfo.format = format;
    formatInfo.textureSupport = textureSupport;
    formatInfo.renderSupport = renderSupport;
    formatInfo.filterSupport = filterSupport;
    ASSERT(map->count(internalFormat) == 0);
    (*map)[internalFormat] = formatInfo;
}

void AddRGBAFormat(InternalFormatInfoMap *map,
                   GLenum internalFormat,
                   GLuint red,
                   GLuint green,
                   GLuint blue,
                   GLuint alpha,
                   GLuint shared,
                   GLenum format,
                   GLenum type,
                   GLenum componentType,
                   bool srgb,
                   InternalFormat::SupportCheckFunction textureSupport,
                   InternalFormat::SupportCheckFunction renderSupport,
                   InternalFormat::SupportCheckFunction filterSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat = internalFormat;
    formatInfo.redBits = red;
    formatInfo.greenBits = green;
    formatInfo.blueBits = blue;
    formatInfo.alphaBits = alpha;
    formatInfo.sharedBits = shared;
    formatInfo.pixelBytes = (red + green + blue + alpha + shared) / 8;
    formatInfo.componentCount = ((red > 0) ? 1 : 0) + ((green > 0) ? 1 : 0) + ((blue > 0) ? 1 : 0) + ((alpha > 0) ? 1 : 0);
    formatInfo.format = format;
    formatInfo.type = type;
    formatInfo.componentType = componentType;
    formatInfo.colorEncoding = (srgb ? GL_SRGB : GL_LINEAR);
    formatInfo.textureSupport = textureSupport;
    formatInfo.renderSupport = renderSupport;
    formatInfo.filterSupport = filterSupport;
    ASSERT(map->count(internalFormat) == 0);
    (*map)[internalFormat] = formatInfo;
}

static InternalFormat LUMAFormat(GLuint luminance, GLuint alpha, GLenum format, GLenum type, GLenum componentType,
                                 InternalFormat::SupportCheckFunction textureSupport,
                                 InternalFormat::SupportCheckFunction renderSupport,
                                 InternalFormat::SupportCheckFunction filterSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat = GetSizedFormatInternal(format, type);
    formatInfo.luminanceBits = luminance;
    formatInfo.alphaBits = alpha;
    formatInfo.pixelBytes = (luminance + alpha) / 8;
    formatInfo.componentCount = ((luminance > 0) ? 1 : 0) + ((alpha > 0) ? 1 : 0);
    formatInfo.format = format;
    formatInfo.type = type;
    formatInfo.componentType = componentType;
    formatInfo.colorEncoding = GL_LINEAR;
    formatInfo.textureSupport = textureSupport;
    formatInfo.renderSupport = renderSupport;
    formatInfo.filterSupport = filterSupport;
    return formatInfo;
}

void AddDepthStencilFormat(InternalFormatInfoMap *map,
                           GLenum internalFormat,
                           GLuint depthBits,
                           GLuint stencilBits,
                           GLuint unusedBits,
                           GLenum format,
                           GLenum type,
                           GLenum componentType,
                           InternalFormat::SupportCheckFunction textureSupport,
                           InternalFormat::SupportCheckFunction renderSupport,
                           InternalFormat::SupportCheckFunction filterSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat = internalFormat;
    formatInfo.depthBits = depthBits;
    formatInfo.stencilBits = stencilBits;
    formatInfo.pixelBytes = (depthBits + stencilBits + unusedBits) / 8;
    formatInfo.componentCount = ((depthBits > 0) ? 1 : 0) + ((stencilBits > 0) ? 1 : 0);
    formatInfo.format = format;
    formatInfo.type = type;
    formatInfo.componentType = componentType;
    formatInfo.colorEncoding = GL_LINEAR;
    formatInfo.textureSupport = textureSupport;
    formatInfo.renderSupport = renderSupport;
    formatInfo.filterSupport = filterSupport;
    ASSERT(map->count(internalFormat) == 0);
    (*map)[internalFormat] = formatInfo;
}

static InternalFormat CompressedFormat(GLuint compressedBlockWidth, GLuint compressedBlockHeight, GLuint compressedBlockSize,
                                       GLuint componentCount, GLenum format, GLenum type, bool srgb,
                                       InternalFormat::SupportCheckFunction textureSupport,
                                       InternalFormat::SupportCheckFunction renderSupport,
                                       InternalFormat::SupportCheckFunction filterSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat        = format;
    formatInfo.compressedBlockWidth = compressedBlockWidth;
    formatInfo.compressedBlockHeight = compressedBlockHeight;
    formatInfo.pixelBytes = compressedBlockSize / 8;
    formatInfo.componentCount = componentCount;
    formatInfo.format = format;
    formatInfo.type = type;
    formatInfo.componentType = GL_UNSIGNED_NORMALIZED;
    formatInfo.colorEncoding = (srgb ? GL_SRGB : GL_LINEAR);
    formatInfo.compressed = true;
    formatInfo.textureSupport = textureSupport;
    formatInfo.renderSupport = renderSupport;
    formatInfo.filterSupport = filterSupport;
    return formatInfo;
}

static InternalFormatInfoMap BuildInternalFormatInfoMap()
{
    InternalFormatInfoMap map;

    // From ES 3.0.1 spec, table 3.12
    map.insert(InternalFormatInfoPair(GL_NONE, InternalFormat()));

    // clang-format off

    //                 | Internal format     | R | G | B | A |S | Format         | Type                           | Component type        | SRGB | Texture supported                        | Renderable                               | Filterable    |
    AddRGBAFormat(&map, GL_R8,                 8,  0,  0,  0, 0, GL_RED,          GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, false, RequireESOrExt<3, &Extensions::textureRG>, RequireESOrExt<3, &Extensions::textureRG>, AlwaysSupported);
    AddRGBAFormat(&map, GL_R8_SNORM,           8,  0,  0,  0, 0, GL_RED,          GL_BYTE,                         GL_SIGNED_NORMALIZED,   false, RequireES<3>,                              NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_RG8,                8,  8,  0,  0, 0, GL_RG,           GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, false, RequireESOrExt<3, &Extensions::textureRG>, RequireESOrExt<3, &Extensions::textureRG>, AlwaysSupported);
    AddRGBAFormat(&map, GL_RG8_SNORM,          8,  8,  0,  0, 0, GL_RG,           GL_BYTE,                         GL_SIGNED_NORMALIZED,   false, RequireES<3>,                              NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB8,               8,  8,  8,  0, 0, GL_RGB,          GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, false, RequireESOrExt<3, &Extensions::rgb8rgba8>, RequireESOrExt<3, &Extensions::rgb8rgba8>, AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB8_SNORM,         8,  8,  8,  0, 0, GL_RGB,          GL_BYTE,                         GL_SIGNED_NORMALIZED,   false, RequireES<3>,                              NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB565,             5,  6,  5,  0, 0, GL_RGB,          GL_UNSIGNED_SHORT_5_6_5,         GL_UNSIGNED_NORMALIZED, false, RequireES<2>,                              RequireES<2>,                              AlwaysSupported);
    AddRGBAFormat(&map, GL_RGBA4,              4,  4,  4,  4, 0, GL_RGBA,         GL_UNSIGNED_SHORT_4_4_4_4,       GL_UNSIGNED_NORMALIZED, false, RequireES<2>,                              RequireES<2>,                              AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB5_A1,            5,  5,  5,  1, 0, GL_RGBA,         GL_UNSIGNED_SHORT_5_5_5_1,       GL_UNSIGNED_NORMALIZED, false, RequireES<2>,                              RequireES<2>,                              AlwaysSupported);
    AddRGBAFormat(&map, GL_RGBA8,              8,  8,  8,  8, 0, GL_RGBA,         GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, false, RequireESOrExt<3, &Extensions::rgb8rgba8>, RequireESOrExt<3, &Extensions::rgb8rgba8>, AlwaysSupported);
    AddRGBAFormat(&map, GL_RGBA8_SNORM,        8,  8,  8,  8, 0, GL_RGBA,         GL_BYTE,                         GL_SIGNED_NORMALIZED,   false, RequireES<3>,                              NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB10_A2,          10, 10, 10,  2, 0, GL_RGBA,         GL_UNSIGNED_INT_2_10_10_10_REV,  GL_UNSIGNED_NORMALIZED, false, RequireES<3>,                              RequireES<3>,                              AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB10_A2UI,        10, 10, 10,  2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV,  GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_SRGB8,              8,  8,  8,  0, 0, GL_RGB,          GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, true,  RequireESOrExt<3, &Extensions::sRGB>,      NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_SRGB8_ALPHA8,       8,  8,  8,  8, 0, GL_RGBA,         GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, true,  RequireESOrExt<3, &Extensions::sRGB>,      RequireESOrExt<3, &Extensions::sRGB>,      AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB9_E5,            9,  9,  9,  0, 5, GL_RGB,          GL_UNSIGNED_INT_5_9_9_9_REV,     GL_FLOAT,               false, RequireES<3>,                              NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_R8I,                8,  0,  0,  0, 0, GL_RED_INTEGER,  GL_BYTE,                         GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_R8UI,               8,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_BYTE,                GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_R16I,              16,  0,  0,  0, 0, GL_RED_INTEGER,  GL_SHORT,                        GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_R16UI,             16,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_SHORT,               GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_R32I,              32,  0,  0,  0, 0, GL_RED_INTEGER,  GL_INT,                          GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_R32UI,             32,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_INT,                 GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RG8I,               8,  8,  0,  0, 0, GL_RG_INTEGER,   GL_BYTE,                         GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RG8UI,              8,  8,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_BYTE,                GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RG16I,             16, 16,  0,  0, 0, GL_RG_INTEGER,   GL_SHORT,                        GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RG16UI,            16, 16,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_SHORT,               GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RG32I,             32, 32,  0,  0, 0, GL_RG_INTEGER,   GL_INT,                          GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_R11F_G11F_B10F,    11, 11, 10,  0, 0, GL_RGB,          GL_UNSIGNED_INT_10F_11F_11F_REV, GL_FLOAT,               false, RequireES<3>,                              RequireExt<&Extensions::colorBufferFloat>, AlwaysSupported);
    AddRGBAFormat(&map, GL_RG32UI,            32, 32,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_INT,                 GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RGB8I,              8,  8,  8,  0, 0, GL_RGB_INTEGER,  GL_BYTE,                         GL_INT,                 false, RequireES<3>,                              NeverSupported,                            NeverSupported);
    AddRGBAFormat(&map, GL_RGB8UI,             8,  8,  8,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_BYTE,                GL_UNSIGNED_INT,        false, RequireES<3>,                              NeverSupported,                            NeverSupported);
    AddRGBAFormat(&map, GL_RGB16I,            16, 16, 16,  0, 0, GL_RGB_INTEGER,  GL_SHORT,                        GL_INT,                 false, RequireES<3>,                              NeverSupported,                            NeverSupported);
    AddRGBAFormat(&map, GL_RGB16UI,           16, 16, 16,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_SHORT,               GL_UNSIGNED_INT,        false, RequireES<3>,                              NeverSupported,                            NeverSupported);
    AddRGBAFormat(&map, GL_RGB32I,            32, 32, 32,  0, 0, GL_RGB_INTEGER,  GL_INT,                          GL_INT,                 false, RequireES<3>,                              NeverSupported,                            NeverSupported);
    AddRGBAFormat(&map, GL_RGB32UI,           32, 32, 32,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_INT,                 GL_UNSIGNED_INT,        false, RequireES<3>,                              NeverSupported,                            NeverSupported);
    AddRGBAFormat(&map, GL_RGBA8I,             8,  8,  8,  8, 0, GL_RGBA_INTEGER, GL_BYTE,                         GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RGBA8UI,            8,  8,  8,  8, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,                GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RGBA16I,           16, 16, 16, 16, 0, GL_RGBA_INTEGER, GL_SHORT,                        GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RGBA16UI,          16, 16, 16, 16, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT,               GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RGBA32I,           32, 32, 32, 32, 0, GL_RGBA_INTEGER, GL_INT,                          GL_INT,                 false, RequireES<3>,                              RequireES<3>,                              NeverSupported);
    AddRGBAFormat(&map, GL_RGBA32UI,          32, 32, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT,                 GL_UNSIGNED_INT,        false, RequireES<3>,                              RequireES<3>,                              NeverSupported);

    AddRGBAFormat(&map, GL_BGRA8_EXT,          8,  8,  8,  8, 0, GL_BGRA_EXT,     GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureFormatBGRA8888>, RequireExt<&Extensions::textureFormatBGRA8888>, AlwaysSupported);
    AddRGBAFormat(&map, GL_BGRA4_ANGLEX,       4,  4,  4,  4, 0, GL_BGRA_EXT,     GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT, GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureFormatBGRA8888>, RequireExt<&Extensions::textureFormatBGRA8888>, AlwaysSupported);
    AddRGBAFormat(&map, GL_BGR5_A1_ANGLEX,     5,  5,  5,  1, 0, GL_BGRA_EXT,     GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureFormatBGRA8888>, RequireExt<&Extensions::textureFormatBGRA8888>, AlwaysSupported);

    // Special format which is not really supported, so always false for all supports.
    AddRGBAFormat(&map, GL_BGR565_ANGLEX,      5,  6,  5,  1, 0, GL_BGRA_EXT,     GL_UNSIGNED_SHORT_5_6_5,           GL_UNSIGNED_NORMALIZED, false, NeverSupported, NeverSupported, NeverSupported);

    // Floating point renderability and filtering is provided by OES_texture_float and OES_texture_half_float
    //                 | Internal format     | D |S | Format             | Type                                   | Comp   | SRGB |  Texture supported | Renderable                  | Filterable                                    |
    //                 |                     |   |  |                    |                                        | type   |      |                    |                             |                                               |
    AddRGBAFormat(&map, GL_R16F,              16,  0,  0,  0, 0, GL_RED,          GL_HALF_FLOAT,                   GL_FLOAT, false, HalfFloatSupportRG, HalfFloatRenderableSupportRG, RequireExt<&Extensions::textureHalfFloatLinear>);
    AddRGBAFormat(&map, GL_RG16F,             16, 16,  0,  0, 0, GL_RG,           GL_HALF_FLOAT,                   GL_FLOAT, false, HalfFloatSupportRG, HalfFloatRenderableSupportRG, RequireExt<&Extensions::textureHalfFloatLinear>);
    AddRGBAFormat(&map, GL_RGB16F,            16, 16, 16,  0, 0, GL_RGB,          GL_HALF_FLOAT,                   GL_FLOAT, false, HalfFloatSupport,   HalfFloatRenderableSupport,   RequireExt<&Extensions::textureHalfFloatLinear>);
    AddRGBAFormat(&map, GL_RGBA16F,           16, 16, 16, 16, 0, GL_RGBA,         GL_HALF_FLOAT,                   GL_FLOAT, false, HalfFloatSupport,   HalfFloatRenderableSupport,   RequireExt<&Extensions::textureHalfFloatLinear>);
    AddRGBAFormat(&map, GL_R32F,              32,  0,  0,  0, 0, GL_RED,          GL_FLOAT,                        GL_FLOAT, false, FloatSupportRG,     FloatRenderableSupportRG,     RequireExt<&Extensions::textureFloatLinear>    );
    AddRGBAFormat(&map, GL_RG32F,             32, 32,  0,  0, 0, GL_RG,           GL_FLOAT,                        GL_FLOAT, false, FloatSupportRG,     FloatRenderableSupportRG,     RequireExt<&Extensions::textureFloatLinear>    );
    AddRGBAFormat(&map, GL_RGB32F,            32, 32, 32,  0, 0, GL_RGB,          GL_FLOAT,                        GL_FLOAT, false, FloatSupport,       FloatRenderableSupport,       RequireExt<&Extensions::textureFloatLinear>    );
    AddRGBAFormat(&map, GL_RGBA32F,           32, 32, 32, 32, 0, GL_RGBA,         GL_FLOAT,                        GL_FLOAT, false, FloatSupport,       FloatRenderableSupport,       RequireExt<&Extensions::textureFloatLinear>    );

    // Depth stencil formats
    //                         | Internal format         | D |S | X | Format            | Type                             | Component type        | Supported                                    | Renderable                                                                         | Filterable                                  |
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT16,     16, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,                 GL_UNSIGNED_NORMALIZED, RequireES<2>,                                  RequireES<2>,                                                                        RequireESOrExt<3, &Extensions::depthTextures>);
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT24,     24, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,                   GL_UNSIGNED_NORMALIZED, RequireES<3>,                                  RequireES<3>,                                                                        RequireESOrExt<3, &Extensions::depthTextures>);
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT32F,    32, 0,  0, GL_DEPTH_COMPONENT, GL_FLOAT,                          GL_FLOAT,               RequireES<3>,                                  RequireES<3>,                                                                        RequireESOrExt<3, &Extensions::depthTextures>);
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT32_OES, 32, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,                   GL_UNSIGNED_NORMALIZED, RequireExtOrExt<&Extensions::depthTextures, &Extensions::depth32>, RequireExtOrExt<&Extensions::depthTextures, &Extensions::depth32>, AlwaysSupported                            );
    AddDepthStencilFormat(&map, GL_DEPTH24_STENCIL8,      24, 8,  0, GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8,              GL_UNSIGNED_NORMALIZED, RequireESOrExt<3, &Extensions::depthTextures>, RequireESOrExtOrExt<3, &Extensions::depthTextures, &Extensions::packedDepthStencil>, AlwaysSupported                              );
    AddDepthStencilFormat(&map, GL_DEPTH32F_STENCIL8,     32, 8, 24, GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV, GL_FLOAT,               RequireES<3>,                                  RequireES<3>,                                                                        AlwaysSupported                              );
    // STENCIL_INDEX8 is special-cased, see around the bottom of the list.

    // Luminance alpha formats
    //                               | Internal format          |          | L | A | Format            | Type            | Component type        | Supported                                                                    | Renderable    | Filterable    |
    map.insert(InternalFormatInfoPair(GL_ALPHA8_EXT,             LUMAFormat( 0,  8, GL_ALPHA,           GL_UNSIGNED_BYTE, GL_UNSIGNED_NORMALIZED, RequireExt<&Extensions::textureStorage>,                                      NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE8_EXT,         LUMAFormat( 8,  0, GL_LUMINANCE,       GL_UNSIGNED_BYTE, GL_UNSIGNED_NORMALIZED, RequireExt<&Extensions::textureStorage>,                                      NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_ALPHA32F_EXT,           LUMAFormat( 0, 32, GL_ALPHA,           GL_FLOAT,         GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureFloat>,     NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE32F_EXT,       LUMAFormat(32,  0, GL_LUMINANCE,       GL_FLOAT,         GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureFloat>,     NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_ALPHA16F_EXT,           LUMAFormat( 0, 16, GL_ALPHA,           GL_HALF_FLOAT,    GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureHalfFloat>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE16F_EXT,       LUMAFormat(16,  0, GL_LUMINANCE,       GL_HALF_FLOAT,    GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureHalfFloat>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE8_ALPHA8_EXT,  LUMAFormat( 8,  8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, GL_UNSIGNED_NORMALIZED, RequireExt<&Extensions::textureStorage>,                                      NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE_ALPHA32F_EXT, LUMAFormat(32, 32, GL_LUMINANCE_ALPHA, GL_FLOAT,         GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureFloat>,     NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE_ALPHA16F_EXT, LUMAFormat(16, 16, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT,    GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureHalfFloat>, NeverSupported, AlwaysSupported)));

    // Unsized formats
    //                    | Internal format   | Format            | Supported                                         | Renderable                                        | Filterable    |
    AddUnsizedFormat(&map, GL_ALPHA,           GL_ALPHA,           RequireES<2>,                                       NeverSupported,                                     AlwaysSupported);
    AddUnsizedFormat(&map, GL_LUMINANCE,       GL_LUMINANCE,       RequireES<2>,                                       NeverSupported,                                     AlwaysSupported);
    AddUnsizedFormat(&map, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, RequireES<2>,                                       NeverSupported,                                     AlwaysSupported);
    AddUnsizedFormat(&map, GL_RED,             GL_RED,             RequireESOrExt<3, &Extensions::textureRG>,          NeverSupported,                                     AlwaysSupported);
    AddUnsizedFormat(&map, GL_RG,              GL_RG,              RequireESOrExt<3, &Extensions::textureRG>,          NeverSupported,                                     AlwaysSupported);
    AddUnsizedFormat(&map, GL_RGB,             GL_RGB,             RequireES<2>,                                       RequireES<2>,                                       AlwaysSupported);
    AddUnsizedFormat(&map, GL_RGBA,            GL_RGBA,            RequireES<2>,                                       RequireES<2>,                                       AlwaysSupported);
    AddUnsizedFormat(&map, GL_RED_INTEGER,     GL_RED_INTEGER,     RequireES<3>,                                       NeverSupported,                                     NeverSupported );
    AddUnsizedFormat(&map, GL_RG_INTEGER,      GL_RG_INTEGER,      RequireES<3>,                                       NeverSupported,                                     NeverSupported );
    AddUnsizedFormat(&map, GL_RGB_INTEGER,     GL_RGB_INTEGER,     RequireES<3>,                                       NeverSupported,                                     NeverSupported );
    AddUnsizedFormat(&map, GL_RGBA_INTEGER,    GL_RGBA_INTEGER,    RequireES<3>,                                       NeverSupported,                                     NeverSupported );
    AddUnsizedFormat(&map, GL_BGRA_EXT,        GL_BGRA_EXT,        RequireExt<&Extensions::textureFormatBGRA8888>,     RequireExt<&Extensions::textureFormatBGRA8888>,     AlwaysSupported);
    AddUnsizedFormat(&map, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, RequireES<2>,                                       RequireES<2>,                                       AlwaysSupported);
    AddUnsizedFormat(&map, GL_DEPTH_STENCIL,   GL_DEPTH_STENCIL,   RequireESOrExt<3, &Extensions::packedDepthStencil>, RequireESOrExt<3, &Extensions::packedDepthStencil>, AlwaysSupported);
    AddUnsizedFormat(&map, GL_SRGB_EXT,        GL_RGB,             RequireESOrExt<3, &Extensions::sRGB>,               NeverSupported,                                     AlwaysSupported);
    AddUnsizedFormat(&map, GL_SRGB_ALPHA_EXT,  GL_RGBA,            RequireESOrExt<3, &Extensions::sRGB>,               RequireESOrExt<3, &Extensions::sRGB>,               AlwaysSupported);

    // Compressed formats, From ES 3.0.1 spec, table 3.16
    //                               | Internal format                             |                |W |H | BS |CC| Format                                      | Type            | SRGB | Supported   | Renderable    | Filterable    |
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_R11_EAC,                        CompressedFormat(4, 4,  64, 1, GL_COMPRESSED_R11_EAC,                        GL_UNSIGNED_BYTE, false, RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SIGNED_R11_EAC,                 CompressedFormat(4, 4,  64, 1, GL_COMPRESSED_SIGNED_R11_EAC,                 GL_UNSIGNED_BYTE, false, RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RG11_EAC,                       CompressedFormat(4, 4, 128, 2, GL_COMPRESSED_RG11_EAC,                       GL_UNSIGNED_BYTE, false, RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SIGNED_RG11_EAC,                CompressedFormat(4, 4, 128, 2, GL_COMPRESSED_SIGNED_RG11_EAC,                GL_UNSIGNED_BYTE, false, RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGB8_ETC2,                      CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_RGB8_ETC2,                      GL_UNSIGNED_BYTE, false, RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ETC2,                     CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_SRGB8_ETC2,                     GL_UNSIGNED_BYTE, true,  RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  GL_UNSIGNED_BYTE, false, RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_UNSIGNED_BYTE, true,  RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA8_ETC2_EAC,                 CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_RGBA8_ETC2_EAC,                 GL_UNSIGNED_BYTE, false, RequireES<3>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          GL_UNSIGNED_BYTE, true,  RequireES<3>, NeverSupported, AlwaysSupported)));

    // From GL_EXT_texture_compression_dxt1
    //                               | Internal format                   |                |W |H | BS |CC| Format                            | Type            | SRGB | Supported                                         | Renderable    | Filterable    |
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::textureCompressionDXT1>,    NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   CompressedFormat(4, 4,  64, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::textureCompressionDXT1>,    NeverSupported, AlwaysSupported)));

    // From GL_ANGLE_texture_compression_dxt3
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::textureCompressionDXT5>,    NeverSupported, AlwaysSupported)));

    // From GL_ANGLE_texture_compression_dxt5
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::textureCompressionDXT5>,    NeverSupported, AlwaysSupported)));

    // From GL_OES_compressed_ETC1_RGB8_texture
    map.insert(InternalFormatInfoPair(GL_ETC1_RGB8_OES,                   CompressedFormat(4, 4,  64, 3, GL_ETC1_RGB8_OES,                   GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::compressedETC1RGB8Texture>, NeverSupported, AlwaysSupported)));

    // From KHR_texture_compression_astc_hdr
    //                               | Internal format                          |                | W | H | BS |CC| Format                                   | Type            | SRGB | Supported                                                                                     | Renderable     | Filterable    |
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_4x4_KHR,           CompressedFormat( 4,  4, 128, 4, GL_COMPRESSED_RGBA_ASTC_4x4_KHR,           GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_5x4_KHR,           CompressedFormat( 5,  4, 128, 4, GL_COMPRESSED_RGBA_ASTC_5x4_KHR,           GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_5x5_KHR,           CompressedFormat( 5,  5, 128, 4, GL_COMPRESSED_RGBA_ASTC_5x5_KHR,           GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_6x5_KHR,           CompressedFormat( 6,  5, 128, 4, GL_COMPRESSED_RGBA_ASTC_6x5_KHR,           GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_6x6_KHR,           CompressedFormat( 6,  6, 128, 4, GL_COMPRESSED_RGBA_ASTC_6x6_KHR,           GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_8x5_KHR,           CompressedFormat( 8,  5, 128, 4, GL_COMPRESSED_RGBA_ASTC_8x5_KHR,           GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_8x6_KHR,           CompressedFormat( 8,  6, 128, 4, GL_COMPRESSED_RGBA_ASTC_8x6_KHR,           GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_8x8_KHR,           CompressedFormat( 8,  8, 128, 4, GL_COMPRESSED_RGBA_ASTC_8x8_KHR,           GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_10x5_KHR,          CompressedFormat(10,  5, 128, 4, GL_COMPRESSED_RGBA_ASTC_10x5_KHR,          GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_10x6_KHR,          CompressedFormat(10,  6, 128, 4, GL_COMPRESSED_RGBA_ASTC_10x6_KHR,          GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_10x8_KHR,          CompressedFormat(10,  8, 128, 4, GL_COMPRESSED_RGBA_ASTC_10x8_KHR,          GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_10x10_KHR,         CompressedFormat(10, 10, 128, 4, GL_COMPRESSED_RGBA_ASTC_10x10_KHR,         GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_12x10_KHR,         CompressedFormat(12, 10, 128, 4, GL_COMPRESSED_RGBA_ASTC_12x10_KHR,         GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_ASTC_12x12_KHR,         CompressedFormat(12, 12, 128, 4, GL_COMPRESSED_RGBA_ASTC_12x12_KHR,         GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));

    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,   CompressedFormat( 4,  4, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,   GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,   CompressedFormat( 5,  4, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,   GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,   CompressedFormat( 5,  5, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,   GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,   CompressedFormat( 6,  5, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,   GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,   CompressedFormat( 6,  6, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,   GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,   CompressedFormat( 8,  5, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,   GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,   CompressedFormat( 8,  6, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,   GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,   CompressedFormat( 8,  8, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,   GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,  CompressedFormat(10,  5, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,  GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,  CompressedFormat(10,  6, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,  GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,  CompressedFormat(10,  8, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,  GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, CompressedFormat(10, 10, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, CompressedFormat(12, 10, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, CompressedFormat(12, 12, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, NeverSupported, AlwaysSupported)));

    // For STENCIL_INDEX8 we chose a normalized component type for the following reasons:
    // - Multisampled buffer are disallowed for non-normalized integer component types and we want to support it for STENCIL_INDEX8
    // - All other stencil formats (all depth-stencil) are either float or normalized
    // - It affects only validation of internalformat in RenderbufferStorageMultisample.
    //                         | Internal format  |D |S |X | Format    | Type            | Component type        | Supported   | Renderable  | Filterable   |
    AddDepthStencilFormat(&map, GL_STENCIL_INDEX8, 0, 8, 0, GL_STENCIL, GL_UNSIGNED_BYTE, GL_UNSIGNED_NORMALIZED, RequireES<2>, RequireES<2>, NeverSupported);

    // From GL_ANGLE_lossy_etc_decode
    map.insert(InternalFormatInfoPair(GL_ETC1_RGB8_LOSSY_DECODE_ANGLE, CompressedFormat(4, 4, 64, 3, GL_ETC1_RGB8_LOSSY_DECODE_ANGLE, GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::lossyETCDecode>, NeverSupported, AlwaysSupported)));

    // From GL_EXT_texture_norm16
    //                 | Internal format     | R | G | B | A |S | Format         | Type                           | Component type        | SRGB | Texture supported                        | Renderable                               | Filterable    |
    AddRGBAFormat(&map, GL_R16_EXT,           16,  0,  0,  0, 0, GL_RED,          GL_UNSIGNED_SHORT,               GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureNorm16>,    RequireExt<&Extensions::textureNorm16>,    AlwaysSupported);
    AddRGBAFormat(&map, GL_R16_SNORM_EXT,     16,  0,  0,  0, 0, GL_RED,          GL_SHORT,                        GL_SIGNED_NORMALIZED,   false, RequireExt<&Extensions::textureNorm16>,    NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_RG16_EXT,          16, 16,  0,  0, 0, GL_RG,           GL_UNSIGNED_SHORT,               GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureNorm16>,    RequireExt<&Extensions::textureNorm16>,    AlwaysSupported);
    AddRGBAFormat(&map, GL_RG16_SNORM_EXT,    16, 16,  0,  0, 0, GL_RG,           GL_SHORT,                        GL_SIGNED_NORMALIZED,   false, RequireExt<&Extensions::textureNorm16>,    NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB16_EXT,         16, 16, 16,  0, 0, GL_RGB,          GL_UNSIGNED_SHORT,               GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureNorm16>,    NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_RGB16_SNORM_EXT,   16, 16, 16,  0, 0, GL_RGB,          GL_SHORT,                        GL_SIGNED_NORMALIZED,   false, RequireExt<&Extensions::textureNorm16>,    NeverSupported,                            AlwaysSupported);
    AddRGBAFormat(&map, GL_RGBA16_EXT,        16, 16, 16, 16, 0, GL_RGBA,         GL_UNSIGNED_SHORT,               GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureNorm16>,    RequireExt<&Extensions::textureNorm16>,    AlwaysSupported);
    AddRGBAFormat(&map, GL_RGBA16_SNORM_EXT,  16, 16, 16, 16, 0, GL_RGBA,         GL_SHORT,                        GL_SIGNED_NORMALIZED,   false, RequireExt<&Extensions::textureNorm16>,    NeverSupported,                            AlwaysSupported);

    // clang-format on

    return map;
}

static const InternalFormatInfoMap &GetInternalFormatMap()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static const InternalFormatInfoMap formatMap = BuildInternalFormatInfoMap();
#pragma clang diagnostic pop
    return formatMap;
}

static FormatSet BuildAllSizedInternalFormatSet()
{
    FormatSet result;

    for (auto iter : GetInternalFormatMap())
    {
        if (iter.second.pixelBytes > 0)
        {
            // TODO(jmadill): Fix this hack.
            if (iter.first == GL_BGR565_ANGLEX)
                continue;

            result.insert(iter.first);
        }
    }

    return result;
}

const Type &GetTypeInfo(GLenum type)
{
    switch (type)
    {
      case GL_UNSIGNED_BYTE:
      case GL_BYTE:
        {
            static const Type info = GenTypeInfo(1, false);
            return info;
        }
      case GL_UNSIGNED_SHORT:
      case GL_SHORT:
      case GL_HALF_FLOAT:
      case GL_HALF_FLOAT_OES:
        {
            static const Type info = GenTypeInfo(2, false);
            return info;
        }
      case GL_UNSIGNED_INT:
      case GL_INT:
      case GL_FLOAT:
        {
            static const Type info = GenTypeInfo(4, false);
            return info;
        }
      case GL_UNSIGNED_SHORT_5_6_5:
      case GL_UNSIGNED_SHORT_4_4_4_4:
      case GL_UNSIGNED_SHORT_5_5_5_1:
      case GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT:
      case GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT:
        {
            static const Type info = GenTypeInfo(2, true);
            return info;
        }
      case GL_UNSIGNED_INT_2_10_10_10_REV:
      case GL_UNSIGNED_INT_24_8:
      case GL_UNSIGNED_INT_10F_11F_11F_REV:
      case GL_UNSIGNED_INT_5_9_9_9_REV:
        {
            ASSERT(GL_UNSIGNED_INT_24_8_OES == GL_UNSIGNED_INT_24_8);
            static const Type info = GenTypeInfo(4, true);
            return info;
        }
      case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        {
            static const Type info = GenTypeInfo(8, true);
            return info;
        }
      default:
        {
            static const Type defaultInfo;
            return defaultInfo;
        }
    }
}

const InternalFormat &GetInternalFormatInfo(GLenum internalFormat)
{
    const InternalFormatInfoMap &formatMap = GetInternalFormatMap();
    auto iter                              = formatMap.find(internalFormat);
    if (iter != formatMap.end())
    {
        return iter->second;
    }
    else
    {
        static const InternalFormat defaultInternalFormat;
        return defaultInternalFormat;
    }
}

GLuint InternalFormat::computePixelBytes(GLenum formatType) const
{
    const auto &typeInfo = GetTypeInfo(formatType);
    GLuint components    = typeInfo.specialInterpretation ? 1u : componentCount;
    return components * typeInfo.bytes;
}

ErrorOrResult<GLuint> InternalFormat::computeRowPitch(GLenum formatType,
                                                          GLsizei width,
                                                          GLint alignment,
                                                          GLint rowLength) const
{
    // Compressed images do not use pack/unpack parameters.
    if (compressed)
    {
        ASSERT(rowLength == 0);
        return computeCompressedImageSize(formatType, Extents(width, 1, 1));
    }

    CheckedNumeric<GLuint> checkedWidth(rowLength > 0 ? rowLength : width);
    CheckedNumeric<GLuint> checkedRowBytes = checkedWidth * computePixelBytes(formatType);

    ASSERT(alignment > 0 && isPow2(alignment));
    CheckedNumeric<GLuint> checkedAlignment(alignment);
    auto aligned = rx::roundUp(checkedRowBytes, checkedAlignment);
    ANGLE_TRY_CHECKED_MATH(aligned);
    return aligned.ValueOrDie();
}

ErrorOrResult<GLuint> InternalFormat::computeDepthPitch(GLsizei height,
                                                        GLint imageHeight,
                                                        GLuint rowPitch) const
{
    GLuint rows =
        (imageHeight > 0 ? static_cast<GLuint>(imageHeight) : static_cast<GLuint>(height));
    CheckedNumeric<GLuint> checkedRowPitch(rowPitch);

    auto depthPitch = checkedRowPitch * rows;
    ANGLE_TRY_CHECKED_MATH(depthPitch);
    return depthPitch.ValueOrDie();
}

ErrorOrResult<GLuint> InternalFormat::computeDepthPitch(GLenum formatType,
                                                            GLsizei width,
                                                            GLsizei height,
                                                            GLint alignment,
                                                            GLint rowLength,
                                                            GLint imageHeight) const
{
    GLuint rowPitch = 0;
    ANGLE_TRY_RESULT(computeRowPitch(formatType, width, alignment, rowLength), rowPitch);
    return computeDepthPitch(height, imageHeight, rowPitch);
}

ErrorOrResult<GLuint> InternalFormat::computeCompressedImageSize(GLenum formatType,
                                                                     const Extents &size) const
{
    CheckedNumeric<GLuint> checkedWidth(size.width);
    CheckedNumeric<GLuint> checkedHeight(size.height);
    CheckedNumeric<GLuint> checkedDepth(size.depth);
    CheckedNumeric<GLuint> checkedBlockWidth(compressedBlockWidth);
    CheckedNumeric<GLuint> checkedBlockHeight(compressedBlockHeight);

    ASSERT(compressed);
    auto numBlocksWide = (checkedWidth + checkedBlockWidth - 1u) / checkedBlockWidth;
    auto numBlocksHigh = (checkedHeight + checkedBlockHeight - 1u) / checkedBlockHeight;
    auto bytes         = numBlocksWide * numBlocksHigh * pixelBytes * checkedDepth;
    ANGLE_TRY_CHECKED_MATH(bytes);
    return bytes.ValueOrDie();
}

ErrorOrResult<GLuint> InternalFormat::computeSkipBytes(GLuint rowPitch,
                                                           GLuint depthPitch,
                                                           const PixelStoreStateBase &state,
                                                           bool is3D) const
{
    CheckedNumeric<GLuint> checkedRowPitch(rowPitch);
    CheckedNumeric<GLuint> checkedDepthPitch(depthPitch);
    CheckedNumeric<GLuint> checkedSkipImages(static_cast<GLuint>(state.skipImages));
    CheckedNumeric<GLuint> checkedSkipRows(static_cast<GLuint>(state.skipRows));
    CheckedNumeric<GLuint> checkedSkipPixels(static_cast<GLuint>(state.skipPixels));
    CheckedNumeric<GLuint> checkedPixelBytes(pixelBytes);
    auto checkedSkipImagesBytes = checkedSkipImages * checkedDepthPitch;
    if (!is3D)
    {
        checkedSkipImagesBytes = 0;
    }
    auto skipBytes = checkedSkipImagesBytes + checkedSkipRows * checkedRowPitch +
                     checkedSkipPixels * checkedPixelBytes;
    ANGLE_TRY_CHECKED_MATH(skipBytes);
    return skipBytes.ValueOrDie();
}

ErrorOrResult<GLuint> InternalFormat::computePackUnpackEndByte(
    GLenum formatType,
    const Extents &size,
    const PixelStoreStateBase &state,
    bool is3D) const
{
    GLuint rowPitch = 0;
    ANGLE_TRY_RESULT(computeRowPitch(formatType, size.width, state.alignment, state.rowLength),
                     rowPitch);

    GLuint depthPitch = 0;
    if (is3D)
    {
        ANGLE_TRY_RESULT(computeDepthPitch(size.height, state.imageHeight, rowPitch), depthPitch);
    }

    CheckedNumeric<GLuint> checkedCopyBytes = 0;
    if (compressed)
    {
        ANGLE_TRY_RESULT(computeCompressedImageSize(formatType, size), checkedCopyBytes);
    }
    else if (size.height != 0 && (!is3D || size.depth != 0))
    {
        CheckedNumeric<GLuint> bytes = computePixelBytes(formatType);
        checkedCopyBytes += size.width * bytes;

        CheckedNumeric<GLuint> heightMinusOne = size.height - 1;
        checkedCopyBytes += heightMinusOne * rowPitch;

        if (is3D)
        {
            CheckedNumeric<GLuint> depthMinusOne = size.depth - 1;
            checkedCopyBytes += depthMinusOne * depthPitch;
        }
    }

    CheckedNumeric<GLuint> checkedSkipBytes = 0;
    ANGLE_TRY_RESULT(computeSkipBytes(rowPitch, depthPitch, state, is3D), checkedSkipBytes);

    CheckedNumeric<GLuint> endByte = checkedCopyBytes + checkedSkipBytes;

    ANGLE_TRY_CHECKED_MATH(endByte);
    return endByte.ValueOrDie();
}

GLenum GetSizedInternalFormat(GLenum internalFormat, GLenum type)
{
    const InternalFormat &formatInfo = GetInternalFormatInfo(internalFormat);
    if (formatInfo.pixelBytes > 0)
    {
        return internalFormat;
    }
    return GetSizedFormatInternal(internalFormat, type);
}

const FormatSet &GetAllSizedInternalFormats()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static FormatSet formatSet = BuildAllSizedInternalFormatSet();
#pragma clang diagnostic pop
    return formatSet;
}

AttributeType GetAttributeType(GLenum enumValue)
{
    switch (enumValue)
    {
        case GL_FLOAT:
            return ATTRIBUTE_FLOAT;
        case GL_FLOAT_VEC2:
            return ATTRIBUTE_VEC2;
        case GL_FLOAT_VEC3:
            return ATTRIBUTE_VEC3;
        case GL_FLOAT_VEC4:
            return ATTRIBUTE_VEC4;
        case GL_INT:
            return ATTRIBUTE_INT;
        case GL_INT_VEC2:
            return ATTRIBUTE_IVEC2;
        case GL_INT_VEC3:
            return ATTRIBUTE_IVEC3;
        case GL_INT_VEC4:
            return ATTRIBUTE_IVEC4;
        case GL_UNSIGNED_INT:
            return ATTRIBUTE_UINT;
        case GL_UNSIGNED_INT_VEC2:
            return ATTRIBUTE_UVEC2;
        case GL_UNSIGNED_INT_VEC3:
            return ATTRIBUTE_UVEC3;
        case GL_UNSIGNED_INT_VEC4:
            return ATTRIBUTE_UVEC4;
        case GL_FLOAT_MAT2:
            return ATTRIBUTE_MAT2;
        case GL_FLOAT_MAT3:
            return ATTRIBUTE_MAT3;
        case GL_FLOAT_MAT4:
            return ATTRIBUTE_MAT4;
        case GL_FLOAT_MAT2x3:
            return ATTRIBUTE_MAT2x3;
        case GL_FLOAT_MAT2x4:
            return ATTRIBUTE_MAT2x4;
        case GL_FLOAT_MAT3x2:
            return ATTRIBUTE_MAT3x2;
        case GL_FLOAT_MAT3x4:
            return ATTRIBUTE_MAT3x4;
        case GL_FLOAT_MAT4x2:
            return ATTRIBUTE_MAT4x2;
        case GL_FLOAT_MAT4x3:
            return ATTRIBUTE_MAT4x3;
        default:
            UNREACHABLE();
            return ATTRIBUTE_FLOAT;
    }
}

VertexFormatType GetVertexFormatType(GLenum type, GLboolean normalized, GLuint components, bool pureInteger)
{
    switch (type)
    {
        case GL_BYTE:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return VERTEX_FORMAT_SBYTE1_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SBYTE1_NORM;
                    return VERTEX_FORMAT_SBYTE1;
                case 2:
                    if (pureInteger)
                        return VERTEX_FORMAT_SBYTE2_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SBYTE2_NORM;
                    return VERTEX_FORMAT_SBYTE2;
                case 3:
                    if (pureInteger)
                        return VERTEX_FORMAT_SBYTE3_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SBYTE3_NORM;
                    return VERTEX_FORMAT_SBYTE3;
                case 4:
                    if (pureInteger)
                        return VERTEX_FORMAT_SBYTE4_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SBYTE4_NORM;
                    return VERTEX_FORMAT_SBYTE4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_UNSIGNED_BYTE:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return VERTEX_FORMAT_UBYTE1_INT;
                    if (normalized)
                        return VERTEX_FORMAT_UBYTE1_NORM;
                    return VERTEX_FORMAT_UBYTE1;
                case 2:
                    if (pureInteger)
                        return VERTEX_FORMAT_UBYTE2_INT;
                    if (normalized)
                        return VERTEX_FORMAT_UBYTE2_NORM;
                    return VERTEX_FORMAT_UBYTE2;
                case 3:
                    if (pureInteger)
                        return VERTEX_FORMAT_UBYTE3_INT;
                    if (normalized)
                        return VERTEX_FORMAT_UBYTE3_NORM;
                    return VERTEX_FORMAT_UBYTE3;
                case 4:
                    if (pureInteger)
                        return VERTEX_FORMAT_UBYTE4_INT;
                    if (normalized)
                        return VERTEX_FORMAT_UBYTE4_NORM;
                    return VERTEX_FORMAT_UBYTE4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_SHORT:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return VERTEX_FORMAT_SSHORT1_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SSHORT1_NORM;
                    return VERTEX_FORMAT_SSHORT1;
                case 2:
                    if (pureInteger)
                        return VERTEX_FORMAT_SSHORT2_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SSHORT2_NORM;
                    return VERTEX_FORMAT_SSHORT2;
                case 3:
                    if (pureInteger)
                        return VERTEX_FORMAT_SSHORT3_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SSHORT3_NORM;
                    return VERTEX_FORMAT_SSHORT3;
                case 4:
                    if (pureInteger)
                        return VERTEX_FORMAT_SSHORT4_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SSHORT4_NORM;
                    return VERTEX_FORMAT_SSHORT4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_UNSIGNED_SHORT:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return VERTEX_FORMAT_USHORT1_INT;
                    if (normalized)
                        return VERTEX_FORMAT_USHORT1_NORM;
                    return VERTEX_FORMAT_USHORT1;
                case 2:
                    if (pureInteger)
                        return VERTEX_FORMAT_USHORT2_INT;
                    if (normalized)
                        return VERTEX_FORMAT_USHORT2_NORM;
                    return VERTEX_FORMAT_USHORT2;
                case 3:
                    if (pureInteger)
                        return VERTEX_FORMAT_USHORT3_INT;
                    if (normalized)
                        return VERTEX_FORMAT_USHORT3_NORM;
                    return VERTEX_FORMAT_USHORT3;
                case 4:
                    if (pureInteger)
                        return VERTEX_FORMAT_USHORT4_INT;
                    if (normalized)
                        return VERTEX_FORMAT_USHORT4_NORM;
                    return VERTEX_FORMAT_USHORT4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_INT:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return VERTEX_FORMAT_SINT1_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SINT1_NORM;
                    return VERTEX_FORMAT_SINT1;
                case 2:
                    if (pureInteger)
                        return VERTEX_FORMAT_SINT2_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SINT2_NORM;
                    return VERTEX_FORMAT_SINT2;
                case 3:
                    if (pureInteger)
                        return VERTEX_FORMAT_SINT3_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SINT3_NORM;
                    return VERTEX_FORMAT_SINT3;
                case 4:
                    if (pureInteger)
                        return VERTEX_FORMAT_SINT4_INT;
                    if (normalized)
                        return VERTEX_FORMAT_SINT4_NORM;
                    return VERTEX_FORMAT_SINT4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_UNSIGNED_INT:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return VERTEX_FORMAT_UINT1_INT;
                    if (normalized)
                        return VERTEX_FORMAT_UINT1_NORM;
                    return VERTEX_FORMAT_UINT1;
                case 2:
                    if (pureInteger)
                        return VERTEX_FORMAT_UINT2_INT;
                    if (normalized)
                        return VERTEX_FORMAT_UINT2_NORM;
                    return VERTEX_FORMAT_UINT2;
                case 3:
                    if (pureInteger)
                        return VERTEX_FORMAT_UINT3_INT;
                    if (normalized)
                        return VERTEX_FORMAT_UINT3_NORM;
                    return VERTEX_FORMAT_UINT3;
                case 4:
                    if (pureInteger)
                        return VERTEX_FORMAT_UINT4_INT;
                    if (normalized)
                        return VERTEX_FORMAT_UINT4_NORM;
                    return VERTEX_FORMAT_UINT4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_FLOAT:
            switch (components)
            {
                case 1:
                    return VERTEX_FORMAT_FLOAT1;
                case 2:
                    return VERTEX_FORMAT_FLOAT2;
                case 3:
                    return VERTEX_FORMAT_FLOAT3;
                case 4:
                    return VERTEX_FORMAT_FLOAT4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_HALF_FLOAT:
            switch (components)
            {
                case 1:
                    return VERTEX_FORMAT_HALF1;
                case 2:
                    return VERTEX_FORMAT_HALF2;
                case 3:
                    return VERTEX_FORMAT_HALF3;
                case 4:
                    return VERTEX_FORMAT_HALF4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_FIXED:
            switch (components)
            {
                case 1:
                    return VERTEX_FORMAT_FIXED1;
                case 2:
                    return VERTEX_FORMAT_FIXED2;
                case 3:
                    return VERTEX_FORMAT_FIXED3;
                case 4:
                    return VERTEX_FORMAT_FIXED4;
                default:
                    UNREACHABLE();
                    break;
            }
        case GL_INT_2_10_10_10_REV:
            if (pureInteger)
                return VERTEX_FORMAT_SINT210_INT;
            if (normalized)
                return VERTEX_FORMAT_SINT210_NORM;
            return VERTEX_FORMAT_SINT210;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            if (pureInteger)
                return VERTEX_FORMAT_UINT210_INT;
            if (normalized)
                return VERTEX_FORMAT_UINT210_NORM;
            return VERTEX_FORMAT_UINT210;
        default:
            UNREACHABLE();
            break;
    }
    return VERTEX_FORMAT_UBYTE1;
}

VertexFormatType GetVertexFormatType(const VertexAttribute &attrib)
{
    return GetVertexFormatType(attrib.type, attrib.normalized, attrib.size, attrib.pureInteger);
}

VertexFormatType GetVertexFormatType(const VertexAttribute &attrib, GLenum currentValueType)
{
    if (!attrib.enabled)
    {
        return GetVertexFormatType(currentValueType, GL_FALSE, 4, (currentValueType != GL_FLOAT));
    }
    return GetVertexFormatType(attrib);
}

const VertexFormat &GetVertexFormatFromType(VertexFormatType vertexFormatType)
{
    switch (vertexFormatType)
    {
        case VERTEX_FORMAT_SBYTE1:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE1_NORM:
        {
            static const VertexFormat format(GL_BYTE, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE2:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE2_NORM:
        {
            static const VertexFormat format(GL_BYTE, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE3:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE3_NORM:
        {
            static const VertexFormat format(GL_BYTE, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE4:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE4_NORM:
        {
            static const VertexFormat format(GL_BYTE, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE1:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE1_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE2:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE2_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE3:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE3_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE4:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE4_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT1:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT1_NORM:
        {
            static const VertexFormat format(GL_SHORT, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT2:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT2_NORM:
        {
            static const VertexFormat format(GL_SHORT, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT3:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT3_NORM:
        {
            static const VertexFormat format(GL_SHORT, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT4:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT4_NORM:
        {
            static const VertexFormat format(GL_SHORT, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT1:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT1_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT2:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT2_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT3:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT3_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT4:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT4_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT1:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SINT1_NORM:
        {
            static const VertexFormat format(GL_INT, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SINT2:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SINT2_NORM:
        {
            static const VertexFormat format(GL_INT, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SINT3:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SINT3_NORM:
        {
            static const VertexFormat format(GL_INT, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SINT4:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT4_NORM:
        {
            static const VertexFormat format(GL_INT, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UINT1:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_UINT1_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_UINT2:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_UINT2_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_UINT3:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_UINT3_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_UINT4:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UINT4_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE1_INT:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_SBYTE2_INT:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_SBYTE3_INT:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_SBYTE4_INT:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_UBYTE1_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_UBYTE2_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_UBYTE3_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_UBYTE4_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_SSHORT1_INT:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_SSHORT2_INT:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_SSHORT3_INT:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_SSHORT4_INT:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_USHORT1_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_USHORT2_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_USHORT3_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_USHORT4_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_SINT1_INT:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_SINT2_INT:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_SINT3_INT:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_SINT4_INT:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_UINT1_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_UINT2_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_UINT3_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_UINT4_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_FIXED1:
        {
            static const VertexFormat format(GL_FIXED, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_FIXED2:
        {
            static const VertexFormat format(GL_FIXED, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_FIXED3:
        {
            static const VertexFormat format(GL_FIXED, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_FIXED4:
        {
            static const VertexFormat format(GL_FIXED, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_HALF1:
        {
            static const VertexFormat format(GL_HALF_FLOAT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_HALF2:
        {
            static const VertexFormat format(GL_HALF_FLOAT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_HALF3:
        {
            static const VertexFormat format(GL_HALF_FLOAT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_HALF4:
        {
            static const VertexFormat format(GL_HALF_FLOAT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_FLOAT1:
        {
            static const VertexFormat format(GL_FLOAT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_FLOAT2:
        {
            static const VertexFormat format(GL_FLOAT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_FLOAT3:
        {
            static const VertexFormat format(GL_FLOAT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_FLOAT4:
        {
            static const VertexFormat format(GL_FLOAT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT210:
        {
            static const VertexFormat format(GL_INT_2_10_10_10_REV, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UINT210:
        {
            static const VertexFormat format(GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT210_NORM:
        {
            static const VertexFormat format(GL_INT_2_10_10_10_REV, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UINT210_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT210_INT:
        {
            static const VertexFormat format(GL_INT_2_10_10_10_REV, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_UINT210_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 4, true);
            return format;
        }
        default:
        {
            static const VertexFormat format(GL_NONE, GL_FALSE, 0, false);
            return format;
        }
    }
}

VertexFormat::VertexFormat(GLenum typeIn, GLboolean normalizedIn, GLuint componentsIn, bool pureIntegerIn)
    : type(typeIn),
      normalized(normalizedIn),
      components(componentsIn),
      pureInteger(pureIntegerIn)
{
    // float -> !normalized
    ASSERT(!(type == GL_FLOAT || type == GL_HALF_FLOAT || type == GL_FIXED) || normalized == GL_FALSE);
}

}
