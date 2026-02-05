/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#import <wtf/Platform.h>

#ifdef __cplusplus
#import <WebGPU/Float3.h>
#import <WebGPU/Float4x4.h>
#import <simd/simd.h>
#import <wtf/ExportMacros.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>
#endif

typedef struct __IOSurface *IOSurfaceRef;

#if ENABLE(WEBGPU_SWIFT)
#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

enum class WebBridgeDataUpdateType : uint8_t {
    kInitial,
    kDelta
};

@interface WebBridgeVertexAttributeFormat : NSObject

@property (nonatomic, readonly) long semantic;
@property (nonatomic, readonly) unsigned long format;
@property (nonatomic, readonly) long layoutIndex;
@property (nonatomic, readonly) long offset;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSemantic:(long)semantic format:(unsigned long)format layoutIndex:(long)layoutIndex offset:(long)offset NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeVertexLayout : NSObject

@property (nonatomic, readonly) long bufferIndex;
@property (nonatomic, readonly) long bufferOffset;
@property (nonatomic, readonly) long bufferStride;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(long)bufferIndex bufferOffset:(long)bufferOffset bufferStride:(long)bufferStride NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeMeshPart : NSObject

@property (nonatomic, readonly) long indexOffset;
@property (nonatomic, readonly) long indexCount;
@property (nonatomic, readonly) MTLPrimitiveType topology;
@property (nonatomic, readonly) long materialIndex;
@property (nonatomic, readonly) simd_float3 boundsMin;
@property (nonatomic, readonly) simd_float3 boundsMax;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexOffset:(long)indexOffset indexCount:(long)indexCount topology:(MTLPrimitiveType)topology materialIndex:(long)materialIndex boundsMin:(simd_float3)boundsMin boundsMax:(simd_float3)boundsMax NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeMeshDescriptor : NSObject

@property (nonatomic, readonly) long vertexBufferCount;
@property (nonatomic, readonly) long vertexCapacity;
@property (nonatomic, readonly) NSArray<WebBridgeVertexAttributeFormat *> *vertexAttributes;
@property (nonatomic, readonly) NSArray<WebBridgeVertexLayout *> *vertexLayouts;
@property (nonatomic, readonly) long indexCapacity;
@property (nonatomic, readonly) MTLIndexType indexType;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithVertexBufferCount:(long)vertexBufferCount vertexCapacity:(long)vertexCapacity vertexAttributes:(NSArray<WebBridgeVertexAttributeFormat *> *)vertexAttributes vertexLayouts:(NSArray<WebBridgeVertexLayout *> *)vertexLayouts indexCapacity:(long)indexCapacity indexType:(MTLIndexType)indexType NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeSkinningData : NSObject

@property (nonatomic, readonly) uint8_t influencePerVertexCount;
@property (nonatomic, readonly, nullable) NSData *jointTransformsData; // [simd_float4x4]
@property (nonatomic, readonly, nullable) NSData *inverseBindPosesData; // [simd_float4x4]
@property (nonatomic, readonly, nullable) NSData *influenceJointIndicesData; // [UInt32]
@property (nonatomic, readonly, nullable) NSData *influenceWeightsData; // [Float]
@property (nonatomic, readonly) simd_float4x4 geometryBindTransform;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithInfluencePerVertexCount:(uint8_t)influencePerVertexCount jointTransforms:(nullable NSData *)jointTransforms inverseBindPoses:(nullable NSData *)inverseBindPoses influenceJointIndices:(nullable NSData *)influenceJointIndices influenceWeights:(nullable NSData *)influenceWeights geometryBindTransform:(simd_float4x4)geometryBindTransform NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeBlendShapeData : NSObject

@property (nonatomic, readonly) NSData *weights; // [Float]
@property (nonatomic, readonly) NSArray<NSData *> *positionOffsets; // [[SIMD3<Float>]]
@property (nonatomic, readonly) NSArray<NSData *> *normalOffsets; // [[SIMD3<Float>]]

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWeights:(NSData *)weights positionOffsets:(NSArray<NSData *> *)positionOffsets normalOffsets:(NSArray<NSData *> *)normalOffsets NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeRenormalizationData : NSObject

