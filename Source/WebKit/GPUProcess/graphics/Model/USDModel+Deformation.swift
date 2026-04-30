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

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreTextureProcessing, _version: 19) && canImport(_USDKit_RealityKit, _version: 42)
@_spi(UsdLoaderAPI) import _USDKit_RealityKit
@_spi(RealityCoreRendererAPI) import RealityKit
@_spi(RealityCoreTextureProcessingAPI) import RealityCoreTextureProcessing
import USDKit
@_spi(SwiftAPI) import DirectResource
import RealityKit
@_spi(SGPrivate) import ShaderGraph
import RealityCoreDeformation

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V21geometryBindTransformSo13simd_float4x4avg")
    func geometryBindTransformCompat() -> simd_float4x4
}

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V15jointTransformsSaySo13simd_float4x4aGvg")
    func jointTransformsCompat() -> [simd_float4x4]
}

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V16inverseBindPosesSaySo13simd_float4x4aGvg")
    func inverseBindPosesCompat() -> [simd_float4x4]
}

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
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let buffer = makeBuffer(length: length, options: options)!
        buffer.__setOwnerWithIdentity(memoryOwner)
        buffer.copyMemory(fromSpan: fromSpan, byteOffset: 0)
        return buffer
    }
}

extension WKBridgeSkinningData {
    func updateDeformerDescription(description: _Proto_LowLevelSkinningDescription_v1) {
        let skinningDescription = description

        // Only update buffers that have dirty data (non-empty arrays)
        if !self.jointTransforms.isEmpty {
            let buffer = skinningDescription.jointTransforms.metalBuffer
            let byteLength = self.jointTransforms.count * MemoryLayout<simd_float4x4>.size
            assert(buffer.length >= byteLength, "Buffer too small for joint transforms")
            buffer.copyMemory(fromSpan: self.jointTransforms.span, byteOffset: skinningDescription.jointTransforms.offset)
        }

        if !self.inverseBindPoses.isEmpty {
            let buffer = skinningDescription.inverseBindPoses.metalBuffer
            let byteLength = self.inverseBindPoses.count * MemoryLayout<simd_float4x4>.size
            assert(buffer.length >= byteLength, "Buffer too small for inverse bind poses")
            buffer.copyMemory(fromSpan: self.inverseBindPoses.span, byteOffset: skinningDescription.inverseBindPoses.offset)
        }

        if !self.influenceJointIndices.isEmpty {
            let buffer = skinningDescription.influenceJointIndices.metalBuffer
            let byteLength = self.influenceJointIndices.count * MemoryLayout<UInt32>.size
            assert(buffer.length >= byteLength, "Buffer too small for influence joint indices")
            buffer.copyMemory(fromSpan: self.influenceJointIndices.span, byteOffset: skinningDescription.influenceJointIndices.offset)
        }

        if !self.influenceWeights.isEmpty {
            let buffer = skinningDescription.influenceWeights.metalBuffer
            let byteLength = self.influenceWeights.count * MemoryLayout<Float>.size
            assert(buffer.length >= byteLength, "Buffer too small for influence weights")
            buffer.copyMemory(fromSpan: self.influenceWeights.span, byteOffset: skinningDescription.influenceWeights.offset)
        }

        // Note: geometryBindTransform and influencePerVertexCount are properties that can be directly set
        // These don't require buffer updates, just property assignment
        // If these need to be updated in the description, that would require API access we may not have
    }

