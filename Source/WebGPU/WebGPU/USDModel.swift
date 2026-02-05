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

internal import Metal
internal import OSLog
internal import WebGPU_Private.ModelTypes
internal import simd

#if canImport(RealityCoreRenderer, _version: 6) && canImport(USDStageKit, _version: 34)
@_spi(RealityCoreRendererAPI) import RealityKit
@_spi(RealityCoreTextureProcessingAPI) import RealityCoreTextureProcessing
@_spi(UsdLoaderAPI) import _USDKit_RealityKit
@_spi(SwiftAPI) import DirectResource
import _USDKit_RealityKit
import RealityKit
@_spi(SGPrivate) import ShaderGraph
import RealityCoreDeformation

extension _USDKit_RealityKit._Proto_MeshDataUpdate_v1 {
    @_silgen_name("$s18_USDKit_RealityKit24_Proto_MeshDataUpdate_v1V18instanceTransformsSaySo13simd_float4x4aGvg")
    internal func instanceTransformsCompat() -> [simd_float4x4]
}

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V21geometryBindTransformSo13simd_float4x4avg")
    internal func geometryBindTransformCompat() -> simd_float4x4
}

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V15jointTransformsSaySo13simd_float4x4aGvg")
    internal func jointTransformsCompat() -> [simd_float4x4]
}

extension _USDKit_RealityKit._Proto_DeformationData_v1.SkinningData {
    @_silgen_name("$s18_USDKit_RealityKit25_Proto_DeformationData_v1V08SkinningF0V16inverseBindPosesSaySo13simd_float4x4aGvg")
    internal func inverseBindPosesCompat() -> [simd_float4x4]
}

extension RealityCoreRenderer._Proto_LowLevelRenderContext_v1 {
    @_silgen_name(
        "$s19RealityCoreRenderer16MaterialCompilerC24makeShaderGraphFunctionsyAA015_Proto_LowLevelD11Resource_v1C0gH6OutputV10Foundation4DataVSg_ScA_pSgYitYaKF"
    )
    internal func makeShaderGraphFunctions(
        module shaderGraphModule: ShaderGraph.Module,
        geometryModifier geometryModifierFunctionReference: ShaderGraph.FunctionReference?,
        surfaceShader surfaceShaderFunctionReference: ShaderGraph.FunctionReference,
        _ isolation: isolated (any Actor)?
    ) async throws -> sending _Proto_LowLevelMaterialResource_v1.ShaderGraphOutput
}

private func toSGType(_ module: WebBridgeModule?) -> ShaderGraph.Module? {
    guard let module else { return nil }

    // Convert the WebBridgeModule to ShaderGraph.Module
    // LIMITATION: ShaderGraph.Module's public initializer only accepts name and imports
    // The typeDefinitions, functions, and graphs cannot be set via the public API
    //
    // This means converting a complete WebBridgeModule to ShaderGraph.Module will
    // lose the typeDefinitions, functions, and graphs data.
    //
    // For a complete conversion, you may need to:
    // 1. Use the materialSourceArchive approach (see commented code in updateMaterial)
    // 2. Find an alternative ShaderGraph API that accepts all module components
    // 3. Use a different serialization/deserialization approach

    // Convert imports
    let imports = module.imports.compactMap { toSGModuleReference($0) }

    // Log warning if we're losing data
    if !module.typeDefinitions.isEmpty || !module.functions.isEmpty || !module.graphs.isEmpty {
        logError(
            "WARNING: Converting WebBridgeModule to ShaderGraph.Module - losing \(module.typeDefinitions.count) type definitions, \(module.functions.count) functions, and \(module.graphs.count) graphs due to API limitations"
        )
    }

    // Create the ShaderGraph.Module with available data only
    return ShaderGraph.Module(module.name, imports: imports)
}

// Helper conversion functions for nested types
private func toSGModuleReference(_ ref: WebBridgeModuleReference) -> ShaderGraph.ModuleReference? {
    // Convert WebBridgeModuleReference to ShaderGraph.ModuleReference
    guard let module = toSGType(ref.module) else { return nil }
    return ShaderGraph.ModuleReference(module)
}

private func toSGTypeDefinition(_ typeDef: WebBridgeTypeDefinition) -> ShaderGraph.TypeDefinition? {
    // TypeDefinition cannot be constructed - must be obtained from Module
    // This conversion is not possible without a Module context
    nil
}

private func toSGTypeReference(_ typeRef: WebBridgeTypeReference) -> ShaderGraph.TypeReference? {
    // TypeReference cannot be constructed - must be obtained from TypeDefinition
    // This conversion is not possible
    nil
}

private func toSGStructMember(_ member: WebBridgeStructMember) -> ShaderGraph.TypeDefinition.StructMember? {
    guard let type = toSGTypeReference(member.type) else { return nil }
    return ShaderGraph.TypeDefinition.StructMember(member.name, type: type)
}

private func toSGEnumCase(_ enumCase: WebBridgeEnumCase) -> ShaderGraph.TypeDefinition.EnumCase? {
    ShaderGraph.TypeDefinition.EnumCase(enumCase.name, value: enumCase.value)
}

private func toSGFunction(_ function: WebBridgeFunction) -> ShaderGraph.Function? {
    // Function cannot be constructed - must be obtained from Module or created via addGraph
    // This conversion is not possible
    nil
}

private func toSGFunctionArgument(_ arg: WebBridgeFunctionArgument) -> ShaderGraph.Function.Argument? {
    guard let type = toSGTypeReference(arg.type) else { return nil }
    return ShaderGraph.Function.Argument(name: arg.name, type: type)
}

private func toSGModuleGraph(_ graph: WebBridgeModuleGraph) -> ShaderGraph.ModuleGraph? {
    // ModuleGraph cannot be constructed - must be created via Module.addGraph()
    // This conversion is not possible
    nil
}

private func toSGNode(_ node: WebBridgeNode) -> ShaderGraph.ModuleGraph.Node? {
    // Node cannot be constructed - created via ModuleGraph.insert()
    // This conversion is not possible
    nil
}

private func toSGNodeID(_ nodeID: WebBridgeNodeID) -> ShaderGraph.ModuleGraph.Node.ID? {
    // Node.ID cannot be constructed - returned from ModuleGraph.insert()
    // This conversion is not possible
    nil
}

private func toSGNodeInstruction(_ instruction: WebBridgeNodeInstruction) -> ShaderGraph.ModuleGraph.Node.Instruction? {
    // Convert WebBridgeNodeInstruction to ShaderGraph.ModuleGraph.Node.Instruction
    // The instruction type determines which properties are used
    switch instruction.type {
    case .functionCall:
        guard let functionCall = instruction.functionCall else { return nil }
        return toSGNodeInstructionFromFunctionCall(functionCall)

    case .functionConstant:
        guard let constantName = instruction.constantName,
            let literal = instruction.literal
        else { return nil }
        return toSGNodeInstructionFromFunctionConstant(name: constantName, literal: literal)

    case .literal:
        guard let literal = instruction.literal else { return nil }
        return toSGNodeInstructionFromLiteral(literal)

    case .argument:
        guard let argumentName = instruction.argumentName else { return nil }
        return toSGNodeInstructionFromArgument(argumentName)

    case .element:
        guard let elementType = instruction.elementType,
            let elementName = instruction.elementName
        else { return nil }
        return toSGNodeInstructionFromElement(type: elementType, name: elementName)

    @unknown default:
        logError("Unknown WebBridgeNodeInstructionType: \(instruction.type.rawValue)")
        return nil
    }
}

private func toSGNodeInstructionFromFunctionCall(_ functionCall: WebBridgeFunctionCall) -> ShaderGraph.ModuleGraph.Node.Instruction? {
    // Based on the test: .functionCall(.name("dot")) or .functionCall(.reference(fnRef))
    switch functionCall.type {
    case .name:
        guard let name = functionCall.name else { return nil }
        return .functionCall(.name(name))

    case .reference:
        guard let reference = functionCall.reference,
            let sgRef = toSGType(reference)
        else { return nil }
        return .functionCall(.reference(sgRef))

    @unknown default:
        logError("Unknown WebBridgeFunctionCallType: \(functionCall.type.rawValue)")
        return nil
    }
}

private func toSGNodeInstructionFromFunctionConstant(name: String, literal: WebBridgeLiteral) -> ShaderGraph.ModuleGraph.Node.Instruction? {
    // Based on the test: .functionConstant("dayCycleConstant", .int32(0))
    guard let sgLiteral = toSGLiteral(literal) else { return nil }
    return .functionConstant(name, sgLiteral)
}

