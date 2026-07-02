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

#if ENABLE_GPU_PROCESS_MODEL && canImport(RealityCoreDeformation, _version: 23.0.2) && canImport(ShaderGraph, _version: 159.0.3) && arch(arm64)

import DirectResource
import Metal
import USDKit
import RealityKit

final class MeshInstancePool {
    private(set) var meshInstances: LowLevelMeshInstanceArray
    private let renderContext: any LowLevelRenderContext

    init(
        renderContext: any LowLevelRenderContext,
        renderTargets: [LowLevelRenderTarget.Descriptor],
        initialCapacity: Int
    ) throws {
        self.renderContext = renderContext
        self.meshInstances = try renderContext.makeMeshInstanceArray(renderTargets: .init(renderTargets), count: initialCapacity)
    }

    init(renderContext: any LowLevelRenderContext, meshInstances: LowLevelMeshInstanceArray) {
        self.renderContext = renderContext
        self.meshInstances = meshInstances
    }

    func add(_ instance: LowLevelMeshInstance) throws {
        if let emptyIndex = meshInstances.firstIndex(where: { $0 == nil }) {
            try meshInstances.setMeshInstance(instance, index: emptyIndex)
        } else {
            let oldCount = meshInstances.count
            let newArray = try renderContext.makeMeshInstanceArray(renderTargets: meshInstances.renderTargets, count: max(oldCount * 2, 1))
            for (index, existing) in meshInstances.enumerated() {
                try newArray.setMeshInstance(existing, index: index)
            }
            try newArray.setMeshInstance(instance, index: oldCount)
            meshInstances = newArray
        }
    }

    func remove(_ instance: LowLevelMeshInstance) throws {
        guard let index = meshInstances.firstIndex(where: { $0 === instance }) else {
            fatalError("Mesh instance not found in MeshInstancePool")
        }
        try meshInstances.setMeshInstance(nil, index: index)
    }
}

extension simd_float4x4 {
    var minor: simd_float3x3 {
        .init(
            [columns.0.x, columns.0.y, columns.0.z],
            [columns.1.x, columns.1.y, columns.1.z],
            [columns.2.x, columns.2.y, columns.2.z]
        )
    }

    func transformPosition(_ position: simd_float3) -> simd_float3 {
        var result = simd_mul(self, simd_float4(position, 1))
        result /= result.w
        return simd_float3(result.x, result.y, result.z)
    }
}