@property (nonatomic, readonly) NSData *vertexIndicesPerTriangle; // [UInt32]
@property (nonatomic, readonly) NSData *vertexAdjacencies; // [UInt32]
@property (nonatomic, readonly) NSData *vertexAdjacencyEndIndices; // [UInt32]

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithVertexIndicesPerTriangle:(NSData *)vertexIndicesPerTriangle vertexAdjacencies:(NSData *)vertexAdjacencies vertexAdjacencyEndIndices:(NSData *)vertexAdjacencyEndIndices NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeDeformationData : NSObject

@property (nonatomic, readonly, nullable) WebBridgeSkinningData *skinningData;
@property (nonatomic, readonly, nullable) WebBridgeBlendShapeData *blendShapeData;
@property (nonatomic, readonly, nullable) WebBridgeRenormalizationData *renormalizationData;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSkinningData:(nullable WebBridgeSkinningData *)skinningData blendShapeData:(nullable WebBridgeBlendShapeData *)blendShapeData renormalizationData:(nullable WebBridgeRenormalizationData *)renormalizationData NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeUpdateMesh : NSObject

@property (nonatomic, readonly) NSString *identifier;
@property (nonatomic, readonly) WebBridgeDataUpdateType updateType;
@property (nonatomic, strong, readonly, nullable) WebBridgeMeshDescriptor *descriptor;
@property (nonatomic, strong, readonly) NSArray<WebBridgeMeshPart*> *parts;
@property (nonatomic, strong, readonly, nullable) NSData *indexData;
@property (nonatomic, strong, readonly) NSArray<NSData *> *vertexData;
@property (nonatomic, strong, readonly, nullable) NSData *instanceTransformsData;
@property (nonatomic, readonly) long instanceTransformsCount;
@property (nonatomic, strong, readonly) NSArray<NSString *> *materialPrims;
@property (nonatomic, strong, readonly, nullable) WebBridgeDeformationData *deformationData;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithIdentifier:(NSString *)identifier
    updateType:(WebBridgeDataUpdateType)updateType
    descriptor:(nullable WebBridgeMeshDescriptor *)descriptor
    parts:(NSArray<WebBridgeMeshPart*> *)parts
    indexData:(nullable NSData *)indexData
    vertexData:(NSArray<NSData *> *)vertexData
    instanceTransforms:(nullable NSData *)instanceTransforms
    instanceTransformsCount:(long)instanceTransformsCount
    materialPrims:(NSArray<NSString *> *)materialPrims
    deformationData:(nullable WebBridgeDeformationData *)deformationData NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, WebBridgeSemantic) {
    WebBridgeSemanticColor,
    WebBridgeSemanticVector,
    WebBridgeSemanticScalar,
    WebBridgeSemanticUnknown
};

@interface WebBridgeImageAsset : NSObject

@property (nonatomic, nullable, strong, readonly) NSData *data;
@property (nonatomic, readonly) long width;
@property (nonatomic, readonly) long height;
@property (nonatomic, readonly) long depth;
@property (nonatomic, readonly) long bytesPerPixel;
@property (nonatomic, readonly) MTLTextureType textureType;
@property (nonatomic, readonly) MTLPixelFormat pixelFormat;
@property (nonatomic, readonly) long mipmapLevelCount;
@property (nonatomic, readonly) long arrayLength;
@property (nonatomic, readonly) MTLTextureUsage textureUsage;
@property (nonatomic, readonly) MTLTextureSwizzleChannels swizzle;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithData:(nullable NSData *)data width:(long)width height:(long)height depth:(long)depth bytesPerPixel:(long)bytesPerPixel textureType:(MTLTextureType)textureType pixelFormat:(MTLPixelFormat)pixelFormat mipmapLevelCount:(long)mipmapLevelCount arrayLength:(long)arrayLength textureUsage:(MTLTextureUsage)textureUsage swizzle:(MTLTextureSwizzleChannels)swizzle NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeUpdateTexture : NSObject

@property (nonatomic, readonly, strong, nullable) WebBridgeImageAsset *imageAsset;
@property (nonatomic, readonly, strong) NSString *identifier;
@property (nonatomic, readonly, strong) NSString *hashString;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithImageAsset:(nullable WebBridgeImageAsset *)imageAsset identifier:(NSString *)identifier hashString:(NSString *)hashString NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeEdge : NSObject

