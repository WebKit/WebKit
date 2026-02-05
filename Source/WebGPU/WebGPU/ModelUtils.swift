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

import DirectResource
import Metal
@_spi(RealityCoreRendererAPI) import RealityKit
@_spi(SGInternal) import RealityKit
internal import WebGPU_Private.ModelTypes
@_spi(UsdLoaderAPI) import _RealityKit_SwiftUI

nonisolated func mapSemantic(_ semantic: LowLevelMesh.VertexSemantic) -> _Proto_LowLevelMeshResource_v1.VertexSemantic {
    switch semantic {
    case .position: .position
    case .color: .color
    case .normal: .normal
    case .tangent: .tangent
    case .bitangent: .bitangent
    case .uv0: .uv0
    case .uv1: .uv1
    case .uv2: .uv2
    case .uv3: .uv3
    case .uv4: .uv4
    case .uv5: .uv5
    case .uv6: .uv6
    case .uv7: .uv7
    case .unspecified: .unspecified
    default: .unspecified
    }
}

extension _Proto_LowLevelMeshResource_v1.Descriptor {
    nonisolated static func fromLlmDescriptor(_ llmDescriptor: LowLevelMesh.Descriptor) -> Self {
        var descriptor = Self.init()
        descriptor.vertexCapacity = llmDescriptor.vertexCapacity
        descriptor.vertexAttributes = llmDescriptor.vertexAttributes.map { attribute in
            .init(
                semantic: mapSemantic(attribute.semantic),
                format: attribute.format,
                layoutIndex: attribute.layoutIndex,
                offset: attribute.offset
            )
        }
        descriptor.vertexLayouts = llmDescriptor.vertexLayouts.map { layout in
            .init(bufferIndex: layout.bufferIndex, bufferOffset: layout.bufferOffset, bufferStride: layout.bufferStride)
        }
        descriptor.indexCapacity = llmDescriptor.indexCapacity
        descriptor.indexType = llmDescriptor.indexType

        return descriptor
    }
}

extension _Proto_LowLevelMeshResource_v1 {
    nonisolated func replaceVertexData(_ vertexData: [Data]) {
        for (vertexBufferIndex, vertexData) in vertexData.enumerated() {
            let bufferSizeInByte = vertexData.bytes.byteCount
            self.replaceVertices(at: vertexBufferIndex) { vertexBytes in
                vertexBytes.withUnsafeMutableBytes { ptr in
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                    // swift-format-ignore: NeverForceUnwrap
                    vertexData.copyBytes(to: ptr.baseAddress!.assumingMemoryBound(to: UInt8.self), count: bufferSizeInByte)
                }
            }
        }
    }

    nonisolated func replaceIndexData(_ indexData: Data?) {
        if let indexData = indexData {
            self.replaceIndices { indicesBytes in
                indicesBytes.withUnsafeMutableBytes { ptr in
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                    // swift-format-ignore: NeverForceUnwrap
                    indexData.copyBytes(to: ptr.baseAddress!.assumingMemoryBound(to: UInt8.self), count: ptr.count)
                }
            }
        }
    }

    nonisolated func replaceData(indexData: Data?, vertexData: [Data]) {
        // Copy index data
        self.replaceIndexData(indexData)

        // Copy vertex data
        self.replaceVertexData(vertexData)
    }
}

extension _Proto_LowLevelTextureResource_v1.Descriptor {
    static func from(_ textureDescriptor: MTLTextureDescriptor) -> _Proto_LowLevelTextureResource_v1.Descriptor {
        var descriptor = _Proto_LowLevelTextureResource_v1.Descriptor()
        descriptor.width = textureDescriptor.width
        descriptor.height = textureDescriptor.height
        descriptor.depth = textureDescriptor.depth
        descriptor.mipmapLevelCount = textureDescriptor.mipmapLevelCount
        descriptor.arrayLength = textureDescriptor.arrayLength
        descriptor.pixelFormat = textureDescriptor.pixelFormat
        descriptor.textureType = textureDescriptor.textureType
        descriptor.textureUsage = textureDescriptor.usage
        descriptor.swizzle = textureDescriptor.swizzle

        return descriptor
    }

    static func from(_ texture: MTLTexture, swizzle: MTLTextureSwizzleChannels) -> _Proto_LowLevelTextureResource_v1.Descriptor {
        var descriptor = _Proto_LowLevelTextureResource_v1.Descriptor()
        descriptor.width = texture.width
        descriptor.height = texture.height
        descriptor.depth = texture.depth
        descriptor.mipmapLevelCount = texture.mipmapLevelCount
        descriptor.arrayLength = texture.arrayLength
        descriptor.pixelFormat = texture.pixelFormat
        descriptor.textureType = texture.textureType
        descriptor.textureUsage = texture.usage
        descriptor.swizzle = swizzle

        return descriptor
    }
}

#endif
