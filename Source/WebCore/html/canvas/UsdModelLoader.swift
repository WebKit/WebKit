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

#if canImport(USDDirectDrawIO) && canImport(WebCore_Internal)

internal import WebCore_Internal

internal import Foundation
internal import DirectDrawXPCTypes
import XPC
internal import QuartzCore_Private
internal import ShaderGraph
internal import USDDirectDrawIO
internal import os

final class USDModelLoader: DDRenderDelegate {
    private let renderer: UsdDDRenderer
    private var timer: Timer? = nil
    private let nodeDefinitionStore = NodeDefinitionStore()
    private let objcLoader: WebUSDModelLoader

    @Observable
    final class Host: Identifiable {
        enum State {
            case connecting
            case connected(Connected)
        }
        struct Connected {
            let layerHost = CALayerHost()
            let contextId: UInt32
        }

        let id: UUID
        var state: State

        init(id: UUID, state: State) {
            self.id = id
            self.state = state
        }
    }
    private(set) var hosts: [Host] = []

    @Observable
    final class Mesh: Identifiable {
        enum State {
            case pending
            case created
        }

        let id: UUID
        let descriptor: DDMeshDescriptor
        var state: State

        init(id: UUID, descriptor: DDMeshDescriptor, state: State) {
            self.id = id
            self.descriptor = descriptor
            self.state = state
        }
    }
    private(set) var meshes: [Mesh] = []
    private(set) var primIdToMeshId: [String: UUID] = [:]

    @Observable
    final class Material: Identifiable {
        enum State {
            case pending
            case created
        }

        let id: UUID
        var state: State

        init(id: UUID, state: State) {
            self.id = id
            self.state = state
        }
    }
    private(set) var materials: [Material] = []
    private(set) var primIdToMaterialId: [String: UUID] = [:]
    private(set) var materialIdToTextureIds: [UUID: Set<UUID>] = [:]
    private(set) var assetPathToTextureId: [String: UUID] = [:]

    init(objcInstance: WebUSDModelLoader) {
        objcLoader = objcInstance
        renderer = UsdDDRenderer()
        renderer.delegate = self
        timer = Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { timer in
            let timeInterval = timer.timeInterval
            self.renderer.update(deltaTime: timeInterval)
        }
    }

    deinit {
        self.renderer.delegate = nil
    }

    func loadModel(from url: URL) {
        renderer.loadModel(from: url)
        renderer.resetTime()
    }

    func loadModel(from data: Data) {
        renderer.loadModel(from: data)
        renderer.resetTime()
    }

    func addHost() {
        let host = Host(id: UUID(), state: .connecting)
        hosts.append(host)
        Logger.modelWCP.info("AddHostedRendererRequest")
    }

    func updateHost(host: Host, size: CGSize, contentsScale: CGFloat) {
        guard case .connected = host.state else {
            return
        }
        objcLoader.updateHostedRenderer(id: host.id, size: size, contentsScale: contentsScale)
    }

    func removeHost(id: UUID) {
        if let host = self.hosts.first(where: { $0.id == id }),
            case .connected = host.state
        {
            Logger.modelWCP.info("removeHost")
        }
        hosts.removeAll(where: { $0.id == id })
    }

    func removeMesh(id: UUID) {
        if let mesh = self.meshes.first(where: { $0.id == id }),
            case .created = mesh.state
        {
            Logger.modelWCP.info("removeMesh")
        }
        meshes.removeAll(where: { $0.id == id })
    }

    func setCameraDistance(_ distance: Float) {
        Logger.modelWCP.info("setCameraDistance")
    }

    func setEnableModelRotation(_ enable: Bool) {
        Logger.modelWCP.info("setEnableModelRotation")
    }

    // MARK: DDRenderDelegate

    nonisolated func meshCreated(identifier: String) {
        Logger.modelWCP.info("mesh created: \(identifier)")
    }

