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

#include <wtf/Platform.h>

#ifdef __cplusplus
#include <WebKit/Float3.h>
#include <WebKit/Float4x4.h>
#include <wtf/ExportMacros.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>
#endif

typedef struct __IOSurface *IOSurfaceRef;

#ifdef __OBJC__

#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <simd/simd.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

typedef NS_ENUM(uint8_t, WKBridgeDataUpdateType) {
    WKBridgeDataUpdateTypeInitial,
    WKBridgeDataUpdateTypeDelta
};

@interface WKBridgeVertexAttributeFormat : NSObject

@property (nonatomic, readonly) long semantic;
@property (nonatomic, readonly) unsigned long format;
@property (nonatomic, readonly) long layoutIndex;
@property (nonatomic, readonly) long offset;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSemantic:(long)semantic format:(unsigned long)format layoutIndex:(long)layoutIndex offset:(long)offset NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeVertexLayout : NSObject

@property (nonatomic, readonly) long bufferIndex;
@property (nonatomic, readonly) long bufferOffset;
@property (nonatomic, readonly) long bufferStride;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(long)bufferIndex bufferOffset:(long)bufferOffset bufferStride:(long)bufferStride NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeMeshPart : NSObject

@property (nonatomic, readonly) long indexOffset;
@property (nonatomic, readonly) long indexCount;
@property (nonatomic, readonly) MTLPrimitiveType topology;
@property (nonatomic, readonly) long materialIndex;
@property (nonatomic, readonly) simd_float3 boundsMin;
@property (nonatomic, readonly) simd_float3 boundsMax;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexOffset:(long)indexOffset indexCount:(long)indexCount topology:(MTLPrimitiveType)topology materialIndex:(long)materialIndex boundsMin:(simd_float3)boundsMin boundsMax:(simd_float3)boundsMax NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeMeshDescriptor : NSObject

@property (nonatomic, readonly) long vertexBufferCount;
@property (nonatomic, readonly) long vertexCapacity;
@property (nonatomic, readonly) NSArray<WKBridgeVertexAttributeFormat *> *vertexAttributes;
@property (nonatomic, readonly) NSArray<WKBridgeVertexLayout *> *vertexLayouts;
@property (nonatomic, readonly) long indexCapacity;
@property (nonatomic, readonly) MTLIndexType indexType;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithVertexBufferCount:(long)vertexBufferCount vertexCapacity:(long)vertexCapacity vertexAttributes:(NSArray<WKBridgeVertexAttributeFormat *> *)vertexAttributes vertexLayouts:(NSArray<WKBridgeVertexLayout *> *)vertexLayouts indexCapacity:(long)indexCapacity indexType:(MTLIndexType)indexType NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeSkinningData : NSObject

@property (nonatomic, readonly) uint8_t influencePerVertexCount;
@property (nonatomic, readonly, nullable) NSData *jointTransformsData; // [simd_float4x4]
@property (nonatomic, readonly, nullable) NSData *inverseBindPosesData; // [simd_float4x4]
@property (nonatomic, readonly, nullable) NSData *influenceJointIndicesData; // [UInt32]
@property (nonatomic, readonly, nullable) NSData *influenceWeightsData; // [Float]
@property (nonatomic, readonly) simd_float4x4 geometryBindTransform;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithInfluencePerVertexCount:(uint8_t)influencePerVertexCount jointTransforms:(nullable NSData *)jointTransforms inverseBindPoses:(nullable NSData *)inverseBindPoses influenceJointIndices:(nullable NSData *)influenceJointIndices influenceWeights:(nullable NSData *)influenceWeights geometryBindTransform:(simd_float4x4)geometryBindTransform NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeBlendShapeData : NSObject

@property (nonatomic, readonly) NSData *weightsData; // [Float]
@property (nonatomic, readonly) NSArray<NSData *> *positionOffsetsData; // [[SIMD3<Float>]]
@property (nonatomic, readonly) NSArray<NSData *> *normalOffsetsData; // [[SIMD3<Float>]]

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWeights:(NSData *)weights positionOffsets:(NSArray<NSData *> *)positionOffsets normalOffsets:(NSArray<NSData *> *)normalOffsets NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeRenormalizationData : NSObject

@property (nonatomic, readonly) NSData *vertexIndicesPerTriangleData; // [UInt32]
@property (nonatomic, readonly) NSData *vertexAdjacenciesData; // [UInt32]
@property (nonatomic, readonly) NSData *vertexAdjacencyEndIndicesData; // [UInt32]

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithVertexIndicesPerTriangle:(NSData *)vertexIndicesPerTriangle vertexAdjacencies:(NSData *)vertexAdjacencies vertexAdjacencyEndIndices:(NSData *)vertexAdjacencyEndIndices NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeDeformationData : NSObject

