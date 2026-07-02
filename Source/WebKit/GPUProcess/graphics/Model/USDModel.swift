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
import UniformTypeIdentifiers

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

extension LowLevelTextureResource {
    func replace(commandBuffer: any MTLCommandBuffer, memoryOwner: task_id_token_t) -> any MTLTexture {
        let texture = replace(commandBuffer: commandBuffer)
        texture.__setOwnerWithIdentity(memoryOwner)
        return texture
    }
}

private func makeMTLTextureFromImageAsset(
    _ imageAsset: WKBridgeImageAsset,
    device: any MTLDevice,
    generateMips: Bool,
    memoryOwner: task_id_token_t,
    layout: [WKBridgeTextureLevelInfo]
) -> ((any MTLTexture), Int)? {
    guard let imageAssetData = imageAsset.data else {
        logError("no image data")
        return nil
    }
    logInfo(
        "imageAssetData = \(imageAssetData)  -  width = \(imageAsset.width)  -  height = \(imageAsset.height)  - imageAsset.pixelFormat:  \(imageAsset.pixelFormat)"
    )

    let pixelFormat = imageAsset.pixelFormat

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

    func mipDimension(_ base: Int, level: Int) -> Int {
        max(1, base >> level)
    }

    guard let bytesPerPixel = imageAsset.pixelFormat.bytesPerPixel else {
        fatalError("unexpected pixel format \(imageAsset.pixelFormat)")
    }

    var mipLevelsInData = 0
    unsafe imageAssetData.bytes.withUnsafeBytes { textureBytes in
        guard let baseAddress = textureBytes.baseAddress else {
            logError("nil base address in imageAssetData")
            return
        }

        func uploadSlice(face: Int, mipLevel: Int, bytesOffset: Int, bytesPerRow: Int, bytesPerImage: Int) {
            let mipWidth = mipDimension(imageAsset.width, level: mipLevel)
            let mipHeight = mipDimension(imageAsset.height, level: mipLevel)
            unsafe mtlTexture.replace(
                region: MTLRegionMake2D(0, 0, mipWidth, mipHeight),
                mipmapLevel: mipLevel,
                slice: face,
                withBytes: unsafe baseAddress.advanced(by: bytesOffset),
                bytesPerRow: bytesPerRow,
                bytesPerImage: bytesPerImage
            )
        }

        if !layout.isEmpty {
            // Use the pre-computed per-mip layout transmitted over IPC.
            // layout is indexed by mip level; each entry covers all slices at that level.
            mipLevelsInData = min(layout.count, mtlTexture.mipmapLevelCount)
            logInfo("uploading \(mipLevelsInData) of \(mtlTexture.mipmapLevelCount) mip level(s) from imageAssetData (layout-driven)")
            for face in 0..<sliceCount {
                for mipLevel in 0..<mipLevelsInData {
                    let info = layout[mipLevel]
                    // dataOffset covers all slices; advance by bytesPerImage per face.
                    uploadSlice(
                        face: face,
                        mipLevel: mipLevel,
                        bytesOffset: info.dataOffset + face * info.byteCountPerImage,
                        bytesPerRow: info.byteCountPerRow,
                        bytesPerImage: info.byteCountPerImage
                    )
                }
            }
        } else {
            // ---------------------------------------------------------------
            // No layout provided: compute how many mip levels are present in
            // the data by accumulating expected byte counts level by level.
            // ---------------------------------------------------------------
            var bytesAccounted = 0
            for mipLevel in 0..<mtlTexture.mipmapLevelCount {
                let mipWidth = mipDimension(imageAsset.width, level: mipLevel)
                let mipHeight = mipDimension(imageAsset.height, level: mipLevel)
                let mipBytes = mipWidth * bytesPerPixel * mipHeight * sliceCount
                guard bytesAccounted + mipBytes <= textureBytes.count else { break }
                bytesAccounted += mipBytes
                mipLevelsInData += 1
            }

            guard mipLevelsInData > 0 else {
                logError(
                    "imageAssetData too small: have \(textureBytes.count) bytes, "
                        + "need at least \(mipDimension(imageAsset.width, level: 0) * bytesPerPixel * mipDimension(imageAsset.height, level: 0) * sliceCount) "
                        + "for mip level 0"
                )
                return
            }

            if bytesAccounted != textureBytes.count {
                logError(
                    "imageAssetData has \(textureBytes.count - bytesAccounted) unexpected trailing bytes "
                        + "after \(mipLevelsInData) mip level(s) — ignoring"
                )
            }

            logInfo("uploading \(mipLevelsInData) of \(mtlTexture.mipmapLevelCount) mip level(s) from imageAssetData")

            // Data is face-major: Face 0 [Mip 0, Mip 1, …], Face 1 [Mip 0, Mip 1, …], …
            var offset = 0
            for face in 0..<sliceCount {
                for mipLevel in 0..<mipLevelsInData {
                    let mipWidth = mipDimension(imageAsset.width, level: mipLevel)
                    let mipHeight = mipDimension(imageAsset.height, level: mipLevel)
                    let bytesPerRow = mipWidth * bytesPerPixel
                    let bytesPerImage = bytesPerRow * mipHeight
                    uploadSlice(
                        face: face,
                        mipLevel: mipLevel,
                        bytesOffset: offset,
                        bytesPerRow: bytesPerRow,
                        bytesPerImage: bytesPerImage
                    )
                    offset += bytesPerImage
                }
            }
        }
    }

    guard mipLevelsInData > 0 else {
        return nil
    }
    return (mtlTexture, mipLevelsInData)
}

private func makeTextureFromImageAsset(
    _ imageAsset: WKBridgeImageAsset,
    device: any MTLDevice,
    renderContext: any LowLevelRenderContext,
    commandQueue: any MTLCommandQueue,
    generateMips: Bool,
    memoryOwner: task_id_token_t,
    swizzle: MTLTextureSwizzleChannels,
    existingTexture: LowLevelTextureResource?,
    layout: [WKBridgeTextureLevelInfo]
) -> LowLevelTextureResource? {
    guard
        let (mtlTexture, mipLevelsInData) = makeMTLTextureFromImageAsset(
            imageAsset,
            device: device,
            generateMips: generateMips,
            memoryOwner: memoryOwner,
            layout: layout
        )
    else {
        logError("could not create metal texture")
        return nil
    }

    let descriptor = LowLevelTextureResource.Descriptor.from(mtlTexture, swizzle: swizzle)
    if let textureResource = existingTexture ?? (try? renderContext.makeTextureResource(descriptor: descriptor)) {
        guard let commandBuffer = commandQueue.makeCommandBuffer() else {
            fatalError("Could not create command buffer")
        }
        guard let blitEncoder = commandBuffer.makeBlitCommandEncoder() else {
            fatalError("Could not create blit encoder")
        }
        if generateMips && mtlTexture.mipmapLevelCount > mipLevelsInData {
            blitEncoder.generateMipmaps(for: mtlTexture)
        }

        let outTexture = textureResource.replace(commandBuffer: commandBuffer, memoryOwner: memoryOwner)
        blitEncoder.copy(from: mtlTexture, to: outTexture)

        blitEncoder.endEncoding()
        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()
        return textureResource
    }

    return nil
}

private func makeTextureFromImageAsset(
    _ imageAsset: WKBridgeImageAsset,
    device: any MTLDevice,
    renderContext: any LowLevelRenderContext,
    commandQueue: any MTLCommandQueue,
    generateMips: Bool,
    memoryOwner: task_id_token_t,
    swizzle: MTLTextureSwizzleChannels = .init(red: .red, green: .green, blue: .blue, alpha: .alpha)
) -> LowLevelTextureResource? {
    makeTextureFromImageAsset(
        imageAsset,
        device: device,
        renderContext: renderContext,
        commandQueue: commandQueue,
        generateMips: generateMips,
        memoryOwner: memoryOwner,
        swizzle: swizzle,
        existingTexture: nil,
        layout: []
    )
}

private func makeTextureFromImageAsset(
    _ imageAsset: WKBridgeImageAsset,
    device: any MTLDevice,
    renderContext: any LowLevelRenderContext,
    commandQueue: any MTLCommandQueue,
    generateMips: Bool,
    memoryOwner: task_id_token_t,
    existingTexture: LowLevelTextureResource?,
    layout: [WKBridgeTextureLevelInfo]
) -> LowLevelTextureResource? {
    makeTextureFromImageAsset(
        imageAsset,
        device: device,
        renderContext: renderContext,
        commandQueue: commandQueue,
        generateMips: generateMips,
        memoryOwner: memoryOwner,
        swizzle: .init(red: .red, green: .green, blue: .blue, alpha: .alpha),
        existingTexture: existingTexture,
        layout: layout
    )
}

