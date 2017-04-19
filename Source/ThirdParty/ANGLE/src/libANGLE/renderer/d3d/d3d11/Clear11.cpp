//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Clear11.cpp: Framebuffer clear utility class.

#include "libANGLE/renderer/d3d/d3d11/Clear11.h"

#include <algorithm>

#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/FramebufferD3D.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "third_party/trace_event/trace_event.h"

// Precompiled shaders
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11ps.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/clearfloat11_fl9ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/clearuint11ps.h"

#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11vs.h"
#include "libANGLE/renderer/d3d/d3d11/shaders/compiled/clearsint11ps.h"

namespace rx
{

static constexpr uint32_t g_VertexSize = sizeof(d3d11::PositionDepthColorVertex<float>);

template <typename T>
static void ApplyVertices(const gl::Color<T> &color, const float depth, void *buffer)
{
    d3d11::PositionDepthColorVertex<T> *vertices =
        reinterpret_cast<d3d11::PositionDepthColorVertex<T> *>(buffer);

    const float z      = gl::clamp01(depth);
    const float left   = -1.0f;
    const float right  = 1.0f;
    const float top    = -1.0f;
    const float bottom = 1.0f;

    d3d11::SetPositionDepthColorVertex<T>(vertices + 0, left, bottom, z, color);
    d3d11::SetPositionDepthColorVertex<T>(vertices + 1, left, top, z, color);
    d3d11::SetPositionDepthColorVertex<T>(vertices + 2, right, bottom, z, color);
    d3d11::SetPositionDepthColorVertex<T>(vertices + 3, right, top, z, color);
}

Clear11::ClearShader::ClearShader(DXGI_FORMAT colorType,
                                  const char *inputLayoutName,
                                  const BYTE *vsByteCode,
                                  size_t vsSize,
                                  const char *vsDebugName,
                                  const BYTE *psByteCode,
                                  size_t psSize,
                                  const char *psDebugName)
    : inputLayout(nullptr),
      vertexShader(vsByteCode, vsSize, vsDebugName),
      pixelShader(psByteCode, psSize, psDebugName)
{
    D3D11_INPUT_ELEMENT_DESC quadLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, colorType, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    inputLayout = new d3d11::LazyInputLayout(quadLayout, 2, vsByteCode, vsSize, inputLayoutName);
}

Clear11::ClearShader::~ClearShader()
{
    SafeDelete(inputLayout);
    vertexShader.release();
    pixelShader.release();
}

Clear11::Clear11(Renderer11 *renderer)
    : mRenderer(renderer),
      mFloatClearShader(nullptr),
      mUintClearShader(nullptr),
      mIntClearShader(nullptr)
{
    TRACE_EVENT0("gpu.angle", "Clear11::Clear11");

    HRESULT result;
    ID3D11Device *device = renderer->getDevice();

    D3D11_BUFFER_DESC vbDesc;
    vbDesc.ByteWidth           = g_VertexSize * 4;
    vbDesc.Usage               = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    vbDesc.MiscFlags           = 0;
    vbDesc.StructureByteStride = 0;

    result = device->CreateBuffer(&vbDesc, nullptr, mVertexBuffer.GetAddressOf());
    ASSERT(SUCCEEDED(result));
    d3d11::SetDebugName(mVertexBuffer, "Clear11 masked clear vertex buffer");

    D3D11_RASTERIZER_DESC rsDesc;
    rsDesc.FillMode              = D3D11_FILL_SOLID;
    rsDesc.CullMode              = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthBias             = 0;
    rsDesc.DepthBiasClamp        = 0.0f;
    rsDesc.SlopeScaledDepthBias  = 0.0f;
    rsDesc.DepthClipEnable       = TRUE;
    rsDesc.ScissorEnable         = FALSE;
    rsDesc.MultisampleEnable     = FALSE;
    rsDesc.AntialiasedLineEnable = FALSE;

    result = device->CreateRasterizerState(&rsDesc, mScissorDisabledRasterizerState.GetAddressOf());
    ASSERT(SUCCEEDED(result));
    d3d11::SetDebugName(mScissorDisabledRasterizerState,
                        "Clear11 Rasterizer State with scissor disabled");

    rsDesc.ScissorEnable = TRUE;
    result = device->CreateRasterizerState(&rsDesc, mScissorEnabledRasterizerState.GetAddressOf());
    ASSERT(SUCCEEDED(result));
    d3d11::SetDebugName(mScissorEnabledRasterizerState,
                        "Clear11 Rasterizer State with scissor enabled");

    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        mFloatClearShader =
            new ClearShader(DXGI_FORMAT_R32G32B32A32_FLOAT, "Clear11 Float IL", g_VS_ClearFloat,
                            ArraySize(g_VS_ClearFloat), "Clear11 Float VS", g_PS_ClearFloat_FL9,
                            ArraySize(g_PS_ClearFloat_FL9), "Clear11 Float PS");
    }
    else
    {
        mFloatClearShader =
            new ClearShader(DXGI_FORMAT_R32G32B32A32_FLOAT, "Clear11 Float IL", g_VS_ClearFloat,
                            ArraySize(g_VS_ClearFloat), "Clear11 Float VS", g_PS_ClearFloat,
                            ArraySize(g_PS_ClearFloat), "Clear11 Float PS");
    }