    nonisolated func meshUpdated(identifier: String, data: DDMeshData) {
        Logger.modelWCP.info("mesh updated")
        let resourceData = data
        let instanceTransforms = data.instanceTransforms
        let materialPrims = data.materialPrims

        let materialIds: [UUID] = materialPrims.compactMap { primId in
            guard let materialId = primIdToMaterialId[primId] else {
                Logger.modelWCP.info("no material found for \(primId)")
                return nil
            }

            return materialId
        }
        var meshId: UUID? = nil
        if let existingId = primIdToMeshId[identifier], let existingMesh = meshes.first(where: { $0.id == existingId }) {
            if existingMesh.descriptor != resourceData.descriptor {
                // descriptors don't match. remove the mesh and make a new one.
                if case .created = existingMesh.state {
                    Logger.modelWCP.info("RemoveMeshRequest")
                }
                self.meshes.removeAll(where: { $0.id == existingId })
                primIdToMeshId.removeValue(forKey: identifier)
            } else {
                meshId = existingId
            }
        }

        if let meshId {
            Logger.modelWCP.info("update mesh")
            guard let mesh = meshes.first(where: { $0.id == meshId }) else {
                return
            }

            objcLoader.updateMesh(
                webRequest: Converter.webUpdateMeshRequestFromUpdateMeshRequest(
                    request: UpdateMeshRequest(
                        id: mesh.id,
                        partCount: resourceData.meshParts.count,
                        parts: resourceData.meshParts.enumerated().map({ index, part in .init(partIndex: index, part: part) }),
                        renderFlags: [],
                        vertices: resourceData.vertexData.enumerated().map({ index, data in .init(bufferIndex: index, buffer: data) }),
                        indices: resourceData.indexData,
                        instanceTransforms: instanceTransforms,
                        materialIds: materialIds
                    )
                )
            )
        } else {
            Logger.modelWCP.info("add mesh")
            let mesh = Mesh(id: UUID(), descriptor: resourceData.descriptor, state: .pending)
            meshes.append(mesh)
            primIdToMeshId[identifier] = mesh.id

            objcLoader.addMesh(
                webRequest: Converter.webAddMeshRequestFromAddMeshRequest(
                    request: AddMeshRequest(
                        id: mesh.id,
                        descriptor: resourceData.descriptor
                    )
                )
            ) { result in
                if result == 1 {
                    mesh.state = .created

                    Logger.modelWCP.info("update! mesh")
                    self.objcLoader.updateMesh(
                        webRequest: Converter.webUpdateMeshRequestFromUpdateMeshRequest(
                            request: UpdateMeshRequest(
                                id: mesh.id,
                                partCount: resourceData.meshParts.count,
                                parts: resourceData.meshParts.enumerated().map({ index, part in .init(partIndex: index, part: part) }),
                                renderFlags: [],
                                vertices: resourceData.vertexData.enumerated()
                                    .map({
                                        index,
                                        data in .init(bufferIndex: index, buffer: data)
                                    }
                                    ),
                                indices: resourceData.indexData,
                                instanceTransforms: instanceTransforms,
                                materialIds: materialIds
                            )
                        )
                    )
                }
                if result == 0 {
                    Logger.modelWCP.info("Failed to add mesh \(identifier): \(result)")
                    self.meshes.removeAll(where: { $0.id == mesh.id })
                    self.primIdToMeshId.removeValue(forKey: identifier)
                }
            }
        }
    }

    nonisolated func meshDestroyed(identifier: String) {
        Logger.modelWCP.info("mesh destroyed: \(identifier)")
        guard let meshId = primIdToMeshId[identifier] else {
            return
        }

        if let mesh = meshes.first(where: { $0.id == meshId }) {
            if case .created = mesh.state {
                Logger.modelWCP.info("RemoveMeshRequest")
            }
        }

        meshes.removeAll(where: { $0.id == meshId })
        primIdToMeshId.removeValue(forKey: identifier)
    }

    nonisolated func materialCreated(identifier: String) {
        Logger.modelWCP.info("Material created: \(identifier)")
    }

