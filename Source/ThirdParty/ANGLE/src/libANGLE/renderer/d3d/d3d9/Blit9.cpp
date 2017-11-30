//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Blit9.cpp: Surface copy utility class.

#include "libANGLE/renderer/d3d/d3d9/Blit9.h"

#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/d3d9/renderer9_utils.h"
#include "libANGLE/renderer/d3d/d3d9/formatutils9.h"
#include "libANGLE/renderer/d3d/d3d9/TextureStorage9.h"
#include "libANGLE/renderer/d3d/d3d9/RenderTarget9.h"
#include "libANGLE/renderer/d3d/d3d9/Renderer9.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"

namespace
{
// Precompiled shaders
#include "libANGLE/renderer/d3d/d3d9/shaders/compiled/standardvs.h"
#include "libANGLE/renderer/d3d/d3d9/shaders/compiled/passthroughps.h"
#include "libANGLE/renderer/d3d/d3d9/shaders/compiled/luminanceps.h"
#include "libANGLE/renderer/d3d/d3d9/shaders/compiled/luminancepremultps.h"
#include "libANGLE/renderer/d3d/d3d9/shaders/compiled/luminanceunmultps.h"
#include "libANGLE/renderer/d3d/d3d9/shaders/compiled/componentmaskps.h"
#include "libANGLE/renderer/d3d/d3d9/shaders/compiled/componentmaskpremultps.h"
#include "libANGLE/renderer/d3d/d3d9/shaders/compiled/componentmaskunmultps.h"

const BYTE *const g_shaderCode[] = {
    g_vs20_standardvs,
    g_ps20_passthroughps,
    g_ps20_luminanceps,
    g_ps20_luminancepremultps,
    g_ps20_luminanceunmultps,
    g_ps20_componentmaskps,
    g_ps20_componentmaskpremultps,
    g_ps20_componentmaskunmultps,
};

const size_t g_shaderSize[] = {
    sizeof(g_vs20_standardvs),
    sizeof(g_ps20_passthroughps),
    sizeof(g_ps20_luminanceps),
    sizeof(g_ps20_luminancepremultps),
    sizeof(g_ps20_luminanceunmultps),
    sizeof(g_ps20_componentmaskps),
    sizeof(g_ps20_componentmaskpremultps),
    sizeof(g_ps20_componentmaskunmultps),
};
}

namespace rx
{

Blit9::Blit9(Renderer9 *renderer)
    : mRenderer(renderer),
      mGeometryLoaded(false),
      mQuadVertexBuffer(nullptr),
      mQuadVertexDeclaration(nullptr),
      mSavedStateBlock(nullptr),
      mSavedRenderTarget(nullptr),
      mSavedDepthStencil(nullptr)
{
    memset(mCompiledShaders, 0, sizeof(mCompiledShaders));
}

Blit9::~Blit9()
{
    SafeRelease(mSavedStateBlock);
    SafeRelease(mQuadVertexBuffer);
    SafeRelease(mQuadVertexDeclaration);

    for (int i = 0; i < SHADER_COUNT; i++)
    {
        SafeRelease(mCompiledShaders[i]);
    }
}

gl::Error Blit9::initialize()
{
    if (mGeometryLoaded)
    {
        return gl::NoError();
    }

    static const float quad[] =
    {
        -1, -1,
        -1,  1,
         1, -1,
         1,  1
    };

    IDirect3DDevice9 *device = mRenderer->getDevice();

    HRESULT result = device->CreateVertexBuffer(sizeof(quad), D3DUSAGE_WRITEONLY, 0,
                                                D3DPOOL_DEFAULT, &mQuadVertexBuffer, nullptr);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return gl::OutOfMemory() << "Failed to create internal blit vertex shader, "
                                 << gl::FmtHR(result);
    }

    void *lockPtr = nullptr;
    result = mQuadVertexBuffer->Lock(0, 0, &lockPtr, 0);

    if (FAILED(result) || lockPtr == nullptr)
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        SafeRelease(mQuadVertexBuffer);
        return gl::OutOfMemory() << "Failed to lock internal blit vertex shader, "
                                 << gl::FmtHR(result);
    }

    memcpy(lockPtr, quad, sizeof(quad));
    mQuadVertexBuffer->Unlock();

    static const D3DVERTEXELEMENT9 elements[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };

    result = device->CreateVertexDeclaration(elements, &mQuadVertexDeclaration);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        SafeRelease(mQuadVertexBuffer);
        return gl::OutOfMemory() << "Failed to lock internal blit vertex declaration, "
                                 << gl::FmtHR(result);
    }

    mGeometryLoaded = true;
    return gl::NoError();
}

