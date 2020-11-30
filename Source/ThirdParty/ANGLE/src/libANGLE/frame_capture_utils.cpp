//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_utils.cpp:
//   ANGLE frame capture util implementation.
//

#include "libANGLE/frame_capture_utils.h"

#include <vector>

#include "common/Color.h"
#include "common/MemoryBuffer.h"
#include "common/angleutils.h"
#include "libANGLE/BinaryStream.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Query.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/State.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/FramebufferImpl.h"

namespace angle
{

namespace
{

template <typename T>
void SerializeColor(gl::BinaryOutputStream *bos, const Color<T> &color)
{
    bos->writeInt(color.red);
    bos->writeInt(color.green);
    bos->writeInt(color.blue);
    bos->writeInt(color.alpha);
}

template void SerializeColor<float>(gl::BinaryOutputStream *bos, const Color<float> &color);

template <class ObjectType>
void SerializeOffsetBindingPointerVector(
    gl::BinaryOutputStream *bos,
    const std::vector<gl::OffsetBindingPointer<ObjectType>> &offsetBindingPointerVector)
{
    for (size_t i = 0; i < offsetBindingPointerVector.size(); i++)
    {
        bos->writeInt(offsetBindingPointerVector[i].id().value);
        bos->writeInt(offsetBindingPointerVector[i].getOffset());
        bos->writeInt(offsetBindingPointerVector[i].getSize());
    }
}

template void SerializeOffsetBindingPointerVector<gl::Buffer>(
    gl::BinaryOutputStream *bos,
    const std::vector<gl::OffsetBindingPointer<gl::Buffer>> &offsetBindingPointerVector);

template <class ObjectType>
void SerializeBindingPointerVector(
    gl::BinaryOutputStream *bos,
    const std::vector<gl::BindingPointer<ObjectType>> &bindingPointerVector)
{
    for (size_t i = 0; i < bindingPointerVector.size(); i++)
    {
        bos->writeInt(bindingPointerVector[i].id().value);
    }
}

template void SerializeBindingPointerVector<gl::Texture>(
    gl::BinaryOutputStream *bos,
    const std::vector<gl::BindingPointer<gl::Texture>> &bindingPointerVector);

template void SerializeBindingPointerVector<gl::Sampler>(
    gl::BinaryOutputStream *bos,
    const std::vector<gl::BindingPointer<gl::Sampler>> &bindingPointerVector);

bool IsValidColorAttachmentBinding(GLenum binding, size_t colorAttachmentsCount)
{
    return binding == GL_BACK || (binding >= GL_COLOR_ATTACHMENT0 &&
                                  (binding - GL_COLOR_ATTACHMENT0) < colorAttachmentsCount);
}

Result ReadPixelsFromAttachment(const gl::Context *context,
                                gl::Framebuffer *framebuffer,
                                const gl::FramebufferAttachment &framebufferAttachment,
                                ScratchBuffer *scratchBuffer,
                                MemoryBuffer **pixels)
{
    gl::Extents extents       = framebufferAttachment.getSize();
    GLenum binding            = framebufferAttachment.getBinding();
    gl::InternalFormat format = *framebufferAttachment.getFormat().info;
    if (IsValidColorAttachmentBinding(binding,
                                      framebuffer->getState().getColorAttachments().size()))
    {
        format = framebuffer->getImplementation()->getImplementationColorReadFormat(context);
    }
    ANGLE_CHECK_GL_ALLOC(const_cast<gl::Context *>(context),
                         scratchBuffer->getInitialized(
                             format.pixelBytes * extents.width * extents.height, pixels, 0));
    ANGLE_TRY(framebuffer->readPixels(context, gl::Rectangle{0, 0, extents.width, extents.height},
                                      format.format, format.type, gl::PixelPackState{}, nullptr,
                                      (*pixels)->data()));
    return Result::Continue;
}
void SerializeImageIndex(gl::BinaryOutputStream *bos, const gl::ImageIndex &imageIndex)
{
    bos->writeEnum(imageIndex.getType());
    bos->writeInt(imageIndex.getLevelIndex());
    bos->writeInt(imageIndex.getLayerIndex());
    bos->writeInt(imageIndex.getLayerCount());
}

Result SerializeFramebufferAttachment(const gl::Context *context,
                                      gl::BinaryOutputStream *bos,
                                      ScratchBuffer *scratchBuffer,
                                      gl::Framebuffer *framebuffer,
                                      const gl::FramebufferAttachment &framebufferAttachment)
{
    bos->writeInt(framebufferAttachment.type());
    // serialize target variable
    bos->writeInt(framebufferAttachment.getBinding());
    if (framebufferAttachment.type() == GL_TEXTURE)
    {
        SerializeImageIndex(bos, framebufferAttachment.getTextureImageIndex());
    }
    bos->writeInt(framebufferAttachment.getNumViews());
    bos->writeInt(framebufferAttachment.isMultiview());
    bos->writeInt(framebufferAttachment.getBaseViewIndex());
    bos->writeInt(framebufferAttachment.getRenderToTextureSamples());

    GLenum prevReadBufferState = framebuffer->getReadBufferState();
    GLenum binding             = framebufferAttachment.getBinding();
    if (IsValidColorAttachmentBinding(binding,
                                      framebuffer->getState().getColorAttachments().size()))
    {
        framebuffer->setReadBuffer(framebufferAttachment.getBinding());
        ANGLE_TRY(framebuffer->syncState(context, GL_FRAMEBUFFER));
    }
    MemoryBuffer *pixelsPtr = nullptr;
    ANGLE_TRY(ReadPixelsFromAttachment(context, framebuffer, framebufferAttachment, scratchBuffer,
                                       &pixelsPtr));
    bos->writeBytes(pixelsPtr->data(), pixelsPtr->size());
    // Reset framebuffer state
    framebuffer->setReadBuffer(prevReadBufferState);
    return Result::Continue;
}

Result SerializeFramebufferState(const gl::Context *context,
                                 gl::BinaryOutputStream *bos,
                                 ScratchBuffer *scratchBuffer,
                                 gl::Framebuffer *framebuffer,
                                 const gl::FramebufferState &framebufferState)
{
    bos->writeInt(framebufferState.id().value);
    bos->writeString(framebufferState.getLabel());
    bos->writeIntVector(framebufferState.getDrawBufferStates());
    bos->writeInt(framebufferState.getReadBufferState());
    bos->writeInt(framebufferState.getDefaultWidth());
    bos->writeInt(framebufferState.getDefaultHeight());
    bos->writeInt(framebufferState.getDefaultSamples());
    bos->writeInt(framebufferState.getDefaultFixedSampleLocations());
    bos->writeInt(framebufferState.getDefaultLayers());

    const std::vector<gl::FramebufferAttachment> &colorAttachments =
        framebufferState.getColorAttachments();
    for (const gl::FramebufferAttachment &colorAttachment : colorAttachments)
    {
        if (colorAttachment.isAttached())
        {
            ANGLE_TRY(SerializeFramebufferAttachment(context, bos, scratchBuffer, framebuffer,
                                                     colorAttachment));
        }
    }
    if (framebuffer->getDepthStencilAttachment())
    {
        ANGLE_TRY(SerializeFramebufferAttachment(context, bos, scratchBuffer, framebuffer,
                                                 *framebuffer->getDepthStencilAttachment()));
    }
    else
    {
        if (framebuffer->getDepthAttachment())
        {
            ANGLE_TRY(SerializeFramebufferAttachment(context, bos, scratchBuffer, framebuffer,
                                                     *framebuffer->getDepthAttachment()));
        }
        if (framebuffer->getStencilAttachment())
        {
            ANGLE_TRY(SerializeFramebufferAttachment(context, bos, scratchBuffer, framebuffer,
                                                     *framebuffer->getStencilAttachment()));
        }
    }
    return Result::Continue;
}

Result SerializeFramebuffer(const gl::Context *context,
                            gl::BinaryOutputStream *bos,
                            ScratchBuffer *scratchBuffer,
                            gl::Framebuffer *framebuffer)
{
    return SerializeFramebufferState(context, bos, scratchBuffer, framebuffer,
                                     framebuffer->getState());
}

void SerializeRasterizerState(gl::BinaryOutputStream *bos,
                              const gl::RasterizerState &rasterizerState)
{
    bos->writeInt(rasterizerState.cullFace);
    bos->writeEnum(rasterizerState.cullMode);
    bos->writeInt(rasterizerState.frontFace);
    bos->writeInt(rasterizerState.polygonOffsetFill);
    bos->writeInt(rasterizerState.polygonOffsetFactor);
    bos->writeInt(rasterizerState.polygonOffsetUnits);
    bos->writeInt(rasterizerState.pointDrawMode);
    bos->writeInt(rasterizerState.multiSample);
    bos->writeInt(rasterizerState.rasterizerDiscard);
    bos->writeInt(rasterizerState.dither);
}

void SerializeRectangle(gl::BinaryOutputStream *bos, const gl::Rectangle &rectangle)
{
    bos->writeInt(rectangle.x);
    bos->writeInt(rectangle.y);
    bos->writeInt(rectangle.width);
    bos->writeInt(rectangle.height);
}

void SerializeBlendState(gl::BinaryOutputStream *bos, const gl::BlendState &blendState)
{
    bos->writeInt(blendState.blend);
    bos->writeInt(blendState.sourceBlendRGB);
    bos->writeInt(blendState.destBlendRGB);
    bos->writeInt(blendState.sourceBlendAlpha);
    bos->writeInt(blendState.destBlendAlpha);
    bos->writeInt(blendState.blendEquationRGB);
    bos->writeInt(blendState.blendEquationAlpha);
    bos->writeInt(blendState.colorMaskRed);
    bos->writeInt(blendState.colorMaskGreen);
    bos->writeInt(blendState.colorMaskBlue);
    bos->writeInt(blendState.colorMaskAlpha);
}

void SerializeDepthStencilState(gl::BinaryOutputStream *bos,
                                const gl::DepthStencilState &depthStencilState)
{
    bos->writeInt(depthStencilState.depthTest);
    bos->writeInt(depthStencilState.depthFunc);
    bos->writeInt(depthStencilState.depthMask);
    bos->writeInt(depthStencilState.stencilTest);
    bos->writeInt(depthStencilState.stencilFunc);
    bos->writeInt(depthStencilState.stencilMask);
    bos->writeInt(depthStencilState.stencilFail);
    bos->writeInt(depthStencilState.stencilPassDepthFail);
    bos->writeInt(depthStencilState.stencilPassDepthPass);
    bos->writeInt(depthStencilState.stencilWritemask);
    bos->writeInt(depthStencilState.stencilBackFunc);
    bos->writeInt(depthStencilState.stencilBackMask);
    bos->writeInt(depthStencilState.stencilBackFail);
    bos->writeInt(depthStencilState.stencilBackPassDepthFail);
    bos->writeInt(depthStencilState.stencilBackPassDepthPass);
    bos->writeInt(depthStencilState.stencilBackWritemask);
}

void SerializeVertexAttribCurrentValueData(
    gl::BinaryOutputStream *bos,
    const gl::VertexAttribCurrentValueData &vertexAttribCurrentValueData)
{
    ASSERT(vertexAttribCurrentValueData.Type == gl::VertexAttribType::Float ||
           vertexAttribCurrentValueData.Type == gl::VertexAttribType::Int ||
           vertexAttribCurrentValueData.Type == gl::VertexAttribType::UnsignedInt);
    if (vertexAttribCurrentValueData.Type == gl::VertexAttribType::Float)
    {
        bos->writeInt(vertexAttribCurrentValueData.Values.FloatValues[0]);
        bos->writeInt(vertexAttribCurrentValueData.Values.FloatValues[1]);
        bos->writeInt(vertexAttribCurrentValueData.Values.FloatValues[2]);
        bos->writeInt(vertexAttribCurrentValueData.Values.FloatValues[3]);
    }
    else if (vertexAttribCurrentValueData.Type == gl::VertexAttribType::Int)
    {
        bos->writeInt(vertexAttribCurrentValueData.Values.IntValues[0]);
        bos->writeInt(vertexAttribCurrentValueData.Values.IntValues[1]);
        bos->writeInt(vertexAttribCurrentValueData.Values.IntValues[2]);
        bos->writeInt(vertexAttribCurrentValueData.Values.IntValues[3]);
    }
    else
    {
        bos->writeInt(vertexAttribCurrentValueData.Values.UnsignedIntValues[0]);
        bos->writeInt(vertexAttribCurrentValueData.Values.UnsignedIntValues[1]);
        bos->writeInt(vertexAttribCurrentValueData.Values.UnsignedIntValues[2]);
        bos->writeInt(vertexAttribCurrentValueData.Values.UnsignedIntValues[3]);
    }
}

void SerializePixelPackState(gl::BinaryOutputStream *bos, const gl::PixelPackState &pixelPackState)
{
    bos->writeInt(pixelPackState.alignment);
    bos->writeInt(pixelPackState.rowLength);
    bos->writeInt(pixelPackState.skipRows);
    bos->writeInt(pixelPackState.skipPixels);
    bos->writeInt(pixelPackState.imageHeight);
    bos->writeInt(pixelPackState.skipImages);
    bos->writeInt(pixelPackState.reverseRowOrder);
}

void SerializePixelUnpackState(gl::BinaryOutputStream *bos,
                               const gl::PixelUnpackState &pixelUnpackState)
{
    bos->writeInt(pixelUnpackState.alignment);
    bos->writeInt(pixelUnpackState.rowLength);
    bos->writeInt(pixelUnpackState.skipRows);
    bos->writeInt(pixelUnpackState.skipPixels);
    bos->writeInt(pixelUnpackState.imageHeight);
    bos->writeInt(pixelUnpackState.skipImages);
}

void SerializeImageUnit(gl::BinaryOutputStream *bos, const gl::ImageUnit &imageUnit)
{
    bos->writeInt(imageUnit.level);
    bos->writeInt(imageUnit.layered);
    bos->writeInt(imageUnit.layer);
    bos->writeInt(imageUnit.access);
    bos->writeInt(imageUnit.format);
    bos->writeInt(imageUnit.texture.id().value);
}

void SerializeGLContextStates(gl::BinaryOutputStream *bos, const gl::State &state)
{
    bos->writeInt(state.getClientType());
    bos->writeInt(state.getContextPriority());
    bos->writeInt(state.getClientMajorVersion());
    bos->writeInt(state.getClientMinorVersion());

    SerializeColor(bos, state.getColorClearValue());
    bos->writeInt(state.getDepthClearValue());
    bos->writeInt(state.getStencilClearValue());
    SerializeRasterizerState(bos, state.getRasterizerState());
    bos->writeInt(state.isScissorTestEnabled());
    SerializeRectangle(bos, state.getScissor());
    const gl::BlendStateArray &blendStateArray = state.getBlendStateArray();
    for (size_t i = 0; i < blendStateArray.size(); i++)
    {
        SerializeBlendState(bos, blendStateArray[i]);
    }
    SerializeColor(bos, state.getBlendColor());
    bos->writeInt(state.isSampleAlphaToCoverageEnabled());
    bos->writeInt(state.isSampleCoverageEnabled());
    bos->writeInt(state.getSampleCoverageValue());
    bos->writeInt(state.getSampleCoverageInvert());
    bos->writeInt(state.isSampleMaskEnabled());
    bos->writeInt(state.getMaxSampleMaskWords());
    const auto &sampleMaskValues = state.getSampleMaskValues();
    for (size_t i = 0; i < sampleMaskValues.size(); i++)
    {
        bos->writeInt(sampleMaskValues[i]);
    }
    SerializeDepthStencilState(bos, state.getDepthStencilState());
    bos->writeInt(state.getStencilRef());
    bos->writeInt(state.getStencilBackRef());
    bos->writeInt(state.getLineWidth());
    bos->writeInt(state.getGenerateMipmapHint());
    bos->writeInt(state.getTextureFilteringHint());
    bos->writeInt(state.getFragmentShaderDerivativeHint());
    bos->writeInt(state.isBindGeneratesResourceEnabled());
    bos->writeInt(state.areClientArraysEnabled());
    SerializeRectangle(bos, state.getViewport());
    bos->writeInt(state.getNearPlane());
    bos->writeInt(state.getFarPlane());
    if (state.getReadFramebuffer())
    {
        bos->writeInt(state.getReadFramebuffer()->id().value);
    }
    if (state.getDrawFramebuffer())
    {
        bos->writeInt(state.getDrawFramebuffer()->id().value);
    }
    bos->writeInt(state.getRenderbufferId().value);
    if (state.getProgram())
    {
        bos->writeInt(state.getProgram()->id().value);
    }
    if (state.getProgramPipeline())
    {
        bos->writeInt(state.getProgramPipeline()->id().value);
    }
    bos->writeEnum(state.getProvokingVertex());
    const std::vector<gl::VertexAttribCurrentValueData> &vertexAttribCurrentValues =
        state.getVertexAttribCurrentValues();
    for (size_t i = 0; i < vertexAttribCurrentValues.size(); i++)
    {
        SerializeVertexAttribCurrentValueData(bos, vertexAttribCurrentValues[i]);
    }
    if (state.getVertexArray())
    {
        bos->writeInt(state.getVertexArray()->id().value);
    }
    bos->writeInt(state.getCurrentValuesTypeMask().to_ulong());
    bos->writeInt(state.getActiveSampler());
    for (const auto &textures : state.getBoundTexturesForCapture())
    {
        SerializeBindingPointerVector<gl::Texture>(bos, textures);
    }
    bos->writeInt(state.getTexturesIncompatibleWithSamplers().to_ulong());
    SerializeBindingPointerVector<gl::Sampler>(bos, state.getSamplers());
    for (const gl::ImageUnit &imageUnit : state.getImageUnits())
    {
        SerializeImageUnit(bos, imageUnit);
    }
    for (const auto &query : state.getActiveQueriesForCapture())
    {
        bos->writeInt(query.id().value);
    }
    for (const auto &boundBuffer : state.getBoundBuffersForCapture())
    {
        bos->writeInt(boundBuffer.id().value);
    }
    SerializeOffsetBindingPointerVector<gl::Buffer>(bos,
                                                    state.getOffsetBindingPointerUniformBuffers());
    SerializeOffsetBindingPointerVector<gl::Buffer>(
        bos, state.getOffsetBindingPointerAtomicCounterBuffers());
    SerializeOffsetBindingPointerVector<gl::Buffer>(
        bos, state.getOffsetBindingPointerShaderStorageBuffers());
    if (state.getCurrentTransformFeedback())
    {
        bos->writeInt(state.getCurrentTransformFeedback()->id().value);
    }
    SerializePixelUnpackState(bos, state.getUnpackState());
    SerializePixelPackState(bos, state.getPackState());
    bos->writeInt(state.isPrimitiveRestartEnabled());
    bos->writeInt(state.isMultisamplingEnabled());
    bos->writeInt(state.isSampleAlphaToOneEnabled());
    bos->writeInt(state.getCoverageModulation());
    bos->writeInt(state.getFramebufferSRGB());
    bos->writeInt(state.isRobustResourceInitEnabled());
    bos->writeInt(state.isProgramBinaryCacheEnabled());
    bos->writeInt(state.isTextureRectangleEnabled());
    bos->writeInt(state.getMaxShaderCompilerThreads());
    bos->writeInt(state.getEnabledClipDistances().to_ulong());
    bos->writeInt(state.getBlendFuncConstantAlphaDrawBuffers().to_ulong());
    bos->writeInt(state.getBlendFuncConstantColorDrawBuffers().to_ulong());
    bos->writeInt(state.noSimultaneousConstantColorAndAlphaBlendFunc());
}

void SerializeBufferState(gl::BinaryOutputStream *bos, const gl::BufferState &bufferState)
{
    bos->writeString(bufferState.getLabel());
    bos->writeEnum(bufferState.getUsage());
    bos->writeInt(bufferState.getSize());
    bos->writeInt(bufferState.getAccessFlags());
    bos->writeInt(bufferState.getAccess());
    bos->writeInt(bufferState.isMapped());
    bos->writeInt(bufferState.getMapOffset());
    bos->writeInt(bufferState.getMapLength());
}

Result SerializeBuffer(const gl::Context *context,
                       gl::BinaryOutputStream *bos,
                       ScratchBuffer *scratchBuffer,
                       gl::Buffer *buffer)
{
    SerializeBufferState(bos, buffer->getState());
    MemoryBuffer *dataPtr = nullptr;
    ANGLE_CHECK_GL_ALLOC(
        const_cast<gl::Context *>(context),
        scratchBuffer->getInitialized(static_cast<size_t>(buffer->getSize()), &dataPtr, 0));
    ANGLE_TRY(buffer->getSubData(context, 0, dataPtr->size(), dataPtr->data()));
    bos->writeBytes(dataPtr->data(), dataPtr->size());
    return Result::Continue;
}
void SerializeColorGeneric(gl::BinaryOutputStream *bos, const ColorGeneric &colorGeneric)
{
    ASSERT(colorGeneric.type == ColorGeneric::Type::Float ||
           colorGeneric.type == ColorGeneric::Type::Int ||
           colorGeneric.type == ColorGeneric::Type::UInt);
    bos->writeEnum(colorGeneric.type);
    if (colorGeneric.type == ColorGeneric::Type::Float)
    {
        SerializeColor(bos, colorGeneric.colorF);
    }
    else if (colorGeneric.type == ColorGeneric::Type::Int)
    {
        SerializeColor(bos, colorGeneric.colorI);
    }
    else
    {
        SerializeColor(bos, colorGeneric.colorUI);
    }
}

void SerializeSamplerState(gl::BinaryOutputStream *bos, const gl::SamplerState &samplerState)
{
    bos->writeInt(samplerState.getMinFilter());
    bos->writeInt(samplerState.getMagFilter());
    bos->writeInt(samplerState.getWrapS());
    bos->writeInt(samplerState.getWrapT());
    bos->writeInt(samplerState.getWrapR());
    bos->writeInt(samplerState.getMaxAnisotropy());
    bos->writeInt(samplerState.getMinLod());
    bos->writeInt(samplerState.getMaxLod());
    bos->writeInt(samplerState.getCompareMode());
    bos->writeInt(samplerState.getCompareFunc());
    bos->writeInt(samplerState.getSRGBDecode());
    SerializeColorGeneric(bos, samplerState.getBorderColor());
}

void SerializeSampler(gl::BinaryOutputStream *bos, gl::Sampler *sampler)
{
    bos->writeString(sampler->getLabel());
    SerializeSamplerState(bos, sampler->getSamplerState());
}

}  // namespace

Result SerializeContext(gl::BinaryOutputStream *bos, const gl::Context *context)
{
    SerializeGLContextStates(bos, context->getState());
    ScratchBuffer scratchBuffer(1);
    const gl::FramebufferManager &framebufferManager =
        context->getState().getFramebufferManagerForCapture();
    for (const auto &framebuffer : framebufferManager)
    {
        gl::Framebuffer *framebufferPtr = framebuffer.second;
        ANGLE_TRY(SerializeFramebuffer(context, bos, &scratchBuffer, framebufferPtr));
    }
    const gl::BufferManager &bufferManager = context->getState().getBufferManagerForCapture();
    for (const auto &buffer : bufferManager)
    {
        gl::Buffer *bufferPtr = buffer.second;
        ANGLE_TRY(SerializeBuffer(context, bos, &scratchBuffer, bufferPtr));
    }
    const gl::SamplerManager &samplerManager = context->getState().getSamplerManagerForCapture();
    for (const auto &sampler : samplerManager)
    {
        gl::Sampler *samplerPtr = sampler.second;
        SerializeSampler(bos, samplerPtr);
    }
    scratchBuffer.clear();
    return Result::Continue;
}

}  // namespace angle
