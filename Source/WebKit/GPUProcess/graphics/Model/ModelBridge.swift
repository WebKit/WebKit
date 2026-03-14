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
import WebKit

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreRenderer, _version: 11) && compiler(>=6.2)
@_weakLinked @_spi(UsdLoaderAPI) import _USDKit_RealityKit
@_spi(RealityCoreRendererAPI) import RealityKit
@_weakLinked import USDKit
@_weakLinked @_spi(SwiftAPI) import DirectResource
@_weakLinked import _USDKit_RealityKit
@_weakLinked import ShaderGraph
#endif

@objc
@implementation
extension WKBridgeVertexAttributeFormat {
    let semantic: Int
    let format: UInt
    let layoutIndex: Int
    let offset: Int

    init(
        semantic: Int,
        format: UInt,
        layoutIndex: Int,
        offset: Int
    ) {
        self.semantic = semantic
        self.format = format
        self.layoutIndex = layoutIndex
        self.offset = offset
    }
}

@objc
@implementation
extension WKBridgeVertexLayout {
    let bufferIndex: Int
    let bufferOffset: Int
    let bufferStride: Int

    init(
        bufferIndex: Int,
        bufferOffset: Int,
        bufferStride: Int
    ) {
        self.bufferIndex = bufferIndex
        self.bufferOffset = bufferOffset
        self.bufferStride = bufferStride
    }
}

@objc
@implementation
extension WKBridgeMeshPart {
    let indexOffset: Int
    let indexCount: Int
    let topology: MTLPrimitiveType
    let materialIndex: Int
    let boundsMin: simd_float3
    let boundsMax: simd_float3

    init(
        indexOffset: Int,
        indexCount: Int,
        topology: MTLPrimitiveType,
        materialIndex: Int,
        boundsMin: simd_float3,
        boundsMax: simd_float3
    ) {
        self.indexOffset = indexOffset
        self.indexCount = indexCount
        self.topology = topology
        self.materialIndex = materialIndex
        self.boundsMin = boundsMin
        self.boundsMax = boundsMax
    }
}

@objc
@implementation
extension WKBridgeMeshDescriptor {
    let vertexBufferCount: Int
    let vertexCapacity: Int
    let vertexAttributes: [WKBridgeVertexAttributeFormat]
    let vertexLayouts: [WKBridgeVertexLayout]
    let indexCapacity: Int
    let indexType: MTLIndexType

    init(
        vertexBufferCount: Int,
        vertexCapacity: Int,
        vertexAttributes: [WKBridgeVertexAttributeFormat],
        vertexLayouts: [WKBridgeVertexLayout],
        indexCapacity: Int,
        indexType: MTLIndexType
    ) {
        self.vertexBufferCount = vertexBufferCount
        self.vertexCapacity = vertexCapacity
        self.vertexAttributes = vertexAttributes
        self.vertexLayouts = vertexLayouts
        self.indexCapacity = indexCapacity
        self.indexType = indexType
    }
}

@objc
@implementation
extension WKBridgeSkinningData {
    let influencePerVertexCount: UInt8
    let jointTransformsData: Data?
    let inverseBindPosesData: Data?
    let influenceJointIndicesData: Data?
    let influenceWeightsData: Data?
    let geometryBindTransform: simd_float4x4

    init(
        influencePerVertexCount: UInt8,
        jointTransforms: Data?,
        inverseBindPoses: Data?,
        influenceJointIndices: Data?,
        influenceWeights: Data?,
        geometryBindTransform: simd_float4x4
    ) {
        self.influencePerVertexCount = influencePerVertexCount
        self.jointTransformsData = jointTransforms
        self.inverseBindPosesData = inverseBindPoses
        self.influenceJointIndicesData = influenceJointIndices
        self.influenceWeightsData = influenceWeights
        self.geometryBindTransform = geometryBindTransform
    }
}

@objc
@implementation
extension WKBridgeBlendShapeData {
    let weightsData: Data
    let positionOffsetsData: [Data]
    let normalOffsetsData: [Data]

    init(
        weights: Data,
        positionOffsets: [Data],
        normalOffsets: [Data]
    ) {
        self.weightsData = weights
        self.positionOffsetsData = positionOffsets
        self.normalOffsetsData = normalOffsets
    }
}

@objc
@implementation
extension WKBridgeRenormalizationData {
    let vertexIndicesPerTriangleData: Data
    let vertexAdjacenciesData: Data
    let vertexAdjacencyEndIndicesData: Data

