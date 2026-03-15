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

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreRenderer, _version: 11) && compiler(>=6.2)
@_weakLinked @_spi(UsdLoaderAPI) import _USDKit_RealityKit
@_spi(RealityCoreRendererAPI) import RealityKit
@_weakLinked @_spi(RealityCoreTextureProcessingAPI) import RealityCoreTextureProcessing
@_weakLinked import USDKit
@_weakLinked @_spi(SwiftAPI) import DirectResource
@_weakLinked import USDKit
@_weakLinked import _USDKit_RealityKit
import RealityKit
@_weakLinked @_spi(SGPrivate) import ShaderGraph
@_weakLinked import RealityCoreDeformation

extension _USDKit_RealityKit._Proto_MeshDataUpdate_v1 {
    @_silgen_name("$s18_USDKit_RealityKit24_Proto_MeshDataUpdate_v1V18instanceTransformsSaySo13simd_float4x4aGvg")
    internal func instanceTransformsCompat() -> [simd_float4x4]
}

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V21geometryBindTransformSo13simd_float4x4avg")
    internal func geometryBindTransformCompat() -> simd_float4x4
}

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V15jointTransformsSaySo13simd_float4x4aGvg")
    internal func jointTransformsCompat() -> [simd_float4x4]
}

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V16inverseBindPosesSaySo13simd_float4x4aGvg")
    internal func inverseBindPosesCompat() -> [simd_float4x4]
}

extension MTLCaptureDescriptor {
    fileprivate convenience init(from device: (any MTLDevice)?) {
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

private func makeMTLTextureFromImageAsset(
    _ imageAsset: WKBridgeImageAsset,
    device: any MTLDevice,
    generateMips: Bool,
    memoryOwner: task_id_token_t,
    overridePixelFormat: Bool = false
) -> (any MTLTexture)? {
    guard let imageAssetData = imageAsset.data else {
        logError("no image data")
        return nil
    }
    logInfo(
        "imageAssetData = \(imageAssetData)  -  width = \(imageAsset.width)  -  height = \(imageAsset.height)  -  bytesPerPixel = \(imageAsset.bytesPerPixel) imageAsset.pixelFormat:  \(imageAsset.pixelFormat)"
    )

    var pixelFormat = imageAsset.pixelFormat
    if overridePixelFormat {
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
    }

    let (textureDescriptor, sliceCount) =
        switch imageAsset.textureType {
        case .typeCube:
            (
                MTLTextureDescriptor.textureCubeDescriptor(
                    pixelFormat: pixelFormat,
                    size: imageAsset.width,
                    mipmapped: generateMips
                ), 6
            )
        default:
            (
                MTLTextureDescriptor.texture2DDescriptor(
                    pixelFormat: pixelFormat,
                    width: imageAsset.width,
                    height: imageAsset.height,
                    mipmapped: generateMips
                ), 1
            )
        }

    textureDescriptor.usage = .shaderRead
    textureDescriptor.storageMode = .shared

    guard let mtlTexture = device.makeTexture(descriptor: textureDescriptor) else {
        logError("failed to device.makeTexture")
        return nil
    }
    mtlTexture.__setOwnerWithIdentity(memoryOwner)

    let bytesPerRow = imageAsset.width * imageAsset.bytesPerPixel
    let bytesPerImage = bytesPerRow * imageAsset.height
    let totalBytesNeeded = sliceCount * bytesPerImage

    unsafe imageAssetData.bytes.withUnsafeBytes { textureBytes in
        guard let textureBytesBaseAddress = textureBytes.baseAddress else {
            return
        }

        // Validate that we have enough data
        guard textureBytes.count >= totalBytesNeeded else {
            return
        }

        for face in 0..<sliceCount {
            let offset = face * bytesPerImage
            let facePointer = unsafe textureBytesBaseAddress.advanced(by: offset)

            unsafe mtlTexture.replace(
                region: MTLRegionMake2D(0, 0, imageAsset.width, imageAsset.height),
                mipmapLevel: 0,
                slice: face,
                withBytes: facePointer,
                bytesPerRow: bytesPerRow,
                bytesPerImage: bytesPerImage
            )
        }
    }

    return mtlTexture
}

private func makeTextureFromImageAsset(
    _ imageAsset: WKBridgeImageAsset,
    device: any MTLDevice,
    renderContext: any _Proto_LowLevelRenderContext_v1,
    commandQueue: any MTLCommandQueue,
    generateMips: Bool,
    memoryOwner: task_id_token_t,
    overridePixelFormat: Bool,
    swizzle: MTLTextureSwizzleChannels = .init(red: .red, green: .green, blue: .blue, alpha: .alpha)
) -> _Proto_LowLevelTextureResource_v1? {
    guard
        let mtlTexture = makeMTLTextureFromImageAsset(
            imageAsset,
            device: device,
            generateMips: generateMips,
            memoryOwner: memoryOwner,
            overridePixelFormat: overridePixelFormat
        )
    else {
        logError("could not create metal texture")
        return nil
    }

    let descriptor = _Proto_LowLevelTextureResource_v1.Descriptor.from(mtlTexture, swizzle: swizzle)
    if let textureResource = try? renderContext.makeTextureResource(descriptor: descriptor) {
        guard let commandBuffer = commandQueue.makeCommandBuffer() else {
            fatalError("Could not create command buffer")
        }
        guard let blitEncoder = commandBuffer.makeBlitCommandEncoder() else {
            fatalError("Could not create blit encoder")
        }
        if generateMips {
            blitEncoder.generateMipmaps(for: mtlTexture)
        }

        let outTexture = textureResource.replace(using: commandBuffer)
        blitEncoder.copy(from: mtlTexture, to: outTexture)

        blitEncoder.endEncoding()
        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()
        return textureResource
    }

    return nil
}

private func makeParameters(
    for function: (any _Proto_LowLevelMaterialResource_v1.Function)?,
    renderContext: any _Proto_LowLevelRenderContext_v1,
    textureResources: [String: _Proto_LowLevelTextureResource_v1],
    fallbackTexture: _Proto_LowLevelTextureResource_v1
) throws -> _Proto_LowLevelArgumentTable_v1? {
    guard let function else { return nil }
    guard let argumentTableDescriptor = function.argumentTableDescriptor else { return nil }
    let parameterMapping = function.parameterMapping

    var optTextures: [_Proto_LowLevelTextureResource_v1?] = argumentTableDescriptor.textures.map({ _ in nil })
    for parameter in parameterMapping?.textures ?? [] {
        optTextures[parameter.textureIndex] = textureResources[parameter.name] ?? fallbackTexture
    }
    // swift-format-ignore: NeverForceUnwrap
    let textures = optTextures.map({ $0! })

    let buffers: [_Proto_LowLevelBufferSpan_v1] = try argumentTableDescriptor.buffers.map { bufferRequirements in
        let capacity = (bufferRequirements.size + 16 - 1) / 16 * 16
        let buffer = try renderContext.makeBufferResource(descriptor: .init(capacity: capacity))
        buffer.replace { span in
            for byteOffset in span.byteOffsets {
                span.storeBytes(of: 0, toByteOffset: byteOffset, as: UInt8.self)
            }
        }
        return try _Proto_LowLevelBufferSpan_v1(buffer: buffer, offset: 0, size: bufferRequirements.size)
    }

    return try renderContext.makeArgumentTable(
        descriptor: argumentTableDescriptor,
        buffers: buffers,
        textures: textures
    )
}

extension Logger {
    fileprivate static let modelGPU = Logger(subsystem: "com.apple.WebKit", category: "model")
}

internal func logError(_ error: String) {
    Logger.modelGPU.error("\(error)")
}

internal func logInfo(_ info: String) {
    Logger.modelGPU.info("\(info)")
}

extension simd_float4x4 {
    fileprivate var minor: simd_float3x3 {
        .init(
            [self.columns.0.x, self.columns.0.y, self.columns.0.z],
            [self.columns.1.x, self.columns.1.y, self.columns.1.z],
            [self.columns.2.x, self.columns.2.y, self.columns.2.z]
        )
    }
}

@objc
@implementation
extension WKBridgeUSDConfiguration {
    @nonobjc
    fileprivate let device: any MTLDevice
    @nonobjc
    fileprivate let appRenderer: Renderer
    @nonobjc
    fileprivate final var commandQueue: any MTLCommandQueue {
        get { appRenderer.commandQueue }
    }
    @nonobjc
    fileprivate final var renderer: _Proto_LowLevelRenderer_v1 {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            appRenderer.renderer!
        }
    }
    @nonobjc
    fileprivate final var renderContext: any _Proto_LowLevelRenderContext_v1 {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            appRenderer.renderContext!
        }
    }

    @nonobjc
    fileprivate final var renderTarget: _Proto_LowLevelRenderTarget_v1.Descriptor {
        get { appRenderer.renderTargetDescriptor }
    }

    @objc
    init(device: any MTLDevice, memoryOwner: task_id_token_t) {
        self.device = device
        do {
            self.appRenderer = try Renderer(device: device, memoryOwner: memoryOwner)
        } catch {
            fatalError("Exception creating renderer \(error)")
        }
    }

    @objc(createMaterialCompiler:)
    func createMaterialCompiler() async {
        do {
            try await self.appRenderer.createMaterialCompiler(colorPixelFormat: .rgba16Float, rasterSampleCount: 4)
        } catch {
            fatalError("Exception creating renderer \(error)")
        }
    }
}

@objc
@implementation
extension WKBridgeReceiver {
    @nonobjc
    fileprivate let device: any MTLDevice
    @nonobjc
    fileprivate let textureProcessingContext: _Proto_LowLevelTextureProcessingContext_v1
    @nonobjc
    fileprivate let commandQueue: any MTLCommandQueue

