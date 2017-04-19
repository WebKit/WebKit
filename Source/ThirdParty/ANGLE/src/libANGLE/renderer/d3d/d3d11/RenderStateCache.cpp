//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderStateCache.cpp: Defines rx::RenderStateCache, a cache of Direct3D render
// state objects.

#include "libANGLE/renderer/d3d/d3d11/RenderStateCache.h"

#include <float.h>

#include "common/debug.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/renderer/d3d/FramebufferD3D.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "third_party/murmurhash/MurmurHash3.h"

namespace rx
{
using namespace gl_d3d11;

template <typename mapType>
static void ClearStateMap(mapType &map)
{
    for (typename mapType::iterator i = map.begin(); i != map.end(); i++)
    {
        SafeRelease(i->second.first);
    }
    map.clear();
}

// MSDN's documentation of ID3D11Device::CreateBlendState, ID3D11Device::CreateRasterizerState,
// ID3D11Device::CreateDepthStencilState and ID3D11Device::CreateSamplerState claims the maximum
// number of unique states of each type an application can create is 4096
// TODO(ShahmeerEsmail): Revisit the cache sizes to make sure they are appropriate for most
// scenarios.
const unsigned int RenderStateCache::kMaxBlendStates        = 2048;
const unsigned int RenderStateCache::kMaxRasterizerStates   = 2048;
const unsigned int RenderStateCache::kMaxDepthStencilStates = 2048;
const unsigned int RenderStateCache::kMaxSamplerStates      = 2048;

RenderStateCache::RenderStateCache(Renderer11 *renderer)
    : mRenderer(renderer),
      mCounter(0),
      mBlendStateCache(kMaxBlendStates, HashBlendState, CompareBlendStates),
      mRasterizerStateCache(kMaxRasterizerStates, HashRasterizerState, CompareRasterizerStates),
      mDepthStencilStateCache(kMaxDepthStencilStates, HashDepthStencilState, CompareDepthStencilStates),
      mSamplerStateCache(kMaxSamplerStates, HashSamplerState, CompareSamplerStates),
      mDevice(NULL)
{
}

RenderStateCache::~RenderStateCache()
{
    clear();
}

void RenderStateCache::initialize(ID3D11Device *device)
{
    clear();
    mDevice = device;
}

void RenderStateCache::clear()
{
    ClearStateMap(mBlendStateCache);
    ClearStateMap(mRasterizerStateCache);
    ClearStateMap(mDepthStencilStateCache);
    ClearStateMap(mSamplerStateCache);
}

// static
std::size_t RenderStateCache::HashBlendState(const d3d11::BlendStateKey &blendState)
{
    static const unsigned int seed = 0xABCDEF98;

    std::size_t hash = 0;
    MurmurHash3_x86_32(&blendState, sizeof(d3d11::BlendStateKey), seed, &hash);
    return hash;
}

// static
bool RenderStateCache::CompareBlendStates(const d3d11::BlendStateKey &a,
                                          const d3d11::BlendStateKey &b)
{
    return memcmp(&a, &b, sizeof(d3d11::BlendStateKey)) == 0;
}

// static
d3d11::BlendStateKey RenderStateCache::GetBlendStateKey(const gl::Framebuffer *framebuffer,
                                                        const gl::BlendState &blendState)
{
    d3d11::BlendStateKey key;
    const FramebufferD3D *framebufferD3D   = GetImplAs<FramebufferD3D>(framebuffer);
    const gl::AttachmentList &colorbuffers = framebufferD3D->getColorAttachmentsForRender();
    const UINT8 blendStateMask =
        gl_d3d11::ConvertColorMask(blendState.colorMaskRed, blendState.colorMaskGreen,
                                   blendState.colorMaskBlue, blendState.colorMaskAlpha);

    key.blendState = blendState;
    key.mrt        = false;

    for (size_t i = 0; i < colorbuffers.size(); i++)
    {
        const gl::FramebufferAttachment *attachment = colorbuffers[i];

        if (attachment)
        {
            if (i > 0)
            {
                key.mrt = true;
            }

            key.rtvMasks[i] =
                (gl_d3d11::GetColorMask(attachment->getFormat().info)) & blendStateMask;
        }
        else
        {
            key.rtvMasks[i] = 0;
        }
    }

    for (size_t i = colorbuffers.size(); i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
        key.rtvMasks[i] = 0;
    }

    return key;
}

gl::Error RenderStateCache::getBlendState(const d3d11::BlendStateKey &key,
                                          ID3D11BlendState **outBlendState)
{
    if (!mDevice)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Internal error, RenderStateCache is not initialized.");
    }