    init(
        vertexIndicesPerTriangle: Data,
        vertexAdjacencies: Data,
        vertexAdjacencyEndIndices: Data
    ) {
        self.vertexIndicesPerTriangleData = vertexIndicesPerTriangle
        self.vertexAdjacenciesData = vertexAdjacencies
        self.vertexAdjacencyEndIndicesData = vertexAdjacencyEndIndices
    }
}

@objc
@implementation
extension WKBridgeDeformationData {
    let skinningData: WKBridgeSkinningData?
    let blendShapeData: WKBridgeBlendShapeData?
    let renormalizationData: WKBridgeRenormalizationData?

    init(
        skinningData: WKBridgeSkinningData?,
        blendShapeData: WKBridgeBlendShapeData?,
        renormalizationData: WKBridgeRenormalizationData?
    ) {
        self.skinningData = skinningData
        self.blendShapeData = blendShapeData
        self.renormalizationData = renormalizationData
    }
}

@objc
@implementation
extension WKBridgeUpdateMesh {
    let identifier: String
    let updateType: WKBridgeDataUpdateType
    let descriptor: WKBridgeMeshDescriptor?
    let parts: [WKBridgeMeshPart]
    let indexData: Data?
    let vertexData: [Data]
    let instanceTransformsData: Data? // [float4x4]
    let instanceTransformsCount: Int
    let materialPrims: [String]
    let deformationData: WKBridgeDeformationData?

    init(
        identifier: String,
        updateType: WKBridgeDataUpdateType,
        descriptor: WKBridgeMeshDescriptor?,
        parts: [WKBridgeMeshPart],
        indexData: Data?,
        vertexData: [Data],
        instanceTransforms: Data?,
        instanceTransformsCount: Int,
        materialPrims: [String],
        deformationData: WKBridgeDeformationData?
    ) {
        self.identifier = identifier
        self.updateType = updateType
        self.descriptor = descriptor
        self.parts = parts
        self.indexData = indexData
        self.vertexData = vertexData
        self.instanceTransformsData = instanceTransforms
        self.instanceTransformsCount = instanceTransformsCount
        self.materialPrims = materialPrims
        self.deformationData = deformationData
    }
}

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreRenderer, _version: 11) && compiler(>=6.2)
func decodeValues<T>(from data: Data) -> [T] {
    let stride = MemoryLayout<T>.stride

    guard data.count > 0, data.count % stride == 0 else {
        return []
    }

    return unsafe data.withUnsafeBytes { rawBufferPointer in
        guard let baseAddress = rawBufferPointer.baseAddress else {
            return []
        }

        let count = rawBufferPointer.count / stride
        let pointer = unsafe baseAddress.assumingMemoryBound(to: T.self)
        return (0..<count).map { unsafe pointer[$0] }
    }
}

extension WKBridgeBlendShapeData {
    var weights: [Float] {
        decodeValues(from: weightsData)
    }

    var positionOffsets: [[SIMD3<Float>]] {
        positionOffsetsData.enumerated()
            .compactMap { index, data in
                let values: [SIMD3<Float>] = decodeValues(from: data)
                if values.isEmpty && data.count > 0 {
                    assertionFailure(
                        "positionOffsets[\(index)] data size (\(data.count)) is not a multiple of SIMD3<Float> stride (\(MemoryLayout<SIMD3<Float>>.stride))"
                    )
                    return nil
                }
                return values
            }
    }

    var normalOffsets: [[SIMD3<Float>]] {
        normalOffsetsData.enumerated()
            .compactMap { index, data in
                let values: [SIMD3<Float>] = decodeValues(from: data)
                if values.isEmpty && data.count > 0 {
                    assertionFailure(
                        "normalOffsets[\(index)] data size (\(data.count)) is not a multiple of SIMD3<Float> stride (\(MemoryLayout<SIMD3<Float>>.stride))"
                    )
                    return nil
                }
                return values
            }
    }
}

extension WKBridgeRenormalizationData {
    var vertexIndicesPerTriangle: [UInt32] { decodeValues(from: vertexIndicesPerTriangleData) }
    var vertexAdjacencies: [UInt32] { decodeValues(from: vertexAdjacenciesData) }
    var vertexAdjacencyEndIndices: [UInt32] { decodeValues(from: vertexAdjacencyEndIndicesData) }
}