private func toSGNodeInstructionFromLiteral(_ literal: WebBridgeLiteral) -> ShaderGraph.ModuleGraph.Node.Instruction? {
    // Based on the test: .literal(.float3(SIMD3<Float>(1,0,1)))
    guard let sgLiteral = toSGLiteral(literal) else { return nil }
    return .literal(sgLiteral)
}

private func toSGNodeInstructionFromArgument(_ argumentName: String) -> ShaderGraph.ModuleGraph.Node.Instruction? {
    // Based on the test: .argument("params")
    .argument(argumentName)
}

private func toSGNodeInstructionFromElement(type: WebBridgeTypeReference, name: String) -> ShaderGraph.ModuleGraph.Node.Instruction? {
    // Based on the test: .element(uniformsType, "vector")
    guard let sgType = toSGTypeReference(type) else { return nil }
    return .element(sgType, name)
}

private func toSGLiteral(_ literal: WebBridgeLiteral) -> ShaderGraph.Literal? {
    // Convert WebBridgeLiteral to ShaderGraph.Literal
    // ShaderGraph.Literal is an enum - we need to construct the appropriate case
    let data = literal.archive.data.map { $0.uint32Value }

    // Helper function to reconstruct different types from UInt32 array
    func toFloat(_ value: UInt32) -> Float {
        Float(bitPattern: value)
    }

    func toInt32(_ value: UInt32) -> Int32 {
        Int32(bitPattern: value)
    }

    #if arch(arm64)
    func toHalf(_ value: UInt32) -> Swift.Float16 {
        Swift.Float16(bitPattern: UInt16(value))
    }
    #endif

    // Reconstruct the literal based on its type
    switch literal.type {
    case .bool:
        guard let first = data.first else { return nil }
        return .bool(first != 0)

    case .int32:
        guard let first = data.first else { return nil }
        return .int32(toInt32(first))

    case .uInt32:
        guard let first = data.first else { return nil }
        return .uint32(first)

    case .float:
        guard let first = data.first else { return nil }
        return .float(toFloat(first))

    case .float2:
        guard data.count >= 2 else { return nil }
        return .float2(SIMD2(toFloat(data[0]), toFloat(data[1])))

    case .float3:
        guard data.count >= 3 else { return nil }
        return .float3(SIMD3(toFloat(data[0]), toFloat(data[1]), toFloat(data[2])))

    case .float4:
        guard data.count >= 4 else { return nil }
        return .float4(SIMD4(toFloat(data[0]), toFloat(data[1]), toFloat(data[2]), toFloat(data[3])))

    #if arch(arm64)
    case .half:
        guard let first = data.first else { return nil }
        return .half(toHalf(first))

    case .half2:
        guard data.count >= 2 else { return nil }
        return .half2(SIMD2(toHalf(data[0]), toHalf(data[1])))

    case .half3:
        guard data.count >= 3 else { return nil }
        return .half3(SIMD3(toHalf(data[0]), toHalf(data[1]), toHalf(data[2])))

    case .half4:
        guard data.count >= 4 else { return nil }
        return .half4(SIMD4(toHalf(data[0]), toHalf(data[1]), toHalf(data[2]), toHalf(data[3])))
    #endif

    case .int2:
        guard data.count >= 2 else { return nil }
        return .int2(SIMD2(toInt32(data[0]), toInt32(data[1])))

    case .int3:
        guard data.count >= 3 else { return nil }
        return .int3(SIMD3(toInt32(data[0]), toInt32(data[1]), toInt32(data[2])))

    case .int4:
        guard data.count >= 4 else { return nil }
        return .int4(SIMD4(toInt32(data[0]), toInt32(data[1]), toInt32(data[2]), toInt32(data[3])))

    case .uInt2:
        guard data.count >= 2 else { return nil }
        return .uint2(SIMD2(data[0], data[1]))

    case .uInt3:
        guard data.count >= 3 else { return nil }
        return .uint3(SIMD3(data[0], data[1], data[2]))

    case .uInt4:
        guard data.count >= 4 else { return nil }
        return .uint4(SIMD4(data[0], data[1], data[2], data[3]))

    case .float2x2:
        guard data.count >= 4 else { return nil }
        return .float2x2(
            SIMD2(toFloat(data[0]), toFloat(data[1])),
            SIMD2(toFloat(data[2]), toFloat(data[3]))
        )

    case .float3x3:
        guard data.count >= 9 else { return nil }
        return .float3x3(
            SIMD3(toFloat(data[0]), toFloat(data[1]), toFloat(data[2])),
            SIMD3(toFloat(data[3]), toFloat(data[4]), toFloat(data[5])),
            SIMD3(toFloat(data[6]), toFloat(data[7]), toFloat(data[8]))
        )

    case .float4x4:
        guard data.count >= 16 else { return nil }
        return .float4x4(
            SIMD4(toFloat(data[0]), toFloat(data[1]), toFloat(data[2]), toFloat(data[3])),
            SIMD4(toFloat(data[4]), toFloat(data[5]), toFloat(data[6]), toFloat(data[7])),
            SIMD4(toFloat(data[8]), toFloat(data[9]), toFloat(data[10]), toFloat(data[11])),
            SIMD4(toFloat(data[12]), toFloat(data[13]), toFloat(data[14]), toFloat(data[15]))
        )

    #if arch(arm64)
    case .half2x2:
        guard data.count >= 4 else { return nil }
        return .half2x2(
            SIMD2(toHalf(data[0]), toHalf(data[1])),
            SIMD2(toHalf(data[2]), toHalf(data[3]))
        )

    case .half3x3:
        guard data.count >= 9 else { return nil }
        return .half3x3(
            SIMD3(toHalf(data[0]), toHalf(data[1]), toHalf(data[2])),
            SIMD3(toHalf(data[3]), toHalf(data[4]), toHalf(data[5])),
            SIMD3(toHalf(data[6]), toHalf(data[7]), toHalf(data[8]))
        )

    case .half4x4:
        guard data.count >= 16 else { return nil }
        return .half4x4(
            SIMD4(toHalf(data[0]), toHalf(data[1]), toHalf(data[2]), toHalf(data[3])),
            SIMD4(toHalf(data[4]), toHalf(data[5]), toHalf(data[6]), toHalf(data[7])),
            SIMD4(toHalf(data[8]), toHalf(data[9]), toHalf(data[10]), toHalf(data[11])),
            SIMD4(toHalf(data[12]), toHalf(data[13]), toHalf(data[14]), toHalf(data[15]))
        )
    #endif

    @unknown default:
        logError("Unknown WebBridgeLiteralType: \(literal.type.rawValue)")
        return nil
    }
}

private func toSGGraphEdge(_ edge: WebBridgeGraphEdge) -> (ShaderGraph.ModuleGraph.Node.ID, ShaderGraph.ModuleGraph.Node.ID, String)? {
    // ShaderGraph edges are tuples created via graph.connect()
    // We cannot create them directly without a ModuleGraph context
    // This conversion is not possible
    nil
}

private func toSGType(_ functionRef: WebBridgeFunctionReference?) -> ShaderGraph.FunctionReference? {
    guard let functionRef else { return nil }
    return nil
}

// Conversion functions from ShaderGraph types to WebBridge types
private func fromSGType(_ module: ShaderGraph.Module?) -> WebBridgeModule? {
    guard let module else { return nil }

    // Convert all nested types from ShaderGraph to WebBridge format
    let imports = fromSGModuleReferenceArray(module.imports)
    let typeDefinitions = fromSGTypeDefinitionArray(module.typeDefinitions)
    let functions = fromSGFunctionArray(module.functions)
    let graphs = fromSGModuleGraphArray(module.graphs)

    return WebBridgeModule(
        name: module.name,
        imports: imports,
        typeDefinitions: typeDefinitions,
        functions: functions,
        graphs: graphs
    )
}

// Helper conversion functions from ShaderGraph to WebBridge types
private func fromSGModuleReferenceArray(_ refs: [ShaderGraph.ModuleReference]) -> [WebBridgeModuleReference] {
    refs.compactMap { fromSGModuleReference($0) }
}

private func fromSGModuleReference(_ ref: ShaderGraph.ModuleReference) -> WebBridgeModuleReference? {
    guard let module = fromSGType(ref.module) else { return nil }
    return WebBridgeModuleReference(module: module)
}

private func fromSGTypeDefinitionArray(_ typeDefs: [ShaderGraph.TypeDefinition]) -> [WebBridgeTypeDefinition] {
    typeDefs.compactMap { fromSGTypeDefinition($0) }
}

private func fromSGTypeDefinition(_ typeDef: ShaderGraph.TypeDefinition) -> WebBridgeTypeDefinition? {
    // TypeDefinition doesn't expose structMembers, enumCases, or structureType through public API
    // The proper way to serialize/deserialize is using ModuleCoder
    // For now, return nil to indicate this conversion is not supported
    logError("Cannot convert ShaderGraph.TypeDefinition - use ModuleCoder for serialization")
    return nil
}

