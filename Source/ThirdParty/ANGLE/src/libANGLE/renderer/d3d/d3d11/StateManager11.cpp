//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManager11.cpp: Defines a class for caching D3D11 state

#include "libANGLE/renderer/d3d/d3d11/StateManager11.h"

#include "common/BitSetIterator.h"
#include "common/utilities.h"
#include "libANGLE/Query.h"
#include "libANGLE/renderer/d3d/d3d11/Framebuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"

namespace rx
{

namespace
{
bool ImageIndexConflictsWithSRV(const gl::ImageIndex &index, D3D11_SHADER_RESOURCE_VIEW_DESC desc)
{
    unsigned mipLevel   = index.mipIndex;
    unsigned layerIndex = index.layerIndex;
    GLenum type         = index.type;

    switch (desc.ViewDimension)
    {
        case D3D11_SRV_DIMENSION_TEXTURE2D:
        {
            unsigned maxSrvMip = desc.Texture2D.MipLevels + desc.Texture2D.MostDetailedMip;
            maxSrvMip          = (desc.Texture2D.MipLevels == -1) ? INT_MAX : maxSrvMip;

            unsigned mipMin = index.mipIndex;
            unsigned mipMax = (layerIndex == -1) ? INT_MAX : layerIndex;

            return type == GL_TEXTURE_2D &&
                   gl::RangeUI(mipMin, mipMax)
                       .intersects(gl::RangeUI(desc.Texture2D.MostDetailedMip, maxSrvMip));
        }

        case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
        {
            unsigned maxSrvMip =
                desc.Texture2DArray.MipLevels + desc.Texture2DArray.MostDetailedMip;
            maxSrvMip = (desc.Texture2DArray.MipLevels == -1) ? INT_MAX : maxSrvMip;

            unsigned maxSlice = desc.Texture2DArray.FirstArraySlice + desc.Texture2DArray.ArraySize;

            // Cube maps can be mapped to Texture2DArray SRVs
            return (type == GL_TEXTURE_2D_ARRAY || gl::IsCubeMapTextureTarget(type)) &&
                   desc.Texture2DArray.MostDetailedMip <= mipLevel && mipLevel < maxSrvMip &&
                   desc.Texture2DArray.FirstArraySlice <= layerIndex && layerIndex < maxSlice;
        }

        case D3D11_SRV_DIMENSION_TEXTURECUBE:
        {
            unsigned maxSrvMip = desc.TextureCube.MipLevels + desc.TextureCube.MostDetailedMip;
            maxSrvMip          = (desc.TextureCube.MipLevels == -1) ? INT_MAX : maxSrvMip;

            return gl::IsCubeMapTextureTarget(type) &&
                   desc.TextureCube.MostDetailedMip <= mipLevel && mipLevel < maxSrvMip;
        }

        case D3D11_SRV_DIMENSION_TEXTURE3D:
        {
            unsigned maxSrvMip = desc.Texture3D.MipLevels + desc.Texture3D.MostDetailedMip;
            maxSrvMip          = (desc.Texture3D.MipLevels == -1) ? INT_MAX : maxSrvMip;

            return type == GL_TEXTURE_3D && desc.Texture3D.MostDetailedMip <= mipLevel &&
                   mipLevel < maxSrvMip;
        }
        default:
            // We only handle the cases corresponding to valid image indexes
            UNIMPLEMENTED();
    }

    return false;
}

// Does *not* increment the resource ref count!!
ID3D11Resource *GetViewResource(ID3D11View *view)
{
    ID3D11Resource *resource = NULL;
    ASSERT(view);
    view->GetResource(&resource);
    resource->Release();
    return resource;
}

}  // anonymous namespace

void StateManager11::SRVCache::update(size_t resourceIndex, ID3D11ShaderResourceView *srv)
{
    ASSERT(resourceIndex < mCurrentSRVs.size());
    SRVRecord *record = &mCurrentSRVs[resourceIndex];

    record->srv = reinterpret_cast<uintptr_t>(srv);
    if (srv)
    {
        record->resource = reinterpret_cast<uintptr_t>(GetViewResource(srv));
        srv->GetDesc(&record->desc);
        mHighestUsedSRV = std::max(resourceIndex + 1, mHighestUsedSRV);
    }
    else
    {
        record->resource = 0;

        if (resourceIndex + 1 == mHighestUsedSRV)
        {
            do
            {
                --mHighestUsedSRV;
            } while (mHighestUsedSRV > 0 && mCurrentSRVs[mHighestUsedSRV].srv == 0);
        }
    }
}

void StateManager11::SRVCache::clear()
{
    if (mCurrentSRVs.empty())
    {
        return;
    }

    memset(&mCurrentSRVs[0], 0, sizeof(SRVRecord) * mCurrentSRVs.size());
    mHighestUsedSRV = 0;
}

static const GLenum QueryTypes[] = {GL_ANY_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                    GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_TIME_ELAPSED_EXT};

StateManager11::StateManager11(Renderer11 *renderer)
    : mRenderer(renderer),
      mBlendStateIsDirty(false),
      mCurBlendColor(0, 0, 0, 0),
      mCurSampleMask(0),
      mDepthStencilStateIsDirty(false),
      mCurStencilRef(0),
      mCurStencilBackRef(0),
      mCurStencilSize(0),
      mRasterizerStateIsDirty(false),
      mScissorStateIsDirty(false),
      mCurScissorEnabled(false),
      mCurScissorRect(),
      mViewportStateIsDirty(false),
      mCurViewport(),
      mCurNear(0.0f),
      mCurFar(0.0f),
      mViewportBounds(),
      mRenderTargetIsDirty(false),
      mDirtyCurrentValueAttribs(),
      mCurrentValueAttribs()
{
    mCurBlendState.blend                 = false;
    mCurBlendState.sourceBlendRGB        = GL_ONE;
    mCurBlendState.destBlendRGB          = GL_ZERO;
    mCurBlendState.sourceBlendAlpha      = GL_ONE;
    mCurBlendState.destBlendAlpha        = GL_ZERO;
    mCurBlendState.blendEquationRGB      = GL_FUNC_ADD;
    mCurBlendState.blendEquationAlpha    = GL_FUNC_ADD;
    mCurBlendState.colorMaskRed          = true;
    mCurBlendState.colorMaskBlue         = true;
    mCurBlendState.colorMaskGreen        = true;
    mCurBlendState.colorMaskAlpha        = true;
    mCurBlendState.sampleAlphaToCoverage = false;
    mCurBlendState.dither                = false;

    mCurDepthStencilState.depthTest                = false;
    mCurDepthStencilState.depthFunc                = GL_LESS;
    mCurDepthStencilState.depthMask                = true;
    mCurDepthStencilState.stencilTest              = false;
    mCurDepthStencilState.stencilMask              = true;
    mCurDepthStencilState.stencilFail              = GL_KEEP;
    mCurDepthStencilState.stencilPassDepthFail     = GL_KEEP;
    mCurDepthStencilState.stencilPassDepthPass     = GL_KEEP;
    mCurDepthStencilState.stencilWritemask         = static_cast<GLuint>(-1);
    mCurDepthStencilState.stencilBackFunc          = GL_ALWAYS;
    mCurDepthStencilState.stencilBackMask          = static_cast<GLuint>(-1);
    mCurDepthStencilState.stencilBackFail          = GL_KEEP;
    mCurDepthStencilState.stencilBackPassDepthFail = GL_KEEP;
    mCurDepthStencilState.stencilBackPassDepthPass = GL_KEEP;
    mCurDepthStencilState.stencilBackWritemask     = static_cast<GLuint>(-1);

    mCurRasterState.rasterizerDiscard   = false;
    mCurRasterState.cullFace            = false;
    mCurRasterState.cullMode            = GL_BACK;
    mCurRasterState.frontFace           = GL_CCW;
    mCurRasterState.polygonOffsetFill   = false;
    mCurRasterState.polygonOffsetFactor = 0.0f;
    mCurRasterState.polygonOffsetUnits  = 0.0f;
    mCurRasterState.pointDrawMode       = false;
    mCurRasterState.multiSample         = false;

    // Initially all current value attributes must be updated on first use.
    mDirtyCurrentValueAttribs.flip();
}

StateManager11::~StateManager11()
{
}

void StateManager11::updateStencilSizeIfChanged(bool depthStencilInitialized,
                                                unsigned int stencilSize)
{
    if (!depthStencilInitialized || stencilSize != mCurStencilSize)
    {
        mCurStencilSize           = stencilSize;
        mDepthStencilStateIsDirty = true;
    }
}

void StateManager11::setViewportBounds(const int width, const int height)
{
    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3 &&
        (mViewportBounds.width != width || mViewportBounds.height != height))
    {
        mViewportBounds       = gl::Extents(width, height, 1);
        mViewportStateIsDirty = true;
    }
}

