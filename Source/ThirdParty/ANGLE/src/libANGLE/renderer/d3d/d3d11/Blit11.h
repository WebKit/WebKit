//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Blit11.cpp: Texture copy utility class.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_BLIT11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_BLIT11_H_

#include "common/angleutils.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/Error.h"

#include <map>

namespace rx
{
class Renderer11;

class Blit11 : angle::NonCopyable
{
  public:
    explicit Blit11(Renderer11 *renderer);
    ~Blit11();

    gl::Error swizzleTexture(ID3D11ShaderResourceView *source, ID3D11RenderTargetView *dest, const gl::Extents &size,
                             GLenum swizzleRed, GLenum swizzleGreen, GLenum swizzleBlue, GLenum swizzleAlpha);

    gl::Error copyTexture(ID3D11ShaderResourceView *source, const gl::Box &sourceArea, const gl::Extents &sourceSize,
                          ID3D11RenderTargetView *dest, const gl::Box &destArea, const gl::Extents &destSize,
                          const gl::Rectangle *scissor, GLenum destFormat, GLenum filter);

    gl::Error copyStencil(ID3D11Resource *source, unsigned int sourceSubresource, const gl::Box &sourceArea, const gl::Extents &sourceSize,
                          ID3D11Resource *dest, unsigned int destSubresource, const gl::Box &destArea, const gl::Extents &destSize,
                          const gl::Rectangle *scissor);

    gl::Error copyDepth(ID3D11ShaderResourceView *source, const gl::Box &sourceArea, const gl::Extents &sourceSize,
                        ID3D11DepthStencilView *dest, const gl::Box &destArea, const gl::Extents &destSize,
                        const gl::Rectangle *scissor);

    gl::Error copyDepthStencil(ID3D11Resource *source, unsigned int sourceSubresource, const gl::Box &sourceArea, const gl::Extents &sourceSize,
                               ID3D11Resource *dest, unsigned int destSubresource, const gl::Box &destArea, const gl::Extents &destSize,
                               const gl::Rectangle *scissor);

  private:
    enum BlitShaderType
    {
        BLITSHADER_INVALID,
        BLITSHADER_2D_RGBAF,
        BLITSHADER_2D_BGRAF,
        BLITSHADER_2D_RGBF,
        BLITSHADER_2D_RGF,
        BLITSHADER_2D_RF,
        BLITSHADER_2D_ALPHA,
        BLITSHADER_2D_LUMA,
        BLITSHADER_2D_LUMAALPHA,
        BLITSHADER_2D_RGBAUI,
        BLITSHADER_2D_RGBAI,
        BLITSHADER_2D_RGBUI,
        BLITSHADER_2D_RGBI,
        BLITSHADER_2D_RGUI,
        BLITSHADER_2D_RGI,
        BLITSHADER_2D_RUI,
        BLITSHADER_2D_RI,
        BLITSHADER_3D_RGBAF,
        BLITSHADER_3D_RGBAUI,
        BLITSHADER_3D_RGBAI,
        BLITSHADER_3D_BGRAF,
        BLITSHADER_3D_RGBF,
        BLITSHADER_3D_RGBUI,
        BLITSHADER_3D_RGBI,
        BLITSHADER_3D_RGF,
        BLITSHADER_3D_RGUI,
        BLITSHADER_3D_RGI,
        BLITSHADER_3D_RF,
        BLITSHADER_3D_RUI,
        BLITSHADER_3D_RI,
        BLITSHADER_3D_ALPHA,
        BLITSHADER_3D_LUMA,
        BLITSHADER_3D_LUMAALPHA,
    };

    static BlitShaderType GetBlitShaderType(GLenum destinationFormat, bool isSigned, bool is3D);

    enum SwizzleShaderType
    {
        SWIZZLESHADER_INVALID,
        SWIZZLESHADER_2D_FLOAT,
        SWIZZLESHADER_2D_UINT,
        SWIZZLESHADER_2D_INT,
        SWIZZLESHADER_CUBE_FLOAT,
        SWIZZLESHADER_CUBE_UINT,
        SWIZZLESHADER_CUBE_INT,
        SWIZZLESHADER_3D_FLOAT,
        SWIZZLESHADER_3D_UINT,
        SWIZZLESHADER_3D_INT,
        SWIZZLESHADER_ARRAY_FLOAT,
        SWIZZLESHADER_ARRAY_UINT,
        SWIZZLESHADER_ARRAY_INT,
    };

    static SwizzleShaderType GetSwizzleShaderType(GLenum type, D3D11_SRV_DIMENSION dimensionality);

    gl::Error copyDepthStencil(ID3D11Resource *source, unsigned int sourceSubresource, const gl::Box &sourceArea, const gl::Extents &sourceSize,
                               ID3D11Resource *dest, unsigned int destSubresource, const gl::Box &destArea, const gl::Extents &destSize,
                               const gl::Rectangle *scissor, bool stencilOnly);

    typedef void (*WriteVertexFunction)(const gl::Box &sourceArea, const gl::Extents &sourceSize,
                                        const gl::Box &destArea, const gl::Extents &destSize,
                                        void *outVertices, unsigned int *outStride, unsigned int *outVertexCount,
                                        D3D11_PRIMITIVE_TOPOLOGY *outTopology);

    struct Shader
    {
        WriteVertexFunction mVertexWriteFunction;
        ID3D11InputLayout *mInputLayout;
        ID3D11VertexShader *mVertexShader;
        ID3D11GeometryShader *mGeometryShader;
        ID3D11PixelShader *mPixelShader;
    };

    void add2DBlitShaderToMap(BlitShaderType blitShaderType, ID3D11PixelShader *ps);
    void add3DBlitShaderToMap(BlitShaderType blitShaderType, ID3D11PixelShader *ps);

    gl::Error getBlitShader(GLenum destFormat, bool isSigned, bool is3D, const Shader **shaderOut);
    gl::Error getSwizzleShader(GLenum type, D3D11_SRV_DIMENSION viewDimension, const Shader **shaderOut);

    void addSwizzleShaderToMap(SwizzleShaderType swizzleShaderType, bool is2D, ID3D11PixelShader *ps);

    void clearShaderMap();

    Renderer11 *mRenderer;

    std::map<BlitShaderType, Shader> mBlitShaderMap;
    std::map<SwizzleShaderType, Shader> mSwizzleShaderMap;

    ID3D11Buffer *mVertexBuffer;
    ID3D11SamplerState *mPointSampler;
    ID3D11SamplerState *mLinearSampler;
    ID3D11RasterizerState *mScissorEnabledRasterizerState;
    ID3D11RasterizerState *mScissorDisabledRasterizerState;
    ID3D11DepthStencilState *mDepthStencilState;

    ID3D11InputLayout *mQuad2DIL;
    ID3D11VertexShader *mQuad2DVS;
    ID3D11PixelShader *mDepthPS;

    ID3D11InputLayout *mQuad3DIL;
    ID3D11VertexShader *mQuad3DVS;
    ID3D11GeometryShader *mQuad3DGS;

    ID3D11Buffer *mSwizzleCB;
};

}

#endif   // LIBANGLE_RENDERER_D3D_D3D11_BLIT11_H_