    @nonobjc
    fileprivate let renderContext: any _Proto_LowLevelRenderContext_v1
    @nonobjc
    fileprivate let renderer: _Proto_LowLevelRenderer_v1
    @nonobjc
    fileprivate let appRenderer: Renderer
    @nonobjc
    fileprivate let lightingFunction: _Proto_LowLevelMaterialResource_v1.LightingFunction
    @nonobjc
    fileprivate let lightingArguments: _Proto_LowLevelArgumentTable_v1
    @nonobjc
    fileprivate var lightingArgumentBuffer: _Proto_LowLevelArgumentTable_v1?

    @nonobjc
    fileprivate final var renderTarget: _Proto_LowLevelRenderTarget_v1.Descriptor {
        get { appRenderer.renderTargetDescriptor }
    }
    @nonobjc
    fileprivate var meshInstancePlainArray: [_Proto_LowLevelMeshInstance_v1?]
    @nonobjc
    fileprivate var meshInstances: _Proto_LowLevelMeshInstanceArray_v1

    @nonobjc
    fileprivate var meshResources: [_Proto_ResourceId: _Proto_LowLevelMeshResource_v1] = [:]
    @nonobjc
    fileprivate var meshResourceToMaterials: [_Proto_ResourceId: [_Proto_ResourceId]] = [:]
    @nonobjc
    fileprivate var meshToMeshInstances: [_Proto_ResourceId: [_Proto_LowLevelMeshInstance_v1]] = [:]
    @nonobjc
    fileprivate var meshTransforms: [_Proto_ResourceId: [simd_float4x4]] = [:]
    @nonobjc
    fileprivate var rotationAngle: Float = 0

    @nonobjc
    fileprivate let deformationSystem: _Proto_LowLevelDeformationSystem_v1

    struct DeformationContext {
        let deformation: _Proto_Deformation_v1
        var description: _Proto_LowLevelDeformationDescription_v1
        var dirty: Bool
    }
    @nonobjc
    fileprivate var meshResourceToDeformationContext: [_Proto_ResourceId: DeformationContext] = [:]

    struct Material {
        let resource: _Proto_LowLevelMaterialResource_v1
        let geometryArguments: _Proto_LowLevelArgumentTable_v1?
        let surfaceArguments: _Proto_LowLevelArgumentTable_v1?
        #if canImport(RealityCoreRenderer, _version: 12)
        let blending: _Proto_LowLevelMaterialResource_v1.ShaderGraphOutput.Blending
        #endif
    }
    @nonobjc
    fileprivate var materialsAndParams: [_Proto_ResourceId: Material] = [:]

    @nonobjc
    fileprivate var textureResources: [String: _Proto_LowLevelTextureResource_v1] = [:]

    @nonobjc
    fileprivate var modelTransform: simd_float4x4
    @nonobjc
    fileprivate var modelDistance: Float

    @nonobjc
    fileprivate var dontCaptureAgain: Bool = false

    @nonobjc
    fileprivate final var memoryOwner: task_id_token_t {
        appRenderer.memoryOwner
    }

    @nonobjc
    fileprivate let fallbackTexture: _Proto_LowLevelTextureResource_v1

    init(
        configuration: WKBridgeUSDConfiguration,
        diffuseAsset: WKBridgeImageAsset,
        specularAsset: WKBridgeImageAsset
    ) throws {
        self.renderContext = configuration.renderContext
        self.renderer = configuration.renderer
        self.appRenderer = configuration.appRenderer
        self.device = configuration.device
        self.textureProcessingContext = _Proto_LowLevelTextureProcessingContext_v1(device: configuration.device)
        self.commandQueue = configuration.commandQueue
        self.deformationSystem = try _Proto_LowLevelDeformationSystem_v1.make(configuration.device, configuration.commandQueue).get()
        modelTransform = matrix_identity_float4x4
        modelDistance = 1.0
        self.meshInstancePlainArray = []
        let meshInstances = try configuration.renderContext.makeMeshInstanceArray(renderTargets: [configuration.renderTarget], count: 16)
        let lightingFunction = configuration.renderContext.makePhysicallyBasedLightingFunction()
        guard
            let diffuseTexture = makeTextureFromImageAsset(
                diffuseAsset,
                device: device,
                renderContext: renderContext,
                commandQueue: configuration.commandQueue,
                generateMips: true,
                memoryOwner: configuration.appRenderer.memoryOwner,
                overridePixelFormat: false,
                swizzle: .init(red: .red, green: .red, blue: .red, alpha: .one)
            )
        else {
            fatalError("Could not create diffuseTexture")
        }
        guard
            let specularTexture = makeTextureFromImageAsset(
                specularAsset,
                device: device,
                renderContext: renderContext,
                commandQueue: configuration.commandQueue,
                generateMips: true,
                memoryOwner: configuration.appRenderer.memoryOwner,
                overridePixelFormat: false,
                swizzle: .init(red: .red, green: .red, blue: .red, alpha: .one)
            )
        else {
            fatalError("Could not create specularTexture")
        }
        self.meshInstances = meshInstances
        self.lightingFunction = lightingFunction
        guard let lightingFunctionArgumentTableDescriptor = lightingFunction.argumentTableDescriptor else {
            fatalError("Could not create lighting function")
        }
        self.lightingArguments = try configuration.renderContext.makeArgumentTable(
            descriptor: lightingFunctionArgumentTableDescriptor,
            buffers: [],
            textures: [
                diffuseTexture, specularTexture,
            ]
        )

        self.fallbackTexture = makeFallBackTextureResource(
            renderContext,
            commandQueue: configuration.commandQueue,
            device: configuration.device
        )
    }

    @objc(renderWithTexture:)
    func render(with texture: any MTLTexture) {
        for (identifier, meshes) in meshToMeshInstances {
            let originalTransforms = meshTransforms[identifier]
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap

            for (index, meshInstance) in meshes.enumerated() {
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                let computedTransform = modelTransform * originalTransforms![index]
                meshInstance.setTransform(.single(computedTransform))
            }
        }

        // animate
        if !meshResourceToDeformationContext.isEmpty {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let commandBuffer = self.commandQueue.makeCommandBuffer()!

            for (identifier, deformationContext) in meshResourceToDeformationContext where deformationContext.dirty {
                deformationContext.deformation.execute(deformation: deformationContext.description, commandBuffer: commandBuffer) {
                    (commandBuffer: any MTLCommandBuffer) in
                }
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                meshResourceToDeformationContext[identifier]!.dirty = false
            }

            commandBuffer.enqueue()
            commandBuffer.commit()
        }

        // render
        if dontCaptureAgain == false {
            let captureDescriptor = MTLCaptureDescriptor(from: device)
            let captureManager = MTLCaptureManager.shared()
            do {
                try captureManager.startCapture(with: captureDescriptor)
                print("Capture started at \(captureDescriptor.outputURL?.absoluteString ?? "")")
            } catch {
                logError("failed to start gpu capture \(error)")
                dontCaptureAgain = true
            }
        }

        do {
            try appRenderer.render(meshInstances: meshInstances, texture: texture)
        } catch {
            logError("failed to start gpu capture \(error)")
        }

        let captureManager = MTLCaptureManager.shared()
        if captureManager.isCapturing {
            captureManager.stopCapture()
        }
    }

    @objc(updateTexture:)
    func updateTexture(_ data: WKBridgeUpdateTexture) {
        guard let asset = data.imageAsset else {
            logError("Image asset was nil")
            return
        }

        let textureHash = data.hashString
        if textureResources[textureHash] != nil {
            logError("Texture already exists")
            return
        }

        let commandQueue = appRenderer.commandQueue
        if let textureResource = makeTextureFromImageAsset(
            asset,
            device: device,
            renderContext: renderContext,
            commandQueue: commandQueue,
            generateMips: true,
            memoryOwner: self.memoryOwner,
            overridePixelFormat: false
        ) {
            textureResources[textureHash] = textureResource
        }
    }

    @objc(updateMaterial:completionHandler:)
    func updateMaterial(_ data: WKBridgeUpdateMaterial) async {
        logInfo("updateMaterial (pre-dispatch) \(data.identifier)")
        do {
            let identifier = data.identifier
            logInfo("updateMaterial \(identifier)")

            #if canImport(RealityCoreRenderer, _version: 12)
            guard let shaderGraph = ShaderGraph._Proto_ShaderNodeGraph.fromWKDescriptor(data.materialGraph) else {
                logError("No materialGraph data provided for material \(identifier)")
                return
            }

            let shaderGraphOutput = try await renderContext.makeShaderGraphFunctions(shaderGraph: shaderGraph)
            #else
            let shaderGraphOutput = try await renderContext.makeShaderGraphFunctions(nil)
            #endif

            let geometryArguments = try makeParameters(
                for: shaderGraphOutput.geometryModifier,
                renderContext: renderContext,
                textureResources: textureResources,
                fallbackTexture: self.fallbackTexture
            )
            let surfaceArguments = try makeParameters(
                for: shaderGraphOutput.surfaceShader,
                renderContext: renderContext,
                textureResources: textureResources,
                fallbackTexture: self.fallbackTexture
            )

            let geometryModifier = shaderGraphOutput.geometryModifier ?? renderContext.makeDefaultGeometryModifier()
            let surfaceShader = shaderGraphOutput.surfaceShader
            let materialResource = try await renderContext.makeMaterialResource(
                descriptor: .init(
                    geometry: geometryModifier,
                    surface: surfaceShader,
                    lighting: lightingFunction
                )
            )
            #if canImport(RealityCoreRenderer, _version: 12)
            materialsAndParams[identifier] = .init(
                resource: materialResource,
                geometryArguments: geometryArguments,
                surfaceArguments: surfaceArguments,
                blending: shaderGraphOutput.blending
            )
            #else
            materialsAndParams[identifier] = .init(
                resource: materialResource,
                geometryArguments: geometryArguments,
                surfaceArguments: surfaceArguments
            )
            #endif
        } catch {
            logError("updateMaterial failed \(error)")
        }
    }

