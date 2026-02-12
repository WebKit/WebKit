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

internal import Metal
internal import WebKit_Internal

#if canImport(RealityCoreRenderer, _version: 9) && os(macOS) && canImport(_USDKit_RealityKit)
@_spi(RealityCoreRendererAPI) internal import RealityKit
@_spi(UsdLoaderAPI) internal import _USDKit_RealityKit
@_spi(SwiftAPI) internal import DirectResource
internal import _USDKit_RealityKit
internal import ShaderGraph
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
    let weights: Data
    let positionOffsets: [Data]
    let normalOffsets: [Data]

    init(
        weights: Data,
        positionOffsets: [Data],
        normalOffsets: [Data]
    ) {
        self.weights = weights
        self.positionOffsets = positionOffsets
        self.normalOffsets = normalOffsets
    }
}

@objc
@implementation
extension WKBridgeRenormalizationData {
    let vertexIndicesPerTriangle: Data
    let vertexAdjacencies: Data
    let vertexAdjacencyEndIndices: Data

    init(
        vertexIndicesPerTriangle: Data,
        vertexAdjacencies: Data,
        vertexAdjacencyEndIndices: Data
    ) {
        self.vertexIndicesPerTriangle = vertexIndicesPerTriangle
        self.vertexAdjacencies = vertexAdjacencies
        self.vertexAdjacencyEndIndices = vertexAdjacencyEndIndices
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

extension WKBridgeUpdateMesh {
    var instanceTransforms: [simd_float4x4] {
        guard let data = instanceTransformsData else {
            return []
        }

        guard instanceTransformsCount > 0 else {
            return []
        }

        let matrixSize = MemoryLayout<simd_float4x4>.stride
        let expectedSize = matrixSize * instanceTransformsCount

        guard data.count >= expectedSize else {
            assertionFailure("instanceTransforms data size (\(data.count)) is less than expected (\(expectedSize))")
            return []
        }

#if compiler(>=6.2)
        return unsafe data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = unsafe baseAddress.assumingMemoryBound(to: simd_float4x4.self)
            return (0..<instanceTransformsCount).map { unsafe matrices[$0] }
        }
#else
        return data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = baseAddress.assumingMemoryBound(to: simd_float4x4.self)
            return (0..<instanceTransformsCount).map { matrices[$0] }
        }
#endif
    }
}

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
extension WKBridgeTypeReference {
    let moduleName: String
    let name: String
    let typeDefIndex: Int

    init(module: String, name: String, typeDefIndex: Int) {
        self.moduleName = module
        self.name = name
        self.typeDefIndex = typeDefIndex
    }
}

@objc
@implementation
extension WKBridgeFunctionReference {
    let moduleName: String
    let functionIndex: Int

    init(
        moduleName: String,
        functionIndex: Int
    ) {
        self.moduleName = moduleName
        self.functionIndex = functionIndex
    }
}

@objc
@implementation
extension WKBridgeModule {
    let name: String
    let imports: [WKBridgeModuleReference]
    let typeDefinitions: [WKBridgeTypeDefinition]
    let functions: [WKBridgeFunction]
    let graphs: [WKBridgeModuleGraph]

    init(
        name: String,
        imports: [WKBridgeModuleReference] = [],
        typeDefinitions: [WKBridgeTypeDefinition] = [],
        functions: [WKBridgeFunction] = [],
        graphs: [WKBridgeModuleGraph] = []
    ) {
        self.name = name
        self.imports = imports
        self.typeDefinitions = typeDefinitions
        self.functions = functions
        self.graphs = graphs
    }
}

@objc
@implementation
extension WKBridgeModuleReference {
    let module: WKBridgeModule
    let name: String
    let imports: [WKBridgeModuleReference]
    let typeDefinitions: [WKBridgeTypeDefinition]
    let functions: [WKBridgeFunction]

    init(module: WKBridgeModule) {
        self.module = module
        self.name = module.name
        self.imports = module.imports
        self.typeDefinitions = module.typeDefinitions
        self.functions = module.functions
    }
}

@objc
@implementation
extension WKBridgeStructMember {
    let name: String
    let type: WKBridgeTypeReference

    init(name: String, type: WKBridgeTypeReference) {
        self.name = name
        self.type = type
    }
}

@objc
@implementation
extension WKBridgeEnumCase {
    let name: String
    let value: Int

    init(name: String, value: Int) {
        self.name = name
        self.value = value
    }
}

@objc
@implementation
extension WKBridgeTypeDefinition {
    let name: String
    let typeReference: WKBridgeTypeReference
    let structureType: WKBridgeTypeStructure
    let structMembers: [WKBridgeStructMember]?
    let enumCases: [WKBridgeEnumCase]?

