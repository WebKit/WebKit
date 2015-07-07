#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils.cpp: Queries for GL image formats.

#include "common/mathutil.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/imageformats.h"
#include "libGLESv2/renderer/copyimage.h"

namespace gl
{

// ES2 requires that format is equal to internal format at all glTex*Image2D entry points and the implementation
// can decide the true, sized, internal format. The ES2FormatMap determines the internal format for all valid
// format and type combinations.

struct FormatTypeInfo
{
    GLenum mInternalFormat;
    ColorWriteFunction mColorWriteFunction;

    FormatTypeInfo(GLenum internalFormat, ColorWriteFunction writeFunc)
        : mInternalFormat(internalFormat), mColorWriteFunction(writeFunc)
    { }
};

typedef std::pair<GLenum, GLenum> FormatTypePair;
typedef std::pair<FormatTypePair, FormatTypeInfo> FormatPair;
typedef std::map<FormatTypePair, FormatTypeInfo> FormatMap;

// A helper function to insert data into the format map with fewer characters.
static inline void InsertFormatMapping(FormatMap *map, GLenum format, GLenum type, GLenum internalFormat, ColorWriteFunction writeFunc)
{
    map->insert(FormatPair(FormatTypePair(format, type), FormatTypeInfo(internalFormat, writeFunc)));
}

FormatMap BuildES2FormatMap()
{
    FormatMap map;

    using namespace rx;

    //                       | Format                            | Type                             | Internal format                   | Color write function             |
    InsertFormatMapping(&map, GL_ALPHA,                           GL_UNSIGNED_BYTE,                  GL_ALPHA8_EXT,                      WriteColor<A8, GLfloat>           );
    InsertFormatMapping(&map, GL_ALPHA,                           GL_FLOAT,                          GL_ALPHA32F_EXT,                    WriteColor<A32F, GLfloat>         );
    InsertFormatMapping(&map, GL_ALPHA,                           GL_HALF_FLOAT_OES,                 GL_ALPHA16F_EXT,                    WriteColor<A16F, GLfloat>         );

    InsertFormatMapping(&map, GL_LUMINANCE,                       GL_UNSIGNED_BYTE,                  GL_LUMINANCE8_EXT,                  WriteColor<L8, GLfloat>           );
    InsertFormatMapping(&map, GL_LUMINANCE,                       GL_FLOAT,                          GL_LUMINANCE32F_EXT,                WriteColor<L32F, GLfloat>         );
    InsertFormatMapping(&map, GL_LUMINANCE,                       GL_HALF_FLOAT_OES,                 GL_LUMINANCE16F_EXT,                WriteColor<L16F, GLfloat>         );

    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA,                 GL_UNSIGNED_BYTE,                  GL_LUMINANCE8_ALPHA8_EXT,           WriteColor<L8A8, GLfloat>         );
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA,                 GL_FLOAT,                          GL_LUMINANCE_ALPHA32F_EXT,          WriteColor<L32A32F, GLfloat>      );
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA,                 GL_HALF_FLOAT_OES,                 GL_LUMINANCE_ALPHA16F_EXT,          WriteColor<L16A16F, GLfloat>      );

    InsertFormatMapping(&map, GL_RED,                             GL_UNSIGNED_BYTE,                  GL_R8_EXT,                          WriteColor<R8, GLfloat>           );
    InsertFormatMapping(&map, GL_RED,                             GL_FLOAT,                          GL_R32F_EXT,                        WriteColor<R32F, GLfloat>         );
    InsertFormatMapping(&map, GL_RED,                             GL_HALF_FLOAT_OES,                 GL_R16F_EXT,                        WriteColor<R16F, GLfloat>         );

    InsertFormatMapping(&map, GL_RG,                              GL_UNSIGNED_BYTE,                  GL_RG8_EXT,                         WriteColor<R8G8, GLfloat>         );
    InsertFormatMapping(&map, GL_RG,                              GL_FLOAT,                          GL_RG32F_EXT,                       WriteColor<R32G32F, GLfloat>      );
    InsertFormatMapping(&map, GL_RG,                              GL_HALF_FLOAT_OES,                 GL_RG16F_EXT,                       WriteColor<R16G16F, GLfloat>      );

    InsertFormatMapping(&map, GL_RGB,                             GL_UNSIGNED_BYTE,                  GL_RGB8_OES,                        WriteColor<R8G8B8, GLfloat>       );
    InsertFormatMapping(&map, GL_RGB,                             GL_UNSIGNED_SHORT_5_6_5,           GL_RGB565,                          WriteColor<R5G6B5, GLfloat>       );
    InsertFormatMapping(&map, GL_RGB,                             GL_FLOAT,                          GL_RGB32F_EXT,                      WriteColor<R32G32B32F, GLfloat>   );
    InsertFormatMapping(&map, GL_RGB,                             GL_HALF_FLOAT_OES,                 GL_RGB16F_EXT,                      WriteColor<R16G16B16F, GLfloat>   );

    InsertFormatMapping(&map, GL_RGBA,                            GL_UNSIGNED_BYTE,                  GL_RGBA8_OES,                       WriteColor<R8G8B8A8, GLfloat>     );
    InsertFormatMapping(&map, GL_RGBA,                            GL_UNSIGNED_SHORT_4_4_4_4,         GL_RGBA4,                           WriteColor<R4G4B4A4, GLfloat>     );
    InsertFormatMapping(&map, GL_RGBA,                            GL_UNSIGNED_SHORT_5_5_5_1,         GL_RGB5_A1,                         WriteColor<R5G5B5A1, GLfloat>     );
    InsertFormatMapping(&map, GL_RGBA,                            GL_FLOAT,                          GL_RGBA32F_EXT,                     WriteColor<R32G32B32A32F, GLfloat>);
    InsertFormatMapping(&map, GL_RGBA,                            GL_HALF_FLOAT_OES,                 GL_RGBA16F_EXT,                     WriteColor<R16G16B16A16F, GLfloat>);

    InsertFormatMapping(&map, GL_BGRA_EXT,                        GL_UNSIGNED_BYTE,                  GL_BGRA8_EXT,                       WriteColor<B8G8R8A8, GLfloat>     );
    InsertFormatMapping(&map, GL_BGRA_EXT,                        GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT, GL_BGRA4_ANGLEX,                    WriteColor<B4G4R4A4, GLfloat>     );
    InsertFormatMapping(&map, GL_BGRA_EXT,                        GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, GL_BGR5_A1_ANGLEX,                  WriteColor<B5G5R5A1, GLfloat>     );

    InsertFormatMapping(&map, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    GL_UNSIGNED_BYTE,                  GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    NULL                              );
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   GL_UNSIGNED_BYTE,                  GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   NULL                              );
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, GL_UNSIGNED_BYTE,                  GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, NULL                              );
    InsertFormatMapping(&map, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, GL_UNSIGNED_BYTE,                  GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, NULL                              );

    InsertFormatMapping(&map, GL_DEPTH_COMPONENT,                 GL_UNSIGNED_SHORT,                 GL_DEPTH_COMPONENT16,               NULL                              );
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT,                 GL_UNSIGNED_INT,                   GL_DEPTH_COMPONENT32_OES,           NULL                              );

    InsertFormatMapping(&map, GL_DEPTH_STENCIL_OES,               GL_UNSIGNED_INT_24_8_OES,          GL_DEPTH24_STENCIL8_OES,            NULL                              );

    return map;
}

