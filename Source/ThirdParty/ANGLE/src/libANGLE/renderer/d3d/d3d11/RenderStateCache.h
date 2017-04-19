//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderStateCache.h: Defines rx::RenderStateCache, a cache of Direct3D render
// state objects.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_RENDERSTATECACHE_H_
#define LIBANGLE_RENDERER_D3D_D3D11_RENDERSTATECACHE_H_

#include "libANGLE/angletypes.h"
#include "libANGLE/Error.h"
#include "common/angleutils.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

#include <unordered_map>

namespace gl
{
class Framebuffer;
}

namespace rx
{
class Renderer11;

class RenderStateCache : angle::NonCopyable
{
  public:
    RenderStateCache(Renderer11 *renderer);
    virtual ~RenderStateCache();

    void initialize(ID3D11Device *device);
    void clear();

    static d3d11::BlendStateKey GetBlendStateKey(const gl::Framebuffer *framebuffer,
                                                 const gl::BlendState &blendState);
    gl::Error getBlendState(const d3d11::BlendStateKey &key, ID3D11BlendState **outBlendState);
    gl::Error getRasterizerState(const gl::RasterizerState &rasterState, bool scissorEnabled, ID3D11RasterizerState **outRasterizerState);
    gl::Error getDepthStencilState(const gl::DepthStencilState &dsState,
                                   ID3D11DepthStencilState **outDSState);
    gl::Error getSamplerState(const gl::SamplerState &samplerState, ID3D11SamplerState **outSamplerState);

  private:
    Renderer11 *mRenderer;
    unsigned long long mCounter;

    // Blend state cache
    static std::size_t HashBlendState(const d3d11::BlendStateKey &blendState);
    static bool CompareBlendStates(const d3d11::BlendStateKey &a, const d3d11::BlendStateKey &b);
    static const unsigned int kMaxBlendStates;

    typedef std::size_t (*BlendStateHashFunction)(const d3d11::BlendStateKey &);
    typedef bool (*BlendStateEqualityFunction)(const d3d11::BlendStateKey &,
                                               const d3d11::BlendStateKey &);
    typedef std::pair<ID3D11BlendState*, unsigned long long> BlendStateCounterPair;
    typedef std::unordered_map<d3d11::BlendStateKey,
                               BlendStateCounterPair,
                               BlendStateHashFunction,
                               BlendStateEqualityFunction>
        BlendStateMap;
    BlendStateMap mBlendStateCache;

    // Rasterizer state cache
    struct RasterizerStateKey
    {
        gl::RasterizerState rasterizerState;
        bool scissorEnabled;
    };
    static std::size_t HashRasterizerState(const RasterizerStateKey &rasterState);
    static bool CompareRasterizerStates(const RasterizerStateKey &a, const RasterizerStateKey &b);
    static const unsigned int kMaxRasterizerStates;

    typedef std::size_t (*RasterizerStateHashFunction)(const RasterizerStateKey &);
    typedef bool (*RasterizerStateEqualityFunction)(const RasterizerStateKey &, const RasterizerStateKey &);
    typedef std::pair<ID3D11RasterizerState*, unsigned long long> RasterizerStateCounterPair;
    typedef std::unordered_map<RasterizerStateKey, RasterizerStateCounterPair, RasterizerStateHashFunction, RasterizerStateEqualityFunction> RasterizerStateMap;
    RasterizerStateMap mRasterizerStateCache;

    // Depth stencil state cache
    static std::size_t HashDepthStencilState(const gl::DepthStencilState &dsState);
    static bool CompareDepthStencilStates(const gl::DepthStencilState &a, const gl::DepthStencilState &b);
    static const unsigned int kMaxDepthStencilStates;

    typedef std::size_t (*DepthStencilStateHashFunction)(const gl::DepthStencilState &);
    typedef bool (*DepthStencilStateEqualityFunction)(const gl::DepthStencilState &, const gl::DepthStencilState &);
    typedef std::pair<ID3D11DepthStencilState*, unsigned long long> DepthStencilStateCounterPair;
    typedef std::unordered_map<gl::DepthStencilState,
                               DepthStencilStateCounterPair,
                               DepthStencilStateHashFunction,
                               DepthStencilStateEqualityFunction> DepthStencilStateMap;
    DepthStencilStateMap mDepthStencilStateCache;

    // Sample state cache
    static std::size_t HashSamplerState(const gl::SamplerState &samplerState);
    static bool CompareSamplerStates(const gl::SamplerState &a, const gl::SamplerState &b);
    static const unsigned int kMaxSamplerStates;

    typedef std::size_t (*SamplerStateHashFunction)(const gl::SamplerState &);
    typedef bool (*SamplerStateEqualityFunction)(const gl::SamplerState &, const gl::SamplerState &);
    typedef std::pair<ID3D11SamplerState*, unsigned long long> SamplerStateCounterPair;
    typedef std::unordered_map<gl::SamplerState,
                               SamplerStateCounterPair,
                               SamplerStateHashFunction,
                               SamplerStateEqualityFunction> SamplerStateMap;
    SamplerStateMap mSamplerStateCache;

    ID3D11Device *mDevice;
};

}

#endif // LIBANGLE_RENDERER_D3D_D3D11_RENDERSTATECACHE_H_