extension WKBridgeUpdateMesh {
    var instanceTransforms: [simd_float4x4] {
        guard let data = instanceTransformsData, instanceTransformsCount > 0 else {
            return []
        }

        let expectedSize = MemoryLayout<simd_float4x4>.stride * instanceTransformsCount

        guard data.count >= expectedSize else {
            assertionFailure("instanceTransforms data size (\(data.count)) is less than expected (\(expectedSize))")
            return []
        }

        return decodeValues(from: data.prefix(expectedSize))
    }
}
#endif

@objc
@implementation
extension WKBridgeImageAsset {
    let data: Data?
    let width: Int
    let height: Int
    let depth: Int
    let bytesPerPixel: Int
    let textureType: MTLTextureType
    let pixelFormat: MTLPixelFormat
    let mipmapLevelCount: Int
    let arrayLength: Int
    let textureUsage: MTLTextureUsage
    let swizzle: MTLTextureSwizzleChannels

    init(
        data: Data?,
        width: Int,
        height: Int,
        depth: Int,
        bytesPerPixel: Int,
        textureType: MTLTextureType,
        pixelFormat: MTLPixelFormat,
        mipmapLevelCount: Int,
        arrayLength: Int,
        textureUsage: MTLTextureUsage,
        swizzle: MTLTextureSwizzleChannels
    ) {
        self.data = data
        self.width = width
        self.height = height
        self.depth = depth
        self.bytesPerPixel = bytesPerPixel
        self.textureType = textureType
        self.pixelFormat = pixelFormat
        self.mipmapLevelCount = mipmapLevelCount
        self.arrayLength = arrayLength
        self.textureUsage = textureUsage
        self.swizzle = swizzle
    }
}

@objc
@implementation
extension WKBridgeUpdateTexture {
    let imageAsset: WKBridgeImageAsset?
    let identifier: String
    let hashString: String

    init(
        imageAsset: WKBridgeImageAsset?,
        identifier: String,
        hashString: String
    ) {
        self.imageAsset = imageAsset
        self.identifier = identifier
        self.hashString = hashString
    }
}

@objc
@implementation
extension WKBridgeUpdateMaterial {
    let materialGraph: WKBridgeMaterialGraph?
    let identifier: String

    init(
        materialGraph: WKBridgeMaterialGraph?,
        identifier: String,
    ) {
        self.materialGraph = materialGraph
        self.identifier = identifier
    }
}

@objc
@implementation
extension WKBridgeInputOutput {
    let type: WKBridgeDataType
    let name: String
    let semanticType: WKBridgeDataType
    let hasSemanticType: Bool
    let defaultValue: WKBridgeConstantContainer?

    init(
        type: WKBridgeDataType,
        name: String,
        semanticType: WKBridgeDataType,
        hasSemanticType: Bool,
        defaultValue: WKBridgeConstantContainer?
    ) {
        self.type = type
        self.name = name
        self.semanticType = semanticType
        self.hasSemanticType = hasSemanticType
        self.defaultValue = defaultValue
    }
}

@objc
@implementation
extension WKBridgeConstantContainer {
    let constant: WKBridgeConstant
    let constantValues: [WKBridgeValueString]
    let name: String

    init(
        constant: WKBridgeConstant,
        constantValues: [WKBridgeValueString],
        name: String
    ) {
        self.constant = constant
        self.constantValues = constantValues
        self.name = name
    }
}

@objc
@implementation
extension WKBridgeBuiltin {
    let definition: String
    let name: String

    init(
        definition: String,
        name: String
    ) {
        self.definition = definition
        self.name = name
    }
}

@objc
@implementation
extension WKBridgeEdge {
    let outputNode: String
    let outputPort: String
    let inputNode: String
    let inputPort: String

    init(
        outputNode: String,
        outputPort: String,
        inputNode: String,
        inputPort: String
    ) {
        self.outputNode = outputNode
        self.outputPort = outputPort
        self.inputNode = inputNode
        self.inputPort = inputPort
    }
}

@objc
@implementation
extension WKBridgeValueString {
    let number: NSNumber
    let string: String
    let isString: Bool

    init(
        string: String
    ) {
        self.number = NSNumber(value: 0)
        self.string = string
        self.isString = true
    }

    init(
        number: NSNumber
    ) {
        self.number = number
        self.string = ""
        self.isString = false
    }
}

