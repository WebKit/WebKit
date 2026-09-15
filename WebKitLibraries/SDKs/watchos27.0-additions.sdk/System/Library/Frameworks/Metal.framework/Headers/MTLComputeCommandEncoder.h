//
//  MTLComputeCommandEncoder.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLCommandEncoder.h>
#import <Metal/MTLTexture.h>
#import <Metal/MTLCommandBuffer.h>
#import <Metal/MTLComputePass.h>
#import <Metal/MTLFence.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLFunction;
@protocol MTLBuffer;
@protocol MTLSamplerState;
@protocol MTLTexture;
@protocol MTLComputePipelineState;
@protocol MTLResource;
@protocol MTLVisibleFunctionTable;
@protocol MTLAccelerationStructure;

typedef struct {
    uint32_t threadgroupsPerGrid[3];
} MTLDispatchThreadgroupsIndirectArguments;

typedef struct {
    uint32_t threadsPerGrid[3];
    uint32_t threadsPerThreadgroup[3];
} MTLDispatchThreadsIndirectArguments;

typedef struct {
	uint32_t  stageInOrigin[3];
	uint32_t  stageInSize[3];
} MTLStageInRegionIndirectArguments API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @protocol MTLComputeCommandEncoder
 @abstract A command encoder that writes data parallel compute commands.
 */
API_AVAILABLE(macos(10.11), ios(8.0))
@protocol MTLComputeCommandEncoder <MTLCommandEncoder>

/*!
 @property dispatchType
 @abstract The dispatch type of the compute command encoder.
 */

@property (readonly) MTLDispatchType dispatchType API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method setComputePipelineState:
 @abstract Set the compute pipeline state that will be used.
 */
- (void)setComputePipelineState:(id <MTLComputePipelineState>)state;

/*!
 @method setBytes:length:atIndex:
 @brief Set the data (by copy) for a given buffer binding point.  This will remove any existing MTLBuffer from the binding point.
 */
- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3));

/*!
 @method setBuffer:offset:atIndex:
 @brief Set a global buffer for all compute kernels at the given bind point index.
 */
- (void)setBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;

/*!
 @method setBufferOffset:atIndex:
 @brief Set the offset within the current global buffer for all compute kernels at the given bind point index.
 */
- (void)setBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3));

/*!
 @method setBuffers:offsets:withRange:
 @brief Set an array of global buffers for all compute kernels with the given bind point range.
 */
- (void)setBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range;

/*!
  @brief
    sets kernel buffer at specified index with provided offset and stride.
    only call this when the kernel-buffer is part of the stageInputDescriptor
    and has set its stride to `MTLBufferLayoutStrideDynamic`
*/
- (void) setBuffer:(id<MTLBuffer>)buffer
            offset:(NSUInteger)offset
   attributeStride:(NSUInteger)stride
           atIndex:(NSUInteger)index
API_AVAILABLE(macos(14.0), ios(17.0));
/*!
  @brief
    sets an array of kernel buffers with provided offsets and strides with the
    given bind point range. Only call this when at least one buffer is part of
    the vertexDescriptor, other buffers must set `MTLAttributeStrideStatic`
*/
- (void) setBuffers:(const id<MTLBuffer> __nullable [__nonnull])buffers
            offsets:(const NSUInteger [__nonnull])offsets
   attributeStrides:(const NSUInteger [__nonnull])strides
          withRange:(NSRange)range
API_AVAILABLE(macos(14.0), ios(17.0));
/*!
  @brief
    only call this when the buffer-index is part of the stageInputDescriptor
    and has set its stride to `MTLBufferLayoutStrideDynamic`
*/
- (void) setBufferOffset:(NSUInteger)offset
         attributeStride:(NSUInteger)stride
                 atIndex:(NSUInteger)index