@property (nonatomic, readonly, nullable) WKBridgeSkinningData *skinningData;
@property (nonatomic, readonly, nullable) WKBridgeBlendShapeData *blendShapeData;
@property (nonatomic, readonly, nullable) WKBridgeRenormalizationData *renormalizationData;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSkinningData:(nullable WKBridgeSkinningData *)skinningData blendShapeData:(nullable WKBridgeBlendShapeData *)blendShapeData renormalizationData:(nullable WKBridgeRenormalizationData *)renormalizationData NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, WKBridgeSemantic) {
    WKBridgeSemanticColor,
    WKBridgeSemanticVector,
    WKBridgeSemanticScalar,
    WKBridgeSemanticUnknown
};

@interface WKBridgeEdge : NSObject

@property (nonatomic, readonly) NSString *outputNode;
@property (nonatomic, readonly) NSString *outputPort;
@property (nonatomic, readonly) NSString *inputNode;
@property (nonatomic, readonly) NSString *inputPort;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithOutputNode:(NSString *)outputNode
    outputPort:(NSString *)outputPort
    inputNode:(NSString *)inputNode
    inputPort:(NSString *)inputPort NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, WKBridgeDataType) {
    WKBridgeDataTypeBool,
    WKBridgeDataTypeUchar,
    WKBridgeDataTypeInt,
    WKBridgeDataTypeUint,
    WKBridgeDataTypeInt2,
    WKBridgeDataTypeInt3,
    WKBridgeDataTypeInt4,
    WKBridgeDataTypeFloat,
    WKBridgeDataTypeColor3f,
    WKBridgeDataTypeColor3h,
    WKBridgeDataTypeColor4f,
    WKBridgeDataTypeColor4h,
    WKBridgeDataTypeFloat2,
    WKBridgeDataTypeFloat3,
    WKBridgeDataTypeFloat4,
    WKBridgeDataTypeHalf,
    WKBridgeDataTypeHalf2,
    WKBridgeDataTypeHalf3,
    WKBridgeDataTypeHalf4,
    WKBridgeDataTypeMatrix2f,
    WKBridgeDataTypeMatrix3f,
    WKBridgeDataTypeMatrix4f,
    WKBridgeDataTypeMatrix2h,
    WKBridgeDataTypeMatrix3h,
    WKBridgeDataTypeMatrix4h,
    WKBridgeDataTypeQuat,
    WKBridgeDataTypeSurfaceShader,
    WKBridgeDataTypeGeometryModifier,
    WKBridgeDataTypePostLightingShader,
    WKBridgeDataTypeString,
    WKBridgeDataTypeToken,
    WKBridgeDataTypeAsset
};

@class WKBridgeConstantContainer;

@interface WKBridgeInputOutput : NSObject

@property (nonatomic, readonly) WKBridgeDataType type;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) WKBridgeDataType semanticType;
@property (nonatomic, readonly) BOOL hasSemanticType;
@property (nonatomic, readonly, nullable) WKBridgeConstantContainer *defaultValue;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithType:(WKBridgeDataType)dataType name:(NSString *)name semanticType:(WKBridgeDataType)semanticType hasSemanticType:(BOOL)hasSemanticType defaultValue:(nullable WKBridgeConstantContainer *)defaultValue NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, WKBridgeConstant) {
    WKBridgeConstantBool,
    WKBridgeConstantUchar,
    WKBridgeConstantInt,
    WKBridgeConstantUint,
    WKBridgeConstantHalf,
    WKBridgeConstantFloat,
    WKBridgeConstantTimecode,
    WKBridgeConstantString,
    WKBridgeConstantToken,
    WKBridgeConstantAsset,
    WKBridgeConstantMatrix2f,
    WKBridgeConstantMatrix3f,
    WKBridgeConstantMatrix4f,
    WKBridgeConstantQuatf,
    WKBridgeConstantQuath,
    WKBridgeConstantFloat2,
    WKBridgeConstantHalf2,
    WKBridgeConstantInt2,
    WKBridgeConstantFloat3,
    WKBridgeConstantHalf3,
    WKBridgeConstantInt3,
    WKBridgeConstantFloat4,
    WKBridgeConstantHalf4,
    WKBridgeConstantInt4,

    // semantic types
    WKBridgeConstantPoint3f,
    WKBridgeConstantPoint3h,
    WKBridgeConstantNormal3f,
    WKBridgeConstantNormal3h,
    WKBridgeConstantVector3f,
    WKBridgeConstantVector3h,
    WKBridgeConstantColor3f,
    WKBridgeConstantColor3h,
    WKBridgeConstantColor4f,
    WKBridgeConstantColor4h,
    WKBridgeConstantTexCoord2h,
    WKBridgeConstantTexCoord2f,
    WKBridgeConstantTexCoord3h,
    WKBridgeConstantTexCoord3f
};