@objc
@implementation
extension WKBridgeNode {
    let bridgeNodeType: WKBridgeNodeType
    let builtin: WKBridgeBuiltin?
    let constant: WKBridgeConstantContainer?

    init(
        bridgeNodeType: WKBridgeNodeType,
        builtin: WKBridgeBuiltin,
        constant: WKBridgeConstantContainer
    ) {
        self.bridgeNodeType = bridgeNodeType
        self.builtin = builtin
        self.constant = constant
    }
}

@objc
@implementation
extension WKBridgeMaterialGraph {
    let nodes: [WKBridgeNode]
    let edges: [WKBridgeEdge]
    let arguments: WKBridgeNode
    let results: WKBridgeNode
    let inputs: [WKBridgeInputOutput]
    let outputs: [WKBridgeInputOutput]

    init(
        nodes: [WKBridgeNode],
        edges: [WKBridgeEdge],
        arguments: WKBridgeNode,
        results: WKBridgeNode,
        inputs: [WKBridgeInputOutput],
        outputs: [WKBridgeInputOutput]
    ) {
        self.nodes = nodes
        self.edges = edges
        self.arguments = arguments
        self.results = results
        self.inputs = inputs
        self.outputs = outputs
    }
}

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreRenderer, _version: 11) && compiler(>=6.2)

internal func toData<T>(_ input: [T]) -> Data {
    // FIXME: (rdar://164559261) understand/document/remove unsafety
    unsafe input.withUnsafeBytes { bufferPointer in
        unsafe Data(bufferPointer)
    }
}

private func toDataArray<T>(_ input: [[T]]) -> [Data] {
    input.map { toData($0) }
}

private func convertSemantic(_ semantic: LowLevelMesh.VertexSemantic) -> Int {
    switch semantic {
    case .position: 0
    case .color: 1
    case .normal: 2
    case .tangent: 3
    case .bitangent: 4
    case .uv0: 5
    case .uv1: 6
    case .uv2: 7
    case .uv3: 8
    case .uv4: 9
    case .uv5: 10
    case .uv6: 11
    case .uv7: 12
    default: 13
    }
}

private func webAttributesFromAttributes(_ attributes: [LowLevelMesh.Attribute]) -> [WKBridgeVertexAttributeFormat] {
    attributes.map({ a in
        WKBridgeVertexAttributeFormat(
            semantic: convertSemantic(a.semantic),
            format: a.format.rawValue,
            layoutIndex: a.layoutIndex,
            offset: a.offset
        )
    })
}

private func webLayoutsFromLayouts(_ attributes: [LowLevelMesh.Layout]) -> [WKBridgeVertexLayout] {
    attributes.map({ a in
        WKBridgeVertexLayout(bufferIndex: a.bufferIndex, bufferOffset: a.bufferOffset, bufferStride: a.bufferStride)
    })
}