private func fromSGTypeReference(_ typeRef: ShaderGraph.TypeReference) -> WebBridgeTypeReference? {
    // TypeReference doesn't expose moduleName or typeDefIndex through public API
    // The proper way to serialize/deserialize is using ModuleCoder
    logError("Cannot convert ShaderGraph.TypeReference - use ModuleCoder for serialization")
    return nil
}

private func fromSGStructMember(_ member: ShaderGraph.TypeDefinition.StructMember) -> WebBridgeStructMember? {
    guard let type = fromSGTypeReference(member.type) else { return nil }
    return WebBridgeStructMember(name: member.name, type: type)
}

private func fromSGEnumCase(_ enumCase: ShaderGraph.TypeDefinition.EnumCase) -> WebBridgeEnumCase? {
    WebBridgeEnumCase(name: enumCase.name, value: enumCase.value)
}

private func fromSGFunctionArray(_ functions: [ShaderGraph.Function]) -> [WebBridgeFunction] {
    functions.compactMap { fromSGFunction($0) }
}

private func fromSGFunction(_ function: ShaderGraph.Function) -> WebBridgeFunction? {
    // Function doesn't expose kind or kindName through public API
    // The proper way to serialize/deserialize is using ModuleCoder
    logError("Cannot convert ShaderGraph.Function - use ModuleCoder for serialization")
    return nil
}

private func fromSGFunctionArgument(_ arg: ShaderGraph.Function.Argument) -> WebBridgeFunctionArgument? {
    guard let type = fromSGTypeReference(arg.type) else { return nil }
    return WebBridgeFunctionArgument(name: arg.name, type: type)
}

private func fromSGModuleGraphArray(_ graphs: [ShaderGraph.ModuleGraph]) -> [WebBridgeModuleGraph] {
    graphs.compactMap { fromSGModuleGraph($0) }
}

private func fromSGModuleGraph(_ graph: ShaderGraph.ModuleGraph) -> WebBridgeModuleGraph? {
    // ModuleGraph doesn't expose index property through public API
    // The proper way to serialize/deserialize is using ModuleCoder
    logError("Cannot convert ShaderGraph.ModuleGraph - use ModuleCoder for serialization")
    return nil
}

private func fromSGNode(_ node: ShaderGraph.ModuleGraph.Node) -> WebBridgeNode? {
    // Node doesn't expose identifier property through public API
    // The proper way to serialize/deserialize is using ModuleCoder
    logError("Cannot convert ShaderGraph.ModuleGraph.Node - use ModuleCoder for serialization")
    return nil
}

private func fromSGNodeID(_ nodeID: ShaderGraph.ModuleGraph.Node.ID) -> WebBridgeNodeID? {
    // Node.ID doesn't expose value property through public API
    // The proper way to serialize/deserialize is using ModuleCoder
    logError("Cannot convert ShaderGraph.ModuleGraph.Node.ID - use ModuleCoder for serialization")
    return nil
}

private func fromSGNodeInstruction(_ instruction: ShaderGraph.ModuleGraph.Node.Instruction) -> WebBridgeNodeInstruction? {
    // Convert ShaderGraph.ModuleGraph.Node.Instruction to WebBridgeNodeInstruction
    // Based on the test example, Instruction is an enum with cases:
    // .functionCall(FunctionCall), .literal(Literal), .argument(String), .element(TypeReference, String), .functionConstant(String, Literal)
    switch instruction {
    case .functionCall(let call):
        // FunctionCall is itself an enum with .name(String) or .reference(FunctionReference)
        switch call {
        case .name(let name):
            let functionCall = WebBridgeFunctionCall(name: name)
            return WebBridgeNodeInstruction(functionCall: functionCall)
        case .reference(let ref):
            guard let webRef = fromSGType(ref) else { return nil }
            let functionCall = WebBridgeFunctionCall(reference: webRef)
            return WebBridgeNodeInstruction(functionCall: functionCall)
        @unknown default:
            fatalError("unexpecetd value contained in call")
        }

    case .functionConstant(let name, let literal):
        guard let webLiteral = fromSGLiteral(literal) else { return nil }
        return WebBridgeNodeInstruction(functionConstant: name, literal: webLiteral)

    case .literal(let literal):
        guard let webLiteral = fromSGLiteral(literal) else { return nil }
        return WebBridgeNodeInstruction(literal: webLiteral)

    case .argument(let argumentName):
        return WebBridgeNodeInstruction(argument: argumentName)

    case .element(let type, let name):
        guard let webType = fromSGTypeReference(type) else { return nil }
        return WebBridgeNodeInstruction(elementType: webType, elementName: name)
    @unknown default:
        fatalError("unexpecetd value contained in instruction")
    }
}

private func fromSGLiteral(_ literal: ShaderGraph.Literal) -> WebBridgeLiteral? {
    // Convert ShaderGraph.Literal to WebBridgeLiteral
    // ShaderGraph.Literal is an enum with associated values
    // We need to pattern match each case and convert to WebBridgeLiteral format
    let (literalType, data): (WebBridgeLiteralType, [UInt32])

    switch literal {
    case .bool(let value):
        literalType = .bool
        data = [value ? 1 : 0]

    case .int32(let value):
        literalType = .int32
        data = [UInt32(bitPattern: value)]

    case .uint32(let value):
        literalType = .uInt32
        data = [value]

    case .float(let value):
        literalType = .float
        data = [value.bitPattern]

    case .float2(let value):
        literalType = .float2
        data = [value.x.bitPattern, value.y.bitPattern]

    case .float3(let value):
        literalType = .float3
        data = [value.x.bitPattern, value.y.bitPattern, value.z.bitPattern]

    case .float4(let value):
        literalType = .float4
        data = [value.x.bitPattern, value.y.bitPattern, value.z.bitPattern, value.w.bitPattern]

    #if arch(arm64)
    case .half(let value):
        literalType = .half
        data = [UInt32(value.bitPattern)]

    case .half2(let value):
        literalType = .half2
        data = [UInt32(value.x.bitPattern), UInt32(value.y.bitPattern)]

    case .half3(let value):
        literalType = .half3
        data = [UInt32(value.x.bitPattern), UInt32(value.y.bitPattern), UInt32(value.z.bitPattern)]

    case .half4(let value):
        literalType = .half4
        data = [UInt32(value.x.bitPattern), UInt32(value.y.bitPattern), UInt32(value.z.bitPattern), UInt32(value.w.bitPattern)]
    #endif

    case .int2(let value):
        literalType = .int2
        data = [UInt32(bitPattern: value.x), UInt32(bitPattern: value.y)]

    case .int3(let value):
        literalType = .int3
        data = [UInt32(bitPattern: value.x), UInt32(bitPattern: value.y), UInt32(bitPattern: value.z)]

    case .int4(let value):
        literalType = .int4
        data = [UInt32(bitPattern: value.x), UInt32(bitPattern: value.y), UInt32(bitPattern: value.z), UInt32(bitPattern: value.w)]

    case .uint2(let value):
        literalType = .uInt2
        data = [value.x, value.y]

    case .uint3(let value):
        literalType = .uInt3
        data = [value.x, value.y, value.z]

    case .uint4(let value):
        literalType = .uInt4
        data = [value.x, value.y, value.z, value.w]

    case .float2x2(let col0, let col1):
        literalType = .float2x2
        data = [col0.x.bitPattern, col0.y.bitPattern, col1.x.bitPattern, col1.y.bitPattern]

    case .float3x3(let col0, let col1, let col2):
        literalType = .float3x3
        data = [
            col0.x.bitPattern, col0.y.bitPattern, col0.z.bitPattern,
            col1.x.bitPattern, col1.y.bitPattern, col1.z.bitPattern,
            col2.x.bitPattern, col2.y.bitPattern, col2.z.bitPattern,
        ]

    case .float4x4(let col0, let col1, let col2, let col3):
        literalType = .float4x4
        data = [
            col0.x.bitPattern, col0.y.bitPattern, col0.z.bitPattern, col0.w.bitPattern,
            col1.x.bitPattern, col1.y.bitPattern, col1.z.bitPattern, col1.w.bitPattern,
            col2.x.bitPattern, col2.y.bitPattern, col2.z.bitPattern, col2.w.bitPattern,
            col3.x.bitPattern, col3.y.bitPattern, col3.z.bitPattern, col3.w.bitPattern,
        ]

    #if arch(arm64)
    case .half2x2(let col0, let col1):
        literalType = .half2x2
        data = [
            UInt32(col0.x.bitPattern), UInt32(col0.y.bitPattern),
            UInt32(col1.x.bitPattern), UInt32(col1.y.bitPattern),
        ]

    case .half3x3(let col0, let col1, let col2):
        literalType = .half3x3
        data = [
            UInt32(col0.x.bitPattern), UInt32(col0.y.bitPattern), UInt32(col0.z.bitPattern),
            UInt32(col1.x.bitPattern), UInt32(col1.y.bitPattern), UInt32(col1.z.bitPattern),
            UInt32(col2.x.bitPattern), UInt32(col2.y.bitPattern), UInt32(col2.z.bitPattern),
        ]

    case .half4x4(let col0, let col1, let col2, let col3):
        literalType = .half4x4
        data = [
            UInt32(col0.x.bitPattern), UInt32(col0.y.bitPattern), UInt32(col0.z.bitPattern), UInt32(col0.w.bitPattern),
            UInt32(col1.x.bitPattern), UInt32(col1.y.bitPattern), UInt32(col1.z.bitPattern), UInt32(col1.w.bitPattern),
            UInt32(col2.x.bitPattern), UInt32(col2.y.bitPattern), UInt32(col2.z.bitPattern), UInt32(col2.w.bitPattern),
            UInt32(col3.x.bitPattern), UInt32(col3.y.bitPattern), UInt32(col3.z.bitPattern), UInt32(col3.w.bitPattern),
        ]
    #endif

    @unknown default:
        logError("Unknown ShaderGraph.Literal case")
        return nil
    }

    // Convert UInt32 array to NSNumber array
    let nsData = data.map { NSNumber(value: $0) }

    return WebBridgeLiteral(type: literalType, data: nsData)
}

