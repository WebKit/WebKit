//
//  MTLCommandBuffer.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
    
NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;
@protocol MTLCommandQueue;
@protocol MTLBlitCommandEncoder;
@protocol MTLRenderCommandEncoder;
@protocol MTLParallelRenderCommandEncoder;
@protocol MTLComputeCommandEncoder;
@protocol MTLCommandQueue;
@protocol MTLDrawable;
@protocol MTLCommandBuffer;
@protocol MTLEvent;
@protocol MTLLogContainer;
@protocol MTLLogState;
@protocol MTLResourceStateCommandEncoder;
@protocol MTLAccelerationStructureCommandEncoder;
@class MTLRenderPassDescriptor;
@class MTLComputePassDescriptor;
@class MTLBlitPassDescriptor;
@class MTLResourceStatePassDescriptor;
@class MTLAccelerationStructurePassDescriptor;
@protocol MTLResidencySet;

/*!
 @enum MTLCommandBufferStatus
 
 @abstract MTLCommandBufferStatus reports the current stage in the lifetime of MTLCommandBuffer, as it proceeds to enqueued, committed, scheduled, and completed.
 
 @constant MTLCommandBufferStatusNotEnqueued
 The command buffer has not been enqueued yet.
 
 @constant MTLCommandBufferStatusEnqueued
 This command buffer is enqueued, but not committed.
 
 @constant MTLCommandBufferStatusCommitted
 Commited to its command queue, but not yet scheduled for execution.
 
 @constant MTLCommandBufferStatusScheduled
 All dependencies have been resolved and the command buffer has been scheduled for execution.
 
 @constant MTLCommandBufferStatusCompleted
 The command buffer has finished executing successfully: any blocks set with -addCompletedHandler: may now be called.
 
 @constant MTLCommandBufferStatusError
 Execution of the command buffer was aborted due to an error during execution.  Check -error for more information.
 */
typedef NS_ENUM(NSUInteger, MTLCommandBufferStatus) {
    MTLCommandBufferStatusNotEnqueued = 0,
    MTLCommandBufferStatusEnqueued = 1,
    MTLCommandBufferStatusCommitted = 2,
    MTLCommandBufferStatusScheduled = 3,
    MTLCommandBufferStatusCompleted = 4,
    MTLCommandBufferStatusError = 5,
} API_AVAILABLE(macos(10.11), ios(8.0));

 /*!
 @constant MTLCommandBufferErrorDomain
 @abstract An error domain for NSError objects produced by MTLCommandBuffer
 */
API_AVAILABLE(macos(10.11), ios(8.0))
MTL_EXTERN NSErrorDomain const MTLCommandBufferErrorDomain;

/*!
 @enum MTLCommandBufferError
 @abstract Error codes that can be found in MTLCommandBuffer.error
 
 @constant MTLCommandBufferErrorInternal
 An internal error that doesn't fit into the other categories. The actual low level error code is encoded in the local description.
 
 @constant MTLCommandBufferErrorTimeout
 Execution of this command buffer took too long, execution of this command was interrupted and aborted.
 
 @constant MTLCommandBufferErrorPageFault
 Execution of this command buffer generated an unserviceable GPU page fault. This can caused by buffer read write attribute mismatch or out of boundary access.
 
 @constant MTLCommandBufferErrorAccessRevoked
 Access to this device has been revoked because this client has been responsible for too many timeouts or hangs.
 
 @constant MTLCommandBufferErrorNotPermitted
 This process does not have access to use this device.
 
 @constant MTLCommandBufferErrorOutOfMemory
 Insufficient memory was available to execute this command buffer.
 
 @constant MTLCommandBufferErrorInvalidResource
 The command buffer referenced an invalid resource.  This is most commonly caused when the caller deletes a resource before executing a command buffer that refers to it.
 
 @constant MTLCommandBufferErrorMemoryless
 One or more internal resources limits reached that prevent using memoryless render pass attachments. See error string for more detail.
 
 @constant MTLCommandBufferErrorDeviceRemoved
 The device was physically removed before the command could finish execution

 @constant MTLCommandBufferErrorStackOverflow
 Execution of the command buffer was stopped due to Stack Overflow Exception. [MTLComputePipelineDescriptor maxCallStackDepth] setting needs to be checked.
 */

