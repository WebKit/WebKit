//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
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
      mFeaturesInitialized(false),
      mDisjoint(false),
      mDeviceLost(false)
{}

RendererD3D::~RendererD3D() {}

bool RendererD3D::skipDraw(const gl::State &glState, gl::PrimitiveMode drawMode)
{
    if (drawMode == gl::PrimitiveMode::Points)
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

gl::GraphicsResetStatus RendererD3D::getResetStatus()
{
    if (!mDeviceLost)
    {
        if (testDeviceLost())
        {
            mDeviceLost = true;
            notifyDeviceLost();
            return gl::GraphicsResetStatus::UnknownContextReset;
        }
        return gl::GraphicsResetStatus::NoError;
    }

    if (testDeviceResettable())
    {
        return gl::GraphicsResetStatus::NoError;
    }

    return gl::GraphicsResetStatus::UnknownContextReset;
}

void RendererD3D::notifyDeviceLost()
{
    mDisplay->notifyDeviceLost();
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

Serial RendererD3D::generateSerial()
{
    return mSerialFactory.generate();
}

bool InstancedPointSpritesActive(ProgramD3D *programD3D, gl::PrimitiveMode mode)
{
    return programD3D->usesPointSize() && programD3D->usesInstancedPointSpriteEmulation() &&
           mode == gl::PrimitiveMode::Points;
}

angle::Result RendererD3D::initRenderTarget(const gl::Context *context,
                                            RenderTargetD3D *renderTarget)
{
    return clearRenderTarget(context, renderTarget, gl::ColorF(0, 0, 0, 0), 1, 0);
}

const angle::FeaturesD3D &RendererD3D::getFeatures() const
{
    if (!mFeaturesInitialized)
    {
        initializeFeatures(&mFeatures);
        mFeaturesInitialized = true;
    }

    return mFeatures;
}

unsigned int GetBlendSampleMask(const gl::State &glState, int samples)
{
    unsigned int mask = 0;
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

GLenum DefaultGLErrorCode(HRESULT hr)
{
    switch (hr)
    {
#ifdef ANGLE_ENABLE_D3D9
        case D3DERR_OUTOFVIDEOMEMORY:
#endif
        case E_OUTOFMEMORY:
            return GL_OUT_OF_MEMORY;
        default:
            return GL_INVALID_OPERATION;
    }
}
}  // namespace rx
