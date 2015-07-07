#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils9.cpp: Queries for GL image formats and their translations to D3D9
// formats.

#include "libGLESv2/renderer/d3d9/formatutils9.h"
#include "libGLESv2/renderer/d3d9/Renderer9.h"
#include "libGLESv2/renderer/generatemip.h"
#include "libGLESv2/renderer/loadimage.h"
#include "libGLESv2/renderer/copyimage.h"
#include "libGLESv2/renderer/vertexconversion.h"

namespace rx
{

// Each GL internal format corresponds to one D3D format and data loading function.
// Due to not all formats being available all the time, some of the function/format types are wrapped
// in templates that perform format support queries on a Renderer9 object which is supplied
// when requesting the function or format.

typedef bool ((Renderer9::*Renderer9FormatCheckFunction)(void) const);
typedef LoadImageFunction (*RendererCheckLoadFunction)(const Renderer9 *renderer);

template <Renderer9FormatCheckFunction pred, LoadImageFunction prefered, LoadImageFunction fallback>
LoadImageFunction RendererCheckLoad(const Renderer9 *renderer)
{
    return ((renderer->*pred)()) ? prefered : fallback;
}

template <LoadImageFunction loadFunc>
LoadImageFunction SimpleLoad(const Renderer9 *renderer)
{
    return loadFunc;
}

LoadImageFunction UnreachableLoad(const Renderer9 *renderer)
{
    UNREACHABLE();
    return NULL;
}

typedef bool (*FallbackPredicateFunction)(void);

template <FallbackPredicateFunction pred, LoadImageFunction prefered, LoadImageFunction fallback>
LoadImageFunction FallbackLoadFunction(const Renderer9 *renderer)
{
    return pred() ? prefered : fallback;
}

typedef D3DFORMAT (*FormatQueryFunction)(const rx::Renderer9 *renderer);

template <Renderer9FormatCheckFunction pred, D3DFORMAT prefered, D3DFORMAT fallback>
D3DFORMAT CheckFormatSupport(const rx::Renderer9 *renderer)
{
    return (renderer->*pred)() ? prefered : fallback;
}

template <D3DFORMAT format>
D3DFORMAT D3D9Format(const rx::Renderer9 *renderer)
{
    return format;
}

struct D3D9FormatInfo
{
    FormatQueryFunction mTexFormat;
    FormatQueryFunction mRenderFormat;
    RendererCheckLoadFunction mLoadFunction;

    D3D9FormatInfo()
        : mTexFormat(NULL), mRenderFormat(NULL), mLoadFunction(NULL)
    { }