typedef NS_ENUM(NSUInteger, MTLCommandBufferError)
{
    MTLCommandBufferErrorNone = 0,
    MTLCommandBufferErrorInternal = 1,
    MTLCommandBufferErrorTimeout = 2,
    MTLCommandBufferErrorPageFault = 3,
    MTLCommandBufferErrorBlacklisted API_DEPRECATED_WITH_REPLACEMENT("MTLCommandBufferErrorAccessRevoked", macos(10.11, 13.0), ios(8.0, 16.0)) = 4, 
    MTLCommandBufferErrorAccessRevoked = 4,
    MTLCommandBufferErrorNotPermitted = 7,
    MTLCommandBufferErrorOutOfMemory = 8,
    MTLCommandBufferErrorInvalidResource = 9,
    MTLCommandBufferErrorMemoryless API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(10.0)) = 10,
    MTLCommandBufferErrorDeviceRemoved API_DEPRECATED("MTLCommandBufferErrorDeviceRemoved cannot occur on Apple Silicon", macos(10.13, 27.0)) API_UNAVAILABLE(ios) = 11,
    MTLCommandBufferErrorStackOverflow API_AVAILABLE(macos(12.0), ios(15.0)) = 12,
} API_AVAILABLE(macos(10.11), ios(8.0));

/*!
 @abstract Key in the userInfo for MTLCommandBufferError NSErrors. Value is an NSArray of MTLCommandBufferEncoderInfo objects in recorded order if an appropriate MTLCommandBufferErrorOption was set, otherwise the key will not exist in the userInfo dictionary.
 */
API_AVAILABLE(macos(11.0), ios(14.0))
MTL_EXTERN NSErrorUserInfoKey const MTLCommandBufferEncoderInfoErrorKey;

/*!
 @abstract Options for controlling the error reporting for Metal command buffer objects.
 
 @constant MTLCommandBufferErrorOptionNone
 No special error reporting.
 
 @constant MTLCommandBufferErrorOptionEncoderExecutionStatus
 Provide the execution status of the individual encoders within the command buffer. In the event of a command buffer error, populate the `userInfo` dictionary of the command buffer's NSError parameter, see MTLCommandBufferEncoderInfoErrorKey and MTLCommandBufferEncoderInfo. Note that enabling this error reporting option may increase CPU, GPU, and/or memory overhead on some platforms; testing for impact is suggested.
 */
typedef NS_OPTIONS(NSUInteger, MTLCommandBufferErrorOption) {
    MTLCommandBufferErrorOptionNone = 0,
    MTLCommandBufferErrorOptionEncoderExecutionStatus = 1 << 0,
} API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @abstract The error states for a Metal command encoder after command buffer execution.
 
 @constant MTLCommandEncoderErrorStateUnknown
 The state of the commands associated with the encoder is unknown (the error information was likely not requested).
 
 @constant MTLCommandEncoderErrorStateCompleted
 The commands associated with the encoder were completed.
 
 @constant MTLCommandEncoderErrorStateAffected
 The commands associated with the encoder were affected by an error, which may or may not have been caused by the commands themselves, and failed to execute in full.
 
 @constant MTLCommandEncoderErrorStatePending
 The commands associated with the encoder never started execution.
 
 @constant MTLCommandEncoderErrorStateFaulted
 The commands associated with the encoder caused an error.
 */