private func makeParameters(
    for function: (any LowLevelMaterialResource.Function)?,
    renderContext: any LowLevelRenderContext,
    textureHashesAndResources: [WKBridgeTypedResourceId: (String, LowLevelTextureResource)],
    fallbackTexture: LowLevelTextureResource
) throws -> LowLevelArgumentTable? {
    guard let function else { return nil }
    guard let argumentTableDescriptor = function.argumentTableDescriptor else { return nil }
    let parameterMapping = function.parameterMapping

    var optTextures: [LowLevelTextureResource?] = argumentTableDescriptor.textures.map({ _ in nil })
    for parameter in parameterMapping?.textures ?? [] {
        if let textureHashAndResource = textureHashesAndResources.values.first(where: { $0.0 == parameter.name }) {
            optTextures[parameter.textureIndex] = textureHashAndResource.1
        } else {
            // use fallback texture if no texture if found
            logInfo("Cannot find texture \(parameter.name), use fallback texture instead")
            optTextures[parameter.textureIndex] = fallbackTexture
        }
    }
    let textures = optTextures.map({ $0! })

    let buffers: [LowLevelBufferSlice] = try argumentTableDescriptor.buffers.map { bufferRequirements in
        let capacity = (bufferRequirements.size + 16 - 1) / 16 * 16
        let buffer = try renderContext.makeBufferResource(descriptor: .init(capacity: capacity))
        buffer.replace { span in
            for byteOffset in span.byteOffsets {
                span.storeBytes(of: 0, toByteOffset: byteOffset, as: UInt8.self)
            }
        }
        return try LowLevelBufferSlice(buffer: buffer, offset: 0, size: bufferRequirements.size)
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

// Mirrors RemoteMeshProxy.cpp::computeMinAndMaxCorners: for each root joint, computes
// the skin matrix that places the mesh in world space for AABB purposes, returning
// one matrix per root joint. Using only root joints avoids wildly over-inflating the
// bounds — child joints move sub-regions that stay within the local AABB extent.
private func rootSkinMatrices(_ skinningData: WKBridgeSkinningData) -> [simd_float4x4] {
    let jointTransforms = skinningData.jointTransforms
    guard !jointTransforms.isEmpty else { return [] }
    let inverseBindPoses = skinningData.inverseBindPoses
    let geomBind = skinningData.geometryBindTransform
    // Fall back to index 0 when rootJointIndices is empty (single-root or unknown).
    let indices = skinningData.rootJointIndices.isEmpty ? [UInt32(0)] : skinningData.rootJointIndices
    return indices.compactMap { idx -> simd_float4x4? in
        let i = Int(idx)
        guard i < jointTransforms.count else { return nil }
        let invBind = i < inverseBindPoses.count ? inverseBindPoses[i] : matrix_identity_float4x4
        return simd_mul(simd_mul(jointTransforms[i], invBind), geomBind)
    }
}

// Computes root joint indices for the mesh at meshPath by reading the skeleton's
// joint token paths from the USDStage. A root joint is one whose parent token path
// is not present in the skeleton's joint list.
private func rootJointIndices(forMeshAt meshPath: String, in stage: USDStage) -> [UInt32] {
    let meshPrim = stage.prim(at: USDLayer.Path(meshPath))
    guard meshPrim.isValid else { return [] }

    // Walk up the prim hierarchy to find a skel:skeleton relationship.
    var skelPrimPath: USDLayer.Path? = nil
    var current: USDPrim? = meshPrim
    while let prim = current {
        if let rel = prim.relationship(named: "skel:skeleton"),
            let target = rel.targets.first
        {
            skelPrimPath = target
            break
        }
        current = prim.parent
    }

    guard let skelPath = skelPrimPath else { return [] }
    let skelPrim = stage.prim(at: skelPath)
    let jointsAttr = skelPrim.attribute(named: "joints")
    guard skelPrim.isValid,
        jointsAttr.isValid,
        let jointTokens = skelPrim["joints", as: USDArray<USDToken>.self]
    else { return [] }

    return rootJointIndices(from: jointTokens.map(\.string))
}

// Pure string computation: returns the indices of joints whose parent path does
// not appear in the joints array. USD requires parents before children, so root
// joints always appear before their descendants but there can be multiple roots
// (e.g. ["A", "A/B", "C", "C/D/E"] has roots at indices 0 and 2).
private func rootJointIndices(from joints: [String]) -> [UInt32] {
    let jointSet = Set(joints)
    return joints.enumerated()
        .compactMap { index, joint -> UInt32? in
            var path = joint
            while let slash = path.lastIndex(of: "/") {
                path = String(path[..<slash])
                if jointSet.contains(path) { return nil }
            }
            return UInt32(index)
        }
}

// Transforms a local-space AABB by one or more skin matrices and returns the
// union of the resulting world-space AABBs. Passing multiple matrices handles
// multi-root skeletons correctly.
private func computeSkinningAABB(
    _ skinMatrices: [simd_float4x4],
    _ localMin: SIMD3<Float>,
    _ localMax: SIMD3<Float>
) -> (SIMD3<Float>, SIMD3<Float>) {
    let matrices = skinMatrices.isEmpty ? [matrix_identity_float4x4] : skinMatrices
    var effectiveMin = SIMD3<Float>(repeating: .greatestFiniteMagnitude)
    var effectiveMax = SIMD3<Float>(repeating: -.greatestFiniteMagnitude)
    for skinMatrix in matrices {
        for i in 0..<8 {
            let corner = SIMD4<Float>(
                (i & 1) != 0 ? localMax.x : localMin.x,
                (i & 2) != 0 ? localMax.y : localMin.y,
                (i & 4) != 0 ? localMax.z : localMin.z,
                1
            )
            let transformed = simd_mul(skinMatrix, corner)
            let p = SIMD3<Float>(transformed.x, transformed.y, transformed.z)
            effectiveMin = simd_min(effectiveMin, p)
            effectiveMax = simd_max(effectiveMax, p)
        }
    }
    return (effectiveMin, effectiveMax)
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
    fileprivate final var renderer: LowLevelRenderer {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            appRenderer.renderer!
        }
    }
    @nonobjc
    fileprivate final var renderContext: any LowLevelRenderContext {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            appRenderer.renderContext!
        }
    }

    @nonobjc
    fileprivate final var renderTarget: LowLevelRenderTarget.Descriptor {
        get { appRenderer.renderTargetDescriptor }
    }

    init(device: any MTLDevice, memoryOwner: task_id_token_t) {
        self.device = device
        do {
            self.appRenderer = try Renderer(device: device, memoryOwner: memoryOwner)
        } catch {
            fatalError("Exception creating renderer \(error)")
        }
    }

    var standardDynamicRange: Bool = false

    func makeStandaloneResources() async {
        do {
            appRenderer.pendingStandaloneResources = try await LowLevelRenderContextStandalone.Resources(device: self.device)
        } catch {
            fatalError("Exception creating standalone resources \(error)")
        }
    }

    func createMaterialCompiler() {
        do {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            try appRenderer.createMaterialCompiler(resources: appRenderer.pendingStandaloneResources!)
            appRenderer.pendingStandaloneResources = nil
        } catch {
            fatalError("Exception creating material compiler \(error)")
        }
    }

    func makeRendererResources() async {
        do {
            let colorPixelFormat: MTLPixelFormat = standardDynamicRange ? .bgra8Unorm : .rgba16Float
            let sampleCount: Int = 4
            appRenderer.pendingRendererResources = try await LowLevelRenderer.Resources(
                configuration: .init(
                    output: .init(colorPixelFormat: colorPixelFormat),
                    rasterSampleCount: sampleCount,
                    enableTonemap: standardDynamicRange,
                    enableColorMatch: false,
                    alphaPremultiply: false
                ),
                renderContext: self.renderContext
            )
            appRenderer.rasterSampleCount = sampleCount
        } catch {
            fatalError("Exception creating renderer resources \(error)")
        }
    }

    func createRenderer() {
        do {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            try appRenderer.createRenderer(resources: appRenderer.pendingRendererResources!)
            appRenderer.pendingRendererResources = nil
            appRenderer.tonemapEnabled = standardDynamicRange
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
    fileprivate let skyboxGenerator: SkyboxGenerator
    @nonobjc
    fileprivate let imageBasedLightTextureGenerator: ImageBasedLightTextureGenerator
    @nonobjc
    fileprivate let commandQueue: any MTLCommandQueue

    @nonobjc
    fileprivate let renderContext: any LowLevelRenderContext
    @nonobjc
    fileprivate let renderer: LowLevelRenderer
    @nonobjc
    fileprivate let appRenderer: Renderer
    @nonobjc
    fileprivate let lightingFunction: LowLevelMaterialResource.LightingFunction
    @nonobjc
    fileprivate let lightingArguments: LowLevelArgumentTable
    @nonobjc
    fileprivate var lightingArgumentBuffer: LowLevelArgumentTable?

    @nonobjc
    fileprivate final var renderTarget: LowLevelRenderTarget.Descriptor {
        get { appRenderer.renderTargetDescriptor }
    }
    @nonobjc
    fileprivate var meshInstancePool: MeshInstancePool

    @nonobjc
    fileprivate var meshResources: [WKBridgeTypedResourceId: LowLevelMeshResource] = [:]
    @nonobjc
    fileprivate var meshResourceToMaterials: [WKBridgeTypedResourceId: [WKBridgeTypedResourceId]] = [:]
    @nonobjc
    fileprivate var meshToMeshInstances: [WKBridgeTypedResourceId: [LowLevelMeshInstance]] = [:]
    @nonobjc
    fileprivate var meshTransforms: [WKBridgeTypedResourceId: [simd_float4x4]] = [:]
    @nonobjc
    fileprivate var modelTransform: simd_float4x4 = matrix_identity_float4x4
    @nonobjc
    fileprivate var rotationAngle: Float = 0

    @nonobjc
    fileprivate let deformationContext: LowLevelDeformationContext

    @nonobjc
    fileprivate var meshResourceToDeformationContext: [WKBridgeTypedResourceId: DeformationContext] = [:]

    @nonobjc
    fileprivate var deformationResidencySet: (any MTLResidencySet)?

    @nonobjc
    fileprivate var deformationResidencySetNeedsCommit: Bool = false

    struct Material {
        let resource: LowLevelMaterialResource
        let geometryArguments: LowLevelArgumentTable?
        let surfaceArguments: LowLevelArgumentTable?
        let blending: LowLevelMaterialResource.ShaderGraphOutput.Blending
    }
    @nonobjc
    fileprivate var materialsAndParams: [WKBridgeTypedResourceId: Material] = [:]

    @nonobjc
    fileprivate var textureHashesAndResources: [WKBridgeTypedResourceId: (String, LowLevelTextureResource)] = [:]

    @nonobjc
    fileprivate var dontCaptureAgain: Bool = false

    @nonobjc
    fileprivate final var memoryOwner: task_id_token_t {
        appRenderer.memoryOwner
    }

    @nonobjc
    fileprivate let fallbackTexture: LowLevelTextureResource

    struct DeferredMeshUpdate {
        enum UpdateType {
            // First time mesh update, should add mesh instances to the scene
            case newMesh
        }

        let identifier: WKBridgeTypedResourceId
        let type: UpdateType
        var updatedInstances: [LowLevelMeshInstance]

        init(identifier: WKBridgeTypedResourceId, type: UpdateType, updatedInstances: [LowLevelMeshInstance]) {
            self.identifier = identifier
            self.type = type
            self.updatedInstances = updatedInstances
        }
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
        self.skyboxGenerator = SkyboxGenerator(device: configuration.device)
        self.imageBasedLightTextureGenerator = ImageBasedLightTextureGenerator(device: configuration.device)
        self.commandQueue = configuration.commandQueue
        self.deformationContext = try LowLevelDeformationContext(configuration.device)
        let residencyDescriptor = MTLResidencySetDescriptor()
        residencyDescriptor.label = "DeformationResidencySet"
        residencyDescriptor.initialCapacity = 32
        self.deformationResidencySet = try? configuration.device.makeResidencySet(descriptor: residencyDescriptor)
        self.meshInstancePool = try MeshInstancePool(
            renderContext: configuration.renderContext,
            renderTargets: [configuration.renderTarget],
            initialCapacity: 16
        )
        let lightingFunction = configuration.renderContext.lighting.makeImageBasedLightingFunction()
        guard
            let diffuseTexture = makeTextureFromImageAsset(
                diffuseAsset,
                device: device,
                renderContext: renderContext,
                commandQueue: configuration.commandQueue,
                generateMips: false,
                memoryOwner: configuration.appRenderer.memoryOwner,
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
                swizzle: .init(red: .red, green: .red, blue: .red, alpha: .one)
            )
        else {
            fatalError("Could not create specularTexture")
        }
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
            device: configuration.device,
            memoryOwner: configuration.appRenderer.memoryOwner
        )
    }

    func commandBuffer() -> (any MTLCommandBuffer)? {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        commandQueue.makeCommandBuffer()!
    }

    @objc(renderWithTexture:commandBuffer:)
    func render(with texture: any MTLTexture, commandBuffer: any MTLCommandBuffer) {
        // Apply the latest model transform to every mesh instance, composed with
        // each instance's original USD-space transform.
        for (identifier, meshes) in meshToMeshInstances {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let originalTransforms = meshTransforms[identifier]!
            for (index, meshInstance) in meshes.enumerated() {
                meshInstance.transform = modelTransform * originalTransforms[index]
            }
        }

        // animate
        if !meshResourceToDeformationContext.isEmpty {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let deformationCommandBuffer = self.commandQueue.makeCommandBuffer()!

            if deformationResidencySetNeedsCommit, let residencySet = deformationResidencySet {
                residencySet.removeAllAllocations()
                for context in meshResourceToDeformationContext.values {
                    for buffer in context.mtlBuffers {
                        residencySet.addAllocation(buffer)
                    }
                }
                residencySet.commit()
                deformationResidencySetNeedsCommit = false
            }
            if let residencySet = deformationResidencySet {
                deformationCommandBuffer.useResidencySet(residencySet)
            }

            for (identifier, ctx) in meshResourceToDeformationContext where ctx.dirty {
                guard let computeEncoder = deformationCommandBuffer.makeComputeCommandEncoder() else {
                    fatalError("Failed to create compute command encoder for deformation")
                }
                do {
                    try ctx.deformation.encode(into: computeEncoder)
                } catch {
                    computeEncoder.endEncoding()
                    fatalError("Failed to execute deformation work \(error)")
                }
                computeEncoder.endEncoding()
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                meshResourceToDeformationContext[identifier]!.dirty = false
            }

            deformationCommandBuffer.enqueue()
            deformationCommandBuffer.commit()
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
            try appRenderer.render(meshInstances: meshInstancePool.meshInstances, texture: texture, commandBuffer: commandBuffer)
        } catch {
            logError("failed to render \(error)")
        }

        let captureManager = MTLCaptureManager.shared()
        if captureManager.isCapturing {
            captureManager.stopCapture()
        }
    }

    @objc(updateTexture:)
    func updateTexture(_ datas: [WKBridgeUpdateTexture]) {
        for textureData in datas {
            let asset = textureData.imageAsset
            let existingTexture = textureHashesAndResources[textureData.identifier]?.1
            let needsNewTexture: Bool
            if let existingTexture {
                needsNewTexture = existingTexture.descriptor != LowLevelTextureResource.Descriptor(from: asset)
            } else {
                needsNewTexture = true
            }

            let commandQueue = appRenderer.commandQueue
            if let textureResource = makeTextureFromImageAsset(
                asset,
                device: device,
                renderContext: renderContext,
                commandQueue: commandQueue,
                generateMips: true,
                memoryOwner: self.memoryOwner,
                existingTexture: needsNewTexture ? nil : existingTexture,
                layout: textureData.layout
            ) {
                textureHashesAndResources[textureData.identifier] = (textureData.hashString, textureResource)
            }
        }
    }

    @objc(updateMaterial:)
    func updateMaterial(_ updates: [WKBridgeUpdateMaterial]) {
        do {
            for data in updates {
                logInfo("updateMaterial (pre-dispatch) \(data.identifier)")

                let identifier = data.identifier
                logInfo("updateMaterial \(identifier)")

                guard let shaderGraph = ShaderGraph.fromWKDescriptor(data.materialGraph) else {
                    fatalError("No materialGraph data provided for material \(identifier)")
                }

                let shaderGraphOutput = try renderContext.shaderGraph.makeShaderGraphFunctions(
                    shaderGraph: shaderGraph,
                    constantValues: .init()
                )

                let geometryArguments = try makeParameters(
                    for: shaderGraphOutput.geometryModifier,
                    renderContext: renderContext,
                    textureHashesAndResources: textureHashesAndResources,
                    fallbackTexture: self.fallbackTexture
                )
                let surfaceArguments = try makeParameters(
                    for: shaderGraphOutput.surfaceShader,
                    renderContext: renderContext,
                    textureHashesAndResources: textureHashesAndResources,
                    fallbackTexture: self.fallbackTexture
                )

                let geometryModifier = shaderGraphOutput.geometryModifier ?? renderContext.makeDefaultGeometryModifier()
                let surfaceShader = shaderGraphOutput.surfaceShader
                let materialResource = try renderContext.makeMaterialResource(
                    descriptor: .init(
                        geometry: geometryModifier,
                        surface: surfaceShader,
                        lighting: lightingFunction
                    )
                )

                materialsAndParams[identifier] = .init(
                    resource: materialResource,
                    geometryArguments: geometryArguments,
                    surfaceArguments: surfaceArguments,
                    blending: shaderGraphOutput.blending
                )
            }
        } catch {
            logError("updateMaterial failed \(error)")
        }
    }

    @objc
    func processRemovals(
        _ meshRemovals: [WKBridgeTypedResourceId],
        materialRemovals: [WKBridgeTypedResourceId],
        textureRemovals: [WKBridgeTypedResourceId]
    ) -> Bool {
        do {
            for meshId in meshRemovals {
                logInfo("mesh destroyed: \(meshId)")
                if let meshInstancesToRemove = meshToMeshInstances.removeValue(forKey: meshId) {
                    for meshInstanceToRemove in meshInstancesToRemove {
                        try meshInstancePool.remove(meshInstanceToRemove)
                    }
                }
                meshResources.removeValue(forKey: meshId)
                meshResourceToMaterials.removeValue(forKey: meshId)
                meshResourceToDeformationContext.removeValue(forKey: meshId)
            }

            for materialId in materialRemovals {
                logInfo("material destroyed: \(materialId)")
                materialsAndParams.removeValue(forKey: materialId)
            }

            for textureId in textureRemovals {
                logInfo("texture destroyed: \(textureId)")
                textureHashesAndResources.removeValue(forKey: textureId)
            }
        } catch {
            logError("\(error)")
            return false
        }
        return true
    }

    @objc(updateMesh:)
    func updateMesh(_ updates: [WKBridgeUpdateMesh]) {
        do {
            var deferredMeshUpdates: [DeferredMeshUpdate] = []

            for meshData in updates {
                let identifier = meshData.identifier

                let meshResource: LowLevelMeshResource
                if meshData.updateType == .initial || meshData.descriptor != nil {
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                    // swift-format-ignore: NeverForceUnwrap
                    let meshDescriptor = meshData.descriptor!
                    let descriptor = LowLevelMeshResource.Descriptor.fromLlmDescriptor(meshDescriptor)
                    if let cachedMeshResource = meshResources[identifier] {
                        meshResource = cachedMeshResource
                    } else {
                        meshResource = try renderContext.makeMeshResource(descriptor: descriptor)
                    }
                    meshResource.replaceData(indexData: meshData.indexData, vertexData: meshData.vertexData)
                    meshResources[identifier] = meshResource
                    meshResourceToDeformationContext.removeValue(forKey: identifier)
                } else {
                    guard let cachedMeshResource = meshResources[identifier] else {
                        fatalError("Mesh resource should already be created from previous update")
                    }

                    if meshData.indexData != nil || !meshData.vertexData.isEmpty {
                        cachedMeshResource.replaceData(indexData: meshData.indexData, vertexData: meshData.vertexData)
                    }
                    meshResource = cachedMeshResource
                }

                if let deformationData = meshData.deformationData {
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                    // swift-format-ignore: NeverForceUnwrap
                    let commandBuffer = commandQueue.makeCommandBuffer()!
                    // TODO: delta update
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                    // swift-format-ignore: NeverForceUnwrap
                    configureDeformation(
                        identifier: identifier,
                        deformationData: deformationData,
                        commandBuffer: commandBuffer,
                        device: device,
                        meshResource: meshResources[identifier]!,
                        meshResourceToDeformationContext: &meshResourceToDeformationContext,
                        deformationContext: deformationContext,
                        memoryOwner: self.memoryOwner
                    )
                    deformationResidencySetNeedsCommit = true
                    commandBuffer.enqueue()
                    commandBuffer.commit()
                }

                if meshData.instanceTransformsCount > 0 {
                    if meshToMeshInstances[identifier] == nil {
                        meshToMeshInstances[identifier] = []
                        meshTransforms[identifier] = []

                        var deferredMeshUpdate = DeferredMeshUpdate(identifier: identifier, type: .newMesh, updatedInstances: [])

                        let skinMatrices = meshData.deformationData?.skinningData.map { rootSkinMatrices($0) } ?? []
                        for part in meshData.parts {
                            if part.materialIndex >= meshData.assignedMaterials.count {
                                fatalError(
                                    "index out of range: material index \(part.materialIndex) while only \(meshData.assignedMaterials.count) were found"
                                )
                            }
                            let materialIdentifier = meshData.assignedMaterials[part.materialIndex]
                            guard let material = materialsAndParams[materialIdentifier] else {
                                fatalError("Failed to get material instance \(materialIdentifier)")
                            }

                            let pipeline = try renderContext.makeRenderPipelineState(
                                descriptor: .init(
                                    mesh: meshResource.descriptor,
                                    material: material.resource,
                                    renderTargets: [renderTarget],
                                    blending: material.blending == .transparent ? .sourceOver : nil
                                )
                            )

                            let (effectiveMin, effectiveMax) = computeSkinningAABB(
                                skinMatrices,
                                part.boundsMin,
                                part.boundsMax
                            )

                            let meshPart = try renderContext.makeMeshPart(
                                resource: meshResource,
                                indexOffset: part.indexOffset,
                                indexCount: part.indexCount,
                                primitive: part.topology,
                                windingOrder: .counterClockwise,
                                bounds: .init(
                                    boxMin: effectiveMin,
                                    boxMax: effectiveMax
                                )
                            )

                            for instanceTransform in meshData.instanceTransforms {
                                let meshInstance = try renderContext.makeMeshInstance(
                                    meshPart: meshPart,
                                    pipeline: pipeline,
                                    geometryArguments: material.geometryArguments,
                                    surfaceArguments: material.surfaceArguments,
                                    lightingArguments: lightingArguments,
                                    transform: instanceTransform,
                                    sortCategory: material.blending == .transparent ? .transparent : .opaque
                                )

                                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                                // swift-format-ignore: NeverForceUnwrap
                                meshToMeshInstances[identifier]!.append(meshInstance)
                                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                                // swift-format-ignore: NeverForceUnwrap
                                meshTransforms[identifier]!.append(instanceTransform)
                                deferredMeshUpdate.updatedInstances.append(meshInstance)
                            }
                        }

                        deferredMeshUpdates.append(deferredMeshUpdate)
                    } else {
                        // Update transforms otherwise
                        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                        // swift-format-ignore: NeverForceUnwrap
                        let partCount = meshToMeshInstances[identifier]!.count / meshData.instanceTransforms.count
                        for (instanceIndex, instanceTransform) in meshData.instanceTransforms.enumerated() {
                            for partIndex in 0..<partCount {
                                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                                // swift-format-ignore: NeverForceUnwrap
                                meshTransforms[identifier]![instanceIndex * partCount + partIndex] = instanceTransform
                            }
                        }
                    }
                }

                if !meshData.assignedMaterials.isEmpty {
                    meshResourceToMaterials[identifier] = meshData.assignedMaterials
                }
            }

            // Process all deferred mesh updates at once to avoid mesh popping
            for deferredUpdate in deferredMeshUpdates {
                switch deferredUpdate.type {
                case .newMesh:
                    for newMeshInstance in deferredUpdate.updatedInstances {
                        try meshInstancePool.add(newMeshInstance)
                    }
                }
            }
        } catch {
            logError("updateMesh \(error)")
        }
    }

    @objc(setTransform:)
    func setTransform(_ transform: simd_float4x4) {
        modelTransform = transform
    }

    func setFOV(_ fovY: Float) {
        appRenderer.setFOV(fovY)
    }

    func setPlaying(_ play: Bool) {
    }

    func setEnvironmentMap(_ textureData: WKBridgeUpdateTexture) {
        do {
            let imageAsset = textureData.imageAsset
            guard
                let (mtlTextureEquirectangular, _) = makeMTLTextureFromImageAsset(
                    imageAsset,
                    device: device,
                    generateMips: true,
                    memoryOwner: self.memoryOwner,
                    layout: textureData.layout
                )
            else {
                fatalError("Could not make metal texture from environment asset data")
            }

            let cubeMTLTextureDescriptor = try self.skyboxGenerator.makeDescriptor(
                fromEquirectangular: mtlTextureEquirectangular
            )
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let cubeMTLTexture = self.device.makeTexture(descriptor: cubeMTLTextureDescriptor)!
            cubeMTLTexture.__setOwnerWithIdentity(self.memoryOwner)

            let diffuseMTLTextureDescriptor = try self.imageBasedLightTextureGenerator.makeDiffuseDescriptor(
                fromCube: cubeMTLTexture
            )
            let diffuseTextureDescriptor = LowLevelTextureResource.Descriptor.from(diffuseMTLTextureDescriptor)
            let diffuseTexture = try self.renderContext.makeTextureResource(descriptor: diffuseTextureDescriptor)

            let specularMTLTextureDescriptor = try self.imageBasedLightTextureGenerator.makeSpecularDescriptor(
                fromCube: cubeMTLTexture
            )
            let specularTextureDescriptor = LowLevelTextureResource.Descriptor.from(specularMTLTextureDescriptor)
            let specularTexture = try self.renderContext.makeTextureResource(descriptor: specularTextureDescriptor)

            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let commandBuffer = self.commandQueue.makeCommandBuffer()!

            try self.skyboxGenerator.generateSkybox(
                using: commandBuffer,
                fromEquirectangular: mtlTextureEquirectangular,
                into: cubeMTLTexture
            )

            let diffuseMTLTexture = diffuseTexture.replace(commandBuffer: commandBuffer, memoryOwner: self.memoryOwner)
            let specularMTLTexture = specularTexture.replace(commandBuffer: commandBuffer, memoryOwner: self.memoryOwner)

            try self.imageBasedLightTextureGenerator.generateDiffuse(
                using: commandBuffer,
                fromSkyboxCube: cubeMTLTexture,
                into: diffuseMTLTexture
            )
            try self.imageBasedLightTextureGenerator.generateSpecular(
                using: commandBuffer,
                fromSkyboxCube: cubeMTLTexture,
                into: specularMTLTexture
            )

            try self.lightingArguments.setTexture(diffuseTexture, at: 0)
            try self.lightingArguments.setTexture(specularTexture, at: 1)

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

private func webUpdateTextureRequestFromTextureData(_ textureData: TextureData, hashString: String) -> WKBridgeUpdateTexture {
    WKBridgeUpdateTexture(
        imageAsset: .init(textureData.descriptor, data: textureData.data),
        identifier: makeTypedResourceId(
            uuid: makeUUID(from: textureData.id.description),
            path: textureData.assetPath,
            hashValue: textureData.id.hashValue
        ),
        hashString: hashString,
        layout: textureData.layout.map {
            WKBridgeTextureLevelInfo(
                dataOffset: $0.dataOffset,
                byteCountPerRow: $0.byteCountPerRow,
                byteCountPerImage: $0.byteCountPerImage
            )
        }
    )
}

private func deformationID(from meshType: MeshData.MeshType) -> DeformationID? {
    if case .deformable(let id) = meshType { return id }
    return nil
}

// Accumulates per-frame overrides from DeformationData.Update so the next
// WKBridgeDeformationData we ship to the receiver carries the latest joint
// transforms / blend shape weights / renormalization buffers. The override
// has the same shape as DeformationData.SkinningData / BlendShapeData /
// RenormalizationData; absent fields fall back to the original DeformationData
// captured at deformationAdditions time.
struct DeformationOverrides {
    var jointTransforms: [simd_float4x4]?
    var inverseBindPoses: [simd_float4x4]?
    var influenceJointIndices: [UInt32]?
    var influenceWeights: [Float]?
    var geometryBindTransform: simd_float4x4?
    var blendShapeWeights: [Float]?
    var blendShapePositionOffsets: [[SIMD3<Float>]]?
    var vertexIndicesPerTriangle: [UInt32]?
    var vertexAdjacencies: [UInt32]?
    var vertexAdjacencyEndIndices: [UInt32]?

    mutating func apply(_ update: DeformationData.Update) {
        if let s = update.skinning {
            if let v = s.jointTransforms { jointTransforms = v }
            if let v = s.inverseBindPoses { inverseBindPoses = v }
            if let v = s.influenceJointIndices { influenceJointIndices = v }
            if let v = s.influenceWeights { influenceWeights = v }
            if let v = s.geometryBindTransform { geometryBindTransform = v }
        }
        if let b = update.blendShapes {
            if let v = b.weights { blendShapeWeights = v }
            if let v = b.positionOffsets { blendShapePositionOffsets = v }
        }
        if let r = update.renormalization {
            if let v = r.vertexIndicesPerTriangle { vertexIndicesPerTriangle = v }
            if let v = r.vertexAdjacencies { vertexAdjacencies = v }
            if let v = r.vertexAdjacencyEndIndices { vertexAdjacencyEndIndices = v }
        }
    }
}

private func bridgeDeformation(
    base: DeformationData?,
    overrides: DeformationOverrides?,
    rootJointIndices: [UInt32]
) -> WKBridgeDeformationData? {
    guard let base else { return nil }
    let skinning: WKBridgeSkinningData? = base.skinning.map {
        WKBridgeSkinningData(
            influencePerVertexCount: $0.influencePerVertexCount,
            jointTransforms: toData(overrides?.jointTransforms ?? $0.jointTransforms),
            inverseBindPoses: toData(overrides?.inverseBindPoses ?? $0.inverseBindPoses),
            influenceJointIndices: toData(overrides?.influenceJointIndices ?? $0.influenceJointIndices),
            influenceWeights: toData(overrides?.influenceWeights ?? $0.influenceWeights),
            geometryBindTransform: overrides?.geometryBindTransform ?? $0.geometryBindTransform,
            rootJointIndices: rootJointIndices.isEmpty ? nil : toData(rootJointIndices)
        )
    }
    let blendShape: WKBridgeBlendShapeData? = base.blendShapes.map {
        WKBridgeBlendShapeData(
            weights: toData(overrides?.blendShapeWeights ?? $0.weights),
            positionOffsets: toDataArray(overrides?.blendShapePositionOffsets ?? $0.positionOffsets),
            normalOffsets: []
        )
    }
    let renormalization: WKBridgeRenormalizationData? = base.renormalization.map {
        WKBridgeRenormalizationData(
            vertexIndicesPerTriangle: toData(overrides?.vertexIndicesPerTriangle ?? $0.vertexIndicesPerTriangle),
            vertexAdjacencies: toData(overrides?.vertexAdjacencies ?? $0.vertexAdjacencies),
            vertexAdjacencyEndIndices: toData(overrides?.vertexAdjacencyEndIndices ?? $0.vertexAdjacencyEndIndices)
        )
    }
    return WKBridgeDeformationData(
        skinningData: skinning,
        blendShapeData: blendShape,
        renormalizationData: renormalization
    )
}

private func webMeshFromMeshData(
    _ meshData: MeshData,
    deformation: WKBridgeDeformationData?
) -> WKBridgeUpdateMesh {
    WKBridgeUpdateMesh(
        identifier: makeTypedResourceId(
            uuid: makeUUID(from: meshData.id.description),
            path: meshData.primPath,
            hashValue: meshData.id.hashValue
        ),
        updateType: .initial,
        descriptor: .init(request: meshData.descriptor),
        parts: webPartsFromParts(meshData.parts),
        indexData: meshData.indexData,
        vertexData: meshData.vertexData,
        instanceTransforms: toData(meshData.instanceTransforms),
        instanceTransformsCount: meshData.instanceTransforms.count,
        assignedMaterials: meshData.assignedMaterials.map { id in
            makeTypedResourceId(
                uuid: makeUUID(from: id.description),
                path: "",
                hashValue: id.hashValue
            )
        },
        deformationData: deformation
    )
}

private func webMeshFromMeshUpdate(
    _ update: MeshData.Update,
    primPath: String,
    deformation: WKBridgeDeformationData?
) -> WKBridgeUpdateMesh {
    WKBridgeUpdateMesh(
        identifier: makeTypedResourceId(
            uuid: makeUUID(from: update.id.description),
            path: primPath,
            hashValue: update.id.hashValue
        ),
        updateType: .delta,
        descriptor: nil,
        parts: webPartsFromParts(update.parts ?? []),
        indexData: update.indexData,
        vertexData: update.vertexData ?? [],
        instanceTransforms: (update.instanceTransforms).map { toData($0) },
        instanceTransformsCount: update.instanceTransforms?.count ?? 0,
        assignedMaterials: (update.assignedMaterials ?? [])
            .map { id in
                makeTypedResourceId(
                    uuid: makeUUID(from: id.description),
                    path: "",
                    hashValue: id.hashValue
                )
            },
        deformationData: deformation
    )
}

// Synthesizes a delta mesh update that only refreshes the deformation, for use
// when DeformationData.Update fires without an accompanying MeshData.Update.
// Carries no mesh data — just the new joint transforms / blend weights / etc.
private func webMeshDeformationDelta(
    meshId: MeshID,
    primPath: String,
    deformation: WKBridgeDeformationData
) -> WKBridgeUpdateMesh {
    WKBridgeUpdateMesh(
        identifier: makeTypedResourceId(
            uuid: makeUUID(from: meshId.description),
            path: primPath,
            hashValue: meshId.hashValue
        ),
        updateType: .delta,
        descriptor: nil,
        parts: [],
        indexData: nil,
        vertexData: [],
        instanceTransforms: nil,
        instanceTransformsCount: 0,
        assignedMaterials: [],
        deformationData: deformation
    )
}

private func makeTypedResourceId(uuid: UUID, path: String, hashValue: Int) -> WKBridgeTypedResourceId {
    WKBridgeTypedResourceId(value: uuid, path: path, hashValue: hashValue)
}

private func makeUUID(from description: String, in context: StaticString = #function) -> UUID {
    guard let uuid = UUID(uuidString: description) else {
        fatalError("UUID in \(context) could not be constructed")
    }
    return uuid
}

// Maps a canonical UV name string (e.g. "UV0") to a `ShaderGraph.TextureCoordinate`,
// used by `fromWKDescriptor` to restore primvarMappings on the receiver side.
private func textureCoordinate(from name: String) -> ShaderGraph.TextureCoordinate? {
    switch name {
    case "UV0": .uv0
    case "UV1": .uv1
    case "UV2": .uv2
    case "UV3": .uv3
    case "UV4": .uv4
    case "UV5": .uv5
    case "UV6": .uv6
    case "UV7": .uv7
    default: nil
    }
}

private func toWKBridgeDataType(_ dataType: ShaderGraph.DataType) -> WKBridgeDataType {
    switch dataType {
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
    case .surfaceShader: .surfaceShader
    case .geometryModifier: .geometryModifier
    case .postLightingShader: .postLightingShader
    case .cgColor3: .cgColor3
    case .cgColor4: .cgColor4
    case .texture: .asset
    @unknown default: .asset
    }
}

private func constantValues(value: ShaderGraph.Value) -> ([WKBridgeValueString], WKBridgeConstant) {
    switch value {
    case .bool(let v): return ([WKBridgeValueString(number: NSNumber(booleanLiteral: v))], .bool)
    case .uchar(let v): return ([WKBridgeValueString(number: NSNumber(value: v))], .uchar)
    case .int(let v): return ([WKBridgeValueString(number: NSNumber(value: v))], .int)
    case .uint(let v): return ([WKBridgeValueString(number: NSNumber(value: v))], .uint)
    case .half(let v): return ([WKBridgeValueString(number: NSNumber(value: v.bitPattern))], .half)
    case .float(let v): return ([WKBridgeValueString(number: NSNumber(value: v))], .float)
    case .string(let v): return ([WKBridgeValueString(string: v)], .string)
    case .float2(let v):
        return ([WKBridgeValueString(number: NSNumber(value: v.x)), WKBridgeValueString(number: NSNumber(value: v.y))], .float2)
    case .float3(let v):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: v.x)), WKBridgeValueString(number: NSNumber(value: v.y)),
                WKBridgeValueString(number: NSNumber(value: v.z)),
            ], .float3
        )
    case .float4(let v):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: v.x)), WKBridgeValueString(number: NSNumber(value: v.y)),
                WKBridgeValueString(number: NSNumber(value: v.z)), WKBridgeValueString(number: NSNumber(value: v.w)),
            ], .float4
        )
    case .half2(let v):
        return (
            [WKBridgeValueString(number: NSNumber(value: v.x.bitPattern)), WKBridgeValueString(number: NSNumber(value: v.y.bitPattern))],
            .half2
        )
    case .half3(let v):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: v.x.bitPattern)), WKBridgeValueString(number: NSNumber(value: v.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: v.z.bitPattern)),
            ], .half3
        )
    case .half4(let v):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: v.x.bitPattern)), WKBridgeValueString(number: NSNumber(value: v.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: v.z.bitPattern)), WKBridgeValueString(number: NSNumber(value: v.w.bitPattern)),
            ], .half4
        )
    case .int2(let v):
        return ([WKBridgeValueString(number: NSNumber(value: v.x)), WKBridgeValueString(number: NSNumber(value: v.y))], .int2)
    case .int3(let v):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: v.x)), WKBridgeValueString(number: NSNumber(value: v.y)),
                WKBridgeValueString(number: NSNumber(value: v.z)),
            ], .int3
        )
    case .int4(let v):
        return (
            [
                WKBridgeValueString(number: NSNumber(value: v.x)), WKBridgeValueString(number: NSNumber(value: v.y)),
                WKBridgeValueString(number: NSNumber(value: v.z)), WKBridgeValueString(number: NSNumber(value: v.w)),
            ], .int4
        )
    case .cgColor3(let color):
        guard let c = color.components, c.count >= 3 else { fatalError("constantValues(value:): cgColor3 missing components") }
        return (
            [
                WKBridgeValueString(number: NSNumber(value: Float(c[0]))), WKBridgeValueString(number: NSNumber(value: Float(c[1]))),
                WKBridgeValueString(number: NSNumber(value: Float(c[2]))),
            ], .cgColor3
        )
    case .cgColor4(let color):
        guard let c = color.components, c.count >= 4 else { fatalError("constantValues(value:): cgColor4 missing components") }
        return (
            [
                WKBridgeValueString(number: NSNumber(value: Float(c[0]))), WKBridgeValueString(number: NSNumber(value: Float(c[1]))),
                WKBridgeValueString(number: NSNumber(value: Float(c[2]))), WKBridgeValueString(number: NSNumber(value: Float(c[3]))),
            ], .cgColor4
        )
    case .float2x2(let m):
        let col0 = m.columns.0
        let col1 = m.columns.1
        return (
            [
                WKBridgeValueString(number: NSNumber(value: col0.x)), WKBridgeValueString(number: NSNumber(value: col0.y)),
                WKBridgeValueString(number: NSNumber(value: col1.x)), WKBridgeValueString(number: NSNumber(value: col1.y)),
            ], .matrix2f
        )
    case .float3x3(let m):
        let col0 = m.columns.0
        let col1 = m.columns.1
        let col2 = m.columns.2
        return (
            [
                WKBridgeValueString(number: NSNumber(value: col0.x)), WKBridgeValueString(number: NSNumber(value: col0.y)),
                WKBridgeValueString(number: NSNumber(value: col0.z)), WKBridgeValueString(number: NSNumber(value: col1.x)),
                WKBridgeValueString(number: NSNumber(value: col1.y)), WKBridgeValueString(number: NSNumber(value: col1.z)),
                WKBridgeValueString(number: NSNumber(value: col2.x)), WKBridgeValueString(number: NSNumber(value: col2.y)),
                WKBridgeValueString(number: NSNumber(value: col2.z)),
            ], .matrix3f
        )
    case .float4x4(let m):
        let col0 = m.columns.0
        let col1 = m.columns.1
        let col2 = m.columns.2
        let col3 = m.columns.3
        return (
            [
                WKBridgeValueString(number: NSNumber(value: col0.x)), WKBridgeValueString(number: NSNumber(value: col0.y)),
                WKBridgeValueString(number: NSNumber(value: col0.z)), WKBridgeValueString(number: NSNumber(value: col0.w)),
                WKBridgeValueString(number: NSNumber(value: col1.x)), WKBridgeValueString(number: NSNumber(value: col1.y)),
                WKBridgeValueString(number: NSNumber(value: col1.z)), WKBridgeValueString(number: NSNumber(value: col1.w)),
                WKBridgeValueString(number: NSNumber(value: col2.x)), WKBridgeValueString(number: NSNumber(value: col2.y)),
                WKBridgeValueString(number: NSNumber(value: col2.z)), WKBridgeValueString(number: NSNumber(value: col2.w)),
                WKBridgeValueString(number: NSNumber(value: col3.x)), WKBridgeValueString(number: NSNumber(value: col3.y)),
                WKBridgeValueString(number: NSNumber(value: col3.z)), WKBridgeValueString(number: NSNumber(value: col3.w)),
            ], .matrix4f
        )
    case .half2x2(let m):
        let col0 = m.columns.0
        let col1 = m.columns.1
        return (
            [
                WKBridgeValueString(number: NSNumber(value: col0.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col0.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.y.bitPattern)),
            ], .matrix2h
        )
    case .half3x3(let m):
        let col0 = m.columns.0
        let col1 = m.columns.1
        let col2 = m.columns.2
        return (
            [
                WKBridgeValueString(number: NSNumber(value: col0.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col0.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col0.z.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.z.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col2.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col2.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col2.z.bitPattern)),
            ], .matrix3h
        )
    case .half4x4(let m):
        let col0 = m.columns.0
        let col1 = m.columns.1
        let col2 = m.columns.2
        let col3 = m.columns.3
        return (
            [
                WKBridgeValueString(number: NSNumber(value: col0.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col0.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col0.z.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col0.w.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.z.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col1.w.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col2.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col2.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col2.z.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col2.w.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col3.x.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col3.y.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col3.z.bitPattern)),
                WKBridgeValueString(number: NSNumber(value: col3.w.bitPattern)),
            ], .matrix4h
        )
    @unknown default: fatalError("constantValues(value:): unhandled ShaderGraph.Value type \(value)")
    }
}

