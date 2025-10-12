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

#import <Foundation/Foundation.h>

#import <simd/simd.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@interface DDBridgeVertexAttributeFormat : NSObject

@property (nonatomic, readonly) int32_t semantic;
@property (nonatomic, readonly) int32_t format;
@property (nonatomic, readonly) int32_t layoutIndex;
@property (nonatomic, readonly) int32_t offset;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSemantic:(int32_t)semantic format:(int32_t)format layoutIndex:(int32_t)layoutIndex offset:(int32_t)offset NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeVertexLayout : NSObject

@property (nonatomic, readonly) int32_t bufferIndex;
@property (nonatomic, readonly) int32_t bufferOffset;
@property (nonatomic, readonly) int32_t bufferStride;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(int32_t)bufferIndex bufferOffset:(int32_t)bufferOffset bufferStride:(int32_t)bufferStride NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeAddMeshRequest : NSObject

@property (nonatomic, readonly) int32_t indexCapacity;
@property (nonatomic, readonly) int32_t indexType;
@property (nonatomic, readonly) int32_t vertexBufferCount;
@property (nonatomic, readonly) int32_t vertexCapacity;
@property (nonatomic, readonly, nullable) NSArray<DDBridgeVertexAttributeFormat*>* vertexAttributes;
@property (nonatomic, readonly, nullable) NSArray<DDBridgeVertexLayout*>* vertexLayouts;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexCapacity:(int32_t)indexCapacity
    indexType:(int32_t)indexType
    vertexBufferCount:(int32_t)vertexBufferCount
    vertexCapacity:(int32_t)vertexCapacity
    vertexAttributes:(nullable NSArray<DDBridgeVertexAttributeFormat*>*)vertexAttributes
    vertexLayouts:(nullable NSArray<DDBridgeVertexLayout*>*)vertexLayouts NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeMeshPart : NSObject

@property (nonatomic, readonly) unsigned long indexOffset;
@property (nonatomic, readonly) unsigned long indexCount;
@property (nonatomic, readonly) unsigned long topology;
@property (nonatomic, readonly) unsigned long materialIndex;
@property (nonatomic, readonly) simd_float3 boundsMin;
@property (nonatomic, readonly) simd_float3 boundsMax;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexOffset:(unsigned long)indexOffset indexCount:(unsigned long)indexCount topology:(unsigned long)topology materialIndex:(unsigned long)materialIndex boundsMin:(simd_float3)boundsMin boundsMax:(simd_float3)boundsMax NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeSetPart : NSObject

@property (nonatomic, readonly) long partIndex;
@property (nonatomic, readonly, strong) DDBridgeMeshPart *part;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndex:(long)index part:(DDBridgeMeshPart*)part NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeSetRenderFlags : NSObject

@property (nonatomic, readonly) long partIndex;
@property (nonatomic, readonly) uint64_t renderFlags;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndex:(long)index renderFlags:(uint64_t)renderFlags NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeReplaceVertices : NSObject

@property (nonatomic, readonly) long bufferIndex;

@property (nonatomic, readonly, strong) NSData* buffer;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(long)bufferIndex buffer:(NSData*)buffer NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeChainedFloat4x4 : NSObject

@property (nonatomic) simd_float4x4 transform;
@property (nonatomic, nullable) DDBridgeChainedFloat4x4 *next;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithTransform:(simd_float4x4)transform NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeUpdateMesh : NSObject

@property (nonatomic, readonly) long partCount;
@property (nonatomic, readonly, nullable) NSArray<DDBridgeSetPart*> *parts;
@property (nonatomic, readonly, nullable) NSArray<DDBridgeSetRenderFlags*> *renderFlags;
@property (nonatomic, readonly, nullable) NSArray<DDBridgeReplaceVertices*> *vertices;
@property (nonatomic, readonly, nullable) NSData *indices;
@property (nonatomic, readonly) simd_float4x4 transform;
@property (nonatomic, readonly, nullable) DDBridgeChainedFloat4x4 *instanceTransforms;
@property (nonatomic, readonly, nullable) NSArray<NSUUID *> *materialIds;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPartCount:(long)partCount
    parts:(nullable NSArray<DDBridgeSetPart *> *)WebSetPart
    renderFlags:(nullable NSArray<DDBridgeSetRenderFlags *> *)renderFlags
    vertices:(nullable NSArray<DDBridgeReplaceVertices *> *)vertices
    indices:(nullable NSData *)indices
    transform:(simd_float4x4)transform
    instanceTransforms:(nullable DDBridgeChainedFloat4x4 *)instanceTransforms
    materialIds:(nullable NSArray<NSUUID *> *)materialIds NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeReceiver : NSObject

- (void)setDeviceWithDevice:(id<MTLDevice>)device;
- (void)renderWithTexture:(id<MTLTexture>)texture;
- (bool)addMesh:(DDBridgeAddMeshRequest*)descriptor identifier:(NSUUID*)identifier;
- (void)updateMesh:(DDBridgeUpdateMesh*)descriptor identifier:(NSUUID*)identifier;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
