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

#import <Foundation/Foundation.h>

#import <simd/simd.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@interface WebDDVertexAttributeFormat : NSObject

@property (nonatomic, readonly) int32_t semantic;
@property (nonatomic, readonly) int32_t format;
@property (nonatomic, readonly) int32_t layoutIndex;
@property (nonatomic, readonly) int32_t offset;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSemantic:(int32_t)semantic format:(int32_t)format layoutIndex:(int32_t)layoutIndex offset:(int32_t)offset NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDVertexLayout : NSObject

@property (nonatomic, readonly) int32_t bufferIndex;
@property (nonatomic, readonly) int32_t bufferOffset;
@property (nonatomic, readonly) int32_t bufferStride;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(int32_t)bufferIndex bufferOffset:(int32_t)bufferOffset bufferStride:(int32_t)bufferStride NS_DESIGNATED_INITIALIZER;

@end

@interface WebAddMeshRequest : NSObject

@property (nonatomic, readonly) int32_t indexCapacity;
@property (nonatomic, readonly) int32_t indexType;
@property (nonatomic, readonly) int32_t vertexBufferCount;
@property (nonatomic, readonly) int32_t vertexCapacity;
@property (nonatomic, readonly, strong) NSArray<WebDDVertexAttributeFormat*> *vertexAttributes;
@property (nonatomic, readonly, strong) NSArray<WebDDVertexLayout*> *vertexLayouts;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithIndexCapacity:(int32_t)indexCapacity indexType:(int32_t)indexType vertexBufferCount:(int32_t)vertexBufferCount vertexCapacity:(int32_t)vertexCapacity vertexAttributes:(NSArray<WebDDVertexAttributeFormat*> *)vertexAttributes vertexLayouts:(NSArray<WebDDVertexLayout*> *)vertexLayouts NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDMeshPart : NSObject

@property (nonatomic, readonly) unsigned long indexOffset;
@property (nonatomic, readonly) unsigned long indexCount;
@property (nonatomic, readonly) unsigned long topology;
@property (nonatomic, readonly) unsigned long materialIndex;
@property (nonatomic, readonly) simd_float3 boundsMin;
@property (nonatomic, readonly) simd_float3 boundsMax;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexOffset:(unsigned long)indexOffset indexCount:(unsigned long)indexCount topology:(unsigned long)topology materialIndex:(unsigned long)materialIndex boundsMin:(simd_float3)boundsMin boundsMax:(simd_float3)boundsMax NS_DESIGNATED_INITIALIZER;

@end

@interface WebSetPart : NSObject

@property (nonatomic, readonly) long partIndex;
@property (nonatomic, strong, readonly) WebDDMeshPart *part;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndex:(long)index part:(WebDDMeshPart*)part NS_DESIGNATED_INITIALIZER;

@end

@interface WebSetRenderFlags : NSObject

@property (nonatomic, readonly) long partIndex;
@property (nonatomic, readonly) uint64_t renderFlags;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndex:(long)index renderFlags:(uint64_t)renderFlags NS_DESIGNATED_INITIALIZER;

@end

@interface WebReplaceVertices : NSObject

@property (nonatomic, readonly) long bufferIndex;

@property (nonatomic, strong, readonly) NSData* buffer;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(long)bufferIndex buffer:(NSData*)buffer NS_DESIGNATED_INITIALIZER;

@end

@interface WebChainedFloat4x4 : NSObject

@property (nonatomic, readonly) simd_float4x4 transform;
@property (nonatomic, nullable, strong) WebChainedFloat4x4 *next;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithTransform:(simd_float4x4)transform NS_DESIGNATED_INITIALIZER;

@end

@interface WebUpdateMeshRequest : NSObject

@property (nonatomic, readonly) long partCount;
@property (nonatomic, nullable, strong, readonly) NSArray<WebSetPart *> *parts;
@property (nonatomic, nullable, strong, readonly) NSArray<WebSetRenderFlags *> *renderFlags;
@property (nonatomic, nullable, strong, readonly) NSArray<WebReplaceVertices *> *vertices;
@property (nonatomic, nullable, strong, readonly) NSData *indices;
@property (nonatomic, readonly) simd_float4x4 transform;
@property (nonatomic, nullable, strong) WebChainedFloat4x4 *instanceTransforms;
@property (nonatomic, nullable, strong, readonly) NSArray<NSUUID *> *materialIds;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPartCount:(long)partCount
    parts:(nullable NSArray<WebSetPart *> *)WebSetPart
    renderFlags:(nullable NSArray<WebSetRenderFlags *> *)renderFlags
    vertices:(nullable NSArray<WebReplaceVertices *> *)vertices
    indices:(nullable NSData *)indices
    transform:(simd_float4x4)transform
    instanceTransforms:(nullable WebChainedFloat4x4 *)instanceTransforms
    materialIds:(nullable NSArray<NSUUID *> *)materialIds NS_DESIGNATED_INITIALIZER;

@end

@interface WebUSDModelLoader : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (void)loadModelFrom:(NSURL *)url;
- (void)setCallbacksWithModelAddedCallback:(void (^)(WebAddMeshRequest *))addRequest modelUpdatedCallback:(void (^)(WebUpdateMeshRequest *))modelUpdatedCallback;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
