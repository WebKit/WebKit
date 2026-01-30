//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProvokingVertexHelper.mm:
//    Implements the class methods for ProvokingVertexHelper.
//

#include "libANGLE/renderer/metal/ProvokingVertexHelper.h"
#import <Foundation/Foundation.h>
#include "libANGLE/Display.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/shaders/rewrite_indices_shared.h"
namespace rx
{

namespace
{
constexpr size_t kInitialIndexBufferSize = 0xFFFF;  // Initial 64k pool.

struct IndexedDrawRewriteInfoMtl {
    uint32_t primitiveCount;
    size_t newIndexCount;
    gl::PrimitiveMode newPrimitiveMode;
};

// Returns counts for Metal indexed draw for GL indexed draw parameters.
inline IndexedDrawRewriteInfoMtl resolveIndexedDrawRewriteInfo(gl::PrimitiveMode mode, GLsizei count)
{
    uint32_t indexCount = static_cast<uint32_t>(count);
    switch (mode)
    {
        case gl::PrimitiveMode::Points:
            return {indexCount, indexCount, gl::PrimitiveMode::Points};
        case gl::PrimitiveMode::Lines:
            return {indexCount / 2, indexCount - (indexCount % 2), gl::PrimitiveMode::Lines};
        case gl::PrimitiveMode::LineLoop:
            return {indexCount, static_cast<size_t>(indexCount) * 2, gl::PrimitiveMode::Lines};
        case gl::PrimitiveMode::LineStrip:
            indexCount = indexCount < 1 ? 0 : indexCount - 1;
            return {indexCount, static_cast<size_t>(indexCount) * 2, gl::PrimitiveMode::Lines};
        case gl::PrimitiveMode::Triangles:
            return {indexCount / 3, indexCount - (indexCount % 3), gl::PrimitiveMode::Triangles};
        case gl::PrimitiveMode::TriangleStrip:
        case gl::PrimitiveMode::TriangleFan:
            indexCount = indexCount < 2 ? 0 : indexCount - 2;
            return {indexCount, static_cast<size_t>(indexCount) * 3, gl::PrimitiveMode::Triangles};
        default:
            UNREACHABLE();
            return {0, 0, gl::PrimitiveMode::InvalidEnum};
    }
}

}

ProvokingVertexHelper::ProvokingVertexHelper(ContextMtl *context) : mIndexBuffers(false)
{
    mIndexBuffers.initialize(context, kInitialIndexBufferSize, mtl::kIndexBufferOffsetAlignment, 0);
}

void ProvokingVertexHelper::onDestroy(ContextMtl *context)
{
    mIndexBuffers.destroy(context);
}

void ProvokingVertexHelper::releaseInFlightBuffers(ContextMtl *contextMtl)
{
    mIndexBuffers.releaseInFlightBuffers(contextMtl);
}

static uint buildIndexBufferKey(const mtl::ProvokingVertexComputePipelineDesc &pipelineDesc)
{
    uint indexBufferKey              = 0;
    gl::DrawElementsType elementType = (gl::DrawElementsType)pipelineDesc.elementType;
    bool doPrimPrestart              = pipelineDesc.primitiveRestartEnabled;
    gl::PrimitiveMode primMode       = pipelineDesc.primitiveMode;
    switch (elementType)
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
            ASSERT(false);  // Index type should only be short or int.
            break;
    }
    indexBufferKey |= (uint)primMode << MtlFixIndexBufferKeyModeShift;
    indexBufferKey |= doPrimPrestart ? MtlFixIndexBufferKeyPrimRestart : 0;
    // We only rewrite indices if we're switching the provoking vertex mode.
    indexBufferKey |= MtlFixIndexBufferKeyProvokingVertexLast;
    return indexBufferKey;
}

angle::Result ProvokingVertexHelper::getComputePipleineState(
    ContextMtl *context,
    const mtl::ProvokingVertexComputePipelineDesc &desc,
    angle::ObjCPtr<id<MTLComputePipelineState>> *outComputePipeline)
{
    auto iter = mComputeFunctions.find(desc);
    if (iter != mComputeFunctions.end())
    {
        return context->getPipelineCache().getComputePipeline(context, iter->second,
                                                              outComputePipeline);
    }

    id<MTLLibrary> provokingVertexLibrary = context->getDisplay()->getDefaultShadersLib();
    uint indexBufferKey                   = buildIndexBufferKey(desc);
    auto fcValues = angle::adoptObjCPtr([[MTLFunctionConstantValues alloc] init]);
    [fcValues setConstantValue:&indexBufferKey type:MTLDataTypeUInt withName:@"fixIndexBufferKey"];

    angle::ObjCPtr<id<MTLFunction>> computeShader;
    if (desc.generateIndices)
    {
        ANGLE_TRY(CreateMslShader(context, provokingVertexLibrary, @"genIndexBuffer",
                                  fcValues.get(), &computeShader));
    }
    else
    {
        ANGLE_TRY(CreateMslShader(context, provokingVertexLibrary, @"fixIndexBuffer",
                                  fcValues.get(), &computeShader));
    }
    mComputeFunctions[desc] = computeShader;

    return context->getPipelineCache().getComputePipeline(context, computeShader,
                                                          outComputePipeline);
}

angle::Result ProvokingVertexHelper::prepareCommandEncoderForDescriptor(
    ContextMtl *context,
    mtl::ComputeCommandEncoder *encoder,
    mtl::ProvokingVertexComputePipelineDesc desc)
{
    angle::ObjCPtr<id<MTLComputePipelineState>> pipelineState;
    ANGLE_TRY(getComputePipleineState(context, desc, &pipelineState));

    encoder->setComputePipelineState(pipelineState);

    return angle::Result::Continue;
}