@property (nonatomic, readonly) long upstreamNodeIndex;
@property (nonatomic, readonly) long downstreamNodeIndex;
@property (nonatomic, readonly) NSString *upstreamOutputName;
@property (nonatomic, readonly) NSString *downstreamInputName;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithUpstreamNodeIndex:(long)upstreamNodeIndex
    downstreamNodeIndex:(long)downstreamNodeIndex
    upstreamOutputName:(NSString *)upstreamOutputName
    downstreamInputName:(NSString *)downstreamInputName NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, WebBridgeDataType) {
    WebBridgeDataTypeBool,
    WebBridgeDataTypeInt,
    WebBridgeDataTypeInt2,
    WebBridgeDataTypeInt3,
    WebBridgeDataTypeInt4,
    WebBridgeDataTypeFloat,
    WebBridgeDataTypeColor3f,
    WebBridgeDataTypeColor3h,
    WebBridgeDataTypeColor4f,
    WebBridgeDataTypeColor4h,
    WebBridgeDataTypeFloat2,
    WebBridgeDataTypeFloat3,
    WebBridgeDataTypeFloat4,
    WebBridgeDataTypeHalf,
    WebBridgeDataTypeHalf2,
    WebBridgeDataTypeHalf3,
    WebBridgeDataTypeHalf4,
    WebBridgeDataTypeMatrix2f,
    WebBridgeDataTypeMatrix3f,
    WebBridgeDataTypeMatrix4f,
    WebBridgeDataTypeSurfaceShader,
    WebBridgeDataTypeGeometryModifier,
    WebBridgeDataTypeString,
    WebBridgeDataTypeToken,
    WebBridgeDataTypeAsset
};

@interface WebBridgeInputOutput : NSObject

@property (nonatomic, readonly) WebBridgeDataType type;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithType:(WebBridgeDataType)dataType name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, WebBridgeConstant) {
    WebBridgeConstantBool,
    WebBridgeConstantUchar,
    WebBridgeConstantInt,
    WebBridgeConstantUint,
    WebBridgeConstantHalf,
    WebBridgeConstantFloat,
    WebBridgeConstantTimecode,
    WebBridgeConstantString,
    WebBridgeConstantToken,
    WebBridgeConstantAsset,
    WebBridgeConstantMatrix2f,
    WebBridgeConstantMatrix3f,
    WebBridgeConstantMatrix4f,
    WebBridgeConstantQuatf,
    WebBridgeConstantQuath,
    WebBridgeConstantFloat2,
    WebBridgeConstantHalf2,
    WebBridgeConstantInt2,
    WebBridgeConstantFloat3,
    WebBridgeConstantHalf3,
    WebBridgeConstantInt3,
    WebBridgeConstantFloat4,
    WebBridgeConstantHalf4,
    WebBridgeConstantInt4,

    // semantic types
    WebBridgeConstantPoint3f,
    WebBridgeConstantPoint3h,
    WebBridgeConstantNormal3f,
    WebBridgeConstantNormal3h,
    WebBridgeConstantVector3f,
    WebBridgeConstantVector3h,
    WebBridgeConstantColor3f,
    WebBridgeConstantColor3h,
    WebBridgeConstantColor4f,
    WebBridgeConstantColor4h,
    WebBridgeConstantTexCoord2h,
    WebBridgeConstantTexCoord2f,
    WebBridgeConstantTexCoord3h,
    WebBridgeConstantTexCoord3f
};

typedef NS_ENUM(NSInteger, WebBridgeNodeType) {
    WebBridgeNodeTypeBuiltin,
    WebBridgeNodeTypeConstant,
    WebBridgeNodeTypeArguments,
    WebBridgeNodeTypeResults
};

@interface WebValueString : NSObject

@property (nonatomic, readonly) NSNumber *number;
@property (nonatomic, readonly) NSString *string;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNumber:(NSNumber *)number;
- (instancetype)initWithString:(NSString *)string;

@end

@interface WebBridgeConstantContainer : NSObject

@property (nonatomic, readonly) WebBridgeConstant constant;
@property (nonatomic, readonly, strong) NSArray<WebValueString *> *constantValues;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithConstant:(WebBridgeConstant)constant constantValues:(NSArray<WebValueString *> *)constantValues name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeBuiltin : NSObject

