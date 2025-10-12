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

import Foundation
import Metal
import QuartzCore
import WebGPU_Internal
#if canImport(DirectDrawBackend)
import DirectDrawBackend
import DirectResource
import ShaderGraph
#endif

@objc
@implementation
extension DDBridgeVertexAttributeFormat {
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
extension DDBridgeVertexLayout {
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
extension DDBridgeAddMeshRequest {
    let indexCapacity: Int32
    let indexType: Int32
    let vertexBufferCount: Int32
    let vertexCapacity: Int32
    let vertexAttributes: [DDBridgeVertexAttributeFormat]?
    let vertexLayouts: [DDBridgeVertexLayout]?

    init(
        indexCapacity: Int32,
        indexType: Int32,
        vertexBufferCount: Int32,
        vertexCapacity: Int32,
        vertexAttributes: [DDBridgeVertexAttributeFormat]?,
        vertexLayouts: [DDBridgeVertexLayout]?
    ) {
        self.indexCapacity = indexCapacity
        self.indexType = indexType
        self.vertexBufferCount = vertexBufferCount
        self.vertexCapacity = vertexCapacity
        self.vertexAttributes = vertexAttributes
        self.vertexLayouts = vertexLayouts
    }

    #if canImport(DirectDrawBackend)
    fileprivate var reDescriptor: DirectResource.MeshDescriptor {
        let descriptor = DirectResource.MeshDescriptor()

        descriptor.indexCapacity = DRInteger(indexCapacity)
        descriptor.indexType = MTLIndexType(rawValue: UInt(indexType)) ?? .uint16
        descriptor.vertexBufferCount = DRInteger(vertexBufferCount)
        descriptor.vertexCapacity = DRInteger(vertexCapacity)
        if let vertexAttributes {
            descriptor.vertexAttributeCount = vertexAttributes.count
            for (index, attribute) in vertexAttributes.enumerated() {
                descriptor.setVertexAttribute(
                    index: UInt(index),
                    semantic: DRVertexSemantic(rawValue: attribute.semantic) ?? .invalid,
                    format: MTLVertexFormat(rawValue: UInt(attribute.format)) ?? .invalid,
                    layoutIndex: DRUInteger(attribute.layoutIndex),
                    offset: DRUInteger(attribute.offset)
                )
            }
        }
        if let vertexLayouts {
            descriptor.vertexLayoutCount = vertexLayouts.count
            for (index, layout) in vertexLayouts.enumerated() {
                descriptor.setVertexLayout(
                    layoutIndex: UInt(index),
                    bufferIndex: DRUInteger(layout.bufferIndex), // RE ensures this is within bounds
                    bufferOffset: DRUInteger(layout.bufferOffset),
                    bufferStride: DRUInteger(layout.bufferStride)
                )
            }
        }
        return descriptor
    }
    #endif
}

@objc
@implementation
extension DDBridgeMeshPart {
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
extension DDBridgeSetPart {
    let partIndex: Int
    let part: DDBridgeMeshPart

    init(
        index: Int,
        part: DDBridgeMeshPart
    ) {
        self.partIndex = index
        self.part = part
    }
}

@objc
@implementation
extension DDBridgeSetRenderFlags {
    let partIndex: Int
    let renderFlags: UInt64

    init(
        index: Int,
        renderFlags: UInt64
    ) {
        self.partIndex = index
        self.renderFlags = renderFlags
    }
}

@objc
@implementation
extension DDBridgeReplaceVertices {
    let bufferIndex: Int
    let buffer: Data