template <class D3DShaderType>
gl::Error Blit9::setShader(ShaderId source, const char *profile,
                           gl::Error (Renderer9::*createShader)(const DWORD *, size_t length, D3DShaderType **outShader),
                           HRESULT (WINAPI IDirect3DDevice9::*setShader)(D3DShaderType*))
{
    IDirect3DDevice9 *device = mRenderer->getDevice();

    D3DShaderType *shader = nullptr;

    if (mCompiledShaders[source] != nullptr)
    {
        shader = static_cast<D3DShaderType*>(mCompiledShaders[source]);
    }
    else
    {
        const BYTE* shaderCode = g_shaderCode[source];
        size_t shaderSize = g_shaderSize[source];
        ANGLE_TRY((mRenderer->*createShader)(reinterpret_cast<const DWORD*>(shaderCode), shaderSize, &shader));
        mCompiledShaders[source] = shader;
    }

    HRESULT hr = (device->*setShader)(shader);
    if (FAILED(hr))
    {
        return gl::OutOfMemory() << "Failed to set shader for blit operation, " << gl::FmtHR(hr);
    }

    return gl::NoError();
}

gl::Error Blit9::setVertexShader(ShaderId shader)
{
    return setShader<IDirect3DVertexShader9>(shader, "vs_2_0", &Renderer9::createVertexShader, &IDirect3DDevice9::SetVertexShader);
}

gl::Error Blit9::setPixelShader(ShaderId shader)
{
    return setShader<IDirect3DPixelShader9>(shader, "ps_2_0", &Renderer9::createPixelShader, &IDirect3DDevice9::SetPixelShader);
}

RECT Blit9::getSurfaceRect(IDirect3DSurface9 *surface) const
{
    D3DSURFACE_DESC desc;
    surface->GetDesc(&desc);

    RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = desc.Width;
    rect.bottom = desc.Height;

    return rect;
}

gl::Extents Blit9::getSurfaceSize(IDirect3DSurface9 *surface) const
{
    D3DSURFACE_DESC desc;
    surface->GetDesc(&desc);

    return gl::Extents(desc.Width, desc.Height, 1);
}

gl::Error Blit9::boxFilter(IDirect3DSurface9 *source, IDirect3DSurface9 *dest)
{
    ANGLE_TRY(initialize());

    IDirect3DBaseTexture9 *texture = nullptr;
    ANGLE_TRY(copySurfaceToTexture(source, getSurfaceRect(source), &texture));

    IDirect3DDevice9 *device = mRenderer->getDevice();

    saveState();

    device->SetTexture(0, texture);
    device->SetRenderTarget(0, dest);

    ANGLE_TRY(setVertexShader(SHADER_VS_STANDARD));
    ANGLE_TRY(setPixelShader(SHADER_PS_PASSTHROUGH));

    setCommonBlitState();
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    setViewportAndShaderConstants(getSurfaceRect(source), getSurfaceSize(source),
                                  getSurfaceRect(dest), false);

    render();

    SafeRelease(texture);

    restoreState();

    return gl::NoError();
}

gl::Error Blit9::copy2D(const gl::Context *context,
                        const gl::Framebuffer *framebuffer,
                        const RECT &sourceRect,
                        GLenum destFormat,
                        const gl::Offset &destOffset,
                        TextureStorage *storage,
                        GLint level)
{
    ANGLE_TRY(initialize());

    const gl::FramebufferAttachment *colorbuffer = framebuffer->getColorbuffer(0);
    ASSERT(colorbuffer);

    RenderTarget9 *renderTarget9 = nullptr;
    ANGLE_TRY(colorbuffer->getRenderTarget(context, &renderTarget9));
    ASSERT(renderTarget9);

    IDirect3DSurface9 *source = renderTarget9->getSurface();
    ASSERT(source);

    IDirect3DSurface9 *destSurface = nullptr;
    TextureStorage9 *storage9      = GetAs<TextureStorage9>(storage);
    gl::Error error = storage9->getSurfaceLevel(context, GL_TEXTURE_2D, level, true, &destSurface);
    if (error.isError())
    {
        SafeRelease(source);
        return error;
    }
    ASSERT(destSurface);

    gl::Error result =
        copy(source, nullptr, sourceRect, destFormat, destOffset, destSurface, false, false, false);

    SafeRelease(destSurface);
    SafeRelease(source);

    return result;
}