@property (nonatomic, readonly) NSString *definition;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDefinition:(NSString *)definition name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeFunctionReference : NSObject

@property (nonatomic, strong, readonly) NSString *moduleName;
@property (nonatomic, readonly) NSInteger functionIndex;

- (instancetype)initWithModuleName:(NSString *)moduleName functionIndex:(NSInteger)functionIndex;

@end

@class WebBridgeModuleReference;
@class WebBridgeTypeDefinition;
@class WebBridgeFunction;
@class WebBridgeModuleGraph;

NS_SWIFT_SENDABLE
@interface WebBridgeModule : NSObject

@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSArray<WebBridgeModuleReference *> *imports;
@property (nonatomic, readonly) NSArray<WebBridgeTypeDefinition *> *typeDefinitions;
@property (nonatomic, readonly) NSArray<WebBridgeFunction *> *functions;
@property (nonatomic, readonly) NSArray<WebBridgeModuleGraph *> *graphs;

- (instancetype)initWithName:(NSString *)name
    imports:(NSArray<WebBridgeModuleReference *> *)imports
    typeDefinitions:(NSArray<WebBridgeTypeDefinition *> *)typeDefinitions
    functions:(NSArray<WebBridgeFunction *> *)functions
    graphs:(NSArray<WebBridgeModuleGraph *> *)graphs NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeModuleReference : NSObject

@property (nonatomic, strong, readonly) WebBridgeModule *module;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSArray<WebBridgeModuleReference *> *imports;
@property (nonatomic, readonly) NSArray<WebBridgeTypeDefinition *> *typeDefinitions;
@property (nonatomic, readonly) NSArray<WebBridgeFunction *> *functions;

- (instancetype)initWithModule:(WebBridgeModule *)module NS_DESIGNATED_INITIALIZER;

@end

@class WebBridgeTypeReference;

NS_SWIFT_SENDABLE
@interface WebBridgeTypeReference : NSObject

@property (nonatomic, readonly) NSString *moduleName;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, assign, readonly) NSInteger typeDefIndex;

- (instancetype)initWithModule:(NSString *)moduleName
    name:(NSString *)name
    typeDefIndex:(NSInteger)typeDefIndex NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, WebBridgeTypeStructure) {
    WebBridgeTypeStructurePrimitive,
    WebBridgeTypeStructureStruct,
    WebBridgeTypeStructureEnum
};

NS_SWIFT_SENDABLE
@interface WebBridgeStructMember : NSObject

@property (nonatomic, readonly) NSString *name;
@property (nonatomic, strong, readonly) WebBridgeTypeReference *type;

- (instancetype)initWithName:(NSString *)name type:(WebBridgeTypeReference *)type NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeEnumCase : NSObject

@property (nonatomic, readonly) NSString *name;
@property (nonatomic, assign, readonly) NSInteger value;

- (instancetype)initWithName:(NSString *)name value:(NSInteger)value NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeTypeDefinition : NSObject

@property (nonatomic, readonly) NSString *name;
@property (nonatomic, strong, readonly) WebBridgeTypeReference *typeReference;
@property (nonatomic, assign, readonly) WebBridgeTypeStructure structureType;
@property (nonatomic, readonly, nullable) NSArray<WebBridgeStructMember *> *structMembers; // nil unless structureType == WebBridgeTypeStructureStruct
@property (nonatomic, readonly, nullable) NSArray<WebBridgeEnumCase *> *enumCases; // nil unless structureType == WebBridgeTypeStructureEnum

- (instancetype)initWithName:(NSString *)name
    typeReference:(WebBridgeTypeReference *)typeReference
    structureType:(WebBridgeTypeStructure)structureType
    structMembers:(nullable NSArray<WebBridgeStructMember *> *)structMembers
    enumCases:(nullable NSArray<WebBridgeEnumCase *> *)enumCases NS_DESIGNATED_INITIALIZER;

@end

@class WebBridgeFunctionReference;

typedef NS_ENUM(NSInteger, WebBridgeFunctionKind) {
    WebBridgeFunctionKindGraph,
    WebBridgeFunctionKindIntrinsic
};

NS_SWIFT_SENDABLE
@interface WebBridgeFunctionArgument : NSObject

