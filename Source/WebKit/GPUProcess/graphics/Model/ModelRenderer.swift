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

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreDeformation, _version: 23.0.2) && canImport(ShaderGraph, _version: 159.0.3) && arch(arm64)

import QuartzCore
import USDKit
import RealityKit
import simd

final class Renderer {
    private let device: any MTLDevice
    let commandQueue: any MTLCommandQueue
    var renderContext: (any LowLevelRenderContext)?
    var renderer: LowLevelRenderer?
    var pendingStandaloneResources: LowLevelRenderContextStandalone.Resources?
    var pendingRendererResources: LowLevelRenderer.Resources?

    private var cameraPosition: SIMD3<Float>
    private var cameraRotation: simd_quatf
    private static let cameraDistance: Float = 0.5
    private var effectiveCameraDistance: Float = Renderer.cameraDistance
    private var fovY: Float = 60 * .pi / 180
    var tonemapEnabled: Bool = false
    var rasterSampleCount: Int = 1
    let memoryOwner: task_id_token_t

    private struct ColorAdjustTilePipelineKey: Hashable {
        let pixelFormat: MTLPixelFormat
        let rasterSampleCount: Int
    }

    private var colorAdjustTilePipelineStates: [ColorAdjustTilePipelineKey: any MTLRenderPipelineState] = [:]

    private var portalContentConversionPipelineState: (any MTLComputePipelineState)?

    private lazy var portalQuadDepthStencilState: any MTLDepthStencilState = {
        let descriptor = MTLDepthStencilDescriptor()
        descriptor.label = "Portal Quad Depth Stencil"
        descriptor.depthCompareFunction = .greaterEqual
        descriptor.isDepthWriteEnabled = false
        guard let depthStencilState = device.makeDepthStencilState(descriptor: descriptor) else {
            fatalError("Failed to make the portal quad depth stencil state")
        }
        return depthStencilState
    }()

    private static let portalContentConversionShaderSource = """
        #include <metal_stdlib>
        using namespace metal;

        struct FilmicSegment {
            float offsX, offsY, scaleX, scaleY, lnA, B;
        };

        constant float kWhitePoint = 2.01402664;
        constant float kScaleY = 0.778939604;
        constant float kY0 = 0.0420799 * kScaleY;
        constant float kY1 = 0.3294559 * kScaleY;
        constant float kHDRGain = 1.5032214035;

        constant FilmicSegment kSegments[3] = {
            { 0.0,           0.0,        1.0,  kScaleY,  1.60599995,  1.33333337 },
            { 0.00696447864, 0.0,        1.0,  kScaleY,  0.700136005, 1.0        },
            { 3.0,           1.16840935, -1.0, -kScaleY, -4.90600586, 4.86833191 }
        };

        static float decodeSRGB(float v)
        {
            return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
        }

        static float encodeSRGB(float v)
        {
            return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
        }

        static float inverseFilmic(float y)
        {
            y = clamp(y, 0.0, 1.1683);
            int index = (y >= kY0 ? 1 : 0) + (y >= kY1 ? 1 : 0);
            FilmicSegment segment = kSegments[index];
            float normalized = (y - segment.offsY) / segment.scaleY;
            if (normalized <= 0.0)
                return 0.0;
            float x0 = exp((log(normalized) - segment.lnA) / segment.B);
            return (x0 / segment.scaleX + segment.offsX) * kWhitePoint;
        }

        struct Parameters {
            uint isHDR;
            uint isOpaque;
        };

        kernel void convertPortalContent(texture2d<float, access::read> source [[texture(0)]],
                                         texture2d<float, access::write> destination [[texture(1)]],
                                         constant Parameters &parameters [[buffer(0)]],
                                         uint2 position [[thread_position_in_grid]])
        {
            float4 pixel = source.read(position);
            float alpha = parameters.isOpaque ? 1.0 : pixel.a;
            if (alpha <= 0.0) {
                destination.write(float4(0.0), position);
                return;
            }

            float3 straight = saturate(pixel.rgb / alpha);
            float3 srgb = float3(decodeSRGB(straight.r), decodeSRGB(straight.g), decodeSRGB(straight.b));
            float3 target = float3(dot(srgb, float3(0.8224621, 0.1775380, 0.0)),
                                   dot(srgb, float3(0.0331941, 0.9668058, 0.0)),
                                   dot(srgb, float3(0.0170827, 0.0723974, 0.9105199)));
            float3 scene;
            if (parameters.isHDR) {
                float3 premultiplied = target * alpha / kHDRGain;
                scene = float3(inverseFilmic(premultiplied.r), inverseFilmic(premultiplied.g), inverseFilmic(premultiplied.b)) / alpha;
            } else {
                float3 premultiplied = alpha * float3(encodeSRGB(target.r), encodeSRGB(target.g), encodeSRGB(target.b));
                float3 linear = float3(decodeSRGB(premultiplied.r), decodeSRGB(premultiplied.g), decodeSRGB(premultiplied.b)) / (alpha * alpha);
                scene = float3(inverseFilmic(linear.r), inverseFilmic(linear.g), inverseFilmic(linear.b));
            }
            destination.write(float4(scene, alpha), position);
        }
        """