API_AVAILABLE(macos(14.0), ios(17.0));
/*!
  @brief
    only call this when the buffer-index is part of the stageInputDescriptor
    and has set its stride to `MTLBufferLayoutStrideDynamic`
*/
- (void)setBytes:(void const *)bytes
          length:(NSUInteger)length
 attributeStride:(NSUInteger)stride
         atIndex:(NSUInteger)index
API_AVAILABLE(macos(14.0), ios(17.0));


/*!
 * @method setVisibleFunctionTable:atBufferIndex:
 * @brief Set a visible function table at the given buffer index
 */
- (void)setVisibleFunctionTable:(nullable id <MTLVisibleFunctionTable>)visibleFunctionTable atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 * @method setVisibleFunctionTables:withBufferRange:
 * @brief Set visible function tables at the given buffer index range
 */
- (void)setVisibleFunctionTables:(const id <MTLVisibleFunctionTable> __nullable [__nonnull])visibleFunctionTables withBufferRange:(NSRange)range API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 * @method setIntersectionFunctionTable:atBufferIndex:
 * @brief Set a visible function table at the given buffer index
 */
- (void)setIntersectionFunctionTable:(nullable id <MTLIntersectionFunctionTable>)intersectionFunctionTable atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 * @method setIntersectionFunctionTables:withBufferRange:
 * @brief Set visible function tables at the given buffer index range
 */
- (void)setIntersectionFunctionTables:(const id <MTLIntersectionFunctionTable> __nullable [__nonnull])intersectionFunctionTables withBufferRange:(NSRange)range API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));


/*!
 @method setAccelerationStructure:atBufferIndex:
 @brief Set a global raytracing acceleration structure for all compute kernels at the given buffer bind point index.
 */
- (void)setAccelerationStructure:(nullable id <MTLAccelerationStructure>)accelerationStructure atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 @method setTexture:atIndex:
 @brief Set a global texture for all compute kernels at the given bind point index.
 */
- (void)setTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index;

/*!
 @method setTextures:withRange:
 @brief Set an array of global textures for all compute kernels with the given bind point range.
 */
- (void)setTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range;

/*!
 @method setSamplerState:atIndex:
 @brief Set a global sampler for all compute kernels at the given bind point index.
 */
- (void)setSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index;

/*!
 @method setSamplers:withRange:
 @brief Set an array of global samplers for all compute kernels with the given bind point range.
 */
- (void)setSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range;

/*!
 @method setSamplerState:lodMinClamp:lodMaxClamp:atIndex:
 @brief Set a global sampler for all compute kernels at the given bind point index.
 */
- (void)setSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index;

/*!
 @method setSamplers:lodMinClamps:lodMaxClamps:withRange:
 @brief Set an array of global samplers for all compute kernels with the given bind point range.
 */
- (void)setSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range;


/*!
 @method setThreadgroupMemoryLength:atIndex:
 @brief Set the threadgroup memory byte length at the binding point specified by the index. This applies to all compute kernels.
 */
- (void)setThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index;



/*!
 @method setImageblockWidth:height:
 @brief Set imageblock sizes.
 */
- (void)setImageblockWidth:(NSUInteger)width height:(NSUInteger)height API_AVAILABLE(ios(11.0), macos(11.0), macCatalyst(14.0), tvos(14.5));

/*
 @method setStageInRegion:region:
 @brief Set the region of the stage_in attributes to apply the compute kernel.
*/
- (void)setStageInRegion:(MTLRegion)region API_AVAILABLE(macos(10.12), ios(10.0));

/*
 @method setStageInRegionWithIndirectBuffer:indirectBufferOffset:
 @abstract sets the stage in region indirectly for the following indirect dispatch calls.
 @param indirectBuffer A buffer object that the device will read the stageIn region arguments from, see MTLStageInRegionIndirectArguments.
 @param indirectBufferOffset Byte offset within indirectBuffer to read arguments from. indirectBufferOffset must be a multiple of 4.
 */
- (void)setStageInRegionWithIndirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(10.14), ios(12.0));

