//
//  MTL4CommandAllocator.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4CommandAllocator_h
#define MTL4CommandAllocator_h

#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>


NS_ASSUME_NONNULL_BEGIN

/// Groups together parameters for creating a command allocator.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4CommandAllocatorDescriptor : NSObject <NSCopying>

/// An optional label you can assign to the command allocator to aid debugging.
@property (nullable, copy, nonatomic) NSString *label;

@end

/// Manages the memory backing the encoding of GPU commands into command buffers.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4CommandAllocator <NSObject>

/// Returns the GPU device that this command allocator belongs to.
@property (readonly) id<MTLDevice> device;

/// Provides the optional label you specify at creation time for debug purposes.
@property (nullable, readonly) NSString *label;

/// Queries the size of the internal memory heaps of this command allocator that support encoding
/// commands into command buffers.
///
/// - Returns: a size in bytes.
- (uint64_t)allocatedSize;

/// Marks the command allocator's heaps for reuse.
///
/// Calling this method allows new ``MTL4CommandBuffer`` to reuse its existing internal
/// memory heaps to encode new GPU commands.
///
/// You are responsible to ensure that all command buffers with memory originating
/// from this allocator instance are complete before calling resetting it.
- (void)reset;

@end

NS_ASSUME_NONNULL_END


#endif //MTL4CommandAllocator_h