    @objc(updateMesh:completionHandler:)
    func updateMesh(_ data: WKBridgeUpdateMesh) async {
        let identifier = data.identifier
        logInfo("(update mesh) \(identifier) Material ids \(data.materialPrims)")

        do {
            let identifier = data.identifier

            let meshResource: _Proto_LowLevelMeshResource_v1
            if data.updateType == .initial || data.descriptor != nil {
                // swift-format-ignore: NeverForceUnwrap
                let meshDescriptor = data.descriptor!
                let descriptor = _Proto_LowLevelMeshResource_v1.Descriptor.fromLlmDescriptor(meshDescriptor)
                meshResource = try renderContext.makeMeshResource(descriptor: descriptor)
                meshResource.replaceData(indexData: data.indexData, vertexData: data.vertexData)
                meshResources[identifier] = meshResource
            } else {
                guard let cachedMeshResource = meshResources[identifier] else {
                    fatalError("Mesh resource should already be created from previous update")
                }

                if data.indexData != nil || !data.vertexData.isEmpty {
                    cachedMeshResource.replaceData(indexData: data.indexData, vertexData: data.vertexData)
                }
                meshResource = cachedMeshResource
            }

            if let deformationData = data.deformationData {
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                let commandBuffer = self.commandQueue.makeCommandBuffer()!
                // TODO: delta update
                configureDeformation(identifier: identifier, deformationData: deformationData, commandBuffer: commandBuffer)
                commandBuffer.enqueue()
                commandBuffer.commit()
            }

            if data.instanceTransformsCount > 0 {
                // Make new instances
                if meshToMeshInstances[identifier] == nil {
                    meshToMeshInstances[identifier] = []
                    meshTransforms[identifier] = []

                    for (partIndex, _) in data.parts.enumerated() {
                        let materialIdentifier = data.materialPrims[partIndex]
                        guard let material = materialsAndParams[materialIdentifier] else {
                            fatalError("Failed to get material instance \(materialIdentifier)")
                        }

                        let pipeline = try await renderContext.makeRenderPipelineState(
                            descriptor: .descriptor(
                                mesh: meshResource.descriptor,
                                material: material.resource,
                                renderTargets: [renderTarget]
                            )
                        )

                        let meshPart = try renderContext.makeMeshPart(
                            resource: meshResource,
                            indexOffset: data.parts[partIndex].indexOffset,
                            indexCount: data.parts[partIndex].indexCount,
                            primitive: data.parts[partIndex].topology,
                            windingOrder: .counterClockwise,
                            boundsMin: -.one,
                            boundsMax: .one
                        )

                        for instanceTransform in data.instanceTransforms {
                            let meshInstance = try renderContext.makeMeshInstance(
                                meshPart: meshPart,
                                pipeline: pipeline,
                                geometryArguments: material.geometryArguments,
                                surfaceArguments: material.surfaceArguments,
                                lightingArguments: lightingArguments,
                                transform: .single(instanceTransform),
                                category: .opaque
                            )

                            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                            // swift-format-ignore: NeverForceUnwrap
                            meshToMeshInstances[identifier]!.append(meshInstance)
                            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                            // swift-format-ignore: NeverForceUnwrap
                            meshTransforms[identifier]!.append(instanceTransform)

                            let meshInstanceIndex = meshInstancePlainArray.count
                            meshInstancePlainArray.append(meshInstance)
                            if meshInstances.count < meshInstancePlainArray.count {
                                let meshInstances = try renderContext.makeMeshInstanceArray(
                                    renderTargets: [renderTarget],
                                    count: meshInstances.count * 2
                                )
                                for index in meshInstancePlainArray.indices {
                                    try meshInstances.setMeshInstance(meshInstancePlainArray[index], index: index)
                                }
                                self.meshInstances = meshInstances
                            } else {
                                try meshInstances.setMeshInstance(meshInstance, index: meshInstanceIndex)
                            }
                        }
                    }
                } else {
                    // Update transforms otherwise

                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                    // swift-format-ignore: NeverForceUnwrap
                    let partCount = meshToMeshInstances[identifier]!.count / data.instanceTransforms.count
                    for (instanceIndex, instanceTransform) in data.instanceTransforms.enumerated() {
                        for partIndex in 0..<partCount {
                            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                            // swift-format-ignore: NeverForceUnwrap
                            meshTransforms[identifier]![instanceIndex * data.parts.count + partIndex] = instanceTransform
                        }
                    }
                }
            }

            if !data.materialPrims.isEmpty {
                meshResourceToMaterials[identifier] = data.materialPrims
            }
        } catch {
            logError(error.localizedDescription)
        }
    }

    @objc(setTransform:)
    func setTransform(_ transform: simd_float4x4) {
        modelTransform = transform
    }

    @objc
    func setCameraDistance(_ distance: Float) {
        modelDistance = distance
        appRenderer.setCameraDistance(modelDistance)
    }

    @objc
    func setBackgroundColor(_ color: simd_float3) {
        appRenderer.setBackgroundColor(color)
    }

    @objc
    func setPlaying(_ play: Bool) {
        // resourceContext.setEnableModelRotation(play)
    }

    @objc
    func setEnvironmentMap(_ imageAsset: WKBridgeImageAsset) {
        do {
            guard
                let mtlTextureEquirectangular = makeMTLTextureFromImageAsset(
                    imageAsset,
                    device: device,
                    generateMips: true,
                    memoryOwner: self.memoryOwner
                )
            else {
                fatalError("Could not make metal texture from environment asset data")
            }

            let cubeMTLTextureDescriptor = try self.textureProcessingContext.createCubeDescriptor(
                fromEquirectangular: mtlTextureEquirectangular
            )
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let cubeMTLTexture = self.device.makeTexture(descriptor: cubeMTLTextureDescriptor)!
            cubeMTLTexture.__setOwnerWithIdentity(self.memoryOwner)

            let diffuseMTLTextureDescriptor = try self.textureProcessingContext.createImageBasedLightDiffuseDescriptor(
                fromCube: cubeMTLTexture
            )
            let diffuseTextureDescriptor = _Proto_LowLevelTextureResource_v1.Descriptor.from(diffuseMTLTextureDescriptor)
            let diffuseTexture = try self.renderContext.makeTextureResource(descriptor: diffuseTextureDescriptor)

            let specularMTLTextureDescriptor = try self.textureProcessingContext.createImageBasedLightSpecularDescriptor(
                fromCube: cubeMTLTexture
            )
            let specularTextureDescriptor = _Proto_LowLevelTextureResource_v1.Descriptor.from(specularMTLTextureDescriptor)
            let specularTexture = try self.renderContext.makeTextureResource(descriptor: specularTextureDescriptor)

            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let commandBuffer = self.commandQueue.makeCommandBuffer()!

            try self.textureProcessingContext.generateCube(
                using: commandBuffer,
                fromEquirectangular: mtlTextureEquirectangular,
                into: cubeMTLTexture
            )

            let diffuseMTLTexture = diffuseTexture.replace(using: commandBuffer)
            let specularMTLTexture = specularTexture.replace(using: commandBuffer)

            try self.textureProcessingContext.generateImageBasedLightDiffuse(
                using: commandBuffer,
                fromSkyboxCube: cubeMTLTexture,
                into: diffuseMTLTexture
            )
            try self.textureProcessingContext.generateImageBasedLightSpecular(
                using: commandBuffer,
                fromSkyboxCube: cubeMTLTexture,
                into: specularMTLTexture
            )

            try self.lightingArguments.setTexture(at: 0, diffuseTexture)
            try self.lightingArguments.setTexture(at: 1, specularTexture)

            commandBuffer.commit()
        } catch {
            fatalError(error.localizedDescription)
        }
    }
}

private func webPartsFromParts(_ parts: [LowLevelMesh.Part]) -> [WKBridgeMeshPart] {
    parts.map({ a in
        WKBridgeMeshPart(
            indexOffset: a.indexOffset,
            indexCount: a.indexCount,
            topology: a.topology,
            materialIndex: a.materialIndex,
            boundsMin: a.bounds.min,
            boundsMax: a.bounds.max
        )
    })
}

private func convert(_ m: _Proto_DataUpdateType_v1) -> WKBridgeDataUpdateType {
    if m == .initial {
        return .initial
    }
    return .delta
}

private func webUpdateTextureRequestFromUpdateTextureRequest(_ request: _Proto_TextureDataUpdate_v1) -> WKBridgeUpdateTexture {
    // FIXME: remove placeholder code
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
    // swift-format-ignore: NeverForceUnwrap
    let descriptor = request.descriptor!
    let data = request.data
    return WKBridgeUpdateTexture(
        imageAsset: .init(descriptor, data: data),
        identifier: request.identifier,
        hashString: request.hashString
    )
}