private func toWebNode(_ node: ShaderGraph.Node) -> WKBridgeNode {
    let nodeName = node.name.lowercased()
    let bridgeNodeType: WKBridgeNodeType
    if nodeName == "arguments" {
        bridgeNodeType = .arguments
    } else if nodeName == "results" || nodeName == "result" {
        bridgeNodeType = .results
    } else {
        switch node.data {
        case .constant: bridgeNodeType = .constant
        case .definition, .graph: bridgeNodeType = .builtin
        @unknown default: fatalError("toWebNode: unknown bridgeNodeType")
        }
    }

    let builtin: WKBridgeBuiltin =
        switch node.data {
        case .definition(let definition): WKBridgeBuiltin(definition: definition.name, name: node.name)
        case .graph: WKBridgeBuiltin(definition: "graph", name: node.name)
        case .constant: WKBridgeBuiltin(definition: "", name: node.name)
        @unknown default: fatalError("toWebNode: unknown builtin")
        }

    let constant: WKBridgeConstantContainer =
        switch node.data {
        case .constant(let value):
            {
                let (values, constantType) = constantValues(value: value)
                let colorSpace: String? =
                    switch value {
                    case .cgColor3(let color): color.colorSpace?.name as String?
                    case .cgColor4(let color): color.colorSpace?.name as String?
                    default: nil
                    }
                return WKBridgeConstantContainer(
                    constant: constantType,
                    constantValues: values,
                    name: node.name,
                    colorSpaceName: colorSpace
                )
            }()
        default: WKBridgeConstantContainer(constant: .asset, constantValues: [], name: node.name, colorSpaceName: nil)
        }

    return WKBridgeNode(bridgeNodeType: bridgeNodeType, builtin: builtin, constant: constant)
}