@property (nonatomic, readonly) NSString *name;
@property (nonatomic, strong, readonly) WebBridgeTypeReference *type;

- (instancetype)initWithName:(NSString *)name type:(WebBridgeTypeReference *)type NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeFunction : NSObject

@property (nonatomic, strong, readonly) NSString *name;
@property (nonatomic, strong, readonly) NSArray<WebBridgeFunctionArgument *> *arguments;
@property (nonatomic, strong, readonly) WebBridgeTypeReference *returnType;
@property (nonatomic, strong, readonly) WebBridgeFunctionReference *functionReference;
@property (nonatomic, assign, readonly) WebBridgeFunctionKind kind;
@property (nonatomic, readonly) NSString *kindName; // graph name or stitching function name

- (instancetype)initWithName:(NSString *)name
    arguments:(NSArray<WebBridgeFunctionArgument *> *)arguments
    returnType:(WebBridgeTypeReference *)returnType
    functionReference:(WebBridgeFunctionReference *)functionReference
    kind:(WebBridgeFunctionKind)kind
    kindName:(NSString *)kindName NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeNodeID : NSObject

@property (nonatomic, assign, readonly) NSInteger value;

- (instancetype)initWithValue:(NSInteger)value NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, WebBridgeFunctionCallType) {
    WebBridgeFunctionCallTypeName,
    WebBridgeFunctionCallTypeReference
};

NS_SWIFT_SENDABLE
@interface WebBridgeFunctionCall : NSObject

@property (nonatomic, assign, readonly) WebBridgeFunctionCallType type;
@property (nonatomic, readonly, nullable) NSString *name; // if type == WebBridgeFunctionCallTypeName
@property (nonatomic, strong, readonly, nullable) WebBridgeFunctionReference *reference; // if type == WebBridgeFunctionCallTypeReference

- (instancetype)initWithName:(NSString *)name;
- (instancetype)initWithReference:(WebBridgeFunctionReference *)reference;

@end

typedef NS_ENUM(NSInteger, WebBridgeNodeInstructionType) {
    WebBridgeNodeInstructionTypeFunctionCall,
    WebBridgeNodeInstructionTypeFunctionConstant,
    WebBridgeNodeInstructionTypeLiteral,
    WebBridgeNodeInstructionTypeArgument,
    WebBridgeNodeInstructionTypeElement
};

typedef NS_ENUM(uint32_t, WebBridgeLiteralType) {
    WebBridgeLiteralTypeBool,
    WebBridgeLiteralTypeInt32,
    WebBridgeLiteralTypeUInt32,
    WebBridgeLiteralTypeFloat,
    WebBridgeLiteralTypeFloat2,
    WebBridgeLiteralTypeFloat3,
    WebBridgeLiteralTypeFloat4,
#if defined(__arm64__)
    WebBridgeLiteralTypeHalf,
    WebBridgeLiteralTypeHalf2,
    WebBridgeLiteralTypeHalf3,
    WebBridgeLiteralTypeHalf4,
#endif
    WebBridgeLiteralTypeInt2,
    WebBridgeLiteralTypeInt3,
    WebBridgeLiteralTypeInt4,
    WebBridgeLiteralTypeUInt2,
    WebBridgeLiteralTypeUInt3,
    WebBridgeLiteralTypeUInt4,
    WebBridgeLiteralTypeFloat2x2,
    WebBridgeLiteralTypeFloat3x3,
    WebBridgeLiteralTypeFloat4x4,
#if defined(__arm64__)
    WebBridgeLiteralTypeHalf2x2,
    WebBridgeLiteralTypeHalf3x3,
    WebBridgeLiteralTypeHalf4x4,
#endif
};

NS_SWIFT_SENDABLE
@interface WebBridgeLiteralArchive : NSObject

@property (nonatomic, assign, readonly) WebBridgeLiteralType type;
@property (nonatomic, readonly) NSArray<NSNumber *> *data; // Array of UInt32

- (instancetype)initWithType:(WebBridgeLiteralType)type data:(NSArray<NSNumber *> *)data NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeLiteral : NSObject

@property (nonatomic, assign, readonly) WebBridgeLiteralType type;
@property (nonatomic, strong, readonly) WebBridgeLiteralArchive *archive;