void StateManager11::updatePresentPath(bool presentPathFastActive,
                                       const gl::FramebufferAttachment *framebufferAttachment)
{
    const int colorBufferHeight =
        framebufferAttachment ? framebufferAttachment->getSize().height : 0;

    if ((mCurPresentPathFastEnabled != presentPathFastActive) ||
        (presentPathFastActive && (colorBufferHeight != mCurPresentPathFastColorBufferHeight)))
    {
        mCurPresentPathFastEnabled           = presentPathFastActive;
        mCurPresentPathFastColorBufferHeight = colorBufferHeight;
        mViewportStateIsDirty                = true;  // Viewport may need to be vertically inverted
        mScissorStateIsDirty                 = true;  // Scissor rect may need to be vertically inverted
        mRasterizerStateIsDirty              = true;  // Cull Mode may need to be inverted
    }
}

void StateManager11::syncState(const gl::State &state, const gl::State::DirtyBits &dirtyBits)
{
    if (!dirtyBits.any())
    {
        return;
    }

    for (auto dirtyBit : angle::IterateBitSet(dirtyBits))
    {
        switch (dirtyBit)
        {
            case gl::State::DIRTY_BIT_BLEND_EQUATIONS:
            {
                const gl::BlendState &blendState = state.getBlendState();
                if (blendState.blendEquationRGB != mCurBlendState.blendEquationRGB ||
                    blendState.blendEquationAlpha != mCurBlendState.blendEquationAlpha)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_BLEND_FUNCS:
            {
                const gl::BlendState &blendState = state.getBlendState();
                if (blendState.sourceBlendRGB != mCurBlendState.sourceBlendRGB ||
                    blendState.destBlendRGB != mCurBlendState.destBlendRGB ||
                    blendState.sourceBlendAlpha != mCurBlendState.sourceBlendAlpha ||
                    blendState.destBlendAlpha != mCurBlendState.destBlendAlpha)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_BLEND_ENABLED:
                if (state.getBlendState().blend != mCurBlendState.blend)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED:
                if (state.getBlendState().sampleAlphaToCoverage !=
                    mCurBlendState.sampleAlphaToCoverage)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DITHER_ENABLED:
                if (state.getBlendState().dither != mCurBlendState.dither)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_COLOR_MASK:
            {
                const gl::BlendState &blendState = state.getBlendState();
                if (blendState.colorMaskRed != mCurBlendState.colorMaskRed ||
                    blendState.colorMaskGreen != mCurBlendState.colorMaskGreen ||
                    blendState.colorMaskBlue != mCurBlendState.colorMaskBlue ||
                    blendState.colorMaskAlpha != mCurBlendState.colorMaskAlpha)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_BLEND_COLOR:
                if (state.getBlendColor() != mCurBlendColor)
                {
                    mBlendStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DEPTH_MASK:
                if (state.getDepthStencilState().depthMask != mCurDepthStencilState.depthMask)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED:
                if (state.getDepthStencilState().depthTest != mCurDepthStencilState.depthTest)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DEPTH_FUNC:
                if (state.getDepthStencilState().depthFunc != mCurDepthStencilState.depthFunc)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED:
                if (state.getDepthStencilState().stencilTest != mCurDepthStencilState.stencilTest)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT:
            {
                const gl::DepthStencilState &depthStencil = state.getDepthStencilState();
                if (depthStencil.stencilFunc != mCurDepthStencilState.stencilFunc ||
                    depthStencil.stencilMask != mCurDepthStencilState.stencilMask ||
                    state.getStencilRef() != mCurStencilRef)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK:
            {
                const gl::DepthStencilState &depthStencil = state.getDepthStencilState();
                if (depthStencil.stencilBackFunc != mCurDepthStencilState.stencilBackFunc ||
                    depthStencil.stencilBackMask != mCurDepthStencilState.stencilBackMask ||
                    state.getStencilBackRef() != mCurStencilBackRef)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT:
                if (state.getDepthStencilState().stencilWritemask !=
                    mCurDepthStencilState.stencilWritemask)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK:
                if (state.getDepthStencilState().stencilBackWritemask !=
                    mCurDepthStencilState.stencilBackWritemask)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_FRONT:
            {
                const gl::DepthStencilState &depthStencil = state.getDepthStencilState();
                if (depthStencil.stencilFail != mCurDepthStencilState.stencilFail ||
                    depthStencil.stencilPassDepthFail !=
                        mCurDepthStencilState.stencilPassDepthFail ||
                    depthStencil.stencilPassDepthPass != mCurDepthStencilState.stencilPassDepthPass)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_STENCIL_OPS_BACK:
            {
                const gl::DepthStencilState &depthStencil = state.getDepthStencilState();
                if (depthStencil.stencilBackFail != mCurDepthStencilState.stencilBackFail ||
                    depthStencil.stencilBackPassDepthFail !=
                        mCurDepthStencilState.stencilBackPassDepthFail ||
                    depthStencil.stencilBackPassDepthPass !=
                        mCurDepthStencilState.stencilBackPassDepthPass)
                {
                    mDepthStencilStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_CULL_FACE_ENABLED:
                if (state.getRasterizerState().cullFace != mCurRasterState.cullFace)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_CULL_FACE:
                if (state.getRasterizerState().cullMode != mCurRasterState.cullMode)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_FRONT_FACE:
                if (state.getRasterizerState().frontFace != mCurRasterState.frontFace)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED:
                if (state.getRasterizerState().polygonOffsetFill !=
                    mCurRasterState.polygonOffsetFill)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET:
            {
                const gl::RasterizerState &rasterState = state.getRasterizerState();
                if (rasterState.polygonOffsetFactor != mCurRasterState.polygonOffsetFactor ||
                    rasterState.polygonOffsetUnits != mCurRasterState.polygonOffsetUnits)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            }
            case gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED:
                if (state.getRasterizerState().rasterizerDiscard !=
                    mCurRasterState.rasterizerDiscard)
                {
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_SCISSOR:
                if (state.getScissor() != mCurScissorRect)
                {
                    mScissorStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_SCISSOR_TEST_ENABLED:
                if (state.isScissorTestEnabled() != mCurScissorEnabled)
                {
                    mScissorStateIsDirty = true;
                    // Rasterizer state update needs mCurScissorsEnabled and updates when it changes
                    mRasterizerStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DEPTH_RANGE:
                if (state.getNearPlane() != mCurNear || state.getFarPlane() != mCurFar)
                {
                    mViewportStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_VIEWPORT:
                if (state.getViewport() != mCurViewport)
                {
                    mViewportStateIsDirty = true;
                }
                break;
            case gl::State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING:
                mRenderTargetIsDirty = true;
                break;
            default:
                if (dirtyBit >= gl::State::DIRTY_BIT_CURRENT_VALUE_0 &&
                    dirtyBit < gl::State::DIRTY_BIT_CURRENT_VALUE_MAX)
                {
                    size_t attribIndex =
                        static_cast<size_t>(dirtyBit - gl::State::DIRTY_BIT_CURRENT_VALUE_0);
                    mDirtyCurrentValueAttribs.set(attribIndex);
                }
                break;
        }
    }
}

gl::Error StateManager11::setBlendState(const gl::Framebuffer *framebuffer,
                                        const gl::BlendState &blendState,
                                        const gl::ColorF &blendColor,
                                        unsigned int sampleMask)
{
    if (!mBlendStateIsDirty && sampleMask == mCurSampleMask)
    {
        return gl::Error(GL_NO_ERROR);
    }

    ID3D11BlendState *dxBlendState = nullptr;
    gl::Error error =
        mRenderer->getStateCache().getBlendState(framebuffer, blendState, &dxBlendState);
    if (error.isError())
    {
        return error;
    }

    ASSERT(dxBlendState != nullptr);

    float blendColors[4] = {0.0f};
    if (blendState.sourceBlendRGB != GL_CONSTANT_ALPHA &&
        blendState.sourceBlendRGB != GL_ONE_MINUS_CONSTANT_ALPHA &&
        blendState.destBlendRGB != GL_CONSTANT_ALPHA &&
        blendState.destBlendRGB != GL_ONE_MINUS_CONSTANT_ALPHA)
    {
        blendColors[0] = blendColor.red;
        blendColors[1] = blendColor.green;
        blendColors[2] = blendColor.blue;
        blendColors[3] = blendColor.alpha;
    }
    else
    {
        blendColors[0] = blendColor.alpha;
        blendColors[1] = blendColor.alpha;
        blendColors[2] = blendColor.alpha;
        blendColors[3] = blendColor.alpha;
    }

    mRenderer->getDeviceContext()->OMSetBlendState(dxBlendState, blendColors, sampleMask);

    mCurBlendState = blendState;
    mCurBlendColor = blendColor;
    mCurSampleMask = sampleMask;

    mBlendStateIsDirty = false;

    return error;
}

gl::Error StateManager11::setDepthStencilState(const gl::State &glState)
{
    const auto &fbo = *glState.getDrawFramebuffer();

    // Disable the depth test/depth write if we are using a stencil-only attachment.
    // This is because ANGLE emulates stencil-only with D24S8 on D3D11 - we should neither read
    // nor write to the unused depth part of this emulated texture.
    bool disableDepth = (!fbo.hasDepth() && fbo.hasStencil());

    // Similarly we disable the stencil portion of the DS attachment if the app only binds depth.
    bool disableStencil = (fbo.hasDepth() && !fbo.hasStencil());

    // CurDisableDepth/Stencil are reset automatically after we call forceSetDepthStencilState.
    if (!mDepthStencilStateIsDirty && mCurDisableDepth.valid() &&
        disableDepth == mCurDisableDepth.value() && mCurDisableStencil.valid() &&
        disableStencil == mCurDisableStencil.value())
    {
        return gl::Error(GL_NO_ERROR);
    }

    const auto &depthStencilState = glState.getDepthStencilState();
    int stencilRef                = glState.getStencilRef();
    int stencilBackRef            = glState.getStencilBackRef();

    // get the maximum size of the stencil ref
    unsigned int maxStencil = 0;
    if (depthStencilState.stencilTest && mCurStencilSize > 0)
    {
        maxStencil = (1 << mCurStencilSize) - 1;
    }
    ASSERT((depthStencilState.stencilWritemask & maxStencil) ==
           (depthStencilState.stencilBackWritemask & maxStencil));
    ASSERT(stencilRef == stencilBackRef);
    ASSERT((depthStencilState.stencilMask & maxStencil) ==
           (depthStencilState.stencilBackMask & maxStencil));

    ID3D11DepthStencilState *dxDepthStencilState = NULL;
    gl::Error error = mRenderer->getStateCache().getDepthStencilState(
        depthStencilState, disableDepth, disableStencil, &dxDepthStencilState);
    if (error.isError())
    {
        return error;
    }

    ASSERT(dxDepthStencilState);

    // Max D3D11 stencil reference value is 0xFF,
    // corresponding to the max 8 bits in a stencil buffer
    // GL specifies we should clamp the ref value to the
    // nearest bit depth when doing stencil ops
    static_assert(D3D11_DEFAULT_STENCIL_READ_MASK == 0xFF,
                  "Unexpected value of D3D11_DEFAULT_STENCIL_READ_MASK");
    static_assert(D3D11_DEFAULT_STENCIL_WRITE_MASK == 0xFF,
                  "Unexpected value of D3D11_DEFAULT_STENCIL_WRITE_MASK");
    UINT dxStencilRef = std::min<UINT>(stencilRef, 0xFFu);

    mRenderer->getDeviceContext()->OMSetDepthStencilState(dxDepthStencilState, dxStencilRef);

    mCurDepthStencilState = depthStencilState;
    mCurStencilRef        = stencilRef;
    mCurStencilBackRef    = stencilBackRef;
    mCurDisableDepth      = disableDepth;
    mCurDisableStencil    = disableStencil;

    mDepthStencilStateIsDirty = false;

    return gl::Error(GL_NO_ERROR);
}

gl::Error StateManager11::setRasterizerState(const gl::RasterizerState &rasterState)
{
    // TODO: Remove pointDrawMode and multiSample from gl::RasterizerState.
    if (!mRasterizerStateIsDirty && rasterState.pointDrawMode == mCurRasterState.pointDrawMode &&
        rasterState.multiSample == mCurRasterState.multiSample)
    {
        return gl::Error(GL_NO_ERROR);
    }

    ID3D11RasterizerState *dxRasterState = nullptr;
    gl::Error error(GL_NO_ERROR);

    if (mCurPresentPathFastEnabled)
    {
        gl::RasterizerState modifiedRasterState = rasterState;

        // If prseent path fast is active then we need invert the front face state.
        // This ensures that both gl_FrontFacing is correct, and front/back culling
        // is performed correctly.
        if (modifiedRasterState.frontFace == GL_CCW)
        {
            modifiedRasterState.frontFace = GL_CW;
        }
        else
        {
            ASSERT(modifiedRasterState.frontFace == GL_CW);
            modifiedRasterState.frontFace = GL_CCW;
        }

        error = mRenderer->getStateCache().getRasterizerState(modifiedRasterState,
                                                              mCurScissorEnabled, &dxRasterState);
    }
    else
    {
        error = mRenderer->getStateCache().getRasterizerState(rasterState, mCurScissorEnabled,
                                                              &dxRasterState);
    }

    if (error.isError())
    {
        return error;
    }

    mRenderer->getDeviceContext()->RSSetState(dxRasterState);

    mCurRasterState         = rasterState;
    mRasterizerStateIsDirty = false;

    return error;
}

void StateManager11::setScissorRectangle(const gl::Rectangle &scissor, bool enabled)
{
    if (!mScissorStateIsDirty)
        return;

    int modifiedScissorY = scissor.y;
    if (mCurPresentPathFastEnabled)
    {
        modifiedScissorY = mCurPresentPathFastColorBufferHeight - scissor.height - scissor.y;
    }

    if (enabled)
    {
        D3D11_RECT rect;
        rect.left   = std::max(0, scissor.x);
        rect.top    = std::max(0, modifiedScissorY);
        rect.right  = scissor.x + std::max(0, scissor.width);
        rect.bottom = modifiedScissorY + std::max(0, scissor.height);

        mRenderer->getDeviceContext()->RSSetScissorRects(1, &rect);
    }

    mCurScissorRect      = scissor;
    mCurScissorEnabled   = enabled;
    mScissorStateIsDirty = false;
}

void StateManager11::setViewport(const gl::Caps *caps,
                                 const gl::Rectangle &viewport,
                                 float zNear,
                                 float zFar)
{
    if (!mViewportStateIsDirty)
        return;

    float actualZNear = gl::clamp01(zNear);
    float actualZFar  = gl::clamp01(zFar);

    int dxMaxViewportBoundsX = static_cast<int>(caps->maxViewportWidth);
    int dxMaxViewportBoundsY = static_cast<int>(caps->maxViewportHeight);
    int dxMinViewportBoundsX = -dxMaxViewportBoundsX;
    int dxMinViewportBoundsY = -dxMaxViewportBoundsY;

    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        // Feature Level 9 viewports shouldn't exceed the dimensions of the rendertarget.
        dxMaxViewportBoundsX = static_cast<int>(mViewportBounds.width);
        dxMaxViewportBoundsY = static_cast<int>(mViewportBounds.height);
        dxMinViewportBoundsX = 0;
        dxMinViewportBoundsY = 0;
    }

    int dxViewportTopLeftX = gl::clamp(viewport.x, dxMinViewportBoundsX, dxMaxViewportBoundsX);
    int dxViewportTopLeftY = gl::clamp(viewport.y, dxMinViewportBoundsY, dxMaxViewportBoundsY);
    int dxViewportWidth    = gl::clamp(viewport.width, 0, dxMaxViewportBoundsX - dxViewportTopLeftX);
    int dxViewportHeight   = gl::clamp(viewport.height, 0, dxMaxViewportBoundsY - dxViewportTopLeftY);

    D3D11_VIEWPORT dxViewport;
    dxViewport.TopLeftX = static_cast<float>(dxViewportTopLeftX);

    if (mCurPresentPathFastEnabled)
    {
        // When present path fast is active and we're rendering to framebuffer 0, we must invert
        // the viewport in Y-axis.
        // NOTE: We delay the inversion until right before the call to RSSetViewports, and leave
        // dxViewportTopLeftY unchanged. This allows us to calculate viewAdjust below using the
        // unaltered dxViewportTopLeftY value.
        dxViewport.TopLeftY = static_cast<float>(mCurPresentPathFastColorBufferHeight -
                                                 dxViewportTopLeftY - dxViewportHeight);
    }
    else
    {
        dxViewport.TopLeftY = static_cast<float>(dxViewportTopLeftY);
    }

    dxViewport.Width    = static_cast<float>(dxViewportWidth);
    dxViewport.Height   = static_cast<float>(dxViewportHeight);
    dxViewport.MinDepth = actualZNear;
    dxViewport.MaxDepth = actualZFar;

    mRenderer->getDeviceContext()->RSSetViewports(1, &dxViewport);

    mCurViewport = viewport;
    mCurNear     = actualZNear;
    mCurFar      = actualZFar;

    // On Feature Level 9_*, we must emulate large and/or negative viewports in the shaders
    // using viewAdjust (like the D3D9 renderer).
    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        mVertexConstants.viewAdjust[0] = static_cast<float>((viewport.width - dxViewportWidth) +
                                                            2 * (viewport.x - dxViewportTopLeftX)) /
                                         dxViewport.Width;
        mVertexConstants.viewAdjust[1] = static_cast<float>((viewport.height - dxViewportHeight) +
                                                            2 * (viewport.y - dxViewportTopLeftY)) /
                                         dxViewport.Height;
        mVertexConstants.viewAdjust[2] = static_cast<float>(viewport.width) / dxViewport.Width;
        mVertexConstants.viewAdjust[3] = static_cast<float>(viewport.height) / dxViewport.Height;
    }

    mPixelConstants.viewCoords[0] = viewport.width * 0.5f;
    mPixelConstants.viewCoords[1] = viewport.height * 0.5f;
    mPixelConstants.viewCoords[2] = viewport.x + (viewport.width * 0.5f);
    mPixelConstants.viewCoords[3] = viewport.y + (viewport.height * 0.5f);

    // Instanced pointsprite emulation requires ViewCoords to be defined in the
    // the vertex shader.
    mVertexConstants.viewCoords[0] = mPixelConstants.viewCoords[0];
    mVertexConstants.viewCoords[1] = mPixelConstants.viewCoords[1];
    mVertexConstants.viewCoords[2] = mPixelConstants.viewCoords[2];
    mVertexConstants.viewCoords[3] = mPixelConstants.viewCoords[3];

    mPixelConstants.depthFront[0] = (actualZFar - actualZNear) * 0.5f;
    mPixelConstants.depthFront[1] = (actualZNear + actualZFar) * 0.5f;

    mVertexConstants.depthRange[0] = actualZNear;
    mVertexConstants.depthRange[1] = actualZFar;
    mVertexConstants.depthRange[2] = actualZFar - actualZNear;

    mPixelConstants.depthRange[0] = actualZNear;
    mPixelConstants.depthRange[1] = actualZFar;
    mPixelConstants.depthRange[2] = actualZFar - actualZNear;

    mPixelConstants.viewScale[0] = 1.0f;
    mPixelConstants.viewScale[1] = mCurPresentPathFastEnabled ? 1.0f : -1.0f;
    mPixelConstants.viewScale[2] = 1.0f;
    mPixelConstants.viewScale[3] = 1.0f;

    mVertexConstants.viewScale[0] = mPixelConstants.viewScale[0];
    mVertexConstants.viewScale[1] = mPixelConstants.viewScale[1];
    mVertexConstants.viewScale[2] = mPixelConstants.viewScale[2];
    mVertexConstants.viewScale[3] = mPixelConstants.viewScale[3];

    mViewportStateIsDirty = false;
}

void StateManager11::invalidateRenderTarget()
{
    mRenderTargetIsDirty = true;
}

void StateManager11::invalidateBoundViews()
{
    mCurVertexSRVs.clear();
    mCurPixelSRVs.clear();

    invalidateRenderTarget();
}

void StateManager11::invalidateEverything()
{
    mBlendStateIsDirty        = true;
    mDepthStencilStateIsDirty = true;
    mRasterizerStateIsDirty   = true;
    mScissorStateIsDirty      = true;
    mViewportStateIsDirty     = true;

    // We reset the current SRV data because it might not be in sync with D3D's state
    // anymore. For example when a currently used SRV is used as an RTV, D3D silently
    // remove it from its state.
    invalidateBoundViews();
}

void StateManager11::setOneTimeRenderTarget(ID3D11RenderTargetView *renderTarget,
                                            ID3D11DepthStencilView *depthStencil)
{
    mRenderer->getDeviceContext()->OMSetRenderTargets(1, &renderTarget, depthStencil);
    mRenderTargetIsDirty = true;
}

void StateManager11::setOneTimeRenderTargets(
    const std::vector<ID3D11RenderTargetView *> &renderTargets,
    ID3D11DepthStencilView *depthStencil)
{
    UINT count               = static_cast<UINT>(renderTargets.size());
    auto renderTargetPointer = (!renderTargets.empty() ? renderTargets.data() : nullptr);

    mRenderer->getDeviceContext()->OMSetRenderTargets(count, renderTargetPointer, depthStencil);
    mRenderTargetIsDirty = true;
}

void StateManager11::onBeginQuery(Query11 *query)
{
    mCurrentQueries.insert(query);
}

void StateManager11::onDeleteQueryObject(Query11 *query)
{
    mCurrentQueries.erase(query);
}

gl::Error StateManager11::onMakeCurrent(const gl::Data &data)
{
    const gl::State &state = *data.state;

    for (Query11 *query : mCurrentQueries)
    {
        query->pause();
    }
    mCurrentQueries.clear();

    for (GLenum queryType : QueryTypes)
    {
        gl::Query *query = state.getActiveQuery(queryType);
        if (query != nullptr)
        {
            Query11 *query11 = GetImplAs<Query11>(query);
            query11->resume();
            mCurrentQueries.insert(query11);
        }
    }

    return gl::Error(GL_NO_ERROR);
}

void StateManager11::setShaderResource(gl::SamplerType shaderType,
                                       UINT resourceSlot,
                                       ID3D11ShaderResourceView *srv)
{
    auto &currentSRVs = (shaderType == gl::SAMPLER_VERTEX ? mCurVertexSRVs : mCurPixelSRVs);

    ASSERT(static_cast<size_t>(resourceSlot) < currentSRVs.size());
    const SRVRecord &record = currentSRVs[resourceSlot];

    if (record.srv != reinterpret_cast<uintptr_t>(srv))
    {
        auto deviceContext = mRenderer->getDeviceContext();
        if (shaderType == gl::SAMPLER_VERTEX)
        {
            deviceContext->VSSetShaderResources(resourceSlot, 1, &srv);
        }
        else
        {
            deviceContext->PSSetShaderResources(resourceSlot, 1, &srv);
        }

        currentSRVs.update(resourceSlot, srv);
    }
}

gl::Error StateManager11::clearTextures(gl::SamplerType samplerType,
                                        size_t rangeStart,
                                        size_t rangeEnd)
{
    if (rangeStart == rangeEnd)
    {
        return gl::Error(GL_NO_ERROR);
    }

    auto &currentSRVs = (samplerType == gl::SAMPLER_VERTEX ? mCurVertexSRVs : mCurPixelSRVs);

    gl::Range<size_t> clearRange(rangeStart, rangeStart);
    clearRange.extend(std::min(rangeEnd, currentSRVs.highestUsed()));

    if (clearRange.empty())
    {
        return gl::Error(GL_NO_ERROR);
    }

    auto deviceContext = mRenderer->getDeviceContext();
    if (samplerType == gl::SAMPLER_VERTEX)
    {
        deviceContext->VSSetShaderResources(static_cast<unsigned int>(rangeStart),
                                            static_cast<unsigned int>(rangeEnd - rangeStart),
                                            &mNullSRVs[0]);
    }
    else
    {
        deviceContext->PSSetShaderResources(static_cast<unsigned int>(rangeStart),
                                            static_cast<unsigned int>(rangeEnd - rangeStart),
                                            &mNullSRVs[0]);
    }

    for (size_t samplerIndex = rangeStart; samplerIndex < rangeEnd; ++samplerIndex)
    {
        currentSRVs.update(samplerIndex, nullptr);
    }

    return gl::Error(GL_NO_ERROR);
}

void StateManager11::unsetConflictingSRVs(gl::SamplerType samplerType,
                                          uintptr_t resource,
                                          const gl::ImageIndex &index)
{
    auto &currentSRVs = (samplerType == gl::SAMPLER_VERTEX ? mCurVertexSRVs : mCurPixelSRVs);

    for (size_t resourceIndex = 0; resourceIndex < currentSRVs.size(); ++resourceIndex)
    {
        auto &record = currentSRVs[resourceIndex];

        if (record.srv && record.resource == resource &&
            ImageIndexConflictsWithSRV(index, record.desc))
        {
            setShaderResource(samplerType, static_cast<UINT>(resourceIndex), NULL);
        }
    }
}

void StateManager11::unsetConflictingAttachmentResources(
    const gl::FramebufferAttachment *attachment,
    ID3D11Resource *resource)
{
    // Unbind render target SRVs from the shader here to prevent D3D11 warnings.
    if (attachment->type() == GL_TEXTURE)
    {
        uintptr_t resourcePtr       = reinterpret_cast<uintptr_t>(resource);
        const gl::ImageIndex &index = attachment->getTextureImageIndex();
        // The index doesn't need to be corrected for the small compressed texture workaround
        // because a rendertarget is never compressed.
        unsetConflictingSRVs(gl::SAMPLER_VERTEX, resourcePtr, index);
        unsetConflictingSRVs(gl::SAMPLER_PIXEL, resourcePtr, index);
    }
}

void StateManager11::initialize(const gl::Caps &caps)
{
    mCurVertexSRVs.initialize(caps.maxVertexTextureImageUnits);
    mCurPixelSRVs.initialize(caps.maxTextureImageUnits);

    // Initialize cached NULL SRV block
    mNullSRVs.resize(caps.maxTextureImageUnits, nullptr);

    mCurrentValueAttribs.resize(caps.maxVertexAttributes);
}

void StateManager11::deinitialize()
{
    mCurrentValueAttribs.clear();
}

gl::Error StateManager11::syncFramebuffer(const gl::Framebuffer *framebuffer)
{
    const Framebuffer11 *framebuffer11 = GetImplAs<Framebuffer11>(framebuffer);
    gl::Error error = framebuffer11->invalidateSwizzles();
    if (error.isError())
    {
        return error;
    }

    if (framebuffer11->hasAnyInternalDirtyBit())
    {
        ASSERT(framebuffer->id() != 0);
        framebuffer11->syncInternalState();
    }

    if (!mRenderTargetIsDirty)
    {
        return gl::Error(GL_NO_ERROR);
    }

    mRenderTargetIsDirty = false;

    // Check for zero-sized default framebuffer, which is a special case.
    // in this case we do not wish to modify any state and just silently return false.
    // this will not report any gl error but will cause the calling method to return.
    if (framebuffer->id() == 0)
    {
        ASSERT(!framebuffer11->hasAnyInternalDirtyBit());
        const gl::Extents &size = framebuffer->getFirstColorbuffer()->getSize();
        if (size.width == 0 || size.height == 0)
        {
            return gl::Error(GL_NO_ERROR);
        }
    }

    // Get the color render buffer and serial
    // Also extract the render target dimensions and view
    unsigned int renderTargetWidth  = 0;
    unsigned int renderTargetHeight = 0;
    RTVArray framebufferRTVs;
    bool missingColorRenderTarget = true;

    framebufferRTVs.fill(nullptr);

    const auto &colorRTs = framebuffer11->getCachedColorRenderTargets();

    size_t appliedRTIndex  = 0;
    bool skipInactiveRTs   = mRenderer->getWorkarounds().mrtPerfWorkaround;
    const auto &drawStates = framebuffer->getDrawBufferStates();

    for (size_t rtIndex = 0; rtIndex < colorRTs.size(); ++rtIndex)
    {
        const RenderTarget11 *renderTarget = colorRTs[rtIndex];

        // Skip inactive rendertargets if the workaround is enabled.
        if (skipInactiveRTs && (!renderTarget || drawStates[rtIndex] == GL_NONE))
        {
            continue;
        }

        if (renderTarget)
        {
            framebufferRTVs[appliedRTIndex] = renderTarget->getRenderTargetView();
            ASSERT(framebufferRTVs[appliedRTIndex]);

            if (missingColorRenderTarget)
            {
                renderTargetWidth        = renderTarget->getWidth();
                renderTargetHeight       = renderTarget->getHeight();
                missingColorRenderTarget = false;
            }
        }

        // Unset conflicting texture SRVs
        const auto *attachment = framebuffer->getColorbuffer(rtIndex);
        ASSERT(attachment);
        unsetConflictingAttachmentResources(attachment, renderTarget->getTexture());

        appliedRTIndex++;
    }

    // Get the depth stencil buffers
    ID3D11DepthStencilView *framebufferDSV = nullptr;
    const auto *depthStencilRenderTarget = framebuffer11->getCachedDepthStencilRenderTarget();
    if (depthStencilRenderTarget)
    {
        framebufferDSV = depthStencilRenderTarget->getDepthStencilView();
        ASSERT(framebufferDSV);

        // If there is no render buffer, the width, height and format values come from
        // the depth stencil
        if (missingColorRenderTarget)
        {
            renderTargetWidth  = depthStencilRenderTarget->getWidth();
            renderTargetHeight = depthStencilRenderTarget->getHeight();
        }

        // Unset conflicting texture SRVs
        const auto *attachment = framebuffer->getDepthOrStencilbuffer();
        ASSERT(attachment);
        unsetConflictingAttachmentResources(attachment, depthStencilRenderTarget->getTexture());
    }

    // TODO(jmadill): Use context caps?
    UINT drawBuffers = mRenderer->getRendererCaps().maxDrawBuffers;

    // Apply the render target and depth stencil
    mRenderer->getDeviceContext()->OMSetRenderTargets(drawBuffers, framebufferRTVs.data(),
                                                      framebufferDSV);

    // The D3D11 blend state is heavily dependent on the current render target.
    mBlendStateIsDirty = true;

    setViewportBounds(renderTargetWidth, renderTargetHeight);

    return gl::Error(GL_NO_ERROR);
}

gl::Error StateManager11::updateCurrentValueAttribs(const gl::State &state,
                                                    VertexDataManager *vertexDataManager)
{
    const auto &activeAttribsMask  = state.getProgram()->getActiveAttribLocationsMask();
    const auto &dirtyActiveAttribs = (activeAttribsMask & mDirtyCurrentValueAttribs);
    const auto &vertexAttributes   = state.getVertexArray()->getVertexAttributes();

    for (auto attribIndex : angle::IterateBitSet(dirtyActiveAttribs))
    {
        if (vertexAttributes[attribIndex].enabled)
            continue;

        mDirtyCurrentValueAttribs.reset(attribIndex);

        const auto &currentValue =
            state.getVertexAttribCurrentValue(static_cast<unsigned int>(attribIndex));
        auto currentValueAttrib              = &mCurrentValueAttribs[attribIndex];
        currentValueAttrib->currentValueType = currentValue.Type;
        currentValueAttrib->attribute        = &vertexAttributes[attribIndex];

        gl::Error error = vertexDataManager->storeCurrentValue(currentValue, currentValueAttrib,
                                                               static_cast<size_t>(attribIndex));
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

const std::vector<TranslatedAttribute> &StateManager11::getCurrentValueAttribs() const
{
    return mCurrentValueAttribs;
}

}  // namespace rx