    if (renderer->isES3Capable())
    {
        mUintClearShader =
            new ClearShader(DXGI_FORMAT_R32G32B32A32_UINT, "Clear11 UINT IL", g_VS_ClearUint,
                            ArraySize(g_VS_ClearUint), "Clear11 UINT VS", g_PS_ClearUint,
                            ArraySize(g_PS_ClearUint), "Clear11 UINT PS");
        mIntClearShader =
            new ClearShader(DXGI_FORMAT_R32G32B32A32_UINT, "Clear11 SINT IL", g_VS_ClearSint,
                            ArraySize(g_VS_ClearSint), "Clear11 SINT VS", g_PS_ClearSint,
                            ArraySize(g_PS_ClearSint), "Clear11 SINT PS");
    }

    // Initialize DepthstencilStateKey with defaults
    mDepthStencilStateKey.depthTest                = false;
    mDepthStencilStateKey.depthMask                = false;
    mDepthStencilStateKey.depthFunc                = GL_ALWAYS;
    mDepthStencilStateKey.stencilWritemask         = static_cast<GLuint>(-1);
    mDepthStencilStateKey.stencilBackWritemask     = static_cast<GLuint>(-1);
    mDepthStencilStateKey.stencilBackMask          = static_cast<GLuint>(-1);
    mDepthStencilStateKey.stencilTest              = false;
    mDepthStencilStateKey.stencilMask              = static_cast<GLuint>(-1);
    mDepthStencilStateKey.stencilFail              = GL_REPLACE;
    mDepthStencilStateKey.stencilPassDepthFail     = GL_REPLACE;
    mDepthStencilStateKey.stencilPassDepthPass     = GL_REPLACE;
    mDepthStencilStateKey.stencilFunc              = GL_ALWAYS;
    mDepthStencilStateKey.stencilBackFail          = GL_REPLACE;
    mDepthStencilStateKey.stencilBackPassDepthFail = GL_REPLACE;
    mDepthStencilStateKey.stencilBackPassDepthPass = GL_REPLACE;
    mDepthStencilStateKey.stencilBackFunc          = GL_ALWAYS;

    // Initialize BlendStateKey with defaults
    mBlendStateKey.blendState.blend                 = false;
    mBlendStateKey.blendState.sourceBlendRGB        = GL_ONE;
    mBlendStateKey.blendState.sourceBlendAlpha      = GL_ONE;
    mBlendStateKey.blendState.destBlendRGB          = GL_ZERO;
    mBlendStateKey.blendState.destBlendAlpha        = GL_ZERO;
    mBlendStateKey.blendState.blendEquationRGB      = GL_FUNC_ADD;
    mBlendStateKey.blendState.blendEquationAlpha    = GL_FUNC_ADD;
    mBlendStateKey.blendState.sampleAlphaToCoverage = false;
    mBlendStateKey.blendState.dither                = true;
    mBlendStateKey.mrt                              = false;
    memset(mBlendStateKey.rtvMasks, 0, sizeof(mBlendStateKey.rtvMasks));
}

