//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManager11.h: Defines a class for caching D3D11 state

#ifndef LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_
#define LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_

#include <array>

#include "libANGLE/angletypes.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/State.h"
#include "libANGLE/renderer/d3d/d3d11/RenderStateCache.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/Query11.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace rx
{

struct RenderTargetDesc;
struct Renderer11DeviceCaps;

struct dx_VertexConstants11
{
    float depthRange[4];
    float viewAdjust[4];
    float viewCoords[4];
    float viewScale[4];
};

struct dx_PixelConstants11
{
    float depthRange[4];
    float viewCoords[4];
    float depthFront[4];
    float viewScale[4];
};

class StateManager11 final : angle::NonCopyable
{
  public:
    StateManager11(Renderer11 *renderer);
    ~StateManager11();

    void initialize(const gl::Caps &caps);
    void deinitialize();
    void syncState(const gl::State &state, const gl::State::DirtyBits &dirtyBits);

    gl::Error setBlendState(const gl::Framebuffer *framebuffer,
                            const gl::BlendState &blendState,
                            const gl::ColorF &blendColor,
                            unsigned int sampleMask);

    gl::Error setDepthStencilState(const gl::State &glState);

    gl::Error setRasterizerState(const gl::RasterizerState &rasterState);

    void setScissorRectangle(const gl::Rectangle &scissor, bool enabled);

    void setViewport(const gl::Caps *caps, const gl::Rectangle &viewport, float zNear, float zFar);

    void updatePresentPath(bool presentPathFastActive,
                           const gl::FramebufferAttachment *framebufferAttachment);

    const dx_VertexConstants11 &getVertexConstants() const { return mVertexConstants; }
    const dx_PixelConstants11 &getPixelConstants() const { return mPixelConstants; }

    void updateStencilSizeIfChanged(bool depthStencilInitialized, unsigned int stencilSize);

    void setShaderResource(gl::SamplerType shaderType,
                           UINT resourceSlot,
                           ID3D11ShaderResourceView *srv);
    gl::Error clearTextures(gl::SamplerType samplerType, size_t rangeStart, size_t rangeEnd);

    gl::Error syncFramebuffer(gl::Framebuffer *framebuffer);

    void invalidateRenderTarget();
    void invalidateBoundViews();
    void invalidateEverything();

    void setOneTimeRenderTarget(ID3D11RenderTargetView *renderTarget,
                                ID3D11DepthStencilView *depthStencil);
    void setOneTimeRenderTargets(const std::vector<ID3D11RenderTargetView *> &renderTargets,
                                 ID3D11DepthStencilView *depthStencil);

    void onBeginQuery(Query11 *query);
    void onDeleteQueryObject(Query11 *query);
    gl::Error onMakeCurrent(const gl::ContextState &data);

    gl::Error updateCurrentValueAttribs(const gl::State &state,
                                        VertexDataManager *vertexDataManager);

    const std::vector<TranslatedAttribute> &getCurrentValueAttribs() const;

  private:
    void setViewportBounds(const int width, const int height);
    void unsetConflictingSRVs(gl::SamplerType shaderType,
                              uintptr_t resource,
                              const gl::ImageIndex &index);
    void unsetConflictingAttachmentResources(const gl::FramebufferAttachment *attachment,
                                             ID3D11Resource *resource);

    Renderer11 *mRenderer;

    // Blend State
    bool mBlendStateIsDirty;
    // TODO(dianx) temporary representation of a dirty bit. once we move enough states in,
    // try experimenting with dirty bit instead of a bool
    gl::BlendState mCurBlendState;
    gl::ColorF mCurBlendColor;
    unsigned int mCurSampleMask;

    // Currently applied depth stencil state
    bool mDepthStencilStateIsDirty;
    gl::DepthStencilState mCurDepthStencilState;
    int mCurStencilRef;
    int mCurStencilBackRef;
    unsigned int mCurStencilSize;
    Optional<bool> mCurDisableDepth;
    Optional<bool> mCurDisableStencil;

    // Currently applied rasterizer state
    bool mRasterizerStateIsDirty;
    gl::RasterizerState mCurRasterState;

    // Currently applied scissor rectangle state
    bool mScissorStateIsDirty;
    bool mCurScissorEnabled;
    gl::Rectangle mCurScissorRect;

    // Currently applied viewport state
    bool mViewportStateIsDirty;
    gl::Rectangle mCurViewport;
    float mCurNear;
    float mCurFar;

    // Things needed in viewport state
    dx_VertexConstants11 mVertexConstants;
    dx_PixelConstants11 mPixelConstants;

    // Render target variables
    gl::Extents mViewportBounds;

    // EGL_ANGLE_experimental_present_path variables
    bool mCurPresentPathFastEnabled;
    int mCurPresentPathFastColorBufferHeight;

    // Current RenderTarget state
    bool mRenderTargetIsDirty;

    // Queries that are currently active in this state
    std::set<Query11 *> mCurrentQueries;

    // Currently applied textures
    struct SRVRecord
    {
        uintptr_t srv;
        uintptr_t resource;
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    };

    // A cache of current SRVs that also tracks the highest 'used' (non-NULL) SRV
    // We might want to investigate a more robust approach that is also fast when there's
    // a large gap between used SRVs (e.g. if SRV 0 and 7 are non-NULL, this approach will
    // waste time on SRVs 1-6.)
    class SRVCache : angle::NonCopyable
    {
      public:
        SRVCache() : mHighestUsedSRV(0) {}

        void initialize(size_t size) { mCurrentSRVs.resize(size); }

        size_t size() const { return mCurrentSRVs.size(); }
        size_t highestUsed() const { return mHighestUsedSRV; }

        const SRVRecord &operator[](size_t index) const { return mCurrentSRVs[index]; }
        void clear();
        void update(size_t resourceIndex, ID3D11ShaderResourceView *srv);

      private:
        std::vector<SRVRecord> mCurrentSRVs;
        size_t mHighestUsedSRV;
    };

    SRVCache mCurVertexSRVs;
    SRVCache mCurPixelSRVs;

    // A block of NULL pointers, cached so we don't re-allocate every draw call
    std::vector<ID3D11ShaderResourceView *> mNullSRVs;

    // Current translations of "Current-Value" data - owned by Context, not VertexArray.
    gl::AttributesMask mDirtyCurrentValueAttribs;
    std::vector<TranslatedAttribute> mCurrentValueAttribs;
};

}  // namespace rx
#endif  // LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_