private func toWebMaterialGraph(_ material: ShaderGraph?) -> WKBridgeMaterialGraph {
    guard let material else {
        return WKBridgeMaterialGraph(
            graphName: "",
            nodes: [],
            edges: [],
            arguments: WKBridgeNode(
                bridgeNodeType: .arguments,
                builtin: WKBridgeBuiltin(definition: "", name: "arguments"),
                constant: WKBridgeConstantContainer(constant: .asset, constantValues: [], name: "", colorSpaceName: nil)
            ),
            results: WKBridgeNode(
                bridgeNodeType: .results,
                builtin: WKBridgeBuiltin(definition: "", name: "results"),
                constant: WKBridgeConstantContainer(constant: .asset, constantValues: [], name: "", colorSpaceName: nil)
            ),
            inputs: [],
            outputs: [],
            primvarMappingPrimvarNames: [],
            primvarMappingTexcoordNames: [],
            functionConstantInputNames: []
        )
    }

    let nodes = material.nodes.values.map { toWebNode($0) }
    let edges = material.edges.map { edge in
        WKBridgeEdge(outputNode: edge.outputNode, outputPort: edge.outputPort ?? "", inputNode: edge.inputNode, inputPort: edge.inputPort)
    }
    let argumentsNode = toWebNode(material.arguments)
    let resultsNode = toWebNode(material.results)
    let inputs = material.inputs.map { i -> WKBridgeInputOutput in
        let defaultValueContainer: WKBridgeConstantContainer? = i.defaultValue.map { value in
            let (values, constantType) = constantValues(value: value)
            let colorSpace: String? =
                switch value {
                case .cgColor3(let color): color.colorSpace?.name as String?
                case .cgColor4(let color): color.colorSpace?.name as String?
                default: nil
                }
            return WKBridgeConstantContainer(constant: constantType, constantValues: values, name: "", colorSpaceName: colorSpace)
        }
        return WKBridgeInputOutput(
            type: toWKBridgeDataType(i.type),
            name: i.name,
            semanticTypeName: i.semanticType?.name,
            defaultValue: defaultValueContainer
        )
    }
    let outputs = material.outputs.map { o -> WKBridgeInputOutput in
        let defaultValueContainer: WKBridgeConstantContainer? = o.defaultValue.map { value in
            let (values, constantType) = constantValues(value: value)
            let colorSpace: String? =
                switch value {
                case .cgColor3(let color): color.colorSpace?.name as String?
                case .cgColor4(let color): color.colorSpace?.name as String?
                default: nil
                }
            return WKBridgeConstantContainer(constant: constantType, constantValues: values, name: "", colorSpaceName: colorSpace)
        }
        return WKBridgeInputOutput(
            type: toWKBridgeDataType(o.type),
            name: o.name,
            semanticTypeName: o.semanticType?.name,
            defaultValue: defaultValueContainer
        )
    }
    let sortedPrimvarKeys = material.primvarMappings.keys.sorted()
    let primvarTexcoordNames = sortedPrimvarKeys.map { (key: String) -> String in
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        switch material.primvarMappings[key]! {
        case .uv0: "UV0"
        case .uv1: "UV1"
        case .uv2: "UV2"
        case .uv3: "UV3"
        case .uv4: "UV4"
        case .uv5: "UV5"
        case .uv6: "UV6"
        case .uv7: "UV7"
        @unknown default: "UV0"
        }
    }

    return WKBridgeMaterialGraph(
        graphName: "MaterialGraph",
        nodes: Array(nodes),
        edges: edges,
        arguments: argumentsNode,
        results: resultsNode,
        inputs: inputs,
        outputs: outputs,
        primvarMappingPrimvarNames: Array(sortedPrimvarKeys),
        primvarMappingTexcoordNames: primvarTexcoordNames,
        functionConstantInputNames: material.functionConstantInputs
    )
}