FormatMap BuildES3FormatMap()
{
    FormatMap map;

    using namespace rx;

    //                       | Format               | Type                             | Internal format          | Color write function             |
    InsertFormatMapping(&map, GL_RGBA,               GL_UNSIGNED_BYTE,                  GL_RGBA8,                  WriteColor<R8G8B8A8, GLfloat>     );
    InsertFormatMapping(&map, GL_RGBA,               GL_BYTE,                           GL_RGBA8_SNORM,            WriteColor<R8G8B8A8S, GLfloat>    );
    InsertFormatMapping(&map, GL_RGBA,               GL_UNSIGNED_SHORT_4_4_4_4,         GL_RGBA4,                  WriteColor<R4G4B4A4, GLfloat>     );
    InsertFormatMapping(&map, GL_RGBA,               GL_UNSIGNED_SHORT_5_5_5_1,         GL_RGB5_A1,                WriteColor<R5G5B5A1, GLfloat>     );
    InsertFormatMapping(&map, GL_RGBA,               GL_UNSIGNED_INT_2_10_10_10_REV,    GL_RGB10_A2,               WriteColor<R10G10B10A2, GLfloat>  );
    InsertFormatMapping(&map, GL_RGBA,               GL_FLOAT,                          GL_RGBA32F,                WriteColor<R32G32B32A32F, GLfloat>);
    InsertFormatMapping(&map, GL_RGBA,               GL_HALF_FLOAT,                     GL_RGBA16F,                WriteColor<R16G16B16A16F, GLfloat>);

    InsertFormatMapping(&map, GL_RGBA_INTEGER,       GL_UNSIGNED_BYTE,                  GL_RGBA8UI,                WriteColor<R8G8B8A8, GLuint>      );
    InsertFormatMapping(&map, GL_RGBA_INTEGER,       GL_BYTE,                           GL_RGBA8I,                 WriteColor<R8G8B8A8S, GLint>      );
    InsertFormatMapping(&map, GL_RGBA_INTEGER,       GL_UNSIGNED_SHORT,                 GL_RGBA16UI,               WriteColor<R16G16B16A16, GLuint>  );
    InsertFormatMapping(&map, GL_RGBA_INTEGER,       GL_SHORT,                          GL_RGBA16I,                WriteColor<R16G16B16A16S, GLint>  );
    InsertFormatMapping(&map, GL_RGBA_INTEGER,       GL_UNSIGNED_INT,                   GL_RGBA32UI,               WriteColor<R32G32B32A32, GLuint>  );
    InsertFormatMapping(&map, GL_RGBA_INTEGER,       GL_INT,                            GL_RGBA32I,                WriteColor<R32G32B32A32S, GLint>  );
    InsertFormatMapping(&map, GL_RGBA_INTEGER,       GL_UNSIGNED_INT_2_10_10_10_REV,    GL_RGB10_A2UI,             WriteColor<R10G10B10A2, GLuint>   );

    InsertFormatMapping(&map, GL_RGB,                GL_UNSIGNED_BYTE,                  GL_RGB8,                   WriteColor<R8G8B8, GLfloat>       );
    InsertFormatMapping(&map, GL_RGB,                GL_BYTE,                           GL_RGB8_SNORM,             WriteColor<R8G8B8S, GLfloat>      );
    InsertFormatMapping(&map, GL_RGB,                GL_UNSIGNED_SHORT_5_6_5,           GL_RGB565,                 WriteColor<R5G6B5, GLfloat>       );
    InsertFormatMapping(&map, GL_RGB,                GL_UNSIGNED_INT_10F_11F_11F_REV,   GL_R11F_G11F_B10F,         WriteColor<R11G11B10F, GLfloat>   );
    InsertFormatMapping(&map, GL_RGB,                GL_UNSIGNED_INT_5_9_9_9_REV,       GL_RGB9_E5,                WriteColor<R9G9B9E5, GLfloat>     );
    InsertFormatMapping(&map, GL_RGB,                GL_FLOAT,                          GL_RGB32F,                 WriteColor<R32G32B32F, GLfloat>   );
    InsertFormatMapping(&map, GL_RGB,                GL_HALF_FLOAT,                     GL_RGB16F,                 WriteColor<R16G16B16F, GLfloat>   );

    InsertFormatMapping(&map, GL_RGB_INTEGER,        GL_UNSIGNED_BYTE,                  GL_RGB8UI,                 WriteColor<R8G8B8, GLuint>        );
    InsertFormatMapping(&map, GL_RGB_INTEGER,        GL_BYTE,                           GL_RGB8I,                  WriteColor<R8G8B8S, GLint>        );
    InsertFormatMapping(&map, GL_RGB_INTEGER,        GL_UNSIGNED_SHORT,                 GL_RGB16UI,                WriteColor<R16G16B16, GLuint>     );
    InsertFormatMapping(&map, GL_RGB_INTEGER,        GL_SHORT,                          GL_RGB16I,                 WriteColor<R16G16B16S, GLint>     );
    InsertFormatMapping(&map, GL_RGB_INTEGER,        GL_UNSIGNED_INT,                   GL_RGB32UI,                WriteColor<R32G32B32, GLuint>     );
    InsertFormatMapping(&map, GL_RGB_INTEGER,        GL_INT,                            GL_RGB32I,                 WriteColor<R32G32B32S, GLint>     );

    InsertFormatMapping(&map, GL_RG,                 GL_UNSIGNED_BYTE,                  GL_RG8,                    WriteColor<R8G8, GLfloat>         );
    InsertFormatMapping(&map, GL_RG,                 GL_BYTE,                           GL_RG8_SNORM,              WriteColor<R8G8S, GLfloat>        );
    InsertFormatMapping(&map, GL_RG,                 GL_FLOAT,                          GL_RG32F,                  WriteColor<R32G32F, GLfloat>      );
    InsertFormatMapping(&map, GL_RG,                 GL_HALF_FLOAT,                     GL_RG16F,                  WriteColor<R16G16F, GLfloat>      );

    InsertFormatMapping(&map, GL_RG_INTEGER,         GL_UNSIGNED_BYTE,                  GL_RG8UI,                  WriteColor<R8G8, GLuint>          );
    InsertFormatMapping(&map, GL_RG_INTEGER,         GL_BYTE,                           GL_RG8I,                   WriteColor<R8G8S, GLint>          );
    InsertFormatMapping(&map, GL_RG_INTEGER,         GL_UNSIGNED_SHORT,                 GL_RG16UI,                 WriteColor<R16G16, GLuint>        );
    InsertFormatMapping(&map, GL_RG_INTEGER,         GL_SHORT,                          GL_RG16I,                  WriteColor<R16G16S, GLint>        );
    InsertFormatMapping(&map, GL_RG_INTEGER,         GL_UNSIGNED_INT,                   GL_RG32UI,                 WriteColor<R32G32, GLuint>        );
    InsertFormatMapping(&map, GL_RG_INTEGER,         GL_INT,                            GL_RG32I,                  WriteColor<R32G32S, GLint>        );

    InsertFormatMapping(&map, GL_RED,                GL_UNSIGNED_BYTE,                  GL_R8,                     WriteColor<R8, GLfloat>           );
    InsertFormatMapping(&map, GL_RED,                GL_BYTE,                           GL_R8_SNORM,               WriteColor<R8S, GLfloat>          );
    InsertFormatMapping(&map, GL_RED,                GL_FLOAT,                          GL_R32F,                   WriteColor<R32F, GLfloat>         );
    InsertFormatMapping(&map, GL_RED,                GL_HALF_FLOAT,                     GL_R16F,                   WriteColor<R16F, GLfloat>         );

    InsertFormatMapping(&map, GL_RED_INTEGER,        GL_UNSIGNED_BYTE,                  GL_R8UI,                   WriteColor<R8, GLuint>            );
    InsertFormatMapping(&map, GL_RED_INTEGER,        GL_BYTE,                           GL_R8I,                    WriteColor<R8S, GLint>            );
    InsertFormatMapping(&map, GL_RED_INTEGER,        GL_UNSIGNED_SHORT,                 GL_R16UI,                  WriteColor<R16, GLuint>           );
    InsertFormatMapping(&map, GL_RED_INTEGER,        GL_SHORT,                          GL_R16I,                   WriteColor<R16S, GLint>           );
    InsertFormatMapping(&map, GL_RED_INTEGER,        GL_UNSIGNED_INT,                   GL_R32UI,                  WriteColor<R32, GLuint>           );
    InsertFormatMapping(&map, GL_RED_INTEGER,        GL_INT,                            GL_R32I,                   WriteColor<R32S, GLint>           );

    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA,    GL_UNSIGNED_BYTE,                  GL_LUMINANCE8_ALPHA8_EXT,  WriteColor<L8A8, GLfloat>         );
    InsertFormatMapping(&map, GL_LUMINANCE,          GL_UNSIGNED_BYTE,                  GL_LUMINANCE8_EXT,         WriteColor<L8, GLfloat>           );
    InsertFormatMapping(&map, GL_ALPHA,              GL_UNSIGNED_BYTE,                  GL_ALPHA8_EXT,             WriteColor<A8, GLfloat>           );
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA,    GL_FLOAT,                          GL_LUMINANCE_ALPHA32F_EXT, WriteColor<L32A32F, GLfloat>      );
    InsertFormatMapping(&map, GL_LUMINANCE,          GL_FLOAT,                          GL_LUMINANCE32F_EXT,       WriteColor<L32F, GLfloat>         );
    InsertFormatMapping(&map, GL_ALPHA,              GL_FLOAT,                          GL_ALPHA32F_EXT,           WriteColor<A32F, GLfloat>         );
    InsertFormatMapping(&map, GL_LUMINANCE_ALPHA,    GL_HALF_FLOAT,                     GL_LUMINANCE_ALPHA16F_EXT, WriteColor<L16A16F, GLfloat>      );
    InsertFormatMapping(&map, GL_LUMINANCE,          GL_HALF_FLOAT,                     GL_LUMINANCE16F_EXT,       WriteColor<L16F, GLfloat>         );
    InsertFormatMapping(&map, GL_ALPHA,              GL_HALF_FLOAT,                     GL_ALPHA16F_EXT,           WriteColor<A16F, GLfloat>         );

    InsertFormatMapping(&map, GL_BGRA_EXT,           GL_UNSIGNED_BYTE,                  GL_BGRA8_EXT,              WriteColor<B8G8R8A8, GLfloat>     );
    InsertFormatMapping(&map, GL_BGRA_EXT,           GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT, GL_BGRA4_ANGLEX,           WriteColor<B4G4R4A4, GLfloat>     );
    InsertFormatMapping(&map, GL_BGRA_EXT,           GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, GL_BGR5_A1_ANGLEX,         WriteColor<B5G5R5A1, GLfloat>     );

    InsertFormatMapping(&map, GL_DEPTH_COMPONENT,    GL_UNSIGNED_SHORT,                 GL_DEPTH_COMPONENT16,      NULL                              );
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT,    GL_UNSIGNED_INT,                   GL_DEPTH_COMPONENT24,      NULL                              );
    InsertFormatMapping(&map, GL_DEPTH_COMPONENT,    GL_FLOAT,                          GL_DEPTH_COMPONENT32F,     NULL                              );

    InsertFormatMapping(&map, GL_DEPTH_STENCIL,      GL_UNSIGNED_INT_24_8,              GL_DEPTH24_STENCIL8,       NULL                              );
    InsertFormatMapping(&map, GL_DEPTH_STENCIL,      GL_FLOAT_32_UNSIGNED_INT_24_8_REV, GL_DEPTH32F_STENCIL8,      NULL                              );

    return map;
}

static const FormatMap &GetFormatMap(GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        static const FormatMap formats = BuildES2FormatMap();
        return formats;
    }
    else if (clientVersion == 3)
    {
        static const FormatMap formats = BuildES3FormatMap();
        return formats;
    }
    else
    {
        UNREACHABLE();
        static FormatMap emptyMap;
        return emptyMap;
    }
}

struct FormatInfo
{
    GLenum mInternalformat;
    GLenum mFormat;
    GLenum mType;

    FormatInfo(GLenum internalformat, GLenum format, GLenum type)
        : mInternalformat(internalformat), mFormat(format), mType(type) { }

    bool operator<(const FormatInfo& other) const
    {
        return memcmp(this, &other, sizeof(FormatInfo)) < 0;
    }
};

// ES3 has a specific set of permutations of internal formats, formats and types which are acceptable.
typedef std::set<FormatInfo> ES3FormatSet;

