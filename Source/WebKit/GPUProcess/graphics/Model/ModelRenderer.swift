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

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreRenderer, _version: 11) && compiler(>=6.2)

internal import QuartzCore
@_weakLinked internal import USDKit
@_weakLinked @_spi(UsdLoaderAPI) internal import _USDKit_RealityKit
@_spi(RealityCoreRendererAPI) @_spi(Private) import RealityKit
import simd

internal struct CameraTransform {
    var rotation: simd_quatf
    var translation: simd_float3
    var scale: simd_float3
}

nonisolated class Renderer {
    let device: MTLDevice
    let commandQueue: MTLCommandQueue
    var renderContext: _Proto_LowLevelRenderContext_v1?
    var renderer: _Proto_LowLevelRenderer_v1?
    var renderTargetDescriptor: _Proto_LowLevelRenderTarget_v1.Descriptor {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        renderer!.renderTargetDescriptor
    }
    var pose: _Proto_Pose_v1
    var modelDistance: Float = 1.0
    let memoryOwner: task_id_token_t

    init(device: MTLDevice, memoryOwner: task_id_token_t) throws {
        guard let commandQueue = device.makeCommandQueue() else {
            fatalError("Failed to create command queue.")
        }
        commandQueue.label = "LowLevelRenderer Command Queue"

        self.device = device
        self.commandQueue = commandQueue
        self.pose = .init(translation: [0, 0, 1], rotation: .init(ix: 0, iy: 0, iz: 0, r: 1))
        self.memoryOwner = memoryOwner
    }

    func createMaterialCompiler(colorPixelFormat: MTLPixelFormat, rasterSampleCount: Int, colorSpace: CGColorSpace? = nil) async throws {
        #if canImport(RealityCoreRenderer, _version: 9999)
        var configuration = _Proto_LowLevelRenderContextStandaloneConfiguration_v1(device: device)
        configuration.memoryOwner = self.memoryOwner
        #else
        var configuration = _Proto_LowLevelRenderContextStandaloneConfiguration_v1(device: device)
        #endif
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

    func render(
        meshInstances: _Proto_LowLevelMeshInstanceArray_v1,
        texture: MTLTexture
    ) throws {
        guard let renderer else {
            return
        }

        let time = CACurrentMediaTime()

        let aspect = Float(texture.width) / Float(texture.height)
        let projection = _Proto_LowLevelRenderer_v1.Camera.Projection.perspective(
            fovYRadians: 90 * .pi / 180,
            aspectRatio: aspect,
            nearZ: modelDistance * 0.01,
            farZ: modelDistance * 100,
            reverseZ: true
        )

        renderer.cameras[0].pose = pose
        renderer.cameras[0].projection = projection
        renderer.output.clearColor = .init(red: 0.0, green: 0.0, blue: 0.0, alpha: 0.0)
        renderer.output.color = .init(texture: texture)
        renderer.meshInstances = meshInstances

        guard let renderCommandBuffer = commandQueue.makeCommandBuffer() else {
            fatalError("Failed to make command buffer")
        }
        renderCommandBuffer.label = "Render Camera"
        try renderer.render(for: renderCommandBuffer)
        renderCommandBuffer.commit()
    }

    internal func setCameraDistance(_ distance: Float) {
        modelDistance = distance
        pose = .init(
            translation: [0, 0, distance],
            rotation: simd_quatf(angle: 0, axis: [0, 0, 1]),
        )
    }

    internal func setCameraTransform(_ transform: CameraTransform) {
        pose = .init(
            translation: transform.translation,
            rotation: transform.rotation,
        )
    }
}

#endif