typedef NS_ENUM(NSInteger, MTLCommandEncoderErrorState) {
    MTLCommandEncoderErrorStateUnknown = 0,
    MTLCommandEncoderErrorStateCompleted = 1,
    MTLCommandEncoderErrorStateAffected = 2,
    MTLCommandEncoderErrorStatePending = 3,
    MTLCommandEncoderErrorStateFaulted = 4,
} API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @class MTLCommandBufferDescriptor
 @abstract An object that you use to configure new Metal command buffer objects.
*/
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLCommandBufferDescriptor : NSObject <NSCopying>

/*!
 @property retainedReferences
 @abstract If YES, the created command buffer holds strong references to objects needed for it to execute. If NO, the created command buffer does not hold strong references to objects needed for it to execute.
*/
@property (readwrite, nonatomic) BOOL retainedReferences;

/*!
 @property errorOptions
 @abstract A set of options to influence the error reporting of the created command buffer. See MTLCommandBufferErrorOption.
*/
@property (readwrite, nonatomic) MTLCommandBufferErrorOption errorOptions;

/*!
 @property logState
 @abstract Contains information related to shader logging.
*/
@property (readwrite, nonatomic, nullable, retain) id<MTLLogState> logState
        API_AVAILABLE(macos(15.0), ios(18.0));

@end // MTLCommandBufferDescriptor

/*!
 @abstract Provides execution status information for a Metal command encoder.
 */
API_AVAILABLE(macos(11.0), ios(14.0))
@protocol MTLCommandBufferEncoderInfo <NSObject>

/*!
 @abstract The debug label given to the associated Metal command encoder at command buffer submission.
*/
@property (readonly, nonatomic) NSString* label;

/*!
 @abstract The debug signposts inserted into the associated Metal command encoder.
*/
@property (readonly, nonatomic) NSArray<NSString*>* debugSignposts;

/*!
 @abstract The error state of the associated Metal command encoder.
*/
@property (readonly, nonatomic) MTLCommandEncoderErrorState errorState;

@end // MTLCommandBufferEncoderInfo


typedef void (^ NS_SWIFT_SENDABLE MTLCommandBufferHandler)(id <MTLCommandBuffer>);

/*!
 @enum MTLDispatchType
 
 @abstract MTLDispatchType Describes how a command encoder will execute dispatched work.
 
 @constant MTLDispatchTypeSerial
 Command encoder dispatches are executed in dispatched order.
 
 @constant MTLDispatchTypeConcurrent
 Command encoder dispatches are executed in parallel with each other. 
*/

typedef NS_ENUM(NSUInteger, MTLDispatchType){
    MTLDispatchTypeSerial,
    MTLDispatchTypeConcurrent,
}API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @protocol MTLCommandBuffer
 @abstract A serial list of commands for the device to execute.
 */
API_AVAILABLE(macos(10.11), ios(8.0))
@protocol MTLCommandBuffer <NSObject>

/*!
 @property device
 @abstract The device this resource was created against.
 */
@property (readonly) id <MTLDevice> device;

/*!
 @property commandQueue
 @abstract The command queue this command buffer was created from.
 */
@property (readonly) id <MTLCommandQueue> commandQueue;

/*!
 @property retainedReferences
 @abstract If YES, this command buffer holds strong references to objects needed to execute this command buffer.
 */
@property (readonly) BOOL retainedReferences;

/*!
 @abstract The set of options configuring the error reporting of the created command buffer.
*/
@property (readonly) MTLCommandBufferErrorOption errorOptions API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, copy, atomic) NSString *label;

@property (readonly) CFTimeInterval kernelStartTime API_AVAILABLE(macos(10.15), macCatalyst(13.0), ios(10.3));
@property (readonly) CFTimeInterval kernelEndTime API_AVAILABLE(macos(10.15), macCatalyst(13.0), ios(10.3));

/*!
@property logs
@abstract Logs generated by the command buffer during execution of the GPU commands. Valid after GPU execution is completed
*/
@property (readonly) id<MTLLogContainer> logs API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @property GPUStartTime
 @abstract The host time in seconds that GPU starts executing this command buffer. Returns zero if it has not started. This usually can be called in command buffer completion handler.
 */
