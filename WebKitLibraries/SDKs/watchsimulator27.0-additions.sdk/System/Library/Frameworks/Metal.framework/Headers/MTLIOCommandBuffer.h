//
//  MTLIOCommandBuffer.h
//  Metal
//
//  Copyright © 2022 Apple, Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>



NS_ASSUME_NONNULL_BEGIN

@protocol MTLIOCommandBuffer;
@protocol MTLIOFileHandle;

/* Used by MTLIOCommandBuffer to indicate completion status. */
typedef NS_ENUM(NSInteger, MTLIOStatus) {
    MTLIOStatusPending = 0,
    MTLIOStatusCancelled = 1,
    MTLIOStatusError = 2,
    MTLIOStatusComplete = 3,
} API_AVAILABLE(macos(13.0), ios(16.0));

typedef void (^ NS_SWIFT_SENDABLE MTLIOCommandBufferHandler)(id<MTLIOCommandBuffer>);

/*!
 @protocol MTLIOCommandBuffer
 @abstract represents a list of IO commands for a queue to execute
*/
API_AVAILABLE(macos(13.0), ios(16.0))
@protocol MTLIOCommandBuffer <NSObject>

/*!
 @method addCompletedHandler:block:
 @abstract Add a block to be called when this command buffer has completed execution.
 */
- (void) addCompletedHandler:(MTLIOCommandBufferHandler)block;

/*!
 @method loadBytes:size:sourceHandle:sourceHandleOffset
 @abstract Encodes a command that loads from a handle
 and offset into a memory location.
 */
- (void) loadBytes:(void *)pointer
            size:(NSUInteger)size
            sourceHandle:(id<MTLIOFileHandle>)sourceHandle
            sourceHandleOffset:(NSUInteger)sourceHandleOffset;

/*!
 @method loadBuffer:offset:size:sourceHandle:sourceHandleOffset
 @abstract Encodes a command that loads from a handle
 and offset into a buffer and an offset.
 */
- (void) loadBuffer:(id<MTLBuffer>)buffer
            offset:(NSUInteger)offset
            size:(NSUInteger)size
            sourceHandle:(id<MTLIOFileHandle>)sourceHandle
            sourceHandleOffset:(NSUInteger)sourceHandleOffset;

/*!
 @method loadTexture:texture:slice:level:size:sourceBytesPerRow:sourceBytesPerImage:destinationOrigin:sourceHandle:sourceHandleOffset:
 @abstract Encodes a command that loads a region from a handle
 and offset into a texture at a given slice, level and origin.
 */
- (void) loadTexture:(id<MTLTexture>)texture
            slice:(NSUInteger)slice
            level:(NSUInteger)level
            size:(MTLSize)size
            sourceBytesPerRow:(NSUInteger)sourceBytesPerRow
            sourceBytesPerImage:(NSUInteger)sourceBytesPerImage
            destinationOrigin:(MTLOrigin)destinationOrigin
            sourceHandle:(id<MTLIOFileHandle>)sourceHandle
            sourceHandleOffset:(NSUInteger)sourceHandleOffset;
/*!
 @method copyStatusToBuffer:buffer:offset
 @abstract Encodes a command that writes the status of this commandBuffer upon completion
 to a buffer at a given offset
 */
- (void) copyStatusToBuffer:(id<MTLBuffer>)buffer
                       offset:(NSUInteger)offset;

/*!
 @method commit
 @abstract Commit a command buffer so it can be executed as soon as possible.
 */
- (void) commit;

/*!
 @method waitUntilCompleted
 @abstract Synchronously wait for this command buffer to complete.
 */
- (void) waitUntilCompleted;

/*!
 @method tryCancel
 @abstract request a cancellation of an in-flight command buffer.
 */
- (void) tryCancel;

/*!
 @method addBarrier
 @abstract add a barrier that starts subsequent commands after all
 the previously encoded commands have completed.
 */
- (void) addBarrier;

/*!
 @method pushDebugGroup:
 @abstract Push a new named string onto a stack of string labels.
 */
- (void)pushDebugGroup:(NSString *)string;

/*!
 @method popDebugGroup
 @abstract Pop the latest named string off of the stack.
 */
- (void)popDebugGroup;



/*!
 @method enqueue
 @abstract Append this command buffer to the end of its MTLCommandQueue.
*/
- (void) enqueue;

/*!
@method waitForEvent:value:
@abstract Encodes a command that pauses execution of this command buffer until the specified event reaches a given value.
*/
- (void) waitForEvent:(id<MTLSharedEvent>)event value:(uint64_t)value;

/*!
 @method signalEvent:value:
 @abstract Encodes a command that signals an event with a given value.
 */
- (void) signalEvent:(id<MTLSharedEvent>)event value:(uint64_t)value;

/*!
 @property label
 @abstract An optional label for this handle.
*/
@property (nullable, copy, atomic) NSString *label;

/*!
 @property status
 @abstract status reports the completion status of the MTLIOCommandBuffer, pending, cancelled, error or complete.
 */
@property (readonly) MTLIOStatus status;

/*!
 @property error
 @abstract If an error occurred during execution, the NSError may contain more details about the problem.
 */
@property (nullable, readonly) NSError *error;

@end

NS_ASSUME_NONNULL_END