    private var portalContentConversionParameterBuffers: [UInt32: any MTLBuffer] = [:]

    private func portalContentConversionParameterBuffer(isOpaque: Bool) -> any MTLBuffer {
        let isHDR: UInt32 = tonemapEnabled ? 0 : 1
        let key = isHDR << 1 | (isOpaque ? 1 : 0)
        if let buffer = portalContentConversionParameterBuffers[key] {
            return buffer
        }
        let values: [UInt32] = [isHDR, isOpaque ? 1 : 0]
        let buffer = device.makeBuffer(
            fromSpan: values.span,
            length: MemoryLayout<UInt32>.stride * values.count,
            memoryOwner: memoryOwner
        )
        portalContentConversionParameterBuffers[key] = buffer
        return buffer
    }

    private func portalContentConversionPipeline() throws -> any MTLComputePipelineState {
        if let portalContentConversionPipelineState {
            return portalContentConversionPipelineState
        }
        let library = try device.makeLibrary(source: Self.portalContentConversionShaderSource, options: nil)
        guard let function = library.makeFunction(name: "convertPortalContent") else {
            fatalError("Failed to find convertPortalContent compute function")
        }
        let pipelineState = try device.makeComputePipelineState(function: function)
        portalContentConversionPipelineState = pipelineState
        return pipelineState
    }

    func convertPortalContent(
        source: any MTLTexture,
        destination: any MTLTexture,
        isOpaque: Bool,
        commandBuffer: any MTLCommandBuffer
    ) throws {
        let pipelineState = try portalContentConversionPipeline()
        guard let encoder = commandBuffer.makeComputeCommandEncoder() else {
            fatalError("Failed to make compute command encoder")
        }

        encoder.label = "Convert Portal Content"
        encoder.setComputePipelineState(pipelineState)
        encoder.setTexture(source, index: 0)
        encoder.setTexture(destination, index: 1)
        encoder.setBuffer(portalContentConversionParameterBuffer(isOpaque: isOpaque), offset: 0, index: 0)
        encoder.dispatchThreads(
            MTLSize(width: destination.width, height: destination.height, depth: 1),
            threadsPerThreadgroup: MTLSize(width: 8, height: 8, depth: 1)
        )
        encoder.endEncoding()
    }