    BlendStateMap::iterator keyIter = mBlendStateCache.find(key);
    if (keyIter != mBlendStateCache.end())
    {
        BlendStateCounterPair &state = keyIter->second;
        state.second = mCounter++;
        *outBlendState = state.first;
        return gl::NoError();
    }
    else
    {
        if (mBlendStateCache.size() >= kMaxBlendStates)
        {
            WARN() << "Overflowed the limit of " << kMaxBlendStates
                   << " blend states, removing the least recently used to make room.";

            BlendStateMap::iterator leastRecentlyUsed = mBlendStateCache.begin();
            for (BlendStateMap::iterator i = mBlendStateCache.begin(); i != mBlendStateCache.end(); i++)
            {
                if (i->second.second < leastRecentlyUsed->second.second)
                {
                    leastRecentlyUsed = i;
                }
            }
            SafeRelease(leastRecentlyUsed->second.first);
            mBlendStateCache.erase(leastRecentlyUsed);
        }

        // Create a new blend state and insert it into the cache
        D3D11_BLEND_DESC blendDesc;
        D3D11_RENDER_TARGET_BLEND_DESC &rtDesc0 = blendDesc.RenderTarget[0];
        const gl::BlendState &blendState        = key.blendState;

        blendDesc.AlphaToCoverageEnable  = blendState.sampleAlphaToCoverage;
        blendDesc.IndependentBlendEnable = key.mrt ? TRUE : FALSE;

        rtDesc0 = {};

        if (blendState.blend)
        {
            rtDesc0.BlendEnable    = true;
            rtDesc0.SrcBlend       = gl_d3d11::ConvertBlendFunc(blendState.sourceBlendRGB, false);
            rtDesc0.DestBlend      = gl_d3d11::ConvertBlendFunc(blendState.destBlendRGB, false);
            rtDesc0.BlendOp        = gl_d3d11::ConvertBlendOp(blendState.blendEquationRGB);
            rtDesc0.SrcBlendAlpha  = gl_d3d11::ConvertBlendFunc(blendState.sourceBlendAlpha, true);
            rtDesc0.DestBlendAlpha = gl_d3d11::ConvertBlendFunc(blendState.destBlendAlpha, true);
            rtDesc0.BlendOpAlpha   = gl_d3d11::ConvertBlendOp(blendState.blendEquationAlpha);
        }

        rtDesc0.RenderTargetWriteMask = key.rtvMasks[0];

        for (unsigned int i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        {
            blendDesc.RenderTarget[i]                       = rtDesc0;
            blendDesc.RenderTarget[i].RenderTargetWriteMask = key.rtvMasks[i];
        }

        HRESULT result = mDevice->CreateBlendState(&blendDesc, outBlendState);
        if (FAILED(result) || !(*outBlendState))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Unable to create a ID3D11BlendState, HRESULT: 0x%X.", result);
        }

        mBlendStateCache.insert(std::make_pair(key, std::make_pair(*outBlendState, mCounter++)));

        return gl::NoError();
    }
}

// static
std::size_t RenderStateCache::HashRasterizerState(const RasterizerStateKey &rasterState)
{
    static const unsigned int seed = 0xABCDEF98;

    std::size_t hash = 0;
    MurmurHash3_x86_32(&rasterState, sizeof(RasterizerStateKey), seed, &hash);
    return hash;
}