private func webUpdateMeshRequestFromUpdateMeshRequest(
    _ request: _Proto_MeshDataUpdate_v1
) -> WKBridgeUpdateMesh {
    var descriptor: WKBridgeMeshDescriptor?
    if let requestDescriptor = request.descriptor {
        descriptor = .init(request: requestDescriptor)
    }

    return WKBridgeUpdateMesh(
        identifier: request.identifier,
        updateType: convert(request.updateType),
        descriptor: descriptor,
        parts: webPartsFromParts(request.parts),
        indexData: request.indexData,
        vertexData: request.vertexData,
        instanceTransforms: toData(request.instanceTransformsCompat()),
        instanceTransformsCount: request.instanceTransformsCompat().count,
        materialPrims: request.materialPrims,
        deformationData: .init(request.deformationData)
    )
}

private func toWKBridgeNodeType(_ node: _Proto_ShaderNodeGraph.Node) -> WKBridgeNodeType {
    // Determine node type based on the node's name or data type
    let nodeName = node.name.lowercased()
    if nodeName == "arguments" {
        return .arguments
    } else if nodeName == "results" || nodeName == "result" {
        return .results
    }

    // Check the node data type
    switch node.data {
    case .constant:
        return .constant
    case .definition, .graph:
        return .builtin
    default: fatalError("toWKBridgeNodeType - unknown _Proto_ShaderNodeGraph.Node.data type")
    }
}

private func toWKBridgeBuiltin(_ node: _Proto_ShaderNodeGraph.Node) -> WKBridgeBuiltin {
    // Extract builtin information from the node
    switch node.data {
    case .definition(let definition):
        return WKBridgeBuiltin(definition: definition.name, name: node.name)
    case .graph:
        return WKBridgeBuiltin(definition: "graph", name: node.name)
    case .constant:
        return WKBridgeBuiltin(definition: "", name: node.name)
    default: fatalError("toWKBridgeBuiltin - unknown _Proto_ShaderNodeGraph.Node.data type")
    }
}

private func constantValues(_ constant: _Proto_ShaderGraphValue) -> ([WKBridgeValueString], WKBridgeConstant) {
    switch constant {
    case .bool(let bool):
        return ([WKBridgeValueString(number: NSNumber(booleanLiteral: bool))], .bool)
    case .uchar(let uchar):
        return ([WKBridgeValueString(number: NSNumber(value: uchar))], .uchar)
    case .int(let int):
        return ([WKBridgeValueString(number: NSNumber(value: int))], .int)
    case .uint(let uint):
        return ([WKBridgeValueString(number: NSNumber(value: uint))], .uint)
    case .half(let uint16):
        return ([WKBridgeValueString(number: NSNumber(value: uint16))], .half)
    case .float(let float):
        return ([WKBridgeValueString(number: NSNumber(value: float))], .float)
    case .string(let string):
        return ([WKBridgeValueString(string: string)], .string)
    case .float2(let vector2f):
        return (
            [WKBridgeValueString(number: NSNumber(value: vector2f.x)), WKBridgeValueString(number: NSNumber(value: vector2f.y))], .float2
        )
    case .float3(let float3):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: float3.x)),
                WKBridgeValueString(number: NSNumber(value: float3.y)),
                WKBridgeValueString(number: NSNumber(value: float3.z)),
            ], .float3
        )
    case .float4(let vector4f):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: vector4f.x)),
                WKBridgeValueString(number: NSNumber(value: vector4f.y)),
                WKBridgeValueString(number: NSNumber(value: vector4f.z)),
                WKBridgeValueString(number: NSNumber(value: vector4f.w)),
            ], .float4
        )
    case .half2(let vector2h):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: vector2h.x)),
                WKBridgeValueString(number: NSNumber(value: vector2h.y)),
            ], .half2
        )
    case .half3(let half3):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: half3.x)),
                WKBridgeValueString(number: NSNumber(value: half3.y)),
                WKBridgeValueString(number: NSNumber(value: half3.z)),
            ], .half3
        )
    case .half4(let vector4h):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: vector4h.x)),
                WKBridgeValueString(number: NSNumber(value: vector4h.y)),
                WKBridgeValueString(number: NSNumber(value: vector4h.z)),
                WKBridgeValueString(number: NSNumber(value: vector4h.w)),
            ], .half4
        )
    case .int2(let vector2i):
        return (
            [WKBridgeValueString(number: NSNumber(value: vector2i.x)), WKBridgeValueString(number: NSNumber(value: vector2i.y))], .int2
        )
    case .int3(let vector3i):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: vector3i.x)),
                WKBridgeValueString(number: NSNumber(value: vector3i.y)),
                WKBridgeValueString(number: NSNumber(value: vector3i.z)),
            ], .int3
        )
    case .int4(let vector4i):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: vector4i.x)),
                WKBridgeValueString(number: NSNumber(value: vector4i.y)),
                WKBridgeValueString(number: NSNumber(value: vector4i.z)),
                WKBridgeValueString(number: NSNumber(value: vector4i.w)),
            ], .int4
        )
    case .cgColor3(let color3):
        // Extract RGB components from CGColor
        guard let components = color3.components, components.count >= 3 else {
            return ([], .asset)
        }
        return (
            [
                WKBridgeValueString(number: NSNumber(value: Float(components[0]))),
                WKBridgeValueString(number: NSNumber(value: Float(components[1]))),
                WKBridgeValueString(number: NSNumber(value: Float(components[2]))),
            ], .color3f
        )
    case .cgColor4(let color4):
        // Extract RGBA components from CGColor
        guard let components = color4.components, components.count >= 4 else {
            return ([], .asset)
        }
        return (
            [
                WKBridgeValueString(number: NSNumber(value: Float(components[0]))),
                WKBridgeValueString(number: NSNumber(value: Float(components[1]))),
                WKBridgeValueString(number: NSNumber(value: Float(components[2]))),
                WKBridgeValueString(number: NSNumber(value: Float(components[3]))),
            ], .color4f
        )
    case .float3x3(let col0, let col1, let col2):
        // Extract 9 float values from the 3x3 matrix (3 columns of 3 rows each)
        return (
            [
                WKBridgeValueString(number: NSNumber(value: col0.x)),
                WKBridgeValueString(number: NSNumber(value: col0.y)),
                WKBridgeValueString(number: NSNumber(value: col0.z)),
                WKBridgeValueString(number: NSNumber(value: col1.x)),
                WKBridgeValueString(number: NSNumber(value: col1.y)),
                WKBridgeValueString(number: NSNumber(value: col1.z)),
                WKBridgeValueString(number: NSNumber(value: col2.x)),
                WKBridgeValueString(number: NSNumber(value: col2.y)),
                WKBridgeValueString(number: NSNumber(value: col2.z)),
            ], .matrix3f
        )
    case .float4x4(let col0, let col1, let col2, let col3):
        // Extract 16 float values from the 4x4 matrix (4 columns of 4 rows each)
        return (
            [
                WKBridgeValueString(number: NSNumber(value: col0.x)),
                WKBridgeValueString(number: NSNumber(value: col0.y)),
                WKBridgeValueString(number: NSNumber(value: col0.z)),
                WKBridgeValueString(number: NSNumber(value: col0.w)),
                WKBridgeValueString(number: NSNumber(value: col1.x)),
                WKBridgeValueString(number: NSNumber(value: col1.y)),
                WKBridgeValueString(number: NSNumber(value: col1.z)),
                WKBridgeValueString(number: NSNumber(value: col1.w)),
                WKBridgeValueString(number: NSNumber(value: col2.x)),
                WKBridgeValueString(number: NSNumber(value: col2.y)),
                WKBridgeValueString(number: NSNumber(value: col2.z)),
                WKBridgeValueString(number: NSNumber(value: col2.w)),
                WKBridgeValueString(number: NSNumber(value: col3.x)),
                WKBridgeValueString(number: NSNumber(value: col3.y)),
                WKBridgeValueString(number: NSNumber(value: col3.z)),
                WKBridgeValueString(number: NSNumber(value: col3.w)),
            ], .matrix4f
        )
    default:
        // For any unsupported types, return empty asset
        return ([], .asset)
    }
}

private func toWKBridgeConstantContainer(_ node: _Proto_ShaderNodeGraph.Node) -> WKBridgeConstantContainer {
    // Extract constant value if this is a constant node
    switch node.data {
    case .constant(let value):
        let converted = constantValues(value)
        return WKBridgeConstantContainer(constant: converted.1, constantValues: converted.0, name: node.name)
    case .definition, .graph:
        return WKBridgeConstantContainer(constant: .asset, constantValues: [], name: node.name)
    default: fatalError("toWKBridgeConstantContainer - unknown _Proto_ShaderNodeGraph.Node.data type")
    }
}

private func toWebNodes(_ nodes: [_Proto_ShaderNodeGraph.Node]) -> [WKBridgeNode] {
    nodes.map { e in
        WKBridgeNode(bridgeNodeType: toWKBridgeNodeType(e), builtin: toWKBridgeBuiltin(e), constant: toWKBridgeConstantContainer(e))
    }
}

private func toWKBridgeDataType(_ dataType: _Proto_ShaderDataType) -> WKBridgeDataType {
    switch dataType {
    case .invalid: .asset // Map invalid to asset as fallback
    case .bool: .bool
    case .uchar: .uchar
    case .int: .int
    case .uint: .uint
    case .half: .half
    case .float: .float
    case .string: .string
    case .float2: .float2
    case .float3: .float3
    case .float4: .float4
    case .half2: .half2
    case .half3: .half3
    case .half4: .half4
    case .int2: .int2
    case .int3: .int3
    case .int4: .int4
    case .float2x2: .matrix2f
    case .float3x3: .matrix3f
    case .float4x4: .matrix4f
    case .half2x2: .matrix2h
    case .half3x3: .matrix3h
    case .half4x4: .matrix4h
    case .quaternion: .quat
    case .surfaceShader: .surfaceShader
    case .geometryModifier: .geometryModifier
    case .postLightingShader: .postLightingShader
    case .cgColor3: .color3f // Assuming float color
    case .cgColor4: .color4f // Assuming float color
    case .filename: .asset
    @unknown default: .asset
    }
}