    D3D9FormatInfo(FormatQueryFunction textureFormat, FormatQueryFunction renderFormat, RendererCheckLoadFunction loadFunc)
        : mTexFormat(textureFormat), mRenderFormat(renderFormat), mLoadFunction(loadFunc)
    { }
};

const D3DFORMAT D3DFMT_INTZ = ((D3DFORMAT)(MAKEFOURCC('I','N','T','Z')));
const D3DFORMAT D3DFMT_NULL = ((D3DFORMAT)(MAKEFOURCC('N','U','L','L')));

typedef std::pair<GLenum, D3D9FormatInfo> D3D9FormatPair;
typedef std::map<GLenum, D3D9FormatInfo> D3D9FormatMap;

static D3D9FormatMap BuildD3D9FormatMap()
{
    D3D9FormatMap map;

    //                       | Internal format                                   | Texture format                  | Render format                    | Load function                                   |
    map.insert(D3D9FormatPair(GL_NONE,                             D3D9FormatInfo(D3D9Format<D3DFMT_NULL>,          D3D9Format<D3DFMT_NULL>,           UnreachableLoad                                  )));

    map.insert(D3D9FormatPair(GL_DEPTH_COMPONENT16,                D3D9FormatInfo(D3D9Format<D3DFMT_INTZ>,          D3D9Format<D3DFMT_D24S8>,          UnreachableLoad                                  )));
    map.insert(D3D9FormatPair(GL_DEPTH_COMPONENT32_OES,            D3D9FormatInfo(D3D9Format<D3DFMT_INTZ>,          D3D9Format<D3DFMT_D32>,            UnreachableLoad                                  )));
    map.insert(D3D9FormatPair(GL_DEPTH24_STENCIL8_OES,             D3D9FormatInfo(D3D9Format<D3DFMT_INTZ>,          D3D9Format<D3DFMT_D24S8>,          UnreachableLoad                                  )));
    map.insert(D3D9FormatPair(GL_STENCIL_INDEX8,                   D3D9FormatInfo(D3D9Format<D3DFMT_UNKNOWN>,       D3D9Format<D3DFMT_D24S8>,          UnreachableLoad                                  ))); // TODO: What's the texture format?

    map.insert(D3D9FormatPair(GL_RGBA32F_EXT,                      D3D9FormatInfo(D3D9Format<D3DFMT_A32B32G32R32F>, D3D9Format<D3DFMT_A32B32G32R32F>,  SimpleLoad<loadToNative<GLfloat, 4> >            )));
    map.insert(D3D9FormatPair(GL_RGB32F_EXT,                       D3D9FormatInfo(D3D9Format<D3DFMT_A32B32G32R32F>, D3D9Format<D3DFMT_A32B32G32R32F>,  SimpleLoad<loadToNative3To4<GLfloat, gl::Float32One> >)));
    map.insert(D3D9FormatPair(GL_RG32F_EXT,                        D3D9FormatInfo(D3D9Format<D3DFMT_G32R32F>,       D3D9Format<D3DFMT_G32R32F>,        SimpleLoad<loadToNative<GLfloat, 2> >            )));
    map.insert(D3D9FormatPair(GL_R32F_EXT,                         D3D9FormatInfo(D3D9Format<D3DFMT_R32F>,          D3D9Format<D3DFMT_R32F>,           SimpleLoad<loadToNative<GLfloat, 1> >            )));
    map.insert(D3D9FormatPair(GL_ALPHA32F_EXT,                     D3D9FormatInfo(D3D9Format<D3DFMT_A32B32G32R32F>, D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadAlphaFloatDataToRGBA>             )));
    map.insert(D3D9FormatPair(GL_LUMINANCE32F_EXT,                 D3D9FormatInfo(D3D9Format<D3DFMT_A32B32G32R32F>, D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadLuminanceFloatDataToRGBA>         )));
    map.insert(D3D9FormatPair(GL_LUMINANCE_ALPHA32F_EXT,           D3D9FormatInfo(D3D9Format<D3DFMT_A32B32G32R32F>, D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadLuminanceAlphaFloatDataToRGBA>    )));

    map.insert(D3D9FormatPair(GL_RGBA16F_EXT,                      D3D9FormatInfo(D3D9Format<D3DFMT_A16B16G16R16F>, D3D9Format<D3DFMT_A16B16G16R16F>,  SimpleLoad<loadToNative<GLhalf, 4> >             )));
    map.insert(D3D9FormatPair(GL_RGB16F_EXT,                       D3D9FormatInfo(D3D9Format<D3DFMT_A16B16G16R16F>, D3D9Format<D3DFMT_A16B16G16R16F>,  SimpleLoad<loadToNative3To4<GLhalf, gl::Float16One> >)));
    map.insert(D3D9FormatPair(GL_RG16F_EXT,                        D3D9FormatInfo(D3D9Format<D3DFMT_G16R16F>,       D3D9Format<D3DFMT_G16R16F>,        SimpleLoad<loadToNative<GLhalf, 2> >             )));
    map.insert(D3D9FormatPair(GL_R16F_EXT,                         D3D9FormatInfo(D3D9Format<D3DFMT_R16F>,          D3D9Format<D3DFMT_R16F>,           SimpleLoad<loadToNative<GLhalf, 1> >             )));
    map.insert(D3D9FormatPair(GL_ALPHA16F_EXT,                     D3D9FormatInfo(D3D9Format<D3DFMT_A16B16G16R16F>, D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadAlphaHalfFloatDataToRGBA>         )));
    map.insert(D3D9FormatPair(GL_LUMINANCE16F_EXT,                 D3D9FormatInfo(D3D9Format<D3DFMT_A16B16G16R16F>, D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadLuminanceHalfFloatDataToRGBA>     )));
    map.insert(D3D9FormatPair(GL_LUMINANCE_ALPHA16F_EXT,           D3D9FormatInfo(D3D9Format<D3DFMT_A16B16G16R16F>, D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadLuminanceAlphaHalfFloatDataToRGBA>)));

    map.insert(D3D9FormatPair(GL_ALPHA8_EXT,                       D3D9FormatInfo(D3D9Format<D3DFMT_A8R8G8B8>,      D3D9Format<D3DFMT_A8R8G8B8>,       FallbackLoadFunction<gl::supportsSSE2, loadAlphaDataToBGRASSE2, loadAlphaDataToBGRA>)));

    map.insert(D3D9FormatPair(GL_RGB8_OES,                         D3D9FormatInfo(D3D9Format<D3DFMT_X8R8G8B8>,      D3D9Format<D3DFMT_X8R8G8B8>,       SimpleLoad<loadRGBUByteDataToBGRX>                )));
    map.insert(D3D9FormatPair(GL_RGB565,                           D3D9FormatInfo(CheckFormatSupport<&Renderer9::getRGB565TextureSupport, D3DFMT_R5G6B5, D3DFMT_X8R8G8B8>, CheckFormatSupport<&Renderer9::getRGB565TextureSupport, D3DFMT_R5G6B5, D3DFMT_X8R8G8B8>, RendererCheckLoad<&Renderer9::getRGB565TextureSupport, loadToNative<GLushort, 1>, loadRGB565DataToBGRA>)));
    map.insert(D3D9FormatPair(GL_RGBA8_OES,                        D3D9FormatInfo(D3D9Format<D3DFMT_A8R8G8B8>,      D3D9Format<D3DFMT_A8R8G8B8>,       FallbackLoadFunction<gl::supportsSSE2, loadRGBAUByteDataToBGRASSE2, loadRGBAUByteDataToBGRA>)));
    map.insert(D3D9FormatPair(GL_RGBA4,                            D3D9FormatInfo(D3D9Format<D3DFMT_A8R8G8B8>,      D3D9Format<D3DFMT_A8R8G8B8>,       SimpleLoad<loadRGBA4444DataToBGRA>                )));
    map.insert(D3D9FormatPair(GL_RGB5_A1,                          D3D9FormatInfo(D3D9Format<D3DFMT_A8R8G8B8>,      D3D9Format<D3DFMT_A8R8G8B8>,       SimpleLoad<loadRGBA5551DataToBGRA>                )));
    map.insert(D3D9FormatPair(GL_R8_EXT,                           D3D9FormatInfo(D3D9Format<D3DFMT_X8R8G8B8>,      D3D9Format<D3DFMT_X8R8G8B8>,       SimpleLoad<loadRUByteDataToBGRX>                  )));
    map.insert(D3D9FormatPair(GL_RG8_EXT,                          D3D9FormatInfo(D3D9Format<D3DFMT_X8R8G8B8>,      D3D9Format<D3DFMT_X8R8G8B8>,       SimpleLoad<loadRGUByteDataToBGRX>                 )));

    map.insert(D3D9FormatPair(GL_BGRA8_EXT,                        D3D9FormatInfo(D3D9Format<D3DFMT_A8R8G8B8>,      D3D9Format<D3DFMT_A8R8G8B8>,       SimpleLoad<loadToNative<GLubyte, 4> >             )));
    map.insert(D3D9FormatPair(GL_BGRA4_ANGLEX,                     D3D9FormatInfo(D3D9Format<D3DFMT_A8R8G8B8>,      D3D9Format<D3DFMT_A8R8G8B8>,       SimpleLoad<loadRGBA4444DataToRGBA>                )));
    map.insert(D3D9FormatPair(GL_BGR5_A1_ANGLEX,                   D3D9FormatInfo(D3D9Format<D3DFMT_A8R8G8B8>,      D3D9Format<D3DFMT_A8R8G8B8>,       SimpleLoad<loadRGBA5551DataToRGBA>                )));

    map.insert(D3D9FormatPair(GL_COMPRESSED_RGB_S3TC_DXT1_EXT,     D3D9FormatInfo(D3D9Format<D3DFMT_DXT1>,          D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadCompressedBlockDataToNative<4, 4,  8> >)));
    map.insert(D3D9FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,    D3D9FormatInfo(D3D9Format<D3DFMT_DXT1>,          D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadCompressedBlockDataToNative<4, 4,  8> >)));
    map.insert(D3D9FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE,  D3D9FormatInfo(D3D9Format<D3DFMT_DXT3>,          D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadCompressedBlockDataToNative<4, 4, 16> >)));
    map.insert(D3D9FormatPair(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE,  D3D9FormatInfo(D3D9Format<D3DFMT_DXT5>,          D3D9Format<D3DFMT_UNKNOWN>,        SimpleLoad<loadCompressedBlockDataToNative<4, 4, 16> >)));

    // These formats require checking if the renderer supports D3DFMT_L8 or D3DFMT_A8L8 and
    // then changing the format and loading function appropriately.
    map.insert(D3D9FormatPair(GL_LUMINANCE8_EXT,                   D3D9FormatInfo(CheckFormatSupport<&Renderer9::getLuminanceTextureSupport, D3DFMT_L8, D3DFMT_A8R8G8B8>,        D3D9Format<D3DFMT_UNKNOWN>,  RendererCheckLoad<&Renderer9::getLuminanceTextureSupport, loadToNative<GLubyte, 1>, loadLuminanceDataToBGRA>)));
    map.insert(D3D9FormatPair(GL_LUMINANCE8_ALPHA8_EXT,            D3D9FormatInfo(CheckFormatSupport<&Renderer9::getLuminanceAlphaTextureSupport, D3DFMT_A8L8, D3DFMT_A8R8G8B8>, D3D9Format<D3DFMT_UNKNOWN>,  RendererCheckLoad<&Renderer9::getLuminanceTextureSupport, loadToNative<GLubyte, 2>, loadLuminanceAlphaDataToBGRA>)));

    return map;
}