gl::Error Blit9::copyCube(const gl::Context *context,
                          const gl::Framebuffer *framebuffer,
                          const RECT &sourceRect,
                          GLenum destFormat,
                          const gl::Offset &destOffset,
                          TextureStorage *storage,
                          GLenum target,
                          GLint level)
{
    gl::Error error = initialize();
    if (error.isError())
    {
        return error;
    }

    const gl::FramebufferAttachment *colorbuffer = framebuffer->getColorbuffer(0);
    ASSERT(colorbuffer);

    RenderTarget9 *renderTarget9 = nullptr;
    error                        = colorbuffer->getRenderTarget(context, &renderTarget9);
    if (error.isError())
    {
        return error;
    }
    ASSERT(renderTarget9);

    IDirect3DSurface9 *source = renderTarget9->getSurface();
    ASSERT(source);

    IDirect3DSurface9 *destSurface = nullptr;
    TextureStorage9 *storage9      = GetAs<TextureStorage9>(storage);
    error = storage9->getSurfaceLevel(context, target, level, true, &destSurface);
    if (error.isError())
    {
        SafeRelease(source);
        return error;
    }
    ASSERT(destSurface);

    gl::Error result =
        copy(source, nullptr, sourceRect, destFormat, destOffset, destSurface, false, false, false);

    SafeRelease(destSurface);
    SafeRelease(source);

    return result;
}

gl::Error Blit9::copyTexture(const gl::Context *context,
                             const gl::Texture *source,
                             GLint sourceLevel,
                             const RECT &sourceRect,
                             GLenum destFormat,
                             const gl::Offset &destOffset,
                             TextureStorage *storage,
                             GLenum destTarget,
                             GLint destLevel,
                             bool flipY,
                             bool premultiplyAlpha,
                             bool unmultiplyAlpha)
{
    ANGLE_TRY(initialize());

    const TextureD3D *sourceD3D = GetImplAs<TextureD3D>(source);

    TextureStorage *sourceStorage = nullptr;
    ANGLE_TRY(const_cast<TextureD3D *>(sourceD3D)->getNativeTexture(context, &sourceStorage));

    TextureStorage9_2D *sourceStorage9 = GetAs<TextureStorage9_2D>(sourceStorage);
    ASSERT(sourceStorage9);

    TextureStorage9 *destStorage9 = GetAs<TextureStorage9>(storage);
    ASSERT(destStorage9);

    ASSERT(sourceLevel == 0);
    IDirect3DBaseTexture9 *sourceTexture = nullptr;
    ANGLE_TRY(sourceStorage9->getBaseTexture(context, &sourceTexture));

    IDirect3DSurface9 *sourceSurface = nullptr;
    ANGLE_TRY(
        sourceStorage9->getSurfaceLevel(context, GL_TEXTURE_2D, sourceLevel, true, &sourceSurface));

    IDirect3DSurface9 *destSurface = nullptr;
    gl::Error error =
        destStorage9->getSurfaceLevel(context, destTarget, destLevel, true, &destSurface);
    if (error.isError())
    {
        SafeRelease(sourceSurface);
        return error;
    }

    error = copy(sourceSurface, sourceTexture, sourceRect, destFormat, destOffset, destSurface,
                 flipY, premultiplyAlpha, unmultiplyAlpha);

    SafeRelease(sourceSurface);
    SafeRelease(destSurface);

    return error;
}