private func fromSGGraphEdge(_ edge: (ShaderGraph.ModuleGraph.Node.ID, ShaderGraph.ModuleGraph.Node.ID, String)) -> WebBridgeGraphEdge? {
    guard let source = fromSGNodeID(edge.0) else { return nil }
    guard let destination = fromSGNodeID(edge.1) else { return nil }
    return WebBridgeGraphEdge(source: source, destination: destination, argument: edge.2)
}

private func fromSGType(_ functionRef: ShaderGraph.FunctionReference?) -> WebBridgeFunctionReference? {
    guard let functionRef else { return nil }
    #if canImport(RealityCoreRenderer, _version: 8)
    return WebBridgeFunctionReference(moduleName: functionRef.module, functionIndex: 0)
    #else
    return WebBridgeFunctionReference(moduleName: "functionRef.module", functionIndex: 0)
    #endif
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

private func mapSemantic(_ semantic: Int) -> _Proto_LowLevelMeshResource_v1.VertexSemantic {
    switch semantic {
    case 0: .position
    case 1: .color
    case 2: .normal
    case 3: .tangent
    case 4: .bitangent
    case 5: .uv0
    case 6: .uv1
    case 7: .uv2
    case 8: .uv3
    case 9: .uv4
    case 10: .uv5
    case 11: .uv6
    case 12: .uv7
    case 13: .unspecified
    default: .unspecified
    }
}

extension _Proto_LowLevelMeshResource_v1.Descriptor {
    nonisolated static func fromLlmDescriptor(_ llmDescriptor: WebBridgeMeshDescriptor) -> Self {
        var descriptor = Self.init()
        descriptor.vertexCapacity = Int(llmDescriptor.vertexCapacity)
        descriptor.vertexAttributes = llmDescriptor.vertexAttributes.map { attribute in
            .init(
                semantic: mapSemantic(attribute.semantic),
                format: MTLVertexFormat(rawValue: UInt(attribute.format)) ?? .invalid,
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

private func isNonZero(value: Float) -> Bool {
    abs(value) > Float.ulpOfOne
}

private func isNonZero(_ vector: simd_float4) -> Bool {
    isNonZero(value: vector[0]) || isNonZero(value: vector[1]) || isNonZero(value: vector[2]) || isNonZero(value: vector[3])
}

private func isNonZero(matrix: simd_float4x4) -> Bool {
    isNonZero(_: matrix.columns.0) || isNonZero(_: matrix.columns.1) || isNonZero(_: matrix.columns.2) || isNonZero(_: matrix.columns.3)
}

private func makeMTLTextureFromImageAsset(
    _ imageAsset: WebBridgeImageAsset,
    device: MTLDevice,
    generateMips: Bool,
    overridePixelFormat: Bool = false
) -> MTLTexture? {
    guard let imageAssetData = imageAsset.data else {
        logError("no image data")
        return nil
    }
    logError(
        "imageAssetData = \(imageAssetData)  -  width = \(imageAsset.width)  -  height = \(imageAsset.height)  -  bytesPerPixel = \(imageAsset.bytesPerPixel) imageAsset.pixelFormat:  \(imageAsset.pixelFormat)"
    )

    var pixelFormat = imageAsset.pixelFormat
    if overridePixelFormat {
        switch imageAsset.bytesPerPixel {
        case 1:
            pixelFormat = .r8Unorm
        case 2:
            pixelFormat = .rg8Unorm
        case 4:
            pixelFormat = .rgba8Unorm
        default:
            pixelFormat = .rgba8Unorm
        }
    }

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

    let bytesPerRow = imageAsset.width * imageAsset.bytesPerPixel
    let bytesPerImage = bytesPerRow * imageAsset.height

    unsafe imageAssetData.bytes.withUnsafeBytes { textureBytes in
        guard let textureBytesBaseAddress = textureBytes.baseAddress else {
            return
        }
        for face in 0..<sliceCount {
            let offset = face * bytesPerImage
            let facePointer = unsafe textureBytesBaseAddress.advanced(by: offset)

            unsafe mtlTexture.replace(
                region: MTLRegionMake2D(0, 0, imageAsset.width, imageAsset.height),
                mipmapLevel: 0,
                slice: face,
                withBytes: facePointer,
                bytesPerRow: bytesPerRow,
                bytesPerImage: bytesPerImage
            )
        }
    }

    return mtlTexture
}

private func makeTextureFromImageAsset(
    _ imageAsset: WebBridgeImageAsset,
    device: MTLDevice,
    renderContext: _Proto_LowLevelRenderContext_v1,
    commandQueue: MTLCommandQueue,
    generateMips: Bool,
    overridePixelFormat: Bool,
    swizzle: MTLTextureSwizzleChannels = .init(red: .red, green: .green, blue: .blue, alpha: .alpha)
) -> _Proto_LowLevelTextureResource_v1? {
    guard
        let mtlTexture = makeMTLTextureFromImageAsset(
            imageAsset,
            device: device,
            generateMips: generateMips,
            overridePixelFormat: overridePixelFormat
        )
    else {
        logError("could not create metal texture")
        return nil
    }

    let descriptor = _Proto_LowLevelTextureResource_v1.Descriptor.from(mtlTexture, swizzle: swizzle)
    if let textureResource = try? renderContext.makeTextureResource(descriptor: descriptor) {
        guard let commandBuffer = commandQueue.makeCommandBuffer() else {
            fatalError("Could not create command buffer")
        }
        guard let blitEncoder = commandBuffer.makeBlitCommandEncoder() else {
            fatalError("Could not create blit encoder")
        }
        if generateMips {
            blitEncoder.generateMipmaps(for: mtlTexture)
        }

        let outTexture = textureResource.replace(using: commandBuffer)
        blitEncoder.copy(from: mtlTexture, to: outTexture)

        blitEncoder.endEncoding()
        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()
        return textureResource
    }

    return nil
}

private func makeParameters(
    for function: _Proto_LowLevelMaterialResource_v1.Function?,
    renderContext: _Proto_LowLevelRenderContext_v1,
    textureResources: [String: _Proto_LowLevelTextureResource_v1]
) throws -> _Proto_LowLevelArgumentTable_v1? {
    guard let function else { return nil }
    guard let argumentTableDescriptor = function.argumentTableDescriptor else { return nil }
    let parameterMapping = function.parameterMapping

    var optTextures: [_Proto_LowLevelTextureResource_v1?] = argumentTableDescriptor.textures.map({ _ in nil })
    for parameter in parameterMapping?.textures ?? [] {
        guard let textureResource = textureResources[parameter.name] else {
            fatalError("Failed to find texture resource \(parameter.name)")
        }
        optTextures[parameter.textureIndex] = textureResource
    }
    let textures = optTextures.map({ $0! })

    let buffers: [_Proto_LowLevelBufferSpan_v1] = try argumentTableDescriptor.buffers.map { bufferRequirements in
        let capacity = (bufferRequirements.size + 16 - 1) / 16 * 16
        let buffer = try renderContext.makeBufferResource(descriptor: .init(capacity: capacity))
        buffer.replace { span in
            for byteOffset in span.byteOffsets {
                span.storeBytes(of: 0, toByteOffset: byteOffset, as: UInt8.self)
            }
        }
        return try _Proto_LowLevelBufferSpan_v1(buffer: buffer, offset: 0, size: bufferRequirements.size)
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

private func logError(_ error: String) {
    Logger.modelGPU.error("\(error)")
}

private func logInfo(_ info: String) {
    Logger.modelGPU.info("\(info)")
}

extension simd_float4x4 {
    fileprivate var minor: simd_float3x3 {
        .init(
            [self.columns.0.x, self.columns.0.y, self.columns.0.z],
            [self.columns.1.x, self.columns.1.y, self.columns.1.z],
            [self.columns.2.x, self.columns.2.y, self.columns.2.z]
        )
    }
}

private class RenderTargetWrapper {
    var descriptor: _Proto_LowLevelRenderTarget_v1.Descriptor?
}

@objc
@implementation
extension WebUSDConfiguration {
    @nonobjc
    fileprivate let device: MTLDevice
    @nonobjc
    fileprivate let appRenderer: Renderer
    @nonobjc
    fileprivate final var commandQueue: MTLCommandQueue {
        get { appRenderer.commandQueue }
    }
    @nonobjc
    fileprivate final var renderer: _Proto_LowLevelRenderContext_v1 {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            appRenderer.renderContext!
        }
    }
    @nonobjc
    fileprivate final var renderWorkload: _Proto_LowLevelCameraRenderWorkload_v1 {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            appRenderer.renderWorkload!
        }
    }
    @nonobjc
    fileprivate final var renderContext: _Proto_LowLevelRenderContext_v1 {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            appRenderer.renderContext!
        }
    }
    @nonobjc
    fileprivate let renderTargetWrapper = RenderTargetWrapper()
    @nonobjc
    fileprivate final var renderTarget: _Proto_LowLevelRenderTarget_v1.Descriptor {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            renderTargetWrapper.descriptor!
        }
        set { renderTargetWrapper.descriptor = newValue }
    }

    @objc(initWithDevice:)
    init(device: MTLDevice) {
        let renderTarget = _Proto_LowLevelRenderTarget_v1.Descriptor.texture(color: .bgra8Unorm_srgb, sampleCount: 4)
        self.renderTargetWrapper.descriptor = renderTarget
        self.device = device
        do {
            self.appRenderer = try Renderer(device: device)
        } catch {
            fatalError("Exception creating renderer \(error)")
        }
    }

    @objc(createMaterialCompiler:)
    func createMaterialCompiler() async {
        do {
            try await self.appRenderer.createMaterialCompiler(renderTargetDescriptor: .texture(color: .bgra8Unorm_srgb, sampleCount: 4))
        } catch {
            fatalError("Exception creating renderer \(error)")
        }
    }
}

extension WebBridgeReceiver {
    fileprivate func configureDeformation(
        identifier: _Proto_ResourceId,
        deformationData: WebBridgeDeformationData,
        commandBuffer: MTLCommandBuffer
    ) {
        var deformers: [_Proto_LowLevelDeformerDescription_v1] = []

        if let skinningData = deformationData.skinningData {
            let skinningDeformer = skinningData.makeDeformerDescription(device: self.device)
            deformers.append(skinningDeformer)
        }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let meshResource = meshResources[identifier]!

        var inputMeshDescription: _Proto_LowLevelDeformationDescription_v1.MeshDescription?
        if self.meshResourceToDeformationContext[identifier] == nil {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let vertexPositionsBuffer = meshResource.readVertices(at: 1, using: commandBuffer)!
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let inputPositionsBuffer = device.makeBuffer(length: vertexPositionsBuffer.length, options: .storageModeShared)!

            // Copy data from vertexPositionsBuffer to inputPositionsBuffer
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let blitEncoder = commandBuffer.makeBlitCommandEncoder()!
            blitEncoder.copy(
                from: vertexPositionsBuffer,
                sourceOffset: 0,
                to: inputPositionsBuffer,
                destinationOffset: 0,
                size: vertexPositionsBuffer.length
            )
            blitEncoder.endEncoding()

            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let inputPositions = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
                inputPositionsBuffer,
                offset: 0,
                occupiedLength: inputPositionsBuffer.length,
                elementType: .float3
            )!
            inputMeshDescription = _Proto_LowLevelDeformationDescription_v1.MeshDescription(descriptions: [
                _Proto_LowLevelDeformationDescription_v1.SemanticBuffer(.position, inputPositions)
            ])
        } else {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            inputMeshDescription = self.meshResourceToDeformationContext[identifier]!.description.input
        }

        guard let inputMeshDescription else {
            logError("inputMeshDescription is unexpectedly nil")
            return
        }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let outputPositionsBuffer = meshResource.replaceVertices(at: 1, using: commandBuffer)!
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let outputPositions = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            outputPositionsBuffer,
            offset: 0,
            occupiedLength: outputPositionsBuffer.length,
            elementType: .float3
        )!

        let outputMeshDescription = _Proto_LowLevelDeformationDescription_v1.MeshDescription(descriptions: [
            _Proto_LowLevelDeformationDescription_v1.SemanticBuffer(.position, outputPositions)
        ])

        guard
            let deformationDescription =
                try? _Proto_LowLevelDeformationDescription_v1.make(
                    input: inputMeshDescription,
                    deformers: deformers,
                    output: outputMeshDescription
                )
                .get()
        else {
            logError("_Proto_LowLevelDeformationDescription_v1.make failed unexpectedly")
            return
        }

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        guard let deformation = try? self.deformationSystem.make(description: deformationDescription).get() else {
            logError("deformationSystem.make failed unexpectedly")
            return
        }

        self.meshResourceToDeformationContext[identifier] = DeformationContext.init(
            deformation: deformation,
            description: deformationDescription,
            dirty: true
        )
    }
}

@objc
@implementation
extension WebBridgeReceiver {
    @nonobjc
    fileprivate let device: MTLDevice
    @nonobjc
    fileprivate let textureProcessingContext: _Proto_LowLevelTextureProcessingContext_v1
    @nonobjc
    fileprivate let commandQueue: MTLCommandQueue

    @nonobjc
    fileprivate let renderContext: _Proto_LowLevelRenderContext_v1
    @nonobjc
    fileprivate let renderWorkload: _Proto_LowLevelCameraRenderWorkload_v1
    @nonobjc
    fileprivate let appRenderer: Renderer
    @nonobjc
    fileprivate let lightingFunction: _Proto_LowLevelMaterialResource_v1.LightingFunction
    @nonobjc
    fileprivate let lightingArguments: _Proto_LowLevelArgumentTable_v1
    @nonobjc
    fileprivate var lightingArgumentBuffer: _Proto_LowLevelArgumentTable_v1?

    @nonobjc
    private let renderTargetWrapper = RenderTargetWrapper()
    @nonobjc
    private final var renderTarget: _Proto_LowLevelRenderTarget_v1.Descriptor {
        get {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            renderTargetWrapper.descriptor!
        }
        set { renderTargetWrapper.descriptor = newValue }
    }
    @nonobjc
    fileprivate var meshInstancePlainArray: [_Proto_LowLevelMeshInstance_v1?]
    @nonobjc
    fileprivate var meshInstances: _Proto_LowLevelMeshInstanceArray_v1

    @nonobjc
    fileprivate var meshResources: [_Proto_ResourceId: _Proto_LowLevelMeshResource_v1] = [:]
    @nonobjc
    fileprivate var meshResourceToMaterials: [_Proto_ResourceId: [_Proto_ResourceId]] = [:]
    @nonobjc
    fileprivate var meshToMeshInstances: [_Proto_ResourceId: [_Proto_LowLevelMeshInstance_v1]] = [:]
    @nonobjc
    fileprivate var meshTransforms: [_Proto_ResourceId: [simd_float4x4]] = [:]
    @nonobjc
    fileprivate var rotationAngle: Float = 0

    @nonobjc
    fileprivate let deformationSystem: _Proto_LowLevelDeformationSystem_v1

    struct DeformationContext {
        let deformation: _Proto_Deformation_v1
        var description: _Proto_LowLevelDeformationDescription_v1
        var dirty: Bool
    }
    @nonobjc
    fileprivate var meshResourceToDeformationContext: [_Proto_ResourceId: DeformationContext] = [:]

    struct Material {
        let resource: _Proto_LowLevelMaterialResource_v1
        let geometryArguments: _Proto_LowLevelArgumentTable_v1?
        let surfaceArguments: _Proto_LowLevelArgumentTable_v1?
    }
    @nonobjc
    fileprivate var materialsAndParams: [_Proto_ResourceId: Material] = [:]

    @nonobjc
    fileprivate var textureResources: [String: _Proto_LowLevelTextureResource_v1] = [:]

    @nonobjc
    fileprivate var modelTransform: simd_float4x4
    @nonobjc
    fileprivate var modelDistance: Float

    @nonobjc
    fileprivate var dontCaptureAgain: Bool = false

    init(
        configuration: WebUSDConfiguration,
        diffuseAsset: WebBridgeImageAsset,
        specularAsset: WebBridgeImageAsset
    ) throws {
        self.renderContext = configuration.renderContext
        self.renderWorkload = configuration.renderWorkload
        self.appRenderer = configuration.appRenderer
        self.device = configuration.device
        self.textureProcessingContext = _Proto_LowLevelTextureProcessingContext_v1(device: configuration.device)
        self.commandQueue = configuration.commandQueue
        self.deformationSystem = try _Proto_LowLevelDeformationSystem_v1.make(configuration.device, configuration.commandQueue).get()
        self.renderTargetWrapper.descriptor = configuration.renderTargetWrapper.descriptor
        modelTransform = matrix_identity_float4x4
        modelDistance = 1.0
        self.meshInstancePlainArray = []
        let meshInstances = try configuration.renderContext.makeMeshInstanceArray(renderTargets: [configuration.renderTarget], count: 16)
        let lightingFunction = configuration.renderContext.makePhysicallyBasedLightingFunction()
        guard
            let diffuseTexture = makeTextureFromImageAsset(
                diffuseAsset,
                device: device,
                renderContext: renderContext,
                commandQueue: configuration.commandQueue,
                generateMips: true,
                overridePixelFormat: false,
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
                overridePixelFormat: false,
                swizzle: .init(red: .red, green: .red, blue: .red, alpha: .one)
            )
        else {
            fatalError("Could not create specularTexture")
        }
        self.meshInstances = meshInstances
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
    }

    @objc(renderWithTexture:)
    func render(with texture: MTLTexture) {
        for (identifier, meshes) in meshToMeshInstances {
            let originalTransforms = meshTransforms[identifier]
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let angle: Float = 0.707
            let rotationY90 = simd_float4x4(
                simd_float4(angle, 0, angle, 0), // column 0
                simd_float4(0, 1, 0, 0), // column 1
                simd_float4(-angle, 0, angle, 0), // column 2
                simd_float4(0, 0, 0, 1) // column 3
            )

            for (index, meshInstance) in meshes.enumerated() {
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                let computedTransform = modelTransform * rotationY90 * originalTransforms![index]
                meshInstance.setTransform(.single(computedTransform))
            }
        }

        // animate
        if !meshResourceToDeformationContext.isEmpty {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let commandBuffer = self.commandQueue.makeCommandBuffer()!

            for (identifier, deformationContext) in meshResourceToDeformationContext where deformationContext.dirty {
                deformationContext.deformation.execute(deformation: deformationContext.description, commandBuffer: commandBuffer) {
                    (commandBuffer: any MTLCommandBuffer) in
                }
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                meshResourceToDeformationContext[identifier]!.dirty = false
            }

            commandBuffer.enqueue()
            commandBuffer.commit()
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
            try appRenderer.render(meshInstances: meshInstances, texture: texture)
        } catch {
            logError("failed to start gpu capture \(error)")
        }

        let captureManager = MTLCaptureManager.shared()
        if captureManager.isCapturing {
            captureManager.stopCapture()
        }
    }

    @objc(updateTexture:)
    func updateTexture(_ data: WebBridgeUpdateTexture) {
        guard let asset = data.imageAsset else {
            logError("Image asset was nil")
            return
        }

        let textureHash = data.hashString
        if textureResources[textureHash] != nil {
            logError("Texture already exists")
            return
        }

        let commandQueue = appRenderer.commandQueue
        if let textureResource = makeTextureFromImageAsset(
            asset,
            device: device,
            renderContext: renderContext,
            commandQueue: commandQueue,
            generateMips: true,
            overridePixelFormat: false
        ) {
            textureResources[textureHash] = textureResource
        }
    }

    @objc(updateMaterial:completionHandler:)
    func updateMaterial(_ data: WebBridgeUpdateMaterial) async {
        logInfo("updateMaterial (pre-dispatch) \(data.identifier)")
        do {
            let identifier = data.identifier
            logInfo("updateMaterial \(identifier)")

            guard let materialSourceArchive = data.materialGraph else {
                logError("No materialGraph data provided for material \(identifier)")
                return
            }

            let shaderGraphFunctions = try await renderContext.makeShaderGraphFunctions(data.materialGraph)

            let geometryArguments = try makeParameters(
                for: shaderGraphFunctions.geometryModifier,
                renderContext: renderContext,
                textureResources: textureResources
            )
            let surfaceArguments = try makeParameters(
                for: shaderGraphFunctions.surfaceShader,
                renderContext: renderContext,
                textureResources: textureResources
            )

            let geometryModifier = shaderGraphFunctions.geometryModifier ?? renderContext.makeDefaultGeometryModifier()
            let surfaceShader = shaderGraphFunctions.surfaceShader
            let materialResource = try await renderContext.makeMaterialResource(
                descriptor: .init(
                    geometry: geometryModifier,
                    surface: surfaceShader,
                    lighting: lightingFunction
                )
            )
            materialsAndParams[identifier] = .init(
                resource: materialResource,
                geometryArguments: geometryArguments,
                surfaceArguments: surfaceArguments
            )
        } catch {
            logError("updateMaterial failed \(error)")
        }
    }

    @objc(updateMesh:completionHandler:)
    func updateMesh(_ data: WebBridgeUpdateMesh) async {
        let identifier = data.identifier
        logInfo("(update mesh) \(identifier) Material ids \(data.materialPrims)")

        do {
            let identifier = data.identifier

            let meshResource: _Proto_LowLevelMeshResource_v1
            if data.updateType == .initial || data.descriptor != nil {
                let meshDescriptor = data.descriptor!
                let descriptor = _Proto_LowLevelMeshResource_v1.Descriptor.fromLlmDescriptor(meshDescriptor)
                meshResource = try renderContext.makeMeshResource(descriptor: descriptor)
                meshResource.replaceData(indexData: data.indexData, vertexData: data.vertexData)
                meshResources[identifier] = meshResource
            } else {
                guard let cachedMeshResource = meshResources[identifier] else {
                    fatalError("Mesh resource should already be created from previous update")
                }

                if data.indexData != nil || !data.vertexData.isEmpty {
                    cachedMeshResource.replaceData(indexData: data.indexData, vertexData: data.vertexData)
                }
                meshResource = cachedMeshResource
            }

            if let deformationData = data.deformationData {
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                // swift-format-ignore: NeverForceUnwrap
                let commandBuffer = self.commandQueue.makeCommandBuffer()!
                // TODO: delta update
                configureDeformation(identifier: identifier, deformationData: deformationData, commandBuffer: commandBuffer)
                commandBuffer.enqueue()
                commandBuffer.commit()
            }

            if data.instanceTransformsCount > 0 {
                // Make new instances
                if meshToMeshInstances[identifier] == nil {
                    meshToMeshInstances[identifier] = []
                    meshTransforms[identifier] = []

                    for (partIndex, _) in data.parts.enumerated() {
                        let materialIdentifier = data.materialPrims[partIndex]
                        guard let material = materialsAndParams[materialIdentifier] else {
                            fatalError("Failed to get material instance \(materialIdentifier)")
                        }

                        let pipeline = try await renderContext.makeRenderPipelineState(
                            descriptor: .descriptor(
                                mesh: meshResource.descriptor,
                                material: material.resource,
                                renderTargets: [renderTarget]
                            )
                        )

                        let meshPart = try renderContext.makeMeshPart(
                            resource: meshResource,
                            indexOffset: data.parts[partIndex].indexOffset,
                            indexCount: data.parts[partIndex].indexCount,
                            primitive: data.parts[partIndex].topology,
                            windingOrder: .counterClockwise,
                            boundsMin: -.one,
                            boundsMax: .one
                        )

                        for instanceTransform in data.instanceTransforms {
                            let meshInstance = try renderContext.makeMeshInstance(
                                meshPart: meshPart,
                                pipeline: pipeline,
                                geometryArguments: material.geometryArguments,
                                surfaceArguments: material.surfaceArguments,
                                lightingArguments: lightingArguments,
                                transform: .single(instanceTransform),
                                category: .opaque
                            )

                            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                            // swift-format-ignore: NeverForceUnwrap
                            meshToMeshInstances[identifier]!.append(meshInstance)
                            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                            // swift-format-ignore: NeverForceUnwrap
                            meshTransforms[identifier]!.append(instanceTransform)

                            let meshInstanceIndex = meshInstancePlainArray.count
                            meshInstancePlainArray.append(meshInstance)
                            if meshInstances.count < meshInstancePlainArray.count {
                                let meshInstances = try renderContext.makeMeshInstanceArray(
                                    renderTargets: [renderTarget],
                                    count: meshInstances.count * 2
                                )
                                for index in meshInstancePlainArray.indices {
                                    try meshInstances.setMeshInstance(meshInstancePlainArray[index], index: index)
                                }
                                self.meshInstances = meshInstances
                            } else {
                                try meshInstances.setMeshInstance(meshInstance, index: meshInstanceIndex)
                            }
                        }
                    }
                } else {
                    // Update transforms otherwise

                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                    // swift-format-ignore: NeverForceUnwrap
                    let partCount = meshToMeshInstances[identifier]!.count / data.instanceTransforms.count
                    for (instanceIndex, instanceTransform) in data.instanceTransforms.enumerated() {
                        for partIndex in 0..<partCount {
                            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                            // swift-format-ignore: NeverForceUnwrap
                            let meshInstance = meshToMeshInstances[identifier]![instanceIndex * data.parts.count + partIndex]
                            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
                            // swift-format-ignore: NeverForceUnwrap
                            meshTransforms[identifier]![instanceIndex * data.parts.count + partIndex] = instanceTransform
                        }
                    }
                }
            }

            if !data.materialPrims.isEmpty {
                meshResourceToMaterials[identifier] = data.materialPrims
            }
        } catch {
            logError(error.localizedDescription)
        }
    }

    @objc(setTransform:)
    func setTransform(_ transform: simd_float4x4) {
        modelTransform = transform
    }

    @objc
    func setCameraDistance(_ distance: Float) {
        modelDistance = distance
        appRenderer.setCameraDistance(modelDistance)
    }

    @objc
    func setPlaying(_ play: Bool) {
        // resourceContext.setEnableModelRotation(play)
    }

    @objc
    func setEnvironmentMap(_ imageAsset: WebBridgeImageAsset) {
        do {
            guard let mtlTextureEquirectangular = makeMTLTextureFromImageAsset(imageAsset, device: device, generateMips: true) else {
                fatalError("Could not make metal texture from environment asset data")
            }

            let cubeMTLTextureDescriptor = try self.textureProcessingContext.createCubeDescriptor(
                fromEquirectangular: mtlTextureEquirectangular
            )
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let cubeMTLTexture = self.device.makeTexture(descriptor: cubeMTLTextureDescriptor)!

            let diffuseMTLTextureDescriptor = try self.textureProcessingContext.createImageBasedLightDiffuseDescriptor(
                fromCube: cubeMTLTexture
            )
            let diffuseTextureDescriptor = _Proto_LowLevelTextureResource_v1.Descriptor.from(diffuseMTLTextureDescriptor)
            let diffuseTexture = try self.renderContext.makeTextureResource(descriptor: diffuseTextureDescriptor)

            let specularMTLTextureDescriptor = try self.textureProcessingContext.createImageBasedLightSpecularDescriptor(
                fromCube: cubeMTLTexture
            )
            let specularTextureDescriptor = _Proto_LowLevelTextureResource_v1.Descriptor.from(specularMTLTextureDescriptor)
            let specularTexture = try self.renderContext.makeTextureResource(descriptor: specularTextureDescriptor)

            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
            // swift-format-ignore: NeverForceUnwrap
            let commandBuffer = self.commandQueue.makeCommandBuffer()!

            try self.textureProcessingContext.generateCube(
                using: commandBuffer,
                fromEquirectangular: mtlTextureEquirectangular,
                into: cubeMTLTexture
            )

            let diffuseMTLTexture = diffuseTexture.replace(using: commandBuffer)
            let specularMTLTexture = specularTexture.replace(using: commandBuffer)

            try self.textureProcessingContext.generateImageBasedLightDiffuse(
                using: commandBuffer,
                fromSkyboxCube: cubeMTLTexture,
                into: diffuseMTLTexture
            )
            try self.textureProcessingContext.generateImageBasedLightSpecular(
                using: commandBuffer,
                fromSkyboxCube: cubeMTLTexture,
                into: specularMTLTexture
            )

            try self.lightingArguments.setTexture(at: 0, diffuseTexture)
            try self.lightingArguments.setTexture(at: 1, specularTexture)

            commandBuffer.commit()
        } catch {
            fatalError(error.localizedDescription)
        }
    }
}

private func webPartsFromParts(_ parts: [LowLevelMesh.Part]) -> [WebBridgeMeshPart] {
    parts.map({ a in
        WebBridgeMeshPart(
            indexOffset: a.indexOffset,
            indexCount: a.indexCount,
            topology: a.topology,
            materialIndex: a.materialIndex,
            boundsMin: a.bounds.min,
            boundsMax: a.bounds.max
        )
    })
}

private func convert(_ m: _Proto_DataUpdateType_v1) -> WebBridgeDataUpdateType {
    if m == .initial {
        return .initial
    }
    return .delta
}

private func webUpdateTextureRequestFromUpdateTextureRequest(_ request: _Proto_TextureDataUpdate_v1) -> WebBridgeUpdateTexture {
    // FIXME: remove placeholder code
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
    // swift-format-ignore: NeverForceUnwrap
    let descriptor = request.descriptor!
    let data = request.data
    return WebBridgeUpdateTexture(
        imageAsset: .init(descriptor, data: data),
        identifier: request.identifier,
        hashString: request.hashString
    )
}

private func webUpdateMeshRequestFromUpdateMeshRequest(
    _ request: _Proto_MeshDataUpdate_v1
) -> WebBridgeUpdateMesh {
    var descriptor: WebBridgeMeshDescriptor?
    if let requestDescriptor = request.descriptor {
        descriptor = .init(request: requestDescriptor)
    }

    return WebBridgeUpdateMesh(
        identifier: request.identifier,
        updateType: convert(request.updateType),
        descriptor: descriptor,
        parts: webPartsFromParts(request.parts),
        indexData: request.indexData,
        vertexData: request.vertexData,
        instanceTransforms: toData(request.instanceTransformsCompat()),
        instanceTransformsCount: request.instanceTransformsCompat().count,
        materialPrims: request.materialPrims,
        deformationData: .init(request.deformationData)
    )
}

nonisolated func webUpdateMaterialRequestFromUpdateMaterialRequest(
    _ request: _Proto_MaterialDataUpdate_v1
) -> WebBridgeUpdateMaterial {
    WebBridgeUpdateMaterial(
        materialGraph: request.materialSourceArchive,
        identifier: request.identifier,
        geometryModifierFunctionReference: nil,
        surfaceShaderFunctionReference: nil,
        shaderGraphModule: nil,
    )
}

final class USDModelLoader: _Proto_UsdStageSession_v1.Delegate {
    fileprivate let usdLoader: _Proto_UsdStageSession_v1
    fileprivate var stage: UsdStage?
    fileprivate var data: Data?
    private let objcLoader: WebBridgeModelLoader

    @nonobjc
    private let dispatchSerialQueue: DispatchSerialQueue

    @nonobjc
    fileprivate var time: TimeInterval = 0

    @nonobjc
    fileprivate var startTime: TimeInterval = 0
    @nonobjc
    fileprivate var endTime: TimeInterval = 1
    @nonobjc
    fileprivate var timeCodePerSecond: TimeInterval = 1

    init(objcInstance: WebBridgeModelLoader) {
        objcLoader = objcInstance
        usdLoader = _Proto_UsdStageSession_v1.noMetalSession(gpuFamily: MTLGPUFamily.apple5)
        dispatchSerialQueue = DispatchSerialQueue(label: "USDModelWebProcess", qos: .userInteractive)
        usdLoader.delegate = self
    }

    func iblTextureUpdated(data: consuming sending _Proto_TextureDataUpdate_v1) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=299480 - [Model element] Support `environmentMap` attribute in GPU process model element
    }

    func iblTextureDestroyed(identifier: _Proto_ResourceId) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=303906
    }

    func meshUpdated(data: consuming sending _Proto_MeshDataUpdate_v1) {
        let identifier = data.identifier
        self.dispatchSerialQueue.async {
            self.objcLoader.updateMesh(webRequest: webUpdateMeshRequestFromUpdateMeshRequest(data))
        }
    }

    func meshDestroyed(identifier: String) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=303906
    }

    func materialUpdated(data: consuming sending _Proto_MaterialDataUpdate_v1) {
        let identifier = data.identifier
        self.dispatchSerialQueue.async {
            self.objcLoader.updateMaterial(webRequest: webUpdateMaterialRequestFromUpdateMaterialRequest(data))
        }
    }

    func materialDestroyed(identifier: String) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=303906
    }

    func textureUpdated(data: consuming sending _Proto_TextureDataUpdate_v1) {
        let identifier = data.identifier
        self.dispatchSerialQueue.async {
            self.objcLoader.updateTexture(webRequest: webUpdateTextureRequestFromUpdateTextureRequest(data))
        }
    }

    func textureDestroyed(identifier: String) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=303906
    }

    func loadModel(from url: Foundation.URL) {
        do {
            let stage = try UsdStage(contentsOf: url)
            self.timeCodePerSecond = stage.timeCodesPerSecond
            self.startTime = stage.startTimeCode
            self.endTime = stage.endTimeCode
            self.usdLoader.loadStage(stage)
        } catch {
            fatalError(error.localizedDescription)
        }
    }

    func loadModel(data: Foundation.Data) {
        do {
            self.data = data
            // swift-format-ignore: NeverForceUnwrap
            self.stage = try UsdStage.open(buffer: self.data!)
            guard let stage = self.stage else {
                logError("model data is corrupted")
                return
            }
            self.timeCodePerSecond = stage.timeCodesPerSecond
            self.startTime = stage.startTimeCode
            self.endTime = stage.endTimeCode
            self.usdLoader.loadStage(stage)
        } catch {
            fatalError(error.localizedDescription)
        }
    }

    func duration() -> Double {
        if timeCodePerSecond > 0 {
            return (endTime - startTime) / timeCodePerSecond
        }

        return 0.0
    }

    func currentTime() -> Double {
        time - startTime
    }

    func loadModel(from data: Data) {
    }

    func update(deltaTime: TimeInterval) {
        usdLoader.update(time: time)

        time = fmod(deltaTime * self.timeCodePerSecond + time - startTime, max(endTime - startTime, 1)) + startTime
    }
}