    func makeDeformerDescription(device: any MTLDevice, memoryOwner: task_id_token_t) -> any _Proto_LowLevelDeformerDescription_v1 {
        let jointTransformsBuffer = device.makeBuffer(
            fromSpan: self.jointTransforms.span,
            length: self.jointTransforms.count * MemoryLayout<simd_float4x4>.size,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointTransformsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            jointTransformsBuffer,
            offset: 0,
            occupiedLength: jointTransformsBuffer.length,
            dataType: .float4x4
        )!
        let inverseBindPosesBuffer = device.makeBuffer(
            fromSpan: self.inverseBindPoses.span,
            length: self.inverseBindPoses.count * MemoryLayout<simd_float4x4>.size,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let inverseBindPosesDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            inverseBindPosesBuffer,
            offset: 0,
            occupiedLength: inverseBindPosesBuffer.length,
            dataType: .float4x4
        )!

        let jointIndicesBuffer = device.makeBuffer(
            fromSpan: self.influenceJointIndices.span,
            length: self.influenceJointIndices.count * MemoryLayout<UInt32>.size,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointIndicesDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            jointIndicesBuffer,
            offset: 0,
            occupiedLength: jointIndicesBuffer.length,
            dataType: .uint
        )!

        let influenceWeightsBuffer = device.makeBuffer(
            fromSpan: self.influenceWeights.span,
            length: self.influenceWeights.count * MemoryLayout<Float>.size,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let influenceWeightsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            influenceWeightsBuffer,
            offset: 0,
            occupiedLength: influenceWeightsBuffer.length,
            dataType: .float
        )!

        let deformerDescription = _Proto_LowLevelSkinningDescription_v1(
            jointTransforms: jointTransformsDescription,
            inverseBindPoses: inverseBindPosesDescription,
            influenceJointIndices: jointIndicesDescription,
            influenceWeights: influenceWeightsDescription,
            geometryBindTransform: self.geometryBindTransform,
            influencePerVertexCount: self.influencePerVertexCount
        )

        return deformerDescription
    }
}

extension WKBridgeBlendShapeData {
    func updateDeformerDescription(description: _Proto_LowLevelBlendShapeDescription_v1) {
        let blendShapeDescription = description

        // Only update if we have dirty data
        let hasWeights = !self.weights.isEmpty
        let hasPositionOffsets = !self.positionOffsets.isEmpty && !self.positionOffsets.allSatisfy { $0.isEmpty }

        if hasWeights || hasPositionOffsets {
            // TODO: This currently only works for the first blend target
            // rdar://171221610 (`LowLevelBlendShapeDescription` should support multiple blend targets in one deformation compute)
            if hasWeights {
                let inputWeights: [Float] = self.weights

                if let blendShapeWeightBuffer = blendShapeDescription.weights {
                    let buffer = blendShapeWeightBuffer.metalBuffer
                    let byteLength = inputWeights.count * MemoryLayout<Float>.size
                    assert(buffer.length >= byteLength, "Buffer too small for blend weights")
                    buffer.copyMemory(fromSpan: inputWeights.span, byteOffset: blendShapeWeightBuffer.offset)
                }
            }

            if hasPositionOffsets {
                let positionOffsets = self.positionOffsets.flatMap(\.self)
                let buffer = blendShapeDescription.positionOffsets.metalBuffer
                let byteLength = positionOffsets.count * MemoryLayout<SIMD3<Float>>.size
                assert(buffer.length >= byteLength, "Buffer too small for position offsets")
                buffer.copyMemory(fromSpan: positionOffsets.span, byteOffset: blendShapeDescription.positionOffsets.offset)
            }
        }
    }

    func makeDeformerDescription(device: any MTLDevice, memoryOwner: task_id_token_t) throws -> any _Proto_LowLevelDeformerDescription_v1 {
        var inputWeights: [Float] = []

        let blendTargetCount = self.weights.count
        let positionCount = self.positionOffsets[0].count
        for i in 0..<blendTargetCount {
            inputWeights += Array(repeating: self.weights[i], count: positionCount)
        }

        let blendWeightsBuffer = device.makeBuffer(
            fromSpan: inputWeights.span,
            length: inputWeights.count * MemoryLayout<Float>.size,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let blendWeightsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            blendWeightsBuffer,
            offset: 0,
            occupiedLength: blendWeightsBuffer.length,
            dataType: .float
        )!

        let positionOffsets = self.positionOffsets.flatMap(\.self)
        let positionOffsetsBuffer = device.makeBuffer(
            fromSpan: positionOffsets.span,
            length: positionOffsets.count * MemoryLayout<SIMD3<Float>>.size,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let positionOffsetsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            positionOffsetsBuffer,
            offset: 0,
            occupiedLength: positionOffsetsBuffer.length,
            dataType: .float3
        )!

        let deformerDescriptionResult = _Proto_LowLevelBlendShapeDescription_v1.make(
            weights: blendWeightsDescription,
            positionOffsets: positionOffsetsDescription,
            sparseIndices: nil,
            normalOffsets: nil,
            tangentOffsets: nil,
            blendBitangents: false
        )

        return try deformerDescriptionResult.get()
    }
}

