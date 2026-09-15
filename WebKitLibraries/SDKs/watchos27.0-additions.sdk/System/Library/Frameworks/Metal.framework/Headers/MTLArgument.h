//
//  MTLArgument.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTexture.h>
#import <Metal/MTLCommandEncoder.h>
#import <Metal/MTLDataType.h>
#import <Metal/MTLTensor.h>


NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, MTLIndexType) {
    MTLIndexTypeUInt16 = 0,
    MTLIndexTypeUInt32 = 1,
} API_AVAILABLE(macos(10.11), ios(8.0));

@class MTLArgument;

/*!
 @enum MTLBindingsType
 @abstract The type of a resource binding.
 
 @constant MTLBindingTypeBuffer
 This binding represents a buffer.
 
 @constant MTLBindingTypeThreadgroupMemory
 This binding represents threadgroup memory.
 
 @constant MTLBindingTypeTexture
 This binding represents a texture.
 
 @constant MTLBindingTypeSampler
 This binding represents a sampler.
 
 @constant MTLBindingTypeImageblockData
 This binding represents an image block data.
 
 @constant MTLBindingTypeImageblock
 This binding represents an image block.
  
 @constant MTLBindingTypeVisibleFunctionTable
 This binding represents a visible function table object.
 
 @constant MTLBindingTypePrimitiveAccelerationStructure
 This binding represents a primitive acceleration structure object.
 
 @constant MTLBindingTypeInstanceAccelerationStructure
 This binding represents an instance acceleration structure object.
 
 @constant MTLBinidngTypeIntersectionFunctionTable
 This binding represents an intersection function table object.
 
 @constant MTLBindingTypeObjectPayload
 This binding represents an object payload.
 
 @constant MTLBindingTypeTensor
 This binding represents a tensor object.
*/
typedef NS_ENUM(NSInteger, MTLBindingType) {
    MTLBindingTypeBuffer = 0,
    MTLBindingTypeThreadgroupMemory = 1,
    MTLBindingTypeTexture = 2,
    MTLBindingTypeSampler = 3,
    MTLBindingTypeImageblockData = 16,
    MTLBindingTypeImageblock = 17,
    MTLBindingTypeVisibleFunctionTable = 24,
    MTLBindingTypePrimitiveAccelerationStructure = 25,
    MTLBindingTypeInstanceAccelerationStructure = 26,
    MTLBindingTypeIntersectionFunctionTable = 27,
    MTLBindingTypeObjectPayload = 34,
    MTLBindingTypeTensor API_AVAILABLE(macos(26.0), ios(26.0)) = 37,
} API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @enum MTLArgumentType
 @abstract The type for an input to a MTLRenderPipelineState or a MTLComputePipelineState
 
 @constant MTLArgumentTypeBuffer
 This input is a MTLBuffer
 
 @constant MTLArgumentTypeThreadgroupMemory
 This input is a pointer to the threadgroup memory.
 
 @constant MTLArgumentTypeTexture
 This input is a MTLTexture.
 
 @constant MTLArgumentTypeSampler
 This input is a sampler.
*/
typedef NS_ENUM(NSUInteger, MTLArgumentType) {

    MTLArgumentTypeBuffer = 0,
    MTLArgumentTypeThreadgroupMemory= 1,
    MTLArgumentTypeTexture = 2,
    MTLArgumentTypeSampler = 3,

    MTLArgumentTypeImageblockData API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))     = 16,
    MTLArgumentTypeImageblock API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))         = 17,
    MTLArgumentTypeVisibleFunctionTable API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 24,
    MTLArgumentTypePrimitiveAccelerationStructure API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 25,
    MTLArgumentTypeInstanceAccelerationStructure API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 26,
    MTLArgumentTypeIntersectionFunctionTable API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 27,
} API_DEPRECATED_WITH_REPLACEMENT("MTLBindingType", macos(10.11, 13.0), ios(8.0, 16.0));


typedef NS_ENUM(NSUInteger, MTLBindingAccess) {
    MTLBindingAccessReadOnly   = 0,
    MTLBindingAccessReadWrite  = 1,
    MTLBindingAccessWriteOnly  = 2,
    MTLArgumentAccessReadOnly API_DEPRECATED_WITH_REPLACEMENT("MTLBindingAccessReadOnly", macos(10.13, 14.0), ios(8.0, 17.0)) = MTLBindingAccessReadOnly,
    MTLArgumentAccessReadWrite API_DEPRECATED_WITH_REPLACEMENT("MTLBindingAccessReadWrite", macos(10.13, 14.0), ios(8.0, 17.0)) = MTLBindingAccessReadWrite,
    MTLArgumentAccessWriteOnly API_DEPRECATED_WITH_REPLACEMENT("MTLBindingAccessWriteOnly", macos(10.13, 14.0), ios(8.0, 17.0)) = MTLBindingAccessWriteOnly,
};

typedef MTLBindingAccess MTLArgumentAccess API_DEPRECATED_WITH_REPLACEMENT("MTLBindingAccess", macos(10.11, 14.0), ios(8.0, 17.0));