extension WKBridgeMeshDescriptor {
    @nonobjc
    convenience init(request: LowLevelMesh.Descriptor) {
        self.init(
            vertexBufferCount: request.vertexBufferCount,
            vertexCapacity: request.vertexCapacity,
            vertexAttributes: webAttributesFromAttributes(request.vertexAttributes),
            vertexLayouts: webLayoutsFromLayouts(request.vertexLayouts),
            indexCapacity: request.indexCapacity,
            indexType: request.indexType
        )
    }
}
extension WKBridgeSkinningData {
    var jointTransforms: [simd_float4x4] {
        guard let data = jointTransformsData else {
            return []
        }

        let jointTransformsCount = data.count / MemoryLayout<simd_float4x4>.size
        guard jointTransformsCount > 0 else {
            return []
        }

        let matrixSize = MemoryLayout<simd_float4x4>.stride
        let expectedSize = matrixSize * jointTransformsCount

        guard data.count >= expectedSize else {
            assertionFailure("instanceTransforms data size (\(data.count)) is less than expected (\(expectedSize))")
            return []
        }

        return unsafe data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = unsafe baseAddress.assumingMemoryBound(to: simd_float4x4.self)
            return (0..<jointTransformsCount).map { unsafe matrices[$0] }
        }
    }

    var inverseBindPoses: [simd_float4x4] {
        guard let data = inverseBindPosesData else {
            return []
        }

        let inverseBindPosesCount = data.count / MemoryLayout<simd_float4x4>.size
        guard inverseBindPosesCount > 0 else {
            return []
        }

        let matrixSize = MemoryLayout<simd_float4x4>.stride
        let expectedSize = matrixSize * inverseBindPosesCount

        guard data.count >= expectedSize else {
            assertionFailure("instanceTransforms data size (\(data.count)) is less than expected (\(expectedSize))")
            return []
        }

        return unsafe data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = unsafe baseAddress.assumingMemoryBound(to: simd_float4x4.self)
            return (0..<inverseBindPosesCount).map { unsafe matrices[$0] }
        }
    }

    var influenceJointIndices: [UInt32] {
        guard let data = influenceJointIndicesData else {
            return []
        }

        let influenceJointIndicesCount = data.count / MemoryLayout<UInt32>.size
        guard influenceJointIndicesCount > 0 else {
            return []
        }

        let matrixSize = MemoryLayout<UInt32>.stride
        let expectedSize = matrixSize * influenceJointIndicesCount

        guard data.count >= expectedSize else {
            assertionFailure("instanceTransforms data size (\(data.count)) is less than expected (\(expectedSize))")
            return []
        }

        return unsafe data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = unsafe baseAddress.assumingMemoryBound(to: UInt32.self)
            return (0..<influenceJointIndicesCount).map { unsafe matrices[$0] }
        }
    }

    var influenceWeights: [Float] {
        guard let data = influenceWeightsData else {
            return []
        }

        let influenceWeightsCount = data.count / MemoryLayout<Float>.size
        guard influenceWeightsCount > 0 else {
            return []
        }

        let matrixSize = MemoryLayout<Float>.stride
        let expectedSize = matrixSize * influenceWeightsCount

        guard data.count >= expectedSize else {
            assertionFailure("instanceTransforms data size (\(data.count)) is less than expected (\(expectedSize))")
            return []
        }

        #if compiler(>=6.2)
        return unsafe data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = unsafe baseAddress.assumingMemoryBound(to: Float.self)
            return (0..<influenceWeightsCount).map { unsafe matrices[$0] }
        }
        #else
        return data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = baseAddress.assumingMemoryBound(to: Float.self)
            return (0..<influenceWeightsCount).map { matrices[$0] }
        }
        #endif
    }

    @nonobjc
    convenience init?(_ request: _Proto_DeformationData_v1.SkinningData?) {
        guard let request else {
            return nil
        }

        self.init(
            influencePerVertexCount: request.influencePerVertexCount,
            jointTransforms: toData(request.jointTransformsCompat()),
            inverseBindPoses: toData(request.inverseBindPosesCompat()),
            influenceJointIndices: toData(request.influenceJointIndices),
            influenceWeights: toData(request.influenceWeights),
            geometryBindTransform: request.geometryBindTransformCompat()
        )
    }
}
extension WKBridgeBlendShapeData {
    @nonobjc
    convenience init?(_ request: _Proto_DeformationData_v1.BlendShapeData?) {
        guard let request else {
            return nil
        }

        self.init(
            weights: toData(request.weights),
            positionOffsets: toDataArray(request.positionOffsets),
            normalOffsets: toDataArray(request.normalOffsets)
        )
    }
}
extension WKBridgeRenormalizationData {
    @nonobjc
    convenience init?(_ request: _Proto_DeformationData_v1.RenormalizationData?) {
        guard let request else {
            return nil
        }

        self.init(
            vertexIndicesPerTriangle: toData(request.vertexIndicesPerTriangle),
            vertexAdjacencies: toData(request.vertexAdjacencies),
            vertexAdjacencyEndIndices: toData(request.vertexAdjacencyEndIndices)
        )
    }
}
extension WKBridgeDeformationData {
    @nonobjc
    convenience init?(_ request: _Proto_DeformationData_v1?) {
        guard let request else {
            return nil
        }

        self.init(
            skinningData: .init(request.skinningData),
            blendShapeData: .init(request.blendShapeData),
            renormalizationData: .init(request.renormalizationData)
        )
    }
}
extension WKBridgeImageAsset {
    @nonobjc
    convenience init(_ asset: LowLevelTexture.Descriptor, data: Data) {
        self.init(
            data: data,
            width: asset.width,
            height: asset.height,
            depth: asset.depth,
            bytesPerPixel: 0, // client calculates this
            textureType: asset.textureType,
            pixelFormat: asset.pixelFormat,
            mipmapLevelCount: asset.mipmapLevelCount,
            arrayLength: asset.arrayLength,
            textureUsage: asset.textureUsage,
            swizzle: asset.swizzle
        )
    }
}

#endif