Clear11::~Clear11()
{
    SafeDelete(mFloatClearShader);
    SafeDelete(mUintClearShader);
    SafeDelete(mIntClearShader);
}

gl::Error Clear11::clearFramebuffer(const ClearParameters &clearParams,
                                    const gl::FramebufferState &fboData)
{
    const auto &colorAttachments  = fboData.getColorAttachments();
    const auto &drawBufferStates  = fboData.getDrawBufferStates();
    const gl::FramebufferAttachment *depthStencilAttachment = fboData.getDepthOrStencilAttachment();
    RenderTarget11 *depthStencilRenderTarget                = nullptr;

    ASSERT(colorAttachments.size() <= drawBufferStates.size());

    if (clearParams.clearDepth || clearParams.clearStencil)
    {
        ASSERT(depthStencilAttachment != nullptr);
        ANGLE_TRY(depthStencilAttachment->getRenderTarget(&depthStencilRenderTarget));
    }

    // Iterate over the color buffers which require clearing and determine if they can be
    // cleared with ID3D11DeviceContext::ClearRenderTargetView or ID3D11DeviceContext1::ClearView.
    // This requires:
    // 1) The render target is being cleared to a float value (will be cast to integer when clearing
    // integer render targets as expected but does not work the other way around)
    // 2) The format of the render target has no color channels that are currently masked out.
    // Clear the easy-to-clear buffers on the spot and accumulate the ones that require special
    // work.
    //
    // If these conditions are met, and:
    // - No scissored clear is needed, then clear using ID3D11DeviceContext::ClearRenderTargetView.
    // - A scissored clear is needed then clear using ID3D11DeviceContext1::ClearView if available.
    //   Otherwise draw a quad.
    //
    // Also determine if the depth stencil can be cleared with
    // ID3D11DeviceContext::ClearDepthStencilView
    // by checking if the stencil write mask covers the entire stencil.
    //
    // To clear the remaining buffers, quads must be drawn containing an int, uint or float vertex
    // color
    // attribute.

    gl::Extents framebufferSize;

    if (depthStencilRenderTarget != nullptr)
    {
        framebufferSize = depthStencilRenderTarget->getExtents();
    }
    else
    {
        const auto colorAttachment = fboData.getFirstColorAttachment();

        if (!colorAttachment)
        {
            UNREACHABLE();
            return gl::InternalError();
        }

        framebufferSize = colorAttachment->getSize();
    }

    bool needScissoredClear = false;

    if (clearParams.scissorEnabled)
    {
        if (clearParams.scissor.x >= framebufferSize.width ||
            clearParams.scissor.y >= framebufferSize.height ||
            clearParams.scissor.x + clearParams.scissor.width <= 0 ||
            clearParams.scissor.y + clearParams.scissor.height <= 0 ||
            clearParams.scissor.width == 0 || clearParams.scissor.height == 0)
        {
            // Scissor rect is outside the renderbuffer or is an empty rect
            return gl::NoError();
        }

        needScissoredClear =
            clearParams.scissor.x > 0 || clearParams.scissor.y > 0 ||
            clearParams.scissor.x + clearParams.scissor.width < framebufferSize.width ||
            clearParams.scissor.y + clearParams.scissor.height < framebufferSize.height;
    }

    ID3D11DeviceContext *deviceContext   = mRenderer->getDeviceContext();
    ID3D11DeviceContext1 *deviceContext1 = mRenderer->getDeviceContext1IfSupported();

    std::array<ID3D11RenderTargetView *, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs;
    std::array<uint8_t, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvMasks;
    ID3D11DepthStencilView *dsv = nullptr;
    uint32_t numRtvs            = 0;
    const uint8_t colorMask =
        gl_d3d11::ConvertColorMask(clearParams.colorMaskRed, clearParams.colorMaskGreen,
                                   clearParams.colorMaskBlue, clearParams.colorMaskAlpha);

    for (size_t colorAttachmentIndex = 0; colorAttachmentIndex < colorAttachments.size();
         colorAttachmentIndex++)
    {
        const gl::FramebufferAttachment &attachment = colorAttachments[colorAttachmentIndex];

        if (clearParams.clearColor[colorAttachmentIndex] && attachment.isAttached() &&
            drawBufferStates[colorAttachmentIndex] != GL_NONE)
        {
            RenderTarget11 *renderTarget = nullptr;
            ANGLE_TRY(attachment.getRenderTarget(&renderTarget));

            const gl::InternalFormat &formatInfo = *attachment.getFormat().info;

            if (clearParams.colorClearType == GL_FLOAT &&
                !(formatInfo.componentType == GL_FLOAT ||
                  formatInfo.componentType == GL_UNSIGNED_NORMALIZED ||
                  formatInfo.componentType == GL_SIGNED_NORMALIZED))
            {
                ERR() << "It is undefined behaviour to clear a render buffer which is not "
                         "normalized fixed point or floating-point to floating point values (color "
                         "attachment "
                      << colorAttachmentIndex << " has internal format " << attachment.getFormat()
                      << ").";
            }

            if ((formatInfo.redBits == 0 || !clearParams.colorMaskRed) &&
                (formatInfo.greenBits == 0 || !clearParams.colorMaskGreen) &&
                (formatInfo.blueBits == 0 || !clearParams.colorMaskBlue) &&
                (formatInfo.alphaBits == 0 || !clearParams.colorMaskAlpha))
            {
                // Every channel either does not exist in the render target or is masked out
                continue;
            }

            ID3D11RenderTargetView *framebufferRTV = renderTarget->getRenderTargetView();
            if (!framebufferRTV)
            {
                return gl::OutOfMemory()
                       << "Clear11: Render target view pointer unexpectedly null.";
            }

            if ((!(mRenderer->getRenderer11DeviceCaps().supportsClearView) && needScissoredClear) ||
                clearParams.colorClearType != GL_FLOAT ||
                (formatInfo.redBits > 0 && !clearParams.colorMaskRed) ||
                (formatInfo.greenBits > 0 && !clearParams.colorMaskGreen) ||
                (formatInfo.blueBits > 0 && !clearParams.colorMaskBlue) ||
                (formatInfo.alphaBits > 0 && !clearParams.colorMaskAlpha))
            {
                rtvs[numRtvs]     = framebufferRTV;
                rtvMasks[numRtvs] = gl_d3d11::GetColorMask(&formatInfo) & colorMask;
                numRtvs++;
            }
            else
            {
                // ID3D11DeviceContext::ClearRenderTargetView or ID3D11DeviceContext1::ClearView is
                // possible

                const auto &nativeFormat = renderTarget->getFormatSet().format();

                // Check if the actual format has a channel that the internal format does not and
                // set them to the default values
                float clearValues[4] = {
                    ((formatInfo.redBits == 0 && nativeFormat.redBits > 0)
                         ? 0.0f
                         : clearParams.colorFClearValue.red),
                    ((formatInfo.greenBits == 0 && nativeFormat.greenBits > 0)
                         ? 0.0f
                         : clearParams.colorFClearValue.green),
                    ((formatInfo.blueBits == 0 && nativeFormat.blueBits > 0)
                         ? 0.0f
                         : clearParams.colorFClearValue.blue),
                    ((formatInfo.alphaBits == 0 && nativeFormat.alphaBits > 0)
                         ? 1.0f
                         : clearParams.colorFClearValue.alpha),
                };

                if (formatInfo.alphaBits == 1)
                {
                    // Some drivers do not correctly handle calling Clear() on a format with 1-bit
                    // alpha. They can incorrectly round all non-zero values up to 1.0f. Note that
                    // WARP does not do this. We should handle the rounding for them instead.
                    clearValues[3] = (clearParams.colorFClearValue.alpha >= 0.5f) ? 1.0f : 0.0f;
                }

                if (needScissoredClear)
                {
                    // We shouldn't reach here if deviceContext1 is unavailable.
                    ASSERT(deviceContext1);

                    D3D11_RECT rect;
                    rect.left   = clearParams.scissor.x;
                    rect.right  = clearParams.scissor.x + clearParams.scissor.width;
                    rect.top    = clearParams.scissor.y;
                    rect.bottom = clearParams.scissor.y + clearParams.scissor.height;

                    deviceContext1->ClearView(framebufferRTV, clearValues, &rect, 1);
                    if (mRenderer->getWorkarounds().callClearTwiceOnSmallTarget)
                    {
                        if (clearParams.scissor.width <= 16 || clearParams.scissor.height <= 16)
                        {
                            deviceContext1->ClearView(framebufferRTV, clearValues, &rect, 1);
                        }
                    }
                }
                else
                {
                    deviceContext->ClearRenderTargetView(framebufferRTV, clearValues);
                    if (mRenderer->getWorkarounds().callClearTwiceOnSmallTarget)
                    {
                        if (framebufferSize.width <= 16 || framebufferSize.height <= 16)
                        {
                            deviceContext->ClearRenderTargetView(framebufferRTV, clearValues);
                        }
                    }
                }
            }
        }
    }

    if (depthStencilRenderTarget)
    {
        dsv = depthStencilRenderTarget->getDepthStencilView();

        if (!dsv)
        {
            return gl::OutOfMemory() << "Clear11: Depth stencil view pointer unexpectedly null.";
        }

        const auto &nativeFormat = depthStencilRenderTarget->getFormatSet().format();
        const gl::FramebufferAttachment *stencilAttachment = fboData.getStencilAttachment();

        uint32_t stencilUnmasked =
            (stencilAttachment != nullptr) ? (1 << nativeFormat.stencilBits) - 1 : 0;
        bool needMaskedStencilClear =
            clearParams.clearStencil &&
            (clearParams.stencilWriteMask & stencilUnmasked) != stencilUnmasked;

        if (!needScissoredClear && !needMaskedStencilClear)
        {
            const UINT clearFlags = (clearParams.clearDepth ? D3D11_CLEAR_DEPTH : 0) |
                                    (clearParams.clearStencil ? D3D11_CLEAR_STENCIL : 0);
            const FLOAT depthClear   = gl::clamp01(clearParams.depthClearValue);
            const UINT8 stencilClear = clearParams.stencilClearValue & 0xFF;

            deviceContext->ClearDepthStencilView(dsv, clearFlags, depthClear, stencilClear);

            dsv = nullptr;
        }
    }

    if (numRtvs == 0 && !dsv)
    {
        return gl::NoError();
    }

    // To clear the render targets and depth stencil in one pass:
    //
    // Render a quad clipped to the scissor rectangle which draws the clear color and a blend
    // state that will perform the required color masking.
    //
    // The quad's depth is equal to the depth clear value with a depth stencil state that
    // will enable or disable depth test/writes if the depth buffer should be cleared or not.
    //
    // The rasterizer state's stencil is set to always pass or fail based on if the stencil
    // should be cleared or not with a stencil write mask of the stencil clear value.
    //
    // ======================================================================================
    //
    // Luckily, the gl spec (ES 3.0.2 pg 183) states that the results of clearing a render-
    // buffer that is not normalized fixed point or floating point with floating point values
    // are undefined so we can just write floats to them and D3D11 will bit cast them to
    // integers.
    //
    // Also, we don't have to worry about attempting to clear a normalized fixed/floating point
    // buffer with integer values because there is no gl API call which would allow it,
    // glClearBuffer* calls only clear a single renderbuffer at a time which is verified to
    // be a compatible clear type.

    ASSERT(numRtvs <= mRenderer->getNativeCaps().maxDrawBuffers);

    const UINT sampleMask        = 0xFFFFFFFF;
    ID3D11BlendState *blendState = nullptr;

    if (numRtvs > 0)
    {
        // Setup BlendStateKey parameters
        mBlendStateKey.blendState.colorMaskRed   = clearParams.colorMaskRed;
        mBlendStateKey.blendState.colorMaskGreen = clearParams.colorMaskGreen;
        mBlendStateKey.blendState.colorMaskBlue  = clearParams.colorMaskBlue;
        mBlendStateKey.blendState.colorMaskAlpha = clearParams.colorMaskAlpha;
        mBlendStateKey.mrt                       = numRtvs > 1;
        memcpy(mBlendStateKey.rtvMasks, &rtvMasks[0], sizeof(mBlendStateKey.rtvMasks));

        // Get BlendState
        ANGLE_TRY(mRenderer->getStateCache().getBlendState(mBlendStateKey, &blendState));
    }

    const UINT stencilValue          = clearParams.stencilClearValue & 0xFF;
    ID3D11DepthStencilState *dsState = nullptr;
    const float *zValue              = nullptr;

    if (dsv)
    {
        // Setup DepthStencilStateKey
        mDepthStencilStateKey.depthTest        = clearParams.clearDepth;
        mDepthStencilStateKey.depthMask        = clearParams.clearDepth;
        mDepthStencilStateKey.stencilWritemask = clearParams.stencilWriteMask;
        mDepthStencilStateKey.stencilTest      = clearParams.clearStencil;

        // Get DepthStencilState
        ANGLE_TRY(mRenderer->getStateCache().getDepthStencilState(mDepthStencilStateKey, &dsState));
        zValue = clearParams.clearDepth ? &clearParams.depthClearValue : nullptr;
    }

    // Set the vertices
    ClearShader *shader = nullptr;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result =
        deviceContext->Map(mVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Clear11: Failed to map internal VB, " << result;
    }

    switch (clearParams.colorClearType)
    {
        case GL_FLOAT:
            ApplyVertices(clearParams.colorFClearValue, clearParams.depthClearValue,
                          mappedResource.pData);
            shader       = mFloatClearShader;
            break;

        case GL_UNSIGNED_INT:
            ApplyVertices(clearParams.colorUIClearValue, clearParams.depthClearValue,
                          mappedResource.pData);
            shader       = mUintClearShader;
            break;

        case GL_INT:
            ApplyVertices(clearParams.colorIClearValue, clearParams.depthClearValue,
                          mappedResource.pData);
            shader       = mIntClearShader;
            break;

        default:
            UNREACHABLE();
            break;
    }

    deviceContext->Unmap(mVertexBuffer.Get(), 0);

    // Set the viewport to be the same size as the framebuffer
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = static_cast<FLOAT>(framebufferSize.width);
    viewport.Height   = static_cast<FLOAT>(framebufferSize.height);
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    deviceContext->RSSetViewports(1, &viewport);

    // Apply state
    deviceContext->OMSetBlendState(blendState, nullptr, sampleMask);
    deviceContext->OMSetDepthStencilState(dsState, stencilValue);

    if (needScissoredClear)
    {
        const D3D11_RECT scissorRect = {clearParams.scissor.x, clearParams.scissor.y,
                                        clearParams.scissor.x1(), clearParams.scissor.y1()};
        deviceContext->RSSetScissorRects(1, &scissorRect);
        deviceContext->RSSetState(mScissorEnabledRasterizerState.Get());
    }
    else
    {
        deviceContext->RSSetState(mScissorDisabledRasterizerState.Get());
    }

    // Apply shaders
    ID3D11Device *device = mRenderer->getDevice();
    deviceContext->IASetInputLayout(shader->inputLayout->resolve(device));
    deviceContext->VSSetShader(shader->vertexShader.resolve(device), nullptr, 0);
    deviceContext->PSSetShader(shader->pixelShader.resolve(device), nullptr, 0);
    deviceContext->GSSetShader(nullptr, nullptr, 0);

    // Apply vertex buffer
    const UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, mVertexBuffer.GetAddressOf(), &g_VertexSize, &offset);
    deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Apply render targets
    mRenderer->getStateManager()->setOneTimeRenderTargets(&rtvs[0], numRtvs, dsv);

    // Draw the clear quad
    deviceContext->Draw(4, 0);

    // Clean up
    mRenderer->markAllStateDirty();

    return gl::NoError();
}
}