gl::Error Blit9::copy(IDirect3DSurface9 *source,
                      IDirect3DBaseTexture9 *sourceTexture,
                      const RECT &sourceRect,
                      GLenum destFormat,
                      const gl::Offset &destOffset,
                      IDirect3DSurface9 *dest,
                      bool flipY,
                      bool premultiplyAlpha,
                      bool unmultiplyAlpha)
{
    ASSERT(source != nullptr && dest != nullptr);

    IDirect3DDevice9 *device = mRenderer->getDevice();

    D3DSURFACE_DESC sourceDesc;
    D3DSURFACE_DESC destDesc;
    source->GetDesc(&sourceDesc);
    dest->GetDesc(&destDesc);

    // Check if it's possible to use StetchRect
    if (sourceDesc.Format == destDesc.Format && (destDesc.Usage & D3DUSAGE_RENDERTARGET) &&
        d3d9_gl::IsFormatChannelEquivalent(destDesc.Format, destFormat) && !flipY &&
        premultiplyAlpha == unmultiplyAlpha)
    {
        RECT destRect = { destOffset.x, destOffset.y, destOffset.x + (sourceRect.right - sourceRect.left), destOffset.y + (sourceRect.bottom - sourceRect.top)};
        HRESULT result = device->StretchRect(source, &sourceRect, dest, &destRect, D3DTEXF_POINT);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return gl::OutOfMemory()
                   << "Failed to blit between textures, StretchRect " << gl::FmtHR(result);
        }

        return gl::NoError();
    }
    else
    {
        IDirect3DBaseTexture9 *texture = sourceTexture;
        RECT adjustedSourceRect        = sourceRect;
        gl::Extents sourceSize(sourceDesc.Width, sourceDesc.Height, 1);

        if (texture == nullptr)
        {
            ANGLE_TRY(copySurfaceToTexture(source, sourceRect, &texture));

            // copySurfaceToTexture only copies in the sourceRect area of the source surface.
            // Adjust sourceRect so that it is now covering the entire source texture
            adjustedSourceRect.left   = 0;
            adjustedSourceRect.right  = sourceRect.right - sourceRect.left;
            adjustedSourceRect.top    = 0;
            adjustedSourceRect.bottom = sourceRect.bottom - sourceRect.top;

            sourceSize.width  = sourceRect.right - sourceRect.left;
            sourceSize.height = sourceRect.bottom - sourceRect.top;
        }
        else
        {
            texture->AddRef();
        }

        gl::Error error = formatConvert(texture, adjustedSourceRect, sourceSize, destFormat,
                                        destOffset, dest, flipY, premultiplyAlpha, unmultiplyAlpha);

        SafeRelease(texture);

        return error;
    }
}

gl::Error Blit9::formatConvert(IDirect3DBaseTexture9 *source,
                               const RECT &sourceRect,
                               const gl::Extents &sourceSize,
                               GLenum destFormat,
                               const gl::Offset &destOffset,
                               IDirect3DSurface9 *dest,
                               bool flipY,
                               bool premultiplyAlpha,
                               bool unmultiplyAlpha)
{
    ANGLE_TRY(initialize());

    IDirect3DDevice9 *device = mRenderer->getDevice();

    saveState();

    device->SetTexture(0, source);
    device->SetRenderTarget(0, dest);

    RECT destRect;
    destRect.left   = destOffset.x;
    destRect.right  = destOffset.x + (sourceRect.right - sourceRect.left);
    destRect.top    = destOffset.y;
    destRect.bottom = destOffset.y + (sourceRect.bottom - sourceRect.top);

    setViewportAndShaderConstants(sourceRect, sourceSize, destRect, flipY);

    setCommonBlitState();

    gl::Error error = setFormatConvertShaders(destFormat, flipY, premultiplyAlpha, unmultiplyAlpha);
    if (!error.isError())
    {
        render();
    }

    restoreState();

    return error;
}