extension WKBridgeRenormalizationData {
    func updateDeformerDescription(description: _Proto_LowLevelRenormalizationDescription_v1) {
        let renormalizationDescription = description

        // Renormalization data is all-or-nothing, so if we get here, update everything
        if !vertexAdjacencies.isEmpty {
            let buffer = renormalizationDescription.adjacencies.metalBuffer
            let byteLength = vertexAdjacencies.count * MemoryLayout<UInt32>.size
            assert(buffer.length >= byteLength, "Buffer too small for vertex adjacencies")
            buffer.copyMemory(fromSpan: vertexAdjacencies.span, byteOffset: renormalizationDescription.adjacencies.offset)
        }

        if !vertexAdjacencyEndIndices.isEmpty {
            let buffer = renormalizationDescription.adjacencyEndIndices.metalBuffer
            let byteLength = vertexAdjacencyEndIndices.count * MemoryLayout<UInt32>.size
            assert(buffer.length >= byteLength, "Buffer too small for adjacency end indices")
            buffer.copyMemory(fromSpan: vertexAdjacencyEndIndices.span, byteOffset: renormalizationDescription.adjacencyEndIndices.offset)
        }
    }

    func makeDeformerDescription(device: any MTLDevice, memoryOwner: task_id_token_t) throws -> any _Proto_LowLevelDeformerDescription_v1 {
        // Create adjacency buffer
        let adjacenciesMetalBuffer = device.makeBuffer(
            fromSpan: vertexAdjacencies.span,
            length: vertexAdjacencies.count * MemoryLayout<UInt32>.size,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let adjacenciesBuffer = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            adjacenciesMetalBuffer,
            offset: 0,
            occupiedLength: adjacenciesMetalBuffer.length,
            dataType: .uint
        )!

        // Create adjacency end indices buffer
        let adjacencyEndIndicesMetalBuffer = device.makeBuffer(
            fromSpan: vertexAdjacencyEndIndices.span,
            length: vertexAdjacencyEndIndices.count * MemoryLayout<UInt32>.size,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let adjacencyEndIndicesBuffer = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            adjacencyEndIndicesMetalBuffer,
            offset: 0,
            occupiedLength: adjacencyEndIndicesMetalBuffer.length,
            dataType: .uint
        )!

        let deformerDescription = _Proto_LowLevelRenormalizationDescription_v1.make(
            recalculateNormals: true,
            recalculateTangents: false,
            recalculateBitangents: false,
            adjacencies: adjacenciesBuffer,
            adjacencyEndIndices: adjacencyEndIndicesBuffer
        )

        return deformerDescription
    }
}

struct DeformationContext {
    let deformation: _Proto_Deformation_v1
    var description: _Proto_LowLevelDeformationDescription_v1
    var dirty: Bool
}

func configureDeformation(
    identifier: WKBridgeTypedResourceId,
    deformationData: WKBridgeDeformationData,
    commandBuffer: any MTLCommandBuffer,
    device: any MTLDevice,
    meshResource: _Proto_LowLevelMeshResource_v1,
    meshResourceToDeformationContext: inout [WKBridgeTypedResourceId: DeformationContext],
    deformationSystem: _Proto_LowLevelDeformationSystem_v1,
    memoryOwner: task_id_token_t
) {
    meshResourceToDeformationContext[identifier] = buildDeformationContext(
        meshResource: meshResource,
        deformationData: deformationData,
        commandBuffer: commandBuffer,
        device: device,
        deformationSystem: deformationSystem,
        existingContext: meshResourceToDeformationContext[identifier],
        identifierDescription: identifier.description,
        memoryOwner: memoryOwner
    )
}