// static
bool RenderStateCache::CompareRasterizerStates(const RasterizerStateKey &a, const RasterizerStateKey &b)
{
    return memcmp(&a, &b, sizeof(RasterizerStateKey)) == 0;
}

gl::Error RenderStateCache::getRasterizerState(const gl::RasterizerState &rasterState, bool scissorEnabled,
                                               ID3D11RasterizerState **outRasterizerState)
{
    if (!mDevice)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Internal error, RenderStateCache is not initialized.");
    }

    RasterizerStateKey key = {};
    key.rasterizerState = rasterState;
    key.scissorEnabled = scissorEnabled;

    RasterizerStateMap::iterator keyIter = mRasterizerStateCache.find(key);
    if (keyIter != mRasterizerStateCache.end())
    {
        RasterizerStateCounterPair &state = keyIter->second;
        state.second = mCounter++;
        *outRasterizerState = state.first;
        return gl::NoError();
    }
    else
    {
        if (mRasterizerStateCache.size() >= kMaxRasterizerStates)
        {
            WARN() << "Overflowed the limit of " << kMaxRasterizerStates
                   << " rasterizer states, removing the least recently used to make room.";

            RasterizerStateMap::iterator leastRecentlyUsed = mRasterizerStateCache.begin();
            for (RasterizerStateMap::iterator i = mRasterizerStateCache.begin(); i != mRasterizerStateCache.end(); i++)
            {
                if (i->second.second < leastRecentlyUsed->second.second)
                {
                    leastRecentlyUsed = i;
                }
            }
            SafeRelease(leastRecentlyUsed->second.first);
            mRasterizerStateCache.erase(leastRecentlyUsed);
        }

        D3D11_CULL_MODE cullMode = gl_d3d11::ConvertCullMode(rasterState.cullFace, rasterState.cullMode);

        // Disable culling if drawing points
        if (rasterState.pointDrawMode)
        {
            cullMode = D3D11_CULL_NONE;
        }

        D3D11_RASTERIZER_DESC rasterDesc;
        rasterDesc.FillMode = D3D11_FILL_SOLID;
        rasterDesc.CullMode = cullMode;
        rasterDesc.FrontCounterClockwise = (rasterState.frontFace == GL_CCW) ? FALSE: TRUE;
        rasterDesc.DepthBiasClamp = 0.0f; // MSDN documentation of DepthBiasClamp implies a value of zero will preform no clamping, must be tested though.
        rasterDesc.DepthClipEnable = TRUE;
        rasterDesc.ScissorEnable = scissorEnabled ? TRUE : FALSE;
        rasterDesc.MultisampleEnable = rasterState.multiSample;
        rasterDesc.AntialiasedLineEnable = FALSE;

        if (rasterState.polygonOffsetFill)
        {
            rasterDesc.SlopeScaledDepthBias = rasterState.polygonOffsetFactor;
            rasterDesc.DepthBias = (INT)rasterState.polygonOffsetUnits;
        }
        else
        {
            rasterDesc.SlopeScaledDepthBias = 0.0f;
            rasterDesc.DepthBias = 0;
        }

        ID3D11RasterizerState *dx11RasterizerState = NULL;
        HRESULT result = mDevice->CreateRasterizerState(&rasterDesc, &dx11RasterizerState);
        if (FAILED(result) || !dx11RasterizerState)
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Unable to create a ID3D11RasterizerState, HRESULT: 0x%X.", result);
        }

        mRasterizerStateCache.insert(std::make_pair(key, std::make_pair(dx11RasterizerState, mCounter++)));

        *outRasterizerState = dx11RasterizerState;
        return gl::NoError();
    }
}

// static
std::size_t RenderStateCache::HashDepthStencilState(const gl::DepthStencilState &dsState)
{
    static const unsigned int seed = 0xABCDEF98;

    std::size_t hash = 0;
    MurmurHash3_x86_32(&dsState, sizeof(gl::DepthStencilState), seed, &hash);
    return hash;
}