extension LowLevelMeshResource.Descriptor {
    static func fromLlmDescriptor(_ llmDescriptor: LowLevelMesh.Descriptor) -> Self {
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

@_lifetime(buffer: copy buffer)
private func copyDataIntoBuffer(_ buffer: inout MutableRawSpan, from data: Data) {
    precondition(
        data.count <= buffer.byteCount,
        "copyDataIntoBuffer: data (\(data.count) B) exceeds buffer capacity (\(buffer.byteCount) B)"
    )

    // unsafe used here as that is the method for populating a MTLBuffer
    unsafe buffer.withUnsafeMutableBytes { unsafe $0.copyBytes(from: data) }
}

extension LowLevelMeshResource {
    func replaceVertexData(_ vertexData: [Data]) {
        for (vertexBufferIndex, vertexData) in vertexData.enumerated() {
            self.replaceVertices(at: vertexBufferIndex) { copyDataIntoBuffer(&$0, from: vertexData) }
        }
    }

    func replaceIndexData(_ indexData: Data?) {
        if let indexData = indexData {
            self.replaceIndices { copyDataIntoBuffer(&$0, from: indexData) }
        }
    }

    func replaceData(indexData: Data?, vertexData: [Data]) {
        // Copy index data
        self.replaceIndexData(indexData)

        // Copy vertex data
        self.replaceVertexData(vertexData)
    }
}

extension LowLevelTextureResource.Descriptor {
    static func from(_ textureDescriptor: MTLTextureDescriptor) -> LowLevelTextureResource.Descriptor {
        var descriptor = LowLevelTextureResource.Descriptor()
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

    static func from(_ texture: any MTLTexture, swizzle: MTLTextureSwizzleChannels) -> LowLevelTextureResource.Descriptor {
        var descriptor = LowLevelTextureResource.Descriptor()
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

    init(from descriptor: WKBridgeImageAsset) {
        self.init(
            textureType: descriptor.textureType,
            pixelFormat: descriptor.pixelFormat,
            width: descriptor.width,
            height: descriptor.height,
            depth: descriptor.depth,
            mipmapLevelCount: descriptor.mipmapLevelCount,
            arrayLength: descriptor.arrayLength,
            textureUsage: descriptor.textureUsage,
            swizzle: descriptor.swizzle
        )
    }
}

private func mapSemantic(_ semantic: LowLevelMesh.VertexSemantic) -> LowLevelMeshResource.VertexSemantic {
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

private func mapSemantic(_ semantic: WKBridgeVertexSemantic) -> LowLevelMeshResource.VertexSemantic {
    switch semantic {
    case .position: .position
    case .color: .color
    case .normal: .normal
    case .tangent: .tangent
    case .bitangent: .bitangent
    case .UV0: .uv0
    case .UV1: .uv1
    case .UV2: .uv2
    case .UV3: .uv3
    case .UV4: .uv4
    case .UV5: .uv5
    case .UV6: .uv6
    case .UV7: .uv7
    case .unspecified: .unspecified
    @unknown default: .unspecified
    }
}

extension LowLevelMeshResource.Descriptor {
    static func fromLlmDescriptor(_ llmDescriptor: WKBridgeMeshDescriptor) -> Self {
        var descriptor = Self.init()
        descriptor.vertexCapacity = Int(llmDescriptor.vertexCapacity)
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

internal func debugPrintShaderGraph(_ graph: ShaderGraph?, prefix: String = "", indent: String = "") {
    guard let graph = graph else {
        logInfo("\(indent)\(prefix)ShaderGraph: nil")
        return
    }

    logInfo("\(indent)\(prefix)ShaderGraph:")
    let nextIndent = indent + "  "

    // Print inputs
    logInfo("\(nextIndent)Inputs (\(graph.inputs.count)):")
    for (index, input) in graph.inputs.enumerated() {
        logInfo("\(nextIndent)  [\(index)] name: \(input.name), type: \(input.type)")
    }

    // Print outputs
    logInfo("\(nextIndent)Outputs (\(graph.outputs.count)):")
    for (index, output) in graph.outputs.enumerated() {
        logInfo("\(nextIndent)  [\(index)] name: \(output.name), type: \(output.type)")
    }

    // Print arguments node
    logInfo("\(nextIndent)Arguments Node:")
    debugPrintNode(graph.arguments, indent: nextIndent + "  ")

    // Print results node
    logInfo("\(nextIndent)Results Node:")
    debugPrintNode(graph.results, indent: nextIndent + "  ")

    // Print all nodes
    logInfo("\(nextIndent)Nodes (\(graph.nodes.count)):")
    for (key, node) in graph.nodes.sorted(by: { $0.key < $1.key }) {
        logInfo("\(nextIndent)  [\(key)]:")
        debugPrintNode(node, indent: nextIndent + "    ")
    }

    // Print edges
    logInfo("\(nextIndent)Edges (\(graph.edges.count)):")
    for (index, edge) in graph.edges.enumerated() {
        logInfo("\(nextIndent)  [\(index)] \(edge.outputNode):\(edge.outputPort ?? "") -> \(edge.inputNode):\(edge.inputPort)")
    }
}

private func debugPrintNode(_ node: ShaderGraph.Node, indent: String = "") {
    logInfo("\(indent)Name: \(node.name)")

    switch node.data {
    case .constant(let value):
        logInfo("\(indent)Type: Constant")
        debugPrintValue(value, indent: indent + "  ")

    case .definition(let definition):
        logInfo("\(indent)Type: Definition")
        logInfo("\(indent)  Definition Name: \(definition.name)")
        logInfo("\(indent)  Inputs (\(definition.inputs.count)):")
        for input in definition.inputs {
            logInfo("\(indent)    - \(input.name): \(input.type)")
        }
        logInfo("\(indent)  Outputs (\(definition.outputs.count)):")
        for output in definition.outputs {
            logInfo("\(indent)    - \(output.name): \(output.type)")
        }

    case .graph(let subgraph):
        logInfo("\(indent)Type: Subgraph")
        debugPrintShaderGraph(subgraph, prefix: "", indent: indent + "  ")

    @unknown default:
        logInfo("\(indent)Type: Unknown")
    }
}

private func debugPrintValue(_ value: ShaderGraph.Value, indent: String = "") {
    switch value {
    case .bool(let val):
        logInfo("\(indent)Value: bool(\(val))")
    case .uchar(let val):
        logInfo("\(indent)Value: uchar(\(val))")
    case .int(let val):
        logInfo("\(indent)Value: int(\(val))")
    case .uint(let val):
        logInfo("\(indent)Value: uint(\(val))")
    case .half(let val):
        logInfo("\(indent)Value: half(\(val))")
    case .float(let val):
        logInfo("\(indent)Value: float(\(val))")
    case .string(let val):
        logInfo("\(indent)Value: string(\"\(val)\")")
    case .float2(let val):
        logInfo("\(indent)Value: float2(\(val.x), \(val.y))")
    case .float3(let val):
        logInfo("\(indent)Value: float3(\(val.x), \(val.y), \(val.z))")
    case .float4(let val):
        logInfo("\(indent)Value: float4(\(val.x), \(val.y), \(val.z), \(val.w))")
    case .half2(let val):
        logInfo("\(indent)Value: half2(\(val.x), \(val.y))")
    case .half3(let val):
        logInfo("\(indent)Value: half3(\(val.x), \(val.y), \(val.z))")
    case .half4(let val):
        logInfo("\(indent)Value: half4(\(val.x), \(val.y), \(val.z), \(val.w))")
    case .int2(let val):
        logInfo("\(indent)Value: int2(\(val.x), \(val.y))")
    case .int3(let val):
        logInfo("\(indent)Value: int3(\(val.x), \(val.y), \(val.z))")
    case .int4(let val):
        logInfo("\(indent)Value: int4(\(val.x), \(val.y), \(val.z), \(val.w))")
    case .cgColor3(let color):
        if let components = color.components {
            logInfo("\(indent)Value: cgColor3(\(components[0]), \(components[1]), \(components[2]))")
        } else {
            fatalError("\(indent)Value: cgColor3(invalid)")
        }
    case .cgColor4(let color):
        if let components = color.components {
            logInfo("\(indent)Value: cgColor4(\(components[0]), \(components[1]), \(components[2]), \(components[3]))")
        } else {
            fatalError("\(indent)Value: cgColor4(invalid)")
        }
    case .float2x2(let m):
        logInfo("\(indent)Value: float2x2(")
        logInfo("\(indent)  [\(m.columns.0.x), \(m.columns.1.x)]")
        logInfo("\(indent)  [\(m.columns.0.y), \(m.columns.1.y)]")
        logInfo("\(indent))")
    case .float3x3(let m):
        logInfo("\(indent)Value: float3x3(")
        logInfo("\(indent)  [\(m.columns.0.x), \(m.columns.1.x), \(m.columns.2.x)]")
        logInfo("\(indent)  [\(m.columns.0.y), \(m.columns.1.y), \(m.columns.2.y)]")
        logInfo("\(indent)  [\(m.columns.0.z), \(m.columns.1.z), \(m.columns.2.z)]")
        logInfo("\(indent))")
    case .float4x4(let m):
        logInfo("\(indent)Value: float4x4(")
        logInfo("\(indent)  [\(m.columns.0.x), \(m.columns.1.x), \(m.columns.2.x), \(m.columns.3.x)]")
        logInfo("\(indent)  [\(m.columns.0.y), \(m.columns.1.y), \(m.columns.2.y), \(m.columns.3.y)]")
        logInfo("\(indent)  [\(m.columns.0.z), \(m.columns.1.z), \(m.columns.2.z), \(m.columns.3.z)]")
        logInfo("\(indent)  [\(m.columns.0.w), \(m.columns.1.w), \(m.columns.2.w), \(m.columns.3.w)]")
        logInfo("\(indent))")
    case .half2x2(let m):
        logInfo("\(indent)Value: half2x2(")
        logInfo("\(indent)  [\(m.columns.0.x), \(m.columns.1.x)]")
        logInfo("\(indent)  [\(m.columns.0.y), \(m.columns.1.y)]")
        logInfo("\(indent))")
    case .half3x3(let m):
        logInfo("\(indent)Value: half3x3(")
        logInfo("\(indent)  [\(m.columns.0.x), \(m.columns.1.x), \(m.columns.2.x)]")
        logInfo("\(indent)  [\(m.columns.0.y), \(m.columns.1.y), \(m.columns.2.y)]")
        logInfo("\(indent)  [\(m.columns.0.z), \(m.columns.1.z), \(m.columns.2.z)]")
        logInfo("\(indent))")
    case .half4x4(let m):
        logInfo("\(indent)Value: half4x4(")
        logInfo("\(indent)  [\(m.columns.0.x), \(m.columns.1.x), \(m.columns.2.x), \(m.columns.3.x)]")
        logInfo("\(indent)  [\(m.columns.0.y), \(m.columns.1.y), \(m.columns.2.y), \(m.columns.3.y)]")
        logInfo("\(indent)  [\(m.columns.0.z), \(m.columns.1.z), \(m.columns.2.z), \(m.columns.3.z)]")
        logInfo("\(indent)  [\(m.columns.0.w), \(m.columns.1.w), \(m.columns.2.w), \(m.columns.3.w)]")
        logInfo("\(indent))")
    @unknown default:
        logInfo("\(indent)Value: unknown type")
    }
}

internal func compareShaderGraphs(
    _ graph1: ShaderGraph?,
    _ graph2: ShaderGraph?,
    label1: String = "Graph 1",
    label2: String = "Graph 2"
) {
    logInfo("\n=== Comparing Shader Graphs: \(label1) vs \(label2) ===\n")

    guard let graph1 = graph1, let graph2 = graph2 else {
        logInfo("❌ One or both graphs are nil")
        logInfo("  \(label1): \(graph1 == nil ? "nil" : "present")")
        logInfo("  \(label2): \(graph2 == nil ? "nil" : "present")")
        return
    }

    var differences: [String] = []

    // Compare inputs
    if graph1.inputs.count != graph2.inputs.count {
        differences.append("Input count differs: \(graph1.inputs.count) vs \(graph2.inputs.count)")
    } else {
        for (index, (input1, input2)) in zip(graph1.inputs, graph2.inputs).enumerated() {
            if input1.name != input2.name {
                differences.append("  Input[\(index)] name differs: '\(input1.name)' vs '\(input2.name)'")
            }
            if input1.type != input2.type {
                differences.append("  Input[\(index)] type differs: \(input1.type) vs \(input2.type)")
            }
        }
    }

    // Compare outputs
    if graph1.outputs.count != graph2.outputs.count {
        differences.append("Output count differs: \(graph1.outputs.count) vs \(graph2.outputs.count)")
    } else {
        for (index, (output1, output2)) in zip(graph1.outputs, graph2.outputs).enumerated() {
            if output1.name != output2.name {
                differences.append("  Output[\(index)] name differs: '\(output1.name)' vs '\(output2.name)'")
            }
            if output1.type != output2.type {
                differences.append("  Output[\(index)] type differs: \(output1.type) vs \(output2.type)")
            }
        }
    }

    // Compare node counts
    if graph1.nodes.count != graph2.nodes.count {
        differences.append("Node count differs: \(graph1.nodes.count) vs \(graph2.nodes.count)")

        let keys1 = Set(graph1.nodes.keys)
        let keys2 = Set(graph2.nodes.keys)
        let onlyIn1 = keys1.subtracting(keys2)
        let onlyIn2 = keys2.subtracting(keys1)

        if !onlyIn1.isEmpty {
            differences.append("  Nodes only in \(label1): \(onlyIn1.sorted())")
        }
        if !onlyIn2.isEmpty {
            differences.append("  Nodes only in \(label2): \(onlyIn2.sorted())")
        }
    }

    // Compare common nodes
    let commonKeys = Set(graph1.nodes.keys).intersection(Set(graph2.nodes.keys))
    for key in commonKeys.sorted() {
        guard let node1 = graph1.nodes[key], let node2 = graph2.nodes[key] else { continue }

        if node1.name != node2.name {
            differences.append("  Node[\(key)].name differs: '\(node1.name)' vs '\(node2.name)'")
        }

        // Compare node data types
        let type1 = nodeDataTypeString(node1.data)
        let type2 = nodeDataTypeString(node2.data)
        if type1 != type2 {
            differences.append("  Node[\(key)].data type differs: \(type1) vs \(type2)")
        }
    }

    // Compare edges
    if graph1.edges.count != graph2.edges.count {
        differences.append("Edge count differs: \(graph1.edges.count) vs \(graph2.edges.count)")
    } else {
        // Create comparable edge descriptions
        let edges1Set = Set(graph1.edges.map { "\($0.outputNode):\($0.outputPort ?? "") -> \($0.inputNode):\($0.inputPort)" })
        let edges2Set = Set(graph2.edges.map { "\($0.outputNode):\($0.outputPort ?? "") -> \($0.inputNode):\($0.inputPort)" })

        let onlyIn1 = edges1Set.subtracting(edges2Set)
        let onlyIn2 = edges2Set.subtracting(edges1Set)

        if !onlyIn1.isEmpty {
            differences.append("  Edges only in \(label1):")
            for edge in onlyIn1.sorted() {
                differences.append("    - \(edge)")
            }
        }
        if !onlyIn2.isEmpty {
            differences.append("  Edges only in \(label2):")
            for edge in onlyIn2.sorted() {
                differences.append("    - \(edge)")
            }
        }
    }

    // Print results
    if differences.isEmpty {
        logInfo("✅ Graphs are identical")
    } else {
        logInfo("❌ Found \(differences.count) difference(s):\n")
        for diff in differences {
            logInfo("  • \(diff)")
        }
    }

    logInfo("\n=== End Comparison ===\n")
}

private func nodeDataTypeString(_ data: ShaderGraph.Node.NodeData) -> String {
    switch data {
    case .constant(let value):
        return "constant(\(value))"
    case .definition(let def):
        return "definition(\(def.name))"
    case .graph:
        return "graph"
    @unknown default:
        return "unknown"
    }
}

extension MTLPixelFormat {
    var bytesPerPixel: Int? {
        switch self {
        // MARK: - 8-bit (1 byte)
        case .a8Unorm,
            .r8Unorm,
            .r8Unorm_srgb,
            .r8Snorm,
            .r8Uint,
            .r8Sint,
            .stencil8:
            return 1

        // MARK: - 16-bit (2 bytes)
        case .r16Unorm,
            .r16Snorm,
            .r16Uint,
            .r16Sint,
            .r16Float,
            .rg8Unorm,
            .rg8Unorm_srgb,
            .rg8Snorm,
            .rg8Uint,
            .rg8Sint,
            .b5g6r5Unorm,
            .a1bgr5Unorm,
            .abgr4Unorm,
            .bgr5A1Unorm,
            .depth16Unorm:
            return 2

        // MARK: - 32-bit (4 bytes)
        case .r32Uint,
            .r32Sint,
            .r32Float,
            .rg16Unorm,
            .rg16Snorm,
            .rg16Uint,
            .rg16Sint,
            .rg16Float,
            .rgba8Unorm,
            .rgba8Unorm_srgb,
            .rgba8Snorm,
            .rgba8Uint,
            .rgba8Sint,
            .bgra8Unorm,
            .bgra8Unorm_srgb,
            .rgb10a2Unorm,
            .rgb10a2Uint,
            .rg11b10Float,
            .rgb9e5Float,
            .bgr10a2Unorm,
            .bgr10_xr,
            .bgr10_xr_srgb,
            .depth32Float,
            .x24_stencil8:
            return 4

        // MARK: - 64-bit (8 bytes)
        case .rgba16Unorm,
            .rgba16Snorm,
            .rgba16Uint,
            .rgba16Sint,
            .rgba16Float,
            .rg32Uint,
            .rg32Sint,
            .rg32Float,
            .bgra10_xr,
            .bgra10_xr_srgb,
            .depth32Float_stencil8,
            .x32_stencil8:
            return 8

        // MARK: - 128-bit (16 bytes)
        case .rgba32Uint,
            .rgba32Sint,
            .rgba32Float:
            return 16

        // MARK: - Compressed / Unknown
        default:
            return nil // Block-compressed (BCn, ASTC, EAC, PVRTC, etc.) or invalid
        }
    }
}

#endif