- (instancetype)initWithType:(WebBridgeLiteralType)type data:(NSArray<NSNumber *> *)data NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeNodeInstruction : NSObject

@property (nonatomic, assign, readonly) WebBridgeNodeInstructionType type;

// Properties based on type
@property (nonatomic, strong, readonly, nullable) WebBridgeFunctionCall *functionCall;
@property (nonatomic, readonly, nullable) NSString *constantName;
@property (nonatomic, strong, readonly, nullable) WebBridgeLiteral *literal;
@property (nonatomic, readonly, nullable) NSString *argumentName;
@property (nonatomic, strong, readonly, nullable) WebBridgeTypeReference *elementType;
@property (nonatomic, readonly, nullable) NSString *elementName;

- (instancetype)initWithFunctionCall:(WebBridgeFunctionCall *)functionCall;
- (instancetype)initWithFunctionConstant:(NSString *)name literal:(WebBridgeLiteral *)literal;
- (instancetype)initWithLiteral:(WebBridgeLiteral *)literal;
- (instancetype)initWithArgument:(NSString *)argumentName;
- (instancetype)initWithElementType:(WebBridgeTypeReference *)type elementName:(NSString *)elementName;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeArgumentError : NSObject

@property (nonatomic, readonly) NSString *message;
@property (nonatomic, strong, readonly) WebBridgeFunctionArgument *argument;

- (instancetype)initWithMessage:(NSString *)message argument:(WebBridgeFunctionArgument *)argument NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeNode : NSObject

@property (nonatomic, strong, readonly) WebBridgeNodeID *nodeID;
@property (nonatomic, strong, readonly) WebBridgeNodeInstruction *instruction;

- (instancetype)initWithIdentifier:(WebBridgeNodeID *)nodeID
    instruction:(WebBridgeNodeInstruction *)instruction NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeGraphEdge : NSObject

@property (nonatomic, strong, readonly) WebBridgeNodeID *source;
@property (nonatomic, strong, readonly) WebBridgeNodeID *destination;
@property (nonatomic, readonly) NSString *argument;

- (instancetype)initWithSource:(WebBridgeNodeID *)source
    destination:(WebBridgeNodeID *)destination
    argument:(NSString *)argument NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeModuleGraph : NSObject

@property (nonatomic, strong, readonly) WebBridgeFunctionReference *functionReference;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSArray<WebBridgeFunctionArgument *> *arguments;
@property (nonatomic, strong, readonly) WebBridgeTypeReference *returnType;
@property (nonatomic, readonly) NSArray<WebBridgeNode *> *nodes;
@property (nonatomic, readonly) NSArray<WebBridgeGraphEdge *> *edges;
@property (nonatomic, readonly) NSInteger index;

- (instancetype)initWithIndex:(NSInteger)index function:(WebBridgeFunction *)function NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WebBridgeUpdateMaterial : NSObject

@property (nonatomic, strong, readonly, nullable) NSData *materialGraph;
@property (nonatomic, strong, readonly) NSString *identifier;
@property (nonatomic, strong, readonly, nullable) WebBridgeFunctionReference *geometryModifierFunctionReference;
@property (nonatomic, strong, readonly, nullable) WebBridgeFunctionReference *surfaceShaderFunctionReference;
@property (nonatomic, strong, readonly, nullable) WebBridgeModule *shaderGraphModule;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithMaterialGraph:(nullable NSData *)materialGraph identifier:(NSString *)identifier geometryModifierFunctionReference:(nullable WebBridgeFunctionReference *)geometryModifierFunctionReference surfaceShaderFunctionReference:(nullable WebBridgeFunctionReference *)surfaceShaderFunctionReference shaderGraphModule:(nullable WebBridgeModule *)shaderGraphModule NS_DESIGNATED_INITIALIZER;

@end

@interface WebUSDConfiguration : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDevice:(id<MTLDevice>)device NS_DESIGNATED_INITIALIZER;

- (void)createMaterialCompiler:(void (^)(void))completionHandler;

@end

@interface WebBridgeReceiver : NSObject

