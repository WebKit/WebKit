//
//  MTLCounters.h
//  Metal
//
//  Copyright (c) 2019 Apple Inc. All rights reserved.
//

#ifndef MTLCounters_h
#define MTLCounters_h

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLResource.h>

@protocol MTLDevice;

NS_ASSUME_NONNULL_BEGIN

#define MTLCounterErrorValue ((uint64_t)~0ULL)
#define MTLCounterDontSample ((NSUInteger)-1)

/*!
 @enum MTLCommonCounter
 @abstract Common counters that, when present, are expected to have similar meanings across
 different implementations.
 @constant MTLCommonCounterTimestamp The GPU time when the sample is taken.
 @constant MTLCommonCounterTessellationInputPatches The number of patches input to the tessellator.
 @constant MTLCommonCounterVertexInvocations The number of times the vertex shader was invoked.
 @constant MTLCommonCounterPostTessellationVertexInvocations The number of times the post tessellation vertex shader was invoked.
 @constant MTLCommonCounterClipperInvocations The number of primitives passed to the clipper.
 @constant MTLCommonCounterClipperPrimitivesOut The number of primitives output from the clipper.
 @constant MTLCommonCounterFragmentInvocations The number of times the fragment shader was invoked.
 @constant MTLCommonCounterFragmentsPassed The number of fragments that passed Depth, Stencil, and Scissor tests.
 @constant MTLCommonCounterComputeKernelInvocations The number of times the computer kernel was invoked.
 @constant MTLCommonCounterTotalCycles The total number of cycles.
 @constant MTLCommonCounterVertexCycles The amount of cycles the vertex shader was running.
 @constant MTLCommonCounterTessellationCycles The amount of cycles spent in the tessellator.
 @constant MTLCommonCounterPostTessellationVertexCycles The amount of cycles the post tessellation vertex shader was running.
 @constant MTLCommonCounterFragmentCycles The amount of cycles the fragment shader was running.
 @constant MTLCommonCounterRenderTargetWriteCycles The amount of cycles spent writing to the render targets.
 */
typedef NSString * const MTLCommonCounter NS_TYPED_ENUM API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterTimestamp API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterTessellationInputPatches API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterVertexInvocations API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterPostTessellationVertexInvocations API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterClipperInvocations API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterClipperPrimitivesOut API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterFragmentInvocations API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterFragmentsPassed API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterComputeKernelInvocations API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterTotalCycles API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterVertexCycles API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterTessellationCycles API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterPostTessellationVertexCycles API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterFragmentCycles API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounter MTLCommonCounterRenderTargetWriteCycles API_AVAILABLE(macos(10.15), ios(14.0));

/*!
 @enum MTLCommonCounterSet
 @abstract Common counter set names.
 @discussion Each of these common counter sets has a defined structure type.  Implementations
 may omit some of the counters from these sets.
 @constant MTLCommonCounterSetTimestamp A counter set containing only the timestamp.
 @constant MTLCommonCounterSetStageUtilization A counter set containing utilization per stage.
 @constant MTLCommonCounterSetStatistic A counter set containing various statistics.
 */
typedef NSString * const MTLCommonCounterSet NS_TYPED_ENUM API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounterSet MTLCommonCounterSetTimestamp API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounterSet MTLCommonCounterSetStageUtilization API_AVAILABLE(macos(10.15), ios(14.0));
MTL_EXTERN MTLCommonCounterSet MTLCommonCounterSetStatistic API_AVAILABLE(macos(10.15), ios(14.0));

typedef struct
{
    uint64_t timestamp;
} MTLCounterResultTimestamp API_AVAILABLE(macos(10.15), ios(14.0));

typedef struct
{
    uint64_t totalCycles;
    uint64_t vertexCycles;
    uint64_t tessellationCycles;
    uint64_t postTessellationVertexCycles;
    uint64_t fragmentCycles;
    uint64_t renderTargetCycles;
} MTLCounterResultStageUtilization API_AVAILABLE(macos(10.15), ios(14.0));

typedef struct
{
    uint64_t tessellationInputPatches;
    uint64_t vertexInvocations;
    uint64_t postTessellationVertexInvocations;
    uint64_t clipperInvocations;
    uint64_t clipperPrimitivesOut;
    uint64_t fragmentInvocations;
    uint64_t fragmentsPassed;
    uint64_t computeKernelInvocations;
} MTLCounterResultStatistic API_AVAILABLE(macos(10.15), ios(14.0));

/*!
 @protocol MTLCounter
 @abstract A descriptor for a single counter.
 */
MTL_EXPORT API_AVAILABLE(macos(10.15), ios(14.0)) NS_SWIFT_SENDABLE
@protocol MTLCounter <NSObject>
@property (readonly, copy) NSString *name API_AVAILABLE(macos(10.15), ios(14.0));
@end

