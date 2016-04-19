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

namespace
{
// If we request a scratch buffer requesting a smaller size this many times,
// release and recreate the scratch buffer. This ensures we don't have a
// degenerate case where we are stuck hogging memory.
const int ScratchMemoryBufferLifetime = 1000;

}  // anonymous namespace

RendererD3D::RendererD3D(egl::Display *display)
    : mDisplay(display),
      mDeviceLost(false),
      mAnnotator(nullptr),
      mPresentPathFastEnabled(false),
      mScratchMemoryBufferResetCounter(0),
      mWorkaroundsInitialized(false),
      mDisjoint(false)
{
}

RendererD3D::~RendererD3D()
{
    cleanup();
}

void RendererD3D::cleanup()
{
    mScratchMemoryBuffer.resize(0);
    for (auto &incompleteTexture : mIncompleteTextures)
    {
        incompleteTexture.second.set(nullptr);
    }
    mIncompleteTextures.clear();

    if (mAnnotator != nullptr)
    {
        gl::UninitializeDebugAnnotations();
        SafeDelete(mAnnotator);
    }
}

SamplerImpl *RendererD3D::createSampler()
{
    return new SamplerD3D();
}

gl::Error RendererD3D::drawArrays(const gl::Data &data, GLenum mode, GLint first, GLsizei count)
{
    return genericDrawArrays(data, mode, first, count, 0);
}

gl::Error RendererD3D::drawArraysInstanced(const gl::Data &data,
                                           GLenum mode,
                                           GLint first,
                                           GLsizei count,
                                           GLsizei instanceCount)
{
    return genericDrawArrays(data, mode, first, count, instanceCount);
}

gl::Error RendererD3D::drawElements(const gl::Data &data,
                                    GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const GLvoid *indices,
                                    const gl::IndexRange &indexRange)
{
    return genericDrawElements(data, mode, count, type, indices, 0, indexRange);
}

gl::Error RendererD3D::drawElementsInstanced(const gl::Data &data,
                                             GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             const GLvoid *indices,
                                             GLsizei instances,
                                             const gl::IndexRange &indexRange)
{
    return genericDrawElements(data, mode, count, type, indices, instances, indexRange);
}

gl::Error RendererD3D::drawRangeElements(const gl::Data &data,
                                         GLenum mode,
                                         GLuint start,
                                         GLuint end,
                                         GLsizei count,
                                         GLenum type,
                                         const GLvoid *indices,
                                         const gl::IndexRange &indexRange)
{
    return genericDrawElements(data, mode, count, type, indices, 0, indexRange);
}

gl::Error RendererD3D::genericDrawElements(const gl::Data &data,
                                           GLenum mode,
                                           GLsizei count,
                                           GLenum type,
                                           const GLvoid *indices,
                                           GLsizei instances,
                                           const gl::IndexRange &indexRange)
{
    gl::Program *program = data.state->getProgram();
    ASSERT(program != nullptr);
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);
    bool usesPointSize     = programD3D->usesPointSize();

    programD3D->updateSamplerMapping();

    ANGLE_TRY(generateSwizzles(data));

    if (!applyPrimitiveType(mode, count, usesPointSize))
    {
        return gl::NoError();
    }

    ANGLE_TRY(updateState(data, mode));

    TranslatedIndexData indexInfo;
    indexInfo.indexRange = indexRange;

    ANGLE_TRY(applyIndexBuffer(data, indices, count, mode, type, &indexInfo));

    applyTransformFeedbackBuffers(*data.state);
    // Transform feedback is not allowed for DrawElements, this error should have been caught at the API validation
    // layer.
    ASSERT(!data.state->isTransformFeedbackActiveUnpaused());

    size_t vertexCount = indexInfo.indexRange.vertexCount();
    ANGLE_TRY(applyVertexBuffer(*data.state, mode, static_cast<GLsizei>(indexInfo.indexRange.start),
                                static_cast<GLsizei>(vertexCount), instances, &indexInfo));
    ANGLE_TRY(applyTextures(data));
    ANGLE_TRY(applyShaders(data, mode));
    ANGLE_TRY(programD3D->applyUniformBuffers(data));

    if (!skipDraw(data, mode))
    {
        ANGLE_TRY(drawElementsImpl(data, indexInfo, mode, count, type, indices, instances));
    }

    return gl::NoError();
}

gl::Error RendererD3D::genericDrawArrays(const gl::Data &data,
                                         GLenum mode,
                                         GLint first,
                                         GLsizei count,
                                         GLsizei instances)
{
    gl::Program *program = data.state->getProgram();
    ASSERT(program != nullptr);
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);
    bool usesPointSize     = programD3D->usesPointSize();

    programD3D->updateSamplerMapping();

    ANGLE_TRY(generateSwizzles(data));
    if (!applyPrimitiveType(mode, count, usesPointSize))
    {
        return gl::NoError();
    }

    ANGLE_TRY(updateState(data, mode));
    ANGLE_TRY(applyTransformFeedbackBuffers(*data.state));
    ANGLE_TRY(applyVertexBuffer(*data.state, mode, first, count, instances, nullptr));
    ANGLE_TRY(applyTextures(data));
    ANGLE_TRY(applyShaders(data, mode));
    ANGLE_TRY(programD3D->applyUniformBuffers(data));

    if (!skipDraw(data, mode))
    {
        ANGLE_TRY(drawArraysImpl(data, mode, first, count, instances));

        if (data.state->isTransformFeedbackActiveUnpaused())
        {
            ANGLE_TRY(markTransformFeedbackUsage(data));
        }
    }

    return gl::NoError();
}

