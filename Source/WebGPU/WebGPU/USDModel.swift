// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if canImport(RealityCoreRenderer)
import Metal
import WebGPU_Internal

@_spi(RealityCoreRendererAPI) @_spi(ShaderGraph) import RealityCoreRenderer
@_spi(RealityCoreRendererAPI) @_spi(ShaderGraph) import RealityKit
@_spi(UsdLoaderAPI) import USDStageKit
@_spi(UsdLoaderAPI) import _USDStageKit_SwiftUI
@_spi(SwiftAPI) import DirectResource
import USDStageKit
import _USDStageKit_SwiftUI
import RealityKit
import ShaderGraph

@objc
@implementation
extension DDBridgeVertexAttributeFormat {
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
extension DDBridgeVertexLayout {
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
extension DDBridgeMeshPart {
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
extension DDBridgeMeshDescriptor {
    let vertexBufferCount: Int
    let vertexCapacity: Int
    let vertexAttributes: [DDBridgeVertexAttributeFormat]
    let vertexLayouts: [DDBridgeVertexLayout]
    let indexCapacity: Int
    let indexType: MTLIndexType

    init(
        vertexBufferCount: Int,
        vertexCapacity: Int,
        vertexAttributes: [DDBridgeVertexAttributeFormat],
        vertexLayouts: [DDBridgeVertexLayout],
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
extension DDBridgeChainedFloat4x4 {
    var transform: simd_float4x4
    var next: DDBridgeChainedFloat4x4?

    init(
        transform: simd_float4x4
    ) {
        self.transform = transform
    }
}

@objc
@implementation
extension DDBridgeUpdateMesh {
    let identifier: String
    let updateType: DDBridgeDataUpdateType
    let descriptor: DDBridgeMeshDescriptor?
    let parts: [DDBridgeMeshPart]
    let indexData: Data?
    let vertexData: [Data]
    var instanceTransforms: DDBridgeChainedFloat4x4? // array of float4x4
    let instanceTransformsCount: Int
    let materialPrims: [String]

    init(
        identifier: String,
        updateType: DDBridgeDataUpdateType,
        descriptor: DDBridgeMeshDescriptor?,
        parts: [DDBridgeMeshPart],
        indexData: Data?,
        vertexData: [Data],
        instanceTransforms: DDBridgeChainedFloat4x4?,
        instanceTransformsCount: Int,
        materialPrims: [String]
    ) {
        self.identifier = identifier
        self.updateType = updateType
        self.descriptor = descriptor
        self.parts = parts
        self.indexData = indexData
        self.vertexData = vertexData
        self.instanceTransforms = instanceTransforms
        self.instanceTransformsCount = instanceTransformsCount
        self.materialPrims = materialPrims
    }
}

@objc
@implementation
extension DDBridgeImageAsset {
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
extension DDBridgeUpdateTexture {
    let imageAsset: DDBridgeImageAsset?
    let identifier: String
    let hashString: String

    init(
        imageAsset: DDBridgeImageAsset?,
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
extension DDBridgeUpdateMaterial {
    let materialGraph: Data?
    let identifier: String

    init(
        materialGraph: Data?,
        identifier: String
    ) {
        self.materialGraph = materialGraph
        self.identifier = identifier
    }
}

@objc
@implementation
extension DDBridgeNode {
    let bridgeNodeType: DDBridgeNodeType
    let builtin: DDBridgeBuiltin
    let constant: DDBridgeConstantContainer

    init(
        bridgeNodeType: DDBridgeNodeType,
        builtin: DDBridgeBuiltin,
        constant: DDBridgeConstantContainer
    ) {
        self.bridgeNodeType = bridgeNodeType
        self.builtin = builtin
        self.constant = constant
    }
}

@objc
@implementation
extension DDBridgeInputOutput {
    let type: DDBridgeDataType
    let name: String

    init(
        type: DDBridgeDataType,
        name: String
    ) {
        self.type = type
        self.name = name
    }
}

@objc
@implementation
extension DDBridgeConstantContainer {
    let constant: DDBridgeConstant
    let constantValues: [DDValueString]
    let name: String

    init(
        constant: DDBridgeConstant,
        constantValues: [DDValueString],
        name: String
    ) {
        self.constant = constant
        self.constantValues = constantValues
        self.name = name
    }
}

@objc
@implementation
extension DDBridgeBuiltin {
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
extension DDBridgeEdge {
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
extension DDValueString {
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

extension MTLCaptureDescriptor {
    fileprivate convenience init(from device: MTLDevice?) {
        self.init()

        captureObject = device
        destination = .gpuTraceDocument
        let now = Date()
        let dateFormatter = DateFormatter()
        dateFormatter.timeZone = .current
        dateFormatter.dateFormat = "yyyy-MM-dd-HH-mm-ss-SSSS"
        let dateString = dateFormatter.string(from: now)

        outputURL = URL.temporaryDirectory.appending(path: "capture_\(dateString).gputrace").standardizedFileURL
    }
}

nonisolated func mapSemantic(_ semantic: Int) -> _Proto_LowLevelMeshResource_v1.VertexSemantic {
    switch semantic {
    case 0: return .position
    case 1: return .color
    case 2: return .normal
    case 3: return .tangent
    case 4: return .bitangent
    case 5: return .uv0
    case 6: return .uv1
    case 7: return .uv2
    case 8: return .uv3
    case 9: return .uv4
    case 10: return .uv5
    case 11: return .uv6
    case 12: return .uv7
    default:
        return .unspecified
    }
}

extension _Proto_LowLevelMeshResource_v1.Descriptor {
    nonisolated static func fromLlmDescriptor(_ llmDescriptor: DDBridgeMeshDescriptor) -> Self {
        var descriptor = Self.init()
        descriptor.vertexCapacity = Int(llmDescriptor.vertexCapacity)
        descriptor.vertexAttributes = llmDescriptor.vertexAttributes.map { attribute in
            .init(
                semantic: mapSemantic(attribute.semantic),
                format: MTLVertexFormat(rawValue: UInt(attribute.format)) ?? .invalid,
                layoutIndex: attribute.layoutIndex,
                offset: attribute.offset
            )
        }
        descriptor.vertexLayouts = llmDescriptor.vertexLayouts.map { layout in
            .init(bufferIndex: layout.bufferIndex, bufferOffset: layout.bufferOffset, bufferStride: layout.bufferStride)
        }
        descriptor.indexCapacity = llmDescriptor.indexCapacity
        descriptor.indexType = llmDescriptor.indexType

        return descriptor
    }
}

extension _Proto_LowLevelTextureResource_v1.Descriptor {
    static func from(_ texture: MTLTexture) -> _Proto_LowLevelTextureResource_v1.Descriptor {
        var descriptor = _Proto_LowLevelTextureResource_v1.Descriptor()
        descriptor.width = texture.width
        descriptor.height = texture.height
        descriptor.depth = texture.depth
        descriptor.mipmapLevelCount = texture.mipmapLevelCount
        descriptor.arrayLength = texture.arrayLength
        descriptor.pixelFormat = texture.pixelFormat
        descriptor.textureType = texture.textureType
        descriptor.textureUsage = texture.usage
        descriptor.swizzle = texture.swizzle

        return descriptor
    }
}

class Helper {
    static fileprivate func isNonZero(value: Float) -> Bool {
        abs(value) > Float.ulpOfOne
    }

    static fileprivate func isNonZero(_ vector: simd_float4) -> Bool {
        isNonZero(value: vector[0]) || isNonZero(value: vector[1]) || isNonZero(value: vector[2]) || isNonZero(value: vector[3])
    }

    static fileprivate func isNonZero(matrix: simd_float4x4) -> Bool {
        isNonZero(_: matrix.columns.0) || isNonZero(_: matrix.columns.1) || isNonZero(_: matrix.columns.2) || isNonZero(_: matrix.columns.3)
    }

    static fileprivate func makeTextureFromImageAsset(
        _ imageAsset: DDBridgeImageAsset,
        device: MTLDevice,
        context: _Proto_LowLevelResourceContext_v1,
        commandQueue: MTLCommandQueue
    ) -> _Proto_LowLevelTextureResource_v1? {
        guard let imageAssetData = imageAsset.data else {
            logError("no image data")
            return nil
        }
        do {
            logError(
                "imageAssetData = \(imageAssetData)  -  width = \(imageAsset.width)  -  height = \(imageAsset.height)  -  bytesPerPixel = \(imageAsset.bytesPerPixel) imageAsset.pixelFormat:  \(imageAsset.pixelFormat)"
            )

            var pixelFormat = imageAsset.pixelFormat
            switch imageAsset.bytesPerPixel {
            case 1:
                pixelFormat = .r8Unorm
            case 2:
                pixelFormat = .rg8Unorm
            case 4:
                pixelFormat = .rgba8Unorm
            default:
                pixelFormat = .rgba8Unorm
            }
            let textureDescriptor = MTLTextureDescriptor.texture2DDescriptor(
                pixelFormat: pixelFormat,
                width: imageAsset.width,
                height: imageAsset.height,
                mipmapped: false
            )

            guard let mtlTexture = device.makeTexture(descriptor: textureDescriptor) else {
                logError("failed to device.makeTexture")
                return nil
            }

            try unsafe imageAssetData.bytes.withUnsafeBytes { textureBytes in
                guard let textureBytesBaseAddress = textureBytes.baseAddress else {
                    return
                }
                if imageAsset.bytesPerPixel == 0 {
                    logError("bytesPerPixel == 0")
                    fatalError()
                }
                unsafe mtlTexture.replace(
                    region: MTLRegionMake2D(0, 0, imageAsset.width, imageAsset.height),
                    mipmapLevel: 0,
                    withBytes: textureBytesBaseAddress,
                    bytesPerRow: imageAsset.width * imageAsset.bytesPerPixel
                )
            }

            guard let commandBuffer = commandQueue.makeCommandBuffer() else {
                fatalError("Could not create command buffer")
            }
            guard let blitEncoder = commandBuffer.makeBlitCommandEncoder() else {
                fatalError("Could not create blit encoder")
            }
            let descriptor = _Proto_LowLevelTextureResource_v1.Descriptor.from(mtlTexture)
            if let textureResource = try? context.makeTextureResource(descriptor: descriptor) {
                let outTexture = textureResource.replace(using: commandBuffer)
                blitEncoder.copy(from: mtlTexture, to: outTexture)

                blitEncoder.endEncoding()
                commandBuffer.commit()
                return textureResource
            }
        } catch {
            logError("failed to make texture from image \(error)")
        }

        return nil
    }

    static fileprivate func makeParameters(
        parameterMapping: _Proto_LowLevelMaterialParameterMapping_v1?,
        argumentTableDescriptor: _Proto_LowLevelArgumentTable_v1.Descriptor,
        textureResources: [String: _Proto_LowLevelTextureResource_v1] = [:],
        resourceContext: _Proto_LowLevelResourceContext_v1
    ) throws
        -> (buffers: [_Proto_LowLevelBufferResource_v1], textures: [_Proto_LowLevelTextureResource_v1])
    {
        var optTextures: [_Proto_LowLevelTextureResource_v1?] = argumentTableDescriptor.textures.map({ _ in nil })
        for parameter in parameterMapping?.textures ?? [] {
            guard let textureResource = textureResources[parameter.name] else {
                fatalError("Failed to find texture resource \(parameter.name)")
            }
            optTextures[parameter.textureIndex] = textureResource
        }
        // FIXME: remove placeholder code
        // swift-format-ignore: NeverForceUnwrap
        let textures = optTextures.map({ $0! })

        let buffers: [_Proto_LowLevelBufferResource_v1] = try argumentTableDescriptor.buffers.map { bufferRequirements in
            let capacity = (bufferRequirements.size + 16 - 1) / 16 * 16
            let buffer = try resourceContext.makeBufferResource(descriptor: .init(capacity: capacity))
            buffer.replace { span in
                for byteOffset in span.byteOffsets {
                    span.storeBytes(of: 0, toByteOffset: byteOffset, as: UInt8.self)
                }
            }
            return buffer
        }

        return (buffers: buffers, textures: textures)
    }
}

extension Logger {
    fileprivate static let modelGPU = Logger(subsystem: "com.apple.WebKit", category: "model")
}

nonisolated func logError(_ error: String) {
    Logger.modelGPU.error("\(error)")
}

nonisolated func logInfo(_ info: String) {
    Logger.modelGPU.info("\(info)")
}

extension _Proto_LowLevelMeshResource_v1 {
    nonisolated func replaceVertexData(_ vertexData: [Data]) {
        for (vertexBufferIndex, vertexData) in vertexData.enumerated() {
            let bufferSizeInByte = vertexData.bytes.byteCount
            self.replaceVertices(at: vertexBufferIndex) { vertexBytes in
                unsafe vertexBytes.withUnsafeMutableBytes { ptr in
                    guard let baseAddress = ptr.baseAddress else {
                        return
                    }
                    unsafe vertexData.copyBytes(to: baseAddress.assumingMemoryBound(to: UInt8.self), count: bufferSizeInByte)
                }
            }
        }
    }

    nonisolated func replaceIndexData(_ indexData: Data?) {
        if let indexData = indexData {
            self.replaceIndices { indicesBytes in
                unsafe indicesBytes.withUnsafeMutableBytes { ptr in
                    guard let baseAddress = ptr.baseAddress else {
                        return
                    }
                    unsafe indexData.copyBytes(to: baseAddress.assumingMemoryBound(to: UInt8.self), count: ptr.count)
                }
            }
        }
    }

    nonisolated func replaceData(replaceParts: [DDBridgeMeshPart], replaceIndexData: Data?, replaceVertexData: [Data]) {
        let partCount = replaceParts.count

        // Copy mesh parts
        self.modifyParts { parts in
            // why are there 8 parts?
            for i in 0..<partCount {
                let part = replaceParts[i]
                parts[i] = .init(
                    indexOffset: part.indexOffset,
                    indexCount: part.indexCount,
                    primitive: part.topology,
                    windingOrder: .counterClockwise,
                    boundsMin: part.boundsMin,
                    boundsMax: part.boundsMax
                )
            }
        }

        // Copy index data
        self.replaceIndexData(replaceIndexData)

        // Copy vertex data
        self.replaceVertexData(replaceVertexData)
    }
}

private class RenderTargetWrapper {
    let descriptor: _Proto_LowLevelRenderTarget_v1.Descriptor
    init(_ descriptor: _Proto_LowLevelRenderTarget_v1.Descriptor) {
        self.descriptor = descriptor
    }
}

private class MeshesWrapper {
    var meshes: _Proto_LowLevelMeshInstanceSet_v1
    init(_ meshes: _Proto_LowLevelMeshInstanceSet_v1) {
        self.meshes = meshes
    }
}

private struct Material {
    fileprivate let resource: _Proto_LowLevelMaterialResource_v1
    fileprivate let geometryParameters: _Proto_LowLevelArgumentTable_v1
    fileprivate let surfaceParameters: _Proto_LowLevelArgumentTable_v1
}

@objc
@implementation
extension DDBridgeReceiver {
    fileprivate let device: MTLDevice

    @nonobjc
    fileprivate let resourceContext: _Proto_LowLevelResourceContext_v1
    @nonobjc
    fileprivate var materialCompiler: _Proto_LowLevelMaterialCompiler_v1?
    @nonobjc
    fileprivate let renderTarget: RenderTargetWrapper
    @nonobjc
    fileprivate var meshContainer: MeshesWrapper
    @nonobjc
    fileprivate var meshInstances: [_Proto_ResourceId: [_Proto_LowLevelMeshInstance_v1]] = [:]
    @nonobjc
    fileprivate var meshTransforms: [_Proto_ResourceId: [simd_float4x4]] = [:]
    @nonobjc
    fileprivate var meshResources: [String: _Proto_LowLevelMeshResource_v1] = [:]
    @nonobjc
    fileprivate var camera: _Proto_LowLevelCamera_v1?
    @nonobjc
    fileprivate let renderer: _Proto_LowLevelRenderer_v1
    @nonobjc
    fileprivate var meshResourceToMaterials: [_Proto_ResourceId: [_Proto_ResourceId]] = [:]
    @nonobjc
    fileprivate var materialsAndParams: [_Proto_ResourceId: Material] = [:]
    @nonobjc
    fileprivate var textureResources: [String: _Proto_LowLevelTextureResource_v1] = [:]
    @nonobjc
    fileprivate var textureData: [_Proto_ResourceId: (MTLTexture, String)] = [:]
    @nonobjc
    fileprivate let commandQueue: MTLCommandQueue?
    @nonobjc
    fileprivate let captureManager: MTLCaptureManager
    @nonobjc
    fileprivate var textureFromPath: [String: String] = [:]

    @nonobjc
    fileprivate var modelTransform: simd_float4x4
    @nonobjc
    fileprivate var modelDistance: Float

    init(
        device: MTLDevice
    ) {
        self.device = device
        modelTransform = matrix_identity_float4x4
        modelDistance = 1.0

        resourceContext = _Proto_LowLevelResourceContext_v1(device: device)
        self.renderTarget = RenderTargetWrapper(_Proto_LowLevelRenderTarget_v1.Descriptor.texture(color: .bgra8Unorm, sampleCount: 4))
        self.meshContainer = MeshesWrapper(_Proto_LowLevelMeshInstanceSet_v1(renderTarget: renderTarget.descriptor))
        commandQueue = device.makeCommandQueue()
        renderer = try _Proto_LowLevelRenderer_v1(configuration: .init(device: device, commandQueue: commandQueue))
        captureManager = MTLCaptureManager.shared()
    }

    @objc(initRenderer:completionHandler:)
    func initRenderer(_ texture: MTLTexture) async {
        do {
            materialCompiler = try await _Proto_LowLevelMaterialCompiler_v1(device: device)
        } catch {
            logError("failed to create renderer")
        }
    }

    @objc(renderWithTexture:)
    func render(with texture: MTLTexture) {
        let instancesAndTransforms = meshInstances.flatMap(\.value)

        // Add instances to scene
        for instance in instancesAndTransforms {
            do {
                try self.meshContainer.meshes.insert(instance)
            } catch {
                logError("could not insert instance")
            }
        }

        for (identifier, meshInstanceArray) in meshInstances {
            let meshTransformsArray = meshTransforms[identifier]
            for (instanceIndex, instance) in meshInstanceArray.enumerated() {
                guard let originalTransform = meshTransformsArray?[instanceIndex] else { fatalError("Transform missing but mesh present") }
                let rotationY90 = simd_float4x4(
                    simd_float4(0, 0, 1, 0), // column 0
                    simd_float4(0, 1, 0, 0), // column 1
                    simd_float4(-1, 0, 0, 0), // column 2
                    simd_float4(0, 0, 0, 1) // column 3
                )
                instance.transform = .single(modelTransform * rotationY90 * originalTransform)
            }
        }

        // render
        let captureDescriptor = MTLCaptureDescriptor(from: device)
        do {
            try captureManager.startCapture(with: captureDescriptor)
            print("Capture started at \(captureDescriptor.outputURL?.absoluteString ?? "")")
        } catch {
            logError("failed to start gpu capture \(error)")
        }

        let aspect = Float(texture.width) / Float(texture.height)
        let projection = _Proto_LowLevelCamera_v1.Projection.perspective(
            fovYRadians: 90 * .pi / 180,
            aspectRatio: aspect,
            nearZ: 0.01,
            farZ: 100.0,
            reverseZ: true
        )

        if camera == nil {
            camera = _Proto_LowLevelCamera_v1(
                target: .texture(color: texture),
                pose: .init(
                    translation: [modelDistance, 0, 0],
                    rotation: simd_quatf(vector: simd_make_float4(0, .pi / 4, 0, .pi / 4))
                ),
                projection: projection
            )
        }
        guard let camera else { fatalError("Camera not initialized") }

        camera.target = .texture(color: texture, sampleCount: 4)
        camera.projection = projection
        camera.pose = .init(
            translation: [modelDistance, 0, 0],
            rotation: simd_quatf(vector: simd_make_float4(0, .pi / 4, 0, .pi / 4))
        )

        do {
            let content = try _Proto_LowLevelRenderer_v1.Content(meshes: meshContainer.meshes, camera: camera)
            renderer.render(content)
        } catch {
            logInfo("failed to render scene \(error)")
        }

        captureManager.stopCapture()
    }

    @objc(updateTexture:completionHandler:)
    func updateTexture(_ data: DDBridgeUpdateTexture) async {
        do {
            guard let asset = data.imageAsset else {
                logError("Image asset was nil")
                return
            }

            let textureHash = data.hashString
            if textureResources[textureHash] != nil {
                logError("Texture already exists")
                return
            }

            if let commandQueue {
                if let newTexture = try Helper.makeTextureFromImageAsset(
                    asset,
                    device: device,
                    context: resourceContext,
                    commandQueue: commandQueue
                ) {
                    textureResources[textureHash] = newTexture
                }
            }
        } catch {
            return
        }
    }

    @objc(updateMaterial:completionHandler:)
    func updateMaterial(_ data: DDBridgeUpdateMaterial) async {
        logInfo("updateMaterial (pre-dispatch) \(data.identifier)")
        do {
            let identifier = data.identifier
            logInfo("updateMaterial \(identifier)")
            let materialSourceArchive = data.materialGraph
            let materialSource = try ShaderGraphService.sourceFromArchive(data: materialSourceArchive)
            guard let materialCompiler = self.materialCompiler else {
                fatalError("materialCompiler is not initialized")
            }
            let sgMaterial = try ShaderGraphService.materialFromSource(materialSource, functionConstantValues: .init())

            logInfo("before await \(identifier)")

            let materialResource = try await materialCompiler.makeShaderGraphMaterial(sgMaterial)
            logInfo("after await  \(identifier)")
            let (geometryBuffers, geometryTextures) = try Helper.makeParameters(
                parameterMapping: materialResource.geometry.parameterMapping,
                argumentTableDescriptor: materialResource.geometry.argumentTableDescriptor,
                textureResources: self.textureResources,
                resourceContext: self.resourceContext
            )
            let (surfaceBuffers, surfaceTextures) = try Helper.makeParameters(
                parameterMapping: materialResource.surface.parameterMapping,
                argumentTableDescriptor: materialResource.surface.argumentTableDescriptor,
                textureResources: self.textureResources,
                resourceContext: self.resourceContext
            )

            let geometryParameters = try resourceContext.makeArgumentTable(
                function: materialResource.geometry,
                buffers: geometryBuffers,
                textures: geometryTextures
            )
            let surfaceParameters = try resourceContext.makeArgumentTable(
                function: materialResource.surface,
                buffers: surfaceBuffers,
                textures: surfaceTextures
            )

            logInfo("inserting \(identifier) into materialsAndParams")
            materialsAndParams[identifier] = .init(
                resource: materialResource,
                geometryParameters: geometryParameters,
                surfaceParameters: surfaceParameters
            )
        } catch {
            logError("updateMaterial failed \(error)")
        }
    }

    @objc(updateMesh:completionHandler:)
    func updateMesh(_ data: DDBridgeUpdateMesh) async {
        logError("(update mesh) Material ids \(data.materialPrims)")
        do {
            let identifier = data.identifier

            let meshResource: _Proto_LowLevelMeshResource_v1
            if let meshDescriptor = data.descriptor {
                let descriptor = _Proto_LowLevelMeshResource_v1.Descriptor.fromLlmDescriptor(meshDescriptor)
                meshResource = try resourceContext.makeMeshResource(descriptor: descriptor)
                try meshResource.setPartCount(data.parts.count)
                meshResource.replaceData(replaceParts: data.parts, replaceIndexData: data.indexData, replaceVertexData: data.vertexData)
            } else {
                guard let cachedMeshResource = meshResources[identifier] else {
                    fatalError("Mesh resource should already be created from previous update")
                }

                if !data.parts.isEmpty || data.indexData != nil || !data.vertexData.isEmpty {
                    cachedMeshResource.replaceData(
                        replaceParts: data.parts,
                        replaceIndexData: data.indexData,
                        replaceVertexData: data.vertexData
                    )
                }
                meshResource = cachedMeshResource
            }

            if data.instanceTransformsCount > 0 {
                // Make new instances
                if meshInstances[identifier] == nil {
                    meshInstances[identifier] = []
                    meshTransforms[identifier] = []

                    for (partIndex, _) in data.parts.enumerated() {
                        let materialIdentifier = data.materialPrims[partIndex]
                        guard let material = materialsAndParams[materialIdentifier] else {
                            fatalError("Material was nil for \(materialIdentifier)")
                        }

                        logInfo("Creating material for \(materialIdentifier)")
                        logInfo("try makeRenderPipelineState \(materialIdentifier)")
                        let pipeline = try await self.materialCompiler?
                            .makeRenderPipelineState(
                                descriptor: .descriptor(
                                    mesh: meshResource.descriptor,
                                    material: material.resource,
                                    renderTarget: self.renderTarget.descriptor
                                )
                            )

                        guard let pipeline else {
                            fatalError("Pipeline failed to create")
                        }
                        var instanceTransformsPointer = data.instanceTransforms
                        while instanceTransformsPointer != nil {
                            // swift-format-ignore: NeverForceUnwrap
                            let instanceTransform = instanceTransformsPointer!.transform
                            let meshInstance = try _Proto_LowLevelMeshInstance_v1(
                                resource: meshResource,
                                pipeline: pipeline,
                                geometryParameters: material.geometryParameters,
                                surfaceParameters: material.surfaceParameters,
                                partIndex: partIndex,
                                transform: .single(instanceTransform)
                            )
                            // FIXME: remove placeholder code
                            // swift-format-ignore: NeverForceUnwrap
                            self.meshInstances[identifier]!.append(meshInstance)
                            // FIXME: remove placeholder code
                            // swift-format-ignore: NeverForceUnwrap
                            self.meshTransforms[identifier]!.append(instanceTransform)
                            instanceTransformsPointer = instanceTransformsPointer?.next
                        }
                        logInfo("updateMaterial for \(materialIdentifier) completed")
                    }
                } else {
                    // Update transforms otherwise
                    // swift-format-ignore: NeverForceUnwrap
                    let partCount = meshInstances[identifier]!.count / data.instanceTransformsCount
                    var instanceTransformsPointer = data.instanceTransforms
                    var instanceIndex: Int = 0
                    while instanceTransformsPointer != nil {
                        let instanceTransform = instanceTransformsPointer?.transform
                        for partIndex in 0..<partCount {
                            // FIXME: remove placeholder force unwrapping
                            // swift-format-ignore: NeverForceUnwrap
                            meshInstances[identifier]![instanceIndex * data.parts.count + partIndex].transform = .single(instanceTransform!)
                            // swift-format-ignore: NeverForceUnwrap
                            meshTransforms[identifier]![instanceIndex * data.parts.count + partIndex] = instanceTransform!
                        }
                        instanceTransformsPointer = instanceTransformsPointer?.next
                        instanceIndex += 1
                    }
                }
            }
        } catch {
            fatalError(error.localizedDescription)
        }
    }

    @objc(setTransform:)
    func setTransform(_ transform: simd_float4x4) {
        modelTransform = transform
    }

    @objc
    func setCameraDistance(_ distance: Float) {
        modelDistance = distance
    }

    @objc
    func setPlaying(_ play: Bool) {
        // resourceContext.setEnableModelRotation(play)
    }
}

final class Converter {
    static func toWebImageAsset(_ asset: LowLevelTexture.Descriptor, data: Data) -> DDBridgeImageAsset {
        DDBridgeImageAsset(
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

    static func convertSemantic(_ semantic: LowLevelMesh.VertexSemantic) -> Int {
        switch semantic {
        case .position: return 0
        case .color: return 1
        case .normal: return 2
        case .tangent: return 3
        case .bitangent: return 4
        case .uv0: return 5
        case .uv1: return 6
        case .uv2: return 7
        case .uv3: return 8
        case .uv4: return 9
        case .uv5: return 10
        case .uv6: return 11
        case .uv7: return 12
        default: return 13
        }
    }

    static func webAttributesFromAttributes(_ attributes: [LowLevelMesh.Attribute]) -> [DDBridgeVertexAttributeFormat] {
        attributes.map({ a in
            DDBridgeVertexAttributeFormat(
                semantic: Converter.convertSemantic(a.semantic),
                format: a.format.rawValue,
                layoutIndex: a.layoutIndex,
                offset: a.offset
            )
        })
    }

    static func webLayoutsFromLayouts(_ attributes: [LowLevelMesh.Layout]) -> [DDBridgeVertexLayout] {
        attributes.map({ a in
            DDBridgeVertexLayout(bufferIndex: a.bufferIndex, bufferOffset: a.bufferOffset, bufferStride: a.bufferStride)
        })
    }

    static func webMeshDescriptorFromMeshDescriptor(_ request: LowLevelMesh.Descriptor) -> DDBridgeMeshDescriptor {
        DDBridgeMeshDescriptor(
            vertexBufferCount: request.vertexBufferCount,
            vertexCapacity: request.vertexCapacity,
            vertexAttributes: Converter.webAttributesFromAttributes(request.vertexAttributes),
            vertexLayouts: Converter.webLayoutsFromLayouts(request.vertexLayouts),
            indexCapacity: request.indexCapacity,
            indexType: request.indexType
        )
    }

    static func webPartsFromParts(_ parts: [LowLevelMesh.Part]) -> [DDBridgeMeshPart] {
        parts.map({ a in
            DDBridgeMeshPart(
                indexOffset: a.indexOffset,
                indexCount: a.indexCount,
                topology: a.topology,
                materialIndex: a.materialIndex,
                boundsMin: a.bounds.min,
                boundsMax: a.bounds.max
            )
        })
    }

    static func convert(_ m: _Proto_DataUpdateType_v1) -> DDBridgeDataUpdateType {
        if m == .initial {
            return .initial
        }
        return .delta
    }

    static func webUpdateTextureRequestFromUpdateTextureRequest(_ request: _Proto_TextureDataUpdate_v1) -> DDBridgeUpdateTexture {
        // FIXME: remove placeholder code
        // swift-format-ignore: NeverForceUnwrap
        let descriptor = request.descriptor!
        // swift-format-ignore: NeverForceUnwrap
        let data = request.data!
        return DDBridgeUpdateTexture(
            imageAsset: toWebImageAsset(descriptor, data: data),
            identifier: request.identifier,
            hashString: request.hashString
        )
    }

    static func webUpdateMeshRequestFromUpdateMeshRequest(
        _ request: _Proto_MeshDataUpdate_v1
    ) -> DDBridgeUpdateMesh {
        var webRequestInstanceTransforms: DDBridgeChainedFloat4x4?

        if request.instanceTransforms.count > 0 {
            let countMinusOne = request.instanceTransforms.count - 1
            webRequestInstanceTransforms = DDBridgeChainedFloat4x4(transform: request.instanceTransforms[0])
            var instanceTransforms = webRequestInstanceTransforms
            if countMinusOne > 0 {
                for i in 1...countMinusOne {
                    instanceTransforms?.next = DDBridgeChainedFloat4x4(transform: request.instanceTransforms[i])
                    instanceTransforms = instanceTransforms?.next
                }
            }
        }

        var descriptor: DDBridgeMeshDescriptor?
        if let requestDescriptor = request.descriptor {
            descriptor = Converter.webMeshDescriptorFromMeshDescriptor(requestDescriptor)
        }

        return DDBridgeUpdateMesh(
            identifier: request.identifier,
            updateType: Converter.convert(request.updateType),
            descriptor: descriptor,
            parts: Converter.webPartsFromParts(request.parts),
            indexData: request.indexData,
            vertexData: request.vertexData,
            instanceTransforms: webRequestInstanceTransforms,
            instanceTransformsCount: request.instanceTransformsCount(),
            materialPrims: request.materialPrims
        )
    }

    static func webUpdateMaterialRequestFromUpdateMaterialRequest(
        _ request: _Proto_MaterialDataUpdate_v1
    ) -> DDBridgeUpdateMaterial {
        DDBridgeUpdateMaterial(materialGraph: request.materialSourceArchive, identifier: request.identifier)
    }
}

final class USDModelLoader: _Proto_UsdStageSession_v1.Delegate {
    fileprivate let usdLoader: _Proto_UsdStageSession_v1
    private let objcLoader: DDBridgeModelLoader

    @nonobjc
    private let dispatchSerialQueue: DispatchSerialQueue

    @nonobjc
    fileprivate var time: TimeInterval = 0

    @nonobjc
    fileprivate var startTime: TimeInterval = 0
    @nonobjc
    fileprivate var endTime: TimeInterval = 1
    @nonobjc
    fileprivate var timeCodePerSecond: TimeInterval = 1

    init(objcInstance: DDBridgeModelLoader) {
        objcLoader = objcInstance
        usdLoader = .init()
        dispatchSerialQueue = DispatchSerialQueue(label: "USDModelWebProcess", qos: .userInteractive)
        usdLoader.delegate = self
    }

    func meshUpdated(data: consuming sending _Proto_MeshDataUpdate_v1) {
        let identifier = data.identifier
        self.dispatchSerialQueue.async {
            self.objcLoader.updateMesh(webRequest: Converter.webUpdateMeshRequestFromUpdateMeshRequest(data))
        }
    }

    func meshDestroyed(identifier: String) {
        //
    }

    func materialUpdated(data: consuming sending _Proto_MaterialDataUpdate_v1) {
        let identifier = data.identifier
        self.dispatchSerialQueue.async {
            self.objcLoader.updateMaterial(webRequest: Converter.webUpdateMaterialRequestFromUpdateMaterialRequest(data))
        }
    }

    func materialDestroyed(identifier: String) {
        //
    }

    func textureUpdated(data: consuming sending _Proto_TextureDataUpdate_v1) {
        let identifier = data.identifier
        self.dispatchSerialQueue.async {
            self.objcLoader.updateTexture(webRequest: Converter.webUpdateTextureRequestFromUpdateTextureRequest(data))
        }
    }

    func textureDestroyed(identifier: String) {
        //
    }

    func loadModel(from url: Foundation.URL) {
        do {
            let stage = try UsdStage(contentsOf: url)
            self.timeCodePerSecond = stage.timeCodesPerSecond
            self.startTime = stage.startTimeCode
            self.endTime = stage.endTimeCode
            self.usdLoader.loadStage(stage)
        } catch {
            fatalError(error.localizedDescription)
        }
    }

    func loadModel(from data: Data) {
        // usdLoader.loadModel(from: data)
    }

    func update(deltaTime: TimeInterval) {
        usdLoader.update(time: time)

        time = fmod(deltaTime * self.timeCodePerSecond + time - startTime, max(endTime - startTime, 1)) + startTime
    }
}

@objc
@implementation
extension DDBridgeModelLoader {
    @nonobjc
    var loader: USDModelLoader?
    @nonobjc
    var modelUpdated: ((DDBridgeUpdateMesh) -> (Void))?
    @nonobjc
    var textureUpdatedCallback: ((DDBridgeUpdateTexture) -> (Void))?
    @nonobjc
    var materialUpdatedCallback: ((DDBridgeUpdateMaterial) -> (Void))?

    @nonobjc
    fileprivate var retainedRequests: Set<NSObject> = []

    override init() {
        super.init()

        self.loader = USDModelLoader(objcInstance: self)
    }

    @objc(
        setCallbacksWithModelUpdatedCallback:
        textureUpdatedCallback:
        materialUpdatedCallback:
    )
    func setCallbacksWithModelUpdatedCallback(
        _ modelUpdatedCallback: @escaping ((DDBridgeUpdateMesh) -> (Void)),
        textureUpdatedCallback: @escaping ((DDBridgeUpdateTexture) -> (Void)),
        materialUpdatedCallback: @escaping ((DDBridgeUpdateMaterial) -> (Void))
    ) {
        self.modelUpdated = modelUpdatedCallback
        self.textureUpdatedCallback = textureUpdatedCallback
        self.materialUpdatedCallback = materialUpdatedCallback
    }

    @objc
    func loadModel(from url: Foundation.URL) {
        self.loader?.loadModel(from: url)
    }

    @objc
    func update(_ deltaTime: Double) {
        self.loader?.update(deltaTime: deltaTime)
    }

    @objc
    func requestCompleted(_ request: NSObject) {
        retainedRequests.remove(request)
    }

    fileprivate func updateMesh(webRequest: DDBridgeUpdateMesh) {
        if let modelUpdated {
            retainedRequests.insert(webRequest)
            modelUpdated(webRequest)
        }
    }

    fileprivate func updateTexture(webRequest: DDBridgeUpdateTexture) {
        if let textureUpdatedCallback {
            retainedRequests.insert(webRequest)
            textureUpdatedCallback(webRequest)
        }
    }

    fileprivate func updateMaterial(webRequest: DDBridgeUpdateMaterial) {
        if let materialUpdatedCallback {
            retainedRequests.insert(webRequest)
            materialUpdatedCallback(webRequest)
        }
    }
}

#endif