/*!
 @protocol MTLCounterSet
 @abstract A collection of MTLCounters that the device can capture in
 a single pass.
 */
MTL_EXPORT API_AVAILABLE(macos(10.15), ios(14.0)) NS_SWIFT_SENDABLE
@protocol MTLCounterSet <NSObject>
/*!
 @property name The name of the counter set.
 */
@property (readonly, copy) NSString *name API_AVAILABLE(macos(10.15), ios(14.0));

/*!
 @property counters The set of counters captured by the counter set.
 @discussion The counters array contains all the counters that will be written
 when a counter sample is collected.  Counters that do not appear in this array
 will not be written to the resolved buffer when the samples are resolved, even if
 they appear in the corresponding resolved counter structure.  Instead
 MTLCounterErrorValue will be written in the resolved buffer.
 */
@property (readonly, copy) NSArray<id<MTLCounter>>* counters API_AVAILABLE(macos(10.15), ios(14.0));

@end

/*!
 @interface MTLCounterSampleBufferDescriptor
 @abstract Object to represent the counter state.
 */
MTL_EXPORT API_AVAILABLE(macos(10.15), ios(14.0))
@interface MTLCounterSampleBufferDescriptor : NSObject <NSCopying>
/*!
 @property counterSet The counterset to be sampled for this counter sample buffer
 */
@property (nullable, readwrite, retain) id<MTLCounterSet> counterSet API_AVAILABLE(macos(10.15), ios(14.0));
/*!
 @property label A label to identify the sample buffer in debugging tools.
 */
@property (readwrite, copy) NSString *label API_AVAILABLE(macos(10.15), ios(14.0));
/*!
 @property storageMode The storage mode for the sample buffer.  Only
 MTLStorageModeShared and MTLStorageModePrivate may be used.
 */
@property (readwrite) MTLStorageMode storageMode API_AVAILABLE(macos(10.15), ios(14.0));

/*!
 @property sampleCount The number of samples that may be stored in the
 counter sample buffer.
 */
@property (readwrite) NSUInteger sampleCount API_AVAILABLE(macos(10.15), ios(14.0));
@end

/*!
 @protocol MTLCounterSampleBuffer
 @abstract The Counter Sample Buffer contains opaque counter samples as well
 as state needed to request a sample from the API.
 */
API_AVAILABLE(macos(10.15), ios(14.0))
@protocol MTLCounterSampleBuffer <NSObject>
/*!
 @property device The device that created the sample buffer.  It is only valid
 to use the sample buffer with this device.
 */
@property (readonly) id<MTLDevice> device API_AVAILABLE(macos(10.15), ios(14.0));

/*!
 @property label The label for the sample buffer.  This is set by the label
 property of the descriptor that is used to create the sample buffer.
 */
@property (readonly) NSString *label API_AVAILABLE(macos(10.15), ios(14.0));
/*!
 @property sampleCount The number of samples that may be stored in this sample buffer.
 */
@property (readonly) NSUInteger sampleCount API_AVAILABLE(macos(10.15), ios(14.0));

/*!
 @method resolveCounterRange:
 @abstract Resolve the counters from the sample buffer to an NSData containing
 the counter values.  This may only be used with sample buffers that have
 MTLStorageModeShared.
 @param range The range of indices in the sample buffer to resolve.
 @return The resolved samples.
 @discussion Samples that encountered an error during resolve will be set to
 MTLCounterErrorValue.
 */
-(nullable NSData *)resolveCounterRange:(NSRange)range
    API_AVAILABLE(macos(10.15), ios(14.0));
@end

/*!
 @constant MTLCounterErrorDomain
 @abstract NSErrors raised when creating a counter sample buffer.
 */
API_AVAILABLE(macos(10.15), ios(14.0))
MTL_EXTERN NSErrorDomain const MTLCounterErrorDomain;

/*!
 @enum MTLCounterSampleBufferError
 @constant MTLCounterSampleBufferErrorOutOfMemory
 There wasn't enough memory available to allocate the counter sample buffer.
 
 @constant MTLCounterSampleBufferErrorInvalid
 Invalid parameter passed while creating counter sample buffer.

 @constant MTLCounterSampleBufferErrorInternal
 There was some other error in allocating the counter sample buffer.
 */
typedef NS_ENUM(NSInteger, MTLCounterSampleBufferError)
{
    MTLCounterSampleBufferErrorOutOfMemory,
    MTLCounterSampleBufferErrorInvalid,
    MTLCounterSampleBufferErrorInternal,
} API_AVAILABLE(macos(10.15), ios(14.0));


NS_ASSUME_NONNULL_END

#endif /* MTLCounters_h */