// static
bool RenderStateCache::CompareDepthStencilStates(const gl::DepthStencilState &a, const gl::DepthStencilState &b)
{
    return memcmp(&a, &b, sizeof(gl::DepthStencilState)) == 0;
}

gl::Error RenderStateCache::getDepthStencilState(const gl::DepthStencilState &glState,
                                                 ID3D11DepthStencilState **outDSState)
{
    if (!mDevice)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Internal error, RenderStateCache is not initialized.");
    }

    auto keyIter = mDepthStencilStateCache.find(glState);
    if (keyIter != mDepthStencilStateCache.end())
    {
        DepthStencilStateCounterPair &state = keyIter->second;
        state.second = mCounter++;
        *outDSState = state.first;
        return gl::NoError();
    }

    if (mDepthStencilStateCache.size() >= kMaxDepthStencilStates)
    {
        WARN() << "Overflowed the limit of " << kMaxDepthStencilStates
               << " depth stencil states, removing the least recently used to make room.";

        auto leastRecentlyUsed = mDepthStencilStateCache.begin();
        for (auto i = mDepthStencilStateCache.begin(); i != mDepthStencilStateCache.end(); i++)
        {
            if (i->second.second < leastRecentlyUsed->second.second)
            {
                leastRecentlyUsed = i;
            }
        }
        SafeRelease(leastRecentlyUsed->second.first);
        mDepthStencilStateCache.erase(leastRecentlyUsed);
    }

    D3D11_DEPTH_STENCIL_DESC dsDesc     = {0};
    dsDesc.DepthEnable                  = glState.depthTest ? TRUE : FALSE;
    dsDesc.DepthWriteMask               = ConvertDepthMask(glState.depthMask);
    dsDesc.DepthFunc                    = ConvertComparison(glState.depthFunc);
    dsDesc.StencilEnable                = glState.stencilTest ? TRUE : FALSE;
    dsDesc.StencilReadMask              = ConvertStencilMask(glState.stencilMask);
    dsDesc.StencilWriteMask             = ConvertStencilMask(glState.stencilWritemask);
    dsDesc.FrontFace.StencilFailOp      = ConvertStencilOp(glState.stencilFail);
    dsDesc.FrontFace.StencilDepthFailOp = ConvertStencilOp(glState.stencilPassDepthFail);
    dsDesc.FrontFace.StencilPassOp      = ConvertStencilOp(glState.stencilPassDepthPass);
    dsDesc.FrontFace.StencilFunc        = ConvertComparison(glState.stencilFunc);
    dsDesc.BackFace.StencilFailOp       = ConvertStencilOp(glState.stencilBackFail);
    dsDesc.BackFace.StencilDepthFailOp  = ConvertStencilOp(glState.stencilBackPassDepthFail);
    dsDesc.BackFace.StencilPassOp       = ConvertStencilOp(glState.stencilBackPassDepthPass);
    dsDesc.BackFace.StencilFunc         = ConvertComparison(glState.stencilBackFunc);

    ID3D11DepthStencilState *dx11DepthStencilState = NULL;
    HRESULT result = mDevice->CreateDepthStencilState(&dsDesc, &dx11DepthStencilState);
    if (FAILED(result) || !dx11DepthStencilState)
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Unable to create a ID3D11DepthStencilState, HRESULT: 0x%X.", result);
    }

    mDepthStencilStateCache.insert(
        std::make_pair(glState, std::make_pair(dx11DepthStencilState, mCounter++)));

    *outDSState = dx11DepthStencilState;
    return gl::NoError();
}

// static
std::size_t RenderStateCache::HashSamplerState(const gl::SamplerState &samplerState)
{
    static const unsigned int seed = 0xABCDEF98;

    std::size_t hash = 0;
    MurmurHash3_x86_32(&samplerState, sizeof(gl::SamplerState), seed, &hash);
    return hash;
}