extension ShaderGraph {
    // Reconstructs the graph from the IPC bridge representation using the ShaderGraph API.
    static func fromWKDescriptor(_ descriptor: WKBridgeMaterialGraph?) -> ShaderGraph? {
        guard let descriptor else { return nil }

        do {
            let library = ShaderGraph.NodeLibrary(version: .materialX138)
            let graph = try ShaderGraph(
                named: descriptor.graphName.isEmpty ? "MaterialGraph" : descriptor.graphName,
                inputs: descriptor.inputs.map {
                    ShaderGraph.NodeDefinition.Input(
                        name: $0.name,
                        type: fromWKBridgeDataType($0.type),
                        semanticType: $0.semanticTypeName.map { .init(name: $0) }
                    )
                },
                outputs: descriptor.outputs.map {
                    ShaderGraph.NodeDefinition.Output(
                        name: $0.name,
                        type: fromWKBridgeDataType($0.type),
                        semanticType: $0.semanticTypeName.map { .init(name: $0) }
                    )
                },
                nodeLibrary: library
            )

            for bridgeNode in descriptor.nodes {
                switch bridgeNode.bridgeNodeType {
                case .constant:
                    if let constant = bridgeNode.constant {
                        try graph.addConstant(fromWKBridgeConstant(constant), named: constant.name)
                    }
                case .builtin:
                    if let builtin = bridgeNode.builtin, !builtin.definition.isEmpty,
                        let definition = library.definition(named: builtin.definition)
                    {
                        try graph.addNode(.init(name: builtin.name, data: .definition(definition)))
                    }
                case .arguments, .results:
                    break
                @unknown default:
                    let name = bridgeNode.builtin?.name ?? bridgeNode.constant?.name ?? "unknown"
                    fatalError("Unknown node type '\(name)'")
                }
            }

            for bridgeEdge in descriptor.edges {
                do {
                    try graph.connect(
                        bridgeEdge.outputNode,
                        outputPort: bridgeEdge.outputPort,
                        to: bridgeEdge.inputNode,
                        inputPort: bridgeEdge.inputPort
                    )
                } catch {
                    logError(
                        "Failed to connect \(bridgeEdge.outputNode):\(bridgeEdge.outputPort) -> \(bridgeEdge.inputNode):\(bridgeEdge.inputPort): \(error)"
                    )
                }
            }

            // Restore primvarMappings so ShaderGraph knows how to resolve UV primvar names
            // (e.g. "UV0") to actual mesh texture coordinates. Without this the ND_geompropvalue
            // node can't find UV data and all texture sampling breaks.
            for (primvarName, texcoordName) in zip(descriptor.primvarMappingPrimvarNames, descriptor.primvarMappingTexcoordNames) {
                if let texcoord = textureCoordinate(from: texcoordName) {
                    graph.primvarMappings[primvarName] = texcoord
                }
            }

            // Restore functionConstantInputs so runtime-driven inputs are declared correctly.
            graph.functionConstantInputs = descriptor.functionConstantInputNames

            return graph
        } catch {
            logError("Failed to reconstruct ShaderNodeGraph: \(error)")
            return nil
        }
    }
}

