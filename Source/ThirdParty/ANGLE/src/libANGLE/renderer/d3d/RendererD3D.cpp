//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererD3D.cpp: Implementation of the base D3D Renderer.

#include "libANGLE/renderer/d3d/RendererD3D.h"

#include "common/MemoryBuffer.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/State.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/TextureImpl.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/DeviceD3D.h"
#include "libANGLE/renderer/d3d/DisplayD3D.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/SamplerD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"

namespace rx
{

RendererD3D::RendererD3D(egl::Display *display)
    : mDisplay(display),
      mPresentPathFastEnabled(false),
      mCapsInitialized(false),
      mWorkaroundsInitialized(false),
      mDisjoint(false),
      mDeviceLost(false),
      mWorkerThreadPool(4)
{
}

RendererD3D::~RendererD3D()
{
    cleanup();
}

void RendererD3D::cleanup()
{
    mIncompleteTextures.onDestroy(mDisplay->getProxyContext());
}

bool RendererD3D::skipDraw(const gl::State &glState, GLenum drawMode)
{
    if (drawMode == GL_POINTS)
    {
        bool usesPointSize = GetImplAs<ProgramD3D>(glState.getProgram())->usesPointSize();

        // ProgramBinary assumes non-point rendering if gl_PointSize isn't written,
        // which affects varying interpolation. Since the value of gl_PointSize is
        // undefined when not written, just skip drawing to avoid unexpected results.
        if (!usesPointSize && !glState.isTransformFeedbackActiveUnpaused())
        {
            // Notify developers of risking undefined behavior.
            WARN() << "Point rendering without writing to gl_PointSize.";
            return true;
        }
    }
    else if (gl::IsTriangleMode(drawMode))
    {
        if (glState.getRasterizerState().cullFace &&
            glState.getRasterizerState().cullMode == gl::CullFaceMode::FrontAndBack)
        {
            return true;
        }
    }

    return false;
}

gl::Error RendererD3D::getIncompleteTexture(const gl::Context *context,
                                            GLenum type,
                                            gl::Texture **textureOut)
{
    return mIncompleteTextures.getIncompleteTexture(context, type, this, textureOut);
}

GLenum RendererD3D::getResetStatus()
{
    if (!mDeviceLost)
    {
        if (testDeviceLost())
        {
            mDeviceLost = true;
            notifyDeviceLost();
            return GL_UNKNOWN_CONTEXT_RESET_EXT;
        }
        return GL_NO_ERROR;
    }

    if (testDeviceResettable())
    {
        return GL_NO_ERROR;
    }

    return GL_UNKNOWN_CONTEXT_RESET_EXT;
}

void RendererD3D::notifyDeviceLost()
{
    mDisplay->notifyDeviceLost();
}

std::string RendererD3D::getVendorString() const
{
    LUID adapterLuid = {0};

    if (getLUID(&adapterLuid))
    {
        char adapterLuidString[64];
        sprintf_s(adapterLuidString, sizeof(adapterLuidString), "(adapter LUID: %08x%08x)",
                  adapterLuid.HighPart, adapterLuid.LowPart);
        return std::string(adapterLuidString);
    }

    return std::string("");
}

void RendererD3D::setGPUDisjoint()
{
    mDisjoint = true;
}

GLint RendererD3D::getGPUDisjoint()
{
    bool disjoint = mDisjoint;

    // Disjoint flag is cleared when read
    mDisjoint = false;

    return disjoint;
}

GLint64 RendererD3D::getTimestamp()
{
    // D3D has no way to get an actual timestamp reliably so 0 is returned
    return 0;
}

void RendererD3D::ensureCapsInitialized() const
{
    if (!mCapsInitialized)
    {
        generateCaps(&mNativeCaps, &mNativeTextureCaps, &mNativeExtensions, &mNativeLimitations);
        mCapsInitialized = true;
    }
}

const gl::Caps &RendererD3D::getNativeCaps() const
{
    ensureCapsInitialized();
    return mNativeCaps;
}

const gl::TextureCapsMap &RendererD3D::getNativeTextureCaps() const
{
    ensureCapsInitialized();
    return mNativeTextureCaps;
}

const gl::Extensions &RendererD3D::getNativeExtensions() const
{
    ensureCapsInitialized();
    return mNativeExtensions;
}

const gl::Limitations &RendererD3D::getNativeLimitations() const
{
    ensureCapsInitialized();
    return mNativeLimitations;
}

angle::WorkerThreadPool *RendererD3D::getWorkerThreadPool()
{
    return &mWorkerThreadPool;
}

Serial RendererD3D::generateSerial()
{
    return mSerialFactory.generate();
}

bool InstancedPointSpritesActive(ProgramD3D *programD3D, GLenum mode)
{
    return programD3D->usesPointSize() && programD3D->usesInstancedPointSpriteEmulation() &&
           mode == GL_POINTS;
}

gl::Error RendererD3D::initRenderTarget(RenderTargetD3D *renderTarget)
{
    return clearRenderTarget(renderTarget, gl::ColorF(0, 0, 0, 0), 1, 0);
}

gl::Error RendererD3D::initializeMultisampleTextureToBlack(const gl::Context *context,
                                                           gl::Texture *glTexture)
{
    ASSERT(glTexture->getTarget() == GL_TEXTURE_2D_MULTISAMPLE);
    TextureD3D *textureD3D        = GetImplAs<TextureD3D>(glTexture);
    gl::ImageIndex index          = gl::ImageIndex::Make2DMultisample();
    RenderTargetD3D *renderTarget = nullptr;
    ANGLE_TRY(textureD3D->getRenderTarget(context, index, &renderTarget));
    return clearRenderTarget(renderTarget, gl::ColorF(0.0f, 0.0f, 0.0f, 1.0f), 1.0f, 0);
}

unsigned int GetBlendSampleMask(const gl::State &glState, int samples)
{
    unsigned int mask   = 0;
    if (glState.isSampleCoverageEnabled())
    {
        GLfloat coverageValue = glState.getSampleCoverageValue();
        if (coverageValue != 0)
        {
            float threshold = 0.5f;

            for (int i = 0; i < samples; ++i)
            {
                mask <<= 1;

                if ((i + 1) * coverageValue >= threshold)
                {
                    threshold += 1.0f;
                    mask |= 1;
                }
            }
        }

        bool coverageInvert = glState.getSampleCoverageInvert();
        if (coverageInvert)
        {
            mask = ~mask;
        }
    }
    else
    {
        mask = 0xFFFFFFFF;
    }

    if (glState.isSampleMaskEnabled())
    {
        mask &= glState.getSampleMaskWord(0);
    }

    return mask;
}

}  // namespace rx
