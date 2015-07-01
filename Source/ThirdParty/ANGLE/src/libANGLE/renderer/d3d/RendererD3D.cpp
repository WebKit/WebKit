//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererD3D.cpp: Implementation of the base D3D Renderer.

#include "libANGLE/renderer/d3d/RendererD3D.h"

#include "common/MemoryBuffer.h"
#include "common/utilities.h"
#include "libANGLE/Display.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/State.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/DisplayD3D.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"

namespace rx
{

namespace
{
// If we request a scratch buffer requesting a smaller size this many times,
// release and recreate the scratch buffer. This ensures we don't have a
// degenerate case where we are stuck hogging memory.
const int ScratchMemoryBufferLifetime = 1000;
}

RendererD3D::RendererD3D(egl::Display *display)
    : mDisplay(display),
      mDeviceLost(false),
      mScratchMemoryBufferResetCounter(0)
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
        incompleteTexture.second.set(NULL);
    }
    mIncompleteTextures.clear();
}

gl::Error RendererD3D::drawElements(const gl::Data &data,
                                    GLenum mode, GLsizei count, GLenum type,
                                    const GLvoid *indices, GLsizei instances,
                                    const RangeUI &indexRange)
{
    if (data.state->isPrimitiveRestartEnabled())
    {
        UNIMPLEMENTED();
        return gl::Error(GL_INVALID_OPERATION, "Primitive restart not implemented");
    }

    gl::Program *program = data.state->getProgram();
    ASSERT(program != NULL);

    program->updateSamplerMapping();

    gl::Error error = generateSwizzles(data);
    if (error.isError())
    {
        return error;
    }

    if (!applyPrimitiveType(mode, count, program->usesPointSize()))
    {
        return gl::Error(GL_NO_ERROR);
    }

    error = applyRenderTarget(data, mode, false);
    if (error.isError())
    {
        return error;
    }

    error = applyState(data, mode);
    if (error.isError())
    {
        return error;
    }

    gl::VertexArray *vao = data.state->getVertexArray();
    TranslatedIndexData indexInfo;
    indexInfo.indexRange = indexRange;
    error = applyIndexBuffer(indices, vao->getElementArrayBuffer(), count, mode, type, &indexInfo);
    if (error.isError())
    {
        return error;
    }

    applyTransformFeedbackBuffers(*data.state);
    // Transform feedback is not allowed for DrawElements, this error should have been caught at the API validation
    // layer.
    ASSERT(!data.state->isTransformFeedbackActiveUnpaused());

    GLsizei vertexCount = indexInfo.indexRange.length() + 1;
    error = applyVertexBuffer(*data.state, mode, indexInfo.indexRange.start, vertexCount, instances);
    if (error.isError())
    {
        return error;
    }

    error = applyShaders(data);
    if (error.isError())
    {
        return error;
    }

    error = applyTextures(data);
    if (error.isError())
    {
        return error;
    }

    error = program->applyUniformBuffers(data);
    if (error.isError())
    {
        return error;
    }

    if (!skipDraw(data, mode))
    {
        error = drawElements(mode, count, type, indices, vao->getElementArrayBuffer(), indexInfo, instances);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error RendererD3D::drawArrays(const gl::Data &data,
                                  GLenum mode, GLint first,
                                  GLsizei count, GLsizei instances)
{
    gl::Program *program = data.state->getProgram();
    ASSERT(program != NULL);

    program->updateSamplerMapping();

    gl::Error error = generateSwizzles(data);
    if (error.isError())
    {
        return error;
    }

    if (!applyPrimitiveType(mode, count, program->usesPointSize()))
    {
        return gl::Error(GL_NO_ERROR);
    }

    error = applyRenderTarget(data, mode, false);
    if (error.isError())
    {
        return error;
    }

    error = applyState(data, mode);
    if (error.isError())
    {
        return error;
    }

    applyTransformFeedbackBuffers(*data.state);

    error = applyVertexBuffer(*data.state, mode, first, count, instances);
    if (error.isError())
    {
        return error;
    }

    error = applyShaders(data);
    if (error.isError())
    {
        return error;
    }

    error = applyTextures(data);
    if (error.isError())
    {
        return error;
    }

    error = program->applyUniformBuffers(data);
    if (error.isError())
    {
        return error;
    }

    if (!skipDraw(data, mode))
    {
        error = drawArrays(data, mode, count, instances, program->usesPointSize());
        if (error.isError())
        {
            return error;
        }

        if (data.state->isTransformFeedbackActiveUnpaused())
        {
            markTransformFeedbackUsage(data);
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error RendererD3D::generateSwizzles(const gl::Data &data, gl::SamplerType type)
{
    gl::Program *program = data.state->getProgram();

    size_t samplerRange = program->getUsedSamplerRange(type);

    for (size_t i = 0; i < samplerRange; i++)
    {
        GLenum textureType = program->getSamplerTextureType(type, i);
        GLint textureUnit = program->getSamplerMapping(type, i, *data.caps);
        if (textureUnit != -1)
        {
            gl::Texture *texture = data.state->getSamplerTexture(textureUnit, textureType);
            ASSERT(texture);
            if (texture->getSamplerState().swizzleRequired())
            {
                gl::Error error = generateSwizzle(texture);
                if (error.isError())
                {
                    return error;
                }
            }
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error RendererD3D::generateSwizzles(const gl::Data &data)
{
    gl::Error error = generateSwizzles(data, gl::SAMPLER_VERTEX);
    if (error.isError())
    {
        return error;
    }

    error = generateSwizzles(data, gl::SAMPLER_PIXEL);
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

// Applies the render target surface, depth stencil surface, viewport rectangle and
// scissor rectangle to the renderer
gl::Error RendererD3D::applyRenderTarget(const gl::Data &data, GLenum drawMode, bool ignoreViewport)
{
    const gl::Framebuffer *framebufferObject = data.state->getDrawFramebuffer();
    ASSERT(framebufferObject && framebufferObject->checkStatus(data) == GL_FRAMEBUFFER_COMPLETE);

    gl::Error error = applyRenderTarget(framebufferObject);
    if (error.isError())
    {
        return error;
    }

    float nearZ = data.state->getNearPlane();
    float farZ = data.state->getFarPlane();
    setViewport(data.state->getViewport(), nearZ, farZ, drawMode,
                data.state->getRasterizerState().frontFace, ignoreViewport);

    setScissorRectangle(data.state->getScissor(), data.state->isScissorTestEnabled());

    return gl::Error(GL_NO_ERROR);
}

// Applies the fixed-function state (culling, depth test, alpha blending, stenciling, etc) to the Direct3D device
gl::Error RendererD3D::applyState(const gl::Data &data, GLenum drawMode)
{
    const gl::Framebuffer *framebufferObject = data.state->getDrawFramebuffer();
    int samples = framebufferObject->getSamples(data);

    gl::RasterizerState rasterizer = data.state->getRasterizerState();
    rasterizer.pointDrawMode = (drawMode == GL_POINTS);
    rasterizer.multiSample = (samples != 0);

    gl::Error error = setRasterizerState(rasterizer);
    if (error.isError())
    {
        return error;
    }

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
    error = setBlendState(framebufferObject, data.state->getBlendState(), data.state->getBlendColor(), mask);
    if (error.isError())
    {
        return error;
    }

    error = setDepthStencilState(data.state->getDepthStencilState(), data.state->getStencilRef(),
                                 data.state->getStencilBackRef(), rasterizer.frontFace == GL_CCW);
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

// Applies the shaders and shader constants to the Direct3D device
gl::Error RendererD3D::applyShaders(const gl::Data &data)
{
    gl::Program *program = data.state->getProgram();

    gl::VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS];
    gl::VertexFormat::GetInputLayout(inputLayout, program, *data.state);

    const gl::Framebuffer *fbo = data.state->getDrawFramebuffer();

    gl::Error error = applyShaders(program, inputLayout, fbo, data.state->getRasterizerState().rasterizerDiscard, data.state->isTransformFeedbackActiveUnpaused());
    if (error.isError())
    {
        return error;
    }

    return program->applyUniforms();
}

// For each Direct3D sampler of either the pixel or vertex stage,
// looks up the corresponding OpenGL texture image unit and texture type,
// and sets the texture and its addressing/filtering state (or NULL when inactive).
gl::Error RendererD3D::applyTextures(const gl::Data &data, gl::SamplerType shaderType,
                                     const FramebufferTextureSerialArray &framebufferSerials, size_t framebufferSerialCount)
{
    gl::Program *program = data.state->getProgram();

    size_t samplerRange = program->getUsedSamplerRange(shaderType);
    for (size_t samplerIndex = 0; samplerIndex < samplerRange; samplerIndex++)
    {
        GLenum textureType = program->getSamplerTextureType(shaderType, samplerIndex);
        GLint textureUnit = program->getSamplerMapping(shaderType, samplerIndex, *data.caps);
        if (textureUnit != -1)
        {
            gl::Texture *texture = data.state->getSamplerTexture(textureUnit, textureType);
            ASSERT(texture);
            gl::SamplerState sampler = texture->getSamplerState();

            gl::Sampler *samplerObject = data.state->getSampler(textureUnit);
            if (samplerObject)
            {
                samplerObject->getState(&sampler);
            }

            // TODO: std::binary_search may become unavailable using older versions of GCC
            if (texture->isSamplerComplete(sampler, data) &&
                !std::binary_search(framebufferSerials.begin(), framebufferSerials.begin() + framebufferSerialCount, texture->getTextureSerial()))
            {
                gl::Error error = setSamplerState(shaderType, samplerIndex, texture, sampler);
                if (error.isError())
                {
                    return error;
                }

                error = setTexture(shaderType, samplerIndex, texture);
                if (error.isError())
                {
                    return error;
                }
            }
            else
            {
                // Texture is not sampler complete or it is in use by the framebuffer.  Bind the incomplete texture.
                gl::Texture *incompleteTexture = getIncompleteTexture(textureType);
                gl::Error error = setTexture(shaderType, samplerIndex, incompleteTexture);
                if (error.isError())
                {
                    return error;
                }
            }
        }
        else
        {
            // No texture bound to this slot even though it is used by the shader, bind a NULL texture
            gl::Error error = setTexture(shaderType, samplerIndex, NULL);
            if (error.isError())
            {
                return error;
            }
        }
    }

    // Set all the remaining textures to NULL
    size_t samplerCount = (shaderType == gl::SAMPLER_PIXEL) ? data.caps->maxTextureImageUnits
                                                            : data.caps->maxVertexTextureImageUnits;
    for (size_t samplerIndex = samplerRange; samplerIndex < samplerCount; samplerIndex++)
    {
        gl::Error error = setTexture(shaderType, samplerIndex, NULL);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error RendererD3D::applyTextures(const gl::Data &data)
{
    FramebufferTextureSerialArray framebufferSerials;
    size_t framebufferSerialCount = getBoundFramebufferTextureSerials(data, &framebufferSerials);

    gl::Error error = applyTextures(data, gl::SAMPLER_VERTEX, framebufferSerials, framebufferSerialCount);
    if (error.isError())
    {
        return error;
    }

    error = applyTextures(data, gl::SAMPLER_PIXEL, framebufferSerials, framebufferSerialCount);
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

bool RendererD3D::skipDraw(const gl::Data &data, GLenum drawMode)
{
    if (drawMode == GL_POINTS)
    {
        // ProgramBinary assumes non-point rendering if gl_PointSize isn't written,
        // which affects varying interpolation. Since the value of gl_PointSize is
        // undefined when not written, just skip drawing to avoid unexpected results.
        if (!data.state->getProgram()->usesPointSize() && !data.state->isTransformFeedbackActiveUnpaused())
        {
            // This is stictly speaking not an error, but developers should be
            // notified of risking undefined behavior.
            ERR("Point rendering without writing to gl_PointSize.");

            return true;
        }
    }
    else if (gl::IsTriangleMode(drawMode))
    {
        if (data.state->getRasterizerState().cullFace && data.state->getRasterizerState().cullMode == GL_FRONT_AND_BACK)
        {
            return true;
        }
    }

    return false;
}

void RendererD3D::markTransformFeedbackUsage(const gl::Data &data)
{
    const gl::TransformFeedback *transformFeedback = data.state->getCurrentTransformFeedback();
    for (size_t i = 0; i < transformFeedback->getIndexedBufferCount(); i++)
    {
        const OffsetBindingPointer<gl::Buffer> &binding = transformFeedback->getIndexedBuffer(i);
        if (binding.get() != nullptr)
        {
            BufferD3D *bufferD3D = GetImplAs<BufferD3D>(binding.get());
            bufferD3D->markTransformFeedbackUsage();
        }
    }
}

size_t RendererD3D::getBoundFramebufferTextureSerials(const gl::Data &data,
                                                      FramebufferTextureSerialArray *outSerialArray)
{
    size_t serialCount = 0;

    const gl::Framebuffer *drawFramebuffer = data.state->getDrawFramebuffer();
    for (unsigned int i = 0; i < data.caps->maxColorAttachments; i++)
    {
        const gl::FramebufferAttachment *attachment = drawFramebuffer->getColorbuffer(i);
        if (attachment && attachment->type() == GL_TEXTURE)
        {
            gl::Texture *texture = attachment->getTexture();
            (*outSerialArray)[serialCount++] = texture->getTextureSerial();
        }
    }

    const gl::FramebufferAttachment *depthStencilAttachment = drawFramebuffer->getDepthOrStencilbuffer();
    if (depthStencilAttachment && depthStencilAttachment->type() == GL_TEXTURE)
    {
        gl::Texture *depthStencilTexture = depthStencilAttachment->getTexture();
        (*outSerialArray)[serialCount++] = depthStencilTexture->getTextureSerial();
    }

    std::sort(outSerialArray->begin(), outSerialArray->begin() + serialCount);

    return serialCount;
}

gl::Texture *RendererD3D::getIncompleteTexture(GLenum type)
{
    if (mIncompleteTextures.find(type) == mIncompleteTextures.end())
    {
        const GLubyte color[] = { 0, 0, 0, 255 };
        const gl::Extents colorSize(1, 1, 1);
        const gl::PixelUnpackState incompleteUnpackState(1, 0);

        gl::Texture* t = new gl::Texture(createTexture(type), gl::Texture::INCOMPLETE_TEXTURE_ID, type);

        if (type == GL_TEXTURE_CUBE_MAP)
        {
            for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++)
            {
                t->setImage(face, 0, GL_RGBA, colorSize, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
            }
        }
        else
        {
            t->setImage(type, 0, GL_RGBA, colorSize, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
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

}