private func createInputOutput(
    name: String,
    type: _Proto_ShaderDataType,
    semanticType: _Proto_ShaderGraphNodeDefinition.SemanticType?,
    defaultValue: _Proto_ShaderGraphValue?,
) -> WKBridgeInputOutput {
    let defaultValueContainer: WKBridgeConstantContainer? = defaultValue.map {
        let (values, constantType) = constantValues($0)
        return WKBridgeConstantContainer(constant: constantType, constantValues: values, name: "")
    }

    let (actualType, hasSemanticType) =
        semanticType.map { semantic in
            let semanticName = semantic.name.lowercased()
            if semanticName.contains("color4") || semanticName == "color4f" {
                return (WKBridgeDataType.color4f, true)
            } else if semanticName.contains("color3") || semanticName == "color3f" {
                return (WKBridgeDataType.color3f, true)
            } else {
                return (toWKBridgeDataType(type), true)
            }
        } ?? (toWKBridgeDataType(type), false)

    return WKBridgeInputOutput(
        type: actualType,
        name: name,
        semanticType: actualType,
        hasSemanticType: hasSemanticType,
        defaultValue: defaultValueContainer
    )
}

private func toWebInputOutputs(_ inputs: [_Proto_ShaderGraphNodeDefinition.Input]) -> [WKBridgeInputOutput] {
    inputs.map { e in
        createInputOutput(
            name: e.name,
            type: e.type,
            semanticType: e.semanticType,
            defaultValue: e.defaultValue
        )
    }
}

private func toWebOutputs(_ outputs: [_Proto_ShaderGraphNodeDefinition.Output]) -> [WKBridgeInputOutput] {
    outputs.map { e in
        createInputOutput(
            name: e.name,
            type: e.type,
            semanticType: e.semanticType,
            defaultValue: e.defaultValue
        )
    }
}

private func toWebNode(_ e: _Proto_ShaderNodeGraph.Node) -> WKBridgeNode {
    WKBridgeNode(bridgeNodeType: toWKBridgeNodeType(e), builtin: toWKBridgeBuiltin(e), constant: toWKBridgeConstantContainer(e))
}

private func toWebEdges(_ edges: [_Proto_ShaderNodeGraph.Edge]) -> [WKBridgeEdge] {
    edges.map { edge in
        WKBridgeEdge(
            outputNode: edge.outputNode,
            outputPort: edge.outputPort,
            inputNode: edge.inputNode,
            inputPort: edge.inputPort
        )
    }
}

private func toWebMaterialGraph(_ material: _Proto_ShaderNodeGraph?) -> WKBridgeMaterialGraph {
    guard let material else {
        // Return empty material graph if nil
        return WKBridgeMaterialGraph(
            nodes: [],
            edges: [],
            arguments: WKBridgeNode(
                bridgeNodeType: .arguments,
                builtin: WKBridgeBuiltin(definition: "", name: "arguments"),
                constant: WKBridgeConstantContainer(constant: .asset, constantValues: [], name: "")
            ),
            results: WKBridgeNode(
                bridgeNodeType: .results,
                builtin: WKBridgeBuiltin(definition: "", name: "results"),
                constant: WKBridgeConstantContainer(constant: .asset, constantValues: [], name: "")
            ),
            inputs: [],
            outputs: []
        )
    }

    // Convert nodes dictionary to array
    let nodes = material.nodes.values.map(toWebNode)

    // Convert edges
    let edges = toWebEdges(material.edges)

    // Get arguments and results nodes directly from the graph
    let argumentsNode = toWebNode(material.arguments)
    let resultsNode = toWebNode(material.results)

    // Convert inputs - filter out invalid material input types
    let inputs = toWebInputOutputs(material.inputs)

    // Convert outputs
    let outputs = toWebOutputs(material.outputs)

    return WKBridgeMaterialGraph(
        nodes: nodes,
        edges: edges,
        arguments: argumentsNode,
        results: resultsNode,
        inputs: inputs,
        outputs: outputs
    )
}

func webUpdateMaterialRequestFromUpdateMaterialRequest(
    _ request: _Proto_MaterialDataUpdate_v1
) -> WKBridgeUpdateMaterial {
    #if canImport(RealityCoreRenderer, _version: 12)
    let bridgeMaterialGraph = toWebMaterialGraph(request.shaderGraph)
    return WKBridgeUpdateMaterial(
        materialGraph: bridgeMaterialGraph,
        identifier: request.identifier
    )
    #else
    let bridgeMaterialGraph = toWebMaterialGraph(nil)
    return WKBridgeUpdateMaterial(
        materialGraph: bridgeMaterialGraph,
        identifier: request.identifier
    )
    #endif
}

extension ShaderGraph._Proto_ShaderNodeGraph {
    static func fromWKDescriptor(_ descriptor: WKBridgeMaterialGraph?) -> _Proto_ShaderNodeGraph? {
        guard let descriptor else { return nil }

        do {
            // Create the shader graph with name, inputs, and outputs
            let graph = try _Proto_ShaderNodeGraph(
                named: "MaterialGraph",
                inputs: descriptor.inputs.map { input in
                    // Create SemanticType if hasSemanticType is true
                    let semanticType: _Proto_ShaderGraphNodeDefinition.SemanticType?
                    let actualType: _Proto_ShaderDataType

                    if input.hasSemanticType {
                        // Determine the semantic type name from the WKBridgeDataType
                        let semanticName: String
                        switch input.semanticType {
                        case .color3f, .color3h:
                            semanticName = "color3f"
                            actualType = .cgColor3
                        case .color4f, .color4h:
                            semanticName = "color4f"
                            actualType = .cgColor4
                        default:
                            semanticName = "\(input.semanticType)"
                            actualType = fromWKBridgeDataType(input.type)
                        }
                        semanticType = _Proto_ShaderGraphNodeDefinition.SemanticType(name: semanticName, values: nil)
                    } else {
                        semanticType = nil
                        actualType = fromWKBridgeDataType(input.type)
                    }

                    let defaultValue: _Proto_ShaderGraphValue?
                    if let container = input.defaultValue {
                        defaultValue = fromWKBridgeConstant(container)
                    } else {
                        defaultValue = nil
                    }

                    return _Proto_ShaderGraphNodeDefinition.Input(
                        name: input.name,
                        type: actualType,
                        semanticType: semanticType,
                        defaultValue: defaultValue
                    )
                },
                outputs: descriptor.outputs.map { output in
                    // Create SemanticType if hasSemanticType is true
                    let semanticType: _Proto_ShaderGraphNodeDefinition.SemanticType?
                    let actualType: _Proto_ShaderDataType

                    if output.hasSemanticType {
                        // Determine the semantic type name from the WKBridgeDataType
                        let semanticName: String
                        switch output.semanticType {
                        case .color3f, .color3h:
                            semanticName = "color3f"
                            actualType = .cgColor3
                        case .color4f, .color4h:
                            semanticName = "color4f"
                            actualType = .cgColor4
                        default:
                            semanticName = "\(output.semanticType)"
                            actualType = fromWKBridgeDataType(output.type)
                        }
                        semanticType = _Proto_ShaderGraphNodeDefinition.SemanticType(name: semanticName, values: nil)
                    } else {
                        semanticType = nil
                        actualType = fromWKBridgeDataType(output.type)
                    }

                    let defaultValue: _Proto_ShaderGraphValue?
                    if let container = output.defaultValue {
                        defaultValue = fromWKBridgeConstant(container)
                    } else {
                        defaultValue = nil
                    }

                    return _Proto_ShaderGraphNodeDefinition.Output(
                        name: output.name,
                        type: actualType,
                        semanticType: semanticType,
                        defaultValue: defaultValue
                    )
                }
            )

            // Get the shared shader graph library for looking up builtin definitions
            let library = _Proto_ShaderNodeGraphLibrary.shared
            // First pass: build a map to look up which edges connect to which inputs
            // This helps us determine the expected type for constant nodes
            var constantToInputType: [String: _Proto_ShaderDataType] = [:]
            for edge in descriptor.edges {
                // Find the definition of the input node to get the expected input type
                if let inputNodeBridge = descriptor.nodes.first(where: { ($0.builtin?.name ?? $0.constant?.name) == edge.inputNode }),
                    let builtin = inputNodeBridge.builtin,
                    !builtin.definition.isEmpty,
                    let definition = library.definition(named: builtin.definition)
                {
                    // Find the input port in the definition
                    if let input = definition.inputs.first(where: { $0.name == edge.inputPort }) {
                        constantToInputType[edge.outputNode] = input.type
                    }
                }
            }

            // Convert bridge nodes to a dictionary of nodes
            var nodesDictionary: [String: _Proto_ShaderNodeGraph.Node] = [:]
            for bridgeNode in descriptor.nodes {
                let nodeName = bridgeNode.builtin?.name ?? bridgeNode.constant?.name ?? "unknown"

                switch bridgeNode.bridgeNodeType {
                case .constant:
                    // Handle constant nodes
                    if let constant = bridgeNode.constant {
                        // Check if this constant feeds into an input that expects a color type
                        var value = fromWKBridgeConstant(constant)
                        // If this constant is float4 but connects to a cgColor4 input, convert it
                        if case .float4(let vec) = value,
                            let expectedType = constantToInputType[nodeName],
                            expectedType == .cgColor4
                        {
                            value = .cgColor4(
                                CGColor(
                                    red: CGFloat(vec.x),
                                    green: CGFloat(vec.y),
                                    blue: CGFloat(vec.z),
                                    alpha: CGFloat(vec.w)
                                )
                            )
                        }
                        let node = _Proto_ShaderNodeGraph.Node(name: nodeName, data: .constant(value))
                        nodesDictionary[nodeName] = node
                    }

                case .builtin, .arguments, .results:
                    // Handle builtin nodes by looking up their definitions
                    if let builtin = bridgeNode.builtin, !builtin.definition.isEmpty {
                        if let definition = library.definition(named: builtin.definition) {
                            let node = _Proto_ShaderNodeGraph.Node(name: nodeName, data: .definition(definition))
                            nodesDictionary[nodeName] = node
                        } else {
                            logError("Could not find builtin definition named '\(builtin.definition)' for node '\(nodeName)'")
                        }
                    }

                @unknown default:
                    fatalError("Unknown node type for node '\(nodeName)'")
                }
            }

            // Convert bridge edges to edges
            let edges = descriptor.edges.map { bridgeEdge in
                _Proto_ShaderNodeGraph.Edge(
                    outputNode: bridgeEdge.outputNode,
                    outputPort: bridgeEdge.outputPort,
                    inputNode: bridgeEdge.inputNode,
                    inputPort: bridgeEdge.inputPort
                )
            }

            // Use replace to set nodes and edges
            try graph.replace(nodes: nodesDictionary, edges: edges)

            return graph
        } catch {
            logError("Failed to create ShaderNodeGraph: \(error)")
            return nil
        }
    }
}

