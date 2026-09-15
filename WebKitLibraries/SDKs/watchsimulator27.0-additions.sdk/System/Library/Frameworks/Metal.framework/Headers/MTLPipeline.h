//
//  MTLPipeline.h
//  Metal
//
//  Copyright (c) 2017 Apple Inc. All rights reserved.
//

#import <Metal/MTLDevice.h>


NS_ASSUME_NONNULL_BEGIN

/*!
 * @enum MTLMutability
 * @abstract Specifies whether a buffer will be modified between the time it is bound and a compute
 * or render pipeline is executed in a draw or dispatch.
 */
typedef NS_ENUM(NSUInteger, MTLMutability)
{
    MTLMutabilityDefault   = 0,
    MTLMutabilityMutable   = 1,
    MTLMutabilityImmutable = 2,
} API_AVAILABLE(macos(10.13), ios(11.0));


typedef NS_ENUM(NSInteger, MTLShaderValidation)
{
    MTLShaderValidationDefault = 0,
    MTLShaderValidationEnabled = 1,
    MTLShaderValidationDisabled = 2,
} API_AVAILABLE(macos(15.0), ios(18.0));


MTL_EXPORT API_AVAILABLE(macos(10.13), ios(11.0))
@interface MTLPipelineBufferDescriptor : NSObject <NSCopying>

/*! Buffer mutability. Defaults to MTLMutabilityDefault: mutable for standard buffers, immutable for argument buffers */
@property (nonatomic) MTLMutability mutability;

@end

MTL_EXPORT API_AVAILABLE(macos(10.13), ios(11.0))
@interface MTLPipelineBufferDescriptorArray : NSObject
/* Individual buffer descriptor access */
- (MTLPipelineBufferDescriptor *)objectAtIndexedSubscript:(NSUInteger)bufferIndex;

/* This always uses 'copy' semantics. It is safe to set the buffer descriptor at any legal index to nil, which resets that buffer descriptor to default values. */
- (void)setObject:(nullable MTLPipelineBufferDescriptor *)buffer atIndexedSubscript:(NSUInteger)bufferIndex;

@end

NS_ASSUME_NONNULL_END
