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

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreTextureProcessing, _version: 19) && canImport(_USDKit_RealityKit, _version: 42)

import QuartzCore
import USDKit
@_spi(UsdLoaderAPI) import _USDKit_RealityKit
@_spi(RealityCoreRendererAPI) @_spi(Private) import RealityKit
import simd

class Renderer {
    let device: any MTLDevice
    let commandQueue: any MTLCommandQueue
    var renderContext: (any _Proto_LowLevelRenderContext_v1)?
    var renderer: _Proto_LowLevelRenderer_v1?
    var renderTargetDescriptor: _Proto_LowLevelRenderTarget_v1.Descriptor {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        renderer!.renderTargetDescriptor
    }

    var pose: _Proto_Pose_v1
    private static let cameraDistance: Float = 0.5
    var fovY: Float = 60 * .pi / 180
    var clearColor: MTLClearColor = .init(red: 1, green: 1, blue: 1, alpha: 1)
    let memoryOwner: task_id_token_t

    init(device: any MTLDevice, memoryOwner: task_id_token_t) throws {
        guard let commandQueue = device.makeCommandQueue() else {
            fatalError("Failed to create command queue.")
        }
        commandQueue.label = "LowLevelRenderer Command Queue"

        self.device = device
        self.commandQueue = commandQueue
        self.pose = .init(
            translation: [0, 0, Self.cameraDistance],
            rotation: .init(ix: 0, iy: 0, iz: 0, r: 1)
        )
        self.memoryOwner = memoryOwner
    }

    func createMaterialCompiler(colorPixelFormat: MTLPixelFormat, rasterSampleCount: Int, colorSpace: CGColorSpace? = nil) async throws {
        var configuration = _Proto_LowLevelRenderContextStandaloneConfiguration_v1(device: device)
        configuration.memoryOwner = self.memoryOwner
        configuration.residencySetBehavior = _Proto_LowLevelRenderContextStandaloneConfiguration_v1.ResidencySetBehavior.disable
        let renderContext = try await _Proto_makeLowLevelRenderContextStandalone_v1(configuration: configuration)

        let renderer = try await _Proto_LowLevelRenderer_v1(
            configuration: .init(
                output: .init(colorPixelFormat: colorPixelFormat),
                rasterSampleCount: rasterSampleCount,
                enableTonemap: true,
                enableColorMatch: colorSpace != nil,
                hasTransparentContent: false
            ),
            renderContext: renderContext
        )
        self.renderContext = renderContext
        self.renderer = renderer
    }

    func newCommandBuffer() -> any MTLCommandBuffer {
        guard let renderCommandBuffer = commandQueue.makeCommandBuffer() else {
            fatalError("Failed to make command buffer")
        }

        return renderCommandBuffer
    }

    func render(
        meshInstances: _Proto_LowLevelMeshInstanceArray_v1,
        texture: any MTLTexture,
        commandBuffer: any MTLCommandBuffer
    ) throws {
        guard let renderer else {
            commandBuffer.commit()
            return
        }

        let time = CACurrentMediaTime()

        let aspect = Float(texture.width) / Float(texture.height)
        let projection = _Proto_LowLevelRenderer_v1.Camera.Projection.perspective(
            fovYRadians: fovY,
            aspectRatio: aspect,
            nearZ: Self.cameraDistance * 0.01,
            farZ: Self.cameraDistance * 100,
            reverseZ: true
        )

        renderer.cameras[0].pose = pose
        renderer.cameras[0].projection = projection
        renderer.output.clearColor = clearColor
        renderer.output.color = .init(texture: texture)
        renderer.meshInstances = meshInstances

        commandBuffer.label = "Render Camera"
        try renderer.render(for: commandBuffer)
        commandBuffer.commit()
    }

    func setFOV(_ fovYRadians: Float) {
        fovY = fovYRadians
    }

    func setBackgroundColor(_ color: simd_float3) {
        clearColor = MTLClearColor(red: 0.0, green: 0.0, blue: 0.0, alpha: 0.0)
    }

    func setCameraTransformForModelTransform(_ modelTransform: simd_float4x4) {
        // To keep the model stationary while achieving the same visual result as applying
        // modelTransform to the model, derive the equivalent camera pose by composing the
        // inverse model transform with the default camera matrix.
        var defaultCameraMatrix = matrix_identity_float4x4
        defaultCameraMatrix.columns.3 = [0, 0, Self.cameraDistance, 1]
        let cameraMatrix = simd_inverse(modelTransform) * defaultCameraMatrix

        let col0 = simd_float3(cameraMatrix.columns.0.x, cameraMatrix.columns.0.y, cameraMatrix.columns.0.z)
        let col1 = simd_float3(cameraMatrix.columns.1.x, cameraMatrix.columns.1.y, cameraMatrix.columns.1.z)
        let col2 = simd_float3(cameraMatrix.columns.2.x, cameraMatrix.columns.2.y, cameraMatrix.columns.2.z)
        let scale = simd_float3(simd_length(col0), simd_length(col1), simd_length(col2))
        let rotation = simd_quatf(simd_float3x3(col0 / scale.x, col1 / scale.y, col2 / scale.z))
        let translation = simd_float3(cameraMatrix.columns.3.x, cameraMatrix.columns.3.y, cameraMatrix.columns.3.z)
        pose = .init(translation: translation, rotation: rotation)
    }
}

#endif
