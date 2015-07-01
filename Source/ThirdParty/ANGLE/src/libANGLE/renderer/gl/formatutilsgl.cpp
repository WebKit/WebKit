//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutilsgl.cpp: Queries for GL image formats and their translations to native
// GL formats.

#include "libANGLE/renderer/gl/formatutilsgl.h"

#include <map>

namespace rx
{

namespace nativegl
{

// Information about internal formats
static bool AlwaysSupported(GLuint, GLuint, const std::vector<std::string> &)
{
    return true;
}

static bool UnimplementedSupport(GLuint, GLuint, const std::vector<std::string> &)
{
    return false;
}

static bool NeverSupported(GLuint, GLuint, const std::vector<std::string> &)
{
    return false;
}

template <GLuint minMajorVersion, GLuint minMinorVersion>
static bool RequireGL(GLuint major, GLuint minor, const std::vector<std::string> &)
{
    return major > minMajorVersion || (major == minMajorVersion && minor >= minMinorVersion);
}


InternalFormat::InternalFormat()
    : textureSupport(NeverSupported),
      renderSupport(NeverSupported),
      filterSupport(NeverSupported)
{
}

typedef std::pair<GLenum, InternalFormat> InternalFormatInfoPair;
typedef std::map<GLenum, InternalFormat> InternalFormatInfoMap;

// A helper function to insert data into the format map with fewer characters.
static inline void InsertFormatMapping(InternalFormatInfoMap *map, GLenum internalFormat,
                                       InternalFormat::SupportCheckFunction textureSupport,
                                       InternalFormat::SupportCheckFunction renderSupport,
                                       InternalFormat::SupportCheckFunction filterSupport)
{
    InternalFormat formatInfo;
    formatInfo.textureSupport = textureSupport;
    formatInfo.renderSupport = renderSupport;
    formatInfo.filterSupport = filterSupport;
    map->insert(std::make_pair(internalFormat, formatInfo));
}

static InternalFormatInfoMap BuildInternalFormatInfoMap()
{
    InternalFormatInfoMap map;

    // From ES 3.0.1 spec, table 3.12
    InsertFormatMapping(&map, GL_NONE,              NeverSupported,       NeverSupported,       NeverSupported);

    //                       | Internal format     | Texture support     | Render support     | Filter support      |
    InsertFormatMapping(&map, GL_R8,                UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R8_SNORM,          UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG8,               UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG8_SNORM,         UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB8,              AlwaysSupported,      AlwaysSupported,      AlwaysSupported     );
    InsertFormatMapping(&map, GL_RGB8_SNORM,        UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB565,            AlwaysSupported,      AlwaysSupported,      AlwaysSupported     );
    InsertFormatMapping(&map, GL_RGBA4,             AlwaysSupported,      AlwaysSupported,      AlwaysSupported     );
    InsertFormatMapping(&map, GL_RGB5_A1,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA8,             AlwaysSupported,      AlwaysSupported,      AlwaysSupported     );
    InsertFormatMapping(&map, GL_RGBA8_SNORM,       UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB10_A2,          UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB10_A2UI,        UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_SRGB8,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_SRGB8_ALPHA8,      UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R11F_G11F_B10F,    UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB9_E5,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R8I,               UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R8UI,              UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R16I,              UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R16UI,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R32I,              UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R32UI,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG8I,              UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG8UI,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG16I,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG16UI,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG32I,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG32UI,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB8I,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB8UI,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB16I,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB16UI,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB32I,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB32UI,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA8I,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA8UI,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA16I,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA16UI,          UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA32I,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA32UI,          UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    InsertFormatMapping(&map, GL_BGRA8_EXT,         UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    // Floating point formats
    //                       | Internal format     | Texture support     | Render support     | Filter support      |
    InsertFormatMapping(&map, GL_R16F,              UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG16F,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB16F,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA16F,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_R32F,              UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG32F,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB32F,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA32F,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    // Depth stencil formats
    //                       | Internal format         | Texture support     | Render support     | Filter support      |
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT16,     AlwaysSupported,      AlwaysSupported,      NeverSupported      );
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT24,     UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT32F,    UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT32_OES, UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_DEPTH24_STENCIL8,      UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_DEPTH32F_STENCIL8,     UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_STENCIL_INDEX8,        AlwaysSupported,      AlwaysSupported,      NeverSupported      );

    // Luminance alpha formats
    //                       | Internal format          | Texture support     | Render support     | Filter support      |
    InsertFormatMapping(&map, GL_ALPHA8_EXT,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_LUMINANCE8_EXT,         UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_ALPHA32F_EXT,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_LUMINANCE32F_EXT,       UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_ALPHA16F_EXT,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_LUMINANCE16F_EXT,       UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_LUMINANCE8_ALPHA8_EXT,  UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA32F_EXT, UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA16F_EXT, UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    // Unsized formats
    //                       | Internal format   | Texture support     | Render support     | Filter support      |
    InsertFormatMapping(&map, GL_ALPHA,           UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_LUMINANCE,       UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA, UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RED,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG,              UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB,             UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA,            UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RED_INTEGER,     UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RG_INTEGER,      UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGB_INTEGER,     UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_RGBA_INTEGER,    UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_BGRA_EXT,        UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT, UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_DEPTH_STENCIL,   UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_SRGB_EXT,        UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_SRGB_ALPHA_EXT,  UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    // Compressed formats, From ES 3.0.1 spec, table 3.16
    //                       | Internal format                             | Texture support     | Render support     | Filter support      |
    InsertFormatMapping(&map, GL_COMPRESSED_R11_EAC,                        UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_SIGNED_R11_EAC,                 UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_RG11_EAC,                       UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_SIGNED_RG11_EAC,                UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_RGB8_ETC2,                      UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_SRGB8_ETC2,                     UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA8_ETC2_EAC,                 UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    // From GL_EXT_texture_compression_dxt1
    //                       | Internal format                   | Texture support     | Render support     | Filter support      |
    InsertFormatMapping(&map, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    // From GL_ANGLE_texture_compression_dxt3
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    // From GL_ANGLE_texture_compression_dxt5
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, UnimplementedSupport, UnimplementedSupport, UnimplementedSupport);

    return map;
}

static const InternalFormatInfoMap &GetInternalFormatMap()
{
    static const InternalFormatInfoMap formatMap = BuildInternalFormatInfoMap();
    return formatMap;
}

const InternalFormat &GetInternalFormatInfo(GLenum internalFormat)
{
    const InternalFormatInfoMap &formatMap = GetInternalFormatMap();
    InternalFormatInfoMap::const_iterator iter = formatMap.find(internalFormat);
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

}

}