@objc
@implementation
extension WebBridgeModelLoader {
    @nonobjc
    var loader: USDModelLoader?
    @nonobjc
    var modelUpdated: ((WebBridgeUpdateMesh) -> (Void))?
    @nonobjc
    var textureUpdatedCallback: ((WebBridgeUpdateTexture) -> (Void))?
    @nonobjc
    var materialUpdatedCallback: ((WebBridgeUpdateMaterial) -> (Void))?

    @nonobjc
    fileprivate var retainedRequests: Set<NSObject> = []

    override init() {
        super.init()

        self.loader = USDModelLoader(objcInstance: self)
    }

    @objc(
        setCallbacksWithModelUpdatedCallback:
        textureUpdatedCallback:
        materialUpdatedCallback:
    )
    func setCallbacksWithModelUpdatedCallback(
        _ modelUpdatedCallback: @escaping ((WebBridgeUpdateMesh) -> (Void)),
        textureUpdatedCallback: @escaping ((WebBridgeUpdateTexture) -> (Void)),
        materialUpdatedCallback: @escaping ((WebBridgeUpdateMaterial) -> (Void))
    ) {
        self.modelUpdated = modelUpdatedCallback
        self.textureUpdatedCallback = textureUpdatedCallback
        self.materialUpdatedCallback = materialUpdatedCallback
    }