- (void)renderWithTexture:(id<MTLTexture>)texture;
- (void)updateMesh:(WebBridgeUpdateMesh *)descriptor completionHandler:(void (^)(void))completionHandler;
- (void)updateTexture:(WebBridgeUpdateTexture *)descriptor;
- (void)updateMaterial:(WebBridgeUpdateMaterial *)descriptor completionHandler:(void (^)(void))completionHandler;
- (void)setTransform:(simd_float4x4)transform;
- (void)setCameraDistance:(float)distance;
- (void)setPlaying:(BOOL)play;
- (void)setEnvironmentMap:(WebBridgeImageAsset *)imageAsset;

- (instancetype)init NS_UNAVAILABLE;
- (nullable instancetype)initWithConfiguration:(WebUSDConfiguration *)configuration diffuseAsset:(WebBridgeImageAsset *)diffuseAsset specularAsset:(WebBridgeImageAsset *)specularAsset error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@end

@interface WebBridgeModelLoader : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (double)currentTime;
- (double)duration;
- (void)loadModelFrom:(NSURL *)url;
- (void)loadModel:(NSData *)data;
- (void)update:(double)deltaTime;
- (void)requestCompleted:(NSObject *)request;
- (void)setCallbacksWithModelUpdatedCallback:(void (^)(WebBridgeUpdateMesh *))modelUpdatedCallback textureUpdatedCallback:(void (^)(WebBridgeUpdateTexture *))textureUpdatedCallback materialUpdatedCallback:(void (^)(WebBridgeUpdateMaterial *))materialUpdatedCallback;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif
#endif

#ifdef __cplusplus

namespace WebModel {

struct ImageAssetSwizzle {
    uint8_t red { 0 };
    uint8_t green { 0 };
    uint8_t blue { 0 };
    uint8_t alpha { 0 };
};

struct ImageAsset {
    Vector<uint8_t> data;
    long width { 0 };
    long height { 0 };
    long depth { 0 };
    long bytesPerPixel { 0 };
    uint64_t textureType { 0 };
    uint64_t pixelFormat { 0 };
    long mipmapLevelCount { 0 };
    long arrayLength { 0 };
    uint64_t textureUsage { 0 };
    ImageAssetSwizzle swizzle { };
};

struct Edge {
    long upstreamNodeIndex;
    long downstreamNodeIndex;
    String upstreamOutputName;
    String downstreamInputName;
};

enum class DataType : uint8_t {
    kBool,
    kInt,
    kInt2,
    kInt3,
    kInt4,
    kFloat,
    kColor3f,
    kColor3h,
    kColor4f,
    kColor4h,
    kFloat2,
    kFloat3,
    kFloat4,
    kHalf,
    kHalf2,
    kHalf3,
    kHalf4,
    kMatrix2f,
    kMatrix3f,
    kMatrix4f,
    kSurfaceShader,
    kGeometryModifier,
    kString,
    kToken,
    kAsset
};

struct Primvar {
    String name;
    String referencedGeomPropName;
    uint64_t attributeFormat;
};

struct InputOutput {
    DataType type;
    String name;
};

enum class Constant : uint8_t {
    kBool,
    kUchar,
    kInt,
    kUint,
    kHalf,
    kFloat,
    kTimecode,
    kString,
    kToken,
    kAsset,
    kMatrix2f,
    kMatrix3f,
    kMatrix4f,
    kQuatf,
    kQuath,
    kFloat2,
    kHalf2,
    kInt2,
    kFloat3,
    kHalf3,
    kInt3,
    kFloat4,
    kHalf4,
    kInt4,

