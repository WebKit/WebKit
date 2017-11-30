//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutilsgl.cpp: Queries for GL image formats and their translations to native
// GL formats.

#include "libANGLE/renderer/gl/formatutilsgl.h"

#include <limits>

#include "common/string_utils.h"
#include "libANGLE/formatutils.h"

namespace rx
{

namespace nativegl
{

SupportRequirement::SupportRequirement()
    : version(std::numeric_limits<GLuint>::max(), std::numeric_limits<GLuint>::max()),
      versionExtensions(),
      requiredExtensions()
{
}

SupportRequirement::SupportRequirement(const SupportRequirement &other) = default;

SupportRequirement::~SupportRequirement()
{
}

InternalFormat::InternalFormat()
    : texture(),
      filter(),
      renderbuffer(),
      framebufferAttachment()
{
}

InternalFormat::InternalFormat(const InternalFormat &other) = default;

InternalFormat::~InternalFormat()
{
}

// supported = version || vertexExt
static inline SupportRequirement VersionOrExts(GLuint major, GLuint minor, const std::string &versionExt)
{
    SupportRequirement requirement;
    requirement.version.major = major;
    requirement.version.minor = minor;
    angle::SplitStringAlongWhitespace(versionExt, &requirement.versionExtensions);
    return requirement;
}

// supported = (version || vertexExt) && requiredExt
static inline SupportRequirement VersionOrExtsAndExts(GLuint major,
                                                      GLuint minor,
                                                      const std::string &versionExt,
                                                      const std::string &requiredExt)
{
    SupportRequirement requirement;
    requirement.version.major = major;
    requirement.version.minor = minor;
    angle::SplitStringAlongWhitespace(versionExt, &requirement.versionExtensions);
    angle::SplitStringAlongWhitespace(requiredExt, &requirement.requiredExtensions);
    return requirement;
}

// supported = version
static inline SupportRequirement VersionOnly(GLuint major, GLuint minor)
{
    SupportRequirement requirement;
    requirement.version.major = major;
    requirement.version.minor = minor;
    return requirement;
}

// supported = ext
static inline SupportRequirement ExtsOnly(const std::string &ext)
{
    SupportRequirement requirement;
    requirement.version.major = 0;
    requirement.version.minor = 0;
    angle::SplitStringAlongWhitespace(ext, &requirement.requiredExtensions);
    return requirement;
}

// supported = true
static inline SupportRequirement AlwaysSupported()
{
    SupportRequirement requirement;
    requirement.version.major = 0;
    requirement.version.minor = 0;
    return requirement;
}

// supported = false
static inline SupportRequirement NeverSupported()
{
    SupportRequirement requirement;
    requirement.version.major = std::numeric_limits<GLuint>::max();
    requirement.version.minor = std::numeric_limits<GLuint>::max();
    return requirement;
}

struct InternalFormatInfo
{
    InternalFormat glesInfo;
    InternalFormat glInfo;
};

typedef std::pair<GLenum, InternalFormatInfo> InternalFormatInfoPair;
typedef std::map<GLenum, InternalFormatInfo> InternalFormatInfoMap;

// A helper function to insert data into the format map with fewer characters.
static inline void InsertFormatMapping(InternalFormatInfoMap *map, GLenum internalFormat,
                                       const SupportRequirement &desktopTexture, const SupportRequirement &desktopFilter, const SupportRequirement &desktopRender,
                                       const SupportRequirement &esTexture, const SupportRequirement &esFilter, const SupportRequirement &esRender)
{
    InternalFormatInfo formatInfo;
    formatInfo.glInfo.texture = desktopTexture;
    formatInfo.glInfo.filter = desktopFilter;
    formatInfo.glInfo.renderbuffer = desktopRender;
    formatInfo.glInfo.framebufferAttachment = desktopRender;
    formatInfo.glesInfo.texture = esTexture;
    formatInfo.glesInfo.filter = esTexture;
    formatInfo.glesInfo.renderbuffer = esFilter;
    formatInfo.glesInfo.framebufferAttachment = esRender;
    map->insert(std::make_pair(internalFormat, formatInfo));
}

static InternalFormatInfoMap BuildInternalFormatInfoMap()
{
    InternalFormatInfoMap map;

    // clang-format off
    //                       | Format              | OpenGL texture support                          | Filter           | OpenGL render support                        | OpenGL ES texture support                 | Filter           | OpenGL ES render support                 |
    InsertFormatMapping(&map, GL_R8,                VersionOrExts(3, 0, "GL_ARB_texture_rg"),         AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOrExts(3, 0, "GL_EXT_texture_rg"),   AlwaysSupported(), VersionOrExts(3, 0, "GL_EXT_texture_rg")  );
    InsertFormatMapping(&map, GL_R8_SNORM,          VersionOnly(3, 1),                                AlwaysSupported(), NeverSupported(),                              VersionOnly(3, 0),                          AlwaysSupported(), NeverSupported()                          );
    InsertFormatMapping(&map, GL_RG8,               VersionOrExts(3, 0, "GL_ARB_texture_rg"),         AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOrExts(3, 0, "GL_EXT_texture_rg"),   AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg")  );
    InsertFormatMapping(&map, GL_RG8_SNORM,         VersionOnly(3, 1),                                AlwaysSupported(), NeverSupported(),                              VersionOnly(3, 0),                          AlwaysSupported(), NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGB8,              AlwaysSupported(),                                AlwaysSupported(), AlwaysSupported(),                             VersionOrExts(3, 0, "GL_OES_rgb8_rgba8"),   AlwaysSupported(), AlwaysSupported()                         );
    InsertFormatMapping(&map, GL_RGB8_SNORM,        VersionOnly(3, 1),                                AlwaysSupported(), NeverSupported(),                              VersionOnly(3, 0),                          AlwaysSupported(), NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGB565,            AlwaysSupported(),                                AlwaysSupported(), AlwaysSupported(),                             AlwaysSupported(),                          AlwaysSupported(), AlwaysSupported()                         );
    InsertFormatMapping(&map, GL_RGBA4,             AlwaysSupported(),                                AlwaysSupported(), AlwaysSupported(),                             AlwaysSupported(),                          AlwaysSupported(), AlwaysSupported()                         );
    InsertFormatMapping(&map, GL_RGB5_A1,           AlwaysSupported(),                                AlwaysSupported(), AlwaysSupported(),                             AlwaysSupported(),                          AlwaysSupported(), AlwaysSupported()                         );
    InsertFormatMapping(&map, GL_RGBA8,             AlwaysSupported(),                                AlwaysSupported(), AlwaysSupported(),                             VersionOrExts(3, 0, "GL_OES_rgb8_rgba8"),   AlwaysSupported(), VersionOrExts(3, 0, "GL_OES_rgb8_rgba8")  );
    InsertFormatMapping(&map, GL_RGBA8_SNORM,       VersionOnly(3, 1),                                AlwaysSupported(), NeverSupported(),                              VersionOnly(3, 0),                          AlwaysSupported(), NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGB10_A2,          AlwaysSupported(),                                AlwaysSupported(), AlwaysSupported(),                             VersionOnly(3, 0),                          AlwaysSupported(), VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RGB10_A2UI,        VersionOrExts(3, 3, "GL_ARB_texture_rgb10_a2ui"), NeverSupported(),  AlwaysSupported(),                             VersionOnly(3, 0),                          NeverSupported(),  AlwaysSupported()                         );
    InsertFormatMapping(&map, GL_SRGB8,             VersionOrExts(2, 1, "GL_EXT_texture_sRGB"),       AlwaysSupported(), VersionOrExts(2, 1, "GL_EXT_texture_sRGB"),    VersionOnly(3, 0),                          AlwaysSupported(), NeverSupported()                          );
    InsertFormatMapping(&map, GL_SRGB8_ALPHA8,      VersionOrExts(2, 1, "GL_EXT_texture_sRGB"),       AlwaysSupported(), VersionOrExts(2, 1, "GL_EXT_texture_sRGB"),    VersionOnly(3, 0),                          AlwaysSupported(), VersionOrExts(3, 0, "GL_EXT_sRGB")        );
    InsertFormatMapping(&map, GL_R8I,               VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_R8UI,              VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_R16I,              VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_R16UI,             VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_R32I,              VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_R32UI,             VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RG8I,              VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RG8UI,             VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RG16I,             VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RG16UI,            VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RG32I,             VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RG32UI,            VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RGB8I,             VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  NeverSupported(),                              VersionOnly(3, 0),                          NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGB8UI,            VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  NeverSupported(),                              VersionOnly(3, 0),                          NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGB16I,            VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  NeverSupported(),                              VersionOnly(3, 0),                          NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGB16UI,           VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  NeverSupported(),                              VersionOnly(3, 0),                          NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGB32I,            VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  NeverSupported(),                              VersionOnly(3, 0),                          NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGB32UI,           VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  NeverSupported(),                              VersionOnly(3, 0),                          NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGBA8I,            VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  VersionOrExts(3, 0, "GL_EXT_texture_integer"), VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RGBA8UI,           VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  VersionOrExts(3, 0, "GL_EXT_texture_integer"), VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RGBA16I,           VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  VersionOrExts(3, 0, "GL_EXT_texture_integer"), VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RGBA16UI,          VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  VersionOrExts(3, 0, "GL_EXT_texture_integer"), VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RGBA32I,           VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  VersionOrExts(3, 0, "GL_EXT_texture_integer"), VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RGBA32UI,          VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  VersionOrExts(3, 0, "GL_EXT_texture_integer"), VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );

    // Unsized formats
    InsertFormatMapping(&map, GL_ALPHA,             NeverSupported(),                                 NeverSupported(),  NeverSupported(),                              NeverSupported(),                           NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_LUMINANCE,         NeverSupported(),                                 NeverSupported(),  NeverSupported(),                              NeverSupported(),                           NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA,   NeverSupported(),                                 NeverSupported(),  NeverSupported(),                              NeverSupported(),                           NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_RED,               VersionOrExts(3, 0, "GL_ARB_texture_rg"),         AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOrExts(3, 0, "GL_EXT_texture_rg"),   AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg")  );
    InsertFormatMapping(&map, GL_RG,                VersionOrExts(3, 0, "GL_ARB_texture_rg"),         AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOrExts(3, 0, "GL_EXT_texture_rg"),   AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg")  );
    InsertFormatMapping(&map, GL_RGB,               AlwaysSupported(),                                AlwaysSupported(), AlwaysSupported(),                             AlwaysSupported(),                          AlwaysSupported(), AlwaysSupported()                         );
    InsertFormatMapping(&map, GL_RGBA,              AlwaysSupported(),                                AlwaysSupported(), AlwaysSupported(),                             AlwaysSupported(),                          AlwaysSupported(), AlwaysSupported()                         );
    InsertFormatMapping(&map, GL_RED_INTEGER,       VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RG_INTEGER,        VersionOrExts(3, 0, "GL_ARB_texture_rg"),         NeverSupported(),  VersionOrExts(3, 0, "GL_ARB_texture_rg"),      VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_RGB_INTEGER,       VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  NeverSupported(),                              VersionOnly(3, 0),                          NeverSupported(),  NeverSupported()                          );
    InsertFormatMapping(&map, GL_RGBA_INTEGER,      VersionOrExts(3, 0, "GL_EXT_texture_integer"),    NeverSupported(),  VersionOrExts(3, 0, "GL_EXT_texture_integer"), VersionOnly(3, 0),                          NeverSupported(),  VersionOnly(3, 0)                         );
    InsertFormatMapping(&map, GL_SRGB,              VersionOrExts(2, 1, "GL_EXT_texture_sRGB"),       AlwaysSupported(), VersionOrExts(2, 1, "GL_EXT_texture_sRGB"),    ExtsOnly("GL_EXT_sRGB"),                    AlwaysSupported(), NeverSupported()                          );
    InsertFormatMapping(&map, GL_SRGB_ALPHA,        VersionOrExts(2, 1, "GL_EXT_texture_sRGB"),       AlwaysSupported(), VersionOrExts(2, 1, "GL_EXT_texture_sRGB"),    ExtsOnly("GL_EXT_sRGB"),                    AlwaysSupported(), NeverSupported()                          );

    // From GL_EXT_texture_format_BGRA8888
    InsertFormatMapping(&map, GL_BGRA8_EXT,         VersionOrExts(1, 2, "GL_EXT_bgra"),               AlwaysSupported(), VersionOrExts(1, 2, "GL_EXT_bgra"),            ExtsOnly("GL_EXT_texture_format_BGRA8888"), AlwaysSupported(), NeverSupported()                          );
    InsertFormatMapping(&map, GL_BGRA_EXT,          VersionOrExts(1, 2, "GL_EXT_bgra"),               AlwaysSupported(), VersionOrExts(1, 2, "GL_EXT_bgra"),            ExtsOnly("GL_EXT_texture_format_BGRA8888"), AlwaysSupported(), NeverSupported()                          );

    // Floating point formats
    // Note that GL_EXT_texture_shared_exponent and GL_ARB_color_buffer_float suggest that RGB9_E5
    // would be renderable, but once support for renderable float textures got rolled into core GL
    // spec it wasn't intended to be renderable. In practice it's not reliably renderable even
    // with the extensions, there's a known bug in at least NVIDIA driver version 370.
    //                       | Format              | OpenGL texture support                                       | Filter           | OpenGL render support                                                                  | OpenGL ES texture support                                         | Filter                                                 | OpenGL ES render support                                                        |
    InsertFormatMapping(&map, GL_R11F_G11F_B10F,    VersionOrExts(3, 0, "GL_EXT_packed_float"),                    AlwaysSupported(), VersionOrExts(3, 0, "GL_EXT_packed_float GL_ARB_color_buffer_float"),                    VersionOnly(3, 0),                                                  AlwaysSupported(),                                       ExtsOnly("GL_EXT_color_buffer_float")                                            );
    InsertFormatMapping(&map, GL_RGB9_E5,           VersionOrExts(3, 0, "GL_EXT_texture_shared_exponent"),         AlwaysSupported(), NeverSupported(),                                                                        VersionOnly(3, 0),                                                  AlwaysSupported(),                                       NeverSupported()                                                                 );
    InsertFormatMapping(&map, GL_R16F,              VersionOrExts(3, 0, "GL_ARB_texture_rg ARB_texture_float"),    AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg GL_ARB_texture_float GL_ARB_color_buffer_float"), VersionOrExts(3, 0, "GL_OES_texture_half_float GL_EXT_texture_rg"), VersionOrExts(3, 0, "GL_OES_texture_half_float_linear"), VersionOrExtsAndExts(3, 0, "GL_EXT_texture_rg", "GL_EXT_color_buffer_half_float"));
    InsertFormatMapping(&map, GL_RG16F,             VersionOrExts(3, 0, "GL_ARB_texture_rg ARB_texture_float"),    AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg GL_ARB_texture_float GL_ARB_color_buffer_float"), VersionOrExts(3, 0, "GL_OES_texture_half_float GL_EXT_texture_rg"), VersionOrExts(3, 0, "GL_OES_texture_half_float_linear"), VersionOrExtsAndExts(3, 0, "GL_EXT_texture_rg", "GL_EXT_color_buffer_half_float"));
    InsertFormatMapping(&map, GL_RGB16F,            VersionOrExts(3, 0, "GL_ARB_texture_float"),                   AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_float GL_ARB_color_buffer_float"),                   VersionOrExts(3, 0, "GL_OES_texture_half_float"),                   VersionOrExts(3, 0, "GL_OES_texture_half_float_linear"), ExtsOnly("GL_EXT_color_buffer_half_float")                                       );
    InsertFormatMapping(&map, GL_RGBA16F,           VersionOrExts(3, 0, "GL_ARB_texture_float"),                   AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_float GL_ARB_color_buffer_float"),                   VersionOrExts(3, 0, "GL_OES_texture_half_float"),                   VersionOrExts(3, 0, "GL_OES_texture_half_float_linear"), ExtsOnly("GL_EXT_color_buffer_half_float")                                       );
    InsertFormatMapping(&map, GL_R32F,              VersionOrExts(3, 0, "GL_ARB_texture_rg GL_ARB_texture_float"), AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg GL_ARB_texture_float GL_ARB_color_buffer_float"), VersionOrExts(3, 0, "GL_OES_texture_float GL_EXT_texture_rg"),      ExtsOnly("GL_OES_texture_float_linear"),                 ExtsOnly("GL_EXT_color_buffer_float")                                            );
    InsertFormatMapping(&map, GL_RG32F,             VersionOrExts(3, 0, "GL_ARB_texture_rg GL_ARB_texture_float"), AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_rg GL_ARB_texture_float GL_ARB_color_buffer_float"), VersionOrExts(3, 0, "GL_OES_texture_float GL_EXT_texture_rg"),      ExtsOnly("GL_OES_texture_float_linear"),                 ExtsOnly("GL_EXT_color_buffer_float")                                            );
    InsertFormatMapping(&map, GL_RGB32F,            VersionOrExts(3, 0, "GL_ARB_texture_float"),                   AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_float GL_ARB_color_buffer_float"),                   VersionOrExts(3, 0, "GL_OES_texture_float"),                        ExtsOnly("GL_OES_texture_float_linear"),                 NeverSupported()                                                                 );
    InsertFormatMapping(&map, GL_RGBA32F,           VersionOrExts(3, 0, "GL_ARB_texture_float"),                   AlwaysSupported(), VersionOrExts(3, 0, "GL_ARB_texture_float GL_ARB_color_buffer_float"),                   VersionOrExts(3, 0, "GL_OES_texture_float"),                        ExtsOnly("GL_OES_texture_float_linear"),                 ExtsOnly("GL_EXT_color_buffer_float")                                            );

    // Depth stencil formats
    //                       | Format                  | OpenGL texture support                            | Filter                                     | OpenGL render support                             | OpenGL ES texture support                  | Filter                                     | OpenGL ES render support                                              |
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT16,     VersionOnly(1, 5),                                  VersionOrExts(1, 5, "GL_ARB_depth_texture"), VersionOnly(1, 5),                                  VersionOnly(2, 0),                           VersionOrExts(3, 0, "GL_OES_depth_texture"), VersionOnly(2, 0)                                                      );
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT24,     VersionOnly(1, 5),                                  VersionOrExts(1, 5, "GL_ARB_depth_texture"), VersionOnly(1, 5),                                  VersionOnly(2, 0),                           VersionOrExts(3, 0, "GL_OES_depth_texture"), VersionOnly(2, 0)                                                      );
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT32_OES, VersionOnly(1, 5),                                  VersionOrExts(1, 5, "GL_ARB_depth_texture"), VersionOnly(1, 5),                                  ExtsOnly("GL_OES_depth_texture"),            AlwaysSupported(),                           ExtsOnly("GL_OES_depth32")                                             );
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT32F,    VersionOrExts(3, 0, "GL_ARB_depth_buffer_float"),   AlwaysSupported(),                           VersionOrExts(3, 0, "GL_ARB_depth_buffer_float"),   VersionOnly(3, 0),                           VersionOrExts(3, 0, "GL_OES_depth_texture"), VersionOnly(3, 0)                                                      );
    InsertFormatMapping(&map, GL_STENCIL_INDEX8,        VersionOrExts(3, 0, "GL_EXT_packed_depth_stencil"), NeverSupported(),                            VersionOrExts(3, 0, "GL_EXT_packed_depth_stencil"), VersionOnly(2, 0),                           NeverSupported(),                            VersionOnly(2, 0)                                                      );
    InsertFormatMapping(&map, GL_DEPTH24_STENCIL8,      VersionOrExts(3, 0, "GL_ARB_framebuffer_object"),   VersionOrExts(3, 0, "GL_ARB_depth_texture"), VersionOrExts(3, 0, "GL_ARB_framebuffer_object"),   VersionOrExts(3, 0, "GL_OES_depth_texture"), AlwaysSupported(),                           VersionOrExts(3, 0, "GL_OES_depth_texture GL_OES_packed_depth_stencil"));
    InsertFormatMapping(&map, GL_DEPTH32F_STENCIL8,     VersionOrExts(3, 0, "GL_ARB_depth_buffer_float"),   AlwaysSupported(),                           VersionOrExts(3, 0, "GL_ARB_depth_buffer_float"),   VersionOnly(3, 0),                           AlwaysSupported(),                           VersionOnly(3, 0)                                                      );
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT,       VersionOnly(1, 5),                                  VersionOrExts(1, 5, "GL_ARB_depth_texture"), VersionOnly(1, 5),                                  VersionOnly(2, 0),                           VersionOrExts(3, 0, "GL_OES_depth_texture"), VersionOnly(2, 0)                                                      );
    InsertFormatMapping(&map, GL_DEPTH_STENCIL,         VersionOnly(1, 5),                                  VersionOrExts(1, 5, "GL_ARB_depth_texture"), VersionOnly(1, 5),                                  VersionOnly(2, 0),                           VersionOrExts(3, 0, "GL_OES_depth_texture"), VersionOnly(2, 0)                                                      );

    // Luminance alpha formats
    //                       | Format                  | OpenGL texture support                      | Filter           | Render          | OpenGL ES texture support                                | Filter                                                 | Render          |
    InsertFormatMapping(&map, GL_ALPHA8_EXT,             AlwaysSupported(),                           AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage"),                        AlwaysSupported(),                                       NeverSupported());
    InsertFormatMapping(&map, GL_LUMINANCE8_EXT,         AlwaysSupported(),                           AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage"),                        AlwaysSupported(),                                       NeverSupported());
    InsertFormatMapping(&map, GL_LUMINANCE8_ALPHA8_EXT,  AlwaysSupported(),                           AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage"),                        AlwaysSupported(),                                       NeverSupported());
    InsertFormatMapping(&map, GL_ALPHA16F_EXT,           VersionOrExts(3, 0, "GL_ARB_texture_float"), AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage OES_texture_half_float"), VersionOrExts(3, 0, "GL_OES_texture_half_float_linear"), NeverSupported());
    InsertFormatMapping(&map, GL_LUMINANCE16F_EXT,       VersionOrExts(3, 0, "GL_ARB_texture_float"), AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage OES_texture_half_float"), VersionOrExts(3, 0, "GL_OES_texture_half_float_linear"), NeverSupported());
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA16F_EXT, VersionOrExts(3, 0, "GL_ARB_texture_float"), AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage OES_texture_half_float"), VersionOrExts(3, 0, "GL_OES_texture_half_float_linear"), NeverSupported());
    InsertFormatMapping(&map, GL_ALPHA32F_EXT,           VersionOrExts(3, 0, "GL_ARB_texture_float"), AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage OES_texture_float"),      ExtsOnly("GL_OES_texture_float_linear"),                 NeverSupported());
    InsertFormatMapping(&map, GL_LUMINANCE32F_EXT,       VersionOrExts(3, 0, "GL_ARB_texture_float"), AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage OES_texture_float"),      ExtsOnly("GL_OES_texture_float_linear"),                 NeverSupported());
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA32F_EXT, VersionOrExts(3, 0, "GL_ARB_texture_float"), AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_storage OES_texture_float"),      ExtsOnly("GL_OES_texture_float_linear"),                 NeverSupported());

    // Compressed formats, From ES 3.0.1 spec, table 3.16
    InsertFormatMapping(&map, GL_COMPRESSED_R11_EAC,                        VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_SIGNED_R11_EAC,                 VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_RG11_EAC,                       VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_SIGNED_RG11_EAC,                VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_RGB8_ETC2,                      VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_SRGB8_ETC2,                     VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA8_ETC2_EAC,                 VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOnly(3, 0), AlwaysSupported(), NeverSupported());

    // From GL_EXT_texture_compression_dxt1
    //                       | Format                            | OpenGL texture support                         | Filter           | Render          | OpenGL ES texture support                    | Filter           | Render           |
    InsertFormatMapping(&map, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    ExtsOnly("GL_EXT_texture_compression_s3tc"),     AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_compression_dxt1"),   AlwaysSupported(), NeverSupported());
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   ExtsOnly("GL_EXT_texture_compression_s3tc"),     AlwaysSupported(), NeverSupported(), ExtsOnly("GL_EXT_texture_compression_dxt1"),   AlwaysSupported(), NeverSupported());

    // From GL_ANGLE_texture_compression_dxt3
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, ExtsOnly("GL_EXT_texture_compression_s3tc"),     AlwaysSupported(), NeverSupported(), ExtsOnly("GL_ANGLE_texture_compression_dxt3"), AlwaysSupported(), NeverSupported());

    // From GL_ANGLE_texture_compression_dxt5
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, ExtsOnly("GL_EXT_texture_compression_s3tc"),     AlwaysSupported(), NeverSupported(), ExtsOnly("GL_ANGLE_texture_compression_dxt5"), AlwaysSupported(), NeverSupported());

    // From GL_ETC1_RGB8_OES
    InsertFormatMapping(&map, GL_ETC1_RGB8_OES,                   VersionOrExts(4, 3, "GL_ARB_ES3_compatibility"), AlwaysSupported(), NeverSupported(), VersionOrExts(3, 0, "GL_ETC1_RGB8_OES"),       AlwaysSupported(), NeverSupported());

    // clang-format on

    return map;
}

static const InternalFormatInfoMap &GetInternalFormatMap()
{
    static const InternalFormatInfoMap formatMap = BuildInternalFormatInfoMap();
    return formatMap;
}

const InternalFormat &GetInternalFormatInfo(GLenum internalFormat, StandardGL standard)
{
    const InternalFormatInfoMap &formatMap = GetInternalFormatMap();
    InternalFormatInfoMap::const_iterator iter = formatMap.find(internalFormat);
    if (iter != formatMap.end())
    {
        const InternalFormatInfo &info = iter->second;
        switch (standard)
        {
          case STANDARD_GL_ES:      return info.glesInfo;
          case STANDARD_GL_DESKTOP: return info.glInfo;
          default: UNREACHABLE();   break;
        }
    }

    static const InternalFormat defaultInternalFormat;
    return defaultInternalFormat;
}

static GLenum GetNativeInternalFormat(const FunctionsGL *functions,
                                      const WorkaroundsGL &workarounds,
                                      const gl::InternalFormat &internalFormat)
{
    GLenum result = internalFormat.internalFormat;

    if (functions->standard == STANDARD_GL_DESKTOP)
    {
        // Use sized internal formats whenever possible to guarantee the requested precision.
        // On Desktop GL, passing an internal format of GL_RGBA will generate a GL_RGBA8 texture
        // even if the provided type is GL_FLOAT.
        result = internalFormat.sizedInternalFormat;

        if (workarounds.avoid1BitAlphaTextureFormats && internalFormat.alphaBits == 1)
        {
            // Use an 8-bit format instead
            result = GL_RGBA8;
        }

        if (workarounds.rgba4IsNotSupportedForColorRendering &&
            internalFormat.sizedInternalFormat == GL_RGBA4)
        {
            // Use an 8-bit format instead
            result = GL_RGBA8;
        }

        if (internalFormat.sizedInternalFormat == GL_RGB565 &&
            !functions->isAtLeastGL(gl::Version(4, 1)) &&
            !functions->hasGLExtension("GL_ARB_ES2_compatibility"))
        {
            // GL_RGB565 is required for basic ES2 functionality but was not added to desktop GL
            // until 4.1.
            // Work around this by using an 8-bit format instead.
            result = GL_RGB8;
        }

        if (internalFormat.sizedInternalFormat == GL_BGRA8_EXT)
        {
            // GLES accepts GL_BGRA as an internal format but desktop GL only accepts it as a type.
            // Update the internal format to GL_RGBA.
            result = GL_RGBA8;
        }

        if ((functions->profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0)
        {
            // Work around deprecated luminance alpha formats in the OpenGL core profile by backing
            // them with R or RG textures.
            if (internalFormat.format == GL_LUMINANCE || internalFormat.format == GL_ALPHA)
            {
                result = gl::GetInternalFormatInfo(GL_RED, internalFormat.type).sizedInternalFormat;
            }

            if (internalFormat.format == GL_LUMINANCE_ALPHA)
            {
                result = gl::GetInternalFormatInfo(GL_RG, internalFormat.type).sizedInternalFormat;
            }
        }
    }
    else if (functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        if (internalFormat.componentType == GL_FLOAT && !internalFormat.isLUMA())
        {
            // Use sized internal formats for floating point textures.  Extensions such as
            // EXT_color_buffer_float require the sized formats to be renderable.
            result = internalFormat.sizedInternalFormat;
        }
    }

    return result;
}

static GLenum GetNativeFormat(const FunctionsGL *functions,
                              const WorkaroundsGL &workarounds,
                              GLenum format)
{
    GLenum result = format;

    if (functions->standard == STANDARD_GL_DESKTOP)
    {
        // The ES SRGB extensions require that the provided format is GL_SRGB or SRGB_ALPHA but
        // the desktop GL extensions only accept GL_RGB or GL_RGBA.  Convert them.
        if (format == GL_SRGB)
        {
            result = GL_RGB;
        }

        if (format == GL_SRGB_ALPHA)
        {
            result = GL_RGBA;
        }

        if ((functions->profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0)
        {
            // Work around deprecated luminance alpha formats in the OpenGL core profile by backing
            // them with R or RG textures.
            if (format == GL_LUMINANCE || format == GL_ALPHA)
            {
                result = GL_RED;
            }

            if (format == GL_LUMINANCE_ALPHA)
            {
                result = GL_RG;
            }
        }
    }

    return result;
}

static GLenum GetNativeCompressedFormat(const FunctionsGL *functions,
                                        const WorkaroundsGL &workarounds,
                                        GLenum format)
{
    GLenum result = format;

    if (functions->standard == STANDARD_GL_DESKTOP)
    {
        if (format == GL_ETC1_RGB8_OES)
        {
            // GL_ETC1_RGB8_OES is not available in any desktop GL extension but the compression
            // format is forwards compatible so just use the ETC2 format.
            result = GL_COMPRESSED_RGB8_ETC2;
        }
    }

    if (functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        if (format == GL_ETC1_RGB8_OES)
        {
            // Pass GL_COMPRESSED_RGB8_ETC2 as the target format in ES3 and higher because it
            // becomes a core format.
            result = GL_COMPRESSED_RGB8_ETC2;
        }
    }

    return result;
}

static GLenum GetNativeType(const FunctionsGL *functions,
                            const WorkaroundsGL &workarounds,
                            GLenum format,
                            GLenum type)
{
    GLenum result = type;

    if (functions->standard == STANDARD_GL_DESKTOP)
    {
        if (type == GL_HALF_FLOAT_OES)
        {
            // The enums differ for the OES half float extensions and desktop GL spec.
            // Update it.
            result = GL_HALF_FLOAT;
        }
    }
    else if (functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        if (type == GL_HALF_FLOAT_OES)
        {
            switch (format)
            {
                case GL_LUMINANCE_ALPHA:
                case GL_LUMINANCE:
                case GL_ALPHA:
                    // In ES3, these formats come from EXT_texture_storage, which uses
                    // HALF_FLOAT_OES. Other formats (like RGBA) use HALF_FLOAT (non-OES) in ES3.
                    break;

                default:
                    result = GL_HALF_FLOAT;
                    break;
            }
        }
    }

    return result;
}

static GLenum GetNativeReadType(const FunctionsGL *functions,
                                const WorkaroundsGL &workarounds,
                                GLenum type)
{
    GLenum result = type;

    if (functions->standard == STANDARD_GL_DESKTOP || functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        if (type == GL_HALF_FLOAT_OES)
        {
            // The enums differ for the OES half float extensions and desktop GL spec. Update it.
            result = GL_HALF_FLOAT;
        }
    }

    return result;
}

static GLenum GetNativeReadFormat(const FunctionsGL *functions,
                                  const WorkaroundsGL &workarounds,
                                  GLenum format)
{
    GLenum result = format;
    return result;
}

TexImageFormat GetTexImageFormat(const FunctionsGL *functions,
                                 const WorkaroundsGL &workarounds,
                                 GLenum internalFormat,
                                 GLenum format,
                                 GLenum type)
{
    TexImageFormat result;
    result.internalFormat = GetNativeInternalFormat(
        functions, workarounds, gl::GetInternalFormatInfo(internalFormat, type));
    result.format = GetNativeFormat(functions, workarounds, format);
    result.type   = GetNativeType(functions, workarounds, format, type);
    return result;
}

TexSubImageFormat GetTexSubImageFormat(const FunctionsGL *functions,
                                       const WorkaroundsGL &workarounds,
                                       GLenum format,
                                       GLenum type)
{
    TexSubImageFormat result;
    result.format = GetNativeFormat(functions, workarounds, format);
    result.type   = GetNativeType(functions, workarounds, format, type);
    return result;
}

CompressedTexImageFormat GetCompressedTexImageFormat(const FunctionsGL *functions,
                                                     const WorkaroundsGL &workarounds,
                                                     GLenum internalFormat)
{
    CompressedTexImageFormat result;
    result.internalFormat = GetNativeCompressedFormat(functions, workarounds, internalFormat);
    return result;
}

CompressedTexSubImageFormat GetCompressedSubTexImageFormat(const FunctionsGL *functions,
                                                           const WorkaroundsGL &workarounds,
                                                           GLenum format)
{
    CompressedTexSubImageFormat result;
    result.format = GetNativeCompressedFormat(functions, workarounds, format);
    return result;
}

CopyTexImageImageFormat GetCopyTexImageImageFormat(const FunctionsGL *functions,
                                                   const WorkaroundsGL &workarounds,
                                                   GLenum internalFormat,
                                                   GLenum framebufferType)
{
    CopyTexImageImageFormat result;
    result.internalFormat = GetNativeInternalFormat(
        functions, workarounds, gl::GetInternalFormatInfo(internalFormat, framebufferType));
    return result;
}

TexStorageFormat GetTexStorageFormat(const FunctionsGL *functions,
                                     const WorkaroundsGL &workarounds,
                                     GLenum internalFormat)
{
    TexStorageFormat result;
    result.internalFormat = GetNativeInternalFormat(functions, workarounds,
                                                    gl::GetSizedInternalFormatInfo(internalFormat));
    return result;
}

RenderbufferFormat GetRenderbufferFormat(const FunctionsGL *functions,
                                         const WorkaroundsGL &workarounds,
                                         GLenum internalFormat)
{
    RenderbufferFormat result;
    result.internalFormat = GetNativeInternalFormat(functions, workarounds,
                                                    gl::GetSizedInternalFormatInfo(internalFormat));
    return result;
}
ReadPixelsFormat GetReadPixelsFormat(const FunctionsGL *functions,
                                     const WorkaroundsGL &workarounds,
                                     GLenum format,
                                     GLenum type)
{
    ReadPixelsFormat result;
    result.format = GetNativeReadFormat(functions, workarounds, format);
    result.type   = GetNativeReadType(functions, workarounds, type);
    return result;
}
}

}