gl::Error Blit9::setFormatConvertShaders(GLenum destFormat,
                                         bool flipY,
                                         bool premultiplyAlpha,
                                         bool unmultiplyAlpha)
{
    ANGLE_TRY(setVertexShader(SHADER_VS_STANDARD));

    switch (destFormat)
    {
      case GL_RGBA:
      case GL_BGRA_EXT:
      case GL_RGB:
      case GL_RG_EXT:
      case GL_RED_EXT:
      case GL_ALPHA:
          if (premultiplyAlpha == unmultiplyAlpha)
          {
              ANGLE_TRY(setPixelShader(SHADER_PS_COMPONENTMASK));
          }
          else if (premultiplyAlpha)
          {
              ANGLE_TRY(setPixelShader(SHADER_PS_COMPONENTMASK_PREMULTIPLY_ALPHA));
          }
          else
          {
              ASSERT(unmultiplyAlpha);
              ANGLE_TRY(setPixelShader(SHADER_PS_COMPONENTMASK_UNMULTIPLY_ALPHA));
          }
          break;

      case GL_LUMINANCE:
      case GL_LUMINANCE_ALPHA:
          if (premultiplyAlpha == unmultiplyAlpha)
          {
              ANGLE_TRY(setPixelShader(SHADER_PS_LUMINANCE));
          }
          else if (premultiplyAlpha)
          {
              ANGLE_TRY(setPixelShader(SHADER_PS_LUMINANCE_PREMULTIPLY_ALPHA));
          }
          else
          {
              ASSERT(unmultiplyAlpha);
              ANGLE_TRY(setPixelShader(SHADER_PS_LUMINANCE_UNMULTIPLY_ALPHA));
          }
          break;

      default:
          UNREACHABLE();
    }

    enum { X = 0, Y = 1, Z = 2, W = 3 };

    // The meaning of this constant depends on the shader that was selected.
    // See the shader assembly code above for details.
    // Allocate one array for both registers and split it into two float4's.
    float psConst[8] = { 0 };
    float *multConst = &psConst[0];
    float *addConst = &psConst[4];

    switch (destFormat)
    {
      default: UNREACHABLE();
      case GL_RGBA:
      case GL_BGRA_EXT:
        multConst[X] = 1;
        multConst[Y] = 1;
        multConst[Z] = 1;
        multConst[W] = 1;
        addConst[X] = 0;
        addConst[Y] = 0;
        addConst[Z] = 0;
        addConst[W] = 0;
        break;

      case GL_RGB:
        multConst[X] = 1;
        multConst[Y] = 1;
        multConst[Z] = 1;
        multConst[W] = 0;
        addConst[X] = 0;
        addConst[Y] = 0;
        addConst[Z] = 0;
        addConst[W] = 1;
        break;

      case GL_RG_EXT:
        multConst[X] = 1;
        multConst[Y] = 1;
        multConst[Z] = 0;
        multConst[W] = 0;
        addConst[X] = 0;
        addConst[Y] = 0;
        addConst[Z] = 0;
        addConst[W] = 1;
        break;

      case GL_RED_EXT:
        multConst[X] = 1;
        multConst[Y] = 0;
        multConst[Z] = 0;
        multConst[W] = 0;
        addConst[X] = 0;
        addConst[Y] = 0;
        addConst[Z] = 0;
        addConst[W] = 1;
        break;

      case GL_ALPHA:
        multConst[X] = 0;
        multConst[Y] = 0;
        multConst[Z] = 0;
        multConst[W] = 1;
        addConst[X] = 0;
        addConst[Y] = 0;
        addConst[Z] = 0;
        addConst[W] = 0;
        break;

      case GL_LUMINANCE:
        multConst[X] = 1;
        multConst[Y] = 0;
        multConst[Z] = 0;
        multConst[W] = 0;
        addConst[X] = 0;
        addConst[Y] = 0;
        addConst[Z] = 0;
        addConst[W] = 1;
        break;

      case GL_LUMINANCE_ALPHA:
        multConst[X] = 1;
        multConst[Y] = 0;
        multConst[Z] = 0;
        multConst[W] = 1;
        addConst[X] = 0;
        addConst[Y] = 0;
        addConst[Z] = 0;
        addConst[W] = 0;
        break;
    }

    mRenderer->getDevice()->SetPixelShaderConstantF(0, psConst, 2);

    return gl::NoError();
}

gl::Error Blit9::copySurfaceToTexture(IDirect3DSurface9 *surface,
                                      const RECT &sourceRect,
                                      IDirect3DBaseTexture9 **outTexture)
{
    ASSERT(surface);

    IDirect3DDevice9 *device = mRenderer->getDevice();

    D3DSURFACE_DESC sourceDesc;
    surface->GetDesc(&sourceDesc);

    // Copy the render target into a texture
    IDirect3DTexture9 *texture;
    HRESULT result = device->CreateTexture(
        sourceRect.right - sourceRect.left, sourceRect.bottom - sourceRect.top, 1,
        D3DUSAGE_RENDERTARGET, sourceDesc.Format, D3DPOOL_DEFAULT, &texture, nullptr);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return gl::OutOfMemory() << "Failed to allocate internal texture for blit, "
                                 << gl::FmtHR(result);
    }

    IDirect3DSurface9 *textureSurface;
    result = texture->GetSurfaceLevel(0, &textureSurface);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        SafeRelease(texture);
        return gl::OutOfMemory() << "Failed to query surface of internal blit texture, "
                                 << gl::FmtHR(result);
    }

    mRenderer->endScene();
    result = device->StretchRect(surface, &sourceRect, textureSurface, nullptr, D3DTEXF_NONE);

    SafeRelease(textureSurface);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        SafeRelease(texture);
        return gl::OutOfMemory() << "Failed to copy between internal blit textures, "
                                 << gl::FmtHR(result);
    }

    *outTexture = texture;
    return gl::NoError();
}

