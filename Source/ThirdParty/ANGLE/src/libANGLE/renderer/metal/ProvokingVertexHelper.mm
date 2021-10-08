//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProvokingVertexHelper.mm:
//    Implements the class methods for ProvokingVertexHelper.
//


#import <Foundation/Foundation.h>
#include "libANGLE/renderer/metal/ProvokingVertexHelper.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/shaders/rewrite_indices_shared.h"
#include "libANGLE/renderer/metal/mtl_constants.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
namespace rx
{

namespace {
constexpr size_t kInitialIndexBufferSize = 0xFFFF; //Initial 64k pool.
}
static inline uint primCountForIndexCount(const uint fixIndexBufferKey, const uint indexCount)
{
    const uint fixIndexBufferMode = (fixIndexBufferKey >> MtlFixIndexBufferKeyModeShift) & MtlFixIndexBufferKeyModeMask;

    switch(fixIndexBufferMode)
    {
        case MtlFixIndexBufferKeyPoints:
            return indexCount;
        case MtlFixIndexBufferKeyLines:
            return indexCount / 2;
        case MtlFixIndexBufferKeyLineStrip:
            return (uint)MAX(0, (int)indexCount - 1);
        case MtlFixIndexBufferKeyTriangles:
            return indexCount / 3;
        case MtlFixIndexBufferKeyTriangleStrip:
            return (uint)MAX(0, (int)indexCount - 2);
        default:
            ASSERT(false);
            return 0;
    }
}

static inline uint indexCountForPrimCount(const uint fixIndexBufferKey, const uint primCount)
{
    const uint fixIndexBufferMode = (fixIndexBufferKey >> MtlFixIndexBufferKeyModeShift) & MtlFixIndexBufferKeyModeMask;
    switch(fixIndexBufferMode)
    {
        case MtlFixIndexBufferKeyPoints:
            return primCount;
        case MtlFixIndexBufferKeyLines:
            return primCount * 2;
        case MtlFixIndexBufferKeyLineStrip:
            return primCount * 2;
        case MtlFixIndexBufferKeyTriangles:
            return primCount * 3;
        case MtlFixIndexBufferKeyTriangleStrip:
            return primCount * 3;
        default:
            ASSERT(false);
            return 0;
    }
}

static inline gl::PrimitiveMode getNewPrimitiveMode(const uint fixIndexBufferKey)
{
    const uint fixIndexBufferMode = (fixIndexBufferKey >> MtlFixIndexBufferKeyModeShift) & MtlFixIndexBufferKeyModeMask;
    switch(fixIndexBufferMode)
    {
        case MtlFixIndexBufferKeyPoints:
            return gl::PrimitiveMode::Points;
        case MtlFixIndexBufferKeyLines:
            return gl::PrimitiveMode::Lines;
        case MtlFixIndexBufferKeyLineStrip:
            return gl::PrimitiveMode::Lines;
        case MtlFixIndexBufferKeyTriangles:
            return gl::PrimitiveMode::Triangles;
        case MtlFixIndexBufferKeyTriangleStrip:
            return gl::PrimitiveMode::Triangles;
        default:
            ASSERT(false);
            return gl::PrimitiveMode::InvalidEnum;
    }
}
ProvokingVertexHelper::ProvokingVertexHelper(ContextMtl *  context, mtl::CommandQueue * commandQueue, DisplayMtl *display):
    mCommandBuffer(commandQueue),
    mCurrentEncoder(&mCommandBuffer),
    mPipelineCache(this),
    mIndexBuffers(true)
    
{
    id<MTLLibrary> mtlLib = display->getDefaultShadersLib();
    mProvokingVertexLibrary = mtlLib;
    mIndexBuffers.initialize(context, kInitialIndexBufferSize, mtl::kIndexBufferOffsetAlignment, 0);
    ensureCommandBufferReady();

}

void ProvokingVertexHelper::onDestroy(ContextMtl * context)
{
    mIndexBuffers.destroy(context);
    mPipelineCache.clear();
}

void ProvokingVertexHelper::commitPreconditionCommandBuffer(ContextMtl * contextMtl)
{
    if(mCurrentEncoder.valid())
    {
        mCurrentEncoder.endEncoding();
    }
    if(mCommandBuffer.valid())
    {
        mCommandBuffer.commit();
    }
    
    mIndexBuffers.releaseInFlightBuffers(contextMtl);
    
}

mtl::ComputeCommandEncoder *ProvokingVertexHelper::getComputeCommandEncoder()
{
    if (mCurrentEncoder.valid())
    {
        return &mCurrentEncoder;
    }

    ensureCommandBufferReady();
    return &mCurrentEncoder.restart();
}



void ProvokingVertexHelper::ensureCommandBufferReady()
{
    if (!mCommandBuffer.valid())
    {
        mCommandBuffer.restart();
    }
    ASSERT(mCommandBuffer.valid());
}

static uint buildIndexBufferKey(const mtl::ProvokingVertexComputePipelineDesc &pipelineDesc)
{
    uint indexBufferKey = 0;
    gl::DrawElementsType elementType  = (gl::DrawElementsType)pipelineDesc.elementType;
    bool doPrimPrestart = pipelineDesc.primitiveRestartEnabled;
    gl::PrimitiveMode primMode = pipelineDesc.primitiveMode;
    switch(elementType)
    {
        case gl::DrawElementsType::UnsignedShort:
            indexBufferKey |= MtlFixIndexBufferKeyUint16 << MtlFixIndexBufferKeyInShift;
            indexBufferKey |= MtlFixIndexBufferKeyUint16 << MtlFixIndexBufferKeyOutShift;
            break;
        case gl::DrawElementsType::UnsignedInt:
            indexBufferKey |= MtlFixIndexBufferKeyUint32 << MtlFixIndexBufferKeyInShift;
            indexBufferKey |= MtlFixIndexBufferKeyUint32 << MtlFixIndexBufferKeyOutShift;
            break;
        default:
            ASSERT(!"Index type should only be short or int.");
            break;
    }
    indexBufferKey |= (uint)primMode << MtlFixIndexBufferKeyModeShift;
    indexBufferKey |= doPrimPrestart ? MtlFixIndexBufferKeyPrimRestart : 0;
    //We only rewrite indices if we're switching the provoking vertex mode.
    indexBufferKey |= MtlFixIndexBufferKeyProvokingVertexLast;
    return indexBufferKey;
}

angle::Result ProvokingVertexHelper::getSpecializedShader(rx::mtl::Context *context,
                                           gl::ShaderType shaderType,
                                           const mtl::ProvokingVertexComputePipelineDesc &pipelineDesc,
                                           id<MTLFunction> *shaderOut)
{
    uint indexBufferKey = buildIndexBufferKey(pipelineDesc);
    auto fcValues = mtl::adoptObjCObj([[MTLFunctionConstantValues alloc] init]);
    [fcValues setConstantValue:&indexBufferKey type:MTLDataTypeUInt withName:@"fixIndexBufferKey"];
    
    return CreateMslShader(context, mProvokingVertexLibrary, @"fixIndexBuffer", fcValues.get(), shaderOut);
}

//Private command buffer
bool ProvokingVertexHelper::hasSpecializedShader(gl::ShaderType shaderType,
                                  const mtl::ProvokingVertexComputePipelineDesc &renderPipelineDesc)
{
    return true;
}

void ProvokingVertexHelper::prepareCommandEncoderForDescriptor(ContextMtl * context, mtl::ComputeCommandEncoder * encoder, mtl::ProvokingVertexComputePipelineDesc desc)
{
    auto pipelineState = mPipelineCache.getComputePipelineState(context, desc);
    
    encoder->setComputePipelineState(pipelineState);
    mCachedDesc = desc;
}
mtl::BufferRef  ProvokingVertexHelper::preconditionIndexBuffer(ContextMtl * context,
                                                               mtl::BufferRef indexBuffer,
                                                               size_t indexCount,
                                                               size_t indexOffset,
                                                               bool primitiveRestartEnabled,
                                                               gl::PrimitiveMode primitiveMode,
                                                               gl::DrawElementsType elementsType,
                                                               size_t & outIndexcount,
                                                               gl::PrimitiveMode & outPrimitiveMode)
{
    // Get specialized program
    // Upload index buffer
    // dispatch per-primitive?
    mtl::ProvokingVertexComputePipelineDesc pipelineDesc;
    pipelineDesc.elementType = (uint8_t)elementsType;
    pipelineDesc.primitiveMode = primitiveMode;
    pipelineDesc.primitiveRestartEnabled = primitiveRestartEnabled;
    uint indexBufferKey = buildIndexBufferKey(pipelineDesc);
    uint primCount = primCountForIndexCount(indexBufferKey, (uint32_t)indexCount);
    uint newIndexCount = indexCountForPrimCount(indexBufferKey, primCount);
    size_t indexSize = gl::GetDrawElementsTypeSize(elementsType);
    mtl::BufferRef newBuffer;
    // To simplify draw loop code, we allocate space for the offset. This could be optimized to reduce
    // memory, if needed.
    if(mIndexBuffers.allocate(context, indexOffset + newIndexCount * indexSize, nullptr, &newBuffer) == angle::Result::Stop)
    {
        return nullptr;
    }
    uint indexCountEncoded = (uint)indexCount;
    auto threadsPerThreadgroup = MTLSizeMake(MIN(primCount, 64u), 1, 1);
    
    
    mtl::ComputeCommandEncoder * encoder = getComputeCommandEncoder();
    prepareCommandEncoderForDescriptor(context, encoder, pipelineDesc);
    encoder->setBuffer(indexBuffer, (uint32_t)indexOffset, 0);
    encoder->setBufferForWrite(newBuffer,(uint32_t) indexOffset, 1);
    encoder->setData(&indexCountEncoded, 2);
    encoder->setData(&primCount, 3);
    encoder->dispatch(MTLSizeMake((primCount + threadsPerThreadgroup.width - 1)/ threadsPerThreadgroup.width, 1, 1), threadsPerThreadgroup);
    outIndexcount = newIndexCount;
    outPrimitiveMode = getNewPrimitiveMode(indexBufferKey);
    return newBuffer;
}
} // namespace rx
