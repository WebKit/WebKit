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
import OSLog
import WebKit
import simd

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreRenderer, _version: 11) && compiler(>=6.2)
@_weakLinked @_spi(UsdLoaderAPI) internal import _USDKit_RealityKit
@_spi(RealityCoreRendererAPI) import RealityKit
@_weakLinked @_spi(RealityCoreTextureProcessingAPI) internal import RealityCoreTextureProcessing
@_weakLinked internal import USDKit
@_weakLinked @_spi(SwiftAPI) internal import DirectResource
@_weakLinked internal import USDKit
@_weakLinked internal import _USDKit_RealityKit
import RealityKit
@_weakLinked @_spi(SGPrivate) internal import ShaderGraph
@_weakLinked internal import RealityCoreDeformation

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

    unsafe imageAssetData.bytes.withUnsafeBytes { textureBytes in
        guard let textureBytesBaseAddress = textureBytes.baseAddress else {
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
    textureResources: [String: _Proto_LowLevelTextureResource_v1]
) throws -> _Proto_LowLevelArgumentTable_v1? {
    guard let function else { return nil }
    guard let argumentTableDescriptor = function.argumentTableDescriptor else { return nil }
    let parameterMapping = function.parameterMapping

    var optTextures: [_Proto_LowLevelTextureResource_v1?] = argumentTableDescriptor.textures.map({ _ in nil })
    for parameter in parameterMapping?.textures ?? [] {
        guard let textureResource = textureResources[parameter.name] else {
            fatalError("Failed to find texture resource \(parameter.name)")
        }
        optTextures[parameter.textureIndex] = textureResource
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
            try await self.appRenderer.createMaterialCompiler(colorPixelFormat: .bgra8Unorm_srgb, rasterSampleCount: 4)
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
        #if canImport(RealityCoreRenderer, _version: 9999)
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

            guard let materialSourceArchive = data.materialGraph else {
                logError("No materialGraph data provided for material \(identifier)")
                return
            }

            let shaderGraphOutput = try await renderContext.makeShaderGraphFunctions(data.materialGraph)

            let geometryArguments = try makeParameters(
                for: shaderGraphOutput.geometryModifier,
                renderContext: renderContext,
                textureResources: textureResources
            )
            let surfaceArguments = try makeParameters(
                for: shaderGraphOutput.surfaceShader,
                renderContext: renderContext,
                textureResources: textureResources
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
            #if canImport(RealityCoreRenderer, _version: 9999)
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

func webUpdateMaterialRequestFromUpdateMaterialRequest(
    _ request: _Proto_MaterialDataUpdate_v1
) -> WKBridgeUpdateMaterial {
    WKBridgeUpdateMaterial(
        materialGraph: request.materialSourceArchive,
        identifier: request.identifier,
        geometryModifierFunctionReference: nil,
        surfaceShaderFunctionReference: nil,
        shaderGraphModule: nil,
    )
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
        time = startTime + fmod(newTime, max(duration(), 1))
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