private func fromWKBridgeDataType(_ dataType: WKBridgeDataType) -> _Proto_ShaderDataType {
    switch dataType {
    case .bool: .bool
    case .uchar: .uchar
    case .int: .int
    case .uint: .uint
    case .int2: .int2
    case .int3: .int3
    case .int4: .int4
    case .float: .float
    case .color3f: .cgColor3
    case .color3h: .cgColor3 // Map to cgColor3, closest match
    case .color4f: .cgColor4
    case .color4h: .cgColor4 // Map to cgColor4, closest match
    case .float2: .float2
    case .float3: .float3
    case .float4: .float4
    case .half: .half
    case .half2: .half2
    case .half3: .half3
    case .half4: .half4
    case .matrix2f: .float2x2
    case .matrix3f: .float3x3
    case .matrix4f: .float4x4
    case .matrix2h: .half2x2
    case .matrix3h: .half3x3
    case .matrix4h: .half4x4
    case .quat: .quaternion
    case .surfaceShader: .surfaceShader
    case .geometryModifier: .geometryModifier
    case .postLightingShader: .postLightingShader
    case .string: .string
    case .token: .string // Map token to string
    case .asset: .filename // Map asset to filename
    @unknown default: .invalid
    }
}

private func fromWKBridgeConstant(_ constant: WKBridgeConstantContainer) -> _Proto_ShaderGraphValue {
    let values = constant.constantValues

    switch constant.constant {
    case .bool:
        return .bool(values.first?.number.boolValue ?? false)
    case .uchar:
        return .uchar(values.first?.number.uint8Value ?? 0)
    case .int:
        return .int(Int32(values.first?.number.intValue ?? 0))
    case .uint:
        return .uint(UInt32(values.first?.number.uintValue ?? 0))
    case .half:
        return .half(values.first?.number.uint16Value ?? 0)
    case .float:
        return .float(values.first?.number.floatValue ?? 0)
    case .string, .token, .asset:
        let stringValue = values.first?.string ?? ""
        if stringValue.isEmpty && !values.isEmpty {
            logInfo(
                "⚠️ DEBUG: String value is empty for node '\(constant.name)'. values.count=\(values.count), first value=\(String(describing: values.first))"
            )
        }
        return .string(stringValue)
    case .timecode:
        // timecode maps to float
        return .float(Float(values.first?.number.doubleValue ?? 0))
    case .float2, .texCoord2f:
        guard values.count >= 2 else { return .float2(.zero) }
        return .float2(
            SIMD2<Float>(
                values[0].number.floatValue,
                values[1].number.floatValue
            )
        )
    case .vector3f, .float3, .point3f, .normal3f, .texCoord3f:
        // All float3-based semantic types map to float3
        guard values.count >= 3 else { return .float3(.zero) }
        return .float3(
            SIMD3<Float>(
                values[0].number.floatValue,
                values[1].number.floatValue,
                values[2].number.floatValue
            )
        )
    case .float4, .matrix2f:
        guard values.count >= 4 else { return .float4(.zero) }
        return .float4(
            SIMD4<Float>(
                values[0].number.floatValue,
                values[1].number.floatValue,
                values[2].number.floatValue,
                values[3].number.floatValue
            )
        )
    case .half2, .texCoord2h:
        // half2-based semantic types map to half2
        guard values.count >= 2 else { return .half2(.zero) }
        return .half2(
            SIMD2<UInt16>(
                values[0].number.uint16Value,
                values[1].number.uint16Value
            )
        )
    case .vector3h, .half3, .point3h, .normal3h, .texCoord3h:
        // All half3-based semantic types map to half3
        guard values.count >= 3 else { return .half3(.zero) }
        return .half3(
            SIMD3<UInt16>(
                values[0].number.uint16Value,
                values[1].number.uint16Value,
                values[2].number.uint16Value
            )
        )
    case .half4:
        guard values.count >= 4 else { return .half4(.zero) }
        return .half4(
            SIMD4<UInt16>(
                values[0].number.uint16Value,
                values[1].number.uint16Value,
                values[2].number.uint16Value,
                values[3].number.uint16Value
            )
        )
    case .int2:
        guard values.count >= 2 else { return .int2(.zero) }
        return .int2(
            SIMD2<Int32>(
                values[0].number.int32Value,
                values[1].number.int32Value
            )
        )
    case .int3:
        guard values.count >= 3 else { return .int3(.zero) }
        return .int3(
            SIMD3<Int32>(
                values[0].number.int32Value,
                values[1].number.int32Value,
                values[2].number.int32Value
            )
        )
    case .int4:
        guard values.count >= 4 else { return .int4(.zero) }
        return .int4(
            SIMD4<Int32>(
                values[0].number.int32Value,
                values[1].number.int32Value,
                values[2].number.int32Value,
                values[3].number.int32Value
            )
        )
    case .matrix3f:
        // matrix3f maps to float3x3 - needs 9 values (3 columns of 3 rows each)
        guard values.count >= 9 else {
            // Return identity matrix if values are missing
            return .float3x3(
                SIMD3<Float>(1, 0, 0),
                SIMD3<Float>(0, 1, 0),
                SIMD3<Float>(0, 0, 1)
            )
        }
        return .float3x3(
            SIMD3<Float>(values[0].number.floatValue, values[1].number.floatValue, values[2].number.floatValue),
            SIMD3<Float>(values[3].number.floatValue, values[4].number.floatValue, values[5].number.floatValue),
            SIMD3<Float>(values[6].number.floatValue, values[7].number.floatValue, values[8].number.floatValue)
        )
    case .matrix4f:
        // matrix4f maps to float4x4 - needs 16 values (4 columns of 4 rows each)
        guard values.count >= 16 else {
            // Return identity matrix if values are missing
            return .float4x4(
                SIMD4<Float>(1, 0, 0, 0),
                SIMD4<Float>(0, 1, 0, 0),
                SIMD4<Float>(0, 0, 1, 0),
                SIMD4<Float>(0, 0, 0, 1)
            )
        }
        return .float4x4(
            SIMD4<Float>(
                values[0].number.floatValue,
                values[1].number.floatValue,
                values[2].number.floatValue,
                values[3].number.floatValue
            ),
            SIMD4<Float>(
                values[4].number.floatValue,
                values[5].number.floatValue,
                values[6].number.floatValue,
                values[7].number.floatValue
            ),
            SIMD4<Float>(
                values[8].number.floatValue,
                values[9].number.floatValue,
                values[10].number.floatValue,
                values[11].number.floatValue
            ),
            SIMD4<Float>(
                values[12].number.floatValue,
                values[13].number.floatValue,
                values[14].number.floatValue,
                values[15].number.floatValue
            )
        )
    case .quatf, .quath:
        // quath/quatf don't exist in the enum - map to float4
        guard values.count >= 4 else { return .float4(.zero) }
        return .float4(
            SIMD4<Float>(
                values[0].number.floatValue,
                values[1].number.floatValue,
                values[2].number.floatValue,
                values[3].number.floatValue
            )
        )
    case .color3f, .color3h:
        // color3f/h map to cgColor3
        guard values.count >= 3 else {
            // Return a default black color if values are missing
            return .cgColor3(CGColor(red: 0, green: 0, blue: 0, alpha: 1))
        }
        let red = CGFloat(values[0].number.floatValue)
        let green = CGFloat(values[1].number.floatValue)
        let blue = CGFloat(values[2].number.floatValue)
        return .cgColor3(CGColor(red: red, green: green, blue: blue, alpha: 1))
    case .color4f, .color4h:
        // color4f/h map to cgColor4
        guard values.count >= 4 else {
            // Return a default transparent black color if values are missing
            return .cgColor4(CGColor(red: 0, green: 0, blue: 0, alpha: 0))
        }
        let red = CGFloat(values[0].number.floatValue)
        let green = CGFloat(values[1].number.floatValue)
        let blue = CGFloat(values[2].number.floatValue)
        let alpha = CGFloat(values[3].number.floatValue)
        return .cgColor4(CGColor(red: red, green: green, blue: blue, alpha: alpha))
    @unknown default:
        return .string("")
    }
}

