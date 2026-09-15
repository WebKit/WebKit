//
//  MTLDepthStencil.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>


NS_ASSUME_NONNULL_BEGIN
typedef NS_ENUM(NSUInteger, MTLCompareFunction) {
    MTLCompareFunctionNever = 0,
    MTLCompareFunctionLess = 1,
    MTLCompareFunctionEqual = 2,
    MTLCompareFunctionLessEqual = 3,
    MTLCompareFunctionGreater = 4,
    MTLCompareFunctionNotEqual = 5,
    MTLCompareFunctionGreaterEqual = 6,
    MTLCompareFunctionAlways = 7,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_ENUM(NSUInteger, MTLStencilOperation) {
    MTLStencilOperationKeep = 0,
    MTLStencilOperationZero = 1,
    MTLStencilOperationReplace = 2,
    MTLStencilOperationIncrementClamp = 3,
    MTLStencilOperationDecrementClamp = 4,
    MTLStencilOperationInvert = 5,
    MTLStencilOperationIncrementWrap = 6,
    MTLStencilOperationDecrementWrap = 7,
} API_AVAILABLE(macos(10.11), ios(8.0));

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLStencilDescriptor : NSObject <NSCopying>

@property (nonatomic) MTLCompareFunction stencilCompareFunction;

/*! Stencil is tested first.  stencilFailureOperation declares how the stencil buffer is updated when the stencil test fails. */
@property (nonatomic) MTLStencilOperation stencilFailureOperation;

/*! If stencil passes, depth is tested next.  Declare what happens when the depth test fails. */
@property (nonatomic) MTLStencilOperation depthFailureOperation;

/*! If both the stencil and depth tests pass, declare how the stencil buffer is updated. */
@property (nonatomic) MTLStencilOperation depthStencilPassOperation;

@property (nonatomic) uint32_t readMask;
@property (nonatomic) uint32_t writeMask;

@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLDepthStencilDescriptor : NSObject <NSCopying>

/* Defaults to MTLCompareFuncAlways, which effectively skips the depth test */
@property (nonatomic) MTLCompareFunction depthCompareFunction;

/* Defaults to NO, so no depth writes are performed */
@property (nonatomic, getter=isDepthWriteEnabled) BOOL depthWriteEnabled;

/* Separate stencil state for front and back state.  Both front and back can be made to track the same state by assigning the same MTLStencilDescriptor to both. */
@property (copy, nonatomic, null_resettable) MTLStencilDescriptor *frontFaceStencil;
@property (copy, nonatomic, null_resettable) MTLStencilDescriptor *backFaceStencil;

/*!
 @property label
 @abstract A string to help identify the created object.
 */
@property (nullable, copy, nonatomic) NSString *label;

@end


/* Device-specific compiled depth/stencil state object */
API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@protocol MTLDepthStencilState <NSObject>

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, readonly) NSString *label;

/*!
 @property device
 @abstract The device this resource was created against.  This resource can only be used with this device.
 */
@property (readonly) id <MTLDevice> device;

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID API_AVAILABLE(macos(26.0), ios(26.0));
@end
NS_ASSUME_NONNULL_END