    init(
        name: String,
        typeReference: WKBridgeTypeReference,
        structureType: WKBridgeTypeStructure,
        structMembers: [WKBridgeStructMember]?,
        enumCases: [WKBridgeEnumCase]?
    ) {
        self.name = name
        self.typeReference = typeReference
        self.structureType = structureType
        self.structMembers = structMembers
        self.enumCases = enumCases
    }
}

@objc
@implementation
extension WKBridgeFunctionArgument {
    let name: String
    let type: WKBridgeTypeReference

    init(name: String, type: WKBridgeTypeReference) {
        self.name = name
        self.type = type
    }
}

@objc
@implementation
extension WKBridgeFunction {
    let name: String
    let arguments: [WKBridgeFunctionArgument]
    let returnType: WKBridgeTypeReference
    let functionReference: WKBridgeFunctionReference
    let kind: WKBridgeFunctionKind
    let kindName: String

    init(
        name: String,
        arguments: [WKBridgeFunctionArgument],
        returnType: WKBridgeTypeReference,
        functionReference: WKBridgeFunctionReference,
        kind: WKBridgeFunctionKind,
        kindName: String
    ) {
        self.name = name
        self.arguments = arguments
        self.returnType = returnType
        self.functionReference = functionReference
        self.kind = kind
        self.kindName = kindName
    }
}

@objc
@implementation
extension WKBridgeNodeID {
    let value: Int

    init(value: Int) {
        self.value = value
    }
}

@objc
@implementation
extension WKBridgeFunctionCall {
    let type: WKBridgeFunctionCallType
    let name: String?
    let reference: WKBridgeFunctionReference?

    init(name: String) {
        self.type = .name
        self.name = name
        self.reference = nil
    }

    init(reference: WKBridgeFunctionReference) {
        self.type = .reference
        self.name = nil
        self.reference = reference
    }
}

@objc
@implementation
extension WKBridgeNodeInstruction {
    let type: WKBridgeNodeInstructionType
    let functionCall: WKBridgeFunctionCall?
    let constantName: String?
    let literal: WKBridgeLiteral?
    let argumentName: String?
    let elementType: WKBridgeTypeReference?
    let elementName: String?

    init(functionCall: WKBridgeFunctionCall) {
        self.functionCall = functionCall
        self.type = .functionCall

        self.constantName = nil
        self.literal = nil
        self.argumentName = nil
        self.elementType = nil
        self.elementName = nil
    }
    init(functionConstant: String, literal: WKBridgeLiteral) {
        self.constantName = functionConstant
        self.literal = literal
        self.type = .functionConstant

        self.functionCall = nil
        self.argumentName = nil
        self.elementType = nil
        self.elementName = nil
    }
    init(literal: WKBridgeLiteral) {
        self.literal = literal
        self.type = .literal

        self.functionCall = nil
        self.constantName = nil
        self.argumentName = nil
        self.elementType = nil
        self.elementName = nil
    }
    init(argument: String) {
        self.argumentName = argument
        self.type = .argument

        self.functionCall = nil
        self.constantName = nil
        self.literal = nil
        self.elementType = nil
        self.elementName = nil
    }
    init(elementType: WKBridgeTypeReference, elementName: String) {
        self.elementType = elementType
        self.elementName = elementName
        self.type = .element

        self.functionCall = nil
        self.constantName = nil
        self.literal = nil
        self.argumentName = nil
    }
}

@objc
@implementation
extension WKBridgeArgumentError {
    let message: String
    let argument: WKBridgeFunctionArgument

    init(message: String, argument: WKBridgeFunctionArgument) {
        self.message = message
        self.argument = argument
    }
}

@objc
@implementation
extension WKBridgeNode {
    let nodeID: WKBridgeNodeID
    let instruction: WKBridgeNodeInstruction

    init(
        identifier: WKBridgeNodeID,
        instruction: WKBridgeNodeInstruction
    ) {
        self.nodeID = identifier
        self.instruction = instruction
    }
}

@objc
@implementation
extension WKBridgeGraphEdge {
    let source: WKBridgeNodeID
    let destination: WKBridgeNodeID
    let argument: String

    init(
        source: WKBridgeNodeID,
        destination: WKBridgeNodeID,
        argument: String
    ) {
        self.source = source
        self.destination = destination
        self.argument = argument
    }
}

@objc
@implementation
extension WKBridgeModuleGraph {
    let functionReference: WKBridgeFunctionReference
    let name: String
    let arguments: [WKBridgeFunctionArgument]
    let returnType: WKBridgeTypeReference
    let nodes: [WKBridgeNode]
    let edges: [WKBridgeGraphEdge]
    let index: Int

