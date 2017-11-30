//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Blit9.cpp: Surface copy utility class.

#ifndef LIBANGLE_RENDERER_D3D_D3D9_BLIT9_H_
#define LIBANGLE_RENDERER_D3D_D3D9_BLIT9_H_

#include "common/angleutils.h"
#include "libANGLE/Error.h"

#include <GLES2/gl2.h>

namespace gl
{
class Context;
class Framebuffer;
class Texture;
struct Extents;
struct Offset;
}

namespace rx
{
class Renderer9;
class TextureStorage;

class Blit9 : angle::NonCopyable
{
  public:
    explicit Blit9(Renderer9 *renderer);
    ~Blit9();

    gl::Error initialize();

    // Copy from source surface to dest surface.
    // sourceRect, xoffset, yoffset are in D3D coordinates (0,0 in upper-left)
    gl::Error copy2D(const gl::Context *context,
                     const gl::Framebuffer *framebuffer,
                     const RECT &sourceRect,
                     GLenum destFormat,
                     const gl::Offset &destOffset,
                     TextureStorage *storage,
                     GLint level);
    gl::Error copyCube(const gl::Context *context,
                       const gl::Framebuffer *framebuffer,
                       const RECT &sourceRect,
                       GLenum destFormat,
                       const gl::Offset &destOffset,
                       TextureStorage *storage,
                       GLenum target,
                       GLint level);
    gl::Error copyTexture(const gl::Context *context,
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
                          bool unmultiplyAlpha);

    // 2x2 box filter sample from source to dest.
    // Requires that source is RGB(A) and dest has the same format as source.
    gl::Error boxFilter(IDirect3DSurface9 *source, IDirect3DSurface9 *dest);

  private:
    Renderer9 *mRenderer;

    bool mGeometryLoaded;
    IDirect3DVertexBuffer9 *mQuadVertexBuffer;
    IDirect3DVertexDeclaration9 *mQuadVertexDeclaration;

    // Copy from source texture to dest surface.
    // sourceRect, xoffset, yoffset are in D3D coordinates (0,0 in upper-left)
    // source is interpreted as RGBA and destFormat specifies the desired result format. For
    // example, if destFormat = GL_RGB, the alpha channel will be forced to 0.
    gl::Error formatConvert(IDirect3DBaseTexture9 *source,
                            const RECT &sourceRect,
                            const gl::Extents &sourceSize,
                            GLenum destFormat,
                            const gl::Offset &destOffset,
                            IDirect3DSurface9 *dest,
                            bool flipY,
                            bool premultiplyAlpha,
                            bool unmultiplyAlpha);
    gl::Error setFormatConvertShaders(GLenum destFormat,
                                      bool flipY,
                                      bool premultiplyAlpha,
                                      bool unmultiplyAlpha);

    gl::Error copy(IDirect3DSurface9 *source,
                   IDirect3DBaseTexture9 *sourceTexture,
                   const RECT &sourceRect,
                   GLenum destFormat,
                   const gl::Offset &destOffset,
                   IDirect3DSurface9 *dest,
                   bool flipY,
                   bool premultiplyAlpha,
                   bool unmultiplyAlpha);
    gl::Error copySurfaceToTexture(IDirect3DSurface9 *surface,
                                   const RECT &sourceRect,
                                   IDirect3DBaseTexture9 **outTexture);
    void setViewportAndShaderConstants(const RECT &sourceRect,
                                       const gl::Extents &sourceSize,
                                       const RECT &destRect,
                                       bool flipY);
    void setCommonBlitState();
    RECT getSurfaceRect(IDirect3DSurface9 *surface) const;
    gl::Extents getSurfaceSize(IDirect3DSurface9 *surface) const;

    // This enum is used to index mCompiledShaders and mShaderSource.
    enum ShaderId
    {
        SHADER_VS_STANDARD,
        SHADER_PS_PASSTHROUGH,
        SHADER_PS_LUMINANCE,
        SHADER_PS_LUMINANCE_PREMULTIPLY_ALPHA,
        SHADER_PS_LUMINANCE_UNMULTIPLY_ALPHA,
        SHADER_PS_COMPONENTMASK,
        SHADER_PS_COMPONENTMASK_PREMULTIPLY_ALPHA,
        SHADER_PS_COMPONENTMASK_UNMULTIPLY_ALPHA,
        SHADER_COUNT,
    };

    // This actually contains IDirect3DVertexShader9 or IDirect3DPixelShader9 casted to IUnknown.
    IUnknown *mCompiledShaders[SHADER_COUNT];

    template <class D3DShaderType>
    gl::Error setShader(ShaderId source, const char *profile,
                        gl::Error (Renderer9::*createShader)(const DWORD *, size_t length, D3DShaderType **outShader),
                        HRESULT (WINAPI IDirect3DDevice9::*setShader)(D3DShaderType*));

    gl::Error setVertexShader(ShaderId shader);
    gl::Error setPixelShader(ShaderId shader);
    void render();

    void saveState();
    void restoreState();
    IDirect3DStateBlock9 *mSavedStateBlock;
    IDirect3DSurface9 *mSavedRenderTarget;
    IDirect3DSurface9 *mSavedDepthStencil;
};

}

#endif   // LIBANGLE_RENDERER_D3D_D3D9_BLIT9_H_