angle::Result ProvokingVertexHelper::preconditionIndexBuffer(ContextMtl *context,
                                                             mtl::BufferRef indexBuffer,
                                                             GLsizei count,
                                                             size_t indexOffset,
                                                             bool primitiveRestartEnabled,
                                                             gl::PrimitiveMode primitiveMode,
                                                             gl::DrawElementsType elementsType,
                                                             uint32_t &outIndexCount,
                                                             size_t &outIndexOffset,
                                                             gl::PrimitiveMode &outPrimitiveMode,
                                                             mtl::BufferRef &outNewBuffer)
{
    // Get specialized program
    // Upload index buffer
    // dispatch per-primitive?
    mtl::ProvokingVertexComputePipelineDesc pipelineDesc;
    pipelineDesc.elementType             = (uint8_t)elementsType;
    pipelineDesc.primitiveMode           = primitiveMode;
    pipelineDesc.primitiveRestartEnabled = primitiveRestartEnabled;
    pipelineDesc.generateIndices         = false;

    auto [primCount, newIndexCount, newPrimitiveMode] = resolveIndexedDrawRewriteInfo(primitiveMode, count);
    // We do not support large buffers at the moment.
    ANGLE_CHECK_GL_MATH(context, newIndexCount <= std::numeric_limits<uint32_t>::max());
    size_t newIndexBufferSize = newIndexCount << gl::GetDrawElementsTypeShift(elementsType);
    size_t newOffset          = 0;
    mtl::BufferRef newBuffer;
    ANGLE_TRY(mIndexBuffers.allocate(context, newIndexBufferSize + indexOffset, nullptr,
                                     &newBuffer, &newOffset));
    auto threadsPerThreadgroup = MTLSizeMake(MIN(primCount, 64u), 1, 1);

    mtl::ComputeCommandEncoder *encoder =
        context->getComputeCommandEncoderWithoutEndingRenderEncoder();
    ANGLE_TRY(prepareCommandEncoderForDescriptor(context, encoder, pipelineDesc));
    encoder->setBuffer(indexBuffer, indexOffset, 0);
    encoder->setBufferForWrite(newBuffer, indexOffset + newOffset, 1);
    encoder->setData(static_cast<uint>(count), 2);
    encoder->setData(primCount, 3);
    encoder->dispatch(
        MTLSizeMake((static_cast<NSUInteger>(primCount) + threadsPerThreadgroup.width - 1) / threadsPerThreadgroup.width, 1,
                    1),
        threadsPerThreadgroup);
    outIndexCount    = static_cast<uint32_t>(newIndexCount);
    outIndexOffset   = newOffset;
    outPrimitiveMode = newPrimitiveMode;
    outNewBuffer     = newBuffer;
    return angle::Result::Continue;
}

angle::Result ProvokingVertexHelper::generateIndexBuffer(ContextMtl *context,
                                                         GLsizei first,
                                                         GLsizei count,
                                                         gl::PrimitiveMode primitiveMode,
                                                         gl::DrawElementsType elementsType,
                                                         uint32_t &outIndexCount,
                                                         size_t &outIndexOffset,
                                                         gl::PrimitiveMode &outPrimitiveMode,
                                                         mtl::BufferRef &outNewBuffer)
{
    // Get specialized program
    // Upload index buffer
    // dispatch per-primitive?
    mtl::ProvokingVertexComputePipelineDesc pipelineDesc;
    pipelineDesc.elementType             = (uint8_t)elementsType;
    pipelineDesc.primitiveMode           = primitiveMode;
    pipelineDesc.primitiveRestartEnabled = false;
    pipelineDesc.generateIndices         = true;
    auto [primCount, newIndexCount, newPrimitiveMode] = resolveIndexedDrawRewriteInfo(primitiveMode, count);
    // We do not support large buffers at the moment.
    ANGLE_CHECK_GL_MATH(context, newIndexCount <= std::numeric_limits<uint32_t>::max());
    size_t newIndexBufferSize = newIndexCount << gl::GetDrawElementsTypeShift(elementsType);
    size_t newIndexOffset = 0;
    mtl::BufferRef newBuffer;
    ANGLE_TRY(mIndexBuffers.allocate(context, newIndexBufferSize, nullptr, &newBuffer,
                                     &newIndexOffset));
    auto threadsPerThreadgroup = MTLSizeMake(MIN(primCount, 64u), 1, 1);

    mtl::ComputeCommandEncoder *encoder =
        context->getComputeCommandEncoderWithoutEndingRenderEncoder();
    ANGLE_TRY(prepareCommandEncoderForDescriptor(context, encoder, pipelineDesc));
    encoder->setBufferForWrite(newBuffer, newIndexOffset, 1);
    encoder->setData(static_cast<uint>(count), 2);
    encoder->setData(primCount, 3);
    encoder->setData(static_cast<uint>(first), 4);
    encoder->dispatch(
        MTLSizeMake((static_cast<NSUInteger>(primCount) + threadsPerThreadgroup.width - 1) / threadsPerThreadgroup.width, 1,
                    1),
        threadsPerThreadgroup);
    outIndexCount    = static_cast<uint32_t>(newIndexCount);
    outIndexOffset   = newIndexOffset;
    outPrimitiveMode = newPrimitiveMode;
    outNewBuffer     = newBuffer;
    return angle::Result::Continue;
}

}  // namespace rx