static bool GetD3D9FormatInfo(GLenum internalFormat, D3D9FormatInfo *outFormatInfo)
{
    static const D3D9FormatMap formatMap = BuildD3D9FormatMap();
    D3D9FormatMap::const_iterator iter = formatMap.find(internalFormat);
    if (iter != formatMap.end())
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

// A map to determine the pixel size and mip generation function of a given D3D format
struct D3DFormatInfo
{
    GLuint mPixelBits;
    GLuint mBlockWidth;
    GLuint mBlockHeight;
    GLenum mInternalFormat;

    MipGenerationFunction mMipGenerationFunction;
    ColorReadFunction mColorReadFunction;

    D3DFormatInfo()
        : mPixelBits(0), mBlockWidth(0), mBlockHeight(0), mInternalFormat(GL_NONE), mMipGenerationFunction(NULL),
          mColorReadFunction(NULL)
    { }

    D3DFormatInfo(GLuint pixelBits, GLuint blockWidth, GLuint blockHeight, GLenum internalFormat,
                  MipGenerationFunction mipFunc, ColorReadFunction readFunc)
        : mPixelBits(pixelBits), mBlockWidth(blockWidth), mBlockHeight(blockHeight), mInternalFormat(internalFormat),
          mMipGenerationFunction(mipFunc), mColorReadFunction(readFunc)
    { }
};

typedef std::pair<D3DFORMAT, D3DFormatInfo> D3D9FormatInfoPair;
typedef std::map<D3DFORMAT, D3DFormatInfo> D3D9FormatInfoMap;

static D3D9FormatInfoMap BuildD3D9FormatInfoMap()
{
    D3D9FormatInfoMap map;

    //                           | D3DFORMAT           |             | S  |W |H | Internal format                   | Mip generation function   | Color read function             |
    map.insert(D3D9FormatInfoPair(D3DFMT_NULL,          D3DFormatInfo(  0, 0, 0, GL_NONE,                            NULL,                       NULL                             )));
    map.insert(D3D9FormatInfoPair(D3DFMT_UNKNOWN,       D3DFormatInfo(  0, 0, 0, GL_NONE,                            NULL,                       NULL                             )));

    map.insert(D3D9FormatInfoPair(D3DFMT_L8,            D3DFormatInfo(  8, 1, 1, GL_LUMINANCE8_EXT,                  GenerateMip<L8>,            ReadColor<L8, GLfloat>           )));
    map.insert(D3D9FormatInfoPair(D3DFMT_A8,            D3DFormatInfo(  8, 1, 1, GL_ALPHA8_EXT,                      GenerateMip<A8>,            ReadColor<A8, GLfloat>           )));
    map.insert(D3D9FormatInfoPair(D3DFMT_A8L8,          D3DFormatInfo( 16, 1, 1, GL_LUMINANCE8_ALPHA8_EXT,           GenerateMip<A8L8>,          ReadColor<A8L8, GLfloat>         )));
    map.insert(D3D9FormatInfoPair(D3DFMT_A4R4G4B4,      D3DFormatInfo( 16, 1, 1, GL_BGRA4_ANGLEX,                    GenerateMip<B4G4R4A4>,      ReadColor<B4G4R4A4, GLfloat>     )));
    map.insert(D3D9FormatInfoPair(D3DFMT_A1R5G5B5,      D3DFormatInfo( 16, 1, 1, GL_BGR5_A1_ANGLEX,                  GenerateMip<B5G5R5A1>,      ReadColor<B5G5R5A1, GLfloat>     )));
    map.insert(D3D9FormatInfoPair(D3DFMT_R5G6B5,        D3DFormatInfo( 16, 1, 1, GL_RGB565,                          GenerateMip<R5G6B5>,        ReadColor<R5G6B5, GLfloat>       )));
    map.insert(D3D9FormatInfoPair(D3DFMT_X8R8G8B8,      D3DFormatInfo( 32, 1, 1, GL_BGRA8_EXT,                       GenerateMip<B8G8R8X8>,      ReadColor<B8G8R8X8, GLfloat>     )));
    map.insert(D3D9FormatInfoPair(D3DFMT_A8R8G8B8,      D3DFormatInfo( 32, 1, 1, GL_BGRA8_EXT,                       GenerateMip<B8G8R8A8>,      ReadColor<B8G8R8A8, GLfloat>     )));
    map.insert(D3D9FormatInfoPair(D3DFMT_R16F,          D3DFormatInfo( 16, 1, 1, GL_R16F_EXT,                        GenerateMip<R16F>,          ReadColor<R16F, GLfloat>         )));
    map.insert(D3D9FormatInfoPair(D3DFMT_G16R16F,       D3DFormatInfo( 32, 1, 1, GL_RG16F_EXT,                       GenerateMip<R16G16F>,       ReadColor<R16G16F, GLfloat>      )));
    map.insert(D3D9FormatInfoPair(D3DFMT_A16B16G16R16F, D3DFormatInfo( 64, 1, 1, GL_RGBA16F_EXT,                     GenerateMip<R16G16B16A16F>, ReadColor<R16G16B16A16F, GLfloat>)));
    map.insert(D3D9FormatInfoPair(D3DFMT_R32F,          D3DFormatInfo( 32, 1, 1, GL_R32F_EXT,                        GenerateMip<R32F>,          ReadColor<R32F, GLfloat>         )));
    map.insert(D3D9FormatInfoPair(D3DFMT_G32R32F,       D3DFormatInfo( 64, 1, 1, GL_RG32F_EXT,                       GenerateMip<R32G32F>,       ReadColor<R32G32F, GLfloat>      )));
    map.insert(D3D9FormatInfoPair(D3DFMT_A32B32G32R32F, D3DFormatInfo(128, 1, 1, GL_RGBA32F_EXT,                     GenerateMip<R32G32B32A32F>, ReadColor<R32G32B32A32F, GLfloat>)));

    map.insert(D3D9FormatInfoPair(D3DFMT_D16,           D3DFormatInfo( 16, 1, 1, GL_DEPTH_COMPONENT16,               NULL,                       NULL                             )));
    map.insert(D3D9FormatInfoPair(D3DFMT_D24S8,         D3DFormatInfo( 32, 1, 1, GL_DEPTH24_STENCIL8_OES,            NULL,                       NULL                             )));
    map.insert(D3D9FormatInfoPair(D3DFMT_D24X8,         D3DFormatInfo( 32, 1, 1, GL_DEPTH_COMPONENT16,               NULL,                       NULL                             )));
    map.insert(D3D9FormatInfoPair(D3DFMT_D32,           D3DFormatInfo( 32, 1, 1, GL_DEPTH_COMPONENT32_OES,           NULL,                       NULL                             )));

    map.insert(D3D9FormatInfoPair(D3DFMT_INTZ,          D3DFormatInfo( 32, 1, 1, GL_DEPTH24_STENCIL8_OES,            NULL,                       NULL                             )));

    map.insert(D3D9FormatInfoPair(D3DFMT_DXT1,          D3DFormatInfo( 64, 4, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   NULL,                       NULL                             )));
    map.insert(D3D9FormatInfoPair(D3DFMT_DXT3,          D3DFormatInfo(128, 4, 4, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, NULL,                       NULL                             )));
    map.insert(D3D9FormatInfoPair(D3DFMT_DXT5,          D3DFormatInfo(128, 4, 4, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, NULL,                       NULL                             )));

    return map;
}

static const D3D9FormatInfoMap &GetD3D9FormatInfoMap()
{
    static const D3D9FormatInfoMap infoMap = BuildD3D9FormatInfoMap();
    return infoMap;
}

static bool GetD3D9FormatInfo(D3DFORMAT format, D3DFormatInfo *outFormatInfo)
{
    const D3D9FormatInfoMap &infoMap = GetD3D9FormatInfoMap();
    D3D9FormatInfoMap::const_iterator iter = infoMap.find(format);
    if (iter != infoMap.end())
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
static d3d9::D3DFormatSet BuildAllD3DFormatSet()
{
    d3d9::D3DFormatSet set;

    const D3D9FormatInfoMap &infoMap = GetD3D9FormatInfoMap();
    for (D3D9FormatInfoMap::const_iterator i = infoMap.begin(); i != infoMap.end(); ++i)
    {
        set.insert(i->first);
    }

    return set;
}

struct D3D9FastCopyFormat
{
    D3DFORMAT mSourceFormat;
    GLenum mDestFormat;
    GLenum mDestType;

    D3D9FastCopyFormat(D3DFORMAT sourceFormat, GLenum destFormat, GLenum destType)
        : mSourceFormat(sourceFormat), mDestFormat(destFormat), mDestType(destType)
    { }

    bool operator<(const D3D9FastCopyFormat& other) const
    {
        return memcmp(this, &other, sizeof(D3D9FastCopyFormat)) < 0;
    }
};

typedef std::map<D3D9FastCopyFormat, ColorCopyFunction> D3D9FastCopyMap;
typedef std::pair<D3D9FastCopyFormat, ColorCopyFunction> D3D9FastCopyPair;

static D3D9FastCopyMap BuildFastCopyMap()
{
    D3D9FastCopyMap map;

    map.insert(D3D9FastCopyPair(D3D9FastCopyFormat(D3DFMT_A8R8G8B8, GL_RGBA, GL_UNSIGNED_BYTE), CopyBGRAUByteToRGBAUByte));

    return map;
}

typedef std::pair<GLint, InitializeTextureDataFunction> InternalFormatInitialzerPair;
typedef std::map<GLint, InitializeTextureDataFunction> InternalFormatInitialzerMap;

static InternalFormatInitialzerMap BuildInternalFormatInitialzerMap()
{
    InternalFormatInitialzerMap map;

    map.insert(InternalFormatInitialzerPair(GL_RGB16F,  initialize4ComponentData<GLhalf,   0x0000,     0x0000,     0x0000,     gl::Float16One>));
    map.insert(InternalFormatInitialzerPair(GL_RGB32F,  initialize4ComponentData<GLfloat,  0x00000000, 0x00000000, 0x00000000, gl::Float32One>));

    return map;
}

static const InternalFormatInitialzerMap &GetInternalFormatInitialzerMap()
{
    static const InternalFormatInitialzerMap map = BuildInternalFormatInitialzerMap();
    return map;
}

namespace d3d9
{

MipGenerationFunction GetMipGenerationFunction(D3DFORMAT format)
{
    D3DFormatInfo d3dFormatInfo;
    if (GetD3D9FormatInfo(format, &d3dFormatInfo))
    {
        return d3dFormatInfo.mMipGenerationFunction;
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

LoadImageFunction GetImageLoadFunction(GLenum internalFormat, const Renderer9 *renderer)
{
    if (!renderer)
    {
        return NULL;
    }

    ASSERT(renderer->getCurrentClientVersion() == 2);

    D3D9FormatInfo d3d9FormatInfo;
    if (GetD3D9FormatInfo(internalFormat, &d3d9FormatInfo))
    {
        return d3d9FormatInfo.mLoadFunction(renderer);
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

GLuint GetFormatPixelBytes(D3DFORMAT format)
{
    D3DFormatInfo d3dFormatInfo;
    if (GetD3D9FormatInfo(format, &d3dFormatInfo))
    {
        return d3dFormatInfo.mPixelBits / 8;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetBlockWidth(D3DFORMAT format)
{
    D3DFormatInfo d3dFormatInfo;
    if (GetD3D9FormatInfo(format, &d3dFormatInfo))
    {
        return d3dFormatInfo.mBlockWidth;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetBlockHeight(D3DFORMAT format)
{
    D3DFormatInfo d3dFormatInfo;
    if (GetD3D9FormatInfo(format, &d3dFormatInfo))
    {
        return d3dFormatInfo.mBlockHeight;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

GLuint GetBlockSize(D3DFORMAT format, GLuint width, GLuint height)
{
    D3DFormatInfo d3dFormatInfo;
    if (GetD3D9FormatInfo(format, &d3dFormatInfo))
    {
        GLuint numBlocksWide = (width + d3dFormatInfo.mBlockWidth - 1) / d3dFormatInfo.mBlockWidth;
        GLuint numBlocksHight = (height + d3dFormatInfo.mBlockHeight - 1) / d3dFormatInfo.mBlockHeight;

        return (d3dFormatInfo.mPixelBits * numBlocksWide * numBlocksHight) / 8;
    }
    else
    {
        UNREACHABLE();
        return 0;
    }
}

void MakeValidSize(bool isImage, D3DFORMAT format, GLsizei *requestWidth, GLsizei *requestHeight, int *levelOffset)
{
    D3DFormatInfo d3dFormatInfo;
    if (GetD3D9FormatInfo(format, &d3dFormatInfo))
    {
        int upsampleCount = 0;

        GLsizei blockWidth = d3dFormatInfo.mBlockWidth;
        GLsizei blockHeight = d3dFormatInfo.mBlockHeight;

        // Don't expand the size of full textures that are at least (blockWidth x blockHeight) already.
        if (isImage || *requestWidth < blockWidth || *requestHeight < blockHeight)
        {
            while (*requestWidth % blockWidth != 0 || *requestHeight % blockHeight != 0)
            {
                *requestWidth <<= 1;
                *requestHeight <<= 1;
                upsampleCount++;
            }
        }
        *levelOffset = upsampleCount;
    }
}

const D3DFormatSet &GetAllUsedD3DFormats()
{
    static const D3DFormatSet formatSet = BuildAllD3DFormatSet();
    return formatSet;
}

ColorReadFunction GetColorReadFunction(D3DFORMAT format)
{
    D3DFormatInfo d3dFormatInfo;
    if (GetD3D9FormatInfo(format, &d3dFormatInfo))
    {
        return d3dFormatInfo.mColorReadFunction;
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

ColorCopyFunction GetFastCopyFunction(D3DFORMAT sourceFormat, GLenum destFormat, GLenum destType, GLuint clientVersion)
{
    static const D3D9FastCopyMap fastCopyMap = BuildFastCopyMap();
    D3D9FastCopyMap::const_iterator iter = fastCopyMap.find(D3D9FastCopyFormat(sourceFormat, destFormat, destType));
    return (iter != fastCopyMap.end()) ? iter->second : NULL;
}

GLenum GetDeclTypeComponentType(D3DDECLTYPE declType)
{
    switch (declType)
    {
      case D3DDECLTYPE_FLOAT1:   return GL_FLOAT;
      case D3DDECLTYPE_FLOAT2:   return GL_FLOAT;
      case D3DDECLTYPE_FLOAT3:   return GL_FLOAT;
      case D3DDECLTYPE_FLOAT4:   return GL_FLOAT;
      case D3DDECLTYPE_UBYTE4:   return GL_UNSIGNED_INT;
      case D3DDECLTYPE_SHORT2:   return GL_INT;
      case D3DDECLTYPE_SHORT4:   return GL_INT;
      case D3DDECLTYPE_UBYTE4N:  return GL_UNSIGNED_NORMALIZED;
      case D3DDECLTYPE_SHORT4N:  return GL_SIGNED_NORMALIZED;
      case D3DDECLTYPE_USHORT4N: return GL_UNSIGNED_NORMALIZED;
      case D3DDECLTYPE_SHORT2N:  return GL_SIGNED_NORMALIZED;
      default: UNREACHABLE();    return GL_NONE;
    }
}

// Attribute format conversion
enum { NUM_GL_VERTEX_ATTRIB_TYPES = 6 };

struct FormatConverter
{
    bool identity;
    std::size_t outputElementSize;
    void (*convertArray)(const void *in, std::size_t stride, std::size_t n, void *out);
    D3DDECLTYPE d3dDeclType;
};

struct TranslationDescription
{
    DWORD capsFlag;
    FormatConverter preferredConversion;
    FormatConverter fallbackConversion;
};

static unsigned int typeIndex(GLenum type);
static const FormatConverter &formatConverter(const gl::VertexAttribute &attribute);

bool mTranslationsInitialized = false;
FormatConverter mFormatConverters[NUM_GL_VERTEX_ATTRIB_TYPES][2][4];

// Mapping from OpenGL-ES vertex attrib type to D3D decl type:
//
// BYTE                 SHORT (Cast)
// BYTE-norm            FLOAT (Normalize) (can't be exactly represented as SHORT-norm)
// UNSIGNED_BYTE        UBYTE4 (Identity) or SHORT (Cast)
// UNSIGNED_BYTE-norm   UBYTE4N (Identity) or FLOAT (Normalize)
// SHORT                SHORT (Identity)
// SHORT-norm           SHORT-norm (Identity) or FLOAT (Normalize)
// UNSIGNED_SHORT       FLOAT (Cast)
// UNSIGNED_SHORT-norm  USHORT-norm (Identity) or FLOAT (Normalize)
// FIXED (not in WebGL) FLOAT (FixedToFloat)
// FLOAT                FLOAT (Identity)

// GLToCType maps from GL type (as GLenum) to the C typedef.
template <GLenum GLType> struct GLToCType { };

template <> struct GLToCType<GL_BYTE>           { typedef GLbyte type;      };
template <> struct GLToCType<GL_UNSIGNED_BYTE>  { typedef GLubyte type;     };
template <> struct GLToCType<GL_SHORT>          { typedef GLshort type;     };
template <> struct GLToCType<GL_UNSIGNED_SHORT> { typedef GLushort type;    };
template <> struct GLToCType<GL_FIXED>          { typedef GLuint type;      };
template <> struct GLToCType<GL_FLOAT>          { typedef GLfloat type;     };

// This differs from D3DDECLTYPE in that it is unsized. (Size expansion is applied last.)
enum D3DVertexType
{
    D3DVT_FLOAT,
    D3DVT_SHORT,
    D3DVT_SHORT_NORM,
    D3DVT_UBYTE,
    D3DVT_UBYTE_NORM,
    D3DVT_USHORT_NORM
};

// D3DToCType maps from D3D vertex type (as enum D3DVertexType) to the corresponding C type.
template <unsigned int D3DType> struct D3DToCType { };

template <> struct D3DToCType<D3DVT_FLOAT> { typedef float type; };
template <> struct D3DToCType<D3DVT_SHORT> { typedef short type; };
template <> struct D3DToCType<D3DVT_SHORT_NORM> { typedef short type; };
template <> struct D3DToCType<D3DVT_UBYTE> { typedef unsigned char type; };
template <> struct D3DToCType<D3DVT_UBYTE_NORM> { typedef unsigned char type; };
template <> struct D3DToCType<D3DVT_USHORT_NORM> { typedef unsigned short type; };

// Encode the type/size combinations that D3D permits. For each type/size it expands to a widener that will provide the appropriate final size.
template <unsigned int type, int size> struct WidenRule { };

template <int size> struct WidenRule<D3DVT_FLOAT, size>          : NoWiden<size> { };
template <int size> struct WidenRule<D3DVT_SHORT, size>          : WidenToEven<size> { };
template <int size> struct WidenRule<D3DVT_SHORT_NORM, size>     : WidenToEven<size> { };
template <int size> struct WidenRule<D3DVT_UBYTE, size>          : WidenToFour<size> { };
template <int size> struct WidenRule<D3DVT_UBYTE_NORM, size>     : WidenToFour<size> { };
template <int size> struct WidenRule<D3DVT_USHORT_NORM, size>    : WidenToEven<size> { };

// VertexTypeFlags encodes the D3DCAPS9::DeclType flag and vertex declaration flag for each D3D vertex type & size combination.
template <unsigned int d3dtype, int size> struct VertexTypeFlags { };

template <unsigned int _capflag, unsigned int _declflag>
struct VertexTypeFlagsHelper
{
    enum { capflag = _capflag };
    enum { declflag = _declflag };
};

template <> struct VertexTypeFlags<D3DVT_FLOAT, 1> : VertexTypeFlagsHelper<0, D3DDECLTYPE_FLOAT1> { };
template <> struct VertexTypeFlags<D3DVT_FLOAT, 2> : VertexTypeFlagsHelper<0, D3DDECLTYPE_FLOAT2> { };
template <> struct VertexTypeFlags<D3DVT_FLOAT, 3> : VertexTypeFlagsHelper<0, D3DDECLTYPE_FLOAT3> { };
template <> struct VertexTypeFlags<D3DVT_FLOAT, 4> : VertexTypeFlagsHelper<0, D3DDECLTYPE_FLOAT4> { };
template <> struct VertexTypeFlags<D3DVT_SHORT, 2> : VertexTypeFlagsHelper<0, D3DDECLTYPE_SHORT2> { };
template <> struct VertexTypeFlags<D3DVT_SHORT, 4> : VertexTypeFlagsHelper<0, D3DDECLTYPE_SHORT4> { };
template <> struct VertexTypeFlags<D3DVT_SHORT_NORM, 2> : VertexTypeFlagsHelper<D3DDTCAPS_SHORT2N, D3DDECLTYPE_SHORT2N> { };
template <> struct VertexTypeFlags<D3DVT_SHORT_NORM, 4> : VertexTypeFlagsHelper<D3DDTCAPS_SHORT4N, D3DDECLTYPE_SHORT4N> { };
template <> struct VertexTypeFlags<D3DVT_UBYTE, 4> : VertexTypeFlagsHelper<D3DDTCAPS_UBYTE4, D3DDECLTYPE_UBYTE4> { };
template <> struct VertexTypeFlags<D3DVT_UBYTE_NORM, 4> : VertexTypeFlagsHelper<D3DDTCAPS_UBYTE4N, D3DDECLTYPE_UBYTE4N> { };
template <> struct VertexTypeFlags<D3DVT_USHORT_NORM, 2> : VertexTypeFlagsHelper<D3DDTCAPS_USHORT2N, D3DDECLTYPE_USHORT2N> { };
template <> struct VertexTypeFlags<D3DVT_USHORT_NORM, 4> : VertexTypeFlagsHelper<D3DDTCAPS_USHORT4N, D3DDECLTYPE_USHORT4N> { };


// VertexTypeMapping maps GL type & normalized flag to preferred and fallback D3D vertex types (as D3DVertexType enums).
template <GLenum GLtype, bool normalized> struct VertexTypeMapping { };

template <D3DVertexType Preferred, D3DVertexType Fallback = Preferred>
struct VertexTypeMappingBase
{
    enum { preferred = Preferred };
    enum { fallback = Fallback };
};

template <> struct VertexTypeMapping<GL_BYTE, false>                        : VertexTypeMappingBase<D3DVT_SHORT> { };                       // Cast
template <> struct VertexTypeMapping<GL_BYTE, true>                         : VertexTypeMappingBase<D3DVT_FLOAT> { };                       // Normalize
template <> struct VertexTypeMapping<GL_UNSIGNED_BYTE, false>               : VertexTypeMappingBase<D3DVT_UBYTE, D3DVT_FLOAT> { };          // Identity, Cast
template <> struct VertexTypeMapping<GL_UNSIGNED_BYTE, true>                : VertexTypeMappingBase<D3DVT_UBYTE_NORM, D3DVT_FLOAT> { };     // Identity, Normalize
template <> struct VertexTypeMapping<GL_SHORT, false>                       : VertexTypeMappingBase<D3DVT_SHORT> { };                       // Identity
template <> struct VertexTypeMapping<GL_SHORT, true>                        : VertexTypeMappingBase<D3DVT_SHORT_NORM, D3DVT_FLOAT> { };     // Cast, Normalize
template <> struct VertexTypeMapping<GL_UNSIGNED_SHORT, false>              : VertexTypeMappingBase<D3DVT_FLOAT> { };                       // Cast
template <> struct VertexTypeMapping<GL_UNSIGNED_SHORT, true>               : VertexTypeMappingBase<D3DVT_USHORT_NORM, D3DVT_FLOAT> { };    // Cast, Normalize
template <bool normalized> struct VertexTypeMapping<GL_FIXED, normalized>   : VertexTypeMappingBase<D3DVT_FLOAT> { };                       // FixedToFloat
template <bool normalized> struct VertexTypeMapping<GL_FLOAT, normalized>   : VertexTypeMappingBase<D3DVT_FLOAT> { };                       // Identity


// Given a GL type & norm flag and a D3D type, ConversionRule provides the type conversion rule (Cast, Normalize, Identity, FixedToFloat).
// The conversion rules themselves are defined in vertexconversion.h.

// Almost all cases are covered by Cast (including those that are actually Identity since Cast<T,T> knows it's an identity mapping).
template <GLenum fromType, bool normalized, unsigned int toType>
struct ConversionRule : Cast<typename GLToCType<fromType>::type, typename D3DToCType<toType>::type> { };

// All conversions from normalized types to float use the Normalize operator.
template <GLenum fromType> struct ConversionRule<fromType, true, D3DVT_FLOAT> : Normalize<typename GLToCType<fromType>::type> { };

// Use a full specialization for this so that it preferentially matches ahead of the generic normalize-to-float rules.
template <> struct ConversionRule<GL_FIXED, true, D3DVT_FLOAT>  : FixedToFloat<GLint, 16> { };
template <> struct ConversionRule<GL_FIXED, false, D3DVT_FLOAT> : FixedToFloat<GLint, 16> { };

// A 2-stage construction is used for DefaultVertexValues because float must use SimpleDefaultValues (i.e. 0/1)
// whether it is normalized or not.
template <class T, bool normalized> struct DefaultVertexValuesStage2 { };

template <class T> struct DefaultVertexValuesStage2<T, true>  : NormalizedDefaultValues<T> { };
template <class T> struct DefaultVertexValuesStage2<T, false> : SimpleDefaultValues<T> { };

// Work out the default value rule for a D3D type (expressed as the C type) and
template <class T, bool normalized> struct DefaultVertexValues : DefaultVertexValuesStage2<T, normalized> { };
template <bool normalized> struct DefaultVertexValues<float, normalized> : SimpleDefaultValues<float> { };

// Policy rules for use with Converter, to choose whether to use the preferred or fallback conversion.
// The fallback conversion produces an output that all D3D9 devices must support.
template <class T> struct UsePreferred { enum { type = T::preferred }; };
template <class T> struct UseFallback { enum { type = T::fallback }; };

// Converter ties it all together. Given an OpenGL type/norm/size and choice of preferred/fallback conversion,
// it provides all the members of the appropriate VertexDataConverter, the D3DCAPS9::DeclTypes flag in cap flag
// and the D3DDECLTYPE member needed for the vertex declaration in declflag.
template <GLenum fromType, bool normalized, int size, template <class T> class PreferenceRule>
struct Converter
    : VertexDataConverter<typename GLToCType<fromType>::type,
                          WidenRule<PreferenceRule< VertexTypeMapping<fromType, normalized> >::type, size>,
                          ConversionRule<fromType,
                                         normalized,
                                         PreferenceRule< VertexTypeMapping<fromType, normalized> >::type>,
                          DefaultVertexValues<typename D3DToCType<PreferenceRule< VertexTypeMapping<fromType, normalized> >::type>::type, normalized > >
{
private:
    enum { d3dtype = PreferenceRule< VertexTypeMapping<fromType, normalized> >::type };
    enum { d3dsize = WidenRule<d3dtype, size>::finalWidth };

public:
    enum { capflag = VertexTypeFlags<d3dtype, d3dsize>::capflag };
    enum { declflag = VertexTypeFlags<d3dtype, d3dsize>::declflag };
};

// Initialize a TranslationInfo
#define TRANSLATION(type, norm, size, preferred)                                    \
    {                                                                               \
        Converter<type, norm, size, preferred>::identity,                           \
        Converter<type, norm, size, preferred>::finalSize,                          \
        Converter<type, norm, size, preferred>::convertArray,                       \
        static_cast<D3DDECLTYPE>(Converter<type, norm, size, preferred>::declflag)  \
    }

#define TRANSLATION_FOR_TYPE_NORM_SIZE(type, norm, size)    \
    {                                                       \
        Converter<type, norm, size, UsePreferred>::capflag, \
        TRANSLATION(type, norm, size, UsePreferred),        \
        TRANSLATION(type, norm, size, UseFallback)          \
    }

#define TRANSLATIONS_FOR_TYPE(type)                                                                                                                                                                         \
    {                                                                                                                                                                                                       \
        { TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 1), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 2), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 3), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 4) }, \
        { TRANSLATION_FOR_TYPE_NORM_SIZE(type, true, 1), TRANSLATION_FOR_TYPE_NORM_SIZE(type, true, 2), TRANSLATION_FOR_TYPE_NORM_SIZE(type, true, 3), TRANSLATION_FOR_TYPE_NORM_SIZE(type, true, 4) },     \
    }

#define TRANSLATIONS_FOR_TYPE_NO_NORM(type)                                                                                                                                                                 \
    {                                                                                                                                                                                                       \
        { TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 1), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 2), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 3), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 4) }, \
        { TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 1), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 2), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 3), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 4) }, \
    }

const TranslationDescription mPossibleTranslations[NUM_GL_VERTEX_ATTRIB_TYPES][2][4] = // [GL types as enumerated by typeIndex()][normalized][size-1]
{
    TRANSLATIONS_FOR_TYPE(GL_BYTE),
    TRANSLATIONS_FOR_TYPE(GL_UNSIGNED_BYTE),
    TRANSLATIONS_FOR_TYPE(GL_SHORT),
    TRANSLATIONS_FOR_TYPE(GL_UNSIGNED_SHORT),
    TRANSLATIONS_FOR_TYPE_NO_NORM(GL_FIXED),
    TRANSLATIONS_FOR_TYPE_NO_NORM(GL_FLOAT)
};

void InitializeVertexTranslations(const rx::Renderer9 *renderer)
{
    DWORD declTypes = renderer->getCapsDeclTypes();

    for (unsigned int i = 0; i < NUM_GL_VERTEX_ATTRIB_TYPES; i++)
    {
        for (unsigned int j = 0; j < 2; j++)
        {
            for (unsigned int k = 0; k < 4; k++)
            {
                if (mPossibleTranslations[i][j][k].capsFlag == 0 || (declTypes & mPossibleTranslations[i][j][k].capsFlag) != 0)
                {
                    mFormatConverters[i][j][k] = mPossibleTranslations[i][j][k].preferredConversion;
                }
                else
                {
                    mFormatConverters[i][j][k] = mPossibleTranslations[i][j][k].fallbackConversion;
                }
            }
        }
    }
}

unsigned int typeIndex(GLenum type)
{
    switch (type)
    {
      case GL_BYTE: return 0;
      case GL_UNSIGNED_BYTE: return 1;
      case GL_SHORT: return 2;
      case GL_UNSIGNED_SHORT: return 3;
      case GL_FIXED: return 4;
      case GL_FLOAT: return 5;

      default: UNREACHABLE(); return 5;
    }
}

const FormatConverter &formatConverter(const gl::VertexFormat &vertexFormat)
{
    // Pure integer attributes only supported in ES3.0
    ASSERT(!vertexFormat.mPureInteger);
    return mFormatConverters[typeIndex(vertexFormat.mType)][vertexFormat.mNormalized][vertexFormat.mComponents - 1];
}

VertexCopyFunction GetVertexCopyFunction(const gl::VertexFormat &vertexFormat)
{
    return formatConverter(vertexFormat).convertArray;
}

size_t GetVertexElementSize(const gl::VertexFormat &vertexFormat)
{
    return formatConverter(vertexFormat).outputElementSize;
}

VertexConversionType GetVertexConversionType(const gl::VertexFormat &vertexFormat)
{
    return (formatConverter(vertexFormat).identity ? VERTEX_CONVERT_NONE : VERTEX_CONVERT_CPU);
}

D3DDECLTYPE GetNativeVertexFormat(const gl::VertexFormat &vertexFormat)
{
    return formatConverter(vertexFormat).d3dDeclType;
}

}

namespace gl_d3d9
{

D3DFORMAT GetTextureFormat(GLenum internalFormat, const Renderer9 *renderer)
{
    if (!renderer)
    {
        UNREACHABLE();
        return D3DFMT_UNKNOWN;
    }

    ASSERT(renderer->getCurrentClientVersion() == 2);

    D3D9FormatInfo d3d9FormatInfo;
    if (GetD3D9FormatInfo(internalFormat, &d3d9FormatInfo))
    {
        return d3d9FormatInfo.mTexFormat(renderer);
    }
    else
    {
        UNREACHABLE();
        return D3DFMT_UNKNOWN;
    }
}

D3DFORMAT GetRenderFormat(GLenum internalFormat, const Renderer9 *renderer)
{
    if (!renderer)
    {
        UNREACHABLE();
        return D3DFMT_UNKNOWN;
    }

    ASSERT(renderer->getCurrentClientVersion() == 2);

    D3D9FormatInfo d3d9FormatInfo;
    if (GetD3D9FormatInfo(internalFormat, &d3d9FormatInfo))
    {
        return d3d9FormatInfo.mRenderFormat(renderer);
    }
    else
    {
        UNREACHABLE();
        return D3DFMT_UNKNOWN;
    }
}

D3DMULTISAMPLE_TYPE GetMultisampleType(GLsizei samples)
{
    return (samples > 1) ? static_cast<D3DMULTISAMPLE_TYPE>(samples) : D3DMULTISAMPLE_NONE;
}

bool RequiresTextureDataInitialization(GLint internalFormat)
{
    const InternalFormatInitialzerMap &map = GetInternalFormatInitialzerMap();
    return map.find(internalFormat) != map.end();
}

InitializeTextureDataFunction GetTextureDataInitializationFunction(GLint internalFormat)
{
    const InternalFormatInitialzerMap &map = GetInternalFormatInitialzerMap();
    InternalFormatInitialzerMap::const_iterator iter = map.find(internalFormat);
    if (iter != map.end())
    {
        return iter->second;
    }
    else
    {
        UNREACHABLE();
        return NULL;
    }
}

}

namespace d3d9_gl
{

GLenum GetInternalFormat(D3DFORMAT format)
{
    static const D3D9FormatInfoMap infoMap = BuildD3D9FormatInfoMap();
    D3D9FormatInfoMap::const_iterator iter = infoMap.find(format);
    if (iter != infoMap.end())
    {
        return iter->second.mInternalFormat;
    }
    else
    {
        UNREACHABLE();
        return GL_NONE;
    }
}

GLsizei GetSamplesCount(D3DMULTISAMPLE_TYPE type)
{
    return (type != D3DMULTISAMPLE_NONMASKABLE) ? type : 0;
}

bool IsFormatChannelEquivalent(D3DFORMAT d3dformat, GLenum format, GLuint clientVersion)
{
    GLenum internalFormat = d3d9_gl::GetInternalFormat(d3dformat);
    GLenum convertedFormat = gl::GetFormat(internalFormat, clientVersion);
    return convertedFormat == format;
}

}

}
