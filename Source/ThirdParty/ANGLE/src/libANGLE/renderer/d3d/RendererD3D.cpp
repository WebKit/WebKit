//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererD3D.cpp: Implementation of the base D3D Renderer.

#include "libANGLE/renderer/d3d/RendererD3D.h"

#include "common/debug.h"
#include "common/MemoryBuffer.h"
#include "common/utilities.h"
#include "libANGLE/Display.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/renderer/TextureImpl.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/DeviceD3D.h"
#include "libANGLE/renderer/d3d/DisplayD3D.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/SamplerD3D.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/State.h"
#include "libANGLE/VertexArray.h"

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
    for (auto &incompleteTexture : mIncompleteTextures)
    {
        incompleteTexture.second.set(nullptr);
    }
    mIncompleteTextures.clear();
}

unsigned int RendererD3D::GetBlendSampleMask(const gl::ContextState &data, int samples)
{
    const auto &glState = data.getState();
    unsigned int mask = 0;
    if (glState.isSampleCoverageEnabled())
    {
        GLclampf coverageValue = glState.getSampleCoverageValue();
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

    return mask;
}

// For each Direct3D sampler of either the pixel or vertex stage,
// looks up the corresponding OpenGL texture image unit and texture type,
// and sets the texture and its addressing/filtering state (or NULL when inactive).
// Sampler mapping needs to be up-to-date on the program object before this is called.
gl::Error RendererD3D::applyTextures(GLImplFactory *implFactory,
                                     const gl::ContextState &data,
                                     gl::SamplerType shaderType,
                                     const FramebufferTextureArray &framebufferTextures,
                                     size_t framebufferTextureCount)
{
    const auto &glState    = data.getState();
    const auto &caps       = data.getCaps();
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(glState.getProgram());

    ASSERT(!programD3D->isSamplerMappingDirty());

    // TODO(jmadill): Use the Program's sampler bindings.

    unsigned int samplerRange = programD3D->getUsedSamplerRange(shaderType);
    for (unsigned int samplerIndex = 0; samplerIndex < samplerRange; samplerIndex++)
    {
        GLenum textureType = programD3D->getSamplerTextureType(shaderType, samplerIndex);
        GLint textureUnit  = programD3D->getSamplerMapping(shaderType, samplerIndex, caps);
        if (textureUnit != -1)
        {
            gl::Texture *texture = glState.getSamplerTexture(textureUnit, textureType);
            ASSERT(texture);

            gl::Sampler *samplerObject = glState.getSampler(textureUnit);

            const gl::SamplerState &samplerState =
                samplerObject ? samplerObject->getSamplerState() : texture->getSamplerState();

            // TODO: std::binary_search may become unavailable using older versions of GCC
            if (texture->getTextureState().isSamplerComplete(samplerState, data) &&
                !std::binary_search(framebufferTextures.begin(),
                                    framebufferTextures.begin() + framebufferTextureCount, texture))
            {
                ANGLE_TRY(setSamplerState(shaderType, samplerIndex, texture, samplerState));
                ANGLE_TRY(setTexture(shaderType, samplerIndex, texture));
            }
            else
            {
                // Texture is not sampler complete or it is in use by the framebuffer.  Bind the incomplete texture.
                gl::Texture *incompleteTexture = getIncompleteTexture(implFactory, textureType);

                ANGLE_TRY(setSamplerState(shaderType, samplerIndex, incompleteTexture,
                                          incompleteTexture->getSamplerState()));
                ANGLE_TRY(setTexture(shaderType, samplerIndex, incompleteTexture));
            }
        }
        else
        {
            // No texture bound to this slot even though it is used by the shader, bind a NULL texture
            ANGLE_TRY(setTexture(shaderType, samplerIndex, nullptr));
        }
    }

    // Set all the remaining textures to NULL
    size_t samplerCount = (shaderType == gl::SAMPLER_PIXEL) ? caps.maxTextureImageUnits
                                                            : caps.maxVertexTextureImageUnits;
    clearTextures(shaderType, samplerRange, samplerCount);

    return gl::NoError();
}

gl::Error RendererD3D::applyTextures(GLImplFactory *implFactory, const gl::ContextState &data)
{
    FramebufferTextureArray framebufferTextures;
    size_t framebufferSerialCount = getBoundFramebufferTextures(data, &framebufferTextures);

    ANGLE_TRY(applyTextures(implFactory, data, gl::SAMPLER_VERTEX, framebufferTextures,
                            framebufferSerialCount));
    ANGLE_TRY(applyTextures(implFactory, data, gl::SAMPLER_PIXEL, framebufferTextures,
                            framebufferSerialCount));
    return gl::NoError();
}

bool RendererD3D::skipDraw(const gl::ContextState &data, GLenum drawMode)
{
    const gl::State &state = data.getState();

    if (drawMode == GL_POINTS)
    {
        bool usesPointSize = GetImplAs<ProgramD3D>(state.getProgram())->usesPointSize();

        // ProgramBinary assumes non-point rendering if gl_PointSize isn't written,
        // which affects varying interpolation. Since the value of gl_PointSize is
        // undefined when not written, just skip drawing to avoid unexpected results.
        if (!usesPointSize && !state.isTransformFeedbackActiveUnpaused())
        {
            // Notify developers of risking undefined behavior.
            WARN() << "Point rendering without writing to gl_PointSize.";
            return true;
        }
    }
    else if (gl::IsTriangleMode(drawMode))
    {
        if (state.getRasterizerState().cullFace &&
            state.getRasterizerState().cullMode == GL_FRONT_AND_BACK)
        {
            return true;
        }
    }

    return false;
}

gl::Error RendererD3D::markTransformFeedbackUsage(const gl::ContextState &data)
{
    const gl::TransformFeedback *transformFeedback = data.getState().getCurrentTransformFeedback();
    for (size_t i = 0; i < transformFeedback->getIndexedBufferCount(); i++)
    {
        const OffsetBindingPointer<gl::Buffer> &binding = transformFeedback->getIndexedBuffer(i);
        if (binding.get() != nullptr)
        {
            BufferD3D *bufferD3D = GetImplAs<BufferD3D>(binding.get());
            ANGLE_TRY(bufferD3D->markTransformFeedbackUsage());
        }
    }

    return gl::NoError();
}

size_t RendererD3D::getBoundFramebufferTextures(const gl::ContextState &data,
                                                FramebufferTextureArray *outTextureArray)
{
    size_t textureCount = 0;

    const gl::Framebuffer *drawFramebuffer = data.getState().getDrawFramebuffer();
    for (size_t i = 0; i < drawFramebuffer->getNumColorBuffers(); i++)
    {
        const gl::FramebufferAttachment *attachment = drawFramebuffer->getColorbuffer(i);
        if (attachment && attachment->type() == GL_TEXTURE)
        {
            (*outTextureArray)[textureCount++] = attachment->getTexture();
        }
    }

    const gl::FramebufferAttachment *depthStencilAttachment = drawFramebuffer->getDepthOrStencilbuffer();
    if (depthStencilAttachment && depthStencilAttachment->type() == GL_TEXTURE)
    {
        (*outTextureArray)[textureCount++] = depthStencilAttachment->getTexture();
    }

    std::sort(outTextureArray->begin(), outTextureArray->begin() + textureCount);

    return textureCount;
}

gl::Texture *RendererD3D::getIncompleteTexture(GLImplFactory *implFactory, GLenum type)
{
    if (mIncompleteTextures.find(type) == mIncompleteTextures.end())
    {
        const GLubyte color[] = { 0, 0, 0, 255 };
        const gl::Extents colorSize(1, 1, 1);
        const gl::PixelUnpackState unpack(1, 0);
        const gl::Box area(0, 0, 0, 1, 1, 1);

        // If a texture is external use a 2D texture for the incomplete texture
        GLenum createType = (type == GL_TEXTURE_EXTERNAL_OES) ? GL_TEXTURE_2D : type;

        // Skip the API layer to avoid needing to pass the Context and mess with dirty bits.
        gl::Texture *t =
            new gl::Texture(implFactory, std::numeric_limits<GLuint>::max(), createType);
        t->setStorage(nullptr, createType, 1, GL_RGBA8, colorSize);
        if (type == GL_TEXTURE_CUBE_MAP)
        {
            for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++)
            {
                t->getImplementation()->setSubImage(nullptr, face, 0, area, GL_RGBA8,
                                                    GL_UNSIGNED_BYTE, unpack, color);
            }
        }
        else
        {
            t->getImplementation()->setSubImage(nullptr, createType, 0, area, GL_RGBA8,
                                                GL_UNSIGNED_BYTE, unpack, color);
        }
        mIncompleteTextures[type].set(t);
    }

    return mIncompleteTextures[type].get();
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
    LUID adapterLuid = { 0 };

    if (getLUID(&adapterLuid))
    {
        char adapterLuidString[64];
        sprintf_s(adapterLuidString, sizeof(adapterLuidString), "(adapter LUID: %08x%08x)", adapterLuid.HighPart, adapterLuid.LowPart);
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

}  // namespace rx