    private static let colorAdjustTileShaderSource = """
        #include <metal_stdlib>
        using namespace metal;

        // Mirrors RealityCoreRenderer's TonemapParameters / kDefaultTonemapParams
        // from RealityCoreRenderer/Shaders/TonemapShared.h and Tonemap.h.
        struct TonemapSegment {
            float offsX, offsY, scaleX, scaleY, lnA, B;
        };

        struct TonemapParameters {
            float whitePoint;
            float oneOverWhitePoint;
            float x0, x1, y0, y1;
            float exposure;
            float pivotFactor;
            float contrast;
            float oneOverPivotFactor;
            float oneOverContrast;
            TonemapSegment segments[3];
            float oneOverExposure;
            float edrScalar;
        };

        constant TonemapParameters kDefaultTonemapParams = {
            .whitePoint = 2.01402664,
            .oneOverWhitePoint = 0.496517748,
            .x0 = 0.0278579127,
            .x1 = 0.17054522,
            .y0 = 0.0420799,
            .y1 = 0.3294559,
            .exposure = 1.0,
            .pivotFactor = 1.0,
            .contrast = 1.0,
            .oneOverPivotFactor = 1.0,
            .oneOverContrast = 1.0,
            .segments = {
                { 0.0,          0.0,        1.0,  0.778939604,  1.60599995,  1.33333337 },
                { 0.00696447864, 0.0,       1.0,  0.778939604,  0.700136005, 1.0        },
                { 3.0,          1.16840935, -1.0, -0.778939604, -4.90600586, 4.86833191 }
            },
            .oneOverExposure = 1.0,
            .edrScalar = 1.0,
        };

        // Mirrors computeFilmic() in RealityCoreRenderer/Shaders/Tonemap.metal.
        static half3 computeFilmic(half3 clr, constant TonemapParameters &params) {
            if (params.contrast != 1.0f) {
                clr = pow(clr, params.contrast) * params.pivotFactor;
            }

            half3 xs = clr * (half)params.oneOverWhitePoint;
            int3 indices = (int3)((half3)params.x0 < xs) + (int3)((half3)params.x1 < xs);

            half3 offsXs  = half3(params.segments[indices.r].offsX,  params.segments[indices.g].offsX,  params.segments[indices.b].offsX);
            half3 scaleXs = half3(params.segments[indices.r].scaleX, params.segments[indices.g].scaleX, params.segments[indices.b].scaleX);
            half3 lnAs    = half3(params.segments[indices.r].lnA,    params.segments[indices.g].lnA,    params.segments[indices.b].lnA);
            half3 Bs      = half3(params.segments[indices.r].B,      params.segments[indices.g].B,      params.segments[indices.b].B);
            half3 scaleYs = half3(params.segments[indices.r].scaleY, params.segments[indices.g].scaleY, params.segments[indices.b].scaleY);
            half3 offsYs  = half3(params.segments[indices.r].offsY,  params.segments[indices.g].offsY,  params.segments[indices.b].offsY);

            half3 x0s = (xs - offsXs) * scaleXs;
            bool3 mask = (x0s > 0);
            half3 y0s = select(half3(0, 0, 0), exp(lnAs + Bs * log(x0s)), mask);
            return y0s * scaleYs + offsYs;
        }

        // Mirrors tonemap() in RealityCoreRenderer/Shaders/Tonemap.metal.
        static half3 tonemap(half3 color, constant TonemapParameters &params) {
            color *= params.exposure;
            return computeFilmic(color, params) * params.edrScalar;
        }

        struct ImplicitFragmentInPlace {
            half4 color [[color(0)]];
        };

        // Mirrors resolveColorInPlace() in RealityCoreRenderer/Shaders/ColorResolve.metal.
        kernel void colorAdjustTile(imageblock<ImplicitFragmentInPlace, imageblock_layout_implicit> ib,
                                    ushort2 localThreadId [[thread_position_in_threadgroup]]) {
            half4 color = 0;
            ushort numColors = ib.get_num_colors(localThreadId);
            for (ushort c = 0; c < numColors; ++c) {
                ImplicitFragmentInPlace f = ib.read(localThreadId, c, imageblock_data_rate::color);
                color += f.color * popcount(ib.get_color_coverage_mask(localThreadId, c));
            }
            color /= (half)ib.get_num_samples();

            color.rgb = tonemap(color.rgb, kDefaultTonemapParams);
            color.rgb *= 1.5032214035h;

            ImplicitFragmentInPlace out;
            out.color = color;
            ib.write(out, localThreadId, 0xF);
        }
        """

    init(device: any MTLDevice, memoryOwner: task_id_token_t) throws {
        guard let commandQueue = device.makeCommandQueue() else {
            fatalError("Failed to create command queue.")
        }
        commandQueue.label = "LowLevelRenderer Command Queue"

        self.device = device
        self.commandQueue = commandQueue
        self.cameraPosition = [0, 0, Renderer.cameraDistance]
        self.cameraRotation = .init(ix: 0, iy: 0, iz: 0, r: 1)
        self.memoryOwner = memoryOwner
    }

    func createMaterialCompiler(resources: LowLevelRenderContextStandalone.Resources) throws {
        var configuration = LowLevelRenderContextStandalone.Configuration(device: device)
        configuration.memoryOwner = self.memoryOwner
        let renderContext = try LowLevelRenderContextStandalone(configuration: configuration, resources: resources)
        self.renderContext = renderContext
    }

    func createRenderer(resources: LowLevelRenderer.Resources) throws {
        let renderer = try LowLevelRenderer(resources: resources)
        self.renderer = renderer
        renderer.meshInstancesArrayCount = 1
    }

