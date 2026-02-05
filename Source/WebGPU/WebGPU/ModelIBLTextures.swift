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

#if canImport(RealityCoreRenderer, _version: 6) && canImport(USDStageKit, _version: 34)

import Metal
@_spi(RealityCoreRendererAPI) import RealityKit
@_spi(RealityCoreTextureProcessingAPI) import RealityKit

class IBLTextures {
    static func loadIBLTextures(
        renderContext: _Proto_LowLevelRenderContext_v1,
        diffuseTextureOriginal: MTLTexture,
        specularTextureOriginal: MTLTexture
    ) throws -> (
        diffuse: _Proto_LowLevelTextureResource_v1,
        specular: _Proto_LowLevelTextureResource_v1
    ) {
        guard let commandQueue = renderContext.device.makeCommandQueue() else {
            fatalError("Failed to create command queue")
        }
        guard let commandBuffer = commandQueue.makeCommandBuffer() else {
            fatalError("Failed to create command buffer")
        }

        let diffuseTextureResource = try renderContext.makeTextureResource(
            descriptor: .init(
                textureType: diffuseTextureOriginal.textureType,
                pixelFormat: diffuseTextureOriginal.pixelFormat,
                width: diffuseTextureOriginal.width,
                height: diffuseTextureOriginal.height,
                depth: diffuseTextureOriginal.depth,
                mipmapLevelCount: diffuseTextureOriginal.mipmapLevelCount,
                arrayLength: diffuseTextureOriginal.arrayLength,
                textureUsage: .shaderRead,
                swizzle: .init(
                    red: .red,
                    green: .red,
                    blue: .red,
                    alpha: .one
                )
            )
        )
        let specularTextureResource = try renderContext.makeTextureResource(
            descriptor: .init(
                textureType: specularTextureOriginal.textureType,
                pixelFormat: specularTextureOriginal.pixelFormat,
                width: specularTextureOriginal.width,
                height: specularTextureOriginal.height,
                depth: specularTextureOriginal.depth,
                mipmapLevelCount: specularTextureOriginal.mipmapLevelCount,
                arrayLength: specularTextureOriginal.arrayLength,
                textureUsage: .shaderRead,
                swizzle: .init(
                    red: .red,
                    green: .red,
                    blue: .red,
                    alpha: .one
                )
            )
        )

        let diffuseTexture = diffuseTextureResource.replace(using: commandBuffer)
        let specularTexture = specularTextureResource.replace(using: commandBuffer)

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let blitEncoder = commandBuffer.makeBlitCommandEncoder()!
        blitEncoder.copy(from: diffuseTextureOriginal, to: diffuseTexture)
        blitEncoder.copy(from: specularTextureOriginal, to: specularTexture)
        blitEncoder.endEncoding()

        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()

        return (diffuseTextureResource, specularTextureResource)
    }
}

#endif