@property (readonly) CFTimeInterval GPUStartTime API_AVAILABLE(macos(10.15), macCatalyst(13.0), ios(10.3));
/*!
 @property GPUEndTime
 @abstract The host time in seconds that GPU finishes executing this command buffer. Returns zero if CPU has not received completion notification. This usually can be called in command buffer completion handler.
 */
@property (readonly) CFTimeInterval GPUEndTime API_AVAILABLE(macos(10.15), macCatalyst(13.0), ios(10.3));

/*!
 @method enqueue
 @abstract Append this command buffer to the end of its MTLCommandQueue.
*/
- (void)enqueue;

/*!
 @method commit
 @abstract Commit a command buffer so it can be executed as soon as possible.
 */
- (void)commit;

/*!
 @method addScheduledHandler:block:
 @abstract Adds a block to be called when this command buffer has been scheduled for execution.
 */
- (void)addScheduledHandler:(MTLCommandBufferHandler)block;

/*!
 @method presentDrawable:
 @abstract Add a drawable present that will be invoked when this command buffer has been scheduled for execution.
 @discussion The submission thread will be lock stepped with present call been serviced by window server
 */
- (void)presentDrawable:(id <MTLDrawable>)drawable;

/*!
 @method presentDrawable:atTime:
 @abstract Add a drawable present for a specific host time that will be invoked when this command buffer has been scheduled for execution.
 @discussion The submission thread will be lock stepped with present call been serviced by window server
*/
- (void)presentDrawable:(id <MTLDrawable>)drawable atTime:(CFTimeInterval)presentationTime;

/*!
 @method presentDrawable:afterMinimumDuration:
 @abstract Add a drawable present for a specific host time that allows previous frame to be on screen for at least duration time.
 @param drawable The drawable to be presented
 @param duration The minimum time that previous frame should be displayed. The time is double preceision floating point in the unit of seconds.
 @discussion The difference of this API versus presentDrawable:atTime is that this API defers calculation of the presentation time until the previous frame's actual presentation time is known, thus to be able to maintain a more consistent and stable frame time. This also provides an easy way to set frame rate.
    The submission thread will be lock stepped with present call been serviced by window server 
 */
- (void)presentDrawable:(id <MTLDrawable>)drawable afterMinimumDuration:(CFTimeInterval)duration API_AVAILABLE(macos(10.15.4), ios(10.3), macCatalyst(13.4));

/*!
 @method waitUntilScheduled
 @abstract Synchronously wait for this command buffer to be scheduled.
 */
- (void)waitUntilScheduled NS_SWIFT_UNAVAILABLE_FROM_ASYNC("Use 'await scheduled()' instead.");

/*!
 @method addCompletedHandler:block:
 @abstract Add a block to be called when this command buffer has completed execution.
 */
- (void)addCompletedHandler:(MTLCommandBufferHandler)block;

/*!
 @method waitUntilCompleted
 @abstract Synchronously wait for this command buffer to complete.
 */
- (void)waitUntilCompleted NS_SWIFT_UNAVAILABLE_FROM_ASYNC("Use 'await completed()' instead.");

/*!
 @property status
 @abstract status reports the current stage in the lifetime of MTLCommandBuffer, as it proceeds to enqueued, committed, scheduled, and completed.
 */
@property (readonly) MTLCommandBufferStatus status;

/*!
 @property error
 @abstract If an error occurred during execution, the NSError may contain more details about the problem.
 */
@property (nullable, readonly) NSError *error;

/*!
 @method blitCommandEncoder
 @abstract returns a blit command encoder to encode into this command buffer.
 */
- (nullable id <MTLBlitCommandEncoder>)blitCommandEncoder;

/*!
 @method renderCommandEncoderWithDescriptor:
 @abstract returns a render command endcoder to encode into this command buffer.
 */
