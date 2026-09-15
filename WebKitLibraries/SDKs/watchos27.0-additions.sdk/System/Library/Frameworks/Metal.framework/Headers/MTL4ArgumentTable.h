//
//  MTL4ArgumentTable.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <Metal/MTLDevice.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>

NS_ASSUME_NONNULL_BEGIN


/// Groups parameters for the creation of a Metal argument table.
///
/// Argument tables provide resource bindings to your Metal pipeline states.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4ArgumentTableDescriptor : NSObject <NSCopying>

/// Determines the number of buffer-binding slots for the argument table.
///
/// The maximum value of this parameter is 31.
@property (readwrite, nonatomic) NSUInteger maxBufferBindCount;

/// Determines the number of texture-binding slots for the argument table.
///
/// The maximum value of this parameter is 128.
@property (readwrite, nonatomic) NSUInteger maxTextureBindCount;

/// Determines the number of sampler state-binding slots for the argument table.
///
/// The maximum value of this parameter is 16.
@property (readwrite, nonatomic) NSUInteger maxSamplerStateBindCount;

/// Configures whether Metal initializes the bindings to nil values upon creation of argument table.
///
/// The default value of this property is <doc://com.apple.documentation/documentation/swift/false>.
@property (readwrite, nonatomic) BOOL initializeBindings;

/// Controls whether Metal should reserve memory for attribute strides in the argument table.
///
/// Set this value to true if you intend to provide dynamic attribute strides when binding vertex
/// array buffers to the argument table by calling ``MTL4ArgumentTable/setAddress:attributeStride:atIndex:``
///
/// The default value of this property is <doc://com.apple.documentation/documentation/swift/false>.
@property (readwrite, nonatomic) BOOL supportAttributeStrides;

/// Assigns an optional label with the argument table for debug purposes.
@property (nullable, copy, nonatomic) NSString *label;

@end

/// Provides a mechanism to manage and provide resource bindings for buffers, textures, sampler states and other Metal resources.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4ArgumentTable <NSObject>

/// Binds a GPU address to a buffer binding slot.
///
/// - Parameters:
///   - gpuAddress: The GPU address of a ``MTLBuffer`` to set.
///   - bindingIndex: a valid binding index in the buffer binding range.
///   It is an error for this value to match or exceed the value of property
///   ``MTL4ArgumentTableDescriptor/maxBufferBindCount`` on the descriptor
///   from which you created this argument table.
- (void)setAddress:(MTLGPUAddress)gpuAddress
           atIndex:(NSUInteger)bindingIndex;

/// Binds a GPU address to a buffer binding slot, providing a dynamic vertex stride.
///
/// This method requires that the value of property ``MTL4ArgumentTableDescriptor/supportAttributeStrides`` on the
/// descriptor from which you created this argument table is true.
///
/// - Parameters:
///   - gpuAddress: The GPU address of a ``MTLBuffer`` to set.
///   - stride: The stride between attributes in the buffer.
///   - bindingIndex: a valid binding index in the buffer binding range.
///   It is an error for this value to match or exceed the value of property
///   ``MTL4ArgumentTableDescriptor/maxBufferBindCount`` on the descriptor
///   from which you created this argument table.
- (void)setAddress:(MTLGPUAddress)gpuAddress
   attributeStride:(NSUInteger)stride
           atIndex:(NSUInteger)bindingIndex;

/// Binds a resource to a buffer binding slot.
///
/// - Parameters:
///   - resourceID: The ``MTLResourceID`` of the Metal resource to bind.
///   - bindingIndex: a valid binding index in the buffer binding range.
///   It is an error for this value to match or exceed the value of property
///   ``MTL4ArgumentTableDescriptor/maxBufferBindCount`` on the descriptor
///   from which you created this argument table.
- (void)setResource:(MTLResourceID)resourceID
      atBufferIndex:(NSUInteger)bindingIndex;

/// Binds a texture to a texture binding slot.
///
/// - Parameters:
///   - resourceID: The ``MTLResourceID`` of the ``MTLTexture`` instance to bind.
///   - bindingIndex: a valid binding index in the texture binding range.
///   It is an error for this value to match or exceed the value of property
///   ``MTL4ArgumentTableDescriptor/maxTextureBindCount`` on the descriptor
///   from which you created this argument table.
- (void)setTexture:(MTLResourceID)resourceID
           atIndex:(NSUInteger)bindingIndex;

/// Binds a sampler state to a sampler state binding slot.
///
/// - Parameters:
///   - resourceID: The ``MTLResourceID`` of the ``MTLSamplerState`` instance to bind.
///   - bindingIndex: a valid binding index in the sampler binding range.
///   It is an error for this value to match or exceed the value of property
///   ``MTL4ArgumentTableDescriptor/maxSamplerStateBindCount`` on the descriptor
///   from which you created this argument table.
- (void)setSamplerState:(MTLResourceID)resourceID
                atIndex:(NSUInteger)bindingIndex;

/// The device from which you created this argument table.
@property (readonly) id<MTLDevice> device;

/// Assigns an optional label with this argument table for debugging purposes.
///
/// You set this label by setting property ``MTL4ArgumentTableDescriptor/label`` on the descriptor object, prior to
/// creating this table instance.
@property (readonly, nonatomic, nullable) NSString *label;

@end


NS_ASSUME_NONNULL_END
