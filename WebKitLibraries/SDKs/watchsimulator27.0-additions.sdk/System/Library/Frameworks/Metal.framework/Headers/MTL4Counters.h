//
//  MTL4Counters.h
//  Metal
//
//  Copyright (c) 2024 Apple Inc. All rights reserved.
//

#ifndef MTL4Counters_h
#define MTL4Counters_h

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>

NS_ASSUME_NONNULL_BEGIN


/// Represents a timestamp data entry in a counter heap of type `MTL4CounterHeapTypeTimestamp`.
typedef struct
{
    uint64_t timestamp;
} MTL4TimestampHeapEntry API_AVAILABLE(macos(26.0), ios(26.0));

/// Defines the type of a ``MTL4CounterHeap`` and the contents of its entries.
typedef NS_ENUM(NSInteger, MTL4CounterHeapType)
{
    /// Specifies that ``MTL4CounterHeap`` entries contain invalid data.
    MTL4CounterHeapTypeInvalid,
    
    /// Specifies that ``MTL4CounterHeap`` entries contain GPU timestamp data.
    MTL4CounterHeapTypeTimestamp,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// Provides a hint to the system about the desired accuracy when writing GPU counter timestamps.
///
/// Pass these values to ``MTL4ComputeCommandEncoder/writeTimestampWithGranularity:intoHeap:atIndex:`` and
/// ``MTL4RenderCommandEncoder/writeTimestampWithGranularity:afterStage:intoHeap:atIndex:`` to control the
/// desired accurracy of the counter sampling operation.
typedef NS_ENUM(NSInteger, MTL4TimestampGranularity)
{
    /// A minimally-invasive timestamp which may be less precise.
    ///
    /// Using this granularity incurs in the lowest overhead, at the cost of precision. For example, it may sample at
    /// command encoder boundaries.
    MTL4TimestampGranularityRelaxed = 0,
    
    /// A timestamp as precise as possible.
    ///
    /// Using this granularity may incur in a performance penalty, for example, it may cause splitting of command encoders.
    MTL4TimestampGranularityPrecise = 1,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// Groups together parameters for configuring a counter heap object at creation time.
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4CounterHeapDescriptor : NSObject<NSCopying>

/// Assigns the type of data that the heap contains.
@property (nonatomic) MTL4CounterHeapType type;

/// Assigns the number of entries in the heap.
///
/// Each entry represents one item in the heap. The size of the individual entries depends on the heap type.
@property (nonatomic) NSUInteger count;

@end

/// Represents an opaque, driver-controlled section of memory that can store GPU counter data.
///
/// The data instances that this type stores correspond to the ``MTL4CounterHeapType`` heap type that you assign at creation time.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4CounterHeap <NSObject>

/// Assigns a label for later inspection or visualization.
@property (nullable, copy, atomic) NSString *label;

/// Queries the number of entries in the heap.
@property (readonly) NSUInteger count;

/// Queries the type of the heap.
@property (readonly) MTL4CounterHeapType type;

/// Resolves heap data on the CPU timeline.
///
/// This method resolves heap data in the CPU timeline. Your app needs to ensure the GPU work has completed in order to
/// retrieve the data correctly. You can alternatively resolve the heap data in the GPU timeline by calling
/// ``MTL4CommandBuffer/resolveCounterHeap:withRange:intoBuffer:waitFence:updateFence:``.
///
/// - Note: When resolving counters in the CPU timeline, signaling an instance of ``MTLSharedEvent`` after any workloads
///  write counters (and waiting on that signal on the CPU) is sufficient to ensure synchronization.
///
/// - Parameter range: The range in the heap to resolve.
/// - Returns a newly allocated autoreleased NSData containing tightly packed resolved heap counter values.
- (nullable NSData*) resolveCounterRange:(NSRange) range;

/// Invalidates a range of entries in this counter heap.
///
/// The effect of this call is immediate on the CPU timeline. You are responsible for ensuring that this counter heap is not currently in use on the GPU.
///
/// - Note: Invalidated entries produce 0 when resolved.
///
/// - Parameters:
///   - range: A heap index range to invalidate.
- (void) invalidateCounterRange:(NSRange) range;

@end


NS_ASSUME_NONNULL_END

#endif /* MTL4Counters_h */