    nonisolated func materialUpdated(identifier: String, data: DDMaterialData) {
        Logger.modelWCP.info("Material changed: \(identifier)")

        // Extract the data we need before entering the Task
        let materialGraph = data.materialGraph
        if let materialId = primIdToMaterialId[identifier] {
            guard let material = materials.first(where: { $0.id == materialId }) else {
                return
            }
            guard case .created = material.state else { return }

            Logger.modelWCP.info("UpdateMaterialRequest")
        } else {
            let material = Material(id: UUID(), state: .pending)
            materials.append(material)
            primIdToMaterialId[identifier] = material.id

            Logger.modelWCP.info("AddMaterialRequest")
        }
    }

    nonisolated func materialDestroyed(identifier: String) {
        Logger.modelWCP.info("Material destroyed: \(identifier)")
        guard let materialId = primIdToMaterialId[identifier] else {
            return
        }

        guard let material = materials.first(where: { $0.id == materialId }) else {
            return
        }
        if case .created = material.state {
            Logger.modelWCP.info("RemoveMaterialRequest")
        }

        materials.removeAll(where: { $0.id == materialId })
        primIdToMaterialId.removeValue(forKey: identifier)
    }

    nonisolated func textureCreated(identifier: String) {
    }

    nonisolated func textureUpdated(identifier: String, data: DDTextureData) {
        Logger.modelWCP.info("Texture Updated: \(identifier)")
        if let textureId = assetPathToTextureId[data.asset.path] {
            Logger.modelWCP.info("UpdateTextureRequest")
        } else {
            let textureId = UUID()
            Logger.modelWCP.info("AddTextureRequest: imageAsset")
            assetPathToTextureId[data.asset.path] = textureId
        }
    }

    nonisolated func textureDestroyed(identifier: String) {
        Logger.modelWCP.info("Texture destroyed: \(identifier)")
        // Assume identifier is aligned with DDImageAsset.path(SdfAsset.resolvedPath)
        if let textureId = assetPathToTextureId[identifier] {
            Logger.modelWCP.info("RemoveTextureRequest: \(textureId)")
        }
    }
}

@objc
@implementation
extension WebDDVertexAttributeFormat {
    let semantic: Int32
    let format: Int32
    let layoutIndex: Int32
    let offset: Int32

    init(
        semantic: Int32,
        format: Int32,
        layoutIndex: Int32,
        offset: Int32
    ) {
        self.semantic = semantic
        self.format = format
        self.layoutIndex = layoutIndex
        self.offset = offset
    }
}

@objc
@implementation
extension WebDDVertexLayout {
    let bufferIndex: Int32
    let bufferOffset: Int32
    let bufferStride: Int32

    init(
        bufferIndex: Int32,
        bufferOffset: Int32,
        bufferStride: Int32
    ) {
        self.bufferIndex = bufferIndex
        self.bufferOffset = bufferOffset
        self.bufferStride = bufferStride
    }
}

@objc
@implementation
extension WebAddMeshRequest {
    let indexCapacity: Int32
    let indexType: Int32
    let vertexBufferCount: Int32
    let vertexCapacity: Int32
    let vertexAttributes: [WebDDVertexAttributeFormat]
    let vertexLayouts: [WebDDVertexLayout]

    init(
        indexCapacity: Int32,
        indexType: Int32,
        vertexBufferCount: Int32,
        vertexCapacity: Int32,
        vertexAttributes: [WebDDVertexAttributeFormat],
        vertexLayouts: [WebDDVertexLayout]
    ) {
        self.indexCapacity = indexCapacity
        self.indexType = indexType
        self.vertexBufferCount = vertexBufferCount
        self.vertexCapacity = vertexCapacity
        self.vertexAttributes = vertexAttributes
        self.vertexLayouts = vertexLayouts
    }
}

@objc
@implementation
extension WebDDMeshPart {
    let indexOffset: UInt
    let indexCount: UInt
    let topology: UInt
    let materialIndex: UInt
    let boundsMin: simd_float3
    let boundsMax: simd_float3