    @objc
    func loadModel(from url: Foundation.URL) {
        self.loader?.loadModel(from: url)
    }

    @objc
    func loadModel(_ data: Foundation.Data) {
        self.loader?.loadModel(data: data)
    }

    @objc
    func update(_ deltaTime: Double) {
        self.loader?.update(deltaTime: deltaTime)
    }

    @objc
    func requestCompleted(_ request: NSObject) {
        retainedRequests.remove(request)
    }

    @objc
    func duration() -> Double {
        guard let loader else {
            return 0.0
        }
        return loader.duration()
    }

    @objc
    func currentTime() -> Double {
        guard let loader else {
            return 0.0
        }
        return loader.currentTime()
    }

    fileprivate func updateMesh(webRequest: WebBridgeUpdateMesh) {
        if let modelUpdated {
            retainedRequests.insert(webRequest)
            modelUpdated(webRequest)
        }
    }

    fileprivate func updateTexture(webRequest: WebBridgeUpdateTexture) {
        if let textureUpdatedCallback {
            retainedRequests.insert(webRequest)
            textureUpdatedCallback(webRequest)
        }
    }

    fileprivate func updateMaterial(webRequest: WebBridgeUpdateMaterial) {
        if let materialUpdatedCallback {
            retainedRequests.insert(webRequest)
            materialUpdatedCallback(webRequest)
        }
    }
}