func updateDeformationContextInPlace(deformationData: WKBridgeDeformationData, context: inout DeformationContext) -> Bool {
    var updatedInPlace = false
    for deformerDescription in context.description.deformerDescriptions {
        if var skinningDescription = deformerDescription as? _Proto_LowLevelSkinningDescription_v1,
            let skinningData = deformationData.skinningData
        {
            skinningData.updateDeformerDescription(description: skinningDescription)
            updatedInPlace = true
        }

        if var blendShapeDescription = deformerDescription as? _Proto_LowLevelBlendShapeDescription_v1,
            let blendShapeData = deformationData.blendShapeData
        {
            blendShapeData.updateDeformerDescription(description: blendShapeDescription)
            updatedInPlace = true
        }

        if var renormalizationDescription = deformerDescription as? _Proto_LowLevelRenormalizationDescription_v1,
            let renormalizationData = deformationData.renormalizationData
        {
            renormalizationData.updateDeformerDescription(description: renormalizationDescription)
            updatedInPlace = true
        }
    }

    return updatedInPlace
}

func buildDeformationContext(
    meshResource: _Proto_LowLevelMeshResource_v1,
    deformationData: WKBridgeDeformationData,
    commandBuffer: any MTLCommandBuffer,
    device: any MTLDevice,
    deformationSystem: _Proto_LowLevelDeformationSystem_v1,
    existingContext: DeformationContext?,
    identifierDescription: String,
    memoryOwner: task_id_token_t
) -> DeformationContext {
    // If we have an existing context, try to update it in place
    if var existingContext = existingContext {
        if updateDeformationContextInPlace(deformationData: deformationData, context: &existingContext) {
            existingContext.dirty = true
            return existingContext
        }
    }

    // Otherwise, build a new deformation context from scratch
    var deformers: [any _Proto_LowLevelDeformerDescription_v1] = []

    if let skinningData = deformationData.skinningData {
        let skinningDeformer = skinningData.makeDeformerDescription(device: device, memoryOwner: memoryOwner)
        deformers.append(skinningDeformer)
    }

    if let blendShapeData = deformationData.blendShapeData {
        do {
            let blendShapeDeformer = try blendShapeData.makeDeformerDescription(device: device, memoryOwner: memoryOwner)
            deformers.append(blendShapeDeformer)
        } catch {
            print("Error creating blend shape deformer for \(identifierDescription): \(error.localizedDescription)")
        }
    }

    if let renormalizationData = deformationData.renormalizationData {
        do {
            let renormalization = try renormalizationData.makeDeformerDescription(device: device, memoryOwner: memoryOwner)
            deformers.append(renormalization)
        } catch {
            print("Error creating renormalization deformer for \(identifierDescription): \(error.localizedDescription)")
        }
    }

    assert(!deformers.isEmpty)

    let inputMeshDescription: _Proto_LowLevelDeformationDescription_v1.MeshDescription?
    if let existingContext {
        inputMeshDescription = existingContext.description.input
    } else {
        inputMeshDescription = makeInputMeshDescriptionForDeformation(
            meshResource: meshResource,
            deformationData: deformationData,
            commandBuffer: commandBuffer,
            device: device,
            memoryOwner: memoryOwner
        )
    }

    let outputMeshDescription = makeOutputMeshDescriptionForDeformation(
        meshResource: meshResource,
        deformationData: deformationData,
        commandBuffer: commandBuffer,
        device: device,
        memoryOwner: memoryOwner
    )

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
    // swift-format-ignore: NeverForceUnwrap
    let deformationDescription =
        try? _Proto_LowLevelDeformationDescription_v1.make(
            input: inputMeshDescription!,
            deformers: deformers,
            output: outputMeshDescription!
        )
        .get()

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
    // swift-format-ignore: NeverForceUnwrap
    let deformation = try? deformationSystem.make(description: deformationDescription!).get()

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
    // swift-format-ignore: NeverForceUnwrap
    return DeformationContext(deformation: deformation!, description: deformationDescription!, dirty: true)
}