private func fromWKBridgeDataType(_ dataType: WKBridgeDataType) -> ShaderGraph.DataType {
    switch dataType {
    case .bool: .bool
    case .uchar: .uchar
    case .int: .int
    case .uint: .uint
    case .int2: .int2
    case .int3: .int3
    case .int4: .int4
    case .float: .float
    case .cgColor3: .cgColor3
    case .cgColor4: .cgColor4
    case .color3h: .cgColor3
    case .color4h: .cgColor4
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
    case .surfaceShader: .surfaceShader
    case .geometryModifier: .geometryModifier
    case .postLightingShader: .postLightingShader
    case .string: .string
    case .asset: .texture
    case .token, .quat: .string
    @unknown default: fatalError("fromWKBridgeDataType: unknown WKBridgeDataType")
    }
}

private func makeCGColor3(colorSpace: CGColorSpace, _ a: CGFloat, _ b: CGFloat, _ c: CGFloat) -> CGColor {
    // rdar://177971578 - CG should provide an API for not requiring unsafe
    guard let cs = unsafe CGColor(colorSpace: colorSpace, components: [a, b, c, 1.0]) else {
        fatalError("\(a) \(b) \(c) could not form a CGColor with colorSpace \(colorSpace)")
    }
    return cs
}

private func makeCGColor4(colorSpace: CGColorSpace, _ a: CGFloat, _ b: CGFloat, _ c: CGFloat, _ d: CGFloat) -> CGColor {
    // rdar://177971578 - CG should provide an API for not requiring unsafe
    guard let cs = unsafe CGColor(colorSpace: colorSpace, components: [a, b, c, d]) else {
        fatalError("\(a) \(b) \(c) \(d) could not form a CGColor with colorSpace \(colorSpace)")
    }
    return cs
}

private func fromWKBridgeConstant(_ constant: WKBridgeConstantContainer) -> ShaderGraph.Value {
    let values = constant.constantValues
    // Resolve the color space from the transmitted name, falling back to extendedLinearSRGB.
    // USD color3f/color4f values are in linear sRGB, so extendedLinearSRGB is the correct fallback.
    // swift-format-ignore: NeverForceUnwrap
    guard
        let colorSpace =
            (constant.colorSpaceName.flatMap { CGColorSpace(name: $0 as CFString) }
                ?? CGColorSpace(name: CGColorSpace.extendedLinearSRGB))
    else {
        fatalError("extendedLinearSRGB could not be constructed, should never occur")
    }

    switch constant.constant {
    case .bool:
        guard let v = values.first else { fatalError("fromWKBridgeConstant: missing value for bool constant '\(constant.name)'") }
        return .bool(v.number.boolValue)
    case .uchar:
        guard let v = values.first else { fatalError("fromWKBridgeConstant: missing value for uchar constant '\(constant.name)'") }
        return .uchar(v.number.uint8Value)
    case .int:
        guard let v = values.first else { fatalError("fromWKBridgeConstant: missing value for int constant '\(constant.name)'") }
        return .int(Int32(v.number.intValue))
    case .uint:
        guard let v = values.first else { fatalError("fromWKBridgeConstant: missing value for uint constant '\(constant.name)'") }
        return .uint(UInt32(v.number.uintValue))
    case .half:
        guard let v = values.first else { fatalError("fromWKBridgeConstant: missing value for half constant '\(constant.name)'") }
        return .half(.init(bitPattern: v.number.uint16Value))
    case .float:
        guard let v = values.first else { fatalError("fromWKBridgeConstant: missing value for float constant '\(constant.name)'") }
        return .float(v.number.floatValue)
    case .string, .token, .asset:
        guard let v = values.first else { fatalError("fromWKBridgeConstant: missing value for string constant '\(constant.name)'") }
        return .string(v.string)
    case .timecode:
        guard let v = values.first else { fatalError("fromWKBridgeConstant: missing value for timecode constant '\(constant.name)'") }
        return .float(Float(v.number.doubleValue))
    case .float2, .texCoord2f:
        guard values.count >= 2 else {
            fatalError("fromWKBridgeConstant: expected 2 values for float2 constant '\(constant.name)', got \(values.count)")
        }
        return .float2(
            SIMD2<Float>(
                values[0].number.floatValue,
                values[1].number.floatValue
            )
        )
    case .vector3f, .float3, .point3f, .normal3f, .texCoord3f:
        // All float3-based semantic types map to float3
        guard values.count >= 3 else {
            fatalError("fromWKBridgeConstant: expected 3 values for float3 constant '\(constant.name)', got \(values.count)")
        }
        return .float3(
            SIMD3<Float>(
                values[0].number.floatValue,
                values[1].number.floatValue,
                values[2].number.floatValue
            )
        )
    case .float4, .matrix2f:
        guard values.count >= 4 else {
            fatalError("fromWKBridgeConstant: expected 4 values for float4 constant '\(constant.name)', got \(values.count)")
        }
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
        guard values.count >= 2 else {
            fatalError("fromWKBridgeConstant: expected 2 values for half2 constant '\(constant.name)', got \(values.count)")
        }
        return .half2(
            SIMD2<Float16>(
                .init(bitPattern: values[0].number.uint16Value),
                .init(bitPattern: values[1].number.uint16Value)
            )
        )
    case .vector3h, .half3, .point3h, .normal3h, .texCoord3h:
        // All half3-based semantic types map to half3
        guard values.count >= 3 else {
            fatalError("fromWKBridgeConstant: expected 3 values for half3 constant '\(constant.name)', got \(values.count)")
        }
        return .half3(
            SIMD3<Float16>(
                .init(bitPattern: values[0].number.uint16Value),
                .init(bitPattern: values[1].number.uint16Value),
                .init(bitPattern: values[2].number.uint16Value)
            )
        )
    case .half4:
        guard values.count >= 4 else {
            fatalError("fromWKBridgeConstant: expected 4 values for half4 constant '\(constant.name)', got \(values.count)")
        }
        return .half4(
            SIMD4<Float16>(
                .init(bitPattern: values[0].number.uint16Value),
                .init(bitPattern: values[1].number.uint16Value),
                .init(bitPattern: values[2].number.uint16Value),
                .init(bitPattern: values[3].number.uint16Value)
            )
        )
    case .int2:
        guard values.count >= 2 else {
            fatalError("fromWKBridgeConstant: expected 2 values for int2 constant '\(constant.name)', got \(values.count)")
        }
        return .int2(
            SIMD2<Int32>(
                values[0].number.int32Value,
                values[1].number.int32Value
            )
        )
    case .int3:
        guard values.count >= 3 else {
            fatalError("fromWKBridgeConstant: expected 3 values for int3 constant '\(constant.name)', got \(values.count)")
        }
        return .int3(
            SIMD3<Int32>(
                values[0].number.int32Value,
                values[1].number.int32Value,
                values[2].number.int32Value
            )
        )
    case .int4:
        guard values.count >= 4 else {
            fatalError("fromWKBridgeConstant: expected 4 values for int4 constant '\(constant.name)', got \(values.count)")
        }
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
            fatalError("fromWKBridgeConstant: expected 9 values for matrix3f constant '\(constant.name)', got \(values.count)")
        }
        return .float3x3(
            .init(
                SIMD3<Float>(values[0].number.floatValue, values[1].number.floatValue, values[2].number.floatValue),
                SIMD3<Float>(values[3].number.floatValue, values[4].number.floatValue, values[5].number.floatValue),
                SIMD3<Float>(values[6].number.floatValue, values[7].number.floatValue, values[8].number.floatValue)
            )
        )
    case .matrix4f:
        // matrix4f maps to float4x4 - needs 16 values (4 columns of 4 rows each)
        guard values.count >= 16 else {
            fatalError("fromWKBridgeConstant: expected 16 values for matrix4f constant '\(constant.name)', got \(values.count)")
        }
        return .float4x4(
            .init(
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
        )
    case .matrix2h:
        guard values.count >= 4 else {
            fatalError("fromWKBridgeConstant: expected 4 values for matrix2h constant '\(constant.name)', got \(values.count)")
        }
        return .half2x2(
            simd_half2x2(
                columns: (
                    SIMD2<Float16>(.init(bitPattern: values[0].number.uint16Value), .init(bitPattern: values[1].number.uint16Value)),
                    SIMD2<Float16>(.init(bitPattern: values[2].number.uint16Value), .init(bitPattern: values[3].number.uint16Value))
                )
            )
        )
    case .matrix3h:
        guard values.count >= 9 else {
            fatalError("fromWKBridgeConstant: expected 9 values for matrix3h constant '\(constant.name)', got \(values.count)")
        }
        return .half3x3(
            simd_half3x3(
                columns: (
                    SIMD3<Float16>(
                        .init(bitPattern: values[0].number.uint16Value),
                        .init(bitPattern: values[1].number.uint16Value),
                        .init(bitPattern: values[2].number.uint16Value)
                    ),
                    SIMD3<Float16>(
                        .init(bitPattern: values[3].number.uint16Value),
                        .init(bitPattern: values[4].number.uint16Value),
                        .init(bitPattern: values[5].number.uint16Value)
                    ),
                    SIMD3<Float16>(
                        .init(bitPattern: values[6].number.uint16Value),
                        .init(bitPattern: values[7].number.uint16Value),
                        .init(bitPattern: values[8].number.uint16Value)
                    )
                )
            )
        )
    case .matrix4h:
        guard values.count >= 16 else {
            fatalError("fromWKBridgeConstant: expected 16 values for matrix4h constant '\(constant.name)', got \(values.count)")
        }
        return .half4x4(
            simd_half4x4(
                columns: (
                    SIMD4<Float16>(
                        .init(bitPattern: values[0].number.uint16Value),
                        .init(bitPattern: values[1].number.uint16Value),
                        .init(bitPattern: values[2].number.uint16Value),
                        .init(bitPattern: values[3].number.uint16Value)
                    ),
                    SIMD4<Float16>(
                        .init(bitPattern: values[4].number.uint16Value),
                        .init(bitPattern: values[5].number.uint16Value),
                        .init(bitPattern: values[6].number.uint16Value),
                        .init(bitPattern: values[7].number.uint16Value)
                    ),
                    SIMD4<Float16>(
                        .init(bitPattern: values[8].number.uint16Value),
                        .init(bitPattern: values[9].number.uint16Value),
                        .init(bitPattern: values[10].number.uint16Value),
                        .init(bitPattern: values[11].number.uint16Value)
                    ),
                    SIMD4<Float16>(
                        .init(bitPattern: values[12].number.uint16Value),
                        .init(bitPattern: values[13].number.uint16Value),
                        .init(bitPattern: values[14].number.uint16Value),
                        .init(bitPattern: values[15].number.uint16Value)
                    )
                )
            )
        )
    case .quatf, .quath:
        // quath/quatf don't exist in the enum - map to float4
        guard values.count >= 4 else {
            fatalError("fromWKBridgeConstant: expected 4 values for quat constant '\(constant.name)', got \(values.count)")
        }
        return .float4(
            SIMD4<Float>(
                values[0].number.floatValue,
                values[1].number.floatValue,
                values[2].number.floatValue,
                values[3].number.floatValue
            )
        )
    case .cgColor3:
        guard values.count >= 3 else {
            fatalError("fromWKBridgeConstant: expected 3 values for color3 constant '\(constant.name)', got \(values.count)")
        }
        return .cgColor3(
            makeCGColor3(
                colorSpace: colorSpace,
                CGFloat(values[0].number.floatValue),
                CGFloat(values[1].number.floatValue),
                CGFloat(values[2].number.floatValue)
            )
        )
    case .cgColor4:
        guard values.count >= 4 else {
            fatalError("fromWKBridgeConstant: expected 4 values for color4 constant '\(constant.name)', got \(values.count)")
        }
        return .cgColor4(
            makeCGColor4(
                colorSpace: colorSpace,
                CGFloat(values[0].number.floatValue),
                CGFloat(values[1].number.floatValue),
                CGFloat(values[2].number.floatValue),
                CGFloat(values[3].number.floatValue)
            )
        )
    case .color4f:
        // USD/MaterialX color4f — encoded as 4 raw floats without CGColor clamping.
        guard values.count >= 4 else {
            fatalError("fromWKBridgeConstant: expected 4 values for color4f constant '\(constant.name)', got \(values.count)")
        }
        return .cgColor4(
            makeCGColor4(
                colorSpace: colorSpace,
                CGFloat(values[0].number.floatValue),
                CGFloat(values[1].number.floatValue),
                CGFloat(values[2].number.floatValue),
                CGFloat(values[3].number.floatValue)
            )
        )
    case .color3f:
        // USD/MaterialX color3f — encoded as 3 raw floats without CGColor clamping.
        guard values.count >= 3 else {
            fatalError("fromWKBridgeConstant: expected 3 values for color3f constant '\(constant.name)', got \(values.count)")
        }
        return .cgColor3(
            makeCGColor3(
                colorSpace: colorSpace,
                CGFloat(values[0].number.floatValue),
                CGFloat(values[1].number.floatValue),
                CGFloat(values[2].number.floatValue)
            )
        )
    case .color3h:
        // USD/MaterialX color3h — half-precision; represented as cgColor3 in ShaderGraph.Value.
        guard values.count >= 3 else {
            fatalError("fromWKBridgeConstant: expected 3 values for color3h constant '\(constant.name)', got \(values.count)")
        }
        return .cgColor3(
            makeCGColor3(
                colorSpace: colorSpace,
                CGFloat(values[0].number.floatValue),
                CGFloat(values[1].number.floatValue),
                CGFloat(values[2].number.floatValue)
            )
        )
    case .color4h:
        // USD/MaterialX color4h — half-precision; represented as cgColor4 in ShaderGraph.Value.
        guard values.count >= 4 else {
            fatalError("fromWKBridgeConstant: expected 4 values for color4h constant '\(constant.name)', got \(values.count)")
        }
        return .cgColor4(
            makeCGColor4(
                colorSpace: colorSpace,
                CGFloat(values[0].number.floatValue),
                CGFloat(values[1].number.floatValue),
                CGFloat(values[2].number.floatValue),
                CGFloat(values[3].number.floatValue)
            )
        )
    @unknown default:
        fatalError("fromWKBridgeConstant: unhandled constant type \(constant.constant) for '\(constant.name)'")
    }
}