gl::Error RendererD3D::generateSwizzles(const gl::Data &data, gl::SamplerType type)
{
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(data.state->getProgram());

    unsigned int samplerRange = programD3D->getUsedSamplerRange(type);

    for (unsigned int i = 0; i < samplerRange; i++)
    {
        GLenum textureType = programD3D->getSamplerTextureType(type, i);
        GLint textureUnit = programD3D->getSamplerMapping(type, i, *data.caps);
        if (textureUnit != -1)
        {
            gl::Texture *texture = data.state->getSamplerTexture(textureUnit, textureType);
            ASSERT(texture);
            if (texture->getTextureState().swizzleRequired())
            {
                ANGLE_TRY(generateSwizzle(texture));
            }
        }
    }

    return gl::NoError();
}

gl::Error RendererD3D::generateSwizzles(const gl::Data &data)
{
    ANGLE_TRY(generateSwizzles(data, gl::SAMPLER_VERTEX));
    ANGLE_TRY(generateSwizzles(data, gl::SAMPLER_PIXEL));
    return gl::NoError();
}

unsigned int RendererD3D::GetBlendSampleMask(const gl::Data &data, int samples)
{
    unsigned int mask = 0;
    if (data.state->isSampleCoverageEnabled())
    {
        GLclampf coverageValue = data.state->getSampleCoverageValue();
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

        bool coverageInvert = data.state->getSampleCoverageInvert();
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

// Applies the shaders and shader constants to the Direct3D device
gl::Error RendererD3D::applyShaders(const gl::Data &data, GLenum drawMode)
{
    gl::Program *program = data.state->getProgram();
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);
    programD3D->updateCachedInputLayout(*data.state);

    ANGLE_TRY(applyShadersImpl(data, drawMode));

    return programD3D->applyUniforms(drawMode);
}

// For each Direct3D sampler of either the pixel or vertex stage,
// looks up the corresponding OpenGL texture image unit and texture type,
// and sets the texture and its addressing/filtering state (or NULL when inactive).
// Sampler mapping needs to be up-to-date on the program object before this is called.
gl::Error RendererD3D::applyTextures(const gl::Data &data, gl::SamplerType shaderType,
                                     const FramebufferTextureArray &framebufferTextures, size_t framebufferTextureCount)
{
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(data.state->getProgram());

    ASSERT(!programD3D->isSamplerMappingDirty());

    unsigned int samplerRange = programD3D->getUsedSamplerRange(shaderType);
    for (unsigned int samplerIndex = 0; samplerIndex < samplerRange; samplerIndex++)
    {
        GLenum textureType = programD3D->getSamplerTextureType(shaderType, samplerIndex);
        GLint textureUnit = programD3D->getSamplerMapping(shaderType, samplerIndex, *data.caps);
        if (textureUnit != -1)
        {
            gl::Texture *texture = data.state->getSamplerTexture(textureUnit, textureType);
            ASSERT(texture);

            gl::Sampler *samplerObject = data.state->getSampler(textureUnit);

            const gl::SamplerState &samplerState =
                samplerObject ? samplerObject->getSamplerState() : texture->getSamplerState();

            // TODO: std::binary_search may become unavailable using older versions of GCC
            if (texture->isSamplerComplete(samplerState, data) &&
                !std::binary_search(framebufferTextures.begin(),
                                    framebufferTextures.begin() + framebufferTextureCount, texture))
            {
                ANGLE_TRY(setSamplerState(shaderType, samplerIndex, texture, samplerState));
                ANGLE_TRY(setTexture(shaderType, samplerIndex, texture));
            }
            else
            {
                // Texture is not sampler complete or it is in use by the framebuffer.  Bind the incomplete texture.
                gl::Texture *incompleteTexture = getIncompleteTexture(textureType);

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
    size_t samplerCount = (shaderType == gl::SAMPLER_PIXEL) ? data.caps->maxTextureImageUnits
                                                            : data.caps->maxVertexTextureImageUnits;
    clearTextures(shaderType, samplerRange, samplerCount);

    return gl::NoError();
}

gl::Error RendererD3D::applyTextures(const gl::Data &data)
{
    FramebufferTextureArray framebufferTextures;
    size_t framebufferSerialCount = getBoundFramebufferTextures(data, &framebufferTextures);

    ANGLE_TRY(applyTextures(data, gl::SAMPLER_VERTEX, framebufferTextures, framebufferSerialCount));
    ANGLE_TRY(applyTextures(data, gl::SAMPLER_PIXEL, framebufferTextures, framebufferSerialCount));
    return gl::NoError();
}

bool RendererD3D::skipDraw(const gl::Data &data, GLenum drawMode)
{
    const gl::State &state = *data.state;

    if (drawMode == GL_POINTS)
    {
        bool usesPointSize = GetImplAs<ProgramD3D>(state.getProgram())->usesPointSize();

        // ProgramBinary assumes non-point rendering if gl_PointSize isn't written,
        // which affects varying interpolation. Since the value of gl_PointSize is
        // undefined when not written, just skip drawing to avoid unexpected results.
        if (!usesPointSize && !state.isTransformFeedbackActiveUnpaused())
        {
            // This is stictly speaking not an error, but developers should be
            // notified of risking undefined behavior.
            ERR("Point rendering without writing to gl_PointSize.");

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

gl::Error RendererD3D::markTransformFeedbackUsage(const gl::Data &data)
{
    const gl::TransformFeedback *transformFeedback = data.state->getCurrentTransformFeedback();
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

size_t RendererD3D::getBoundFramebufferTextures(const gl::Data &data, FramebufferTextureArray *outTextureArray)
{
    size_t textureCount = 0;

    const gl::Framebuffer *drawFramebuffer = data.state->getDrawFramebuffer();
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

gl::Texture *RendererD3D::getIncompleteTexture(GLenum type)
{
    if (mIncompleteTextures.find(type) == mIncompleteTextures.end())
    {
        const GLubyte color[] = { 0, 0, 0, 255 };
        const gl::Extents colorSize(1, 1, 1);
        const gl::PixelUnpackState unpack(1, 0);
        const gl::Box area(0, 0, 0, 1, 1, 1);

        // Skip the API layer to avoid needing to pass the Context and mess with dirty bits.
        gl::Texture *t =
            new gl::Texture(createTexture(type), std::numeric_limits<GLuint>::max(), type);
        t->setStorage(type, 1, GL_RGBA8, colorSize);

        if (type == GL_TEXTURE_CUBE_MAP)
        {
            for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++)
            {
                t->getImplementation()->setSubImage(face, 0, area, GL_RGBA8, GL_UNSIGNED_BYTE,
                                                    unpack, color);
            }
        }
        else
        {
            t->getImplementation()->setSubImage(type, 0, area, GL_RGBA8, GL_UNSIGNED_BYTE, unpack,
                                                color);
        }

        mIncompleteTextures[type].set(t);
    }

    return mIncompleteTextures[type].get();
}

bool RendererD3D::isDeviceLost() const
{
    return mDeviceLost;
}

void RendererD3D::notifyDeviceLost()
{
    mDeviceLost = true;
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

gl::Error RendererD3D::getScratchMemoryBuffer(size_t requestedSize, MemoryBuffer **bufferOut)
{
    if (mScratchMemoryBuffer.size() == requestedSize)
    {
        mScratchMemoryBufferResetCounter = ScratchMemoryBufferLifetime;
        *bufferOut = &mScratchMemoryBuffer;
        return gl::Error(GL_NO_ERROR);
    }

    if (mScratchMemoryBuffer.size() > requestedSize)
    {
        mScratchMemoryBufferResetCounter--;
    }

    if (mScratchMemoryBufferResetCounter <= 0 || mScratchMemoryBuffer.size() < requestedSize)
    {
        mScratchMemoryBuffer.resize(0);
        if (!mScratchMemoryBuffer.resize(requestedSize))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate internal buffer.");
        }
        mScratchMemoryBufferResetCounter = ScratchMemoryBufferLifetime;
    }

    ASSERT(mScratchMemoryBuffer.size() >= requestedSize);

    *bufferOut = &mScratchMemoryBuffer;
    return gl::Error(GL_NO_ERROR);
}

void RendererD3D::insertEventMarker(GLsizei length, const char *marker)
{
    std::vector<wchar_t> wcstring (length + 1);
    size_t convertedChars = 0;
    errno_t err = mbstowcs_s(&convertedChars, wcstring.data(), length + 1, marker, _TRUNCATE);
    if (err == 0)
    {
        getAnnotator()->setMarker(wcstring.data());
    }
}

void RendererD3D::pushGroupMarker(GLsizei length, const char *marker)
{
    std::vector<wchar_t> wcstring(length + 1);
    size_t convertedChars = 0;
    errno_t err = mbstowcs_s(&convertedChars, wcstring.data(), length + 1, marker, _TRUNCATE);
    if (err == 0)
    {
        getAnnotator()->beginEvent(wcstring.data());
    }
}

void RendererD3D::popGroupMarker()
{
    getAnnotator()->endEvent();
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

void RendererD3D::onMakeCurrent(const gl::Data &data)
{
}

void RendererD3D::initializeDebugAnnotator()
{
    createAnnotator();
    ASSERT(mAnnotator);
    gl::InitializeDebugAnnotations(mAnnotator);
}

gl::DebugAnnotator *RendererD3D::getAnnotator()
{
    ASSERT(mAnnotator);
    return mAnnotator;
}
}  // namespace rx