    init(
        indexOffset: UInt,
        indexCount: UInt,
        topology: UInt,
        materialIndex: UInt,
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
extension WebSetPart {
    let partIndex: Int
    let part: WebDDMeshPart

    init(index: Int, part: WebDDMeshPart) {
        self.partIndex = index
        self.part = part
    }
}

@objc
@implementation
extension WebSetRenderFlags {
    let partIndex: Int
    let renderFlags: UInt64

    init(index: Int, renderFlags: UInt64) {
        self.partIndex = index
        self.renderFlags = renderFlags
    }
}

@objc
@implementation
extension WebReplaceVertices {
    let bufferIndex: Int
    let buffer: Data

    init(bufferIndex: Int, buffer: Data) {
        self.bufferIndex = bufferIndex
        self.buffer = buffer
    }
}

@objc
@implementation
extension WebChainedFloat4x4 {
    let transform: simd_float4x4
    var next: WebChainedFloat4x4?

    init(
        transform: simd_float4x4
    ) {
        self.transform = transform
    }
}

extension Logger {
    fileprivate static let modelWCP = Logger(subsystem: "com.apple.WebKit", category: "model [WCP]")
}

@objc
@implementation
extension WebUpdateMeshRequest {
    let partCount: Int
    let parts: [WebSetPart]?
    let renderFlags: [WebSetRenderFlags]?
    let vertices: [WebReplaceVertices]?
    let indices: Data?
    let transform: simd_float4x4
    var instanceTransforms: WebChainedFloat4x4? // array of float4x4
    let materialIds: [UUID]?

    init(
        partCount: Int,
        parts: [WebSetPart]?,
        renderFlags: [WebSetRenderFlags]?,
        vertices: [WebReplaceVertices]?,
        indices: Data?,
        transform: simd_float4x4,
        instanceTransforms: WebChainedFloat4x4?,
        materialIds: [UUID]?
    ) {
        self.partCount = partCount
        self.parts = parts
        self.renderFlags = renderFlags
        self.vertices = vertices
        self.indices = indices
        self.transform = transform
        self.instanceTransforms = instanceTransforms
        self.materialIds = materialIds
    }
}

final class Converter {
    static func toWebSetParts(parts: [UpdateMeshRequest.SetPart]?) -> [WebSetPart]? {
        var results: [WebSetPart] = []
        if let p = parts {
            for part in p {
                let p = part.part
                let newMeshPart = WebDDMeshPart(
                    indexOffset: p.indexOffset,
                    indexCount: p.indexCount,
                    topology: p.topology.rawValue,
                    materialIndex: p.materialIndex,
                    boundsMin: p.boundsMin,
                    boundsMax: p.boundsMax
                )
                let newPart = WebSetPart(index: part.partIndex, part: newMeshPart)
                results.append(newPart)
            }
        }
        return results
    }

    static func toWebRenderFlags(flags: [UpdateMeshRequest.SetRenderFlags]?) -> [WebSetRenderFlags]? {
        var results: [WebSetRenderFlags] = []
        guard let flags else {
            return results
        }

        for flag in flags {
            let newFlag = WebSetRenderFlags(index: flag.partIndex, renderFlags: flag.renderFlags)
            results.append(newFlag)
        }

        return results
    }

    static func toWebReplaceVertices(verts: [UpdateMeshRequest.ReplaceVertices]?) -> [WebReplaceVertices]? {
        var results: [WebReplaceVertices] = []
        guard let verts else {
            return results
        }

        for vert in verts {
            let newVert = WebReplaceVertices(bufferIndex: vert.bufferIndex, buffer: vert.buffer)
            results.append(newVert)
        }

        return results
    }

    static func toWebVertexAttributes(attrs: [DDMeshDescriptor.VertexAttributeFormat]?) -> [WebDDVertexAttributeFormat] {
        var results: [WebDDVertexAttributeFormat] = []
        guard let attrs else {
            return results
        }

        for attr in attrs {
            let format = WebDDVertexAttributeFormat(
                semantic: attr.semantic.rawValue,
                format: Int32(attr.format.rawValue),
                layoutIndex: Int32(attr.layoutIndex),
                offset: Int32(attr.offset)
            )
            results.append(format)
        }

        return results
    }

    static func toWebVertexLayouts(layouts: [DDMeshDescriptor.VertexLayout]?) -> [WebDDVertexLayout] {
        var results: [WebDDVertexLayout] = []
        guard let layouts else {
            return results
        }

        for l in layouts {
            let layout = WebDDVertexLayout(
                bufferIndex: Int32(l.bufferIndex),
                bufferOffset: Int32(l.bufferOffset),
                bufferStride: Int32(l.bufferStride)
            )
            results.append(layout)
        }

        return results
    }

    static func webUpdateMeshRequestFromUpdateMeshRequest(request: UpdateMeshRequest) -> WebUpdateMeshRequest {
        var webRequestInstanceTransforms: WebChainedFloat4x4?
        if request.instanceTransforms.count > 0 {
            let countMinusOne = request.instanceTransforms.count - 1
            webRequestInstanceTransforms = WebChainedFloat4x4(transform: request.instanceTransforms[0])
            var instanceTransforms = webRequestInstanceTransforms
            if countMinusOne > 0 {
                for i in 1...countMinusOne {
                    instanceTransforms?.next = WebChainedFloat4x4(transform: request.instanceTransforms[i])
                    instanceTransforms = instanceTransforms?.next
                }
            }
        }

        let webRequest = WebUpdateMeshRequest(
            partCount: request.partCount ?? 0,
            parts: Converter.toWebSetParts(parts: request.parts),
            renderFlags: Converter.toWebRenderFlags(flags: request.renderFlags),
            vertices: Converter.toWebReplaceVertices(verts: request.vertices),
            indices: request.indices,
            transform: request.transform ?? simd_float4x4(),
            instanceTransforms: webRequestInstanceTransforms,
            materialIds: request.materialIds
        )

        return webRequest
    }

    static func webAddMeshRequestFromAddMeshRequest(request: AddMeshRequest) -> WebAddMeshRequest {
        WebAddMeshRequest(
            indexCapacity: Int32(request.descriptor.indexCapacity),
            indexType: Int32(request.descriptor.indexType.rawValue),
            vertexBufferCount: Int32(request.descriptor.vertexBufferCount),
            vertexCapacity: Int32(request.descriptor.vertexCapacity),
            vertexAttributes: toWebVertexAttributes(attrs: request.descriptor.vertexAttributes),
            vertexLayouts: toWebVertexLayouts(layouts: request.descriptor.vertexLayouts)
        )
    }
}

@objc
@implementation
extension WebUSDModelLoader {
    @nonobjc
    var renderer: USDModelLoader?
    @nonobjc
    var modelAdded: ((WebAddMeshRequest) -> (Void))?
    @nonobjc
    var modelUpdated: ((WebUpdateMeshRequest) -> (Void))?
    @nonobjc
    var modelAddedCallback: ((Int32) -> (Void))?

    override init() {
        super.init()

        self.renderer = USDModelLoader(objcInstance: self)
    }

    @objc(setCallbacksWithModelAddedCallback:modelUpdatedCallback:)
    func setCallbacksWithModelAddedCallback(
        _ modelAddedCallback: @escaping ((WebAddMeshRequest) -> (Void)),
        modelUpdatedCallback: @escaping ((WebUpdateMeshRequest) -> (Void))
    ) {
        self.modelAdded = modelAddedCallback
        self.modelUpdated = modelUpdatedCallback
    }

    @objc
    func loadModel(from url: URL) {
        self.renderer?.loadModel(from: url)
    }

    fileprivate func updateMesh(webRequest: WebUpdateMeshRequest) {
        Logger.modelWCP.info("update! modelUpdated")
        if let modelUpdated {
            modelUpdated(webRequest)
        }
    }

    fileprivate func addMesh(
        webRequest: WebAddMeshRequest,
        callback: @escaping (Int32) -> Void = { result in
            if result == 0 {
                Logger.modelWCP.info("Failure: \(result)")
            }
        }
    ) {
        modelAddedCallback = callback

        if let modelAdded {
            modelAdded(webRequest)
        }

        callback(1)
    }

    fileprivate func meshAdded(result: Int32) {
        if let modelAddedCallback {
            modelAddedCallback(result)
        }
    }

    fileprivate func updateHostedRenderer(id: UUID, size: CGSize, contentsScale: CGFloat) {
        Logger.modelWCP.info("update size scale id")
        return
    }
}

#endif