@class MTLStructType;
@class MTLArrayType;
@class MTLTextureReferenceType;
@class MTLPointerType;
@class MTLTensorReferenceType;

MTL_EXPORT API_AVAILABLE(macos(10.13), ios(11.0))
@interface MTLType : NSObject
@property (readonly) MTLDataType dataType;
@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLStructMember : NSObject
 
@property (readonly) NSString *name;
@property (readonly) NSUInteger offset;
@property (readonly) MTLDataType dataType;


- (nullable MTLStructType *)structType;
- (nullable MTLArrayType *)arrayType;

- (nullable MTLTextureReferenceType *)textureReferenceType  API_AVAILABLE(macos(10.13), ios(11.0));
- (nullable MTLPointerType *)pointerType  API_AVAILABLE(macos(10.13), ios(11.0));
/// Provides a description of the underlying tensor type when this struct member holds a tensor.
///
/// - Returns: A description of the tensor type that this struct member holds, or `nil` if this struct member doesn't hold a tensor.
- (nullable MTLTensorReferenceType *)tensorReferenceType API_AVAILABLE(macos(26.0), ios(26.0));

@property (readonly) NSUInteger argumentIndex API_AVAILABLE(macos(10.13), ios(11.0));


@end
 
MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLStructType  : MTLType

@property (readonly) NSArray <MTLStructMember *> *members;

- (nullable MTLStructMember *)memberByName:(NSString *)name;

@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLArrayType  : MTLType

@property (readonly) MTLDataType elementType;

@property (readonly) NSUInteger arrayLength;
@property (readonly) NSUInteger stride;
@property (readonly) NSUInteger argumentIndexStride API_AVAILABLE(macos(10.13), ios(11.0));
- (nullable MTLStructType *)elementStructType;
- (nullable MTLArrayType *)elementArrayType;
- (nullable MTLTextureReferenceType *)elementTextureReferenceType  API_AVAILABLE(macos(10.13), ios(11.0));
- (nullable MTLPointerType *)elementPointerType  API_AVAILABLE(macos(10.13), ios(11.0));
/// Provides a description of the underlying tensor type when this array holds tensors as its elements.
///
/// - Returns: A description of the tensor type that this array holds, or `nil` if this struct member doesn't hold a tensor.
- (nullable MTLTensorReferenceType *)elementTensorReferenceType API_AVAILABLE(macos(26.0), ios(26.0));


@end

MTL_EXPORT API_AVAILABLE(macos(10.13), ios(11.0))
@interface MTLPointerType : MTLType

@property (readonly) MTLDataType elementType;           // MTLDataTypeFloat, MTLDataTypeFloat4, MTLDataTypeStruct, ...
@property (readonly) MTLBindingAccess access;
@property (readonly) NSUInteger alignment;              // min alignment for the element data
@property (readonly) NSUInteger dataSize;               // sizeof(T) for T *argName

@property (readonly) BOOL elementIsArgumentBuffer API_AVAILABLE(macos(10.13), ios(11.0));

- (nullable MTLStructType *)elementStructType  API_AVAILABLE(macos(10.13), ios(11.0));
- (nullable MTLArrayType *)elementArrayType  API_AVAILABLE(macos(10.13), ios(11.0));

@end

MTL_EXPORT API_AVAILABLE(macos(10.13), ios(11.0))
@interface MTLTextureReferenceType : MTLType

@property (readonly) MTLDataType textureDataType; // half, float, int, or uint.
@property (readonly) MTLTextureType textureType;  // texture1D, texture2D...
@property (readonly) MTLBindingAccess access;    // read, write, read-write
@property (readonly) BOOL isDepthTexture;         // true for depth textures

@end

/// An auxiliary plane that a shader's tensor argument requires.
MTL_EXPORT API_AVAILABLE(macos(27.0), ios(27.0))
@interface MTLTensorAuxiliaryPlaneType : NSObject

/// The data format of all elements in the plane.
@property (readonly) MTLTensorDataType dataType;

/// The number of data plane elements that correspond to one element in this plane.
@property (readonly) MTLTensorExtents *blockFactors;

/// The type of information this plane stores.
@property (readonly) MTLTensorPlaneType planeType;

@end

/// An object that represents a tensor in the shading language in a struct or array.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTLTensorReferenceType : MTLType

/// The underlying data format of the tensor.
@property (readonly) MTLTensorDataType tensorDataType;

/// The data format you use for indexing into the tensor.
@property (readonly) MTLDataType indexType;

/// The array of sizes, in elements, one for each dimension of this tensor.
///
/// For shader-bound tensors with dynamic extents, the ``MTLTensorExtents/rank`` of `dimensions` corresponds to the rank the shader
/// function specifies, and ``MTLTensorExtents/extentAtDimensionIndex:`` always returns a value of -1.
@property (nullable, readonly) MTLTensorExtents *dimensions;


