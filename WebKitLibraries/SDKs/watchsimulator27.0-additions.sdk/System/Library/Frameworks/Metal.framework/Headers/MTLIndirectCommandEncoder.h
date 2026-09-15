//
//  MTLIndirectCommandEncoder.h
//  Metal
//
//  Copyright © 2017 Apple, Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLStageInputOutputDescriptor.h>
#import <Metal/MTLRenderPipeline.h>

NS_ASSUME_NONNULL_BEGIN

/*
 @abstract
 Describes a CPU-recorded indirect render command
 */
API_AVAILABLE(macos(10.14), ios(12.0))
@protocol MTLIndirectRenderCommand <NSObject>
- (void)setRenderPipelineState:(id <MTLRenderPipelineState>)pipelineState API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(13.0));

- (void)setVertexBuffer:(id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setFragmentBuffer:(id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;

/*!
  @brief
    sets vertex buffer at specified index with provided offset and stride.
    only call this when the buffer-index is part of the vertexDescriptor and
    has set its stride to `MTLBufferLayoutStrideDynamic`
*/
- (void) setVertexBuffer:(nonnull id<MTLBuffer>)buffer
                  offset:(NSUInteger)offset
         attributeStride:(NSUInteger)stride
                 atIndex:(NSUInteger)index
API_AVAILABLE(macos(14.0), ios(17.0));


- (void)        drawPatches:(NSUInteger)numberOfPatchControlPoints patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer
     patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance
   tessellationFactorBuffer:(id <MTLBuffer>)buffer tessellationFactorBufferOffset:(NSUInteger)offset tessellationFactorBufferInstanceStride:(NSUInteger)instanceStride API_AVAILABLE(tvos(14.5));



- (void)drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints  patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer
    patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset controlPointIndexBuffer:(id <MTLBuffer>)controlPointIndexBuffer
controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset instanceCount:(NSUInteger)instanceCount
              baseInstance:(NSUInteger)baseInstance tessellationFactorBuffer:(id <MTLBuffer>)buffer
tessellationFactorBufferOffset:(NSUInteger)offset tessellationFactorBufferInstanceStride:(NSUInteger)instanceStride API_AVAILABLE(tvos(14.5));

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount baseVertex:(NSInteger)baseVertex baseInstance:(NSUInteger)baseInstance;

- (void)setObjectThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));
- (void)setObjectBuffer:(id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));
- (void)setMeshBuffer:(id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));
- (void)drawMeshThreadgroups:(MTLSize)threadgroupsPerGrid // MTLIndirectCommandTypeDrawMeshThreadgroups
 threadsPerObjectThreadgroup:(MTLSize)threadsPerObjectThreadgroup
   threadsPerMeshThreadgroup:(MTLSize)threadsPerMeshThreadgroup API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));
- (void)     drawMeshThreads:(MTLSize)threadsPerGrid // MTLIndirectCommandTypeDrawMeshThreads
 threadsPerObjectThreadgroup:(MTLSize)threadsPerObjectThreadgroup
   threadsPerMeshThreadgroup:(MTLSize)threadsPerMeshThreadgroup API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));
- (void)setBarrier API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));
- (void)clearBarrier API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));


- (void)setDepthStencilState:(nullable id<MTLDepthStencilState>)depthStencilState API_AVAILABLE(macos(26.0), ios(26.0));
- (void)setDepthBias:(float)depthBias slopeScale:(float)slopeScale clamp:(float)clamp API_AVAILABLE(macos(26.0), ios(26.0));
- (void)setDepthClipMode:(MTLDepthClipMode)depthClipMode API_AVAILABLE(macos(26.0), ios(26.0));
- (void)setCullMode:(MTLCullMode)cullMode API_AVAILABLE(macos(26.0), ios(26.0));
- (void)setFrontFacingWinding:(MTLWinding)frontFacingWinding API_AVAILABLE(macos(26.0), ios(26.0));
- (void)setTriangleFillMode:(MTLTriangleFillMode)fillMode API_AVAILABLE(macos(26.0), ios(26.0));

- (void)reset;


@end

API_AVAILABLE(ios(13.0), macos(11.0))
@protocol MTLIndirectComputeCommand <NSObject>
- (void)setComputePipelineState:(id <MTLComputePipelineState>)pipelineState API_AVAILABLE(ios(13.0), macos(11.0));

- (void)setKernelBuffer:(id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;

/*!
  @brief
    sets kernel buffer at specified index with provided offset and stride.
    only call this when the buffer-index is part of the stageInputDescriptor
    and has set its stride to `MTLBufferLayoutStrideDynamic`
*/
- (void)setKernelBuffer:(nonnull id<MTLBuffer>)buffer
                 offset:(NSUInteger)offset
        attributeStride:(NSUInteger)stride
                atIndex:(NSUInteger)index
API_AVAILABLE(macos(14.0), ios(17.0));


- (void)concurrentDispatchThreadgroups:(MTLSize)threadgroupsPerGrid
                 threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;
- (void)concurrentDispatchThreads:(MTLSize)threadsPerGrid
            threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;

- (void)setBarrier;

- (void)clearBarrier;

- (void)setImageblockWidth:(NSUInteger)width height:(NSUInteger)height API_AVAILABLE(ios(14.0), macos(11.0));

- (void)reset;


- (void)setThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index;
- (void)setStageInRegion:(MTLRegion)region;
@end

NS_ASSUME_NONNULL_END

