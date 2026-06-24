// Copyright (C) 2025 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import Metal
import OSLog
import WebKit
import simd

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreDeformation, _version: 23.0.2) && canImport(ShaderGraph, _version: 159.0.3) && arch(arm64)
import USDKit
import DirectResource
import RealityKit

// Safe wrapper of unsafe functions
extension MTLBuffer {
    func copyMemory<T>(fromSpan: Span<T>, byteOffset: Int) where T: BitwiseCopyable {
        unsafe copyMemory(from: fromSpan.bytes, byteOffset: byteOffset)
    }

    func copyMemory(from span: RawSpan, byteOffset: Int) {
        precondition(0 <= byteOffset, "Byte offset must not be negative")
        precondition(byteOffset + span.byteCount <= self.length, "Span range exceeds buffer size")
        // Preconditions guarantee that input span does not exceed the range of the MTLBuffer.
        unsafe span.withUnsafeBytes { sourceBuffer in
            guard let baseAddress = sourceBuffer.baseAddress else {
                fatalError("Unable to get the base address of the buffer")
            }
            unsafe self.contents().advanced(by: byteOffset).copyMemory(from: baseAddress, byteCount: sourceBuffer.count)
        }
    }
}

extension MTLDevice {
    func makeBuffer<T>(
        fromSpan: Span<T>,
        length: Int,
        memoryOwner: task_id_token_t,
        options: MTLResourceOptions = []
    ) -> any MTLBuffer where T: BitwiseCopyable {
        let buffer = makeBuffer(length: length, memoryOwner: memoryOwner, options: options)
        buffer.copyMemory(fromSpan: fromSpan, byteOffset: 0)
        return buffer
    }

    func makeBuffer(
        length: Int,
        memoryOwner: task_id_token_t,
        options: MTLResourceOptions = []
    ) -> any MTLBuffer {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let buffer = makeBuffer(length: length, options: options)!
        buffer.__setOwnerWithIdentity(memoryOwner)
        return buffer
    }
}

// Copies the prefix of `data` (up to `span.byteCount` bytes) into `span`.
// Modeled on `copyDataIntoBuffer` from ModelUtils.swift; the `@_lifetime`
// annotation is required because `MutableRawSpan` is `~Escapable` and the
// compiler otherwise rejects the inout binding when nested inside the
// `replaceInfluenceJointIndices`/`replace*Indices`/`replaceAdjacencies` etc.
// closures used by the v2 deformation API.
@_lifetime(span: copy span)
private func writeData(into span: inout MutableRawSpan, from data: Data) {
    let copyCount = min(data.count, span.byteCount)
    unsafe span.withUnsafeMutableBytes { unsafe $0.copyBytes(from: data.prefix(copyCount)) }
}

// V2 deformation context. Holds the compiled pipeline, the live LowLevelDeformation
// instance, and the buffers we own (for residency tracking and in-place updates
// when only joint transforms / blend weights / etc. change between frames).
struct DeformationContext {
    let pipeline: LowLevelDeformation.Pipeline
    let deformation: LowLevelDeformation
    var dirty: Bool

    // All MTLBuffers backing this deformation, retained so the residency set can
    // pin them and so they outlive the GPU's read of them.
    var mtlBuffers: [any MTLBuffer]

    // Skinning per-stage buffers, kept so we can rewrite joint transforms /
    // inverse bind poses / influence weights in place between frames.
    var jointTransformsBuffer: (any MTLBuffer)?
    var inverseBindPosesBuffer: (any MTLBuffer)?
    var influenceWeightsBuffer: (any MTLBuffer)?

    // Blend-shape per-stage buffers.
    var blendShapeWeightsBuffer: (any MTLBuffer)?
    var blendShapePositionOffsetsBuffer: (any MTLBuffer)?

    // Records the "shape" of the deformation: vertex count, and whether each
    // optional stage is configured. Used to detect when an in-place rebind
    // (cheap) suffices vs. when we need to rebuild the pipeline (structural
    // change, e.g. blend target count).
    let vertexCount: Int
    let blendingTargetCount: Int
    let skinningJointTransformCount: Int
    let skinningInfluencesPerVertex: Int
    let renormalizingAdjacenciesCount: Int
    let renormalizingIndexCount: Int
}

