//
//  MTLBuffer.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLTensor.h>
#import <Metal/MTLGPUAddress.h>

NS_ASSUME_NONNULL_BEGIN

@class MTLTextureDescriptor;
@protocol MTLTexture;
@protocol MTLResource;

/*!
 @protocol MTLBuffer
 @abstract A typeless allocation accessible by both the CPU and the GPU (MTLDevice) or by only the GPU when the storage mode is
 MTLResourceStorageModePrivate.
 
 @discussion
 Unlike in OpenGL and OpenCL, access to buffers is not synchronized.  The caller may use the CPU to modify the data at any time
 but is also responsible for ensuring synchronization and coherency.
 
 The contents become undefined if both the CPU and GPU write to the same buffer without a synchronizing action between those writes.
 This is true even when the regions written do not overlap.
 */
API_AVAILABLE(macos(10.11), ios(8.0))
@protocol MTLBuffer <MTLResource>

/*!
 @property length
 @abstract The length of the buffer in bytes.
 */
@property (readonly) NSUInteger length;

/*!
 @method contents
 @abstract Returns the data pointer of this buffer's shared copy.
 */
- (void*)contents NS_RETURNS_INNER_POINTER;

/*!
 @method didModifyRange:
 @abstract Inform the device of the range of a buffer that the CPU has modified, allowing the implementation to invalidate 
 its caches of the buffer's content.
 @discussion When the application writes to a buffer's sysmem copy via @a contents, that range of the buffer immediately 
 becomes undefined for any accesses by the GPU (MTLDevice).  To restore coherency, the buffer modification must be followed
 by -didModifyRange:, and then followed by a commit of the MTLCommandBuffer that will access the buffer.
 -didModifyRange does not make the contents coherent for any previously committed command buffers.
 Note: This method is only required if buffer is created with a storage mode of MTLResourceStorageModeManaged.
 It is not valid to invoke this method on buffers of other storage modes.
 @param range The range of bytes that have been modified.
 */
- (void)didModifyRange:(NSRange)range API_DEPRECATED("Managed storage has no effect on Apple Silicon, use Shared storage instead", macos(10.11, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios);

/*!
 @method newTextureWithDescriptor:offset:bytesPerRow:
 @abstract Create a 2D texture or texture buffer that shares storage with this buffer.
 */
- (nullable id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor*)descriptor offset:(NSUInteger)offset bytesPerRow:(NSUInteger)bytesPerRow API_AVAILABLE(macos(10.13), ios(8.0));

/// Creates a single-plane tensor with the specified descriptor that shares storage with this buffer.
///
/// This method validates the constraints documented on ``MTLTensorDescriptor``,
/// and additionally requires:
/// - `offset` is 0 when ``MTLTensorDescriptor/usage`` contains
///   ``MTLTensorUsage/MTLTensorUsageMachineLearning``.
/// - `offset` is aligned to 128 bytes if the data plane uses a format
///   ``MTLTensorDataType``.
/// - `offset` is aligned to the size of the data type in bytes otherwise.
///
/// This method doesn't create tensors that contain auxiliary planes. Use
/// ``MTLDevice/newTensorWithDescriptor:attachments:error:``
/// instead to create a multi-plane tensor with per-plane buffer backing storage.
///
/// - Parameters:
///   - descriptor: The tensor descriptor configuring the data plane.
///   - offset: The byte offset into the buffer where tensor data begins.
///   - error: On failure, an NSError instance that describes the validation failure.
/// - Returns: A tensor, or `nil` if validation fails.
- (nullable id <MTLTensor>)newTensorWithDescriptor:(MTLTensorDescriptor *)descriptor
                                            offset:(NSUInteger)offset
                                             error:(__autoreleasing NSError * _Nullable * _Nullable)error API_AVAILABLE(macos(26.0), ios(26.0));

/*!
 @method addDebugMarker:range:
 @abstract Adds a marker to a specific range in the buffer.
 When inspecting a buffer in the GPU debugging tools the marker will be shown.
 @param marker A label used for the marker.
 @param range The range of bytes the marker is using.
 */
- (void)addDebugMarker:(NSString*)marker range:(NSRange)range API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @method removeAllDebugMarkers
 @abstract Removes all debug markers from a buffer.
 */
- (void)removeAllDebugMarkers API_AVAILABLE(macos(10.12), ios(10.0));



/*!
 @property gpuAddress
 @abstract Represents the GPU virtual address of a buffer resource
 */
@property (readonly) MTLGPUAddress gpuAddress API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @property sparseBufferTier
 @abstract Query support tier for sparse buffers.
 */
@property (readonly) MTLBufferSparseTier sparseBufferTier API_AVAILABLE(macos(26.0), ios(26.0));


@end
NS_ASSUME_NONNULL_END