/*
 @method dispatchThreadgroups:threadsPerThreadgroup:
 @abstract Enqueue a compute function dispatch as a multiple of the threadgroup size.
 */
- (void)dispatchThreadgroups:(MTLSize)threadgroupsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;

/*
 @method dispatchThreadgroupsWithIndirectBuffer:indirectBufferOffset:threadsPerThreadgroup:
 @abstract Enqueue a compute function dispatch using an indirect buffer for threadgroupsPerGrid see MTLDispatchThreadgroupsIndirectArguments.
 @param indirectBuffer A buffer object that the device will read dispatchThreadgroups arguments from, see MTLDispatchThreadgroupsIndirectArguments.
 @param indirectBufferOffset Byte offset within @a indirectBuffer to read arguments from.  @a indirectBufferOffset must be a multiple of 4.
 */
- (void)dispatchThreadgroupsWithIndirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup API_AVAILABLE(macos(10.11), ios(9.0));

/*
 @method dispatchThreads:threadsPerThreadgroup:
 @abstract Enqueue a compute function dispatch using an arbitrarily-sized grid.
 @discussion threadsPerGrid does not have to be a multiple of the  threadGroup size
 */
- (void)dispatchThreads:(MTLSize)threadsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup API_AVAILABLE(macos(10.13), ios(11.0), tvos(14.5));

/*!
 @method updateFence:
 @abstract Update the fence to capture all GPU work so far enqueued by this encoder.
 @discussion The fence is updated at kernel submission to maintain global order and prevent deadlock.
 Drivers may delay fence updates until the end of the encoder. Drivers may also wait on fences at the beginning of an encoder. It is therefore illegal to wait on a fence after it has been updated in the same encoder.
 */
- (void)updateFence:(id <MTLFence>)fence API_AVAILABLE(macos(10.13), ios(10.0));

/*!
 @method waitForFence:
 @abstract Prevent further GPU work until the fence is reached.
 @discussion The fence is evaluated at kernel submission to maintain global order and prevent deadlock.
 Drivers may delay fence updates until the end of the encoder. Drivers may also wait on fences at the beginning of an encoder. It is therefore illegal to wait on a fence after it has been updated in the same encoder.
 */
- (void)waitForFence:(id <MTLFence>)fence API_AVAILABLE(macos(10.13), ios(10.0));


/*!
 * @method useResource:usage:
 * @abstract Declare that a resource may be accessed by the command encoder through an argument buffer
 * 
 * @discussion For tracked MTLResources, this method protects against data hazards. This method must be called before encoding any dispatch commands which may access the resource through an argument buffer.
 * @warning Prior to iOS 13, macOS 10.15, this method does not protect against data hazards. If you are deploying to older versions of macOS or iOS, use fences to ensure data hazards are resolved.
 */
- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 * @method useResources:count:usage:
 * @abstract Declare that an array of resources may be accessed through an argument buffer by the command encoder
 * @discussion For tracked MTL Resources, this method protects against data hazards. This method must be called before encoding any dispatch commands which may access the resources through an argument buffer.
 * @warning Prior to iOS 13, macOS 10.15, this method does not protect against data hazards. If you are deploying to older versions of macOS or iOS, use fences to ensure data hazards are resolved.
 */