    // semantic types
    kPoint3f,
    kPoint3h,
    kNormal3f,
    kNormal3h,
    kVector3f,
    kVector3h,
    kColor3f,
    kColor3h,
    kColor4f,
    kColor4h,
    kTexCoord2h,
    kTexCoord2f,
    kTexCoord3h,
    kTexCoord3f
};

using NumberOrString = Variant<String, double>;

struct ConstantContainer {
    Constant constant;
    Vector<NumberOrString> constantValues;
    String name;
};

struct Builtin {
    String definition;
    String name;
};

enum class NodeType : uint8_t {
    Builtin,
    Constant,
    Arguments,
    Results
};

struct Node {
    NodeType bridgeNodeType;
    Builtin builtin;
    ConstantContainer constant;
};

struct VertexLayout {
    long bufferIndex;
    long bufferOffset;
    long bufferStride;
};

struct MaterialGraph {
    Vector<Node> nodes;
    Vector<Edge> edges;
    Vector<InputOutput> inputs;
    Vector<InputOutput> outputs;
    Vector<Primvar> primvars;
    String identifier;
};

struct MeshPart {
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t topology;
    uint32_t materialIndex;
    Float3 boundsMin;
    Float3 boundsMax;
};

struct VertexAttributeFormat {
    long semantic;
    unsigned long format;
    long layoutIndex;
    long offset;
};

struct MeshDescriptor {
    long vertexBufferCount;
    long vertexCapacity;
    Vector<VertexAttributeFormat> vertexAttributes;
    Vector<VertexLayout> vertexLayouts;
    long indexCapacity;
    long indexType;
};

struct MaterialDescriptor {
    Vector<uint8_t> materialGraph;
    String identifier;
};

struct UpdateMaterialDescriptor {
    Vector<uint8_t> materialGraph;
    String identifier;
};

struct UpdateTextureDescriptor {
    ImageAsset imageAsset;
    String identifier;
    String hashString;
};

struct SkinningData {
    uint8_t influencePerVertexCount;
    Vector<Float4x4> jointTransforms;
    Vector<Float4x4> inverseBindPoses;
    Vector<uint32_t> influenceJointIndices;
    Vector<float> influenceWeights;
    Float4x4 geometryBindTransform;
};

struct BlendShapeData {
    Vector<float> weights;
    Vector<Vector<Float3>> positionOffsets;
    Vector<Vector<Float3>> normalOffsets;
};

struct RenormalizationData {
    Vector<uint32_t> vertexIndicesPerTriangle;
    Vector<uint32_t> vertexAdjacencies;
    Vector<uint32_t> vertexAdjacencyEndIndices;
};

struct DeformationData {
    std::optional<SkinningData> skinningData;
    std::optional<BlendShapeData> blendShapeData;
    std::optional<RenormalizationData> renormalizationData;
};

struct UpdateMeshDescriptor {
    String identifier;
    uint8_t updateType;
    MeshDescriptor descriptor;
    Vector<MeshPart> parts;
    Vector<uint8_t> indexData;
    Vector<Vector<uint8_t>> vertexData;
    Float4x4 transform;
    Vector<Float4x4> instanceTransforms;
    Vector<String> materialPrims;
    std::optional<DeformationData> deformationData;
};
}

typedef struct WebModelCreateMeshDescriptor {
    unsigned width;
    unsigned height;
    Vector<RetainPtr<IOSurfaceRef>> ioSurfaces;
    const WebModel::ImageAsset& diffuseTexture;
    const WebModel::ImageAsset& specularTexture;
} WebModelCreateMeshDescriptor;

#endif

typedef struct WebMeshImpl* WebMesh;

#ifdef __cplusplus
WTF_EXPORT_DECLARATION void webModelMeshReference(_Nonnull WebMesh);
WTF_EXPORT_DECLARATION void webModelMeshRelease(_Nonnull WebMesh);

WTF_EXPORT_DECLARATION _Nonnull WebMesh webModelMeshCreate(struct WGPUInstanceImpl* _Nonnull, const WebModelCreateMeshDescriptor* _Nonnull);

WTF_EXPORT_DECLARATION void webModelMeshUpdate(_Nonnull WebMesh, const WebModel::UpdateMeshDescriptor&);
WTF_EXPORT_DECLARATION void webModelMeshTextureUpdate(_Nonnull WebMesh, const WebModel::UpdateTextureDescriptor&);
WTF_EXPORT_DECLARATION void webModelMeshMaterialUpdate(_Nonnull WebMesh, const WebModel::UpdateMaterialDescriptor&);
WTF_EXPORT_DECLARATION void webModelMeshRender(_Nonnull WebMesh);
WTF_EXPORT_DECLARATION void webModelMeshSetTransform(_Nonnull WebMesh, const simd_float4x4& transform);
WTF_EXPORT_DECLARATION void webModelMeshSetCameraDistance(_Nonnull WebMesh, float distance);
WTF_EXPORT_DECLARATION void webModelMeshPlay(_Nonnull WebMesh, bool autoplay);
WTF_EXPORT_DECLARATION void webModelMeshSetEnvironmentMap(_Nonnull WebMesh, const WebModel::ImageAsset&);
#endif
