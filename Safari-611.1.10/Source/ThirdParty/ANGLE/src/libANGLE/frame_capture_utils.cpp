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
#include "libANGLE/ResourceMap.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/State.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/FramebufferImpl.h"
#include "libANGLE/renderer/RenderbufferImpl.h"

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

template <class T>
void SerializeRange(gl::BinaryOutputStream *bos, const gl::Range<T> &range)
{
    bos->writeInt(range.low());
    bos->writeInt(range.high());
}

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
    if (framebufferAttachment.type() == GL_TEXTURE ||
        framebufferAttachment.type() == GL_RENDERBUFFER)
    {
        bos->writeInt(framebufferAttachment.id());
    }
    bos->writeInt(framebufferAttachment.type());
    // serialize target variable
    bos->writeInt(framebufferAttachment.getBinding());
    if (framebufferAttachment.type() == GL_TEXTURE)
    {
        SerializeImageIndex(bos, framebufferAttachment.getTextureImageIndex());
    }
    bos->writeInt(framebufferAttachment.getNumViews());
    bos->writeBool(framebufferAttachment.isMultiview());
    bos->writeInt(framebufferAttachment.getBaseViewIndex());
    bos->writeInt(framebufferAttachment.getRenderToTextureSamples());

    if (framebufferAttachment.type() != GL_TEXTURE &&
        framebufferAttachment.type() != GL_RENDERBUFFER)
    {
        GLenum prevReadBufferState = framebuffer->getReadBufferState();
        GLenum binding             = framebufferAttachment.getBinding();
        if (IsValidColorAttachmentBinding(binding,
                                          framebuffer->getState().getColorAttachments().size()))
        {
            framebuffer->setReadBuffer(framebufferAttachment.getBinding());
            ANGLE_TRY(framebuffer->syncState(context, GL_FRAMEBUFFER, gl::Command::Other));
        }
        MemoryBuffer *pixelsPtr = nullptr;
        ANGLE_TRY(ReadPixelsFromAttachment(context, framebuffer, framebufferAttachment,
                                           scratchBuffer, &pixelsPtr));
        bos->writeBytes(pixelsPtr->data(), pixelsPtr->size());
        // Reset framebuffer state
        framebuffer->setReadBuffer(prevReadBufferState);
    }
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
    bos->writeBool(framebufferState.getDefaultFixedSampleLocations());
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
    bos->writeBool(rasterizerState.cullFace);
    bos->writeEnum(rasterizerState.cullMode);
    bos->writeInt(rasterizerState.frontFace);
    bos->writeBool(rasterizerState.polygonOffsetFill);
    bos->writeInt(rasterizerState.polygonOffsetFactor);
    bos->writeInt(rasterizerState.polygonOffsetUnits);
    bos->writeBool(rasterizerState.pointDrawMode);
    bos->writeBool(rasterizerState.multiSample);
    bos->writeBool(rasterizerState.rasterizerDiscard);
    bos->writeBool(rasterizerState.dither);
}

void SerializeRectangle(gl::BinaryOutputStream *bos, const gl::Rectangle &rectangle)
{
    bos->writeInt(rectangle.x);
    bos->writeInt(rectangle.y);
    bos->writeInt(rectangle.width);
    bos->writeInt(rectangle.height);
}

void SerializeBlendStateExt(gl::BinaryOutputStream *bos, const gl::BlendStateExt &blendStateExt)
{
    bos->writeInt(blendStateExt.mEnabledMask.bits());
    bos->writeInt(blendStateExt.mDstColor);
    bos->writeInt(blendStateExt.mDstAlpha);
    bos->writeInt(blendStateExt.mSrcColor);
    bos->writeInt(blendStateExt.mSrcAlpha);
    bos->writeInt(blendStateExt.mEquationColor);
    bos->writeInt(blendStateExt.mEquationAlpha);
    bos->writeInt(blendStateExt.mColorMask);
}