private func usdStageType(forMIMEType mimeType: String) -> UTType {
    switch mimeType {
    case "model/vnd.usdz+zip":
        return .usdz
    case "model/usd", "model/vnd.pixar.usd":
        return .usd
    default:
        return UTType(mimeType: mimeType) ?? .usdz
    }
}

final class USDModelLoader {
    fileprivate var usdPlayer: USDPlayer?
    fileprivate var stage: USDStage?
    fileprivate var data: Data?
    private let objcLoader: WKBridgeModelLoader
    private let gpuFamily: MTLGPUFamily
    private var rootJointIndicesCache: [String: [UInt32]] = [:]

    // TextureID → shader parameter name (e.g. "diffuseColor"). Populated from
    // MaterialData.assignedTextures so we can set WKBridgeUpdateTexture.hashString
    // to the parameter name that the receiver matches against `parameter.name`
    // at material processing time.
    private var textureIdToParameterName: [TextureID: String] = [:]

    // Cached DeformationData by ID. MeshData.MeshType.deformable(DeformationID)
    // points to a DeformationData received in deformationAdditions; we hold it
    // here so subsequent mesh updates can re-use the same deformation binding.
    private var deformationDataById: [DeformationID: DeformationData] = [:]

    // Per-frame field-level overrides applied on top of `deformationDataById`
    // when constructing the WKBridgeDeformationData we send to the receiver.
    // DeformationData.Update fields (jointTransforms, weights, etc.) are merged
    // here so animated skeletons keep deforming on subsequent frames.
    private var deformationOverridesById: [DeformationID: DeformationOverrides] = [:]

    // Reverse lookup so a deformation update can find its mesh: needed to
    // synthesize a delta WKBridgeUpdateMesh that refreshes the receiver's GPU
    // buffers when only the deformation changed this frame (no MeshData.Update).
    private var meshIdByDeformationId: [DeformationID: MeshID] = [:]
    private var meshPathByDeformationId: [DeformationID: String] = [:]

    // ID → primPath maps so we can build WKBridgeTypedResourceId for removals
    // (the new public ID types do not expose a `.path` accessor).
    private var meshIdToPath: [MeshID: String] = [:]
    private var materialIdToPath: [MaterialID: String] = [:]
    private var textureIdToPath: [TextureID: String] = [:]

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

    init(objcInstance: WKBridgeModelLoader, gpuFamily: MTLGPUFamily) {
        objcLoader = objcInstance
        self.gpuFamily = gpuFamily
    }

    private func resetLoaderState() {
        rootJointIndicesCache.removeAll()
        textureIdToParameterName.removeAll()
        deformationDataById.removeAll()
        deformationOverridesById.removeAll()
        meshIdByDeformationId.removeAll()
        meshPathByDeformationId.removeAll()
        meshIdToPath.removeAll()
        materialIdToPath.removeAll()
        textureIdToPath.removeAll()
    }

    func loadModel(data: Foundation.Data, mimeType: String) -> Bool {
        resetLoaderState()
        do {
            self.stage = try USDStage(data, type: usdStageType(forMIMEType: mimeType))
            guard let stage = self.stage else {
                logError("model data is corrupted")
                return false
            }
            self.data = data
            self.setupTimes(from: stage)
            self.usdPlayer = USDPlayer(stage: stage, gpuFamily: gpuFamily)
            return true
        } catch {
            logError(error.localizedDescription)
            return false
        }
    }

    func loadEnvironmentMap(_ data: Foundation.Data) -> WKBridgeUpdateTexture? {
        guard
            let usdPlayer = self.usdPlayer,
            let textureData = try? usdPlayer.importCustomIBLTexture(data: data)
        else { return nil }

        return webUpdateTextureRequestFromTextureData(textureData, hashString: "")
    }

    func setupTimes(from stage: USDStage) {
        timeCodePerSecond = stage.timeCodesPerSecond > 0 ? stage.timeCodesPerSecond : 1
        let timeCodeRange = stage.timeCodeRange
        startTime = (timeCodeRange.lowerBound.value ?? 0) / timeCodePerSecond
        endTime = (timeCodeRange.upperBound.value ?? 0) / timeCodePerSecond
        time = 0
    }

    func duration() -> Double {
        endTime - startTime
    }

    func treatZAsUpAxis() -> Bool {
        stage?.upAxis == USDToken("Z")
    }

    func currentTime() -> Double {
        time - startTime
    }

    func setCurrentTime(_ newTime: Double) {
        time = startTime + newTime
    }

    func update(deltaTime: TimeInterval) {
        guard let usdPlayer else { return }

        let timeCode = USDStage.TimeCode(time * timeCodePerSecond)
        let frameUpdateResult = usdPlayer.update(timeCode: timeCode)

        // Always advance time, even when there's no frame update.
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

        // Optional<~Copyable> must be unwrapped by consuming the optional.
        switch consume frameUpdateResult {
        case .none:
            return
        case .some(var frameUpdate):
            processFrameUpdate(&frameUpdate)
        }
    }

    private func processFrameUpdate(_ frameUpdate: inout USDPlayer.FrameUpdate) {
        for error in frameUpdate.errors {
            logError("USD render error: \(error.localizedDescription)")
        }

        // ===== Phase 1: take deformation data =====
        var dirtyDeformations: Set<DeformationID> = []
        for id in frameUpdate.deformationAdditions {
            if let deformation = frameUpdate.takeDeformationAddition(id: id) {
                deformationDataById[id] = deformation
                deformationOverridesById[id] = DeformationOverrides()
                dirtyDeformations.insert(id)
            }
        }
        // Apply DeformationData.Update fields onto the cached overrides. The
        // receiver's deformation context will pick up the new joint transforms
        // / blend weights / etc. from the next WKBridgeUpdateMesh we send for
        // the corresponding mesh.
        for id in frameUpdate.deformationUpdates {
            if let update = frameUpdate.takeDeformationUpdate(id: id) {
                var overrides = deformationOverridesById[id] ?? DeformationOverrides()
                overrides.apply(update)
                deformationOverridesById[id] = overrides
                dirtyDeformations.insert(id)
            }
        }
        for id in frameUpdate.deformationRemovals {
            deformationDataById.removeValue(forKey: id)
            deformationOverridesById.removeValue(forKey: id)
            meshIdByDeformationId.removeValue(forKey: id)
            meshPathByDeformationId.removeValue(forKey: id)
        }

        // ===== Phase 2: take material data =====
        var materialAdditions: [MaterialData] = []
        for id in frameUpdate.materialAdditions {
            if let material = frameUpdate.takeMaterialAddition(id: id) {
                materialIdToPath[id] = material.primPath
                for (paramName, texId) in material.assignedTextures {
                    textureIdToParameterName[texId] = paramName
                }
                materialAdditions.append(material)
            }
        }
        // Material updates only carry assignedTextures changes — update the
        // texture → parameter map so future textures get the right hashString.
        // The receiver isn't re-notified because MaterialData.Update has no
        // shaderGraph to rebuild from.
        for id in frameUpdate.materialUpdates {
            if let update = frameUpdate.takeMaterialUpdate(id: id) {
                if let textures = update.assignedTextures {
                    for (paramName, texId) in textures {
                        textureIdToParameterName[texId] = paramName
                    }
                }
            }
        }

        // ===== Phase 3: take texture data =====
        var textureAdditions: [TextureData] = []
        for id in frameUpdate.textureAdditions {
            if let textureData = frameUpdate.takeTextureAddition(id: id) {
                textureIdToPath[id] = textureData.assetPath
                textureAdditions.append(textureData)
            }
        }

        // ===== Phase 4: take mesh data =====
        var meshAdditions: [MeshData] = []
        var meshUpdates: [MeshData.Update] = []
        for id in frameUpdate.meshAdditions {
            if let mesh = frameUpdate.takeMeshAddition(id: id) {
                meshIdToPath[id] = mesh.primPath
                if let did = deformationID(from: mesh.meshType) {
                    meshIdByDeformationId[did] = id
                    meshPathByDeformationId[did] = mesh.primPath
                }
                meshAdditions.append(mesh)
            }
        }
        for id in frameUpdate.meshUpdates {
            if let update = frameUpdate.takeMeshUpdate(id: id) {
                if let mt = update.meshType, let did = deformationID(from: mt) {
                    meshIdByDeformationId[did] = id
                    if let p = meshIdToPath[id] { meshPathByDeformationId[did] = p }
                }
                meshUpdates.append(update)
            }
        }

        // ===== Build removal bridge ids and drain bookkeeping =====
        let meshRemovalIds = frameUpdate.meshRemovals.map { id -> WKBridgeTypedResourceId in
            let path = meshIdToPath.removeValue(forKey: id) ?? ""
            return makeTypedResourceId(
                uuid: makeUUID(from: id.description),
                path: path,
                hashValue: id.hashValue
            )
        }
        let materialRemovalIds = frameUpdate.materialRemovals.map { id -> WKBridgeTypedResourceId in
            let path = materialIdToPath.removeValue(forKey: id) ?? ""
            return makeTypedResourceId(
                uuid: makeUUID(from: id.description),
                path: path,
                hashValue: id.hashValue
            )
        }
        let textureRemovalIds = frameUpdate.textureRemovals.map { id -> WKBridgeTypedResourceId in
            let path = textureIdToPath.removeValue(forKey: id) ?? ""
            textureIdToParameterName.removeValue(forKey: id)
            return makeTypedResourceId(
                uuid: makeUUID(from: id.description),
                path: path,
                hashValue: id.hashValue
            )
        }

        dispatchToReceiver(
            meshRemovalIds: meshRemovalIds,
            materialRemovalIds: materialRemovalIds,
            textureRemovalIds: textureRemovalIds,
            textureAdditions: textureAdditions,
            materialAdditions: materialAdditions,
            meshAdditions: meshAdditions,
            meshUpdates: meshUpdates,
            dirtyDeformations: dirtyDeformations
        )
    }