func configureDeformation(
    identifier: WKBridgeTypedResourceId,
    deformationData: WKBridgeDeformationData,
    commandBuffer: any MTLCommandBuffer,
    device: any MTLDevice,
    meshResource: LowLevelMeshResource,
    meshResourceToDeformationContext: inout [WKBridgeTypedResourceId: DeformationContext],
    deformationContext deformationContextRoot: LowLevelDeformationContext,
    memoryOwner: task_id_token_t
) {
    // Try in-place data update if the existing context's shape matches the new data's.
    if var existing = meshResourceToDeformationContext[identifier],
        contextShapeMatches(existing, deformationData: deformationData, meshResource: meshResource)
    {
        updateDeformationDataInPlace(deformationData: deformationData, context: &existing)
        existing.dirty = true
        meshResourceToDeformationContext[identifier] = existing
        return
    }

    // Otherwise, build a fresh context from scratch.
    do {
        let context = try buildDeformationContext(
            meshResource: meshResource,
            deformationData: deformationData,
            commandBuffer: commandBuffer,
            device: device,
            deformationContextRoot: deformationContextRoot,
            memoryOwner: memoryOwner
        )
        meshResourceToDeformationContext[identifier] = context
    } catch {
        // A throw here means the deformation pipeline can't be configured —
        // the mesh will render unskinned at its bind pose, which is almost
        // always visually wrong.
        // Assert in debug so it can't go unnoticed during development; in
        // release we log and continue rather than crashing the GPU process.
        logError("Failed to configure deformation for \(identifier): \(error)")
        assertionFailure("Failed to configure deformation for \(identifier): \(error)")
    }
}

private func contextShapeMatches(
    _ context: DeformationContext,
    deformationData: WKBridgeDeformationData,
    meshResource: LowLevelMeshResource
) -> Bool {
    let vertexCount = meshResource.descriptor.vertexCapacity
    if context.vertexCount != vertexCount { return false }

    if let blend = deformationData.blendShapeData {
        let targetCount = max(1, blend.weights.count)
        if context.blendingTargetCount != targetCount { return false }
    } else {
        if context.blendingTargetCount != 0 { return false }
    }

    if let skinning = deformationData.skinningData {
        let jointCount = max(1, skinning.jointTransforms.count)
        let influencesPerVertex = Int(skinning.influencePerVertexCount)
        if context.skinningJointTransformCount != jointCount { return false }
        if context.skinningInfluencesPerVertex != influencesPerVertex { return false }
    } else {
        if context.skinningJointTransformCount != 0 { return false }
    }

    if let renorm = deformationData.renormalizationData {
        let adjacenciesCount = max(1, renorm.vertexAdjacencies.count)
        let indexCount = max(3, renorm.vertexIndicesPerTriangle.count)
        if context.renormalizingAdjacenciesCount != adjacenciesCount { return false }
        if context.renormalizingIndexCount != indexCount { return false }
    } else {
        if context.renormalizingAdjacenciesCount != 0 { return false }
    }

    return true
}