struct DeformationMeshDescriptionData {
    var vertexAttributes: [_Proto_LowLevelDeformationDescription_v1.VertexAttribute] = []
    var vertexLayouts: [_Proto_LowLevelDeformationDescription_v1.VertexLayout] = []
    var mtlBuffers: [any MTLBuffer] = []
}

func makeMeshDescriptionForDeformation(
    meshResource: _Proto_LowLevelMeshResource_v1,
    deformationData: WKBridgeDeformationData,
    commandBuffer: any MTLCommandBuffer,
    isInput: Bool,
    device: any MTLDevice,
    memoryOwner: task_id_token_t
) -> DeformationMeshDescriptionData {
    var deformationMeshDescriptionData = DeformationMeshDescriptionData()

    // Table to map mesh resource buffer index to deformation buffer index
    // They can be different because deformation may not require all vertex buffers
    // e.g. if position data is in mesh resource buffer 1, its deformation buffer index could be 0
    // if that's the only attribute data require by deformation
    var meshResourceBufferIndexToDeformationBufferIndex: [Int: Int] = [:]
    // Similar usage as the buffer index table. See comment above
    var meshResourceLayoutIndexToDeformationLayoutIndex: [Int: Int] = [:]

    var inputAttributeSemantics: [_Proto_LowLevelMeshResource_v1.VertexSemantic] = [.position]
    if deformationData.renormalizationData != nil {
        inputAttributeSemantics.append(contentsOf: [.normal, .tangent, .bitangent])
    }

    for attribute in meshResource.descriptor.vertexAttributes where inputAttributeSemantics.contains(attribute.semantic) {
        guard let deformationMeshSemantic = attribute.semantic.toDeformationVertexSemantic() else {
            fatalError("Invalid semantic for deformation input: \(attribute.semantic)")
        }

        let layoutIndex = attribute.layoutIndex
        let layout = meshResource.descriptor.vertexLayouts[layoutIndex]
        let bufferIndex = layout.bufferIndex

        var deformationBufferIndex = deformationMeshDescriptionData.mtlBuffers.count
        if let cachedDeformationBufferIndex = meshResourceBufferIndexToDeformationBufferIndex[bufferIndex] {
            deformationBufferIndex = cachedDeformationBufferIndex
        } else {
            meshResourceBufferIndexToDeformationBufferIndex[bufferIndex] = deformationBufferIndex

            let vertexBuffer = meshResource.readVertices_v2(at: bufferIndex, using: commandBuffer)
            if isInput {
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                let mtlBuffer = device.makeBuffer(length: vertexBuffer.length, options: .storageModeShared)!
                // Copy data from vertexPositionsBuffer to inputPositionsBuffer
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                let blitEncoder = commandBuffer.makeBlitCommandEncoder()!
                blitEncoder.copy(from: vertexBuffer, sourceOffset: 0, to: mtlBuffer, destinationOffset: 0, size: vertexBuffer.length)
                deformationMeshDescriptionData.mtlBuffers.append(mtlBuffer)
                blitEncoder.endEncoding()
            } else {
                deformationMeshDescriptionData.mtlBuffers.append(vertexBuffer)
            }
        }

        var deformationLayoutIndex = deformationMeshDescriptionData.vertexLayouts.count
        if let cachedDeformationLayoutIndex = meshResourceLayoutIndexToDeformationLayoutIndex[layoutIndex] {
            deformationLayoutIndex = cachedDeformationLayoutIndex
        } else {
            meshResourceLayoutIndexToDeformationLayoutIndex[layoutIndex] = deformationLayoutIndex
            deformationMeshDescriptionData.vertexLayouts.append(
                .init(bufferIndex: deformationBufferIndex, bufferOffset: layout.bufferOffset, bufferStride: layout.bufferStride)
            )
        }

        // Hardcode elementType for now the inputs could only be position or tangent
        // frame data which are all .float3
        // TODO: fix this when uv is required for deformation
        deformationMeshDescriptionData.vertexAttributes.append(
            .init(elementType: .float3, offset: attribute.offset, layoutIndex: deformationLayoutIndex, semantic: deformationMeshSemantic)
        )
    }

    return deformationMeshDescriptionData
}