    private func dispatchToReceiver(
        meshRemovalIds: [WKBridgeTypedResourceId],
        materialRemovalIds: [WKBridgeTypedResourceId],
        textureRemovalIds: [WKBridgeTypedResourceId],
        textureAdditions: [TextureData],
        materialAdditions: [MaterialData],
        meshAdditions: [MeshData],
        meshUpdates: [MeshData.Update],
        dirtyDeformations: Set<DeformationID>
    ) {
        // ===== Dispatch to receiver in dependency order =====
        // (removals → textures → materials → meshes)
        self.objcLoader.processRemovals(
            removals: WKBridgeRemovals(
                meshRemovals: meshRemovalIds,
                materialRemovals: materialRemovalIds,
                textureRemovals: textureRemovalIds
            )
        )
        self.objcLoader.updateTexture(
            webRequest: textureAdditions.map { textureData in
                webUpdateTextureRequestFromTextureData(
                    textureData,
                    hashString: textureIdToParameterName[textureData.id] ?? ""
                )
            }
        )
        self.objcLoader.updateMaterial(
            webRequest: materialAdditions.map { material in
                WKBridgeUpdateMaterial(
                    materialGraph: toWebMaterialGraph(material.shaderGraph),
                    identifier: makeTypedResourceId(
                        uuid: makeUUID(from: material.id.description),
                        path: material.primPath,
                        hashValue: material.id.hashValue
                    )
                )
            }
        )

        let stageForMeshUpdates = self.stage
        // Track which mesh IDs already get a real mesh update this frame so we
        // don't duplicate them when synthesizing per-deformation deltas below.
        var meshesAlreadyDispatched: Set<MeshID> = []

        let meshAdditionRequests = meshAdditions.map { meshData -> WKBridgeUpdateMesh in
            meshesAlreadyDispatched.insert(meshData.id)
            let bridgeDef = bridgeDeformationFor(meshType: meshData.meshType, primPath: meshData.primPath, stage: stageForMeshUpdates)
            return webMeshFromMeshData(meshData, deformation: bridgeDef)
        }
        let meshUpdateRequests = meshUpdates.map { update -> WKBridgeUpdateMesh in
            meshesAlreadyDispatched.insert(update.id)
            let path = meshIdToPath[update.id] ?? ""
            let bridgeDef: WKBridgeDeformationData?
            if let mt = update.meshType {
                bridgeDef = bridgeDeformationFor(meshType: mt, primPath: path, stage: stageForMeshUpdates)
            } else if let did = meshIdByDeformationId.first(where: { $0.value == update.id })?.key {
                bridgeDef = bridgeDeformation(
                    base: deformationDataById[did],
                    overrides: deformationOverridesById[did],
                    rootJointIndices: resolveRootJointIndices(
                        primPath: path,
                        needsRoots: deformationDataById[did]?.skinning != nil,
                        stage: stageForMeshUpdates
                    )
                )
            } else {
                bridgeDef = nil
            }
            return webMeshFromMeshUpdate(update, primPath: path, deformation: bridgeDef)
        }

        // Synthesize deltas for deformations that ticked without a matching
        // mesh update — without this, an animation with a static mesh never
        // reaches the receiver after the first frame.
        var synthesizedRequests: [WKBridgeUpdateMesh] = []
        for did in dirtyDeformations {
            guard let meshId = meshIdByDeformationId[did],
                !meshesAlreadyDispatched.contains(meshId),
                let path = meshPathByDeformationId[did] ?? meshIdToPath[meshId]
            else { continue }
            let rootIndices = resolveRootJointIndices(
                primPath: path,
                needsRoots: deformationDataById[did]?.skinning != nil,
                stage: stageForMeshUpdates
            )
            guard
                let bridgeDef = bridgeDeformation(
                    base: deformationDataById[did],
                    overrides: deformationOverridesById[did],
                    rootJointIndices: rootIndices
                )
            else { continue }
            synthesizedRequests.append(webMeshDeformationDelta(meshId: meshId, primPath: path, deformation: bridgeDef))
        }

        self.objcLoader.updateMesh(webRequest: meshAdditionRequests + meshUpdateRequests + synthesizedRequests)
    }

    private func bridgeDeformationFor(meshType: MeshData.MeshType, primPath: String, stage: USDStage?) -> WKBridgeDeformationData? {
        guard let did = deformationID(from: meshType) else { return nil }
        let base = deformationDataById[did]
        let rootIndices = resolveRootJointIndices(
            primPath: primPath,
            needsRoots: base?.skinning != nil,
            stage: stage
        )
        return bridgeDeformation(base: base, overrides: deformationOverridesById[did], rootJointIndices: rootIndices)
    }

    private func resolveRootJointIndices(primPath: String, needsRoots: Bool, stage: USDStage?) -> [UInt32] {
        guard needsRoots, let stage, !primPath.isEmpty else { return [] }
        if let cached = rootJointIndicesCache[primPath] {
            return cached
        }
        let computed = rootJointIndices(forMeshAt: primPath, in: stage)
        rootJointIndicesCache[primPath] = computed
        return computed
    }
}

@objc
@implementation
extension WKBridgeModelLoader {
    @nonobjc
    var loader: USDModelLoader?
    @nonobjc
    var modelUpdated: (([WKBridgeUpdateMesh]) -> (Void))?
    @nonobjc
    var textureUpdatedCallback: (([WKBridgeUpdateTexture]) -> (Void))?
    @nonobjc
    var materialUpdatedCallback: (([WKBridgeUpdateMaterial]) -> (Void))?
    @nonobjc
    var processRemovalsCallback: ((WKBridgeRemovals) -> (Void))?

    @nonobjc
    fileprivate var retainedRequests: Set<NSObject> = []

    @objc(initWithGPUFamily:)
    init(gpuFamily: MTLGPUFamily) {
        super.init()

        self.loader = USDModelLoader(objcInstance: self, gpuFamily: gpuFamily)
    }

    @objc(
        setCallbacksWithModelUpdatedCallback:
        textureUpdatedCallback:
        materialUpdatedCallback:
        processRemovalsCallback:
    )
    func setCallbacksWithModelUpdatedCallback(
        _ modelUpdatedCallback: @escaping (([WKBridgeUpdateMesh]) -> (Void)),
        textureUpdatedCallback: @escaping (([WKBridgeUpdateTexture]) -> (Void)),
        materialUpdatedCallback: @escaping (([WKBridgeUpdateMaterial]) -> (Void)),
        processRemovalsCallback: @escaping ((WKBridgeRemovals) -> (Void))
    ) {
        self.modelUpdated = modelUpdatedCallback
        self.textureUpdatedCallback = textureUpdatedCallback
        self.materialUpdatedCallback = materialUpdatedCallback
        self.processRemovalsCallback = processRemovalsCallback
    }

    func loadModel(_ data: Foundation.Data, mimeType: String) -> Bool {
        self.loader?.loadModel(data: data, mimeType: mimeType) ?? false
    }

    func loadEnvironmentMap(_ data: Foundation.Data) -> WKBridgeUpdateTexture? {
        self.loader?.loadEnvironmentMap(data)
    }

    func update(_ deltaTime: Double) {
        self.loader?.update(deltaTime: deltaTime)
    }

    func setLoop(_ loop: Bool) {
        self.loader?.loop = loop
    }

    func requestCompleted(_ request: NSObject) {
        retainedRequests.remove(request)
    }

    func duration() -> Double {
        guard let loader else {
            return 0.0
        }
        return loader.duration()
    }

    func treatZAsUpAxis() -> Bool {
        loader?.treatZAsUpAxis() ?? false
    }

    func currentTime() -> Double {
        guard let loader else {
            return 0.0
        }
        return loader.currentTime()
    }

    func setCurrentTime(_ newTime: Double) {
        loader?.setCurrentTime(newTime)
    }

    fileprivate func updateMesh(webRequest: [WKBridgeUpdateMesh]) {
        if webRequest.isEmpty {
            return
        }
        if let modelUpdated {
            retainedRequests.insert(webRequest as NSArray)
            modelUpdated(webRequest)
        }
    }

    fileprivate func updateTexture(webRequest: [WKBridgeUpdateTexture]) {
        if webRequest.isEmpty {
            return
        }
        if let textureUpdatedCallback {
            retainedRequests.insert(webRequest as NSArray)
            textureUpdatedCallback(webRequest)
        }
    }

    fileprivate func updateMaterial(webRequest: [WKBridgeUpdateMaterial]) {
        if webRequest.isEmpty {
            return
        }
        if let materialUpdatedCallback {
            retainedRequests.insert(webRequest as NSArray)
            materialUpdatedCallback(webRequest)
        }
    }
    fileprivate func processRemovals(removals: WKBridgeRemovals) {
        if removals.isEmpty() {
            return
        }

        if let processRemovalsCallback {
            retainedRequests.insert(removals)
            processRemovalsCallback(removals)
        }
    }
}

private func makeFallBackTextureResource(
    _ renderContext: any LowLevelRenderContext,
    commandQueue: any MTLCommandQueue,
    device: any MTLDevice,
    memoryOwner: task_id_token_t
) -> LowLevelTextureResource {
    // Create 1x1 white fallback texture
    let fallbackDescriptor = LowLevelTextureResource.Descriptor(
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

    let stagingBuffer = device.makeBuffer(fromSpan: whitePixel.span, length: whitePixel.count, memoryOwner: memoryOwner)

    // Create command buffer to upload white pixel data
    // swift-format-ignore: NeverForceUnwrap
    let fallbackCommandBuffer = commandQueue.makeCommandBuffer()!
    let fallbackMTLTexture = fallbackTexture.replace(commandBuffer: fallbackCommandBuffer, memoryOwner: memoryOwner)

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

#else
@objc
@implementation
extension WKBridgeUSDConfiguration {
    init(device: any MTLDevice, memoryOwner: task_id_token_t) {
    }

    var standardDynamicRange: Bool = false

    func makeStandaloneResources() async {
    }

    func createMaterialCompiler() {
    }

    func makeRendererResources() async {
    }

    func createRenderer() {
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

    func commandBuffer() -> (any MTLCommandBuffer)? {
        nil
    }

    @objc(renderWithTexture:commandBuffer:)
    func render(with texture: any MTLTexture, commandBuffer: any MTLCommandBuffer) {
    }

    @objc(updateTexture:)
    func updateTexture(_ data: [WKBridgeUpdateTexture]) {
    }

    @objc(updateMaterial:)
    func updateMaterial(_ data: [WKBridgeUpdateMaterial]) {
    }

    @objc(updateMesh:)
    func updateMesh(_ data: [WKBridgeUpdateMesh]) {
    }

    @objc
    func processRemovals(
        _ meshRemovals: [WKBridgeTypedResourceId],
        materialRemovals: [WKBridgeTypedResourceId],
        textureRemovals: [WKBridgeTypedResourceId]
    ) -> Bool {
        false
    }

    @objc(setTransform:)
    func setTransform(_ transform: simd_float4x4) {
    }

    func setFOV(_ fovY: Float) {
    }

    func setPlaying(_ play: Bool) {
    }

    func setEnvironmentMap(_ imageAsset: WKBridgeUpdateTexture) {
    }
}

@objc
@implementation
extension WKBridgeModelLoader {
    @objc(initWithGPUFamily:)
    init(gpuFamily: MTLGPUFamily) {
        super.init()
    }

    @objc(
        setCallbacksWithModelUpdatedCallback:
        textureUpdatedCallback:
        materialUpdatedCallback:
        processRemovalsCallback:
    )
    func setCallbacksWithModelUpdatedCallback(
        _ modelUpdatedCallback: @escaping (([WKBridgeUpdateMesh]) -> (Void)),
        textureUpdatedCallback: @escaping (([WKBridgeUpdateTexture]) -> (Void)),
        materialUpdatedCallback: @escaping (([WKBridgeUpdateMaterial]) -> (Void)),
        processRemovalsCallback: @escaping ((WKBridgeRemovals) -> (Void))
    ) {
    }

    func loadModel(_ data: Foundation.Data, mimeType: String) -> Bool {
        false
    }

    func loadEnvironmentMap(_ data: Foundation.Data) -> WKBridgeUpdateTexture? {
        nil
    }

    func update(_ deltaTime: Double) {
    }

    func setLoop(_ loop: Bool) {
    }

    func requestCompleted(_ request: NSObject) {
    }

    func duration() -> Double {
        0.0
    }

    func treatZAsUpAxis() -> Bool {
        false
    }

    func currentTime() -> Double {
        0.0
    }

    func setCurrentTime(_ newTime: Double) {
    }
}
#endif
