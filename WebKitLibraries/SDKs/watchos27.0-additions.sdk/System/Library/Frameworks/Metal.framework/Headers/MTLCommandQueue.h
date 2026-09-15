//
//  MTLCommandQueue.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/Metal.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;
@protocol MTLCommandBuffer;
@protocol MTLResidencySet;

/*!
 @protocol MTLCommandQueue
 @brief A serial queue of command buffers to be executed by the device.
 */
API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@protocol MTLCommandQueue <NSObject>

/*! @brief A string to help identify this object */
@property (nullable, copy, atomic) NSString *label;

/*! @brief The device this queue will submit to */
@property (readonly) id <MTLDevice> device;

/*! 
 @method commandBuffer
 @abstract Returns a new autoreleased command buffer used to encode work into this queue that 
 maintains strong references to resources used within the command buffer.
*/
- (nullable id <MTLCommandBuffer>)commandBuffer;

/*!
 @method commandBufferWithDescriptor
 @param descriptor The requested properties of the command buffer.
 @abstract Returns a new autoreleased command buffer used to encode work into this queue.
*/
- (nullable id <MTLCommandBuffer>)commandBufferWithDescriptor:(MTLCommandBufferDescriptor*)descriptor API_AVAILABLE(macos(11.0), ios(14.0));


/*!
 @method commandBufferWithUnretainedReferences
 @abstract Returns a new autoreleased command buffer used to encode work into this queue that 
 does not maintain strong references to resources used within the command buffer.
*/
- (nullable id <MTLCommandBuffer>)commandBufferWithUnretainedReferences;

/*!
 @method insertDebugCaptureBoundary
 @abstract Inform Xcode about when debug capture should start and stop.
 */
- (void)insertDebugCaptureBoundary API_DEPRECATED("Use MTLCaptureScope instead", macos(10.11, 10.13), ios(8.0, 11.0));

/*!
  @method addResidencySet
  @abstract Marks the residency set as part of the command queue execution. This ensures that the residency set is resident during execution of all the command buffers within the queue.
 */
- (void)addResidencySet:(id <MTLResidencySet>)residencySet
                   API_AVAILABLE(macos(15.0), ios(18.0));
/*!
  @method addResidencySets
  @abstract Marks the residency sets as part of the command queue execution. This ensures that the residency sets are resident during execution of all the command buffers within the queue.
 */
- (void)addResidencySets:(const id <MTLResidencySet> _Nonnull[_Nonnull])residencySets
                   count:(NSUInteger)count
                   API_AVAILABLE(macos(15.0), ios(18.0));

/*!
  @method removeResidencySet
  @abstract Removes the residency set from the command queue execution. This ensures that only the remaining residency sets are resident during execution of all the command buffers within the queue.
 */
- (void)removeResidencySet:(id <MTLResidencySet>)residencySet
                   API_AVAILABLE(macos(15.0), ios(18.0));

/*!
  @method removeResidencySets
  @abstract Removes the residency sets from the command queue execution. This ensures that only the remaining residency sets are resident during execution of all the command buffers within the queue.
 */
- (void)removeResidencySets:(const id <MTLResidencySet> _Nonnull[_Nonnull])residencySets
                      count:(NSUInteger)count
API_AVAILABLE(macos(15.0), ios(18.0));

@end

MTL_EXPORT API_AVAILABLE(macos(15.0), ios(18.0))
@interface MTLCommandQueueDescriptor : NSObject <NSCopying>
/*!
 @property maxCommandBufferCount
 @ Specify upper bound on uncompleted command buffers that may be enqueued on this queue
 */
@property (readwrite, nonatomic) NSUInteger maxCommandBufferCount;

/*!
 @property logState
 @ Specify the MTLLogState to enable shader logging
 */
@property (readwrite, nonatomic, nullable, retain) id<MTLLogState> logState;
@end
NS_ASSUME_NONNULL_END