    init(
        bufferIndex: Int,
        buffer: Data
    ) {
        self.bufferIndex = bufferIndex
        self.buffer = buffer
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
    let partCount: Int
    let parts: [DDBridgeSetPart]?
    let renderFlags: [DDBridgeSetRenderFlags]?
    let vertices: [DDBridgeReplaceVertices]?
    let indices: Data?
    let transform: simd_float4x4
    var instanceTransforms: DDBridgeChainedFloat4x4? // array of float4x4
    let materialIds: [UUID]?

    init(
        partCount: Int,
        parts: [DDBridgeSetPart]?,
        renderFlags: [DDBridgeSetRenderFlags]?,
        vertices: [DDBridgeReplaceVertices]?,
        indices: Data?,
        transform: simd_float4x4,
        instanceTransforms: DDBridgeChainedFloat4x4?,
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
}

extension Logger {
    fileprivate static let modelGPU = Logger(subsystem: "com.apple.WebKit", category: "model [GPU]")
}

@objc
@implementation
extension DDBridgeReceiver {
    fileprivate var device: MTLDevice?

    #if canImport(DirectDrawBackend)
    @nonobjc
    fileprivate var context: DirectDrawBackend.Context?
    @nonobjc
    fileprivate var scene: DirectDrawBackend.Scene?
    @nonobjc
    fileprivate var meshes: [UUID: DirectDrawBackend.MeshInstance] = [:]
    @nonobjc
    fileprivate var materials: [UUID: DirectDrawBackend.MaterialInstance] = [:]

    @nonobjc
    fileprivate var camera: Camera?
    #endif
    @nonobjc
    fileprivate var commandQueue: MTLCommandQueue?
    @nonobjc
    fileprivate var captureManager: MTLCaptureManager?

    @objc
    override init() {
        super.init()
    }

    @objc(setDeviceWithDevice:)
    func setDeviceWith(_ device: MTLDevice) {
        self.device = device

        #if canImport(DirectDrawBackend)
        context = DirectDrawBackend.Context.createContext(device: device)
        scene = context?.makeScene()
        commandQueue = device.makeCommandQueue()
        captureManager = MTLCaptureManager.shared()
        camera = context?.makeCamera()
        context?.setCameraDistance(171.0)
        context?.setEnableModelRotation(true)
        #endif
    }

    @objc(renderWithTexture:)
    func render(with texture: MTLTexture) {
        #if canImport(DirectDrawBackend)
        guard let scene, let camera, let context, let captureManager else {
            return
        }

        let captureDescriptor = MTLCaptureDescriptor(from: device)
        do {
            try captureManager.startCapture(with: captureDescriptor)
        } catch {
            Logger.modelGPU.error("failed to start gpu capture \(error)")
        }

        context.update(deltaTime: 1.0 / 60.0)
        camera.renderTarget = .texture(texture)

        context.render(scene: scene, camera: camera)

        captureManager.stopCapture()

        #endif
    }

    @objc(addMesh:identifier:)
    func addMesh(_ descriptor: DDBridgeAddMeshRequest, identifier: UUID) -> Bool {
        #if canImport(DirectDrawBackend)
        let meshDescriptor = descriptor.reDescriptor
        guard let context else {
            return false
        }

        do {
            let mesh = try context.makeMeshResource(descriptor: meshDescriptor)
            let meshInstance = context.makeMeshInstance(
                resource: mesh,
                materials: [],
                transform: matrix_identity_float4x4,
                instanceTransforms: []
            )
            meshes[identifier] = meshInstance
            guard let scene else {
                return false
            }
            scene.meshes.append(meshInstance)
            return true
        } catch {
            Logger.modelGPU.error("unexpected error in adddMesh \(error)")
            return false
        }
        #else
        return false
        #endif
    }

    @objc(updateMesh:identifier:)
    func updateMesh(_ request: DDBridgeUpdateMesh, identifier: UUID) {
        #if canImport(DirectDrawBackend)
        guard let mesh = meshes[identifier] else {
            Logger.modelGPU.error("invalid mesh id")
            return
        }

        let resource = mesh.resource
        guard let context else {
            return
        }
        do {
            if let materialIds = request.materialIds {
                try mesh.materials = materialIds.compactMap { materialId in
                    materials[materialId]
                }
            }

            try context.resourceSync {
                let partCount = request.partCount
                if partCount > 0 {
                    resource.resource.partCount = partCount
                }
                if let parts = request.parts {
                    for setPart in parts {
                        resource.resource.setPart(
                            index: setPart.partIndex,
                            offset: setPart.part.indexOffset,
                            count: setPart.part.indexCount,
                            topology: MTLPrimitiveType(rawValue: setPart.part.topology) ?? .triangle,
                            material: setPart.part.materialIndex,
                            boundsMin: setPart.part.boundsMin,
                            boundsMax: setPart.part.boundsMax
                        )
                    }
                }
                if let renderFlags = request.renderFlags {
                    for setRenderFlags in renderFlags {
                        resource.resource.setPart(
                            index: setRenderFlags.partIndex,
                            renderFlags: setRenderFlags.renderFlags
                        )
                    }
                }
                if let vertices = request.vertices {
                    for replaceVertices in vertices {
                        // RE performs bounds checking
                        unsafe resource.resource.replaceVertices(index: replaceVertices.bufferIndex) { destination, destinationSize in
                            guard destinationSize > 0 else {
                                return
                            }
                            let sourceSize = replaceVertices.buffer.count
                            guard sourceSize <= destinationSize else {
                                Logger.modelGPU.error(
                                    "Mismatched vertex buffer size, expected \(destinationSize) bytes but got \(sourceSize) bytes"
                                )
                                return
                            }
                            unsafe replaceVertices.buffer.withUnsafeBytes { sourceBuffer in
                                guard let baseAddress = sourceBuffer.baseAddress else {
                                    return
                                }
                                unsafe destination.copyMemory(from: baseAddress, byteCount: sourceSize)
                            }
                        }
                    }
                }
                if let replaceIndices = request.indices {
                    unsafe resource.resource.replaceIndices { destination, destinationSize in
                        guard destinationSize > 0 else {
                            return
                        }
                        let sourceSize = replaceIndices.count
                        guard sourceSize <= destinationSize else {
                            Logger.modelGPU.error(
                                "Mismatched index buffer size, expected \(destinationSize) bytes but got \(sourceSize) bytes"
                            )
                            return
                        }
                        unsafe replaceIndices.withUnsafeBytes { sourceBuffer in
                            guard let baseAddress = sourceBuffer.baseAddress else {
                                return
                            }
                            unsafe destination.copyMemory(from: baseAddress, byteCount: sourceSize)
                        }
                    }
                }
            }
            if Helper.isNonZero(matrix: request.transform) {
                mesh.transform = request.transform
            }
            var instTransforms: [simd_float4x4] = []
            if let chainedTransform = request.instanceTransforms {
                instTransforms.append(chainedTransform.transform)
                while let chainedTransform = chainedTransform.next {
                    instTransforms.append(chainedTransform.transform)
                }
            }
            if !instTransforms.isEmpty {
                mesh.instanceTransforms = instTransforms
            }
        } catch {
            Logger.modelGPU.info("unexpected error in updateMesh \(error)")
        }
        #endif
    }
}