- (void)useResources:(const id <MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 * @method useHeap:
 * @abstract Declare that the resources allocated from a heap may be accessed as readonly by the render pass through an argument buffer
 * @discussion For tracked MTLHeaps, this method protects against data hazards. This method must be called before encoding any dispatch commands which may access the resources allocated from the heap through an argument buffer. This method may cause all of the color attachments allocated from the heap to become decompressed. Therefore, it is recommended that the useResource:usage: or useResources:count:usage: methods be used for color attachments instead, with a minimal (i.e. read-only) usage.
 * @warning Prior to iOS 13, macOS 10.15, this method does not protect against data hazards. If you are deploying to older versions of macOS or iOS, use fences to ensure data hazards are resolved.
 */
- (void)useHeap:(id <MTLHeap>)heap API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 * @method useHeaps:count:
 * @abstract Declare that the resources allocated from an array of heaps may be accessed as readonly by the render pass through an argument buffer
 * @discussion For tracked MTLHeaps, this method protects against data hazards. This method must be called before encoding any dispatch commands which may access the resources allocated from the heaps through an argument buffer. This method may cause all of the color attachments allocated from the heaps to become decompressed. Therefore, it is recommended that the useResource:usage: or useResources:count:usage: methods be used for color attachments instead, with a minimal (i.e. read-only) usage.
 * @warning Prior to iOS 13, macOS 10.15, this method does not protect against data hazards. If you are deploying to older versions of macOS or iOS, use fences to ensure data hazards are resolved.
 */
- (void)useHeaps:(const id <MTLHeap> __nonnull[__nonnull])heaps count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(11.0));
    

/*!
 * @method executeCommandsInBuffer:withRange:
 * @abstract Execute commands in the buffer within the range specified.
 * @discussion The same indirect command buffer may be executed any number of times within the same encoder.
 */
- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer withRange:(NSRange)executionRange API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0));

/*!
 * @method executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:
 * @abstract Execute commands in the buffer within the range specified by the indirect range buffer.
 * @param indirectRangeBuffer An indirect buffer from which the device reads the execution range parameter, as laid out in the MTLIndirectCommandBufferExecutionRange structure.
 * @param indirectBufferOffset The byte offset within indirectBuffer where the execution range parameter is located. Must be a multiple of 4 bytes.
 * @discussion The same indirect command buffer may be executed any number of times within the same encoder.
 */
- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandbuffer indirectBuffer:(id<MTLBuffer>)indirectRangeBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0));



/*!
 *@method memoryBarrierWithScope
 *@abstract Encodes a barrier between currently dispatched kernels in a concurrent compute command encoder and any subsequent ones on a specified resource group
 *@discussion  This API ensures that all dispatches in the encoder have completed execution and their side effects are visible to subsequent dispatches in that encoder. Calling barrier on a serial encoder is allowed, but ignored.
 */
-(void)memoryBarrierWithScope:(MTLBarrierScope)scope API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 *@method memoryBarrierWithResources
 *@abstract Encodes a barrier between currently dispatched kernels in a concurrent compute command encoder and any subsequent ones on an array of resources.
 *@discussion  This API ensures that all dispatches in the encoder have completed execution and side effects on the specified resources are visible to subsequent dispatches in that encoder. Calling barrier on a serial encoder is allowed, but ignored.
 */
- (void)memoryBarrierWithResources:(const id<MTLResource> __nonnull [__nonnull])resources count:(NSUInteger)count API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method sampleCountersInBuffer:atSampleIndex:withBarrier:
 @abstract Sample hardware counters at this point in the compute encoder and
 store the counter sample into the sample buffer at the specified index.
 @param sampleBuffer The sample buffer to sample into
 @param sampleIndex The index into the counter buffer to write the sample
 @param barrier Insert a barrier before taking the sample.  Passing
 YES will ensure that all work encoded before this operation in the encoder is
 complete but does not isolate the work with respect to other encoders.  Passing
 NO will allow the sample to be taken concurrently with other operations in this
 encoder.
 In general, passing YES will lead to more repeatable counter results but
 may negatively impact performance.  Passing NO will generally be higher performance
 but counter results may not be repeatable.
 @discussion On devices where MTLCounterSamplingPointAtDispatchBoundary is unsupported,
 this method is not available and will generate an error if called.
 */
-(void)sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sampleBuffer
                atSampleIndex:(NSUInteger)sampleIndex
                  withBarrier:(BOOL)barrier API_AVAILABLE(macos(10.15), ios(14.0));

@end
NS_ASSUME_NONNULL_END