void Blit9::setViewportAndShaderConstants(const RECT &sourceRect,
                                          const gl::Extents &sourceSize,
                                          const RECT &destRect,
                                          bool flipY)
{
    IDirect3DDevice9 *device = mRenderer->getDevice();

    D3DVIEWPORT9 vp;
    vp.X      = destRect.left;
    vp.Y      = destRect.top;
    vp.Width  = destRect.right - destRect.left;
    vp.Height = destRect.bottom - destRect.top;
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    device->SetViewport(&vp);

    float vertexConstants[8] = {
        // halfPixelAdjust
        -1.0f / vp.Width, 1.0f / vp.Height, 0, 0,
        // texcoordOffset
        static_cast<float>(sourceRect.left) / sourceSize.width,
        static_cast<float>(flipY ? sourceRect.bottom : sourceRect.top) / sourceSize.height,
        static_cast<float>(sourceRect.right - sourceRect.left) / sourceSize.width,
        static_cast<float>(flipY ? sourceRect.top - sourceRect.bottom
                                 : sourceRect.bottom - sourceRect.top) /
            sourceSize.height,
    };

    device->SetVertexShaderConstantF(0, vertexConstants, 2);
}

void Blit9::setCommonBlitState()
{
    IDirect3DDevice9 *device = mRenderer->getDevice();

    device->SetDepthStencilSurface(nullptr);

    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    RECT scissorRect = {0};   // Scissoring is disabled for flipping, but we need this to capture and restore the old rectangle
    device->SetScissorRect(&scissorRect);

    for(int i = 0; i < gl::MAX_VERTEX_ATTRIBS; i++)
    {
        device->SetStreamSourceFreq(i, 1);
    }
}

void Blit9::render()
{
    IDirect3DDevice9 *device = mRenderer->getDevice();

    HRESULT hr = device->SetStreamSource(0, mQuadVertexBuffer, 0, 2 * sizeof(float));
    hr = device->SetVertexDeclaration(mQuadVertexDeclaration);

    mRenderer->startScene();
    hr = device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}

void Blit9::saveState()
{
    IDirect3DDevice9 *device = mRenderer->getDevice();

    HRESULT hr;

    device->GetDepthStencilSurface(&mSavedDepthStencil);
    device->GetRenderTarget(0, &mSavedRenderTarget);

    if (mSavedStateBlock == nullptr)
    {
        hr = device->BeginStateBlock();
        ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);

        setCommonBlitState();

        static const float dummyConst[8] = { 0 };

        device->SetVertexShader(nullptr);
        device->SetVertexShaderConstantF(0, dummyConst, 2);
        device->SetPixelShader(nullptr);
        device->SetPixelShaderConstantF(0, dummyConst, 2);

        D3DVIEWPORT9 dummyVp;
        dummyVp.X = 0;
        dummyVp.Y = 0;
        dummyVp.Width = 1;
        dummyVp.Height = 1;
        dummyVp.MinZ = 0;
        dummyVp.MaxZ = 1;

        device->SetViewport(&dummyVp);

        device->SetTexture(0, nullptr);

        device->SetStreamSource(0, mQuadVertexBuffer, 0, 0);

        device->SetVertexDeclaration(mQuadVertexDeclaration);

        hr = device->EndStateBlock(&mSavedStateBlock);
        ASSERT(SUCCEEDED(hr) || hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY);
    }

    ASSERT(mSavedStateBlock != nullptr);

    if (mSavedStateBlock != nullptr)
    {
        hr = mSavedStateBlock->Capture();
        ASSERT(SUCCEEDED(hr));
    }
}

void Blit9::restoreState()
{
    IDirect3DDevice9 *device = mRenderer->getDevice();

    device->SetDepthStencilSurface(mSavedDepthStencil);
    SafeRelease(mSavedDepthStencil);

    device->SetRenderTarget(0, mSavedRenderTarget);
    SafeRelease(mSavedRenderTarget);

    ASSERT(mSavedStateBlock != nullptr);

    if (mSavedStateBlock != nullptr)
    {
        mSavedStateBlock->Apply();
    }
}

}