private func updateDeformationDataInPlace(
    deformationData: WKBridgeDeformationData,
    context: inout DeformationContext
) {
    if let skinningData = deformationData.skinningData {
        if !skinningData.jointTransforms.isEmpty, let buffer = context.jointTransformsBuffer {
            buffer.copyMemory(fromSpan: skinningData.jointTransforms.span, byteOffset: 0)
        }

        if !skinningData.inverseBindPoses.isEmpty, let buffer = context.inverseBindPosesBuffer {
            buffer.copyMemory(fromSpan: skinningData.inverseBindPoses.span, byteOffset: 0)
        }

        context.deformation.skinning.geometryBindTransform = skinningData.geometryBindTransform

        if !skinningData.influenceWeights.isEmpty, let buffer = context.influenceWeightsBuffer {
            buffer.copyMemory(fromSpan: skinningData.influenceWeights.span, byteOffset: 0)
        }

        if !skinningData.influenceJointIndices.isEmpty {
            let indicesData = toData(skinningData.influenceJointIndices)
            try? context.deformation.skinning.replaceInfluenceJointIndices { span in
                writeData(into: &span, from: indicesData)
            }
        }
    }
    if let blendShapeData = deformationData.blendShapeData {
        if !blendShapeData.weights.isEmpty, let buffer = context.blendShapeWeightsBuffer {
            buffer.copyMemory(fromSpan: blendShapeData.weights.span, byteOffset: 0)
        }

        let positions = blendShapeData.positionOffsets.flatMap(\.self)
        if !positions.isEmpty, let buffer = context.blendShapePositionOffsetsBuffer {
            buffer.copyMemory(fromSpan: positions.span, byteOffset: 0)
        }
    }
    if let renormalizationData = deformationData.renormalizationData {
        if !renormalizationData.vertexIndicesPerTriangle.isEmpty {
            let indicesData = toData(renormalizationData.vertexIndicesPerTriangle)
            try? context.deformation.renormalization.replaceTriangleIndices { span in
                writeData(into: &span, from: indicesData)
            }
        }

        if !renormalizationData.vertexAdjacencies.isEmpty {
            let adjacenciesData = toData(renormalizationData.vertexAdjacencies)
            try? context.deformation.renormalization.replaceAdjacencies { span in
                writeData(into: &span, from: adjacenciesData)
            }
        }

        if !renormalizationData.vertexAdjacencyEndIndices.isEmpty {
            let endIndicesData = toData(renormalizationData.vertexAdjacencyEndIndices)
            try? context.deformation.renormalization.replaceAdjacencyEndIndices { span in
                writeData(into: &span, from: endIndicesData)
            }
        }
    }
}

// Map a mesh-resource semantic to the v2 LowLevelDeformation.VertexSemantic.
// Returns nil for semantics the deformation pipeline doesn't accept (e.g. color).
private func toV2VertexSemantic(_ semantic: LowLevelMeshResource.VertexSemantic) -> LowLevelDeformation.VertexSemantic? {
    switch semantic {
    case .position: .position
    case .normal: .normal
    case .tangent: .tangent
    case .bitangent: .bitangent
    case .uv0, .uv1, .uv2, .uv3, .uv4, .uv5, .uv6, .uv7: .uv
    default: nil
    }
}

// Per-attribute binding info: which mesh-resource buffer index, what byte
// offset within the layout, the v2 attribute (semantic + format + stride).
private struct BoundAttribute {
    let v2Attribute: LowLevelDeformation.VertexAttribute
    let bufferIndex: Int
    let offset: Int
}

private func makeBoundAttributes(
    meshResource: LowLevelMeshResource,
    needsNormal: Bool,
    needsTangent: Bool,
    needsBitangent: Bool,
    needsUV: Bool
) -> [BoundAttribute] {
    var result: [BoundAttribute] = []
    var addedUV = false // v2 has a single .uv slot
    let descriptor = meshResource.descriptor
    for attribute in descriptor.vertexAttributes {
        guard let v2Semantic = toV2VertexSemantic(attribute.semantic) else { continue }
        switch v2Semantic {
        case .position: break
        case .normal: if !needsNormal { continue }
        case .tangent: if !needsTangent { continue }
        case .bitangent: if !needsBitangent { continue }
        case .uv:
            if !needsUV { continue }
            if addedUV { continue }
            addedUV = true
        @unknown default: continue
        }
        let layout = descriptor.vertexLayouts[attribute.layoutIndex]
        let format: MTLVertexFormat
        switch v2Semantic {
        case .uv: format = .float2
        case .position, .normal, .tangent, .bitangent: format = .float3
        @unknown default: format = .float3
        }
        let v2Attr = LowLevelDeformation.VertexAttribute(
            semantic: v2Semantic,
            format: format,
            stride: layout.bufferStride
        )
        result.append(
            BoundAttribute(
                v2Attribute: v2Attr,
                bufferIndex: layout.bufferIndex,
                offset: attribute.offset + layout.bufferOffset
            )
        )
    }
    return result
}

