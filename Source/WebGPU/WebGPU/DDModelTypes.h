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

#if ENABLE(GPU_PROCESS_MODEL)

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <simd/simd.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

enum class DDBridgeDataUpdateType : uint8_t {
    kInitial,
    kDelta
};

@interface DDBridgeVertexAttributeFormat : NSObject

@property (nonatomic, readonly) long semantic;
@property (nonatomic, readonly) unsigned long format;
@property (nonatomic, readonly) long layoutIndex;
@property (nonatomic, readonly) long offset;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSemantic:(long)semantic format:(unsigned long)format layoutIndex:(long)layoutIndex offset:(long)offset NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeVertexLayout : NSObject

@property (nonatomic, readonly) long bufferIndex;
@property (nonatomic, readonly) long bufferOffset;
@property (nonatomic, readonly) long bufferStride;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(long)bufferIndex bufferOffset:(long)bufferOffset bufferStride:(long)bufferStride NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeMeshPart : NSObject

@property (nonatomic, readonly) long indexOffset;
@property (nonatomic, readonly) long indexCount;
@property (nonatomic, readonly) MTLPrimitiveType topology;
@property (nonatomic, readonly) long materialIndex;
@property (nonatomic, readonly) simd_float3 boundsMin;
@property (nonatomic, readonly) simd_float3 boundsMax;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexOffset:(long)indexOffset indexCount:(long)indexCount topology:(MTLPrimitiveType)topology materialIndex:(long)materialIndex boundsMin:(simd_float3)boundsMin boundsMax:(simd_float3)boundsMax NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeMeshDescriptor : NSObject

@property (nonatomic, readonly) long vertexBufferCount;
@property (nonatomic, readonly) long vertexCapacity;
@property (nonatomic, readonly) NSArray<DDBridgeVertexAttributeFormat *> *vertexAttributes;
@property (nonatomic, readonly) NSArray<DDBridgeVertexLayout *> *vertexLayouts;
@property (nonatomic, readonly) long indexCapacity;
@property (nonatomic, readonly) MTLIndexType indexType;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithVertexBufferCount:(long)vertexBufferCount vertexCapacity:(long)vertexCapacity vertexAttributes:(NSArray<DDBridgeVertexAttributeFormat *> *)vertexAttributes vertexLayouts:(NSArray<DDBridgeVertexLayout *> *)vertexLayouts indexCapacity:(long)indexCapacity indexType:(MTLIndexType)indexType NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeChainedFloat4x4 : NSObject

@property (nonatomic) simd_float4x4 transform;
@property (nonatomic, strong, nullable) DDBridgeChainedFloat4x4 *next;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithTransform:(simd_float4x4)transform NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeUpdateMesh : NSObject

@property (nonatomic, readonly) NSString *identifier;
@property (nonatomic, readonly) DDBridgeDataUpdateType updateType;
@property (nonatomic, strong, readonly, nullable) DDBridgeMeshDescriptor *descriptor;
@property (nonatomic, strong, readonly) NSArray<DDBridgeMeshPart*> *parts;
@property (nonatomic, strong, readonly, nullable) NSData *indexData;
@property (nonatomic, strong, readonly) NSArray<NSData *> *vertexData;
@property (nonatomic, strong, readonly, nullable) DDBridgeChainedFloat4x4 *instanceTransforms;
@property (nonatomic, readonly) long instanceTransformsCount;
@property (nonatomic, strong, readonly) NSArray<NSString *> *materialPrims;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithIdentifier:(NSString *)identifier
    updateType:(DDBridgeDataUpdateType)updateType
    descriptor:(nullable DDBridgeMeshDescriptor *)descriptor
    parts:(NSArray<DDBridgeMeshPart*> *)parts
    indexData:(nullable NSData *)indexData
    vertexData:(NSArray<NSData *> *)vertexData
    instanceTransforms:(nullable DDBridgeChainedFloat4x4 *)instanceTransforms
    instanceTransformsCount:(long)instanceTransformsCount
    materialPrims:(NSArray<NSString *> *)materialPrims NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, DDBridgeSemantic) {
    DDBridgeSemanticColor,
    DDBridgeSemanticVector,
    DDBridgeSemanticScalar,
    DDBridgeSemanticUnknown
};

@interface DDBridgeImageAsset : NSObject

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

@interface DDBridgeUpdateTexture : NSObject

@property (nonatomic, readonly, strong, nullable) DDBridgeImageAsset *imageAsset;
@property (nonatomic, readonly, strong) NSString *identifier;
@property (nonatomic, readonly, strong) NSString *hashString;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithImageAsset:(nullable DDBridgeImageAsset *)imageAsset identifier:(NSString *)identifier hashString:(NSString *)hashString NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeEdge : NSObject

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

