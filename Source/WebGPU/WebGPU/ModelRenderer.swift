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

#if canImport(RealityCoreRenderer, _version: 6) && canImport(USDStageKit, _version: 34)

import QuartzCore
@_spi(RealityCoreRendererAPI) @_spi(Private) import RealityKit
internal import simd

extension RealityCoreRenderer._Proto_LowLevelGlobalConstantsEncoder_v1 {
    @_silgen_name(
        "$s19RealityCoreRenderer40_Proto_LowLevelGlobalConstantsEncoder_v1C6encode2to10byteOffsetys14MutableRawSpanVz_SitAA01_d1_efc6Error_J0VYKF"
    )
    fileprivate func encode(to mutableSpan: inout MutableRawSpan, byteOffset: Int) throws(_Proto_LowLevelRendererError_v1)
}

internal struct CameraTransform {
    var rotation: simd_quatf
    var translation: simd_float3
    var scale: simd_float3
}

nonisolated class Renderer {
    let device: MTLDevice
    let commandQueue: MTLCommandQueue
    var renderContext: _Proto_LowLevelRenderContext_v1?
    var globalConstants: _Proto_LowLevelBufferSpan_v1?
    var globalConstantsEncoder: _Proto_LowLevelGlobalConstantsEncoder_v1?
    var renderWorkload: _Proto_LowLevelCameraRenderWorkload_v1?
    var cameraPose: _Proto_Pose_v1
    var modelDistance: Float = 1.0

    init(device: MTLDevice) throws {
        guard let commandQueue = device.makeCommandQueue() else {
            fatalError("Failed to create command queue.")
        }
        commandQueue.label = "LowLevelRenderer Command Queue"

        self.device = device
        self.commandQueue = commandQueue
        self.cameraPose = .init(translation: [0, 0, 1], rotation: .init(ix: 0, iy: 0, iz: 0, r: 1))

        prevTime = CACurrentMediaTime()
    }

    func createMaterialCompiler(renderTargetDescriptor: _Proto_LowLevelRenderTarget_v1.Descriptor) async throws {
        var configuration = _Proto_LowLevelRenderContextStandaloneConfiguration_v1(device: device)
        configuration.residencySetBehavior = _Proto_LowLevelRenderContextStandaloneConfiguration_v1.ResidencySetBehavior.disable
        let renderContext = try await _Proto_makeLowLevelRenderContextStandalone_v1(configuration: configuration)

        let globalConstantsSize = (_Proto_LowLevelGlobalConstantsEncoder_v1.encodedLength + 15) / 16 * 16
        let globalConstantsBuffer = try renderContext.makeBufferResource(descriptor: .init(capacity: globalConstantsSize, sizeMultiple: 1))
        let globalConstants = try _Proto_LowLevelBufferSpan_v1(buffer: globalConstantsBuffer, offset: 0, size: globalConstantsSize)

        let meshInstances = try renderContext.makeMeshInstanceArray(renderTargets: [renderTargetDescriptor], count: 0)

        let renderWorkload = try _Proto_LowLevelCameraRenderWorkload_v1(
            renderContext: renderContext,
            renderTargetDescriptor: renderTargetDescriptor,
            camera: .mono(
                pose: .init(
                    translation: .zero,
                    rotation: .init(ix: 0, iy: 0, iz: 0, r: 1)
                ),
                projection: .perspective(
                    fovYRadians: 90 * .pi / 180,
                    aspectRatio: 1,
                    nearZ: 0.01,
                    farZ: 1
                )
            ),
            meshInstances: meshInstances,
            globalConstants: globalConstants
        )
        self.globalConstantsEncoder = renderWorkload.makeGlobalConstantsEncoder()
        self.renderContext = renderContext
        self.renderWorkload = renderWorkload
        self.globalConstants = globalConstants
    }

    var prevTime: CFTimeInterval = 0

    func render(
        meshInstances: _Proto_LowLevelMeshInstanceArray_v1,
        texture: MTLTexture
    ) throws {
        guard let renderWorkload else {
            return
        }

        let time = CACurrentMediaTime()
        try updateGlobalConstants(time: Float(time))

        let aspect = Float(texture.width) / Float(texture.height)
        let projection = _Proto_LowLevelCamera_v1.Projection.perspective(
            fovYRadians: 90 * .pi / 180,
            aspectRatio: aspect,
            nearZ: modelDistance * 0.01,
            farZ: modelDistance * 100,
            reverseZ: true
        )

        let target: _Proto_LowLevelRenderTarget_v1 = .texture(color: texture, sampleCount: 4)
        renderWorkload.camera = .mono(pose: cameraPose, projection: projection)
        renderWorkload.camera.clearColor = .init(red: 0.0, green: 0.0, blue: 0.0, alpha: 0.0)
        try renderWorkload.setMeshInstances(meshInstances)

        guard let renderCommandBuffer = commandQueue.makeCommandBuffer() else {
            fatalError("Failed to make command buffer")
        }
        renderCommandBuffer.label = "Render Camera"
        try renderWorkload.render(for: renderCommandBuffer, target: target)
        renderCommandBuffer.commit()

        prevTime = time
    }

    private func updateGlobalConstants(time: Float) throws(_Proto_LowLevelRendererError_v1) {
        guard let globalConstantsEncoder else {
            fatalError("globalConstantsEncoder is nil")
        }
        guard let globalConstants else {
            fatalError("globalConstants is nil")
        }
        globalConstantsEncoder.setTime(time)
        try globalConstants.buffer.replace { mutableSpan throws(_Proto_LowLevelRendererError_v1) in
            try globalConstantsEncoder.encode(to: &mutableSpan, byteOffset: globalConstants.offset)
        }
    }

    internal func setCameraDistance(_ distance: Float) {
        modelDistance = distance
        cameraPose = .init(
            translation: [0, 0, distance],
            rotation: simd_quatf(angle: 0, axis: [0, 0, 1]),
        )
    }

    internal func setCameraTransform(_ transform: CameraTransform) {
        cameraPose = .init(
            translation: transform.translation,
            rotation: transform.rotation,
        )
    }
}

#endif