- (nullable id <MTLRenderCommandEncoder>)renderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor *)renderPassDescriptor;

/*!
 @method computeCommandEncoderWithDescriptor:
 @abstract returns a compute command endcoder to encode into this command buffer.
 */
- (nullable id <MTLComputeCommandEncoder>)computeCommandEncoderWithDescriptor:(MTLComputePassDescriptor *)computePassDescriptor API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @method blitCommandEncoderWithDescriptor:
 @abstract returns a blit command endcoder to encode into this command buffer.
 */
- (nullable id <MTLBlitCommandEncoder>)blitCommandEncoderWithDescriptor:(MTLBlitPassDescriptor *)blitPassDescriptor API_AVAILABLE(macos(11.0), ios(14.0));
/*!
 @method computeCommandEncoder
 @abstract returns a compute command encoder to encode into this command buffer.
 */
- (nullable id <MTLComputeCommandEncoder>)computeCommandEncoder;
/*! 
 @method computeCommandEncoderWithDispatchType
 @abstract returns a compute command encoder to encode into this command buffer. Optionally allow this command encoder to execute dispatches concurrently.
 @discussion On devices that do not support concurrent command encoders, this call is equivalent to computeCommandEncoder
 */

 - (nullable id<MTLComputeCommandEncoder>)computeCommandEncoderWithDispatchType:(MTLDispatchType) dispatchType API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method encodeWaitForEvent:value:
 @abstract Encodes a command that pauses execution of this command buffer until the specified event reaches a given value.
 @discussion This method may only be called if there is no current command encoder on the receiver.
*/
- (void)encodeWaitForEvent:(id <MTLEvent>)event value:(uint64_t)value API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method encodeSignalEvent:value:
 @abstract Encodes a command that signals an event with a given value.
 @discussion This method may only be called if there is no current command encoder on the receiver.
 */
- (void)encodeSignalEvent:(id <MTLEvent>)event value:(uint64_t)value API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method parallelRenderCommandEncoderWithDescriptor:
 @abstract returns a parallel render pass encoder to encode into this command buffer.
 */
- (nullable id <MTLParallelRenderCommandEncoder>)parallelRenderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor *)renderPassDescriptor;

- (nullable id<MTLResourceStateCommandEncoder>) resourceStateCommandEncoder API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));
- (nullable id<MTLResourceStateCommandEncoder>) resourceStateCommandEncoderWithDescriptor:(MTLResourceStatePassDescriptor *) resourceStatePassDescriptor API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

- (nullable id <MTLAccelerationStructureCommandEncoder>)accelerationStructureCommandEncoder API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

- (id <MTLAccelerationStructureCommandEncoder>)accelerationStructureCommandEncoderWithDescriptor:(MTLAccelerationStructurePassDescriptor *)descriptor API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method pushDebugGroup:
 @abstract Push a new named string onto a stack of string labels.
 */
- (void)pushDebugGroup:(NSString *)string API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @method popDebugGroup
 @abstract Pop the latest named string off of the stack.
 */
- (void)popDebugGroup API_AVAILABLE(macos(10.13), ios(11.0));

/*!
  @method useResidencySet
  @abstract Marks the residency set as part of the current command buffer execution. This ensures that the residency set is resident during execution of the command buffer.
 */
- (void)useResidencySet:(id <MTLResidencySet>)residencySet
                   API_AVAILABLE(macos(15.0), ios(18.0));
/*!
  @method useResidencySets
  @abstract Marks the residency sets as part of the current command buffer execution. This ensures that the residency sets are resident during execution of the command buffer.
 */
- (void)useResidencySets:(const id <MTLResidencySet> _Nonnull[_Nonnull])residencySets
                   count:(NSUInteger)count
                   API_AVAILABLE(macos(15.0), ios(18.0));

@end
NS_ASSUME_NONNULL_END