extension WebBridgeSkinningData {
    fileprivate func makeDeformerDescription(device: MTLDevice) -> _Proto_LowLevelDeformerDescription_v1 {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointTransformsBuffer = device.makeBuffer(
            bytes: self.jointTransforms,
            length: self.jointTransforms.count * MemoryLayout<simd_float4x4>.size,
            options: .storageModeShared
        )!

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointTransformsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            jointTransformsBuffer,
            offset: 0,
            occupiedLength: jointTransformsBuffer.length,
            elementType: .float4x4
        )!

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let inverseBindPosesBuffer = device.makeBuffer(
            bytes: self.inverseBindPoses,
            length: self.inverseBindPoses.count * MemoryLayout<simd_float4x4>.size,
            options: .storageModeShared
        )!

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let inverseBindPosesDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            inverseBindPosesBuffer,
            offset: 0,
            occupiedLength: inverseBindPosesBuffer.length,
            elementType: .float4x4
        )!

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointIndicesBuffer = device.makeBuffer(
            bytes: self.influenceJointIndices,
            length: self.influenceJointIndices.count * MemoryLayout<UInt32>.size,
            options: .storageModeShared
        )!
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let jointIndicesDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            jointIndicesBuffer,
            offset: 0,
            occupiedLength: jointIndicesBuffer.length,
            elementType: .uint
        )!

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let influenceWeightsBuffer = device.makeBuffer(
            bytes: self.influenceWeights,
            length: self.influenceWeights.count * MemoryLayout<Float>.size,
            options: .storageModeShared
        )!
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=305857
        // swift-format-ignore: NeverForceUnwrap
        let influenceWeightsDescription = _Proto_LowLevelDeformationDescription_v1.Buffer.make(
            influenceWeightsBuffer,
            offset: 0,
            occupiedLength: influenceWeightsBuffer.length,
            elementType: .float
        )!

        let deformerDescription = _Proto_LowLevelSkinningDescription_v1(
            jointTransforms: jointTransformsDescription,
            inverseBindPoses: inverseBindPosesDescription,
            influenceJointIndices: jointIndicesDescription,
            influenceWeights: influenceWeightsDescription,
            geometryBindTransform: self.geometryBindTransform,
            influencePerVertexCount: self.influencePerVertexCount
        )

        return deformerDescription
    }
}

