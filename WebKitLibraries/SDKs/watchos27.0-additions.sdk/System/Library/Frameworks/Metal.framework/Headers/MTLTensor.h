//
//  MTLTensor.h
//  Metal
//
//  Copyright © 2024 Apple Inc. All rights reserved.
//

#ifndef MTLTensor_h
#define MTLTensor_h

#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLDataType.h>


/// The possible data types for the elements of a tensor.
typedef NS_ENUM(NSInteger, MTLTensorDataType)
{
    /// An invalid data type.
    MTLTensorDataTypeNone     = MTLDataTypeNone,
    
    /// A single-precision floating point data type.
    MTLTensorDataTypeFloat32  = MTLDataTypeFloat,
    
    /// A half-precision floating point data type.
    MTLTensorDataTypeFloat16  = MTLDataTypeHalf,
    
    /// A 16-bit floating point data type with 8 exponent bits, 7 mantissa bits, and 1 sign bit.
    MTLTensorDataTypeBFloat16 = MTLDataTypeBFloat,
    
    /// An 8-bit signed integer data type.
    MTLTensorDataTypeInt8     = MTLDataTypeChar,
    
    /// An 8-bit unsigned integer data type.
    MTLTensorDataTypeUInt8    = MTLDataTypeUChar,
    
    /// A 16-bit signed integer data type.
    MTLTensorDataTypeInt16    = MTLDataTypeShort,
    
    /// A 16-bit unsigned integer data type.
    MTLTensorDataTypeUInt16   = MTLDataTypeUShort,
    
    /// A 32-bit signed integer data type.
    MTLTensorDataTypeInt32    = MTLDataTypeInt,
    
    /// A 32-bit unsigned integer data type.
    MTLTensorDataTypeUInt32   = MTLDataTypeUInt,
    
    /// A 4-bit signed integer data type.
    MTLTensorDataTypeInt4 API_AVAILABLE(macos(26.4), ios(26.4))  = 143,
    
    /// A 4-bit unsigned integer data type.
    MTLTensorDataTypeUInt4 API_AVAILABLE(macos(26.4), ios(26.4)) = 144,
    
    /// An 8-bit floating point data type with 8 exponent bits, 0 mantissa bits, and no sign bit.
    MTLTensorDataTypeMetalFloat8UE8M0 API_AVAILABLE(macos(27.0), ios(27.0)) = 145,
    
    /// A 2-bit unsigned integer data type.
    MTLTensorDataTypeUInt2 API_AVAILABLE(macos(27.0), ios(27.0)) = 149,
    
    /// A 2-bit signed integer data type.
    MTLTensorDataTypeInt2 API_AVAILABLE(macos(27.0), ios(27.0)) = 150,
    
    /// An 8-bit floating point data type with 5 exponent bits, 2 mantissa bits, and 1 sign bit.
    MTLTensorDataTypeMetalFloat8E5M2 API_AVAILABLE(macos(27.0), ios(27.0)) = 141,
    
    /// An 8-bit floating point data type with 4 exponent bits, 3 mantissa bits, and 1 sign bit.
    MTLTensorDataTypeMetalFloat8E4M3 API_AVAILABLE(macos(27.0), ios(27.0)) = 142,
    
    /// A 4-bit floating point data type with 2 exponent bits, 1 mantissa bit, and 1 sign bit.
    MTLTensorDataTypeMetalFloat4E2M1 API_AVAILABLE(macos(27.0), ios(27.0)) = 148,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// The largest rank a tensor can have.
#define MTL_TENSOR_MAX_RANK 16

/// An integer array that holds per-dimension values such as tensor sizes, strides, or block factors
///
/// Supports a rank between `0` and ``MTL_TENSOR_MAX_RANK``.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTLTensorExtents : NSObject <NSCopying>

/// Creates an extents object with the rank and values you provide.
///
/// Zero rank extents represent scalars. `values` can only be `nil` if `rank` is 0.
/// - Parameters:
///   - rank: The number of values in the extents. Pass `0` to create a scalar (rank-zero) extents.
///   - values: A C array of `rank` integer values, or `nil` when `rank` is `0`. The first element
///     corresponds to the innermost dimension.
/// - Returns: A new extents instance, or `nil` if `rank` exceeds 0 and `values` is `nil`,
///   or if `rank` exceeds ``MTL_TENSOR_MAX_RANK``.
- (nullable instancetype)initWithRank:(NSUInteger)rank values:(nullable const NSInteger *)values;


/// The number of values in the extents object.
@property (readonly) NSUInteger rank;

/// Returns the value at the specified dimension index.
///
/// - Parameters:
///   - dimensionIndex: The index of the value to retrieve. The first index corresponds
///     to the innermost dimension.
/// - Returns: The value at `dimensionIndex`, or `-1` if `dimensionIndex` is greater than or
///   equal to ``rank``.
- (NSInteger)extentAtDimensionIndex:(NSUInteger)dimensionIndex;

@end


NS_ASSUME_NONNULL_BEGIN

@protocol MTLBuffer;

/// An error domain for errors that pertain to creating a tensor.
MTL_EXTERN NSErrorDomain const MTLTensorDomain API_AVAILABLE(macos(26.0), ios(26.0));

/// The error codes that Metal can raise when you create a tensor.
typedef NS_ENUM(NSInteger, MTLTensorError)
{
    /// No error occurred.
    MTLTensorErrorNone              = 0,
    
    /// An internal Metal error occurred.
    MTLTensorErrorInternalError     = 1,
    
    /// The tensor descriptor is invalid.
    MTLTensorErrorInvalidDescriptor = 2,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// The contexts in which you can use a tensor.
typedef NS_OPTIONS(NSUInteger, MTLTensorUsage)
{
    /// A tensor context that applies to compute encoders.
    ///
    /// You can use tensors with this context in ``MTL4ComputeCommandEncoder`` or ``MTLComputeCommandEncoder`` instances.
    MTLTensorUsageCompute = 1 << 0,
    /// A tensor context that applies to render encoders.
    ///
    /// You can use tensors with this context in ``MTL4RenderCommandEncoder`` or ``MTLRenderCommandEncoder`` instances.
    MTLTensorUsageRender  = 1 << 1,
    /// A tensor context that applies to machine learning encoders.
    ///
    /// You can use tensors with this context in ``MTL4MachineLearningCommandEncoder`` instances.
    MTLTensorUsageMachineLearning      = 1 << 2,
} API_AVAILABLE(macos(26.0), ios(26.0));


/// The possible tensor plane types.
typedef NS_ENUM(NSInteger, MTLTensorPlaneType)
{
    /// The main data plane, which every tensor has
    MTLTensorPlaneTypeData = 0,
    
    /// The auxiliary plane that stores scale factors for elements in the data plane.
    MTLTensorPlaneTypeScales = 1,
} API_AVAILABLE(macos(27.0), ios(27.0));

/// A configuration for an auxiliary plane in a multi-plane tensor.
///
/// Use this descriptor to configure an auxiliary plane's data type and block
/// factors before attaching it to a ``MTLTensorDescriptor``.
MTL_EXPORT API_AVAILABLE(macos(27.0), ios(27.0))
@interface MTLTensorAuxiliaryPlaneDescriptor : NSObject <NSCopying>

/// The data format of all elements in the plane.
///
/// The default value of this property is ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8UE8M0``.
@property (readwrite, nonatomic) MTLTensorDataType dataType;

/// An extents instance that represents the number of data plane elements which correspond to one element in
/// a plane you create with this descriptor.
///
/// The number of dimensions in the extents needs to match the number of the tensor's
/// dimensions.
///
/// The first element of the block factors needs to be `32`. All remaining elements
/// need to be `1`.
///
/// The default value is a 1D block size of width `32`.
@property (readwrite, nonatomic, copy) MTLTensorExtents *blockFactors;

@end

/// A map of auxiliary plane descriptors keyed by plane type.
///
/// Use this collection to associate ``MTLTensorPlaneType`` values with
/// ``MTLTensorAuxiliaryPlaneDescriptor`` configurations, then attach
/// it to a ``MTLTensorDescriptor`` to create a multi-plane tensor.
MTL_EXPORT API_AVAILABLE(macos(27.0), ios(27.0))
@interface MTLTensorAuxiliaryPlaneDescriptorMap : NSObject <NSCopying>

/// Sets the auxiliary plane descriptor for the given plane type.
///
/// ``MTLTensorPlaneType/MTLTensorPlaneTypeData`` is not a valid plane type for this method.
/// The data plane is always present, and you configure it directly on ``MTLTensorDescriptor``.
///
/// ``MTLTensorPlaneType/MTLTensorPlaneTypeScales`` auxiliary planes only support
/// ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8UE8M0`` as a data type.
///
/// - Parameters:
///   - descriptor: The descriptor configuring the auxiliary plane.
///   - plane: The plane type to associate the descriptor with.
- (void)setDescriptor:(MTLTensorAuxiliaryPlaneDescriptor *)descriptor
             forPlane:(MTLTensorPlaneType)plane;

/// Returns the auxiliary plane descriptor for the given plane type, or `nil` if
/// none has been set.
///
/// - Parameters:
///   - plane: The plane type to look up.
/// - Returns: The descriptor for the given plane type, or `nil`.
- (nullable MTLTensorAuxiliaryPlaneDescriptor *)descriptorForPlane:(MTLTensorPlaneType)plane;

/// Empties the map of all its elements.
-(void)reset;

@end

/// A configuration type for creating new tensor instances.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTLTensorDescriptor : NSObject <NSCopying>

/// An array of sizes, in elements, one for each dimension of the tensors you create with this descriptor.
///
/// Every element of the array needs to be greater than `0`.
///
/// When ``dataType`` is ``MTLTensorDataType/MTLTensorDataTypeInt2``, ``MTLTensorDataType/MTLTensorDataTypeUInt2``,
/// ``MTLTensorDataType/MTLTensorDataTypeInt4``, ``MTLTensorDataType/MTLTensorDataTypeUInt4``,
/// ``MTLTensorDataType/MTLTensorDataTypeMetalFloat4E2M1``, ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8E4M3``,
/// ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8E5M2``, or ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8UE8M0``:
/// - The dimension value of the array's first element needs to be a multiple of 32 elements.
/// - The extents needs to have at least one dimension.
///
/// If the tensor has auxiliary planes, each dimension needs to be evenly divisible by its corresponding block factor.
///
/// The default value of this property is a rank one extents with size one.
@property (readwrite, nonatomic, copy) MTLTensorExtents *dimensions;

/// An array of strides, in elements, one for each dimension of this tensor, if applicable.
///
/// The stride value of the array's first element needs to be exactly `1`, because it is the innermost dimension.
/// The strides for the subsequent dimensions can have different requirements based on the value of other properties.
/// When the ``usage`` property includes the ``MTLTensorUsage/MTLTensorUsageMachineLearning`` option:
/// - The second element of the array needs to be a multiple of 64 bytes.
/// - The rest of the elements in the array need to equal the product of the previous stride multiplied with the size of
///   the previous dimension. For example: `strides[i] = strides[i - 1] * dimensions[i - 1]`.
///
/// When ``dataType`` is ``MTLTensorDataType/MTLTensorDataTypeInt2``, ``MTLTensorDataType/MTLTensorDataTypeUInt2``,
/// ``MTLTensorDataType/MTLTensorDataTypeInt4``, ``MTLTensorDataType/MTLTensorDataTypeUInt4``,
/// ``MTLTensorDataType/MTLTensorDataTypeMetalFloat4E2M1``, ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8E4M3``,
/// ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8E5M2``, or ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8UE8M0``,
/// all elements of the array, except for the first element, need to be a multiple of 128 bytes.
///
/// > Tip: You can improve runtime performance by using strides that are multiples of 128, even when it's not a requirement.
///
/// Only set this property when creating tensors from a buffer.
@property (readwrite, nonatomic, copy, nullable) MTLTensorExtents *strides;

/// The data format of all elements in the data plane.
///
/// The default value of this property is ``MTLTensorDataType/MTLTensorDataTypeFloat32``.
///
/// ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8UE8M0`` is not a valid data type for this property.
@property (readwrite, nonatomic) MTLTensorDataType dataType;

/// A set of contexts in which you can use tensors you create with this descriptor.
///
/// The default value for this property is a bitwise `OR` of:
/// - ``MTLTensorUsage/MTLTensorUsageRender``
/// - ``MTLTensorUsage/MTLTensorUsageCompute``
@property (readwrite, nonatomic) MTLTensorUsage usage;


/// The auxiliary plane configurations for this tensor.
///
/// Set this property with a populated ``MTLTensorAuxiliaryPlaneDescriptorMap``
/// to create a multi-plane tensor. When `nil`, the tensor has only a data plane.
///
/// Multi-plane tensors do not support ``MTLTensorUsage/MTLTensorUsageMachineLearning``.
/// Use ``MTLTensorUsage/MTLTensorUsageCompute`` or ``MTLTensorUsage/MTLTensorUsageRender``.
///
/// Multi-plane tensors do not support data types larger than one byte as the data plane type.
///
/// Multi-plane tensors do not support rank zero.
///
/// The default value is `nil`.
@property (nullable, retain, nonatomic) MTLTensorAuxiliaryPlaneDescriptorMap *auxiliaryPlanes API_AVAILABLE(macos(27.0), ios(27.0));


/// A packed set of the ``storageMode``, ``cpuCacheMode``, and ``hazardTrackingMode`` properties.
@property (readwrite, nonatomic) MTLResourceOptions resourceOptions;

/// A value that configures the cache mode of CPU mapping of tensors you create with this descriptor.
///
/// The default value of this property is ``MTLCPUCacheMode/MTLCPUCacheModeDefaultCache``.
@property (readwrite, nonatomic) MTLCPUCacheMode cpuCacheMode;

/// A value that configures the memory location and access permissions of tensors you create with this descriptor.
///
/// The default value of this property is ``MTLStorageMode/MTLStorageModeShared``.
@property (readwrite, nonatomic) MTLStorageMode storageMode;

/// A value that configures the hazard tracking of tensors you create with this descriptor.
///
/// The default value of this property is ``MTLHazardTrackingMode/MTLHazardTrackingModeDefault``.
@property (readwrite, nonatomic) MTLHazardTrackingMode hazardTrackingMode;

@end


/// An object that associates each plane of a tensor with a buffer and byte offset for buffer-backed tensor creation.
MTL_EXPORT API_AVAILABLE(macos(27.0), ios(27.0))
@interface MTLTensorBufferAttachments : NSObject <NSCopying>

/// Sets the buffer and byte offset to use as backing storage for the given
/// plane.
///
/// The offset needs to be aligned to 128 bytes if the plane uses ``MTLTensorDataType/MTLTensorDataTypeInt2``,
/// ``MTLTensorDataType/MTLTensorDataTypeUInt2``, ``MTLTensorDataType/MTLTensorDataTypeInt4``,
/// ``MTLTensorDataType/MTLTensorDataTypeUInt4``, ``MTLTensorDataType/MTLTensorDataTypeMetalFloat4E2M1``,
/// ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8E4M3``, ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8E5M2``,
/// or ``MTLTensorDataType/MTLTensorDataTypeMetalFloat8UE8M0``, otherwise it needs to be aligned
/// to the size of the plane's data type in bytes.
///
/// - Parameters:
///   - buffer: The buffer to back the plane.
///   - offset: The byte offset into the buffer.
///   - plane: The plane type to associate the buffer with.
-(void)setBuffer:(id<MTLBuffer>)buffer
          offset:(NSUInteger)offset
        forPlane:(MTLTensorPlaneType)plane;

/// Returns the buffer backing the given plane, or `nil` if none has been set.
///
/// - Parameters:
///   - plane: The plane type to look up.
/// - Returns: The buffer for the given plane, or `nil`.
-(nullable id<MTLBuffer>)bufferForPlane:(MTLTensorPlaneType)plane;

/// Returns the byte offset into the buffer for the given plane.
///
/// - Parameters:
///   - plane: The plane type to look up.
/// - Returns: The byte offset for the given plane.
-(NSUInteger)offsetForPlane:(MTLTensorPlaneType)plane;

/// Empties the container of all its elements.
-(void)reset;

@end

/// A type that represents the configuration and storage of an auxiliary plane in a multi-plane tensor.
MTL_EXPORT API_AVAILABLE(macos(27.0), ios(27.0))
@protocol MTLTensorAuxiliaryPlane <NSObject>

/// The data format of all elements in the plane.
@property (readonly) MTLTensorDataType dataType;

/// The number of data plane elements that correspond to one element in this auxiliary plane.
@property (readonly) MTLTensorExtents *blockFactors;

/// The buffer that provides the underlying storage for this plane, or `nil` if no buffer was provided at initialization.
@property (readonly, nullable) id<MTLBuffer> buffer;

/// The byte offset into ``buffer`` where this plane's data begins, or `0` if no buffer was provided at initialization.
@property (readonly) NSUInteger bufferOffset;

/// The type of information this plane stores.
@property (readonly) MTLTensorPlaneType planeType;

@end

/// A resource representing a multi-dimensional array that you can use with machine learning workloads.
MTL_EXTERN API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTLTensor <MTLResource>

/// A handle that represents the GPU resource, which you can store in an argument buffer.
@property (readonly) MTLResourceID gpuResourceID;

/// A buffer instance this tensor shares its storage with or `nil` if this tensor does not wrap an underlying buffer.
@property (nullable, readonly) id<MTLBuffer> buffer;

/// An offset, in bytes, into the buffer instance this tensor shares its storage with, or zero if this tensor does not wrap an underlying buffer.
@property (readonly) NSUInteger bufferOffset;

/// An array of strides, in elements, one for each dimension of this tensor, if applicable.
///
/// This property is non-nil only for tensors created from a buffer.
@property (nullable, readonly) MTLTensorExtents *strides;

/// An array of sizes, in elements, one for each dimension of this tensor.
@property (readonly) MTLTensorExtents *dimensions;

/// The underlying data format of the data plane.
@property (readonly) MTLTensorDataType dataType;

/// A set of contexts in which you can use this tensor.
@property (readonly) MTLTensorUsage usage;


/// The auxiliary planes of this tensor.
///
/// Returns an array of ``MTLTensorAuxiliaryPlane`` objects describing each
/// auxiliary plane configured on this tensor. For single-plane tensors, this
/// array is empty.
@property (readonly) NSArray<id<MTLTensorAuxiliaryPlane>> *auxiliaryPlanes API_AVAILABLE(macos(27.0), ios(27.0));



/// Replaces a slice of the data plane of this tensor with data from a pointer you provide.
///
/// Create the tensor with ``MTLResourceOptions/MTLResourceStorageModeShared``
/// for CPU access via this method.
///
/// Strides need to be monotonically non-decreasing: for any `i > 0`,
/// `strides[i] >= strides[i-1] * dimensions[i-1]`.
///
/// The first dimension of `sliceOrigin` and `sliceDimensions` needs to be byte aligned.
///
/// - Parameters:
///   - sliceOrigin: An array of per-dimension offsets that together locate the first element
///     to write to in the tensor. Each element in this array corresponds to the dimension at the
///     same index in `sliceDimensions`. Each offset value represents the number of elements from
///     the start of that dimension.
///   - sliceDimensions: An array of per-dimension sizes that together define the extent of the
///     slice to write to in the tensor. Each element in this array corresponds to the dimension
///     at the same index in `sliceOrigin`. Each size value represents the number of elements to
///     include along that dimension, starting from the corresponding offset in `sliceOrigin`.
///   - bytes: A pointer to bytes of data to copy into the slice.
///   - strides: An array of strides, in elements, that describes the layout
///     of the data in `bytes`.
- (void)replaceSliceOrigin:(MTLTensorExtents *)sliceOrigin
           sliceDimensions:(MTLTensorExtents *)sliceDimensions
                 withBytes:(const void *)bytes
                   strides:(MTLTensorExtents *)strides;

/// Copies data from a slice of the data plane of this tensor into a pointer you provide.
///
/// Create the tensor with ``MTLResourceOptions/MTLResourceStorageModeShared``
/// for CPU access via this method.
///
/// Strides need to be monotonically non-decreasing: for any `i > 0`,
/// `strides[i] >= strides[i-1] * dimensions[i-1]`.
///
/// The first dimension of `sliceOrigin` and `sliceDimensions` needs to be byte aligned.
///
/// - Parameters:
///   - bytes: A pointer to bytes of data that this method copies the slice
///     into.
///   - strides: An array of strides, in elements, that describes the layout
///     of the data in `bytes`.
///   - sliceOrigin: An array of per-dimension offsets that together locate the first element
///     to copy in the tensor. Each element in this array corresponds to the dimension at the
///     same index in `sliceDimensions`. Each offset value represents the number of elements from
///     the start of that dimension.
///   - sliceDimensions: An array of per-dimension sizes that together define the extent of the
///     slice to copy from the tensor. Each element in this array corresponds to the dimension
///     at the same index in `sliceOrigin`. Each size value represents the number of elements to
///     include along that dimension, starting from the corresponding offset in `sliceOrigin`.
- (void)getBytes:(void *)bytes
         strides:(MTLTensorExtents *)strides
 fromSliceOrigin:(MTLTensorExtents *)sliceOrigin
 sliceDimensions:(MTLTensorExtents *)sliceDimensions;




/// Copies data from a slice of a plane of this tensor into a pointer you provide.
///
/// When reading from auxiliary planes, specify `sliceOrigin` and `sliceDimensions`
/// in plane coordinates by applying the auxiliary plane's block factors.
///
/// Create the tensor with ``MTLResourceOptions/MTLResourceStorageModeShared``
/// for CPU access via this method.
///
/// Strides need to be monotonically non-decreasing: for any `i > 0`,
/// `strides[i] >= strides[i-1] * dimensions[i-1]`.
///
/// The first dimension of `sliceOrigin` and `sliceDimensions` needs to be
/// byte aligned.
///
/// - Parameters:
///   - bytes: A pointer to bytes of data that this method copies the slice
///     into.
///   - strides: An array of strides, in elements, that describes the layout
///     of the data in `bytes`.
///   - sliceOrigin: An array of per-dimension offsets that together locate the first element
///     to copy in the tensor. Each element in this array corresponds to the dimension at the
///     same index in `sliceDimensions`. Each offset value represents the number of elements from
///     the start of that dimension.
///   - sliceDimensions: An array of per-dimension sizes that together define the extent of the
///     slice to copy from the tensor. Each element in this array corresponds to the dimension
///     at the same index in `sliceOrigin`. Each size value represents the number of elements to
///     include along that dimension, starting from the corresponding offset in `sliceOrigin`.
///   - plane: The plane the method reads data from.
- (void)getBytes:(void *)bytes
         strides:(MTLTensorExtents *)strides
 fromSliceOrigin:(MTLTensorExtents *)sliceOrigin
 sliceDimensions:(MTLTensorExtents *)sliceDimensions
           plane:(MTLTensorPlaneType)plane API_AVAILABLE(macos(27.0), ios(27.0));

/// Replaces a slice of a plane of this tensor with data from a pointer you provide.
///
/// When writing to auxiliary planes, specify `sliceOrigin` and `sliceDimensions`
/// in plane coordinates by applying the auxiliary plane's block factors.
///
/// Create the tensor with ``MTLResourceOptions/MTLResourceStorageModeShared``
/// for CPU access via this method.
///
/// Strides need to be monotonically non-decreasing: for any `i > 0`,
/// `strides[i] >= strides[i-1] * dimensions[i-1]`.
///
/// The first dimension of `sliceOrigin` and `sliceDimensions` needs to be
/// byte aligned.
///
/// - Parameters:
///   - sliceOrigin: An array of per-dimension offsets that together locate the first element
///     to write to in the tensor. Each element in this array corresponds to the dimension at the
///     same index in `sliceDimensions`. Each offset value represents the number of elements from
///     the start of that dimension.
///   - sliceDimensions: An array of per-dimension sizes that together define the extent of the
///     slice to write to in the tensor. Each element in this array corresponds to the dimension
///     at the same index in `sliceOrigin`. Each size value represents the number of elements to
///     include along that dimension, starting from the corresponding offset in `sliceOrigin`.
///   - plane: The plane the method writes data to.
///   - bytes: A pointer to bytes of data to copy into the slice.
///   - strides: An array of strides, in elements, that describes the layout
///     of the data in `bytes`.
- (void)replaceSliceOrigin:(MTLTensorExtents *)sliceOrigin
           sliceDimensions:(MTLTensorExtents *)sliceDimensions
                     plane:(MTLTensorPlaneType)plane
                 withBytes:(const void *)bytes
                   strides:(MTLTensorExtents *)strides API_AVAILABLE(macos(27.0), ios(27.0));


@end

NS_ASSUME_NONNULL_END



#endif // MTLTensor_h