typedef NS_ENUM(NSInteger, DDBridgeDataType) {
    DDBridgeDataTypeBool,
    DDBridgeDataTypeInt,
    DDBridgeDataTypeInt2,
    DDBridgeDataTypeInt3,
    DDBridgeDataTypeInt4,
    DDBridgeDataTypeFloat,
    DDBridgeDataTypeColor3f,
    DDBridgeDataTypeColor3h,
    DDBridgeDataTypeColor4f,
    DDBridgeDataTypeColor4h,
    DDBridgeDataTypeFloat2,
    DDBridgeDataTypeFloat3,
    DDBridgeDataTypeFloat4,
    DDBridgeDataTypeHalf,
    DDBridgeDataTypeHalf2,
    DDBridgeDataTypeHalf3,
    DDBridgeDataTypeHalf4,
    DDBridgeDataTypeMatrix2f,
    DDBridgeDataTypeMatrix3f,
    DDBridgeDataTypeMatrix4f,
    DDBridgeDataTypeSurfaceShader,
    DDBridgeDataTypeGeometryModifier,
    DDBridgeDataTypeString,
    DDBridgeDataTypeToken,
    DDBridgeDataTypeAsset
};

@interface DDBridgeInputOutput : NSObject

@property (nonatomic, readonly) DDBridgeDataType type;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithType:(DDBridgeDataType)dataType name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, DDBridgeConstant) {
    DDBridgeConstantBool,
    DDBridgeConstantUchar,
    DDBridgeConstantInt,
    DDBridgeConstantUint,
    DDBridgeConstantHalf,
    DDBridgeConstantFloat,
    DDBridgeConstantTimecode,
    DDBridgeConstantString,
    DDBridgeConstantToken,
    DDBridgeConstantAsset,
    DDBridgeConstantMatrix2f,
    DDBridgeConstantMatrix3f,
    DDBridgeConstantMatrix4f,
    DDBridgeConstantQuatf,
    DDBridgeConstantQuath,
    DDBridgeConstantFloat2,
    DDBridgeConstantHalf2,
    DDBridgeConstantInt2,
    DDBridgeConstantFloat3,
    DDBridgeConstantHalf3,
    DDBridgeConstantInt3,
    DDBridgeConstantFloat4,
    DDBridgeConstantHalf4,
    DDBridgeConstantInt4,

    // semantic types
    DDBridgeConstantPoint3f,
    DDBridgeConstantPoint3h,
    DDBridgeConstantNormal3f,
    DDBridgeConstantNormal3h,
    DDBridgeConstantVector3f,
    DDBridgeConstantVector3h,
    DDBridgeConstantColor3f,
    DDBridgeConstantColor3h,
    DDBridgeConstantColor4f,
    DDBridgeConstantColor4h,
    DDBridgeConstantTexCoord2h,
    DDBridgeConstantTexCoord2f,
    DDBridgeConstantTexCoord3h,
    DDBridgeConstantTexCoord3f
};

typedef NS_ENUM(NSInteger, DDBridgeNodeType) {
    DDBridgeNodeTypeBuiltin,
    DDBridgeNodeTypeConstant,
    DDBridgeNodeTypeArguments,
    DDBridgeNodeTypeResults
};

@interface DDValueString : NSObject

@property (nonatomic, readonly) NSNumber *number;
@property (nonatomic, readonly) NSString *string;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNumber:(NSNumber *)number;
- (instancetype)initWithString:(NSString *)string;

@end

@interface DDBridgeConstantContainer : NSObject

@property (nonatomic, readonly) DDBridgeConstant constant;
@property (nonatomic, readonly, strong) NSArray<DDValueString *> *constantValues;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithConstant:(DDBridgeConstant)constant constantValues:(NSArray<DDValueString *> *)constantValues name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeBuiltin : NSObject

@property (nonatomic, readonly) NSString *definition;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDefinition:(NSString *)definition name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeNode : NSObject

@property (nonatomic, readonly) DDBridgeNodeType bridgeNodeType;
@property (nonatomic, readonly, strong) DDBridgeBuiltin *builtin;
@property (nonatomic, readonly) DDBridgeConstantContainer *constant;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBridgeNodeType:(DDBridgeNodeType)bridgeNodeType builtin:(DDBridgeBuiltin *)builtin constant:(DDBridgeConstantContainer *)constant NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeUpdateMaterial : NSObject

@property (nonatomic, strong, readonly, nullable) NSData *materialGraph;
@property (nonatomic, strong, readonly) NSString *identifier;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithMaterialGraph:(nullable NSData *)materialGraph identifier:(NSString *)identifier NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeReceiver : NSObject

- (void)initRenderer:(id<MTLTexture>)texture completionHandler:(void (^)(void))completionHandler;
- (void)renderWithTexture:(id<MTLTexture>)texture;
- (void)updateMesh:(DDBridgeUpdateMesh *)descriptor completionHandler:(void (^)(void))completionHandler;
- (void)updateTexture:(DDBridgeUpdateTexture *)descriptor completionHandler:(void (^)(void))completionHandler;
- (void)updateMaterial:(DDBridgeUpdateMaterial *)descriptor completionHandler:(void (^)(void))completionHandler;
- (void)setTransform:(simd_float4x4)transform;
- (void)setCameraDistance:(float)distance;
- (void)setPlaying:(BOOL)play;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDevice:(id<MTLDevice>)device NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeModelLoader : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (void)loadModelFrom:(NSURL *)url;
- (void)update:(double)deltaTime;
- (void)requestCompleted:(NSObject *)request;
- (void)setCallbacksWithModelUpdatedCallback:(void (^)(DDBridgeUpdateMesh *))modelUpdatedCallback textureUpdatedCallback:(void (^)(DDBridgeUpdateTexture *))textureUpdatedCallback materialUpdatedCallback:(void (^)(DDBridgeUpdateMaterial *))materialUpdatedCallback;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif

