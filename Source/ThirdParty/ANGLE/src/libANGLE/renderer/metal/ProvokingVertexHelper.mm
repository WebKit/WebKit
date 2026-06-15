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
#include "common/base/anglebase/numerics/checked_math.h"
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
    uint32_t perPrimitiveIndexCount;
};

// Returns counts for Metal indexed draw for GL indexed draw parameters.
inline IndexedDrawRewriteInfoMtl resolveIndexedDrawRewriteInfo(gl::PrimitiveMode mode,
                                                               GLsizei count,
                                                               bool primitiveRestartEnabled)
{
    uint32_t indexCount = static_cast<uint32_t>(count);
    switch (mode)
    {
        case gl::PrimitiveMode::Lines:
            indexCount -= primitiveRestartEnabled ? 0 : (indexCount % 2);
            return {indexCount / 2, indexCount, gl::PrimitiveMode::Lines, 2};
        case gl::PrimitiveMode::LineLoop:
            return {indexCount, static_cast<size_t>(indexCount) * 2, gl::PrimitiveMode::Lines, 2};
        case gl::PrimitiveMode::LineStrip:
            indexCount = indexCount < 1 ? 0 : indexCount - 1;
            return {indexCount, static_cast<size_t>(indexCount) * 2, gl::PrimitiveMode::Lines, 2};
        case gl::PrimitiveMode::Triangles:
            indexCount -= primitiveRestartEnabled ? 0 : (indexCount % 3);
            return {indexCount / 3, indexCount, gl::PrimitiveMode::Triangles, 3};
        case gl::PrimitiveMode::TriangleStrip:
        case gl::PrimitiveMode::TriangleFan:
            indexCount = indexCount < 2 ? 0 : indexCount - 2;
            return {indexCount, static_cast<size_t>(indexCount) * 3, gl::PrimitiveMode::Triangles,
                    3};
        default:
            UNREACHABLE();
            return {0, 0, gl::PrimitiveMode::InvalidEnum, 0};
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

static uint buildIndexBufferKey(gl::DrawElementsType elementType,
                                bool doPrimPrestart,
                                gl::PrimitiveMode primMode)
{
    uint indexBufferKey              = 0;
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

angle::Result ProvokingVertexHelper::prepareCommandEncoderForFunction(
    ContextMtl *context,
    mtl::ComputeCommandEncoder *encoder,
    uint32_t indexBufferKey,
    bool isForGenerateIndices)
{
    auto &functionMap = isForGenerateIndices ? mGenIndexBufferFunctions : mFixIndexBufferFunctions;
    NSString *functionName = isForGenerateIndices ? @"genIndexBuffer" : @"fixIndexBuffer";

    angle::ObjCPtr<id<MTLFunction>> computeShader;
    auto iter = functionMap.find(indexBufferKey);
    if (iter != functionMap.end())
    {
        computeShader = iter->second;
    }
    else
    {
        id<MTLLibrary> provokingVertexLibrary = context->getDisplay()->getDefaultShadersLib();
        auto fcValues = angle::adoptObjCPtr([[MTLFunctionConstantValues alloc] init]);
        [fcValues setConstantValue:&indexBufferKey
                              type:MTLDataTypeUInt
                          withName:@"fixIndexBufferKey"];

        ANGLE_TRY(CreateMslShader(context, provokingVertexLibrary, functionName, fcValues.get(),
                                  &computeShader));
        functionMap[indexBufferKey] = computeShader;
    }
    angle::ObjCPtr<id<MTLComputePipelineState>> pipelineState;
    ANGLE_TRY(
        context->getPipelineCache().getComputePipeline(context, computeShader, &pipelineState));
    encoder->setComputePipelineState(pipelineState);
    return angle::Result::Continue;
}

angle::Result ProvokingVertexHelper::preconditionIndexBuffer(
    ContextMtl *context,
    GLsizei count,
    gl::PrimitiveMode mode,
    size_t firstIndex,
    bool primitiveRestartEnabled,
    const std::vector<DrawIndexRange> &drawIndexRanges,
    mtl::BufferRef indexBuffer,
    size_t indexBufferOffset,
    gl::DrawElementsType indexBufferType,
    mtl::BufferRef *outNewIndexBuffer,
    size_t *outNewIndexBufferOffset)
{
    // Get specialized program
    // Upload index buffer
    // dispatch per-primitive?
    uint indexBufferKey = buildIndexBufferKey(indexBufferType, primitiveRestartEnabled, mode);
    auto [primCount, newIndexCount, newMode, perPrimitiveIndexCount] =
        resolveIndexedDrawRewriteInfo(mode, count, primitiveRestartEnabled);
    // We do not support large buffers at the moment.
    ANGLE_CHECK_GL_MATH(context, newIndexCount <= std::numeric_limits<uint32_t>::max());
    const size_t indexTypeShift = gl::GetDrawElementsTypeShift(indexBufferType);
    size_t firstIndexOffset     = firstIndex << indexTypeShift;
    size_t newFirstIndexOffset  = firstIndexOffset;
    if (mode != newMode)
    {
        newFirstIndexOffset *= perPrimitiveIndexCount;
    }
    size_t newIndexBufferSize = newFirstIndexOffset + (newIndexCount << indexTypeShift);
    mtl::BufferSlice newIndexBufferSlice;
    ANGLE_TRY(mIndexBuffers.allocate(context, newIndexBufferSize, &newIndexBufferSlice));
    mtl::BufferRef newIndexBuffer = newIndexBufferSlice.buffer();
    size_t newIndexBufferOffset   = newIndexBufferSlice.offset();

    mtl::ComputeCommandEncoder *encoder =
        context->getComputeCommandEncoderWithoutEndingRenderEncoder();
    const bool isForGenerateIndices = false;
    ANGLE_TRY(
        prepareCommandEncoderForFunction(context, encoder, indexBufferKey, isForGenerateIndices));

    const size_t lastIndex = firstIndex + count - 1;
    for (const auto &range : drawIndexRanges)
    {
        if (range.end < firstIndex)
        {
            continue;
        }
        if (range.begin > lastIndex)
        {
            break;
        }
        DrawIndexRange clippedRange{std::max(range.begin, firstIndex),
                                    std::min(range.end, lastIndex)};
        uint32_t indexCount = static_cast<uint32_t>(clippedRange.end - clippedRange.begin + 1);
        if (indexCount < perPrimitiveIndexCount)
        {
            continue;
        }
        size_t beginOffset = clippedRange.begin << indexTypeShift;
        uint32_t primitiveCount;
        size_t newBeginOffset;
        if (mode == newMode)
        {
            primitiveCount = indexCount / perPrimitiveIndexCount;
            newBeginOffset = clippedRange.begin << indexTypeShift;
        }
        else
        {
            // Expanded modes: `N` source indices produce `(N - perPrimitiveIndexCount + 1)`
            // primitives.
            primitiveCount = indexCount - perPrimitiveIndexCount + 1;
            newBeginOffset = (clippedRange.begin << indexTypeShift) * perPrimitiveIndexCount;
        }

        auto threadsPerThreadgroup = MTLSizeMake(MIN(primitiveCount, 64u), 1, 1);
        encoder->setBuffer(indexBuffer, indexBufferOffset + beginOffset, 0);
        encoder->setBufferForWrite(newIndexBuffer, newIndexBufferOffset + newBeginOffset, 1);
        encoder->setData(indexCount, 2);
        encoder->setData(primitiveCount, 3);
        encoder->dispatch(MTLSizeMake((static_cast<NSUInteger>(primitiveCount) +
                                       threadsPerThreadgroup.width - 1) /
                                          threadsPerThreadgroup.width,
                                      1, 1),
                          threadsPerThreadgroup);
    }

    *outNewIndexBuffer       = newIndexBuffer;
    *outNewIndexBufferOffset = newIndexBufferOffset;
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
    const bool primitiveRestartEnabled = false;
    uint indexBufferKey = buildIndexBufferKey(elementsType, primitiveRestartEnabled, primitiveMode);
    auto [primCount, newIndexCount, newPrimitiveMode, perPrimitiveIndexCount] =
        resolveIndexedDrawRewriteInfo(primitiveMode, count, primitiveRestartEnabled);
    // We do not support large buffers at the moment.
    ANGLE_CHECK_GL_MATH(context, newIndexCount <= std::numeric_limits<uint32_t>::max());
    size_t newIndexBufferSize = newIndexCount << gl::GetDrawElementsTypeShift(elementsType);
    mtl::BufferSlice newBufferSlice;
    ANGLE_TRY(mIndexBuffers.allocate(context, newIndexBufferSize, &newBufferSlice));
    mtl::BufferRef newBuffer = newBufferSlice.buffer();
    size_t newIndexOffset    = newBufferSlice.offset();
    auto threadsPerThreadgroup = MTLSizeMake(MIN(primCount, 64u), 1, 1);

    mtl::ComputeCommandEncoder *encoder =
        context->getComputeCommandEncoderWithoutEndingRenderEncoder();
    const bool isForGenerateIndices = true;
    ANGLE_TRY(
        prepareCommandEncoderForFunction(context, encoder, indexBufferKey, isForGenerateIndices));
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