    init(index: Int, function: WKBridgeFunction) {
        self.index = index
        self.functionReference = function.functionReference
        self.name = function.name
        self.arguments = function.arguments
        self.returnType = function.returnType
        self.nodes = []
        self.edges = []
    }
}

@objc
@implementation
extension WKBridgeUpdateMaterial {
    let materialGraph: Data?
    let identifier: String
    let geometryModifierFunctionReference: WKBridgeFunctionReference?
    let surfaceShaderFunctionReference: WKBridgeFunctionReference?
    let shaderGraphModule: WKBridgeModule?

    init(
        materialGraph: Data?,
        identifier: String,
        geometryModifierFunctionReference: WKBridgeFunctionReference?,
        surfaceShaderFunctionReference: WKBridgeFunctionReference?,
        shaderGraphModule: WKBridgeModule?
    ) {
        self.materialGraph = materialGraph
        self.identifier = identifier
        self.geometryModifierFunctionReference = geometryModifierFunctionReference
        self.surfaceShaderFunctionReference = surfaceShaderFunctionReference
        self.shaderGraphModule = shaderGraphModule
    }
}

@objc
@implementation
extension WKBridgeInputOutput {
    let type: WKBridgeDataType
    let name: String

    init(
        type: WKBridgeDataType,
        name: String
    ) {
        self.type = type
        self.name = name
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
    let upstreamNodeIndex: Int
    let downstreamNodeIndex: Int
    let upstreamOutputName: String
    let downstreamInputName: String

    init(
        upstreamNodeIndex: Int,
        downstreamNodeIndex: Int,
        upstreamOutputName: String,
        downstreamInputName: String
    ) {
        self.upstreamNodeIndex = upstreamNodeIndex
        self.downstreamNodeIndex = downstreamNodeIndex
        self.upstreamOutputName = upstreamOutputName
        self.downstreamInputName = downstreamInputName
    }
}

@objc
@implementation
extension WKBridgeValueString {
    let number: NSNumber
    let string: String

    init(
        string: String
    ) {
        self.number = NSNumber(value: 0)
        self.string = string
    }

    init(
        number: NSNumber
    ) {
        self.number = number
        self.string = ""
    }
}

@objc
@implementation
extension WKBridgeLiteralArchive {
    let type: WKBridgeLiteralType
    let data: [NSNumber]

    init(
        type: WKBridgeLiteralType,
        data: [NSNumber]
    ) {
        self.type = type
        self.data = data
    }
}

@objc
@implementation
extension WKBridgeLiteral {
    let type: WKBridgeLiteralType
    let archive: WKBridgeLiteralArchive

    init(type: WKBridgeLiteralType, data: [NSNumber]) {
        self.type = type
        self.archive = .init(type: type, data: data)
    }
}

#if canImport(RealityCoreRenderer, _version: 9) && os(macOS)

internal func toData<T>(_ input: [T]) -> Data {
#if compiler(>=6.2)
    unsafe input.withUnsafeBytes { bufferPointer in
        Data(bufferPointer)
    }
#else
    input.withUnsafeBytes { bufferPointer in
        Data(bufferPointer)
    }
#endif
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

#if compiler(>=6.2)
        return unsafe data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = unsafe baseAddress.assumingMemoryBound(to: simd_float4x4.self)
            return (0..<jointTransformsCount).map { unsafe matrices[$0] }
        }
#else
        return data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = baseAddress.assumingMemoryBound(to: simd_float4x4.self)
            return (0..<jointTransformsCount).map { matrices[$0] }
        }
#endif
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

#if compiler(>=6.2)
        return unsafe data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = unsafe baseAddress.assumingMemoryBound(to: simd_float4x4.self)
            return (0..<inverseBindPosesCount).map { unsafe matrices[$0] }
        }
#else
        return data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = baseAddress.assumingMemoryBound(to: simd_float4x4.self)
            return (0..<inverseBindPosesCount).map { matrices[$0] }
        }
#endif
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

#if compiler(>=6.2)
        return unsafe data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = unsafe baseAddress.assumingMemoryBound(to: UInt32.self)
            return (0..<influenceJointIndicesCount).map { unsafe matrices[$0] }
        }
#else
        return data.withUnsafeBytes { rawBufferPointer in
            guard let baseAddress = rawBufferPointer.baseAddress else {
                return []
            }

            let matrices = baseAddress.assumingMemoryBound(to: UInt32.self)
            return (0..<influenceJointIndicesCount).map { matrices[$0] }
        }
#endif
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