final class USDModelLoader: _Proto_UsdStageSession_v1.Delegate {
    fileprivate let usdLoader: _Proto_UsdStageSession_v1
    fileprivate var stage: UsdStage?
    fileprivate var data: Data?
    private let objcLoader: WKBridgeModelLoader

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
    @nonobjc
    fileprivate var loop: Bool = false

    init(objcInstance: WKBridgeModelLoader) {
        objcLoader = objcInstance
        usdLoader = _Proto_UsdStageSession_v1.noMetalSession(gpuFamily: MTLGPUFamily.apple7)
        dispatchSerialQueue = DispatchSerialQueue(label: "USDModelWebProcess", qos: .userInteractive)
        usdLoader.delegate = self
    }

    func iblTextureUpdated(data: consuming sending _Proto_TextureDataUpdate_v1) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=299480 - [Model element] Support `environmentMap` attribute in GPU process model element
    }

    func iblTextureDestroyed(identifier: _Proto_ResourceId) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=303906
    }

    func meshUpdated(data: consuming sending _Proto_MeshDataUpdate_v1) {
        self.dispatchSerialQueue.async {
            self.objcLoader.updateMesh(webRequest: webUpdateMeshRequestFromUpdateMeshRequest(data))
        }
    }

    func meshDestroyed(identifier: String) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=303906
    }

    func materialUpdated(data: consuming sending _Proto_MaterialDataUpdate_v1) {
        self.dispatchSerialQueue.async {
            self.objcLoader.updateMaterial(webRequest: webUpdateMaterialRequestFromUpdateMaterialRequest(data))
        }
    }

    func materialDestroyed(identifier: String) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=303906
    }

    func textureUpdated(data: consuming sending _Proto_TextureDataUpdate_v1) {
        self.dispatchSerialQueue.async {
            self.objcLoader.updateTexture(webRequest: webUpdateTextureRequestFromUpdateTextureRequest(data))
        }
    }

    func textureDestroyed(identifier: String) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=303906
    }

    func loadModel(from url: Foundation.URL) {
        do {
            let stage = try UsdStage(contentsOf: url)
            self.setupTimes(from: stage)
            self.usdLoader.loadStage(stage)
        } catch {
            fatalError(error.localizedDescription)
        }
    }

    func loadModel(data: Foundation.Data) {
        do {
            self.data = data
            // swift-format-ignore: NeverForceUnwrap
            self.stage = try UsdStage.open(buffer: self.data!)
            guard let stage = self.stage else {
                logError("model data is corrupted")
                return
            }
            self.setupTimes(from: stage)
            self.usdLoader.loadStage(stage)
        } catch {
            fatalError(error.localizedDescription)
        }
    }

    func setupTimes(from stage: UsdStage) {
        timeCodePerSecond = stage.timeCodesPerSecond > 0 ? stage.timeCodesPerSecond : 1
        startTime = stage.startTimeCode / timeCodePerSecond
        endTime = stage.endTimeCode / timeCodePerSecond
    }

    func duration() -> Double {
        endTime - startTime
    }

    func currentTime() -> Double {
        time - startTime
    }

    func setCurrentTime(_ newTime: Double) {
        time = startTime + newTime
    }

    func loadModel(from data: Data) {
    }

    func update(deltaTime: TimeInterval) {
        let newTime = currentTime() + deltaTime
        let adjustedTime: TimeInterval
        if loop {
            let modValue = max(duration(), 1)
            var wrappedTime = fmod(newTime, modValue)
            if wrappedTime < 0 {
                wrappedTime += modValue
            }
            adjustedTime = wrappedTime
        } else {
            adjustedTime = max(0, min(newTime, duration()))
        }
        time = startTime + adjustedTime
        usdLoader.update(time: time * timeCodePerSecond)
    }
}

@objc
@implementation
extension WKBridgeModelLoader {
    @nonobjc
    var loader: USDModelLoader?
    @nonobjc
    var modelUpdated: ((WKBridgeUpdateMesh) -> (Void))?
    @nonobjc
    var textureUpdatedCallback: ((WKBridgeUpdateTexture) -> (Void))?
    @nonobjc
    var materialUpdatedCallback: ((WKBridgeUpdateMaterial) -> (Void))?

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
        _ modelUpdatedCallback: @escaping ((WKBridgeUpdateMesh) -> (Void)),
        textureUpdatedCallback: @escaping ((WKBridgeUpdateTexture) -> (Void)),
        materialUpdatedCallback: @escaping ((WKBridgeUpdateMaterial) -> (Void))
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
    func loadModel(_ data: Foundation.Data) {
        self.loader?.loadModel(data: data)
    }

    @objc
    func update(_ deltaTime: Double) {
        self.loader?.update(deltaTime: deltaTime)
    }

    @objc
    func setLoop(_ loop: Bool) {
        self.loader?.loop = loop
    }

    @objc
    func requestCompleted(_ request: NSObject) {
        retainedRequests.remove(request)
    }

    @objc
    func duration() -> Double {
        guard let loader else {
            return 0.0
        }
        return loader.duration()
    }

    @objc
    func currentTime() -> Double {
        guard let loader else {
            return 0.0
        }
        return loader.currentTime()
    }

    @objc
    func setCurrentTime(_ newTime: Double) {
        loader?.setCurrentTime(newTime)
    }

    fileprivate func updateMesh(webRequest: WKBridgeUpdateMesh) {
        if let modelUpdated {
            retainedRequests.insert(webRequest)
            modelUpdated(webRequest)
        }
    }

    fileprivate func updateTexture(webRequest: WKBridgeUpdateTexture) {
        if let textureUpdatedCallback {
            retainedRequests.insert(webRequest)
            textureUpdatedCallback(webRequest)
        }
    }

    fileprivate func updateMaterial(webRequest: WKBridgeUpdateMaterial) {
        if let materialUpdatedCallback {
            retainedRequests.insert(webRequest)
            materialUpdatedCallback(webRequest)
        }
    }
}

private func makeFallBackTextureResource(
    _ renderContext: any _Proto_LowLevelRenderContext_v1,
    commandQueue: any MTLCommandQueue,
    device: any MTLDevice
) -> _Proto_LowLevelTextureResource_v1 {
    // Create 1x1 white fallback texture
    let fallbackDescriptor = _Proto_LowLevelTextureResource_v1.Descriptor(
        textureType: .type2D,
        pixelFormat: .rgba8Unorm,
        width: 1,
        height: 1,
        mipmapLevelCount: 1,
        textureUsage: .shaderRead
    )
    // swift-format-ignore: NeverUseForceTry
    let fallbackTexture = try! renderContext.makeTextureResource(descriptor: fallbackDescriptor)

    // White color: RGBA = (255, 255, 255, 255)
    let whitePixel: [UInt8] = [255, 255, 255, 255]

    // Create staging buffer for white pixel data
    let stagingBuffer = unsafe whitePixel.withUnsafeBytes { bytes in
        // swift-format-ignore: NeverForceUnwrap
        unsafe device.makeBuffer(bytes: bytes.baseAddress!, length: bytes.count, options: .storageModeShared)!
    }

    // Create command buffer to upload white pixel data
    // swift-format-ignore: NeverForceUnwrap
    let fallbackCommandBuffer = commandQueue.makeCommandBuffer()!
    let fallbackMTLTexture = fallbackTexture.replace(using: fallbackCommandBuffer)

    // Use blit encoder to copy from buffer to texture
    // swift-format-ignore: NeverForceUnwrap
    let blitEncoder = fallbackCommandBuffer.makeBlitCommandEncoder()!
    blitEncoder.copy(
        from: stagingBuffer,
        sourceOffset: 0,
        sourceBytesPerRow: 4,
        sourceBytesPerImage: 4,
        sourceSize: MTLSize(width: 1, height: 1, depth: 1),
        to: fallbackMTLTexture,
        destinationSlice: 0,
        destinationLevel: 0,
        destinationOrigin: MTLOrigin(x: 0, y: 0, z: 0)
    )
    blitEncoder.endEncoding()

    fallbackCommandBuffer.commit()

    return fallbackTexture
}

extension WKBridgeReceiver {
    internal func configureDeformation(
        identifier: _Proto_ResourceId,
        deformationData: WKBridgeDeformationData,
        commandBuffer: any MTLCommandBuffer
    ) {
        var deformers: [any _Proto_LowLevelDeformerDescription_v1] = []

        if let skinningData = deformationData.skinningData {
            let skinningDeformer = skinningData.makeDeformerDescription(device: self.device, memoryOwner: self.memoryOwner)
            deformers.append(skinningDeformer)
        }

        if let blendShapeData = deformationData.blendShapeData {
            do {
                let blendShapeDeformer = try blendShapeData.makeDeformerDescription(device: self.device, memoryOwner: self.memoryOwner)
                deformers.append(blendShapeDeformer)
            } catch {
                logError("Error creating blend shape deformer for \(identifier): \(error.localizedDescription)")
            }
        }

        // TODO: add tangent frame data to input
        // if let renormalizationData = deformationData.renormalizationData {
        //     do {
        //         let renormalization = try renormalizationData.makeDeformerDescription(device: self.device, memoryOwner: self.memoryOwner)
        //         deformers.append(renormalization)
        //     } catch {
        //         logError("Error creating renormalization deformer for \(identifier): \(error.localizedDescription)")
        //     }
        // }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let meshResource = meshResources[identifier]!

        var inputMeshDescription: _Proto_LowLevelDeformationDescription_v1.MeshDescription?
        if self.meshResourceToDeformationContext[identifier] == nil {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let vertexPositionsBuffer = meshResource.readVertices(at: 1, using: commandBuffer)!
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let inputPositionsBuffer = unsafe device.makeBuffer(length: vertexPositionsBuffer.length, options: .storageModeShared)!
            inputPositionsBuffer.__setOwnerWithIdentity(self.memoryOwner)

            // Copy data from vertexPositionsBuffer to inputPositionsBuffer
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let blitEncoder = commandBuffer.makeBlitCommandEncoder()!
            blitEncoder.copy(
                from: vertexPositionsBuffer,
                sourceOffset: 0,
                to: inputPositionsBuffer,
                destinationOffset: 0,
                size: vertexPositionsBuffer.length
            )
            blitEncoder.endEncoding()

            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let inputPositions = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
                inputPositionsBuffer,
                offset: 0,
                occupiedLength: inputPositionsBuffer.length,
                elementType: .float3
            )!
            inputMeshDescription = _Proto_LowLevelDeformationDescription_v1.MeshDescription(descriptions: [
                _Proto_LowLevelDeformationDescription_v1.SemanticBuffer(.position, inputPositions)
            ])
        } else {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            inputMeshDescription = self.meshResourceToDeformationContext[identifier]!.description.input
        }