typedef NS_ENUM(NSInteger, WKBridgeNodeType) {
    WKBridgeNodeTypeBuiltin,
    WKBridgeNodeTypeConstant,
    WKBridgeNodeTypeArguments,
    WKBridgeNodeTypeResults
};

@interface WKBridgeValueString : NSObject

@property (nonatomic, readonly) NSNumber *number;
@property (nonatomic, readonly) NSString *string;
@property (nonatomic, readonly) bool isString;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNumber:(NSNumber *)number;
- (instancetype)initWithString:(NSString *)string;

@end

@interface WKBridgeConstantContainer : NSObject

@property (nonatomic, readonly) WKBridgeConstant constant;
@property (nonatomic, readonly, strong) NSArray<WKBridgeValueString *> *constantValues;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithConstant:(WKBridgeConstant)constant constantValues:(NSArray<WKBridgeValueString *> *)constantValues name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeBuiltin : NSObject

@property (nonatomic, readonly) NSString *definition;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDefinition:(NSString *)definition name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeNode : NSObject

@property (nonatomic, readonly) WKBridgeNodeType bridgeNodeType;
@property (nonatomic, readonly, strong, nullable) WKBridgeBuiltin *builtin;
@property (nonatomic, readonly, nullable) WKBridgeConstantContainer *constant;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBridgeNodeType:(WKBridgeNodeType)bridgeNodeType builtin:(WKBridgeBuiltin *)builtin constant:(WKBridgeConstantContainer *)constant NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WKBridgeMaterialGraph : NSObject

@property (nonatomic, strong, readonly) NSArray<WKBridgeNode *> *nodes;
@property (nonatomic, strong, readonly) NSArray<WKBridgeEdge *> *edges;
@property (nonatomic, strong, readonly) WKBridgeNode *arguments;
@property (nonatomic, strong, readonly) WKBridgeNode *results;
@property (nonatomic, strong, readonly) NSArray<WKBridgeInputOutput *> *inputs;
@property (nonatomic, strong, readonly) NSArray<WKBridgeInputOutput *> *outputs;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNodes:(NSArray<WKBridgeNode *> *)nodes edges:(NSArray<WKBridgeEdge *> *)edges arguments:(WKBridgeNode *)arguments results:(WKBridgeNode *)results inputs:(NSArray<WKBridgeInputOutput *> *)inputs outputs:(NSArray<WKBridgeInputOutput *> *)outputs NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WKBridgeUpdateMesh : NSObject

@property (nonatomic, readonly) NSString *identifier;
@property (nonatomic, readonly) WKBridgeDataUpdateType updateType;
@property (nonatomic, strong, readonly, nullable) WKBridgeMeshDescriptor *descriptor;
@property (nonatomic, strong, readonly) NSArray<WKBridgeMeshPart*> *parts;
@property (nonatomic, strong, readonly, nullable) NSData *indexData;
@property (nonatomic, strong, readonly) NSArray<NSData *> *vertexData;
@property (nonatomic, strong, readonly, nullable) NSData *instanceTransformsData;
@property (nonatomic, readonly) long instanceTransformsCount;
@property (nonatomic, strong, readonly) NSArray<NSString *> *materialPrims;
@property (nonatomic, strong, readonly, nullable) WKBridgeDeformationData *deformationData;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithIdentifier:(NSString *)identifier
    updateType:(WKBridgeDataUpdateType)updateType
    descriptor:(nullable WKBridgeMeshDescriptor *)descriptor
    parts:(NSArray<WKBridgeMeshPart*> *)parts
    indexData:(nullable NSData *)indexData
    vertexData:(NSArray<NSData *> *)vertexData
    instanceTransforms:(nullable NSData *)instanceTransforms
    instanceTransformsCount:(long)instanceTransformsCount
    materialPrims:(NSArray<NSString *> *)materialPrims
    deformationData:(nullable WKBridgeDeformationData *)deformationData NS_DESIGNATED_INITIALIZER;

@end

NS_SWIFT_SENDABLE
@interface WKBridgeUpdateMaterial : NSObject

