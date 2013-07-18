//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Blit.cpp: Surface copy utility class.

#ifndef LIBGLESV2_BLIT_H_
#define LIBGLESV2_BLIT_H_

#include <map>

#define GL_APICALL
#include <GLES2/gl2.h>

#include <d3d9.h>

#include "common/angleutils.h"

#include "libEGL/Display.h"

namespace gl
{
class Context;

class Blit
{
  public:
    explicit Blit(Context *context);
    ~Blit();

    // Copy from source surface to dest surface.
    // sourceRect, xoffset, yoffset are in D3D coordinates (0,0 in upper-left)
    bool copy(IDirect3DSurface9 *source, const RECT &sourceRect, GLenum destFormat, GLint xoffset, GLint yoffset, IDirect3DSurface9 *dest);

    // Copy from source surface to dest surface.
    // sourceRect, xoffset, yoffset are in D3D coordinates (0,0 in upper-left)
    // source is interpreted as RGBA and destFormat specifies the desired result format. For example, if destFormat = GL_RGB, the alpha channel will be forced to 0.
    bool formatConvert(IDirect3DSurface9 *source, const RECT &sourceRect, GLenum destFormat, GLint xoffset, GLint yoffset, IDirect3DSurface9 *dest);

    // 2x2 box filter sample from source to dest.
    // Requires that source is RGB(A) and dest has the same format as source.
    bool boxFilter(IDirect3DSurface9 *source, IDirect3DSurface9 *dest);

  private:
    Context *mContext;

    IDirect3DVertexBuffer9 *mQuadVertexBuffer;
    IDirect3DVertexDeclaration9 *mQuadVertexDeclaration;

    void initGeometry();

    bool setFormatConvertShaders(GLenum destFormat);

    IDirect3DTexture9 *copySurfaceToTexture(IDirect3DSurface9 *surface, const RECT &sourceRect);
    void setViewport(const RECT &sourceRect, GLint xoffset, GLint yoffset);
    void setCommonBlitState();
    RECT getSurfaceRect(IDirect3DSurface9 *surface) const;

    // This enum is used to index mCompiledShaders and mShaderSource.
    enum ShaderId
    {
        SHADER_VS_STANDARD,
        SHADER_VS_FLIPY,
        SHADER_PS_PASSTHROUGH,
        SHADER_PS_LUMINANCE,
        SHADER_PS_COMPONENTMASK,
        SHADER_COUNT
    };

    // This actually contains IDirect3DVertexShader9 or IDirect3DPixelShader9 casted to IUnknown.
    IUnknown *mCompiledShaders[SHADER_COUNT];

    template <class D3DShaderType>
    bool setShader(ShaderId source, const char *profile,
                   D3DShaderType *(egl::Display::*createShader)(const DWORD *, size_t length),
                   HRESULT (WINAPI IDirect3DDevice9::*setShader)(D3DShaderType*));

    bool setVertexShader(ShaderId shader);
    bool setPixelShader(ShaderId shader);
    void render();

    void saveState();
    void restoreState();
    IDirect3DStateBlock9 *mSavedStateBlock;
    IDirect3DSurface9 *mSavedRenderTarget;
    IDirect3DSurface9 *mSavedDepthStencil;

    DISALLOW_COPY_AND_ASSIGN(Blit);
};
}

#endif   // LIBGLESV2_BLIT_H_
