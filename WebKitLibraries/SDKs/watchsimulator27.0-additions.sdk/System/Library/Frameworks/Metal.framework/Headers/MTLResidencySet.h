//
//  MTLResidencySet.h
//  Framework
//
//  Copyright © 2023 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLAllocation;
@protocol MTLResource;
@protocol MTLHeap;

/*!
 @interface MTLResidencySetDescriptor
 @abstract Specifies the parameters for MTLResidencySet creation.
 */
API_AVAILABLE(macos(15.0), ios(18.0))
@interface MTLResidencySetDescriptor : NSObject<NSCopying>
/*!
 @property label
 @abstract An optional label for the MTLResidencySet.
 */
@property (nullable, copy, nonatomic) NSString *label;

/*!
 @property initialCapacity
 @abstract If non-zero, defines the number of allocations for which to initialize the internal arrays. Defaults to zero.
 */
@property (nonatomic) NSUInteger initialCapacity;


@end

/*!
 @protocol MTLResidencySet
 @abstract A residency set is responsible for managing resource and heap residency and is referenced
 by a command buffer or command queue in order to ensure that resources and heaps are resident.
 Resources and heaps are added and removed uncommitted and a subsequent commit call applies all
 of the changes in bulk.
 */
API_AVAILABLE(macos(15.0), ios(18.0))
@protocol MTLResidencySet <NSObject>
/*!
 @property device
 @abstract The device that created the residency set
 */
@property (readonly) id<MTLDevice> _Nonnull device;

/*!
 @property label
 @abstract The label specified at creation.
 */
@property (readonly, nullable) NSString* label;

/*!
 @property allocatedSize
 @abstract The memory footprint of the set in bytes at the last commit operation. This may include internal allocations as well.
 */
@property (readonly) uint64_t allocatedSize;

/*!
  @method requestResidency
  @abstract Requests that the set and all the committed resources and heaps are made resident.
 */
- (void)requestResidency;

/*!
  @method endResidency
  @abstract Requests that the set and all the committed resources and heaps are made non-resident.
*/
- (void)endResidency;

/*!
 @method addAllocation
 @abstract Adds one allocation to the set, leaving it uncommitted until commit is called.
 */
- (void)addAllocation:(id<MTLAllocation>)allocation;

/*!
 @method addAllocations
 @abstract Adds allocations to the set, leaving them uncommitted until commit is called.
 */
- (void)addAllocations:(const id<MTLAllocation> _Nonnull[_Nonnull])allocations
                 count:(NSUInteger)count;

/*!
 @method removeAllocation
 @abstract Marks an allocation to be removed from the set on the next commit call.
 */
- (void)removeAllocation:(id<MTLAllocation>)allocation;

/*!
 @method removeAllocations
 @abstract Marks allocations to be removed from the set on the next commit call.
 */
- (void)removeAllocations:(const id<MTLAllocation> _Nonnull[_Nonnull])allocations
                    count:(NSUInteger)count;

/*!
 @method removeAllAllocations
 @abstract Marks all allocations to be removed from the set on the next commit call.
 */
- (void)removeAllAllocations;

/*!
  @method containsAllocation
  @abstract Returns a boolean indicating whether the allocation is present in the set or not.
  @discussion This check includes non-committed allocations in the set.
  */
- (BOOL)containsAllocation:(id<MTLAllocation>)anAllocation;

/*!
 @property allAllocations
 @abstract Array of all allocations associated with the set.
 @discussion This property includes non-committed allocations in the set.
 */
@property (readonly, copy, nonnull) NSArray<id<MTLAllocation>> *allAllocations;

/*!
 @property allocationCount
 @abstract Returns the current number of unique allocations present in the set.
 @discussion This property includes non-committed allocations in the set.
 */
@property (readonly) NSUInteger allocationCount;

/*!
 @method commit
 @abstract Commits any pending adds/removes.
 @discussion If the residency set is resident, this will try to make added resources and heaps resident instantly, and make removed resources and heaps non-resident.
 */
- (void)commit;


@end

NS_ASSUME_NONNULL_END
