//
//  MTLIndirectCommandBuffer.h
//  Metal
//
//  Copyright © 2017 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLIndirectCommandEncoder.h>

NS_ASSUME_NONNULL_BEGIN


/*!
 @abstract
 A bitfield of commands that may be performed indirectly.
 */
typedef NS_OPTIONS(NSUInteger, MTLIndirectCommandType) {
    MTLIndirectCommandTypeDraw                = (1 << 0),
    MTLIndirectCommandTypeDrawIndexed         = (1 << 1),
    MTLIndirectCommandTypeDrawPatches         API_AVAILABLE(tvos(14.5)) = (1 << 2),
    MTLIndirectCommandTypeDrawIndexedPatches  API_AVAILABLE(tvos(14.5)) = (1 << 3) ,

    MTLIndirectCommandTypeConcurrentDispatch  API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0)) = (1 << 5), /* Dispatch threadgroups with concurrent execution */

    MTLIndirectCommandTypeConcurrentDispatchThreads  API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0)) = (1 << 6), /* Dispatch threads with concurrent execution */
    MTLIndirectCommandTypeDrawMeshThreadgroups API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1)) = (1 << 7),
    MTLIndirectCommandTypeDrawMeshThreads API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1)) = (1 << 8),
} API_AVAILABLE(macos(10.14), ios(12.0));


/*!
 @abstract The data layout required for specifying an indirect command buffer execution range.
 */
typedef struct
{
    uint32_t location;
    uint32_t length;
}  MTLIndirectCommandBufferExecutionRange API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(13.0));

MTL_INLINE MTLIndirectCommandBufferExecutionRange MTLIndirectCommandBufferExecutionRangeMake(uint32_t location, uint32_t length) API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(13.0))
{
    MTLIndirectCommandBufferExecutionRange icbRange = {location, length};
    return icbRange;
}

/*!
 @abstract
 Describes the limits and features that can be used in an indirect command
 */
MTL_EXPORT API_AVAILABLE(macos(10.14), ios(12.0))
@interface MTLIndirectCommandBufferDescriptor : NSObject <NSCopying>

/*!
 @abstract
 A bitfield of the command types that be encoded.
 @discussion
 MTLCommandTypeDispatch cannot be mixed with any other command type.
 */
@property (readwrite, nonatomic) MTLIndirectCommandType commandTypes;

/*!
 @abstract
 Whether the render or compute pipeline are inherited from the encoder
 */
@property (readwrite, nonatomic) BOOL inheritPipelineState API_AVAILABLE(macos(10.14), ios(13.0));

/*!
 @abstract
 Whether the render or compute pipeline can set arguments.
 */
@property (readwrite, nonatomic) BOOL inheritBuffers;

/// Configures whether the indirect command buffer inherits the depth stencil state from the encoder.
///
/// The property's default value is <doc://com.apple.documentation/documentation/swift/true>.
@property (readwrite, nonatomic) BOOL inheritDepthStencilState API_AVAILABLE(macos(26.0), ios(26.0));

/// Configures whether the indirect command buffer inherits the depth bias from the encoder.
///
/// The property's default value is <doc://com.apple.documentation/documentation/swift/true>.
@property (readwrite, nonatomic) BOOL inheritDepthBias API_AVAILABLE(macos(26.0), ios(26.0));

/// Configures whether the indirect command buffer inherits the depth clip mode from the encoder.
///
/// The property's default value is <doc://com.apple.documentation/documentation/swift/true>.
@property (readwrite, nonatomic) BOOL inheritDepthClipMode API_AVAILABLE(macos(26.0), ios(26.0));

/// Configures whether the indirect command buffer inherits the cull mode from the encoder.
///
/// The property's default value is <doc://com.apple.documentation/documentation/swift/true>.
@property (readwrite, nonatomic) BOOL inheritCullMode API_AVAILABLE(macos(26.0), ios(26.0));

/// Configures whether the indirect command buffer inherits the front facing winding from the encoder.
///
/// The property's default value is <doc://com.apple.documentation/documentation/swift/true>.
@property (readwrite, nonatomic) BOOL inheritFrontFacingWinding API_AVAILABLE(macos(26.0), ios(26.0));

/// Configures whether the indirect command buffer inherits the triangle fill mode from the encoder.
///
/// The property's default value is <doc://com.apple.documentation/documentation/swift/true>.
@property (readwrite, nonatomic) BOOL inheritTriangleFillMode API_AVAILABLE(macos(26.0), ios(26.0));


/*!
 @abstract
 The maximum bind index of vertex argument buffers that can be set per command.
 */
@property (readwrite, nonatomic) NSUInteger maxVertexBufferBindCount;

/*!
 @abstract
 The maximum bind index of fragment argument buffers that can be set per command.
 */
@property (readwrite, nonatomic) NSUInteger maxFragmentBufferBindCount;

/*!
 @abstract
 The maximum bind index of kernel (or tile) argument buffers that can be set per command.
 */
@property (readwrite, nonatomic) NSUInteger maxKernelBufferBindCount API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0));

/*!
 @abstract
 The maximum bind index of kernel (or tile) threadgroup memory that can be set per command.
 The default value is 31.
 */
@property (readwrite, nonatomic) NSUInteger maxKernelThreadgroupMemoryBindCount API_AVAILABLE(macos(14.0), ios(17.0));

/*!
 @abstract
 The maximum bind index of object stage buffers that can be set per render command.
 */
@property (readwrite, nonatomic) NSUInteger maxObjectBufferBindCount API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));
/*!
 @abstract
 The maximum bind index of mesh stage buffers that can be set per render command.
 */
@property (readwrite, nonatomic) NSUInteger maxMeshBufferBindCount API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));

/*!
 @abstract
 The maximum bind index of object threadgroup memory that can be set per render command.
 The default value is 0.
 */
@property (readwrite, nonatomic) NSUInteger maxObjectThreadgroupMemoryBindCount API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));

/*!
 @abstract
 Whether the render or compute commands can use ray tracing. Default value is NO.
 */
@property (readwrite, nonatomic) BOOL supportRayTracing API_AVAILABLE(macos(13.0), ios(16.0));

/*!
  @brief
    allows binding pipelines that have at least one MTLBufferLayout with a
    stride of `MTLBufferLayoutStrideDynamic`

    will allow setting attributeStride in `setVertexBuffer` / `setKernelBuffer`
    calls
*/
@property (readwrite, nonatomic) BOOL supportDynamicAttributeStride API_AVAILABLE(macos(14.0), ios(17.0));

/// Specifies if the indirect command buffer should support color attachment mapping.
@property (nonatomic) BOOL supportColorAttachmentMapping API_AVAILABLE(macos(26.0), ios(26.0));

@end


API_AVAILABLE(macos(10.14), ios(12.0))
@protocol MTLIndirectCommandBuffer <MTLResource>

@property (readonly) NSUInteger size;

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID API_AVAILABLE(macos(13.0), ios(16.0));

-(void)resetWithRange:(NSRange)range;

- (id <MTLIndirectRenderCommand>)indirectRenderCommandAtIndex:(NSUInteger)commandIndex;
- (id <MTLIndirectComputeCommand>)indirectComputeCommandAtIndex:(NSUInteger)commandIndex API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0));

@end

NS_ASSUME_NONNULL_END