    func render(
        meshInstances: LowLevelMeshInstanceArray,
        texture: any MTLTexture,
        commandBuffer: any MTLCommandBuffer,
        portalQuads: Set<Int> = [],
        coplanarGroups: [[Int]] = []
    ) throws {
        guard let renderer else {
            commandBuffer.commit()
            return
        }

        let aspect = Float(texture.width) / Float(texture.height)
        let d = effectiveCameraDistance
        let projection = LowLevelRenderer.Camera.Projection.perspective(
            fovYRadians: fovY,
            aspectRatio: aspect,
            nearZ: d * 0.01,
            farZ: d * 100,
            reverseZ: true
        )

        renderer.cameras[0].position = cameraPosition
        renderer.cameras[0].rotation = cameraRotation
        renderer.cameras[0].projection = projection
        renderer.output.clearColor = MTLClearColor(red: 0.0, green: 0.0, blue: 0.0, alpha: 0.0)
        renderer.output.color = .init(texture: texture)
        try renderer.setMeshInstances(meshInstances, at: 0)

        commandBuffer.label = "Render Camera"
        var indices = Array(meshInstances.indices)
        indices = LowLevelRenderer.cullMeshInstances(
            meshInstances,
            indices: indices.span,
            configuration: .init(frustum: .init(from: renderer.cameras[0]))
        )
        var span = indices.mutableSpan
        LowLevelRenderer.sortMeshInstances(
            meshInstances,
            indices: &span,
            configuration: .init(cameraPosition: renderer.cameras[0].position)
        )
        Self.applyPaintOrder(of: coplanarGroups, to: &indices)
        let tilePipelineState: (any MTLRenderPipelineState)? =
            tonemapEnabled ? nil : try colorAdjustTilePipelineState(for: texture.pixelFormat, rasterSampleCount: rasterSampleCount)
        let portalQuadDepthStencil = portalQuads.isEmpty ? nil : portalQuadDepthStencilState
        renderer.render(using: commandBuffer) { state in
            var drawsPortalQuads = false
            for index in indices {
                if let portalQuadDepthStencil, portalQuads.contains(index) != drawsPortalQuads {
                    drawsPortalQuads.toggle()
                    if drawsPortalQuads {
                        state.encoder.setDepthStencilState(portalQuadDepthStencil)
                    } else {
                        state.reset()
                    }
                }
                state.render(meshInstancesArrayIndex: 0, meshInstanceIndex: index)
            }
            if let tilePipelineState {
                let encoder = state.encoder
                encoder.setRenderPipelineState(tilePipelineState)
                encoder.dispatchThreadsPerTile(MTLSize(width: encoder.tileWidth, height: encoder.tileHeight, depth: 1))
            }
            state.reset()
        }
        commandBuffer.commit()
    }

    private func colorAdjustTilePipelineState(for pixelFormat: MTLPixelFormat, rasterSampleCount: Int) throws -> any MTLRenderPipelineState
    {
        let key = ColorAdjustTilePipelineKey(pixelFormat: pixelFormat, rasterSampleCount: rasterSampleCount)
        if let cached = colorAdjustTilePipelineStates[key] {
            return cached
        }
        let library = try device.makeLibrary(source: Self.colorAdjustTileShaderSource, options: nil)
        guard let tileFunction = library.makeFunction(name: "colorAdjustTile") else {
            fatalError("Failed to find colorAdjustTile tile function")
        }
        let descriptor = MTLTileRenderPipelineDescriptor()
        descriptor.label = "Color Adjust Tile Pipeline"
        descriptor.tileFunction = tileFunction
        descriptor.colorAttachments[0].pixelFormat = pixelFormat
        descriptor.rasterSampleCount = rasterSampleCount
        descriptor.threadgroupSizeMatchesTileSize = true
        let (pipelineState, _) = try device.makeRenderPipelineState(tileDescriptor: descriptor, options: [])
        colorAdjustTilePipelineStates[key] = pipelineState
        return pipelineState
    }

    func setFOV(_ fovYRadians: Float) {
        fovY = fovYRadians
    }

    private static func applyPaintOrder(of coplanarGroups: [[Int]], to indices: inout [Int]) {
        guard !coplanarGroups.isEmpty else {
            return
        }

        var groupForIndex: [Int: Int] = [:]
        for (group, members) in coplanarGroups.enumerated() {
            for member in members {
                groupForIndex[member] = group
            }
        }

        var positions = Array(repeating: [Int](), count: coplanarGroups.count)
        for (position, index) in indices.enumerated() {
            if let group = groupForIndex[index] {
                positions[group].append(position)
            }
        }

        for (group, members) in coplanarGroups.enumerated() where positions[group].count > 1 {
            let present = Set(positions[group].map { indices[$0] })
            let ordered = members.filter { present.contains($0) }
            for (position, member) in zip(positions[group], ordered) {
                indices[position] = member
            }
        }
    }
}

#endif
