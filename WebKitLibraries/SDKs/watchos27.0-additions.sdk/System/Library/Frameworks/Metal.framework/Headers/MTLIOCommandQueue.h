//
//  MTLIOCommandQueue.h
//  Metal
//
//  Copyright © 2022 Apple, Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>



NS_ASSUME_NONNULL_BEGIN

@protocol MTLBuffer;
@protocol MTLIOCommandBuffer;


/* Used in MTLIOCommandQueueDescriptor to set the MTLIOQueue priority at creation time. */
typedef NS_ENUM(NSInteger, MTLIOPriority) {
    MTLIOPriorityHigh = 0,
    MTLIOPriorityNormal = 1,
    MTLIOPriorityLow = 2,
} API_AVAILABLE(macos(13.0), ios(16.0));

/* Used in MTLIOCommandQueueDescriptor to set the MTLIOQueue type at creation time. */
typedef NS_ENUM(NSInteger, MTLIOCommandQueueType) {
    MTLIOCommandQueueTypeConcurrent = 0,
    MTLIOCommandQueueTypeSerial = 1,
} API_AVAILABLE(macos(13.0), ios(16.0));

API_AVAILABLE(macos(13.0), ios(16.0))
MTL_EXTERN NSErrorDomain const MTLIOErrorDomain;

/* Used by MTLIOFileHandle creation functions to indicate an error. */
typedef NS_ERROR_ENUM(MTLIOErrorDomain, MTLIOError) {
    MTLIOErrorURLInvalid       = 1,
    MTLIOErrorInternal         = 2,
} API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @protocol MTLIOCommandQueue
 @abstract Represents a queue that schedules command buffers containing command that
 read from handle objects and write to MTLResource objects.
 */
API_AVAILABLE(macos(13.0), ios(16.0)) NS_SWIFT_SENDABLE
@protocol MTLIOCommandQueue <NSObject>


/*!
 @method enqueueBarrier
 @abstract Inserts a barrier that ensures that all commandBuffers commited
 prior are completed before any commandBuffers after start execution.
 @discussion A serial commandQueue has implicit barriers between
 each commandBuffer.
 */
- (void) enqueueBarrier;


/*!
 @method commandBuffer
 @abstract Vends an autoreleased commandBuffer that can be used to
 encode  commands that read from handle objects and write to MTLResource objects.
 */
- (id<MTLIOCommandBuffer>)commandBuffer;


/*!
 @method commandBufferWithUnretainedReferences
 @abstract Vends an autoreleased commandBuffer that can be used to
 encode  commands that read from handle objects and write to MTLResource objects.
 This commandBuffer does not retain objects referenced by the commandBuffer
 as an optimization.
 @discussion For correct execution its the application's responsibility to retain
 objects referenced by commands within the commandBuffer.
 */
- (id<MTLIOCommandBuffer>)commandBufferWithUnretainedReferences;

/*!
 @property label
 @abstract An optional label for this handle.
*/
@property (nullable, copy, atomic) NSString *label;


@end

/*!
 @protocol MTLIOScratchBuffer
 @abstract An extendible protocol that can be used to wrap the buffers vended by
 a custom allocator. The underlying buffer is used as scratch space for IO commands
 that need it.
 */
API_AVAILABLE(macos(13.0), ios(16.0))
@protocol MTLIOScratchBuffer<NSObject>

@property (readonly) id<MTLBuffer> buffer;

@end

/*!
 @protocol MTLIOScratchBufferAllocator
 @abstract An extendible protocol that can implement a custom allocator passed
 to the queue descriptor.
 @discussion If provided, the queue will call newScratchBufferWithMinimumSize
 when it needs scratch storage for IO commands. When the commands that use the memory
 complete they return the storage by dealloc'ing the MTLIOScratchBuffer objects (where
 the application can optionally pool the memory for use by future commands.
 */
API_AVAILABLE(macos(13.0), ios(16.0))
@protocol MTLIOScratchBufferAllocator <NSObject>

/*!
 @method newScratchBufferWithMinimumSize:minimumSize
 @abstract This method is called when additional scratch memory is required by a load command.
 The scratch buffer returned should NOT be an autoreleased object.
 @discussion  Scratch memory is needed for cases where a texture is being copied to. minimumSize
 is the smallest buffer that will allow the command to execute, however a larger buffer can be provided and
 susequent commands will be able to use it, thus avoiding the need for an additional callback. Returning nil
 from the function will result in the load command being skipped and the commandBuffer getting cancelled.
 */

- (__nullable id<MTLIOScratchBuffer>) newScratchBufferWithMinimumSize:(NSUInteger)minimumSize;

@end

/*!
 @interface MTLIOCommandQueueDescriptor
 @abstract Represents a descriptor to create a MTLIOCommandQueue.
 */
MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@interface MTLIOCommandQueueDescriptor :  NSObject <NSCopying>

/*!
 @property maxCommandBufferCount
 @abstract The maximum number of commandBuffers that can be in flight at a given time for the queue.
*/
@property (nonatomic, readwrite) NSUInteger maxCommandBufferCount;

/*!
 @property priority
 @abstract The priority of the commands executed by this queue.
*/
@property (nonatomic, readwrite) MTLIOPriority priority;

/*!
 @property type
 @abstract The type (serial or concurrent) of the queue.
*/
@property (nonatomic, readwrite) MTLIOCommandQueueType type;

/*!
 @property maxCommandsInFlight
 @abstract The maximum number of IO commands that can be in flight at a given time for the queue.
 @discussion A zero value defaults to the system dependent maximum value, a smaller number can be
 provided to bound the utilization of the storage device.
*/
@property (nonatomic, readwrite) NSUInteger maxCommandsInFlight;

/*!
 @property scratchBufferAllocator
 @abstract An optional property that allows setting a custom allocator for scratch buffers by the queue.
 @discussion An application can manage scratch buffers manually by implemeting a class  conforming
 to the MTLIOScratchBufferAllocator protocol and creating an instance that is passed in here.
*/
@property (nullable, readwrite, retain) id<MTLIOScratchBufferAllocator> scratchBufferAllocator;

@end

/*!
 @protocol MTLIOFileHandle
 @abstract Represents a  file (raw or compressed) that can be used as a source
 for load commands encoded in a MTLIOCommandBuffer.
 */
MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0)) NS_SWIFT_SENDABLE
@protocol MTLIOFileHandle <NSObject>

/*!
 @property label
 @abstract An optional label for this handle.
*/
@property (nullable, copy, atomic) NSString *label;

@end

NS_ASSUME_NONNULL_END