ES3FormatSet BuildES3FormatSet()
{
    ES3FormatSet set;

    // Format combinations from ES 3.0.1 spec, table 3.2

    //                   | Internal format      | Format            | Type                            |
    //                   |                      |                   |                                 |
    set.insert(FormatInfo(GL_RGBA8,              GL_RGBA,            GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGB5_A1,            GL_RGBA,            GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGBA4,              GL_RGBA,            GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_SRGB8_ALPHA8,       GL_RGBA,            GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGBA8_SNORM,        GL_RGBA,            GL_BYTE                          ));
    set.insert(FormatInfo(GL_RGBA4,              GL_RGBA,            GL_UNSIGNED_SHORT_4_4_4_4        ));
    set.insert(FormatInfo(GL_RGB10_A2,           GL_RGBA,            GL_UNSIGNED_INT_2_10_10_10_REV   ));
    set.insert(FormatInfo(GL_RGB5_A1,            GL_RGBA,            GL_UNSIGNED_INT_2_10_10_10_REV   ));
    set.insert(FormatInfo(GL_RGB5_A1,            GL_RGBA,            GL_UNSIGNED_SHORT_5_5_5_1        ));
    set.insert(FormatInfo(GL_RGBA16F,            GL_RGBA,            GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_RGBA32F,            GL_RGBA,            GL_FLOAT                         ));
    set.insert(FormatInfo(GL_RGBA16F,            GL_RGBA,            GL_FLOAT                         ));
    set.insert(FormatInfo(GL_RGBA8UI,            GL_RGBA_INTEGER,    GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGBA8I,             GL_RGBA_INTEGER,    GL_BYTE                          ));
    set.insert(FormatInfo(GL_RGBA16UI,           GL_RGBA_INTEGER,    GL_UNSIGNED_SHORT                ));
    set.insert(FormatInfo(GL_RGBA16I,            GL_RGBA_INTEGER,    GL_SHORT                         ));
    set.insert(FormatInfo(GL_RGBA32UI,           GL_RGBA_INTEGER,    GL_UNSIGNED_INT                  ));
    set.insert(FormatInfo(GL_RGBA32I,            GL_RGBA_INTEGER,    GL_INT                           ));
    set.insert(FormatInfo(GL_RGB10_A2UI,         GL_RGBA_INTEGER,    GL_UNSIGNED_INT_2_10_10_10_REV   ));
    set.insert(FormatInfo(GL_RGB8,               GL_RGB,             GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGB565,             GL_RGB,             GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_SRGB8,              GL_RGB,             GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGB8_SNORM,         GL_RGB,             GL_BYTE                          ));
    set.insert(FormatInfo(GL_RGB565,             GL_RGB,             GL_UNSIGNED_SHORT_5_6_5          ));
    set.insert(FormatInfo(GL_R11F_G11F_B10F,     GL_RGB,             GL_UNSIGNED_INT_10F_11F_11F_REV  ));
    set.insert(FormatInfo(GL_RGB9_E5,            GL_RGB,             GL_UNSIGNED_INT_5_9_9_9_REV      ));
    set.insert(FormatInfo(GL_RGB16F,             GL_RGB,             GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_R11F_G11F_B10F,     GL_RGB,             GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_RGB9_E5,            GL_RGB,             GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_RGB32F,             GL_RGB,             GL_FLOAT                         ));
    set.insert(FormatInfo(GL_RGB16F,             GL_RGB,             GL_FLOAT                         ));
    set.insert(FormatInfo(GL_R11F_G11F_B10F,     GL_RGB,             GL_FLOAT                         ));
    set.insert(FormatInfo(GL_RGB9_E5,            GL_RGB,             GL_FLOAT                         ));
    set.insert(FormatInfo(GL_RGB8UI,             GL_RGB_INTEGER,     GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGB8I,              GL_RGB_INTEGER,     GL_BYTE                          ));
    set.insert(FormatInfo(GL_RGB16UI,            GL_RGB_INTEGER,     GL_UNSIGNED_SHORT                ));
    set.insert(FormatInfo(GL_RGB16I,             GL_RGB_INTEGER,     GL_SHORT                         ));
    set.insert(FormatInfo(GL_RGB32UI,            GL_RGB_INTEGER,     GL_UNSIGNED_INT                  ));
    set.insert(FormatInfo(GL_RGB32I,             GL_RGB_INTEGER,     GL_INT                           ));
    set.insert(FormatInfo(GL_RG8,                GL_RG,              GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RG8_SNORM,          GL_RG,              GL_BYTE                          ));
    set.insert(FormatInfo(GL_RG16F,              GL_RG,              GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_RG32F,              GL_RG,              GL_FLOAT                         ));
    set.insert(FormatInfo(GL_RG16F,              GL_RG,              GL_FLOAT                         ));
    set.insert(FormatInfo(GL_RG8UI,              GL_RG_INTEGER,      GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RG8I,               GL_RG_INTEGER,      GL_BYTE                          ));
    set.insert(FormatInfo(GL_RG16UI,             GL_RG_INTEGER,      GL_UNSIGNED_SHORT                ));
    set.insert(FormatInfo(GL_RG16I,              GL_RG_INTEGER,      GL_SHORT                         ));
    set.insert(FormatInfo(GL_RG32UI,             GL_RG_INTEGER,      GL_UNSIGNED_INT                  ));
    set.insert(FormatInfo(GL_RG32I,              GL_RG_INTEGER,      GL_INT                           ));
    set.insert(FormatInfo(GL_R8,                 GL_RED,             GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_R8_SNORM,           GL_RED,             GL_BYTE                          ));
    set.insert(FormatInfo(GL_R16F,               GL_RED,             GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_R32F,               GL_RED,             GL_FLOAT                         ));
    set.insert(FormatInfo(GL_R16F,               GL_RED,             GL_FLOAT                         ));
    set.insert(FormatInfo(GL_R8UI,               GL_RED_INTEGER,     GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_R8I,                GL_RED_INTEGER,     GL_BYTE                          ));
    set.insert(FormatInfo(GL_R16UI,              GL_RED_INTEGER,     GL_UNSIGNED_SHORT                ));
    set.insert(FormatInfo(GL_R16I,               GL_RED_INTEGER,     GL_SHORT                         ));
    set.insert(FormatInfo(GL_R32UI,              GL_RED_INTEGER,     GL_UNSIGNED_INT                  ));
    set.insert(FormatInfo(GL_R32I,               GL_RED_INTEGER,     GL_INT                           ));

    // Unsized formats
    set.insert(FormatInfo(GL_RGBA,               GL_RGBA,            GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGBA,               GL_RGBA,            GL_UNSIGNED_SHORT_4_4_4_4        ));
    set.insert(FormatInfo(GL_RGBA,               GL_RGBA,            GL_UNSIGNED_SHORT_5_5_5_1        ));
    set.insert(FormatInfo(GL_RGB,                GL_RGB,             GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_RGB,                GL_RGB,             GL_UNSIGNED_SHORT_5_6_5          ));
    set.insert(FormatInfo(GL_LUMINANCE_ALPHA,    GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_LUMINANCE,          GL_LUMINANCE,       GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_ALPHA,              GL_ALPHA,           GL_UNSIGNED_BYTE                 ));

    // Depth stencil formats
    set.insert(FormatInfo(GL_DEPTH_COMPONENT16,  GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT                ));
    set.insert(FormatInfo(GL_DEPTH_COMPONENT24,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT                  ));
    set.insert(FormatInfo(GL_DEPTH_COMPONENT16,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT                  ));
    set.insert(FormatInfo(GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT                         ));
    set.insert(FormatInfo(GL_DEPTH24_STENCIL8,   GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8             ));
    set.insert(FormatInfo(GL_DEPTH32F_STENCIL8,  GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV));

    // From GL_OES_texture_float
    set.insert(FormatInfo(GL_LUMINANCE_ALPHA,    GL_LUMINANCE_ALPHA, GL_FLOAT                         ));
    set.insert(FormatInfo(GL_LUMINANCE,          GL_LUMINANCE,       GL_FLOAT                         ));
    set.insert(FormatInfo(GL_ALPHA,              GL_ALPHA,           GL_FLOAT                         ));

    // From GL_OES_texture_half_float
    set.insert(FormatInfo(GL_LUMINANCE_ALPHA,    GL_LUMINANCE_ALPHA, GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_LUMINANCE,          GL_LUMINANCE,       GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_ALPHA,              GL_ALPHA,           GL_HALF_FLOAT                    ));

    // From GL_EXT_texture_format_BGRA8888
    set.insert(FormatInfo(GL_BGRA_EXT,           GL_BGRA_EXT,        GL_UNSIGNED_BYTE                 ));

    // From GL_EXT_texture_storage
    //                   | Internal format          | Format            | Type                            |
    //                   |                          |                   |                                 |
    set.insert(FormatInfo(GL_ALPHA8_EXT,             GL_ALPHA,           GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_LUMINANCE8_EXT,         GL_LUMINANCE,       GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_LUMINANCE8_ALPHA8_EXT,  GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_ALPHA32F_EXT,           GL_ALPHA,           GL_FLOAT                         ));
    set.insert(FormatInfo(GL_LUMINANCE32F_EXT,       GL_LUMINANCE,       GL_FLOAT                         ));
    set.insert(FormatInfo(GL_LUMINANCE_ALPHA32F_EXT, GL_LUMINANCE_ALPHA, GL_FLOAT                         ));
    set.insert(FormatInfo(GL_ALPHA16F_EXT,           GL_ALPHA,           GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_LUMINANCE16F_EXT,       GL_LUMINANCE,       GL_HALF_FLOAT                    ));
    set.insert(FormatInfo(GL_LUMINANCE_ALPHA16F_EXT, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT                    ));

    // From GL_EXT_texture_storage and GL_EXT_texture_format_BGRA8888
    set.insert(FormatInfo(GL_BGRA8_EXT,              GL_BGRA_EXT,        GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_BGRA4_ANGLEX,           GL_BGRA_EXT,        GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT));
    set.insert(FormatInfo(GL_BGRA4_ANGLEX,           GL_BGRA_EXT,        GL_UNSIGNED_BYTE                 ));
    set.insert(FormatInfo(GL_BGR5_A1_ANGLEX,         GL_BGRA_EXT,        GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT));
    set.insert(FormatInfo(GL_BGR5_A1_ANGLEX,         GL_BGRA_EXT,        GL_UNSIGNED_BYTE                 ));

    // From GL_ANGLE_depth_texture
    set.insert(FormatInfo(GL_DEPTH_COMPONENT32_OES,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT_24_8_OES         ));

    // Compressed formats
    // From ES 3.0.1 spec, table 3.16
    //                   | Internal format                             | Format                                      | Type           |
    //                   |                                             |                                             |                |
    set.insert(FormatInfo(GL_COMPRESSED_R11_EAC,                        GL_COMPRESSED_R11_EAC,                        GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_R11_EAC,                        GL_COMPRESSED_R11_EAC,                        GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_SIGNED_R11_EAC,                 GL_COMPRESSED_SIGNED_R11_EAC,                 GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_RG11_EAC,                       GL_COMPRESSED_RG11_EAC,                       GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_SIGNED_RG11_EAC,                GL_COMPRESSED_SIGNED_RG11_EAC,                GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_RGB8_ETC2,                      GL_COMPRESSED_RGB8_ETC2,                      GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_SRGB8_ETC2,                     GL_COMPRESSED_SRGB8_ETC2,                     GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_RGBA8_ETC2_EAC,                 GL_COMPRESSED_RGBA8_ETC2_EAC,                 GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          GL_UNSIGNED_BYTE));


    // From GL_EXT_texture_compression_dxt1
    set.insert(FormatInfo(GL_COMPRESSED_RGB_S3TC_DXT1_EXT,              GL_COMPRESSED_RGB_S3TC_DXT1_EXT,              GL_UNSIGNED_BYTE));
    set.insert(FormatInfo(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,             GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,             GL_UNSIGNED_BYTE));

    // From GL_ANGLE_texture_compression_dxt3
    set.insert(FormatInfo(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE,           GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE,           GL_UNSIGNED_BYTE));

    // From GL_ANGLE_texture_compression_dxt5
    set.insert(FormatInfo(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE,           GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE,           GL_UNSIGNED_BYTE));

    return set;
}

static const ES3FormatSet &GetES3FormatSet()
{
    static const ES3FormatSet es3FormatSet = BuildES3FormatSet();
    return es3FormatSet;
}

// Map of sizes of input types
struct TypeInfo
{
    GLuint mTypeBytes;
    bool mSpecialInterpretation;

    TypeInfo()
        : mTypeBytes(0), mSpecialInterpretation(false) { }

    TypeInfo(GLuint typeBytes, bool specialInterpretation)
        : mTypeBytes(typeBytes), mSpecialInterpretation(specialInterpretation) { }

    bool operator<(const TypeInfo& other) const
    {
        return memcmp(this, &other, sizeof(TypeInfo)) < 0;
    }
};

typedef std::pair<GLenum, TypeInfo> TypeInfoPair;
typedef std::map<GLenum, TypeInfo> TypeInfoMap;