#else
@objc
@implementation
extension WebUSDConfiguration {
    init(device: MTLDevice) {
    }

    @objc(createMaterialCompiler:)
    func createMaterialCompiler() async {
    }
}

@objc
@implementation
extension WebBridgeReceiver {
    init(
        configuration: WebUSDConfiguration,
        diffuseAsset: WebBridgeImageAsset,
        specularAsset: WebBridgeImageAsset
    ) throws {
    }

    @objc(renderWithTexture:)
    func render(with texture: MTLTexture) {
    }

    @objc(updateTexture:)
    func updateTexture(_ data: WebBridgeUpdateTexture) {
    }

    @objc(updateMaterial:completionHandler:)
    func updateMaterial(_ data: WebBridgeUpdateMaterial) async {
    }

    @objc(updateMesh:completionHandler:)
    func updateMesh(_ data: WebBridgeUpdateMesh) async {
    }

    @objc(setTransform:)
    func setTransform(_ transform: simd_float4x4) {
    }

    @objc
    func setCameraDistance(_ distance: Float) {
    }

    @objc
    func setPlaying(_ play: Bool) {
    }

    @objc
    func setEnvironmentMap(_ imageAsset: WebBridgeImageAsset) {
    }
}

@objc
@implementation
extension WebBridgeModelLoader {
    override init() {
        super.init()
    }

    @objc(
        setCallbacksWithModelUpdatedCallback:
        textureUpdatedCallback:
        materialUpdatedCallback:
    )
    func setCallbacksWithModelUpdatedCallback(
        _ modelUpdatedCallback: @escaping ((WebBridgeUpdateMesh) -> (Void)),
        textureUpdatedCallback: @escaping ((WebBridgeUpdateTexture) -> (Void)),
        materialUpdatedCallback: @escaping ((WebBridgeUpdateMaterial) -> (Void))
    ) {
    }

    @objc
    func loadModel(from url: Foundation.URL) {
    }

    @objc
    func loadModel(_ data: Foundation.Data) {
    }

    @objc
    func update(_ deltaTime: Double) {
    }

    @objc
    func requestCompleted(_ request: NSObject) {
    }

    @objc
    func duration() -> Double {
        0.0
    }

    @objc
    func currentTime() -> Double {
        0.0
    }
}
#endif