func makeInputMeshDescriptionForDeformation(
    meshResource: _Proto_LowLevelMeshResource_v1,
    deformationData: WKBridgeDeformationData,
    commandBuffer: any MTLCommandBuffer,
    device: any MTLDevice,
    memoryOwner: task_id_token_t
) -> _Proto_LowLevelDeformationDescription_v1.MeshDescription? {
    var inputMeshDescriptionData = makeMeshDescriptionForDeformation(
        meshResource: meshResource,
        deformationData: deformationData,
        commandBuffer: commandBuffer,
        isInput: true,
        device: device,
        memoryOwner: memoryOwner
    )

    if let renormalizationData = deformationData.renormalizationData {
        let vertexIndices = renormalizationData.vertexIndicesPerTriangle
        let inputIndexBuffer = device.makeBuffer(
            fromSpan: vertexIndices.span,
            length: vertexIndices.count * MemoryLayout<UInt32>.stride,
            memoryOwner: memoryOwner,
            options: .storageModeShared
        )

        inputMeshDescriptionData.vertexAttributes.append(
            .init(elementType: .uint, offset: 0, layoutIndex: inputMeshDescriptionData.vertexLayouts.count, semantic: .index)
        )
        inputMeshDescriptionData.vertexLayouts.append(
            .init(bufferIndex: inputMeshDescriptionData.mtlBuffers.count, bufferOffset: 0, bufferStride: MemoryLayout<UInt32>.stride)
        )

        inputMeshDescriptionData.mtlBuffers.append(inputIndexBuffer)
    }

    let result = _Proto_LowLevelDeformationDescription_v1.MeshDescription.make(
        buffers: inputMeshDescriptionData.mtlBuffers,
        attributes: inputMeshDescriptionData.vertexAttributes,
        layouts: inputMeshDescriptionData.vertexLayouts,
        vertexCount: meshResource.descriptor.vertexCapacity,
        indexCount: meshResource.descriptor.indexCapacity
    )

    switch result {
    case .success(let meshDescription):
        return meshDescription
    case .failure(let error):
        print(error)
        return nil
    }
}

func makeOutputMeshDescriptionForDeformation(
    meshResource: _Proto_LowLevelMeshResource_v1,
    deformationData: WKBridgeDeformationData,
    commandBuffer: any MTLCommandBuffer,
    device: any MTLDevice,
    memoryOwner: task_id_token_t
) -> _Proto_LowLevelDeformationDescription_v1.MeshDescription? {
    let outputMeshDescriptionData = makeMeshDescriptionForDeformation(
        meshResource: meshResource,
        deformationData: deformationData,
        commandBuffer: commandBuffer,
        isInput: false,
        device: device,
        memoryOwner: memoryOwner
    )

    let result = _Proto_LowLevelDeformationDescription_v1.MeshDescription.make(
        buffers: outputMeshDescriptionData.mtlBuffers,
        attributes: outputMeshDescriptionData.vertexAttributes,
        layouts: outputMeshDescriptionData.vertexLayouts,
        vertexCount: meshResource.descriptor.vertexCapacity
    )

    switch result {
    case .success(let meshDescription):
        return meshDescription
    case .failure(let error):
        print(error)
        return nil
    }
}

extension _Proto_LowLevelMeshResource_v1.VertexSemantic {
    func toDeformationVertexSemantic() -> _Proto_LowLevelDeformationDescription_v1.MeshSemantic? {
        switch self {
        case .position:
            return .position
        case .normal:
            return .normal
        case .tangent:
            return .tangent
        case .bitangent:
            return .bitangent
        case .uv0:
            return .uv0
        case .uv1:
            return .uv1
        case .uv2:
            return .uv2
        case .uv3:
            return .uv3
        case .uv4:
            return .uv4
        case .uv5:
            return .uv5
        case .uv6:
            return .uv6
        case .uv7:
            return .uv7
        default:
            return nil
        }
    }
}

#endif