// static
bool RenderStateCache::CompareSamplerStates(const gl::SamplerState &a, const gl::SamplerState &b)
{
    return memcmp(&a, &b, sizeof(gl::SamplerState)) == 0;
}

gl::Error RenderStateCache::getSamplerState(const gl::SamplerState &samplerState, ID3D11SamplerState **outSamplerState)
{
    if (!mDevice)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Internal error, RenderStateCache is not initialized.");
    }

    SamplerStateMap::iterator keyIter = mSamplerStateCache.find(samplerState);
    if (keyIter != mSamplerStateCache.end())
    {
        SamplerStateCounterPair &state = keyIter->second;
        state.second = mCounter++;
        *outSamplerState = state.first;
        return gl::NoError();
    }
    else
    {
        if (mSamplerStateCache.size() >= kMaxSamplerStates)
        {
            WARN() << "Overflowed the limit of " << kMaxSamplerStates
                   << " sampler states, removing the least recently used to make room.";

            SamplerStateMap::iterator leastRecentlyUsed = mSamplerStateCache.begin();
            for (SamplerStateMap::iterator i = mSamplerStateCache.begin(); i != mSamplerStateCache.end(); i++)
            {
                if (i->second.second < leastRecentlyUsed->second.second)
                {
                    leastRecentlyUsed = i;
                }
            }
            SafeRelease(leastRecentlyUsed->second.first);
            mSamplerStateCache.erase(leastRecentlyUsed);
        }

        D3D11_SAMPLER_DESC samplerDesc;
        samplerDesc.Filter = gl_d3d11::ConvertFilter(samplerState.minFilter, samplerState.magFilter,
                                                     samplerState.maxAnisotropy, samplerState.compareMode);
        samplerDesc.AddressU = gl_d3d11::ConvertTextureWrap(samplerState.wrapS);
        samplerDesc.AddressV = gl_d3d11::ConvertTextureWrap(samplerState.wrapT);
        samplerDesc.AddressW = gl_d3d11::ConvertTextureWrap(samplerState.wrapR);
        samplerDesc.MipLODBias = 0;
        samplerDesc.MaxAnisotropy =
            gl_d3d11::ConvertMaxAnisotropy(samplerState.maxAnisotropy, mDevice->GetFeatureLevel());
        samplerDesc.ComparisonFunc = gl_d3d11::ConvertComparison(samplerState.compareFunc);
        samplerDesc.BorderColor[0] = 0.0f;
        samplerDesc.BorderColor[1] = 0.0f;
        samplerDesc.BorderColor[2] = 0.0f;
        samplerDesc.BorderColor[3] = 0.0f;
        samplerDesc.MinLOD = samplerState.minLod;
        samplerDesc.MaxLOD = samplerState.maxLod;

        if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
        {
            // Check that maxLOD is nearly FLT_MAX (1000.0f is the default), since 9_3 doesn't support anything other than FLT_MAX.
            // Note that Feature Level 9_* only supports GL ES 2.0, so the consumer of ANGLE can't modify the Max LOD themselves.
            ASSERT(samplerState.maxLod >= 999.9f);

            // Now just set MaxLOD to FLT_MAX. Other parts of the renderer (e.g. the non-zero max LOD workaround) should take account of this.
            samplerDesc.MaxLOD = FLT_MAX;
        }

        ID3D11SamplerState *dx11SamplerState = NULL;
        HRESULT result = mDevice->CreateSamplerState(&samplerDesc, &dx11SamplerState);
        if (FAILED(result) || !dx11SamplerState)
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Unable to create a ID3D11SamplerState, HRESULT: 0x%X.", result);
        }

        mSamplerStateCache.insert(std::make_pair(samplerState, std::make_pair(dx11SamplerState, mCounter++)));

        *outSamplerState = dx11SamplerState;
        return gl::NoError();
    }
}

}
