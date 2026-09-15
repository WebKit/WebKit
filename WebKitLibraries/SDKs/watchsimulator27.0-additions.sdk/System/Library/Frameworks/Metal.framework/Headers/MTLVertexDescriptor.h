//
//  MTLVertexDescriptor.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>


NS_ASSUME_NONNULL_BEGIN

/*!
 @enum MTLVertexFormat
 @abstract specifies how the vertex attribute data is laid out in memory.
*/
typedef NS_ENUM(NSUInteger, MTLVertexFormat)
{
    MTLVertexFormatInvalid = 0,
    
    MTLVertexFormatUChar2 = 1,
    MTLVertexFormatUChar3 = 2,
    MTLVertexFormatUChar4 = 3,
    
    MTLVertexFormatChar2 = 4,
    MTLVertexFormatChar3 = 5,
    MTLVertexFormatChar4 = 6,
    
    MTLVertexFormatUChar2Normalized = 7,
    MTLVertexFormatUChar3Normalized = 8,
    MTLVertexFormatUChar4Normalized = 9,
    
    MTLVertexFormatChar2Normalized = 10,
    MTLVertexFormatChar3Normalized = 11,
    MTLVertexFormatChar4Normalized = 12,
    
    MTLVertexFormatUShort2 = 13,
    MTLVertexFormatUShort3 = 14,
    MTLVertexFormatUShort4 = 15,
    
    MTLVertexFormatShort2 = 16,
    MTLVertexFormatShort3 = 17,
    MTLVertexFormatShort4 = 18,
    
    MTLVertexFormatUShort2Normalized = 19,
    MTLVertexFormatUShort3Normalized = 20,
    MTLVertexFormatUShort4Normalized = 21,
    
    MTLVertexFormatShort2Normalized = 22,
    MTLVertexFormatShort3Normalized = 23,
    MTLVertexFormatShort4Normalized = 24,
    
    MTLVertexFormatHalf2 = 25,
    MTLVertexFormatHalf3 = 26,
    MTLVertexFormatHalf4 = 27,
    
    MTLVertexFormatFloat = 28,
    MTLVertexFormatFloat2 = 29,
    MTLVertexFormatFloat3 = 30,
    MTLVertexFormatFloat4 = 31,
    
    MTLVertexFormatInt = 32,
    MTLVertexFormatInt2 = 33,
    MTLVertexFormatInt3 = 34,
    MTLVertexFormatInt4 = 35,
    
    MTLVertexFormatUInt = 36,
    MTLVertexFormatUInt2 = 37,
    MTLVertexFormatUInt3 = 38,
    MTLVertexFormatUInt4 = 39,
    
    MTLVertexFormatInt1010102Normalized = 40,
    MTLVertexFormatUInt1010102Normalized = 41,
    
    MTLVertexFormatUChar4Normalized_BGRA API_AVAILABLE(macos(10.13), ios(11.0)) = 42,
    
    MTLVertexFormatUChar API_AVAILABLE(macos(10.13), ios(11.0)) = 45,
    MTLVertexFormatChar API_AVAILABLE(macos(10.13), ios(11.0)) = 46,
    MTLVertexFormatUCharNormalized API_AVAILABLE(macos(10.13), ios(11.0)) = 47,
    MTLVertexFormatCharNormalized API_AVAILABLE(macos(10.13), ios(11.0)) = 48,
    
    MTLVertexFormatUShort API_AVAILABLE(macos(10.13), ios(11.0)) = 49,
    MTLVertexFormatShort API_AVAILABLE(macos(10.13), ios(11.0)) = 50,
    MTLVertexFormatUShortNormalized API_AVAILABLE(macos(10.13), ios(11.0)) = 51,
    MTLVertexFormatShortNormalized API_AVAILABLE(macos(10.13), ios(11.0)) = 52,
    
    MTLVertexFormatHalf API_AVAILABLE(macos(10.13), ios(11.0)) = 53,

    MTLVertexFormatFloatRG11B10 API_AVAILABLE(macos(14.0), ios(17.0)) = 54,
    MTLVertexFormatFloatRGB9E5 API_AVAILABLE(macos(14.0), ios(17.0)) = 55,
    
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_ENUM(NSUInteger, MTLVertexStepFunction)
{
    MTLVertexStepFunctionConstant = 0,
    MTLVertexStepFunctionPerVertex = 1,
    MTLVertexStepFunctionPerInstance = 2,
    MTLVertexStepFunctionPerPatch API_AVAILABLE(macos(10.12), ios(10.0)) = 3,
    MTLVertexStepFunctionPerPatchControlPoint API_AVAILABLE(macos(10.12), ios(10.0)) = 4,
} API_AVAILABLE(macos(10.11), ios(8.0));

/*!
  @brief
    when a MTLVertexBufferLayoutDescriptor has its stride set to this value,
    the stride will be dynamic and must be set explicitly when binding a buffer
    to a render command encoder.
*/
API_AVAILABLE(macos(14.0), ios(17.0))
static const NSUInteger MTLBufferLayoutStrideDynamic = NSUIntegerMax;

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLVertexBufferLayoutDescriptor : NSObject <NSCopying>
@property (assign, nonatomic) NSUInteger stride;
@property (assign, nonatomic) MTLVertexStepFunction stepFunction;
@property (assign, nonatomic) NSUInteger stepRate;
@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLVertexBufferLayoutDescriptorArray : NSObject
- (MTLVertexBufferLayoutDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLVertexBufferLayoutDescriptor *)bufferDesc atIndexedSubscript:(NSUInteger)index;
@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLVertexAttributeDescriptor : NSObject <NSCopying>
@property (assign, nonatomic) MTLVertexFormat format;
@property (assign, nonatomic) NSUInteger offset;
@property (assign, nonatomic) NSUInteger bufferIndex;
@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLVertexAttributeDescriptorArray : NSObject
- (MTLVertexAttributeDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLVertexAttributeDescriptor *)attributeDesc atIndexedSubscript:(NSUInteger)index;
@end

/*
 MTLVertexDescriptor
 */
MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLVertexDescriptor : NSObject <NSCopying>

+ (MTLVertexDescriptor *)vertexDescriptor;

@property (readonly) MTLVertexBufferLayoutDescriptorArray *layouts;
@property (readonly) MTLVertexAttributeDescriptorArray *attributes;

- (void)reset;

@end
NS_ASSUME_NONNULL_END