private func buildDeformationContext(
    meshResource: LowLevelMeshResource,
    deformationData: WKBridgeDeformationData,
    commandBuffer: any MTLCommandBuffer,
    device: any MTLDevice,
    deformationContextRoot: LowLevelDeformationContext,
    memoryOwner: task_id_token_t
) throws -> DeformationContext {
    let vertexCount = meshResource.descriptor.vertexCapacity
    let hasSkinning = deformationData.skinningData != nil
    let hasBlending = deformationData.blendShapeData != nil
    let hasRenormalizing = deformationData.renormalizationData != nil
    let meshHasUV = meshResource.descriptor.vertexAttributes.contains { toV2VertexSemantic($0.semantic) == .uv }
    let meshHasNormal = meshResource.descriptor.vertexAttributes.contains { toV2VertexSemantic($0.semantic) == .normal }
    let meshHasTangent = meshResource.descriptor.vertexAttributes.contains { toV2VertexSemantic($0.semantic) == .tangent }
    let meshHasBitangent = meshResource.descriptor.vertexAttributes.contains { toV2VertexSemantic($0.semantic) == .bitangent }

    // ── Pipeline descriptor ──────────────────────────────────────────────
    var pipelineBlending: LowLevelDeformation.Pipeline.Descriptor.BlendShape? = nil
    if hasBlending {
        pipelineBlending = .init(blendsOutputs: [])
    }
    var pipelineSkinning: LowLevelDeformation.Pipeline.Descriptor.Skinning? = nil
    if hasSkinning {
        pipelineSkinning = .init(jointIndexType: .uint32)
    }
    var pipelineRenormalizing: LowLevelDeformation.Pipeline.Descriptor.Renormalization? = nil
    if hasRenormalizing {
        // Only request renormalization outputs the mesh actually has — the
        // pipeline rejects configuration where it would write an output that
        // doesn't exist on the bound mesh.
        var outputs: LowLevelDeformation.Pipeline.Descriptor.Renormalization.VertexSemanticOutputs = []
        if meshHasNormal { outputs.insert(.normal) }
        if meshHasTangent { outputs.insert(.tangent) }
        if meshHasBitangent { outputs.insert(.bitangent) }
        pipelineRenormalizing = .init(
            outputs: outputs,
            triangleIndexType: .uint32,
            adjacencyIndexType: .uint32,
            adjacencyEndIndexType: .uint32
        )
    }

    let inputAttrs = makeBoundAttributes(
        meshResource: meshResource,
        needsNormal: hasRenormalizing && meshHasNormal,
        needsTangent: hasRenormalizing && meshHasTangent,
        needsBitangent: hasRenormalizing && meshHasBitangent,
        needsUV: meshHasUV
    )
    let outputAttrs = makeBoundAttributes(
        meshResource: meshResource,
        needsNormal: hasRenormalizing && meshHasNormal,
        needsTangent: hasRenormalizing && meshHasTangent,
        needsBitangent: hasRenormalizing && meshHasBitangent,
        needsUV: meshHasUV
    )

    let pipelineDescriptor = LowLevelDeformation.Pipeline.Descriptor(
        inputAttributes: inputAttrs.map(\.v2Attribute),
        outputAttributes: outputAttrs.map(\.v2Attribute),
        blendShape: pipelineBlending,
        skinning: pipelineSkinning,
        renormalization: pipelineRenormalizing
    )
    let pipeline = try deformationContextRoot.makePipeline(pipelineDescriptor)

    // ── Per-frame deformation descriptor ─────────────────────────────────
    var blendingDesc: LowLevelDeformation.Descriptor.BlendShape? = nil
    var blendingTargetCount = 0
    if let blend = deformationData.blendShapeData {
        blendingTargetCount = max(1, blend.weights.count)
        blendingDesc = .init(targetCount: blendingTargetCount)
    }
    var skinningDesc: LowLevelDeformation.Descriptor.Skinning? = nil
    var skinningJointTransformCount = 0
    var skinningInfluencesPerVertex = 0
    if let skinning = deformationData.skinningData {
        skinningJointTransformCount = max(1, skinning.jointTransforms.count)
        skinningInfluencesPerVertex = Int(skinning.influencePerVertexCount)
        skinningDesc = .init(
            jointTransformCount: skinningJointTransformCount,
            influencesPerVertex: skinningInfluencesPerVertex
        )
    }
    var renormalizingDesc: LowLevelDeformation.Descriptor.Renormalization? = nil
    var renormalizingAdjacenciesCount = 0
    var renormalizingIndexCount = 0
    if let renorm = deformationData.renormalizationData {
        renormalizingAdjacenciesCount = max(1, renorm.vertexAdjacencies.count)
        renormalizingIndexCount = max(3, renorm.vertexIndicesPerTriangle.count)
        renormalizingDesc = .init(
            adjacenciesCount: renormalizingAdjacenciesCount,
            indexCount: renormalizingIndexCount
        )
    }
    let descriptor = LowLevelDeformation.Descriptor(
        vertexCount: vertexCount,
        blendShape: blendingDesc,
        skinning: skinningDesc,
        renormalization: renormalizingDesc
    )
    let deformation = try deformationContextRoot.makeDeformation(pipeline: pipeline, descriptor: descriptor)

    // ── Bind input/output mesh vertex buffers (sourced from meshResource) ─
    var collectedBuffers: [any MTLBuffer] = []
    for bound in inputAttrs {
        let resourceBuffer = meshResource.readVertices(at: bound.bufferIndex, commandBuffer: commandBuffer)
        // Copy the source data into a private buffer so the deformation doesn't
        // mutate the mesh resource's primary storage.
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let copy = device.makeBuffer(length: resourceBuffer.length, options: .storageModeShared)!
        copy.__setOwnerWithIdentity(memoryOwner)
        // swift-format-ignore: NeverForceUnwrap
        let blit = commandBuffer.makeBlitCommandEncoder()!
        blit.copy(from: resourceBuffer, sourceOffset: 0, to: copy, destinationOffset: 0, size: resourceBuffer.length)
        blit.endEncoding()
        try deformation.input.setVertices(copy, offset: bound.offset, semantic: bound.v2Attribute.semantic)
        collectedBuffers.append(copy)
    }
    for bound in outputAttrs {
        let resourceBuffer = meshResource.readVertices(at: bound.bufferIndex, commandBuffer: commandBuffer)
        try deformation.output.setVertices(resourceBuffer, offset: bound.offset, semantic: bound.v2Attribute.semantic)
        collectedBuffers.append(resourceBuffer)
    }

    // ── Skinning data ────────────────────────────────────────────────────
    var jointTransformsBuffer: (any MTLBuffer)? = nil
    var inverseBindPosesBuffer: (any MTLBuffer)? = nil
    var influenceWeightsBuffer: (any MTLBuffer)? = nil
    if let skinning = deformationData.skinningData {
        let jointTransforms = skinning.jointTransforms
        if !jointTransforms.isEmpty {
            let buf = device.makeBuffer(
                fromSpan: jointTransforms.span,
                length: jointTransforms.count * MemoryLayout<simd_float4x4>.stride,
                memoryOwner: memoryOwner,
                options: .storageModeShared
            )
            try deformation.skinning.setJointTransforms(buf, offset: 0)
            jointTransformsBuffer = buf
            collectedBuffers.append(buf)
        }

        let inverseBindPoses = skinning.inverseBindPoses
        if !inverseBindPoses.isEmpty {
            let buf = device.makeBuffer(
                fromSpan: inverseBindPoses.span,
                length: inverseBindPoses.count * MemoryLayout<simd_float4x4>.stride,
                memoryOwner: memoryOwner,
                options: .storageModeShared
            )
            try deformation.skinning.setInverseBindPoses(buf, offset: 0)
            inverseBindPosesBuffer = buf
            collectedBuffers.append(buf)
        }

        deformation.skinning.geometryBindTransform = skinning.geometryBindTransform

        let influenceWeights = skinning.influenceWeights
        if !influenceWeights.isEmpty {
            let buf = device.makeBuffer(
                fromSpan: influenceWeights.span,
                length: influenceWeights.count * MemoryLayout<Float>.stride,
                memoryOwner: memoryOwner,
                options: .storageModeShared
            )
            try deformation.skinning.setInfluenceWeights(buf, offset: 0)
            influenceWeightsBuffer = buf
            collectedBuffers.append(buf)
        }

        let influenceJointIndices = skinning.influenceJointIndices
        if !influenceJointIndices.isEmpty {
            let indicesData = toData(influenceJointIndices)
            try deformation.skinning.replaceInfluenceJointIndices { span in
                writeData(into: &span, from: indicesData)
            }
        }
    }

    // ── Blend-shape data ─────────────────────────────────────────────────
    var blendShapeWeightsBuffer: (any MTLBuffer)? = nil
    var blendShapePositionOffsetsBuffer: (any MTLBuffer)? = nil
    if let blend = deformationData.blendShapeData {
        let weights = blend.weights
        if !weights.isEmpty {
            let buf = device.makeBuffer(
                fromSpan: weights.span,
                length: weights.count * MemoryLayout<Float>.stride,
                memoryOwner: memoryOwner,
                options: .storageModeShared
            )
            try deformation.blendShape.setWeights(buf, offset: 0)
            blendShapeWeightsBuffer = buf
            collectedBuffers.append(buf)
        }

        let positions = blend.positionOffsets.flatMap(\.self)
        if !positions.isEmpty {
            let buf = device.makeBuffer(
                fromSpan: positions.span,
                length: positions.count * MemoryLayout<SIMD3<Float>>.stride,
                memoryOwner: memoryOwner,
                options: .storageModeShared
            )
            try deformation.blendShape.setPositionOffsets(buf, offset: 0)
            blendShapePositionOffsetsBuffer = buf
            collectedBuffers.append(buf)
        }
    }

    // ── Renormalization data ─────────────────────────────────────────────
    if let renorm = deformationData.renormalizationData {
        let triangleIndices = renorm.vertexIndicesPerTriangle
        if !triangleIndices.isEmpty {
            let triangleIndicesData = toData(triangleIndices)
            try deformation.renormalization.replaceTriangleIndices { span in
                writeData(into: &span, from: triangleIndicesData)
            }
        }

        let adjacencies = renorm.vertexAdjacencies
        if !adjacencies.isEmpty {
            let adjacenciesData = toData(adjacencies)
            try deformation.renormalization.replaceAdjacencies { span in
                writeData(into: &span, from: adjacenciesData)
            }
        }

        let endIndices = renorm.vertexAdjacencyEndIndices
        if !endIndices.isEmpty {
            let endIndicesData = toData(endIndices)
            try deformation.renormalization.replaceAdjacencyEndIndices { span in
                writeData(into: &span, from: endIndicesData)
            }
        }
    }

    return DeformationContext(
        pipeline: pipeline,
        deformation: deformation,
        dirty: true,
        mtlBuffers: collectedBuffers,
        jointTransformsBuffer: jointTransformsBuffer,
        inverseBindPosesBuffer: inverseBindPosesBuffer,
        influenceWeightsBuffer: influenceWeightsBuffer,
        blendShapeWeightsBuffer: blendShapeWeightsBuffer,
        blendShapePositionOffsetsBuffer: blendShapePositionOffsetsBuffer,
        vertexCount: vertexCount,
        blendingTargetCount: blendingTargetCount,
        skinningJointTransformCount: skinningJointTransformCount,
        skinningInfluencesPerVertex: skinningInfluencesPerVertex,
        renormalizingAdjacenciesCount: renormalizingAdjacenciesCount,
        renormalizingIndexCount: renormalizingIndexCount
    )
}

#endif