        guard let inputMeshDescription else {
            logError("inputMeshDescription is unexpectedly nil")
            return
        }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let outputPositionsBuffer = meshResource.replaceVertices(at: 1, using: commandBuffer)!
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let outputPositions = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            outputPositionsBuffer,
            offset: 0,
            occupiedLength: outputPositionsBuffer.length,
            elementType: .float3
        )!

        let outputMeshDescription = _Proto_LowLevelDeformationDescription_v1.MeshDescription(descriptions: [
            _Proto_LowLevelDeformationDescription_v1.SemanticBuffer(.position, outputPositions)
        ])

        guard
            let deformationDescription =
                try? _Proto_LowLevelDeformationDescription_v1.make(
                    input: inputMeshDescription,
                    deformers: deformers,
                    output: outputMeshDescription
                )
                .get()
        else {
            logError("_Proto_LowLevelDeformationDescription_v1.make failed unexpectedly")
            return
        }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        guard let deformation = try? self.deformationSystem.make(description: deformationDescription).get() else {
            logError("deformationSystem.make failed unexpectedly")
            return
        }

        self.meshResourceToDeformationContext[identifier] = .init(
            deformation: deformation,
            description: deformationDescription,
            dirty: true
        )
    }
}

extension WKBridgeSkinningData {
    fileprivate func makeDeformerDescription(device: any MTLDevice, memoryOwner: mach_port_t) -> any _Proto_LowLevelDeformerDescription_v1 {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointTransformsBuffer = unsafe device.makeBuffer(
            bytes: self.jointTransforms,
            length: self.jointTransforms.count * MemoryLayout<simd_float4x4>.size,
            options: .storageModeShared
        )!
        jointTransformsBuffer.__setOwnerWithIdentity(memoryOwner)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointTransformsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            jointTransformsBuffer,
            offset: 0,
            occupiedLength: jointTransformsBuffer.length,
            elementType: .float4x4
        )!

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let inverseBindPosesBuffer = unsafe device.makeBuffer(
            bytes: self.inverseBindPoses,
            length: self.inverseBindPoses.count * MemoryLayout<simd_float4x4>.size,
            options: .storageModeShared
        )!
        inverseBindPosesBuffer.__setOwnerWithIdentity(memoryOwner)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let inverseBindPosesDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            inverseBindPosesBuffer,
            offset: 0,
            occupiedLength: inverseBindPosesBuffer.length,
            elementType: .float4x4
        )!

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointIndicesBuffer = unsafe device.makeBuffer(
            bytes: self.influenceJointIndices,
            length: self.influenceJointIndices.count * MemoryLayout<UInt32>.size,
            options: .storageModeShared
        )!
        jointIndicesBuffer.__setOwnerWithIdentity(memoryOwner)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointIndicesDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            jointIndicesBuffer,
            offset: 0,
            occupiedLength: jointIndicesBuffer.length,
            elementType: .uint
        )!

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let influenceWeightsBuffer = unsafe device.makeBuffer(
            bytes: self.influenceWeights,
            length: self.influenceWeights.count * MemoryLayout<Float>.size,
            options: .storageModeShared
        )!
        influenceWeightsBuffer.__setOwnerWithIdentity(memoryOwner)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let influenceWeightsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            influenceWeightsBuffer,
            offset: 0,
            occupiedLength: influenceWeightsBuffer.length,
            elementType: .float
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
    func makeDeformerDescription(device: any MTLDevice, memoryOwner: task_id_token_t) throws -> any _Proto_LowLevelDeformerDescription_v1 {
        var weights: [Float] = []

        var debugWeights = self.weights
        var debugPositionOffsets = self.positionOffsets

        let blendTargetCount = self.weights.count
        let positionCount = self.positionOffsets[0].count
        for i in 0..<blendTargetCount {
            weights += Array(repeating: debugWeights[i], count: positionCount)
        }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let blendWeightsBuffer = unsafe device.makeBuffer(
            bytes: weights,
            length: weights.count * MemoryLayout<Float>.size,
            options: .storageModeShared
        )!
        blendWeightsBuffer.__setOwnerWithIdentity(memoryOwner)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let blendWeightsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            blendWeightsBuffer,
            offset: 0,
            occupiedLength: blendWeightsBuffer.length,
            elementType: .float
        )!

        let positionOffsets = debugPositionOffsets.flatMap(\.self)
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let positionOffsetsBuffer = unsafe device.makeBuffer(
            bytes: positionOffsets,
            length: positionOffsets.count * MemoryLayout<SIMD3<Float>>.size,
            options: .storageModeShared
        )!
        positionOffsetsBuffer.__setOwnerWithIdentity(memoryOwner)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let positionOffsetsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            positionOffsetsBuffer,
            offset: 0,
            occupiedLength: positionOffsetsBuffer.length,
            elementType: .float3
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
    func makeDeformerDescription(device: any MTLDevice, memoryOwner: task_id_token_t) throws -> any _Proto_LowLevelDeformerDescription_v1 {
        // Create adjacency buffer
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let adjacenciesMetalBuffer = unsafe device.makeBuffer(
            bytes: vertexAdjacencies,
            length: vertexAdjacencies.count * MemoryLayout<UInt32>.size,
            options: .storageModeShared
        )!
        adjacenciesMetalBuffer.__setOwnerWithIdentity(memoryOwner)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let adjacenciesBuffer = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            adjacenciesMetalBuffer,
            offset: 0,
            occupiedLength: adjacenciesMetalBuffer.length,
            elementType: .uint
        )!

        // Create adjacency end indices buffer
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let adjacencyEndIndicesMetalBuffer = unsafe device.makeBuffer(
            bytes: vertexAdjacencyEndIndices,
            length: vertexAdjacencyEndIndices.count * MemoryLayout<UInt32>.size,
            options: .storageModeShared
        )!
        adjacencyEndIndicesMetalBuffer.__setOwnerWithIdentity(memoryOwner)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let adjacencyEndIndicesBuffer = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            adjacencyEndIndicesMetalBuffer,
            offset: 0,
            occupiedLength: adjacencyEndIndicesMetalBuffer.length,
            elementType: .uint
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

#else
@objc
@implementation
extension WKBridgeUSDConfiguration {
    init(device: any MTLDevice, memoryOwner: task_id_token_t) {
    }

    @objc(createMaterialCompiler:)
    func createMaterialCompiler() async {
    }
}

@objc
@implementation
extension WKBridgeReceiver {
    init(
        configuration: WKBridgeUSDConfiguration,
        diffuseAsset: WKBridgeImageAsset,
        specularAsset: WKBridgeImageAsset
    ) throws {
    }

    @objc(renderWithTexture:)
    func render(with texture: any MTLTexture) {
    }

    @objc(updateTexture:)
    func updateTexture(_ data: WKBridgeUpdateTexture) {
    }

    @objc(updateMaterial:completionHandler:)
    func updateMaterial(_ data: WKBridgeUpdateMaterial) async {
    }

    @objc(updateMesh:completionHandler:)
    func updateMesh(_ data: WKBridgeUpdateMesh) async {
    }

    @objc(setTransform:)
    func setTransform(_ transform: simd_float4x4) {
    }

    @objc
    func setCameraDistance(_ distance: Float) {
    }

    @objc
    func setBackgroundColor(_ color: simd_float3) {
    }

    @objc
    func setPlaying(_ play: Bool) {
    }

    @objc
    func setEnvironmentMap(_ imageAsset: WKBridgeImageAsset) {
    }
}

@objc
@implementation
extension WKBridgeModelLoader {
    override init() {
        super.init()
    }

    @objc(
        setCallbacksWithModelUpdatedCallback:
        textureUpdatedCallback:
        materialUpdatedCallback:
    )
    func setCallbacksWithModelUpdatedCallback(
        _ modelUpdatedCallback: @escaping ((WKBridgeUpdateMesh) -> (Void)),
        textureUpdatedCallback: @escaping ((WKBridgeUpdateTexture) -> (Void)),
        materialUpdatedCallback: @escaping ((WKBridgeUpdateMaterial) -> (Void))
    ) {
    }

    @objc
    func loadModel(from url: Foundation.URL) {
    }

    @objc
    func loadModel(_ data: Foundation.Data) {
    }

    @objc
    func update(_ deltaTime: Double) {
    }

    @objc
    func setLoop(_ loop: Bool) {
    }

    @objc
    func requestCompleted(_ request: NSObject) {
    }

    @objc
    func duration() -> Double {
        0.0
    }

    @objc
    func currentTime() -> Double {
        0.0
    }

    @objc
    func setCurrentTime(_ newTime: Double) {
    }
}
#endif