void SerializeDepthStencilState(gl::BinaryOutputStream *bos,
                                const gl::DepthStencilState &depthStencilState)
{
    bos->writeBool(depthStencilState.depthTest);
    bos->writeInt(depthStencilState.depthFunc);
    bos->writeBool(depthStencilState.depthMask);
    bos->writeBool(depthStencilState.stencilTest);
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
    bos->writeBool(pixelPackState.reverseRowOrder);
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
    bos->writeBool(state.isScissorTestEnabled());
    SerializeRectangle(bos, state.getScissor());
    SerializeBlendStateExt(bos, state.getBlendStateExt());
    SerializeColor(bos, state.getBlendColor());
    bos->writeBool(state.isSampleAlphaToCoverageEnabled());
    bos->writeBool(state.isSampleCoverageEnabled());
    bos->writeInt(state.getSampleCoverageValue());
    bos->writeBool(state.getSampleCoverageInvert());
    bos->writeBool(state.isSampleMaskEnabled());
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
    bos->writeBool(state.isBindGeneratesResourceEnabled());
    bos->writeBool(state.areClientArraysEnabled());
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
    bos->writeBool(state.isPrimitiveRestartEnabled());
    bos->writeBool(state.isMultisamplingEnabled());
    bos->writeBool(state.isSampleAlphaToOneEnabled());
    bos->writeInt(state.getCoverageModulation());
    bos->writeBool(state.getFramebufferSRGB());
    bos->writeBool(state.isRobustResourceInitEnabled());
    bos->writeBool(state.isProgramBinaryCacheEnabled());
    bos->writeBool(state.isTextureRectangleEnabled());
    bos->writeInt(state.getMaxShaderCompilerThreads());
    bos->writeInt(state.getEnabledClipDistances().to_ulong());
    bos->writeInt(state.getBlendFuncConstantAlphaDrawBuffers().to_ulong());
    bos->writeInt(state.getBlendFuncConstantColorDrawBuffers().to_ulong());
    bos->writeBool(state.noSimultaneousConstantColorAndAlphaBlendFunc());
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

void SerializeSwizzleState(gl::BinaryOutputStream *bos, const gl::SwizzleState &swizzleState)
{
    bos->writeInt(swizzleState.swizzleRed);
    bos->writeInt(swizzleState.swizzleGreen);
    bos->writeInt(swizzleState.swizzleBlue);
    bos->writeInt(swizzleState.swizzleAlpha);
}

void SerializeExtents(gl::BinaryOutputStream *bos, const gl::Extents &extents)
{
    bos->writeInt(extents.width);
    bos->writeInt(extents.height);
    bos->writeInt(extents.depth);
}

void SerializeInternalFormat(gl::BinaryOutputStream *bos, const gl::InternalFormat *internalFormat)
{
    bos->writeInt(internalFormat->internalFormat);
}

void SerializeFormat(gl::BinaryOutputStream *bos, const gl::Format &format)
{
    SerializeInternalFormat(bos, format.info);
}

void SerializeRenderbufferState(gl::BinaryOutputStream *bos,
                                const gl::RenderbufferState &renderbufferState)
{
    bos->writeInt(renderbufferState.getWidth());
    bos->writeInt(renderbufferState.getHeight());
    SerializeFormat(bos, renderbufferState.getFormat());
    bos->writeInt(renderbufferState.getSamples());
    bos->writeEnum(renderbufferState.getInitState());
}

Result SerializeRenderbuffer(const gl::Context *context,
                             gl::BinaryOutputStream *bos,
                             ScratchBuffer *scratchBuffer,
                             gl::Renderbuffer *renderbuffer)
{
    SerializeRenderbufferState(bos, renderbuffer->getState());
    bos->writeString(renderbuffer->getLabel());
    MemoryBuffer *pixelsPtr = nullptr;
    ANGLE_CHECK_GL_ALLOC(
        const_cast<gl::Context *>(context),
        scratchBuffer->getInitialized(renderbuffer->getMemorySize(), &pixelsPtr, 0));
    gl::PixelPackState packState;
    packState.alignment = 1;
    ANGLE_TRY(renderbuffer->getImplementation()->getRenderbufferImage(
        context, packState, nullptr, renderbuffer->getImplementationColorReadFormat(context),
        renderbuffer->getImplementationColorReadType(context), pixelsPtr->data()));
    bos->writeBytes(pixelsPtr->data(), pixelsPtr->size());
    return Result::Continue;
}

void SerializeWorkGroupSize(gl::BinaryOutputStream *bos, const sh::WorkGroupSize &workGroupSize)
{
    bos->writeInt(workGroupSize[0]);
    bos->writeInt(workGroupSize[1]);
    bos->writeInt(workGroupSize[2]);
}

void SerializeShaderVariable(gl::BinaryOutputStream *bos, const sh::ShaderVariable &shaderVariable)
{
    bos->writeInt(shaderVariable.type);
    bos->writeInt(shaderVariable.precision);
    bos->writeString(shaderVariable.name);
    bos->writeString(shaderVariable.mappedName);
    bos->writeIntVector(shaderVariable.arraySizes);
    bos->writeBool(shaderVariable.staticUse);
    bos->writeBool(shaderVariable.active);
    for (const sh::ShaderVariable &field : shaderVariable.fields)
    {
        SerializeShaderVariable(bos, field);
    }
    bos->writeString(shaderVariable.structName);
    bos->writeBool(shaderVariable.isRowMajorLayout);
    bos->writeInt(shaderVariable.location);
    bos->writeInt(shaderVariable.binding);
    bos->writeInt(shaderVariable.imageUnitFormat);
    bos->writeInt(shaderVariable.offset);
    bos->writeBool(shaderVariable.readonly);
    bos->writeBool(shaderVariable.writeonly);
    bos->writeInt(shaderVariable.index);
    bos->writeEnum(shaderVariable.interpolation);
    bos->writeBool(shaderVariable.isInvariant);
    bos->writeBool(shaderVariable.texelFetchStaticUse);
}

void SerializeShaderVariablesVector(gl::BinaryOutputStream *bos,
                                    const std::vector<sh::ShaderVariable> &shaderVariables)
{
    for (const sh::ShaderVariable &shaderVariable : shaderVariables)
    {
        SerializeShaderVariable(bos, shaderVariable);
    }
}

void SerializeInterfaceBlocksVector(gl::BinaryOutputStream *bos,
                                    const std::vector<sh::InterfaceBlock> &interfaceBlocks)
{
    for (const sh::InterfaceBlock &interfaceBlock : interfaceBlocks)
    {
        bos->writeString(interfaceBlock.name);
        bos->writeString(interfaceBlock.mappedName);
        bos->writeString(interfaceBlock.instanceName);
        bos->writeInt(interfaceBlock.arraySize);
        bos->writeEnum(interfaceBlock.layout);
        bos->writeInt(interfaceBlock.binding);
        bos->writeBool(interfaceBlock.staticUse);
        bos->writeBool(interfaceBlock.active);
        bos->writeEnum(interfaceBlock.blockType);
        SerializeShaderVariablesVector(bos, interfaceBlock.fields);
    }
}

void SerializeShaderState(gl::BinaryOutputStream *bos, const gl::ShaderState &shaderState)
{
    bos->writeString(shaderState.getLabel());
    bos->writeEnum(shaderState.getShaderType());
    bos->writeInt(shaderState.getShaderVersion());
    bos->writeString(shaderState.getTranslatedSource());
    bos->writeString(shaderState.getSource());
    SerializeWorkGroupSize(bos, shaderState.getLocalSize());
    SerializeShaderVariablesVector(bos, shaderState.getInputVaryings());
    SerializeShaderVariablesVector(bos, shaderState.getOutputVaryings());
    SerializeShaderVariablesVector(bos, shaderState.getUniforms());
    SerializeInterfaceBlocksVector(bos, shaderState.getUniformBlocks());
    SerializeInterfaceBlocksVector(bos, shaderState.getShaderStorageBlocks());
    SerializeShaderVariablesVector(bos, shaderState.getAllAttributes());
    SerializeShaderVariablesVector(bos, shaderState.getActiveAttributes());
    SerializeShaderVariablesVector(bos, shaderState.getActiveOutputVariables());
    bos->writeBool(shaderState.getEarlyFragmentTestsOptimization());
    bos->writeInt(shaderState.getNumViews());
    if (shaderState.getGeometryShaderInputPrimitiveType().valid())
    {
        bos->writeEnum(shaderState.getGeometryShaderInputPrimitiveType().value());
    }
    if (shaderState.getGeometryShaderOutputPrimitiveType().valid())
    {
        bos->writeEnum(shaderState.getGeometryShaderOutputPrimitiveType().value());
    }
    if (shaderState.getGeometryShaderInvocations().valid())
    {
        bos->writeInt(shaderState.getGeometryShaderInvocations().value());
    }
    bos->writeEnum(shaderState.getCompileStatus());
}

void SerializeShader(gl::BinaryOutputStream *bos, gl::Shader *shader)
{
    SerializeShaderState(bos, shader->getState());
    bos->writeInt(shader->getHandle().value);
    bos->writeInt(shader->getRefCount());
    bos->writeBool(shader->isFlaggedForDeletion());
    // does not serialize mType because it is already serialized in SerializeShaderState
    bos->writeString(shader->getInfoLogString());
    bos->writeString(shader->getCompilerResourcesString());
    bos->writeInt(shader->getCurrentMaxComputeWorkGroupInvocations());
    bos->writeInt(shader->getMaxComputeSharedMemory());
}

void SerializeVariableLocationsVector(gl::BinaryOutputStream *bos,
                                      const std::vector<gl::VariableLocation> &variableLocations)
{
    for (const gl::VariableLocation &variableLocation : variableLocations)
    {
        bos->writeInt(variableLocation.arrayIndex);
        bos->writeInt(variableLocation.index);
        bos->writeBool(variableLocation.ignored);
    }
}

void SerializeBlockMemberInfo(gl::BinaryOutputStream *bos,
                              const sh::BlockMemberInfo &blockMemberInfo)
{
    bos->writeInt(blockMemberInfo.offset);
    bos->writeInt(blockMemberInfo.arrayStride);
    bos->writeInt(blockMemberInfo.matrixStride);
    bos->writeBool(blockMemberInfo.isRowMajorMatrix);
    bos->writeInt(blockMemberInfo.topLevelArrayStride);
}

void SerializeActiveVariable(gl::BinaryOutputStream *bos, const gl::ActiveVariable &activeVariable)
{
    bos->writeInt(activeVariable.activeShaders().to_ulong());
}

void SerializeBufferVariablesVector(gl::BinaryOutputStream *bos,
                                    const std::vector<gl::BufferVariable> &bufferVariables)
{
    for (const gl::BufferVariable &bufferVariable : bufferVariables)
    {
        bos->writeInt(bufferVariable.bufferIndex);
        SerializeBlockMemberInfo(bos, bufferVariable.blockInfo);
        bos->writeInt(bufferVariable.topLevelArraySize);
        SerializeActiveVariable(bos, bufferVariable);
        SerializeShaderVariable(bos, bufferVariable);
    }
}

void SerializeProgramAliasedBindings(gl::BinaryOutputStream *bos,
                                     const gl::ProgramAliasedBindings &programAliasedBindings)
{
    for (const auto &programAliasedBinding : programAliasedBindings)
    {
        bos->writeString(programAliasedBinding.first);
        bos->writeInt(programAliasedBinding.second.location);
        bos->writeBool(programAliasedBinding.second.aliased);
    }
}

void SerializeProgramState(gl::BinaryOutputStream *bos, const gl::ProgramState &programState)
{
    bos->writeString(programState.getLabel());
    SerializeWorkGroupSize(bos, programState.getComputeShaderLocalSize());
    for (gl::Shader *shader : programState.getAttachedShaders())
    {
        if (shader)
        {
            bos->writeInt(shader->getHandle().value);
        }
        else
        {
            bos->writeInt(0);
        }
    }
    for (bool isAttached : programState.getAttachedShadersMarkedForDetach())
    {
        bos->writeBool(isAttached);
    }
    bos->writeInt(programState.getLocationsUsedForXfbExtension());
    for (const std::string &transformFeedbackVaryingName :
         programState.getTransformFeedbackVaryingNames())
    {
        bos->writeString(transformFeedbackVaryingName);
    }
    bos->writeInt(programState.getActiveUniformBlockBindingsMask().to_ulong());
    SerializeVariableLocationsVector(bos, programState.getUniformLocations());
    SerializeBufferVariablesVector(bos, programState.getBufferVariables());
    SerializeRange(bos, programState.getAtomicCounterUniformRange());
    SerializeVariableLocationsVector(bos, programState.getSecondaryOutputLocations());
    bos->writeInt(programState.getActiveOutputVariables().to_ulong());
    for (GLenum outputVariableType : programState.getOutputVariableTypes())
    {
        bos->writeInt(outputVariableType);
    }
    bos->writeInt(programState.getDrawBufferTypeMask().to_ulong());
    bos->writeBool(programState.hasBinaryRetrieveableHint());
    bos->writeBool(programState.isSeparable());
    bos->writeBool(programState.hasEarlyFragmentTestsOptimization());
    bos->writeInt(programState.getNumViews());
    bos->writeEnum(programState.getGeometryShaderInputPrimitiveType());
    bos->writeEnum(programState.getGeometryShaderOutputPrimitiveType());
    bos->writeInt(programState.getGeometryShaderInvocations());
    bos->writeInt(programState.getGeometryShaderMaxVertices());
    bos->writeInt(programState.getDrawIDLocation());
    bos->writeInt(programState.getBaseVertexLocation());
    bos->writeInt(programState.getBaseInstanceLocation());
    SerializeProgramAliasedBindings(bos, programState.getUniformLocationBindings());
}

void SerializeProgramBindings(gl::BinaryOutputStream *bos,
                              const gl::ProgramBindings &programBindings)
{
    for (const auto &programBinding : programBindings)
    {
        bos->writeString(programBinding.first);
        bos->writeInt(programBinding.second);
    }
}

void SerializeProgram(gl::BinaryOutputStream *bos, gl::Program *program)
{
    SerializeProgramState(bos, program->getState());
    bos->writeBool(program->isValidated());
    SerializeProgramBindings(bos, program->getAttributeBindings());
    SerializeProgramAliasedBindings(bos, program->getFragmentOutputLocations());
    SerializeProgramAliasedBindings(bos, program->getFragmentOutputIndexes());
    bos->writeBool(program->isLinked());
    bos->writeBool(program->isFlaggedForDeletion());
    bos->writeInt(program->getRefCount());
    bos->writeInt(program->id().value);
}

void SerializeImageDesc(gl::BinaryOutputStream *bos, const gl::ImageDesc &imageDesc)
{
    SerializeExtents(bos, imageDesc.size);
    SerializeFormat(bos, imageDesc.format);
    bos->writeInt(imageDesc.samples);
    bos->writeBool(imageDesc.fixedSampleLocations);
    bos->writeEnum(imageDesc.initState);
}

void SerializeTextureState(gl::BinaryOutputStream *bos, const gl::TextureState &textureState)
{
    bos->writeEnum(textureState.getType());
    SerializeSwizzleState(bos, textureState.getSwizzleState());
    SerializeSamplerState(bos, textureState.getSamplerState());
    bos->writeEnum(textureState.getSRGBOverride());
    bos->writeInt(textureState.getBaseLevel());
    bos->writeInt(textureState.getMaxLevel());
    bos->writeInt(textureState.getDepthStencilTextureMode());
    bos->writeBool(textureState.hasBeenBoundAsImage());
    bos->writeBool(textureState.getImmutableFormat());
    bos->writeInt(textureState.getImmutableLevels());
    bos->writeInt(textureState.getUsage());
    const std::vector<gl::ImageDesc> &imageDescs = textureState.getImageDescs();
    for (const gl::ImageDesc &imageDesc : imageDescs)
    {
        SerializeImageDesc(bos, imageDesc);
    }
    SerializeRectangle(bos, textureState.getCrop());

    bos->writeInt(textureState.getGenerateMipmapHint());
    bos->writeEnum(textureState.getInitState());
}

Result SerializeTextureData(gl::BinaryOutputStream *bos,
                            const gl::Context *context,
                            gl::Texture *texture,
                            ScratchBuffer *scratchBuffer)
{
    gl::ImageIndexIterator imageIter = gl::ImageIndexIterator::MakeGeneric(
        texture->getType(), 0, texture->getMipmapMaxLevel() + 1, gl::ImageIndex::kEntireLevel,
        gl::ImageIndex::kEntireLevel);
    while (imageIter.hasNext())
    {
        gl::ImageIndex index = imageIter.next();

        const gl::ImageDesc &desc = texture->getTextureState().getImageDesc(index);

        if (desc.size.empty())
            continue;

        const gl::InternalFormat &format = *desc.format.info;

        // Check for supported textures
        ASSERT(index.getType() == gl::TextureType::_2D || index.getType() == gl::TextureType::_3D ||
               index.getType() == gl::TextureType::_2DArray ||
               index.getType() == gl::TextureType::CubeMap);

        GLenum getFormat = format.format;
        GLenum getType   = format.type;

        const gl::Extents size(desc.size.width, desc.size.height, desc.size.depth);
        const gl::PixelUnpackState &unpack = context->getState().getUnpackState();

        GLuint endByte  = 0;
        bool unpackSize = format.computePackUnpackEndByte(getType, size, unpack, true, &endByte);
        ASSERT(unpackSize);
        MemoryBuffer *texelsPtr = nullptr;
        ANGLE_CHECK_GL_ALLOC(const_cast<gl::Context *>(context),
                             scratchBuffer->getInitialized(endByte, &texelsPtr, 0));

        gl::PixelPackState packState;
        packState.alignment = 1;

        ANGLE_TRY(texture->getTexImage(context, packState, nullptr, index.getTarget(),
                                       index.getLevelIndex(), getFormat, getType,
                                       texelsPtr->data()));
        bos->writeBytes(texelsPtr->data(), texelsPtr->size());
    }
    return Result::Continue;
}

Result SerializeTexture(const gl::Context *context,
                        gl::BinaryOutputStream *bos,
                        ScratchBuffer *scratchBuffer,
                        gl::Texture *texture)
{
    SerializeTextureState(bos, texture->getState());
    bos->writeString(texture->getLabel());
    // FrameCapture can not serialize mBoundSurface and mBoundStream
    // because they are likely to change with each run
    ANGLE_TRY(SerializeTextureData(bos, context, texture, scratchBuffer));
    return Result::Continue;
}

void SerializeFormat(gl::BinaryOutputStream *bos, const angle::Format *format)
{
    bos->writeInt(format->glInternalFormat);
}

void SerializeVertexAttributeVector(gl::BinaryOutputStream *bos,
                                    const std::vector<gl::VertexAttribute> &vertexAttributes)
{
    for (const gl::VertexAttribute &vertexAttribute : vertexAttributes)
    {
        bos->writeBool(vertexAttribute.enabled);
        ASSERT(vertexAttribute.format);
        SerializeFormat(bos, vertexAttribute.format);
        bos->writeInt(vertexAttribute.relativeOffset);
        bos->writeInt(vertexAttribute.vertexAttribArrayStride);
        bos->writeInt(vertexAttribute.bindingIndex);
    }
}

void SerializeVertexBindingsVector(gl::BinaryOutputStream *bos,
                                   const std::vector<gl::VertexBinding> &vertexBindings)
{
    for (const gl::VertexBinding &vertexBinding : vertexBindings)
    {
        bos->writeInt(vertexBinding.getStride());
        bos->writeInt(vertexBinding.getDivisor());
        bos->writeInt(vertexBinding.getOffset());
        bos->writeInt(vertexBinding.getBuffer().id().value);
        bos->writeInt(vertexBinding.getBoundAttributesMask().to_ulong());
    }
}

void SerializeVertexArrayState(gl::BinaryOutputStream *bos,
                               const gl::VertexArrayState &vertexArrayState)
{
    bos->writeString(vertexArrayState.getLabel());
    SerializeVertexAttributeVector(bos, vertexArrayState.getVertexAttributes());
    if (vertexArrayState.getElementArrayBuffer())
    {
        bos->writeInt(vertexArrayState.getElementArrayBuffer()->id().value);
    }
    else
    {
        bos->writeInt(0);
    }
    SerializeVertexBindingsVector(bos, vertexArrayState.getVertexBindings());
    bos->writeInt(vertexArrayState.getEnabledAttributesMask().to_ulong());
    bos->writeInt(vertexArrayState.getVertexAttributesTypeMask().to_ulong());
    bos->writeInt(vertexArrayState.getClientMemoryAttribsMask().to_ulong());
    bos->writeInt(vertexArrayState.getNullPointerClientMemoryAttribsMask().to_ulong());
}

void SerializeVertexArray(gl::BinaryOutputStream *bos, gl::VertexArray *vertexArray)
{
    bos->writeInt(vertexArray->id().value);
    SerializeVertexArrayState(bos, vertexArray->getState());
    bos->writeBool(vertexArray->isBufferAccessValidationEnabled());
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
    const gl::RenderbufferManager &renderbufferManager =
        context->getState().getRenderbufferManagerForCapture();
    for (const auto &renderbuffer : renderbufferManager)
    {
        gl::Renderbuffer *renderbufferPtr = renderbuffer.second;
        ANGLE_TRY(SerializeRenderbuffer(context, bos, &scratchBuffer, renderbufferPtr));
    }
    const gl::ShaderProgramManager &shaderProgramManager =
        context->getState().getShaderProgramManagerForCapture();
    const gl::ResourceMap<gl::Shader, gl::ShaderProgramID> &shaderManager =
        shaderProgramManager.getShadersForCapture();
    for (const auto &shader : shaderManager)
    {
        gl::Shader *shaderPtr = shader.second;
        SerializeShader(bos, shaderPtr);
    }
    const gl::ResourceMap<gl::Program, gl::ShaderProgramID> &programManager =
        shaderProgramManager.getProgramsForCaptureAndPerf();
    for (const auto &program : programManager)
    {
        gl::Program *programPtr = program.second;
        SerializeProgram(bos, programPtr);
    }
    const gl::TextureManager &textureManager = context->getState().getTextureManagerForCapture();
    for (const auto &texture : textureManager)
    {
        gl::Texture *texturePtr = texture.second;
        ANGLE_TRY(SerializeTexture(context, bos, &scratchBuffer, texturePtr));
    }
    const gl::VertexArrayMap &vertexArrayMap = context->getVertexArraysForCapture();
    for (auto &vertexArray : vertexArrayMap)
    {
        gl::VertexArray *vertexArrayPtr = vertexArray.second;
        SerializeVertexArray(bos, vertexArrayPtr);
    }

    scratchBuffer.clear();
    return Result::Continue;
}

}  // namespace angle