/// The auxiliary planes that this tensor reference requires.
///
/// Returns an array of ``MTLTensorAuxiliaryPlaneType`` objects describing
/// each auxiliary plane the shader expects. Empty if the tensor has no
/// auxiliary planes.
@property (readonly) NSArray<MTLTensorAuxiliaryPlaneType *> *auxiliaryPlanes API_AVAILABLE(macos(27.0), ios(27.0));


/// A value that represents the read/write permissions of the tensor.
@property (readonly) MTLBindingAccess access;

@end

/*!
 MTLArgument
*/
MTL_EXPORT NS_SWIFT_SENDABLE
API_DEPRECATED_WITH_REPLACEMENT("MTLBinding", macos(10.11, 13.0), ios(8.0, 16.0))
@interface MTLArgument : NSObject

@property (readonly) NSString *name;
@property (readonly) MTLArgumentType type;
@property (readonly) MTLBindingAccess access;
@property (readonly) NSUInteger index;

@property (readonly, getter=isActive) BOOL active;

// for buffer arguments
@property (readonly) NSUInteger      bufferAlignment;		// min alignment of starting offset in the buffer
@property (readonly) NSUInteger      bufferDataSize; 		// sizeof(T) for T *argName
@property (readonly) MTLDataType     bufferDataType; 		// MTLDataTypeFloat, MTLDataTypeFloat4, MTLDataTypeStruct, ...
@property (readonly, nullable) MTLStructType  *bufferStructType;
@property (readonly, nullable) MTLPointerType *bufferPointerType API_AVAILABLE(macos(10.13), ios(11.0));

// for threadgroup memory arguments
@property (readonly) NSUInteger     threadgroupMemoryAlignment;
@property (readonly) NSUInteger     threadgroupMemoryDataSize; // sizeof(T) for T *argName

// for texture arguments
@property (readonly) MTLTextureType textureType; // texture1D, texture2D...
@property (readonly) MTLDataType    textureDataType; // half, float, int, or uint.
@property (readonly) BOOL           isDepthTexture API_AVAILABLE(macos(10.12), ios(10.0)); // true for depth textures
@property (readonly) NSUInteger     arrayLength API_AVAILABLE(macos(10.13), ios(10.0));

@end

MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0)) NS_SWIFT_SENDABLE
@protocol MTLBinding<NSObject>
@property (readonly) NSString *name;
@property (readonly) MTLBindingType type;
@property (readonly) MTLBindingAccess access;
@property (readonly) NSUInteger index;

@property (readonly, getter=isUsed) BOOL used;
@property (readonly, getter=isArgument) BOOL argument;
@end

MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@protocol MTLBufferBinding<MTLBinding>
@property (readonly) NSUInteger      bufferAlignment;        // min alignment of starting offset in the buffer
@property (readonly) NSUInteger      bufferDataSize;         // sizeof(T) for T *argName
@property (readonly) MTLDataType     bufferDataType;         // MTLDataTypeFloat, MTLDataTypeFloat4, MTLDataTypeStruct, ...
@property (readonly, nullable) MTLStructType  *bufferStructType;
@property (readonly, nullable) MTLPointerType *bufferPointerType;
@end

MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@protocol MTLThreadgroupBinding<MTLBinding>
@property (readonly) NSUInteger     threadgroupMemoryAlignment;
@property (readonly) NSUInteger     threadgroupMemoryDataSize; // sizeof(T) for T *argName
@end

MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@protocol MTLTextureBinding<MTLBinding>
@property (readonly) MTLTextureType textureType; // texture1D, texture2D...
@property (readonly) MTLDataType    textureDataType; // half, float, int, or uint.
@property (readonly, getter=isDepthTexture) BOOL           depthTexture; // true for depth textures
@property (readonly) NSUInteger     arrayLength;
@end

MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@protocol MTLObjectPayloadBinding<MTLBinding>
@property (readonly) NSUInteger      objectPayloadAlignment;        // min alignment of starting offset in the buffer
@property (readonly) NSUInteger      objectPayloadDataSize;         // sizeof(T) for T *argName
@end

/// An object that represents a tensor bound to a graphics or compute function or a machine learning function.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTLTensorBinding<MTLBinding>

/// The underlying data format of this tensor.
@property (readonly) MTLTensorDataType tensorDataType;

/// The data format you use for indexing into the tensor.
@property (readonly) MTLDataType indexType;

/// The array of sizes, in elements, one for each dimension of this tensor.
///
/// For shader-bound tensors with dynamic extents, the ``MTLTensorExtents/rank`` of `dimensions` corresponds to the rank the shader
/// function specifies, and ``MTLTensorExtents/extentAtDimensionIndex:`` always returns a value of -1.
///
/// For machine learning pipelines, `dimensions` corresponds to the default shape, if you provide one. Otherwise, it's `nil` in the case of an undefined shape.
@property (nullable, readonly) MTLTensorExtents *dimensions;

/// An array of the tensor's auxiliary planes.
@property (readonly) NSArray<MTLTensorAuxiliaryPlaneType *> *auxiliaryPlanes API_AVAILABLE(macos(27.0), ios(27.0));

@end

NS_ASSUME_NONNULL_END