static TypeInfoMap BuildTypeInfoMap()
{
    TypeInfoMap map;

    map.insert(TypeInfoPair(GL_UNSIGNED_BYTE,                  TypeInfo( 1, false)));
    map.insert(TypeInfoPair(GL_BYTE,                           TypeInfo( 1, false)));
    map.insert(TypeInfoPair(GL_UNSIGNED_SHORT,                 TypeInfo( 2, false)));
    map.insert(TypeInfoPair(GL_SHORT,                          TypeInfo( 2, false)));
    map.insert(TypeInfoPair(GL_UNSIGNED_INT,                   TypeInfo( 4, false)));
    map.insert(TypeInfoPair(GL_INT,                            TypeInfo( 4, false)));
    map.insert(TypeInfoPair(GL_HALF_FLOAT,                     TypeInfo( 2, false)));
    map.insert(TypeInfoPair(GL_HALF_FLOAT_OES,                 TypeInfo( 2, false)));
    map.insert(TypeInfoPair(GL_FLOAT,                          TypeInfo( 4, false)));
    map.insert(TypeInfoPair(GL_UNSIGNED_SHORT_5_6_5,           TypeInfo( 2, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_SHORT_4_4_4_4,         TypeInfo( 2, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_SHORT_5_5_5_1,         TypeInfo( 2, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT, TypeInfo( 2, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, TypeInfo( 2, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_INT_2_10_10_10_REV,    TypeInfo( 4, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_INT_24_8,              TypeInfo( 4, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_INT_10F_11F_11F_REV,   TypeInfo( 4, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_INT_5_9_9_9_REV,       TypeInfo( 4, true )));
    map.insert(TypeInfoPair(GL_UNSIGNED_INT_24_8_OES,          TypeInfo( 4, true )));
    map.insert(TypeInfoPair(GL_FLOAT_32_UNSIGNED_INT_24_8_REV, TypeInfo( 8, true )));

    return map;
}

static bool GetTypeInfo(GLenum type, TypeInfo *outTypeInfo)
{
    static const TypeInfoMap infoMap = BuildTypeInfoMap();
    TypeInfoMap::const_iterator iter = infoMap.find(type);
    if (iter != infoMap.end())
    {
        if (outTypeInfo)
        {
            *outTypeInfo = iter->second;
        }
        return true;
    }
    else
    {
        return false;
    }
}

// Information about internal formats
typedef bool ((Context::*ContextSupportCheckMemberFunction)(void) const);
typedef bool (*ContextSupportCheckFunction)(const Context *context);

typedef bool ((rx::Renderer::*RendererSupportCheckMemberFunction)(void) const);
typedef bool (*ContextRendererSupportCheckFunction)(const Context *context, const rx::Renderer *renderer);

template <ContextSupportCheckMemberFunction func>
bool CheckSupport(const Context *context)
{
    return (context->*func)();
}

template <ContextSupportCheckMemberFunction contextFunc, RendererSupportCheckMemberFunction rendererFunc>
bool CheckSupport(const Context *context, const rx::Renderer *renderer)
{
    if (context)
    {
        return (context->*contextFunc)();
    }
    else if (renderer)
    {
        return (renderer->*rendererFunc)();
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

template <typename objectType>
bool AlwaysSupported(const objectType*)
{
    return true;
}

template <typename objectTypeA, typename objectTypeB>
bool AlwaysSupported(const objectTypeA*, const objectTypeB*)
{
    return true;
}

template <typename objectType>
bool NeverSupported(const objectType*)
{
    return false;
}

template <typename objectTypeA, typename objectTypeB>
bool NeverSupported(const objectTypeA *, const objectTypeB *)
{
    return false;
}

template <typename objectType>
bool UnimplementedSupport(const objectType*)
{
    UNIMPLEMENTED();
    return false;
}

template <typename objectTypeA, typename objectTypeB>
bool UnimplementedSupport(const objectTypeA*, const objectTypeB*)
{
    UNIMPLEMENTED();
    return false;
}

struct InternalFormatInfo
{
    GLuint mRedBits;
    GLuint mGreenBits;
    GLuint mBlueBits;

    GLuint mLuminanceBits;

    GLuint mAlphaBits;
    GLuint mSharedBits;

    GLuint mDepthBits;
    GLuint mStencilBits;

    GLuint mPixelBits;

    GLuint mComponentCount;

    GLuint mCompressedBlockWidth;
    GLuint mCompressedBlockHeight;

    GLenum mFormat;
    GLenum mType;

    GLenum mComponentType;
    GLenum mColorEncoding;

    bool mIsCompressed;

    ContextRendererSupportCheckFunction mIsColorRenderable;
    ContextRendererSupportCheckFunction mIsDepthRenderable;
    ContextRendererSupportCheckFunction mIsStencilRenderable;
    ContextRendererSupportCheckFunction mIsTextureFilterable;

    ContextSupportCheckFunction mSupportFunction;

    InternalFormatInfo() : mRedBits(0), mGreenBits(0), mBlueBits(0), mLuminanceBits(0), mAlphaBits(0), mSharedBits(0), mDepthBits(0), mStencilBits(0),
                           mPixelBits(0), mComponentCount(0), mCompressedBlockWidth(0), mCompressedBlockHeight(0), mFormat(GL_NONE), mType(GL_NONE),
                           mComponentType(GL_NONE), mColorEncoding(GL_NONE), mIsCompressed(false), mIsColorRenderable(NeverSupported),
                           mIsDepthRenderable(NeverSupported), mIsStencilRenderable(NeverSupported), mIsTextureFilterable(NeverSupported),
                           mSupportFunction(NeverSupported)
    {
    }

    static InternalFormatInfo UnsizedFormat(GLenum format, ContextSupportCheckFunction supportFunction)
    {
        InternalFormatInfo formatInfo;
        formatInfo.mFormat = format;
        formatInfo.mSupportFunction = supportFunction;

        if (format == GL_RGB || format == GL_RGBA)
            formatInfo.mIsColorRenderable = AlwaysSupported;

        return formatInfo;
    }

    static InternalFormatInfo RGBAFormat(GLuint red, GLuint green, GLuint blue, GLuint alpha, GLuint shared,
                                         GLenum format, GLenum type, GLenum componentType, bool srgb,
                                         ContextRendererSupportCheckFunction colorRenderable,
                                         ContextRendererSupportCheckFunction textureFilterable,
                                         ContextSupportCheckFunction supportFunction)
    {
        InternalFormatInfo formatInfo;
        formatInfo.mRedBits = red;
        formatInfo.mGreenBits = green;
        formatInfo.mBlueBits = blue;
        formatInfo.mAlphaBits = alpha;
        formatInfo.mSharedBits = shared;
        formatInfo.mPixelBits = red + green + blue + alpha + shared;
        formatInfo.mComponentCount = ((red > 0) ? 1 : 0) + ((green > 0) ? 1 : 0) + ((blue > 0) ? 1 : 0) + ((alpha > 0) ? 1 : 0);
        formatInfo.mFormat = format;
        formatInfo.mType = type;
        formatInfo.mComponentType = componentType;
        formatInfo.mColorEncoding = (srgb ? GL_SRGB : GL_LINEAR);
        formatInfo.mIsColorRenderable = colorRenderable;
        formatInfo.mIsTextureFilterable = textureFilterable;
        formatInfo.mSupportFunction = supportFunction;
        return formatInfo;
    }

    static InternalFormatInfo LUMAFormat(GLuint luminance, GLuint alpha, GLenum format, GLenum type, GLenum componentType,
                                         ContextSupportCheckFunction supportFunction)
    {
        InternalFormatInfo formatInfo;
        formatInfo.mLuminanceBits = luminance;
        formatInfo.mAlphaBits = alpha;
        formatInfo.mPixelBits = luminance + alpha;
        formatInfo.mComponentCount = ((luminance > 0) ? 1 : 0) + ((alpha > 0) ? 1 : 0);
        formatInfo.mFormat = format;
        formatInfo.mType = type;
        formatInfo.mComponentType = componentType;
        formatInfo.mColorEncoding = GL_LINEAR;
        formatInfo.mIsTextureFilterable = AlwaysSupported;
        formatInfo.mSupportFunction = supportFunction;
        return formatInfo;
    }

    static InternalFormatInfo DepthStencilFormat(GLuint depthBits, GLuint stencilBits, GLuint unusedBits, GLenum format,
                                                 GLenum type, GLenum componentType,
                                                 ContextRendererSupportCheckFunction depthRenderable,
                                                 ContextRendererSupportCheckFunction stencilRenderable,
                                                 ContextSupportCheckFunction supportFunction)
    {
        InternalFormatInfo formatInfo;
        formatInfo.mDepthBits = depthBits;
        formatInfo.mStencilBits = stencilBits;
        formatInfo.mPixelBits = depthBits + stencilBits + unusedBits;
        formatInfo.mComponentCount = ((depthBits > 0) ? 1 : 0) + ((stencilBits > 0) ? 1 : 0);
        formatInfo.mFormat = format;
        formatInfo.mType = type;
        formatInfo.mComponentType = componentType;
        formatInfo.mColorEncoding = GL_LINEAR;
        formatInfo.mIsDepthRenderable = depthRenderable;
        formatInfo.mIsStencilRenderable = stencilRenderable;
        formatInfo.mIsTextureFilterable = AlwaysSupported;
        formatInfo.mSupportFunction = supportFunction;
        return formatInfo;
    }

    static InternalFormatInfo CompressedFormat(GLuint compressedBlockWidth, GLuint compressedBlockHeight, GLuint compressedBlockSize,
                                               GLuint componentCount, GLenum format, GLenum type, bool srgb,
                                               ContextSupportCheckFunction supportFunction)
    {
        InternalFormatInfo formatInfo;
        formatInfo.mCompressedBlockWidth = compressedBlockWidth;
        formatInfo.mCompressedBlockHeight = compressedBlockHeight;
        formatInfo.mPixelBits = compressedBlockSize;
        formatInfo.mComponentCount = componentCount;
        formatInfo.mFormat = format;
        formatInfo.mType = type;
        formatInfo.mComponentType = GL_UNSIGNED_NORMALIZED;
        formatInfo.mColorEncoding = (srgb ? GL_SRGB : GL_LINEAR);
        formatInfo.mIsCompressed = true;
        formatInfo.mIsTextureFilterable = AlwaysSupported;
        formatInfo.mSupportFunction = supportFunction;
        return formatInfo;
    }
};

typedef std::pair<GLenum, InternalFormatInfo> InternalFormatInfoPair;
typedef std::map<GLenum, InternalFormatInfo> InternalFormatInfoMap;

static InternalFormatInfoMap BuildES3InternalFormatInfoMap()
{
    InternalFormatInfoMap map;

    // From ES 3.0.1 spec, table 3.12
    map.insert(InternalFormatInfoPair(GL_NONE,              InternalFormatInfo()));

    //                               | Internal format     |                              | R | G | B | A |S | Format         | Type                           | Component type        | SRGB | Color          | Texture        | Supported          |
    //                               |                     |                              |   |   |   |   |  |                |                                |                       |      | renderable     | filterable     |                    |
    map.insert(InternalFormatInfoPair(GL_R8,                InternalFormatInfo::RGBAFormat( 8,  0,  0,  0, 0, GL_RED,          GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R8_SNORM,          InternalFormatInfo::RGBAFormat( 8,  0,  0,  0, 0, GL_RED,          GL_BYTE,                         GL_SIGNED_NORMALIZED,   false, NeverSupported,  AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG8,               InternalFormatInfo::RGBAFormat( 8,  8,  0,  0, 0, GL_RG,           GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG8_SNORM,         InternalFormatInfo::RGBAFormat( 8,  8,  0,  0, 0, GL_RG,           GL_BYTE,                         GL_SIGNED_NORMALIZED,   false, NeverSupported,  AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB8,              InternalFormatInfo::RGBAFormat( 8,  8,  8,  0, 0, GL_RGB,          GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB8_SNORM,        InternalFormatInfo::RGBAFormat( 8,  8,  8,  0, 0, GL_RGB,          GL_BYTE,                         GL_SIGNED_NORMALIZED,   false, NeverSupported,  AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB565,            InternalFormatInfo::RGBAFormat( 5,  6,  5,  0, 0, GL_RGB,          GL_UNSIGNED_SHORT_5_6_5,         GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA4,             InternalFormatInfo::RGBAFormat( 4,  4,  4,  4, 0, GL_RGBA,         GL_UNSIGNED_SHORT_4_4_4_4,       GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB5_A1,           InternalFormatInfo::RGBAFormat( 5,  5,  5,  1, 0, GL_RGBA,         GL_UNSIGNED_SHORT_5_5_5_1,       GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA8,             InternalFormatInfo::RGBAFormat( 8,  8,  8,  8, 0, GL_RGBA,         GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA8_SNORM,       InternalFormatInfo::RGBAFormat( 8,  8,  8,  8, 0, GL_RGBA,         GL_BYTE,                         GL_SIGNED_NORMALIZED,   false, NeverSupported,  AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB10_A2,          InternalFormatInfo::RGBAFormat(10, 10, 10,  2, 0, GL_RGBA,         GL_UNSIGNED_INT_2_10_10_10_REV,  GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB10_A2UI,        InternalFormatInfo::RGBAFormat(10, 10, 10,  2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV,  GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_SRGB8,             InternalFormatInfo::RGBAFormat( 8,  8,  8,  0, 0, GL_RGB,          GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, true,  NeverSupported,  AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_SRGB8_ALPHA8,      InternalFormatInfo::RGBAFormat( 8,  8,  8,  8, 0, GL_RGBA,         GL_UNSIGNED_BYTE,                GL_UNSIGNED_NORMALIZED, true,  AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R11F_G11F_B10F,    InternalFormatInfo::RGBAFormat(11, 11, 10,  0, 0, GL_RGB,          GL_UNSIGNED_INT_10F_11F_11F_REV, GL_FLOAT,               false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB9_E5,           InternalFormatInfo::RGBAFormat( 9,  9,  9,  0, 5, GL_RGB,          GL_UNSIGNED_INT_5_9_9_9_REV,     GL_FLOAT,               false, NeverSupported,  AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R8I,               InternalFormatInfo::RGBAFormat( 8,  0,  0,  0, 0, GL_RED_INTEGER,  GL_BYTE,                         GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R8UI,              InternalFormatInfo::RGBAFormat( 8,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_BYTE,                GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R16I,              InternalFormatInfo::RGBAFormat(16,  0,  0,  0, 0, GL_RED_INTEGER,  GL_SHORT,                        GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R16UI,             InternalFormatInfo::RGBAFormat(16,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_SHORT,               GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R32I,              InternalFormatInfo::RGBAFormat(32,  0,  0,  0, 0, GL_RED_INTEGER,  GL_INT,                          GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R32UI,             InternalFormatInfo::RGBAFormat(32,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_INT,                 GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG8I,              InternalFormatInfo::RGBAFormat( 8,  8,  0,  0, 0, GL_RG_INTEGER,   GL_BYTE,                         GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG8UI,             InternalFormatInfo::RGBAFormat( 8,  8,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_BYTE,                GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG16I,             InternalFormatInfo::RGBAFormat(16, 16,  0,  0, 0, GL_RG_INTEGER,   GL_SHORT,                        GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG16UI,            InternalFormatInfo::RGBAFormat(16, 16,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_SHORT,               GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG32I,             InternalFormatInfo::RGBAFormat(32, 32,  0,  0, 0, GL_RG_INTEGER,   GL_INT,                          GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG32UI,            InternalFormatInfo::RGBAFormat(32, 32,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_INT,                 GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB8I,             InternalFormatInfo::RGBAFormat( 8,  8,  8,  0, 0, GL_RGB_INTEGER,  GL_BYTE,                         GL_INT,                 false, NeverSupported,  NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB8UI,            InternalFormatInfo::RGBAFormat( 8,  8,  8,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_BYTE,                GL_UNSIGNED_INT,        false, NeverSupported,  NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB16I,            InternalFormatInfo::RGBAFormat(16, 16, 16,  0, 0, GL_RGB_INTEGER,  GL_SHORT,                        GL_INT,                 false, NeverSupported,  NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB16UI,           InternalFormatInfo::RGBAFormat(16, 16, 16,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_SHORT,               GL_UNSIGNED_INT,        false, NeverSupported,  NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB32I,            InternalFormatInfo::RGBAFormat(32, 32, 32,  0, 0, GL_RGB_INTEGER,  GL_INT,                          GL_INT,                 false, NeverSupported,  NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB32UI,           InternalFormatInfo::RGBAFormat(32, 32, 32,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_INT,                 GL_UNSIGNED_INT,        false, NeverSupported,  NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA8I,            InternalFormatInfo::RGBAFormat( 8,  8,  8,  8, 0, GL_RGBA_INTEGER, GL_BYTE,                         GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA8UI,           InternalFormatInfo::RGBAFormat( 8,  8,  8,  8, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,                GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA16I,           InternalFormatInfo::RGBAFormat(16, 16, 16, 16, 0, GL_RGBA_INTEGER, GL_SHORT,                        GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA16UI,          InternalFormatInfo::RGBAFormat(16, 16, 16, 16, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT,               GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA32I,           InternalFormatInfo::RGBAFormat(32, 32, 32, 32, 0, GL_RGBA_INTEGER, GL_INT,                          GL_INT,                 false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA32UI,          InternalFormatInfo::RGBAFormat(32, 32, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT,                 GL_UNSIGNED_INT,        false, AlwaysSupported, NeverSupported,  AlwaysSupported     )));

    map.insert(InternalFormatInfoPair(GL_BGRA8_EXT,         InternalFormatInfo::RGBAFormat( 8,  8,  8,  8, 0, GL_BGRA_EXT,     GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_BGRA4_ANGLEX,      InternalFormatInfo::RGBAFormat( 4,  4,  4,  4, 0, GL_BGRA_EXT,     GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT, GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_BGR5_A1_ANGLEX,    InternalFormatInfo::RGBAFormat( 5,  5,  5,  1, 0, GL_BGRA_EXT,     GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported     )));

    // Floating point renderability and filtering is provided by OES_texture_float and OES_texture_half_float
    //                               | Internal format        |                                   | D |S | Format             | Type                           | Comp   | SRGB | Color renderable                                                                                           | Texture filterable                                                                                    | Supported          |
    //                               |                        |                                   |   |  |                    |                                | type   |      |                                                                                                            |                                                                                                       |                    |
    map.insert(InternalFormatInfoPair(GL_R16F,              InternalFormatInfo::RGBAFormat(16,  0,  0,  0, 0, GL_RED,          GL_HALF_FLOAT,                   GL_FLOAT, false, CheckSupport<&Context::supportsFloat16RenderableTextures, &rx::Renderer::getFloat16TextureRenderingSupport>, CheckSupport<&Context::supportsFloat16LinearFilter, &rx::Renderer::getFloat16TextureFilteringSupport>, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG16F,             InternalFormatInfo::RGBAFormat(16, 16,  0,  0, 0, GL_RG,           GL_HALF_FLOAT,                   GL_FLOAT, false, CheckSupport<&Context::supportsFloat16RenderableTextures, &rx::Renderer::getFloat16TextureRenderingSupport>, CheckSupport<&Context::supportsFloat16LinearFilter, &rx::Renderer::getFloat16TextureFilteringSupport>, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB16F,            InternalFormatInfo::RGBAFormat(16, 16, 16,  0, 0, GL_RGB,          GL_HALF_FLOAT,                   GL_FLOAT, false, CheckSupport<&Context::supportsFloat16RenderableTextures, &rx::Renderer::getFloat16TextureRenderingSupport>, CheckSupport<&Context::supportsFloat16LinearFilter, &rx::Renderer::getFloat16TextureFilteringSupport>, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA16F,           InternalFormatInfo::RGBAFormat(16, 16, 16, 16, 0, GL_RGBA,         GL_HALF_FLOAT,                   GL_FLOAT, false, CheckSupport<&Context::supportsFloat16RenderableTextures, &rx::Renderer::getFloat16TextureRenderingSupport>, CheckSupport<&Context::supportsFloat16LinearFilter, &rx::Renderer::getFloat16TextureFilteringSupport>, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_R32F,              InternalFormatInfo::RGBAFormat(32,  0,  0,  0, 0, GL_RED,          GL_FLOAT,                        GL_FLOAT, false, CheckSupport<&Context::supportsFloat32RenderableTextures, &rx::Renderer::getFloat32TextureRenderingSupport>, CheckSupport<&Context::supportsFloat32LinearFilter, &rx::Renderer::getFloat32TextureFilteringSupport>, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RG32F,             InternalFormatInfo::RGBAFormat(32, 32,  0,  0, 0, GL_RG,           GL_FLOAT,                        GL_FLOAT, false, CheckSupport<&Context::supportsFloat32RenderableTextures, &rx::Renderer::getFloat32TextureRenderingSupport>, CheckSupport<&Context::supportsFloat32LinearFilter, &rx::Renderer::getFloat32TextureFilteringSupport>, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGB32F,            InternalFormatInfo::RGBAFormat(32, 32, 32,  0, 0, GL_RGB,          GL_FLOAT,                        GL_FLOAT, false, CheckSupport<&Context::supportsFloat32RenderableTextures, &rx::Renderer::getFloat32TextureRenderingSupport>, CheckSupport<&Context::supportsFloat32LinearFilter, &rx::Renderer::getFloat32TextureFilteringSupport>, AlwaysSupported     )));
    map.insert(InternalFormatInfoPair(GL_RGBA32F,           InternalFormatInfo::RGBAFormat(32, 32, 32, 32, 0, GL_RGBA,         GL_FLOAT,                        GL_FLOAT, false, CheckSupport<&Context::supportsFloat32RenderableTextures, &rx::Renderer::getFloat32TextureRenderingSupport>, CheckSupport<&Context::supportsFloat32LinearFilter, &rx::Renderer::getFloat32TextureFilteringSupport>, AlwaysSupported     )));

    // Depth stencil formats
    //                               | Internal format         |                                      | D |S | X | Format            | Type                             | Component type        | Depth          | Stencil        | Supported     |
    //                               |                         |                                      |   |  |   |                   |                                  |                       | renderable     | renderable     |               |
    map.insert(InternalFormatInfoPair(GL_DEPTH_COMPONENT16,     InternalFormatInfo::DepthStencilFormat(16, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,                 GL_UNSIGNED_NORMALIZED, AlwaysSupported, NeverSupported,  AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_DEPTH_COMPONENT24,     InternalFormatInfo::DepthStencilFormat(24, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,                   GL_UNSIGNED_NORMALIZED, AlwaysSupported, NeverSupported,  AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_DEPTH_COMPONENT32F,    InternalFormatInfo::DepthStencilFormat(32, 0,  0, GL_DEPTH_COMPONENT, GL_FLOAT,                          GL_FLOAT,               AlwaysSupported, NeverSupported,  AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_DEPTH_COMPONENT32_OES, InternalFormatInfo::DepthStencilFormat(32, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,                   GL_UNSIGNED_NORMALIZED, AlwaysSupported, NeverSupported,  AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_DEPTH24_STENCIL8,      InternalFormatInfo::DepthStencilFormat(24, 8,  0, GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8,              GL_UNSIGNED_NORMALIZED, AlwaysSupported, AlwaysSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_DEPTH32F_STENCIL8,     InternalFormatInfo::DepthStencilFormat(32, 8, 24, GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV, GL_FLOAT,               AlwaysSupported, AlwaysSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_STENCIL_INDEX8,        InternalFormatInfo::DepthStencilFormat( 0, 8,  0, GL_DEPTH_STENCIL,   GL_UNSIGNED_BYTE,                  GL_UNSIGNED_INT,        NeverSupported,  AlwaysSupported, AlwaysSupported)));

    // Luminance alpha formats
    //                               | Internal format          |                              | L | A | Format            | Type            | Component type        | Supported     |
    map.insert(InternalFormatInfoPair(GL_ALPHA8_EXT,             InternalFormatInfo::LUMAFormat( 0,  8, GL_ALPHA,           GL_UNSIGNED_BYTE, GL_UNSIGNED_NORMALIZED, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE8_EXT,         InternalFormatInfo::LUMAFormat( 8,  0, GL_LUMINANCE,       GL_UNSIGNED_BYTE, GL_UNSIGNED_NORMALIZED, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_ALPHA32F_EXT,           InternalFormatInfo::LUMAFormat( 0, 32, GL_ALPHA,           GL_FLOAT,         GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE32F_EXT,       InternalFormatInfo::LUMAFormat(32,  0, GL_LUMINANCE,       GL_FLOAT,         GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_ALPHA16F_EXT,           InternalFormatInfo::LUMAFormat( 0, 16, GL_ALPHA,           GL_HALF_FLOAT,    GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE16F_EXT,       InternalFormatInfo::LUMAFormat(16,  0, GL_LUMINANCE,       GL_HALF_FLOAT,    GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE8_ALPHA8_EXT,  InternalFormatInfo::LUMAFormat( 8,  8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, GL_UNSIGNED_NORMALIZED, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE_ALPHA32F_EXT, InternalFormatInfo::LUMAFormat(32, 32, GL_LUMINANCE_ALPHA, GL_FLOAT,         GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE_ALPHA16F_EXT, InternalFormatInfo::LUMAFormat(16, 16, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT,    GL_FLOAT,               AlwaysSupported)));

    // Unsized formats
    //                               | Internal format   |                                 | Format            | Supported     |
    map.insert(InternalFormatInfoPair(GL_ALPHA,           InternalFormatInfo::UnsizedFormat(GL_ALPHA,           AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE,       InternalFormatInfo::UnsizedFormat(GL_LUMINANCE,       AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE_ALPHA, InternalFormatInfo::UnsizedFormat(GL_LUMINANCE_ALPHA, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RED,             InternalFormatInfo::UnsizedFormat(GL_RED,             AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RG,              InternalFormatInfo::UnsizedFormat(GL_RG,              AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RGB,             InternalFormatInfo::UnsizedFormat(GL_RGB,             AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RGBA,            InternalFormatInfo::UnsizedFormat(GL_RGBA,            AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RED_INTEGER,     InternalFormatInfo::UnsizedFormat(GL_RED_INTEGER,     AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RG_INTEGER,      InternalFormatInfo::UnsizedFormat(GL_RG_INTEGER,      AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RGB_INTEGER,     InternalFormatInfo::UnsizedFormat(GL_RGB_INTEGER,     AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RGBA_INTEGER,    InternalFormatInfo::UnsizedFormat(GL_RGBA_INTEGER,    AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_BGRA_EXT,        InternalFormatInfo::UnsizedFormat(GL_BGRA_EXT,        AlwaysSupported)));

    // Compressed formats, From ES 3.0.1 spec, table 3.16
    //                               | Internal format                             |                                    |W |H | BS |CC| Format                                      | Type            | SRGB | Supported          |
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_R11_EAC,                        InternalFormatInfo::CompressedFormat(4, 4,  64, 1, GL_COMPRESSED_R11_EAC,                        GL_UNSIGNED_BYTE, false, UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SIGNED_R11_EAC,                 InternalFormatInfo::CompressedFormat(4, 4,  64, 1, GL_COMPRESSED_SIGNED_R11_EAC,                 GL_UNSIGNED_BYTE, false, UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RG11_EAC,                       InternalFormatInfo::CompressedFormat(4, 4, 128, 2, GL_COMPRESSED_RG11_EAC,                       GL_UNSIGNED_BYTE, false, UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SIGNED_RG11_EAC,                InternalFormatInfo::CompressedFormat(4, 4, 128, 2, GL_COMPRESSED_SIGNED_RG11_EAC,                GL_UNSIGNED_BYTE, false, UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGB8_ETC2,                      InternalFormatInfo::CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_RGB8_ETC2,                      GL_UNSIGNED_BYTE, false, UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ETC2,                     InternalFormatInfo::CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_SRGB8_ETC2,                     GL_UNSIGNED_BYTE, true,  UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  InternalFormatInfo::CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  GL_UNSIGNED_BYTE, false, UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, InternalFormatInfo::CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_UNSIGNED_BYTE, true,  UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA8_ETC2_EAC,                 InternalFormatInfo::CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_RGBA8_ETC2_EAC,                 GL_UNSIGNED_BYTE, false, UnimplementedSupport)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          InternalFormatInfo::CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          GL_UNSIGNED_BYTE, true,  UnimplementedSupport)));

    // From GL_EXT_texture_compression_dxt1
    //                               | Internal format                   |                                    |W |H | BS |CC| Format                            | Type            | SRGB | Supported     |
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    InternalFormatInfo::CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    GL_UNSIGNED_BYTE, false, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   InternalFormatInfo::CompressedFormat(4, 4,  64, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   GL_UNSIGNED_BYTE, false, AlwaysSupported)));

    // From GL_ANGLE_texture_compression_dxt3
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, InternalFormatInfo::CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, GL_UNSIGNED_BYTE, false, AlwaysSupported)));

    // From GL_ANGLE_texture_compression_dxt5
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, InternalFormatInfo::CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, GL_UNSIGNED_BYTE, false, AlwaysSupported)));

    return map;
}

static InternalFormatInfoMap BuildES2InternalFormatInfoMap()
{
    InternalFormatInfoMap map;

    // From ES 2.0.25 table 4.5
    map.insert(InternalFormatInfoPair(GL_NONE,                 InternalFormatInfo()));

    //                               | Internal format        |                              | R | G | B | A |S | Format          | Type                     | Component type        | SRGB | Color         | Texture        | Supported      |
    //                               |                        |                              |   |   |   |   |  |                 |                          |                       |      | renderable    | filterable     |                |
    map.insert(InternalFormatInfoPair(GL_RGBA4,                InternalFormatInfo::RGBAFormat( 4,  4,  4,  4, 0, GL_RGBA,          GL_UNSIGNED_SHORT_4_4_4_4, GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RGB5_A1,              InternalFormatInfo::RGBAFormat( 5,  5,  5,  1, 0, GL_RGBA,          GL_UNSIGNED_SHORT_5_5_5_1, GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RGB565,               InternalFormatInfo::RGBAFormat( 5,  6,  5,  0, 0, GL_RGB,           GL_UNSIGNED_SHORT_5_6_5,   GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported)));

    // Extension formats
    map.insert(InternalFormatInfoPair(GL_R8_EXT,               InternalFormatInfo::RGBAFormat( 8,  0,  0,  0, 0, GL_RED_EXT,       GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, false, CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>, CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>, CheckSupport<&Context::supportsRGTextures>)));
    map.insert(InternalFormatInfoPair(GL_RG8_EXT,              InternalFormatInfo::RGBAFormat( 8,  8,  0,  0, 0, GL_RG_EXT,        GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, false, CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>, CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>, CheckSupport<&Context::supportsRGTextures>)));
    map.insert(InternalFormatInfoPair(GL_RGB8_OES,             InternalFormatInfo::RGBAFormat( 8,  8,  8,  0, 0, GL_RGB,           GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RGBA8_OES,            InternalFormatInfo::RGBAFormat( 8,  8,  8,  8, 0, GL_RGBA,          GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_BGRA8_EXT,            InternalFormatInfo::RGBAFormat( 8,  8,  8,  8, 0, GL_BGRA_EXT,      GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, false, AlwaysSupported, AlwaysSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_BGRA4_ANGLEX,         InternalFormatInfo::RGBAFormat( 4,  4,  4,  4, 0, GL_BGRA_EXT,      GL_UNSIGNED_SHORT_4_4_4_4, GL_UNSIGNED_NORMALIZED, false, NeverSupported,  AlwaysSupported, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_BGR5_A1_ANGLEX,       InternalFormatInfo::RGBAFormat( 5,  5,  5,  1, 0, GL_BGRA_EXT,      GL_UNSIGNED_SHORT_5_5_5_1, GL_UNSIGNED_NORMALIZED, false, NeverSupported,  AlwaysSupported, AlwaysSupported)));

    // Floating point formats have to query the renderer for support
    //                               | Internal format        |                              | R | G | B | A |S | Format          | Type                     | Comp    | SRGB | Color renderable                                                                                           | Texture filterable                                                                                   | Supported                                     |
    //                               |                        |                              |   |   |   |   |  |                 |                          | type    |      |                                                                                                            |                                                                                                      |                                               |
    map.insert(InternalFormatInfoPair(GL_R16F_EXT,             InternalFormatInfo::RGBAFormat(16,  0,  0,  0, 0, GL_RED,           GL_HALF_FLOAT_OES,         GL_FLOAT, false, CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>,                              CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>,                        CheckSupport<&Context::supportsRGTextures>     )));
    map.insert(InternalFormatInfoPair(GL_R32F_EXT,             InternalFormatInfo::RGBAFormat(32,  0,  0,  0, 0, GL_RED,           GL_FLOAT,                  GL_FLOAT, false, CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>,                              CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>,                        CheckSupport<&Context::supportsRGTextures>     )));
    map.insert(InternalFormatInfoPair(GL_RG16F_EXT,            InternalFormatInfo::RGBAFormat(16, 16,  0,  0, 0, GL_RG,            GL_HALF_FLOAT_OES,         GL_FLOAT, false, CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>,                              CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>,                        CheckSupport<&Context::supportsRGTextures>     )));
    map.insert(InternalFormatInfoPair(GL_RG32F_EXT,            InternalFormatInfo::RGBAFormat(32, 32,  0,  0, 0, GL_RG,            GL_FLOAT,                  GL_FLOAT, false, CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>,                              CheckSupport<&Context::supportsRGTextures, &rx::Renderer::getRGTextureSupport>,                        CheckSupport<&Context::supportsRGTextures>     )));
    map.insert(InternalFormatInfoPair(GL_RGB16F_EXT,           InternalFormatInfo::RGBAFormat(16, 16, 16,  0, 0, GL_RGB,           GL_HALF_FLOAT_OES,         GL_FLOAT, false, CheckSupport<&Context::supportsFloat16RenderableTextures, &rx::Renderer::getFloat16TextureRenderingSupport>, CheckSupport<&Context::supportsFloat16LinearFilter, &rx::Renderer::getFloat16TextureFilteringSupport>, CheckSupport<&Context::supportsFloat16Textures>)));
    map.insert(InternalFormatInfoPair(GL_RGB32F_EXT,           InternalFormatInfo::RGBAFormat(32, 32, 32,  0, 0, GL_RGB,           GL_FLOAT,                  GL_FLOAT, false, CheckSupport<&Context::supportsFloat32RenderableTextures, &rx::Renderer::getFloat32TextureRenderingSupport>, CheckSupport<&Context::supportsFloat32LinearFilter, &rx::Renderer::getFloat32TextureFilteringSupport>, CheckSupport<&Context::supportsFloat32Textures>)));
    map.insert(InternalFormatInfoPair(GL_RGBA16F_EXT,          InternalFormatInfo::RGBAFormat(16, 16, 16, 16, 0, GL_RGBA,          GL_HALF_FLOAT_OES,         GL_FLOAT, false, CheckSupport<&Context::supportsFloat16RenderableTextures, &rx::Renderer::getFloat16TextureRenderingSupport>, CheckSupport<&Context::supportsFloat16LinearFilter, &rx::Renderer::getFloat16TextureFilteringSupport>, CheckSupport<&Context::supportsFloat16Textures>)));
    map.insert(InternalFormatInfoPair(GL_RGBA32F_EXT,          InternalFormatInfo::RGBAFormat(32, 32, 32, 32, 0, GL_RGBA,          GL_FLOAT,                  GL_FLOAT, false, CheckSupport<&Context::supportsFloat32RenderableTextures, &rx::Renderer::getFloat32TextureRenderingSupport>, CheckSupport<&Context::supportsFloat32LinearFilter, &rx::Renderer::getFloat32TextureFilteringSupport>, CheckSupport<&Context::supportsFloat32Textures>)));

    // Depth and stencil formats
    //                               | Internal format        |                                      | D |S |X | Format              | Type                     | Internal format     | Depth          | Stencil         | Supported                                  |
    //                               |                        |                                      |   |  |  |                     |                          | type                | renderable     | renderable      |                                            |
    map.insert(InternalFormatInfoPair(GL_DEPTH_COMPONENT32_OES,InternalFormatInfo::DepthStencilFormat(32, 0, 0, GL_DEPTH_COMPONENT,   GL_UNSIGNED_INT,           GL_UNSIGNED_NORMALIZED, AlwaysSupported, NeverSupported,  CheckSupport<&Context::supportsDepthTextures>)));
    map.insert(InternalFormatInfoPair(GL_DEPTH24_STENCIL8_OES, InternalFormatInfo::DepthStencilFormat(24, 8, 0, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES,  GL_UNSIGNED_NORMALIZED, AlwaysSupported, AlwaysSupported, CheckSupport<&Context::supportsDepthTextures>)));
    map.insert(InternalFormatInfoPair(GL_DEPTH_COMPONENT16,    InternalFormatInfo::DepthStencilFormat(16, 0, 0, GL_DEPTH_COMPONENT,   GL_UNSIGNED_SHORT,         GL_UNSIGNED_NORMALIZED, AlwaysSupported, NeverSupported,  AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_STENCIL_INDEX8,       InternalFormatInfo::DepthStencilFormat( 0, 8, 0, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, NeverSupported,  AlwaysSupported, AlwaysSupported)));

    // Unsized formats
    //                               | Internal format        |                                 | Format              | Supported     |
    map.insert(InternalFormatInfoPair(GL_ALPHA,                InternalFormatInfo::UnsizedFormat(GL_ALPHA,             AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE,            InternalFormatInfo::UnsizedFormat(GL_LUMINANCE,         AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE_ALPHA,      InternalFormatInfo::UnsizedFormat(GL_LUMINANCE_ALPHA,   AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RED_EXT,              InternalFormatInfo::UnsizedFormat(GL_RED_EXT,           CheckSupport<&Context::supportsRGTextures>)));
    map.insert(InternalFormatInfoPair(GL_RG_EXT,               InternalFormatInfo::UnsizedFormat(GL_RG_EXT,            CheckSupport<&Context::supportsRGTextures>)));
    map.insert(InternalFormatInfoPair(GL_RGB,                  InternalFormatInfo::UnsizedFormat(GL_RGB,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_RGBA,                 InternalFormatInfo::UnsizedFormat(GL_RGBA,              AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_BGRA_EXT,             InternalFormatInfo::UnsizedFormat(GL_BGRA_EXT,          AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_DEPTH_COMPONENT,      InternalFormatInfo::UnsizedFormat(GL_DEPTH_COMPONENT,   AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_DEPTH_STENCIL_OES,    InternalFormatInfo::UnsizedFormat(GL_DEPTH_STENCIL_OES, AlwaysSupported)));

    // Luminance alpha formats from GL_EXT_texture_storage
    //                               | Internal format          |                              | L | A | Format                   | Type                     | Component type        | Supported     |
    map.insert(InternalFormatInfoPair(GL_ALPHA8_EXT,             InternalFormatInfo::LUMAFormat( 0,  8, GL_ALPHA,                  GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE8_EXT,         InternalFormatInfo::LUMAFormat( 8,  0, GL_LUMINANCE,              GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_ALPHA32F_EXT,           InternalFormatInfo::LUMAFormat( 0, 32, GL_ALPHA,                  GL_FLOAT,                  GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE32F_EXT,       InternalFormatInfo::LUMAFormat(32,  0, GL_LUMINANCE,              GL_FLOAT,                  GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_ALPHA16F_EXT,           InternalFormatInfo::LUMAFormat( 0, 16, GL_ALPHA,                  GL_HALF_FLOAT_OES,         GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE16F_EXT,       InternalFormatInfo::LUMAFormat(16,  0, GL_LUMINANCE,              GL_HALF_FLOAT_OES,         GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE8_ALPHA8_EXT,  InternalFormatInfo::LUMAFormat( 8,  8, GL_LUMINANCE_ALPHA,        GL_UNSIGNED_BYTE,          GL_UNSIGNED_NORMALIZED, AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE_ALPHA32F_EXT, InternalFormatInfo::LUMAFormat(32, 32, GL_LUMINANCE_ALPHA,        GL_FLOAT,                  GL_FLOAT,               AlwaysSupported)));
    map.insert(InternalFormatInfoPair(GL_LUMINANCE_ALPHA16F_EXT, InternalFormatInfo::LUMAFormat(16, 16, GL_LUMINANCE_ALPHA,        GL_HALF_FLOAT_OES,         GL_FLOAT,               AlwaysSupported)));

    // From GL_EXT_texture_compression_dxt1
    //                               | Internal format                   |                                    |W |H | BS |CC|Format                             | Type            | SRGB | Supported                                  |
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    InternalFormatInfo::CompressedFormat(4, 4,  64, 3, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    GL_UNSIGNED_BYTE, false, CheckSupport<&Context::supportsDXT1Textures>)));
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   InternalFormatInfo::CompressedFormat(4, 4,  64, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   GL_UNSIGNED_BYTE, false, CheckSupport<&Context::supportsDXT1Textures>)));

    // From GL_ANGLE_texture_compression_dxt3
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, InternalFormatInfo::CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, GL_UNSIGNED_BYTE, false, CheckSupport<&Context::supportsDXT3Textures>)));

    // From GL_ANGLE_texture_compression_dxt5
    map.insert(InternalFormatInfoPair(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, InternalFormatInfo::CompressedFormat(4, 4, 128, 4, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, GL_UNSIGNED_BYTE, false, CheckSupport<&Context::supportsDXT5Textures>)));

    return map;
}

static bool GetInternalFormatInfo(GLenum internalFormat, GLuint clientVersion, InternalFormatInfo *outFormatInfo)
{
    const InternalFormatInfoMap* map = NULL;

    if (clientVersion == 2)
    {
        static const InternalFormatInfoMap formatMap = BuildES2InternalFormatInfoMap();
        map = &formatMap;
    }
    else if (clientVersion == 3)
    {
        static const InternalFormatInfoMap formatMap = BuildES3InternalFormatInfoMap();
        map = &formatMap;
    }
    else
    {
        UNREACHABLE();
    }

    InternalFormatInfoMap::const_iterator iter = map->find(internalFormat);
    if (iter != map->end())
    {
        if (outFormatInfo)
        {
            *outFormatInfo = iter->second;
        }
        return true;
    }
    else
    {
        return false;
    }
}

typedef std::set<GLenum> FormatSet;

static FormatSet BuildES2ValidFormatSet()
{
    static const FormatMap &formatMap = GetFormatMap(2);

    FormatSet set;

    for (FormatMap::const_iterator i = formatMap.begin(); i != formatMap.end(); i++)
    {
        const FormatTypePair& formatPair = i->first;
        set.insert(formatPair.first);
    }

    return set;
}

static FormatSet BuildES3ValidFormatSet()
{
    static const ES3FormatSet &formatSet = GetES3FormatSet();

    FormatSet set;

    for (ES3FormatSet::const_iterator i = formatSet.begin(); i != formatSet.end(); i++)
    {
        const FormatInfo& formatInfo = *i;
        set.insert(formatInfo.mFormat);
    }

    return set;
}

typedef std::set<GLenum> TypeSet;

static TypeSet BuildES2ValidTypeSet()
{
    static const FormatMap &formatMap = GetFormatMap(2);

    TypeSet set;

    for (FormatMap::const_iterator i = formatMap.begin(); i != formatMap.end(); i++)
    {
        const FormatTypePair& formatPair = i->first;
        set.insert(formatPair.second);
    }

    return set;
}

static TypeSet BuildES3ValidTypeSet()
{
    static const ES3FormatSet &formatSet = GetES3FormatSet();

    TypeSet set;

    for (ES3FormatSet::const_iterator i = formatSet.begin(); i != formatSet.end(); i++)
    {
        const FormatInfo& formatInfo = *i;
        set.insert(formatInfo.mType);
    }

    return set;
}

struct EffectiveInternalFormatInfo
{
    GLenum mEffectiveFormat;
    GLenum mDestFormat;
    GLuint mMinRedBits;
    GLuint mMaxRedBits;
    GLuint mMinGreenBits;
    GLuint mMaxGreenBits;
    GLuint mMinBlueBits;
    GLuint mMaxBlueBits;
    GLuint mMinAlphaBits;
    GLuint mMaxAlphaBits;

    EffectiveInternalFormatInfo(GLenum effectiveFormat, GLenum destFormat, GLuint minRedBits, GLuint maxRedBits,
                                GLuint minGreenBits, GLuint maxGreenBits, GLuint minBlueBits, GLuint maxBlueBits,
                                GLuint minAlphaBits, GLuint maxAlphaBits)
        : mEffectiveFormat(effectiveFormat), mDestFormat(destFormat), mMinRedBits(minRedBits),
          mMaxRedBits(maxRedBits), mMinGreenBits(minGreenBits), mMaxGreenBits(maxGreenBits),
          mMinBlueBits(minBlueBits), mMaxBlueBits(maxBlueBits), mMinAlphaBits(minAlphaBits),
          mMaxAlphaBits(maxAlphaBits) {};
};

typedef std::vector<EffectiveInternalFormatInfo> EffectiveInternalFormatList;

static EffectiveInternalFormatList BuildSizedEffectiveInternalFormatList()
{
    EffectiveInternalFormatList list;

    // OpenGL ES 3.0.3 Specification, Table 3.17, pg 141: Effective internal format coresponding to destination internal format and
    //                                                    linear source buffer component sizes.
    //                                                                            | Source channel min/max sizes |
    //                                         Effective Internal Format |  N/A   |  R   |  G   |  B   |  A      |
    list.push_back(EffectiveInternalFormatInfo(GL_ALPHA8_EXT,              GL_NONE, 0,  0, 0,  0, 0,  0, 1, 8));
    list.push_back(EffectiveInternalFormatInfo(GL_R8,                      GL_NONE, 1,  8, 0,  0, 0,  0, 0, 0));
    list.push_back(EffectiveInternalFormatInfo(GL_RG8,                     GL_NONE, 1,  8, 1,  8, 0,  0, 0, 0));
    list.push_back(EffectiveInternalFormatInfo(GL_RGB565,                  GL_NONE, 1,  5, 1,  6, 1,  5, 0, 0));
    list.push_back(EffectiveInternalFormatInfo(GL_RGB8,                    GL_NONE, 6,  8, 7,  8, 6,  8, 0, 0));
    list.push_back(EffectiveInternalFormatInfo(GL_RGBA4,                   GL_NONE, 1,  4, 1,  4, 1,  4, 1, 4));
    list.push_back(EffectiveInternalFormatInfo(GL_RGB5_A1,                 GL_NONE, 5,  5, 5,  5, 5,  5, 1, 1));
    list.push_back(EffectiveInternalFormatInfo(GL_RGBA8,                   GL_NONE, 5,  8, 5,  8, 5,  8, 2, 8));
    list.push_back(EffectiveInternalFormatInfo(GL_RGB10_A2,                GL_NONE, 9, 10, 9, 10, 9, 10, 2, 2));

    return list;
}


static EffectiveInternalFormatList BuildUnsizedEffectiveInternalFormatList()
{
    EffectiveInternalFormatList list;

    // OpenGL ES 3.0.3 Specification, Table 3.17, pg 141: Effective internal format coresponding to destination internal format and
    //                                                    linear source buffer component sizes.
    //                                                                                        |          Source channel min/max sizes            |
    //                                         Effective Internal Format |    Dest Format     |     R     |      G     |      B     |      A     |
    list.push_back(EffectiveInternalFormatInfo(GL_ALPHA8_EXT,              GL_ALPHA,           0, UINT_MAX, 0, UINT_MAX, 0, UINT_MAX, 1,        8));
    list.push_back(EffectiveInternalFormatInfo(GL_LUMINANCE8_EXT,          GL_LUMINANCE,       1,        8, 0, UINT_MAX, 0, UINT_MAX, 0, UINT_MAX));
    list.push_back(EffectiveInternalFormatInfo(GL_LUMINANCE8_ALPHA8_EXT,   GL_LUMINANCE_ALPHA, 1,        8, 0, UINT_MAX, 0, UINT_MAX, 1,        8));
    list.push_back(EffectiveInternalFormatInfo(GL_RGB565,                  GL_RGB,             1,        5, 1,        6, 1,        5, 0, UINT_MAX));
    list.push_back(EffectiveInternalFormatInfo(GL_RGB8,                    GL_RGB,             6,        8, 7,        8, 6,        8, 0, UINT_MAX));
    list.push_back(EffectiveInternalFormatInfo(GL_RGBA4,                   GL_RGBA,            1,        4, 1,        4, 1,        4, 1,        4));
    list.push_back(EffectiveInternalFormatInfo(GL_RGB5_A1,                 GL_RGBA,            5,        5, 5,        5, 5,        5, 1,        1));
    list.push_back(EffectiveInternalFormatInfo(GL_RGBA8,                   GL_RGBA,            5,        8, 5,        8, 5,        8, 5,        8));

    return list;
}

static bool GetEffectiveInternalFormat(const InternalFormatInfo &srcFormat, const InternalFormatInfo &destFormat,
                                       GLuint clientVersion, GLenum *outEffectiveFormat)
{
    const EffectiveInternalFormatList *list = NULL;
    GLenum targetFormat = GL_NONE;

    if (gl::IsSizedInternalFormat(destFormat.mFormat, clientVersion))
    {
        static const EffectiveInternalFormatList sizedList = BuildSizedEffectiveInternalFormatList();
        list = &sizedList;
    }
    else
    {
        static const EffectiveInternalFormatList unsizedList = BuildUnsizedEffectiveInternalFormatList();
        list = &unsizedList;
        targetFormat = destFormat.mFormat;
    }

    for (size_t curFormat = 0; curFormat < list->size(); ++curFormat)
    {
        const EffectiveInternalFormatInfo& formatInfo = list->at(curFormat);
        if ((formatInfo.mDestFormat == targetFormat) &&
            (formatInfo.mMinRedBits   <= srcFormat.mRedBits   && formatInfo.mMaxRedBits   >= srcFormat.mRedBits)   &&
            (formatInfo.mMinGreenBits <= srcFormat.mGreenBits && formatInfo.mMaxGreenBits >= srcFormat.mGreenBits) &&
            (formatInfo.mMinBlueBits  <= srcFormat.mBlueBits  && formatInfo.mMaxBlueBits  >= srcFormat.mBlueBits)  &&
            (formatInfo.mMinAlphaBits <= srcFormat.mAlphaBits && formatInfo.mMaxAlphaBits >= srcFormat.mAlphaBits))
        {
            *outEffectiveFormat = formatInfo.mEffectiveFormat;
            return true;
        }
    }

    return false;
}

struct CopyConversion
{
    GLenum mTextureFormat;
    GLenum mFramebufferFormat;

    CopyConversion(GLenum textureFormat, GLenum framebufferFormat)
        : mTextureFormat(textureFormat), mFramebufferFormat(framebufferFormat) { }

    bool operator<(const CopyConversion& other) const
    {
        return memcmp(this, &other, sizeof(CopyConversion)) < 0;
    }
};

typedef std::set<CopyConversion> CopyConversionSet;

static CopyConversionSet BuildValidES3CopyTexImageCombinations()
{
    CopyConversionSet set;

    // From ES 3.0.1 spec, table 3.15
    set.insert(CopyConversion(GL_ALPHA,           GL_RGBA));
    set.insert(CopyConversion(GL_LUMINANCE,       GL_RED));
    set.insert(CopyConversion(GL_LUMINANCE,       GL_RG));
    set.insert(CopyConversion(GL_LUMINANCE,       GL_RGB));
    set.insert(CopyConversion(GL_LUMINANCE,       GL_RGBA));
    set.insert(CopyConversion(GL_LUMINANCE_ALPHA, GL_RGBA));
    set.insert(CopyConversion(GL_RED,             GL_RED));
    set.insert(CopyConversion(GL_RED,             GL_RG));
    set.insert(CopyConversion(GL_RED,             GL_RGB));
    set.insert(CopyConversion(GL_RED,             GL_RGBA));
    set.insert(CopyConversion(GL_RG,              GL_RG));
    set.insert(CopyConversion(GL_RG,              GL_RGB));
    set.insert(CopyConversion(GL_RG,              GL_RGBA));
    set.insert(CopyConversion(GL_RGB,             GL_RGB));
    set.insert(CopyConversion(GL_RGB,             GL_RGBA));
    set.insert(CopyConversion(GL_RGBA,            GL_RGBA));

    // Necessary for ANGLE back-buffers
    set.insert(CopyConversion(GL_ALPHA,           GL_BGRA_EXT));
    set.insert(CopyConversion(GL_LUMINANCE,       GL_BGRA_EXT));
    set.insert(CopyConversion(GL_LUMINANCE_ALPHA, GL_BGRA_EXT));
    set.insert(CopyConversion(GL_RED,             GL_BGRA_EXT));
    set.insert(CopyConversion(GL_RG,              GL_BGRA_EXT));
    set.insert(CopyConversion(GL_RGB,             GL_BGRA_EXT));
    set.insert(CopyConversion(GL_RGBA,            GL_BGRA_EXT));

    set.insert(CopyConversion(GL_RED_INTEGER,     GL_RED_INTEGER));
    set.insert(CopyConversion(GL_RED_INTEGER,     GL_RG_INTEGER));
    set.insert(CopyConversion(GL_RED_INTEGER,     GL_RGB_INTEGER));
    set.insert(CopyConversion(GL_RED_INTEGER,     GL_RGBA_INTEGER));
    set.insert(CopyConversion(GL_RG_INTEGER,      GL_RG_INTEGER));
    set.insert(CopyConversion(GL_RG_INTEGER,      GL_RGB_INTEGER));
    set.insert(CopyConversion(GL_RG_INTEGER,      GL_RGBA_INTEGER));
    set.insert(CopyConversion(GL_RGB_INTEGER,     GL_RGB_INTEGER));
    set.insert(CopyConversion(GL_RGB_INTEGER,     GL_RGBA_INTEGER));
    set.insert(CopyConversion(GL_RGBA_INTEGER,    GL_RGBA_INTEGER));

    return set;
}

bool IsValidInternalFormat(GLenum internalFormat, const Context *context)
{
    if (!context)
    {
        return false;
    }

    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, context->getClientVersion(), &internalFormatInfo))
    {
        ASSERT(internalFormatInfo.mSupportFunction != NULL);
        return internalFormatInfo.mSupportFunction(context);
    }
    else
    {
        return false;
    }
}

bool IsValidFormat(GLenum format, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        static const FormatSet formatSet = BuildES2ValidFormatSet();
        return formatSet.find(format) != formatSet.end();
    }
    else if (clientVersion == 3)
    {
        static const FormatSet formatSet = BuildES3ValidFormatSet();
        return formatSet.find(format) != formatSet.end();
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsValidType(GLenum type, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        static const TypeSet typeSet = BuildES2ValidTypeSet();
        return typeSet.find(type) != typeSet.end();
    }
    else if (clientVersion == 3)
    {
        static const TypeSet typeSet = BuildES3ValidTypeSet();
        return typeSet.find(type) != typeSet.end();
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsValidFormatCombination(GLenum internalFormat, GLenum format, GLenum type, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        static const FormatMap &formats = GetFormatMap(clientVersion);
        FormatMap::const_iterator iter = formats.find(FormatTypePair(format, type));

        return (iter != formats.end()) && ((internalFormat == (GLint)type) || (internalFormat == iter->second.mInternalFormat));
    }
    else if (clientVersion == 3)
    {
        static const ES3FormatSet &formats = GetES3FormatSet();
        return formats.find(FormatInfo(internalFormat, format, type)) != formats.end();
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsValidCopyTexImageCombination(GLenum textureInternalFormat, GLenum frameBufferInternalFormat, GLuint readBufferHandle, GLuint clientVersion)
{
    InternalFormatInfo textureInternalFormatInfo;
    InternalFormatInfo framebufferInternalFormatInfo;
    if (GetInternalFormatInfo(textureInternalFormat, clientVersion, &textureInternalFormatInfo) &&
        GetInternalFormatInfo(frameBufferInternalFormat, clientVersion, &framebufferInternalFormatInfo))
    {
        if (clientVersion == 2)
        {
            UNIMPLEMENTED();
            return false;
        }
        else if (clientVersion == 3)
        {
            static const CopyConversionSet conversionSet = BuildValidES3CopyTexImageCombinations();
            const CopyConversion conversion = CopyConversion(textureInternalFormatInfo.mFormat,
                                                             framebufferInternalFormatInfo.mFormat);
            if (conversionSet.find(conversion) != conversionSet.end())
            {
                // Section 3.8.5 of the GLES 3.0.3 spec states that source and destination formats
                // must both be signed, unsigned, or fixed point and both source and destinations
                // must be either both SRGB or both not SRGB. EXT_color_buffer_float adds allowed
                // conversion between fixed and floating point.

                if ((textureInternalFormatInfo.mColorEncoding == GL_SRGB) != (framebufferInternalFormatInfo.mColorEncoding == GL_SRGB))
                {
                    return false;
                }

                if (((textureInternalFormatInfo.mComponentType == GL_INT) != (framebufferInternalFormatInfo.mComponentType == GL_INT)) ||
                    ((textureInternalFormatInfo.mComponentType == GL_UNSIGNED_INT) != (framebufferInternalFormatInfo.mComponentType == GL_UNSIGNED_INT)))
                {
                    return false;
                }

                if (gl::IsFloatOrFixedComponentType(textureInternalFormatInfo.mComponentType) &&
                    !gl::IsFloatOrFixedComponentType(framebufferInternalFormatInfo.mComponentType))
                {
                    return false;
                }

                // GLES specification 3.0.3, sec 3.8.5, pg 139-140:
                // The effective internal format of the source buffer is determined with the following rules applied in order:
                //    * If the source buffer is a texture or renderbuffer that was created with a sized internal format then the
                //      effective internal format is the source buffer's sized internal format.
                //    * If the source buffer is a texture that was created with an unsized base internal format, then the
                //      effective internal format is the source image array's effective internal format, as specified by table
                //      3.12, which is determined from the <format> and <type> that were used when the source image array was
                //      specified by TexImage*.
                //    * Otherwise the effective internal format is determined by the row in table 3.17 or 3.18 where
                //      Destination Internal Format matches internalformat and where the [source channel sizes] are consistent
                //      with the values of the source buffer's [channel sizes]. Table 3.17 is used if the
                //      FRAMEBUFFER_ATTACHMENT_ENCODING is LINEAR and table 3.18 is used if the FRAMEBUFFER_ATTACHMENT_ENCODING
                //      is SRGB.
                InternalFormatInfo sourceEffectiveFormat;
                if (readBufferHandle != 0)
                {
                    // Not the default framebuffer, therefore the read buffer must be a user-created texture or renderbuffer
                    if (gl::IsSizedInternalFormat(framebufferInternalFormatInfo.mFormat, clientVersion))
                    {
                        sourceEffectiveFormat = framebufferInternalFormatInfo;
                    }
                    else
                    {
                        // Renderbuffers cannot be created with an unsized internal format, so this must be an unsized-format
                        // texture. We can use the same table we use when creating textures to get its effective sized format.
                        GLenum effectiveFormat = gl::GetSizedInternalFormat(framebufferInternalFormatInfo.mFormat,
                                                                            framebufferInternalFormatInfo.mType, clientVersion);
                        gl::GetInternalFormatInfo(effectiveFormat, clientVersion, &sourceEffectiveFormat);
                    }
                }
                else
                {
                    // The effective internal format must be derived from the source framebuffer's channel sizes.
                    // This is done in GetEffectiveInternalFormat for linear buffers (table 3.17)
                    if (framebufferInternalFormatInfo.mColorEncoding == GL_LINEAR)
                    {
                        GLenum effectiveFormat;
                        if (GetEffectiveInternalFormat(framebufferInternalFormatInfo, textureInternalFormatInfo, clientVersion, &effectiveFormat))
                        {
                            gl::GetInternalFormatInfo(effectiveFormat, clientVersion, &sourceEffectiveFormat);
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else if (framebufferInternalFormatInfo.mColorEncoding == GL_SRGB)
                    {
                        // SRGB buffers can only be copied to sized format destinations according to table 3.18
                        if (gl::IsSizedInternalFormat(textureInternalFormat, clientVersion) &&
                            (framebufferInternalFormatInfo.mRedBits   >= 1 && framebufferInternalFormatInfo.mRedBits   <= 8) &&
                            (framebufferInternalFormatInfo.mGreenBits >= 1 && framebufferInternalFormatInfo.mGreenBits <= 8) &&
                            (framebufferInternalFormatInfo.mBlueBits  >= 1 && framebufferInternalFormatInfo.mBlueBits  <= 8) &&
                            (framebufferInternalFormatInfo.mAlphaBits >= 1 && framebufferInternalFormatInfo.mAlphaBits <= 8))
                        {
                            gl::GetInternalFormatInfo(GL_SRGB8_ALPHA8, clientVersion, &sourceEffectiveFormat);
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        UNREACHABLE();
                    }
                }

                if (gl::IsSizedInternalFormat(textureInternalFormatInfo.mFormat, clientVersion))
                {
                    // Section 3.8.5 of the GLES 3.0.3 spec, pg 139, requires that, if the destination format is sized,
                    // component sizes of the source and destination formats must exactly match
                    if (textureInternalFormatInfo.mRedBits != sourceEffectiveFormat.mRedBits ||
                        textureInternalFormatInfo.mGreenBits != sourceEffectiveFormat.mGreenBits ||
                        textureInternalFormatInfo.mBlueBits != sourceEffectiveFormat.mBlueBits ||
                        textureInternalFormatInfo.mAlphaBits != sourceEffectiveFormat.mAlphaBits)
                    {
                        return false;
                    }
                }


                return true; // A conversion function exists, and no rule in the specification has precluded conversion
                             // between these formats.
            }

            return false;
        }
        else
        {
            UNREACHABLE();
            return false;
        }
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsSizedInternalFormat(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mPixelBits > 0;
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

GLenum GetSizedInternalFormat(GLenum format, GLenum type, GLuint clientVersion)
{
    const FormatMap &formats = GetFormatMap(clientVersion);
    FormatMap::const_iterator iter = formats.find(FormatTypePair(format, type));
    return (iter != formats.end()) ? iter->second.mInternalFormat : GL_NONE;
}

GLuint GetPixelBytes(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mPixelBits / 8;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetAlphaBits(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mAlphaBits;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetRedBits(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mRedBits;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetGreenBits(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mGreenBits;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetBlueBits(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mBlueBits;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetLuminanceBits(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mLuminanceBits;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetDepthBits(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mDepthBits;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetStencilBits(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mStencilBits;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetTypeBytes(GLenum type)
{
    TypeInfo typeInfo;
    if (GetTypeInfo(type, &typeInfo))
    {
        return typeInfo.mTypeBytes;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

bool IsSpecialInterpretationType(GLenum type)
{
    TypeInfo typeInfo;
    if (GetTypeInfo(type, &typeInfo))
    {
        return typeInfo.mSpecialInterpretation;
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsFloatOrFixedComponentType(GLenum type)
{
    if (type == GL_UNSIGNED_NORMALIZED ||
        type == GL_SIGNED_NORMALIZED ||
        type == GL_FLOAT)
    {
        return true;
    }
    else
    {
        return false;
    }
}

GLenum GetFormat(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mFormat;
    }
    else
    {
        UNREACHABLE();
        return GL_NONE;
    }
}

GLenum GetType(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mType;
    }
    else
    {
        UNREACHABLE();
        return GL_NONE;
    }
}

GLenum GetComponentType(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mComponentType;
    }
    else
    {
        UNREACHABLE();
        return GL_NONE;
    }
}

GLuint GetComponentCount(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mComponentCount;
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

GLenum GetColorEncoding(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mColorEncoding;
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsColorRenderingSupported(GLenum internalFormat, const rx::Renderer *renderer)
{
    InternalFormatInfo internalFormatInfo;
    if (renderer && GetInternalFormatInfo(internalFormat, renderer->getCurrentClientVersion(), &internalFormatInfo))
    {
        return internalFormatInfo.mIsColorRenderable(NULL, renderer);
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsColorRenderingSupported(GLenum internalFormat, const Context *context)
{
    InternalFormatInfo internalFormatInfo;
    if (context && GetInternalFormatInfo(internalFormat, context->getClientVersion(), &internalFormatInfo))
    {
        return internalFormatInfo.mIsColorRenderable(context, NULL);
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsTextureFilteringSupported(GLenum internalFormat, const rx::Renderer *renderer)
{
    InternalFormatInfo internalFormatInfo;
    if (renderer && GetInternalFormatInfo(internalFormat, renderer->getCurrentClientVersion(), &internalFormatInfo))
    {
        return internalFormatInfo.mIsTextureFilterable(NULL, renderer);
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsTextureFilteringSupported(GLenum internalFormat, const Context *context)
{
    InternalFormatInfo internalFormatInfo;
    if (context && GetInternalFormatInfo(internalFormat, context->getClientVersion(), &internalFormatInfo))
    {
        return internalFormatInfo.mIsTextureFilterable(context, NULL);
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsDepthRenderingSupported(GLenum internalFormat, const rx::Renderer *renderer)
{
    InternalFormatInfo internalFormatInfo;
    if (renderer && GetInternalFormatInfo(internalFormat, renderer->getCurrentClientVersion(), &internalFormatInfo))
    {
        return internalFormatInfo.mIsDepthRenderable(NULL, renderer);
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsDepthRenderingSupported(GLenum internalFormat, const Context *context)
{
    InternalFormatInfo internalFormatInfo;
    if (context && GetInternalFormatInfo(internalFormat, context->getClientVersion(), &internalFormatInfo))
    {
        return internalFormatInfo.mIsDepthRenderable(context, NULL);
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsStencilRenderingSupported(GLenum internalFormat, const rx::Renderer *renderer)
{
    InternalFormatInfo internalFormatInfo;
    if (renderer && GetInternalFormatInfo(internalFormat, renderer->getCurrentClientVersion(), &internalFormatInfo))
    {
        return internalFormatInfo.mIsStencilRenderable(NULL, renderer);
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsStencilRenderingSupported(GLenum internalFormat, const Context *context)
{
    InternalFormatInfo internalFormatInfo;
    if (context && GetInternalFormatInfo(internalFormat, context->getClientVersion(), &internalFormatInfo))
    {
        return internalFormatInfo.mIsStencilRenderable(context, NULL);
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

GLuint GetRowPitch(GLenum internalFormat, GLenum type, GLuint clientVersion, GLsizei width, GLint alignment)
{
    ASSERT(alignment > 0 && isPow2(alignment));
    return rx::roundUp(GetBlockSize(internalFormat, type, clientVersion, width, 1), static_cast<GLuint>(alignment));
}

GLuint GetDepthPitch(GLenum internalFormat, GLenum type, GLuint clientVersion, GLsizei width, GLsizei height, GLint alignment)
{
    return GetRowPitch(internalFormat, type, clientVersion, width, alignment) * height;
}

GLuint GetBlockSize(GLenum internalFormat, GLenum type, GLuint clientVersion, GLsizei width, GLsizei height)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        if (internalFormatInfo.mIsCompressed)
        {
            GLsizei numBlocksWide = (width + internalFormatInfo.mCompressedBlockWidth - 1) / internalFormatInfo.mCompressedBlockWidth;
            GLsizei numBlocksHight = (height + internalFormatInfo.mCompressedBlockHeight - 1) / internalFormatInfo.mCompressedBlockHeight;

            return (internalFormatInfo.mPixelBits * numBlocksWide * numBlocksHight) / 8;
        }
        else
        {
            TypeInfo typeInfo;
            if (GetTypeInfo(type, &typeInfo))
            {
                if (typeInfo.mSpecialInterpretation)
                {
                    return typeInfo.mTypeBytes * width * height;
                }
                else
                {
                    return internalFormatInfo.mComponentCount * typeInfo.mTypeBytes * width * height;
                }
            }
            else
            {
                UNREACHABLE();
                return 0;
            }
        }
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

bool IsFormatCompressed(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mIsCompressed;
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

GLuint GetCompressedBlockWidth(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mCompressedBlockWidth;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetCompressedBlockHeight(GLenum internalFormat, GLuint clientVersion)
{
    InternalFormatInfo internalFormatInfo;
    if (GetInternalFormatInfo(internalFormat, clientVersion, &internalFormatInfo))
    {
        return internalFormatInfo.mCompressedBlockHeight;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

ColorWriteFunction GetColorWriteFunction(GLenum format, GLenum type, GLuint clientVersion)
{
    static const FormatMap &formats = GetFormatMap(clientVersion);
    FormatMap::const_iterator iter = formats.find(FormatTypePair(format, type));
    return (iter != formats.end()) ? iter->second.mColorWriteFunction : NULL;
}

}