@property (nonatomic, strong, readonly, nullable) WKBridgeMaterialGraph *materialGraph;
@property (nonatomic, strong, readonly) NSString *identifier;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithMaterialGraph:(nullable WKBridgeMaterialGraph *)materialGraph identifier:(NSString *)identifier NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeImageAsset : NSObject

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

@interface WKBridgeUpdateTexture : NSObject

@property (nonatomic, readonly, strong, nullable) WKBridgeImageAsset *imageAsset;
@property (nonatomic, readonly, strong) NSString *identifier;
@property (nonatomic, readonly, strong) NSString *hashString;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithImageAsset:(nullable WKBridgeImageAsset *)imageAsset identifier:(NSString *)identifier hashString:(NSString *)hashString NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeUSDConfiguration : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDevice:(id<MTLDevice>)device memoryOwner:(task_id_token_t)memoryOwner NS_DESIGNATED_INITIALIZER;

- (void)createMaterialCompiler:(void (^)(void))completionHandler;

@end

@interface WKBridgeReceiver : NSObject

- (void)renderWithTexture:(id<MTLTexture>)texture;
- (void)updateMesh:(WKBridgeUpdateMesh *)descriptor completionHandler:(void (^)(void))completionHandler;
- (void)updateTexture:(WKBridgeUpdateTexture *)descriptor;
- (void)updateMaterial:(WKBridgeUpdateMaterial *)descriptor completionHandler:(void (^)(void))completionHandler;
- (void)setTransform:(simd_float4x4)transform;
- (void)setCameraDistance:(float)distance;
- (void)setBackgroundColor:(simd_float3)color;
- (void)setPlaying:(BOOL)play;
- (void)setEnvironmentMap:(WKBridgeImageAsset *)imageAsset;

- (instancetype)init NS_UNAVAILABLE;
- (nullable instancetype)initWithConfiguration:(WKBridgeUSDConfiguration *)configuration diffuseAsset:(WKBridgeImageAsset *)diffuseAsset specularAsset:(WKBridgeImageAsset *)specularAsset error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@end

@interface WKBridgeModelLoader : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (double)currentTime;
- (void)setCurrentTime:(double)newTime;
- (double)duration;
- (void)loadModelFrom:(NSURL *)url;
- (void)loadModel:(NSData *)data;
- (void)update:(double)deltaTime;
- (void)setLoop:(BOOL)loop;
- (void)requestCompleted:(NSObject *)request;
- (void)setCallbacksWithModelUpdatedCallback:(void (^)(WKBridgeUpdateMesh *))modelUpdatedCallback textureUpdatedCallback:(void (^)(WKBridgeUpdateTexture *))textureUpdatedCallback materialUpdatedCallback:(void (^)(WKBridgeUpdateMaterial *))materialUpdatedCallback;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif

#ifdef __cplusplus

namespace WebCore {
class ProcessIdentity;
}

namespace WebModel {

using NumberOrString = Variant<String, double>;

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

struct VertexLayout {
    long bufferIndex;
    long bufferOffset;
    long bufferStride;
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

struct Edge {
    String outputNode;
    String outputPort;
    String inputNode;
    String inputPort;
};

enum class DataType : uint8_t {
    kBool,
    kUchar,
    kInt,
    kUint,
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
    kMatrix2h,
    kMatrix3h,
    kMatrix4h,
    kQuat,
    kSurfaceShader,
    kGeometryModifier,
    kPostLightingShader,
    kString,
    kToken,
    kAsset
};

struct Primvar {
    String name;
    String referencedGeomPropName;
    uint64_t attributeFormat;
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

enum class NodeType : uint8_t {
    Builtin,
    Constant,
    Arguments,
    Results
};

struct ConstantContainer {
    Constant constant;
    Vector<Variant<String, double>> constantValues;
    String name;
};

struct InputOutput {
    DataType type;
    String name;
    std::optional<DataType> semanticType;
    std::optional<ConstantContainer> defaultValue;
};

struct Builtin {
    String definition;
    String name;
};

struct Node {
    NodeType bridgeNodeType;
    Builtin builtin;
    ConstantContainer constant;
};

struct MaterialGraph {
    Vector<Node> nodes;
    Vector<Edge> edges;
    Node arguments;
    Node results;
    Vector<InputOutput> inputs;
    Vector<InputOutput> outputs;
};

struct UpdateMaterialDescriptor {
    MaterialGraph materialGraph;
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
    const WebCore::ProcessIdentity* processIdentity;
} WebModelCreateMeshDescriptor;

#endif
