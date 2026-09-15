//
//  MTL4ComputeCommandEncoder.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4ComputeCommandEncoder_h
#define MTL4ComputeCommandEncoder_h

#import <Metal/MTLDefines.h>
#import <Metal/MTLBlitCommandEncoder.h>

#import <Metal/MTL4CommandEncoder.h>
#import <Metal/MTL4AccelerationStructure.h>
#import <Metal/MTL4Counters.h>


NS_ASSUME_NONNULL_BEGIN

@protocol MTL4ArgumentTable;
@protocol MTLComputePipelineState;
@protocol MTL4CounterHeap;

/// Encodes a compute pass and other memory operations into a command buffer.
///
/// Use instances of this abstraction to encode a compute pass into ``MTL4CommandBuffer`` instances, as well as commands
/// that copy and modify the underlying memory of various Metal resources, and commands that build or refit acceleration
/// structures.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4ComputeCommandEncoder <MTL4CommandEncoder>

/// Queries a bitmask representing the shader stages on which commands currently present in this command encoder
/// operate.
///
/// Metal dynamically updates this property based on the commands you encode into the command encoder, for example,
/// it sets the bit ``MTLStages/MTLStageDispatch`` if this encoder contains any commands that dispatch a compute kernel.
///
/// Similarly, it sets the bit ``MTLStages/MTLStageBlit`` if this encoder contains any commands to copy or modify buffers,
/// textures, or indirect command buffers.
///
/// Finally, Metal sets the bit ``MTLStages/MTLStageAccelerationStructure`` if this encoder contains any commands that
/// build, copy, or refit acceleration structures.
///
///- Returns: a bitmask representing shader stages that commands currently present in this command encoder operate on.
- (MTLStages)stages;

/// Configures this encoder with a compute pipeline state that applies to your subsequent dispatch commands.
///
/// - Parameter state: a non-`nil` ``MTLComputePipelineState``.
- (void)setComputePipelineState:(id<MTLComputePipelineState>)state;

/// Configures the size of a threadgroup memory buffer for a threadgroup argument in the compute shader function.
///
/// - Parameters:
///   - length: The size of the threadgroup memory, in bytes. Use a multiple of `16` bytes.
///   - index: An integer that corresponds to the index of the argument you annotate with attribute `[[threadgroup(index)]]`
///   in the shader function.
- (void)setThreadgroupMemoryLength:(NSUInteger)length
                           atIndex:(NSUInteger)index;

/// Specifies the size, in pixels, of imageblock data in tile memory.
///
/// - Parameters:
///   - width:  The width of the imageblock, in pixels.
///   - height: The height of the imageblock, in pixels.
- (void)setImageblockWidth:(NSUInteger)width
                    height:(NSUInteger)height;

/// Encodes a compute dispatch command using an arbitrarily-sized grid.
///
/// - Parameters:
///   - threadsPerGrid:        An ``MTLSize`` instance that represents the number of threads in the grid,
///                            in each dimension.
///   - threadsPerThreadgroup: An ``MTLSize`` instance that represents the number of threads in one
///                            threadgroup, in each dimension.
- (void)dispatchThreads:(MTLSize)threadsPerGrid
  threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;

/// Encodes a compute dispatch command with a grid that aligns to threadgroup boundaries.
///
/// - Parameters:
///   - threadgroupsPerGrid:   An ``MTLSize`` instance that represents the number of threadgroups in the grid,
///                            in each dimension.
///   - threadsPerThreadgroup: An ``MTLSize`` instance that represents the number of threads in one
///                            threadgroup, in each dimension.
- (void)dispatchThreadgroups:(MTLSize)threadgroupsPerGrid
       threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;

/// Encodes a compute dispatch command with a grid that aligns to threadgroup boundaries, using an indirect buffer
/// for arguments.
///
/// This method allows you to supply the threadgroups-per-grid counts indirectly via an ``MTLBuffer`` index. This
/// enables you to calculate this value in the GPU timeline from a shader function, enabling GPU-driven workflows.
///
/// Metal assumes that the buffer contents correspond to the layout of struct ``MTLDispatchThreadgroupsIndirectArguments``.
/// You are responsible for ensuring this address aligns to 4-bytes.
///
/// Use an instance of ``MTLResidencySet`` to mark residency of the indirect buffer that the `indirectBuffer`
/// parameter references.
///
/// - Parameters:
///   - indirectBuffer:        GPUAddress of a ``MTLBuffer`` instance providing compute parameters.
///                            Lay out the data in this buffer as described in the
///                            ``MTLDispatchThreadgroupsIndirectArguments`` structure. This address
///                            requires 4-byte alignment.
///   - threadsPerThreadgroup: A ``MTLSize`` instance that represents the number of threads in one
///                            threadgroup, in each dimension.
- (void)dispatchThreadgroupsWithIndirectBuffer:(MTLGPUAddress)indirectBuffer
                         threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup;

/// Encodes a compute dispatch command with an arbitrarily sized grid, using an indirect buffer for arguments.
///
/// - Parameters:
///   - indirectBuffer: GPUAddress of a ``MTLBuffer`` instance providing arguments. Lay out the data
///                     in this buffer as described in the ``MTLDispatchThreadsIndirectArguments``
///                     structure. This address requires 4-byte alignment.
- (void)dispatchThreadsWithIndirectBuffer:(MTLGPUAddress)indirectBuffer;

/// Encodes a command to execute a series of commands from an indirect command buffer.
///
/// Use this method to encode the execution of a range of Metal compute commands in the GPU timeline.
///
/// - Note: if the `indirectCommandBuffer` parameter references any pipeline state objects, you are responsible
///     for adding them to a ``MTLResidencySet`` instance in use when you commit the command buffer.
///
///     An indirect compute command references a pipeline state when you pass it as an argument to the
///     command's ``MTLIndirectComputeCommand/setComputePipelineState:`` method during CPU encoding, or
///     `set_compute_pipeline_state()` during GPU encoding.
///
/// - Parameters:
///   - indirectCommandBuffer: ``MTLIndirectCommandBuffer`` instance containing the commands to execute.
///   - executionRange:        The range of commands to execute.
- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer
                      withRange:(NSRange)executionRange;

/// Encodes an instruction to execute commands from an indirect command buffer, using an indirect buffer for
/// arguments.
///
/// Use this method to indicate to Metal the span of indices in the command buffer to execute indirectly via an
/// ``MTLBuffer`` instance you provide in the `indirectRangeBuffer` parameter. This allows you to calculate the
/// span of commands Metal executes in the GPU timeline, enabling GPU-driven workflows.
///
/// Metal requires that the contents of this buffer match the layout of struct ``MTLIndirectCommandBufferExecutionRange``,
/// which specifies a location and a length within the indirect command buffer. You are responsible for ensuring the
/// address of this buffer has 4-byte alignment.
///
/// Use an instance of ``MTLResidencySet`` to mark residency of the indirect buffer that the `indirectRangeBuffer`
/// parameter references.
///
/// - Note: if the `indirectCommandBuffer` parameter references any pipeline state objects, you are responsible
///     for adding them to a ``MTLResidencySet`` instance in use when you commit the command buffer.
///
///     An indirect compute command references a pipeline state when you pass it as an argument to the
///     command's ``MTLIndirectComputeCommand/setComputePipelineState:`` method during CPU encoding, or
///     `set_compute_pipeline_state()` during GPU encoding.
///
/// - Parameters:
///   - indirectCommandbuffer: ``MTLIndirectCommandBuffer`` instance containing the commands to execute.
///   - indirectRangeBuffer:   GPUAddress of a ``MTLBuffer`` containing the execution range. Lay out the data
///                            in this buffer as described in the ``MTLIndirectCommandBufferExecutionRange``
///                            structure. This address requires 4-byte alignment.
- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandbuffer
                 indirectBuffer:(MTLGPUAddress)indirectRangeBuffer;

/// Encodes a command that copies data from a texture to another.
///
/// - Parameters:
///   - sourceTexture:      An ``MTLTexture`` instance the command copies data from.
///   - destinationTexture: Another ``MTLTexture`` instance the command copies the data into that has the same
///                         ``MTLTexture/pixelFormat`` and ``MTLTexture/sampleCount`` as `sourceTexture`.
- (void)copyFromTexture:(id<MTLTexture>)sourceTexture
              toTexture:(id<MTLTexture>)destinationTexture;

/// Encodes a command that copies slices of a texture to slices of another texture.
///
/// - Parameters:
///   - sourceTexture:      A ``MTLTexture`` texture that the command copies data from. To read the source
///                         texture contents, you need to set its ``MTLTexture/framebufferOnly`` property
///                         to <doc://com.apple.documentation/documentation/swift/false> prior to drawing into it.
///   - sourceSlice:        A slice within `sourceTexture` the command uses as a starting point to copy
///                         data from. Set this to `0` if `sourceTexture` isn’t a texture array or a
///                         cube texture.
///   - sourceLevel:        A mipmap level within `sourceTexture`.
///   - destinationTexture: Another ``MTLTexture`` the command copies the data to that has the same
///                         ``MTLTexture/pixelFormat`` and ``MTLTexture/sampleCount`` as `sourceTexture`.
///                         To write the contents into this texture, you need to set its ``MTLTexture/framebufferOnly``
///                         property to <doc://com.apple.documentation/documentation/swift/false>.
///   - destinationSlice:   A slice within `destinationTexture` the command uses as its starting point
///                         for copying data to. Set this to `0` if `destinationTexture` isn’t a texture
///                         array or a cube texture.
///   - destinationLevel:   A mipmap level within `destinationTexture`. The mipmap level you reference needs to
///                         have the same size as the `sourceTexture` slice's mipmap at `sourceLevel`.
///   - sliceCount:         The number of slices the command copies so that it satisfies the conditions
///                         that the sum of `sourceSlice` and `sliceCount` doesn’t exceed the number of
///                         slices in `sourceTexture` and the sum of `destinationSlice` and `sliceCount`
///                         doesn’t exceed the number of slices in `destinationTexture`.
///   - levelCount:         The number of mipmap levels the command copies so that it satisfies the
///                         conditions that the sum of `sourceLevel` and `levelCount` doesn’t exceed the
///                         number of mipmap levels in `sourceTexture` and the sum of `destinationLevel`
///                         and `levelCount` doesn’t exceed the number of mipmap levels in `destinationTexture`.
- (void)copyFromTexture:(id<MTLTexture>)sourceTexture
            sourceSlice:(NSUInteger)sourceSlice
            sourceLevel:(NSUInteger)sourceLevel
              toTexture:(id<MTLTexture>)destinationTexture
       destinationSlice:(NSUInteger)destinationSlice
       destinationLevel:(NSUInteger)destinationLevel
             sliceCount:(NSUInteger)sliceCount
             levelCount:(NSUInteger)levelCount;

/// Encodes a command that copies image data from a slice of a texture into a slice of another texture.
///
/// - Parameters:
///   - sourceTexture:      An ``MTLTexture`` texture that the command copies data from. To read the source
///                         texture contents, you need to set its ``MTLTexture/framebufferOnly`` property
///                         to <doc://com.apple.documentation/documentation/swift/false> prior to drawing into it.
///   - sourceSlice:        A slice within `sourceTexture` the command uses as a starting point to copy
///                         data from. Set this to `0` if `sourceTexture` isn’t a texture array or a
///                         cube texture.
///   - sourceLevel:        A mipmap level within `sourceTexture`.
///   - sourceOrigin:       An ``MTLOrigin`` instance that represents a location within `sourceTexture`
///                         that the command begins copying data from. Assign `0` to each dimension
///                         that’s not relevant to `sourceTexture`.
///   - sourceSize:         An ``MTLSize`` instance that represents the size of the region, in pixels,
///                         that the command copies from `sourceTexture`, starting at `sourceOrigin`.
///                         Assign `1` to each dimension that’s not relevant to `sourceTexture`. If
///                         sourceTexture uses a compressed pixel format, set `sourceSize` to a multiple
///                         of the pixel format’s block size. If the block extends outside the bounds of
///                         the texture, clamp `sourceSize` to the edge of the texture.
///   - destinationTexture: Another ``MTLTexture`` the command copies the data to that has the same
///                         ``MTLTexture/pixelFormat`` and ``MTLTexture/sampleCount`` as `sourceTexture`.
///                         To write the contents into this texture, you need to set its ``MTLTexture/framebufferOnly``
///                         property to <doc://com.apple.documentation/documentation/swift/false>.
///   - destinationSlice:   A slice within `destinationTexture` the command uses as its starting point
///                         for copying data to. Set this to `0` if `destinationTexture` isn’t a texture
///                         array or a cube texture.
///   - destinationLevel:   A mipmap level within `destinationTexture`. The mipmap level you reference needs to
///                         have the same size as the `sourceTexture` slice's mipmap at `sourceLevel`.
///   - destinationOrigin:  An ``MTLOrigin`` instance that represents a location within `destinationTexture`
///                         that the command begins copying data to. Assign `0` to each dimension that’s
///                         not relevant to `destinationTexture`.
- (void)copyFromTexture:(id<MTLTexture>)sourceTexture
            sourceSlice:(NSUInteger)sourceSlice
            sourceLevel:(NSUInteger)sourceLevel
           sourceOrigin:(MTLOrigin)sourceOrigin
             sourceSize:(MTLSize)sourceSize
              toTexture:(id<MTLTexture>)destinationTexture
       destinationSlice:(NSUInteger)destinationSlice
       destinationLevel:(NSUInteger)destinationLevel
      destinationOrigin:(MTLOrigin)destinationOrigin;

/// Encodes a command that copies image data from a slice of an ``MTLTexture`` instance to an ``MTLBuffer`` instance.
///
/// - Parameters:
///   - sourceTexture:            An ``MTLTexture`` texture that the command copies data from. To read the source
///                               texture contents, you need to set its ``MTLTexture/framebufferOnly`` property
///                               to <doc://com.apple.documentation/documentation/swift/false> prior to drawing into it.
///   - sourceSlice:              A slice within `sourceTexture` the command uses as a starting point to copy
///                               data from. Set this to `0` if `sourceTexture` isn’t a texture array or a
///                               cube texture.
///   - sourceLevel:              A mipmap level within `sourceTexture`.
///   - sourceOrigin:             An ``MTLOrigin`` instance that represents a location within `sourceTexture`
///                               that the command begins copying data from. Assign `0` to each dimension
///                               that’s not relevant to `sourceTexture`.
///   - sourceSize:               An ``MTLSize`` instance that represents the size of the region, in pixels,
///                               that the command copies from `sourceTexture`, starting at `sourceOrigin`.
///                               Assign `1` to each dimension that’s not relevant to `sourceTexture`.
///                               If `sourceTexture` uses a compressed pixel format, set `sourceSize` to a
///                               multiple of the `sourceTexture's` ``MTLTexture/pixelFormat`` block size.
///                               If the block extends outside the bounds of the texture, clamp `sourceSize`
///                               to the edge of the texture.
///   - destinationBuffer:        An ``MTLBuffer`` instance the command copies data to.
///   - destinationOffset:        A byte offset within `destinationBuffer` the command copies to. The value
///                               you provide as this argument needs to be a multiple of `sourceTexture's` pixel size,
///                               in bytes.
///   - destinationBytesPerRow:   The number of bytes between adjacent rows of pixels in `destinationBuffer`.
///                               This value must be a multiple of `sourceTexture's` pixel size, in bytes,
///                               and less than or equal to the product of `sourceTexture's` pixel size,
///                               in bytes, and the largest pixel width `sourceTexture’s` type allows. If
///                               `sourceTexture` uses a compressed pixel format, set `destinationBytesPerRow`
///                               to the number of bytes between the starts of two row blocks.
///   - destinationBytesPerImage: The number of bytes between each 2D image of a 3D texture. This value must
///                               be a multiple of `sourceTexture's` pixel size, in bytes. Set this value to
///                               `0` if `sourceSize's` ``MTLSize/depth`` value is `1`.
- (void) copyFromTexture:(id<MTLTexture>)sourceTexture
             sourceSlice:(NSUInteger)sourceSlice
             sourceLevel:(NSUInteger)sourceLevel
            sourceOrigin:(MTLOrigin)sourceOrigin
              sourceSize:(MTLSize)sourceSize
                toBuffer:(id<MTLBuffer>)destinationBuffer
       destinationOffset:(NSUInteger)destinationOffset
  destinationBytesPerRow:(NSUInteger)destinationBytesPerRow
destinationBytesPerImage:(NSUInteger)destinationBytesPerImage;

/// Encodes a command that copies image data from a slice of a texture instance to a buffer, with
/// options for special texture formats.
///
/// - Parameters:
///   - sourceTexture:            An ``MTLTexture`` texture that the command copies data from. To read the source
///                               texture contents, you need to set its ``MTLTexture/framebufferOnly`` property
///                               to <doc://com.apple.documentation/documentation/swift/false> prior to drawing into it.
///   - sourceSlice:              A slice within `sourceTexture` the command uses as a starting point to copy
///                               data from. Set this to `0` if `sourceTexture` isn’t a texture array or a
///                               cube texture.
///   - sourceLevel:              A mipmap level within `sourceTexture`.
///   - sourceOrigin:             An ``MTLOrigin`` instance that represents a location within `sourceTexture`
///                               that the command begins copying data from. Assign `0` to each dimension
///                               that’s not relevant to `sourceTexture`.
///   - sourceSize:               An ``MTLSize`` instance that represents the size of the region, in pixels,
///                               that the command copies from `sourceTexture`, starting at `sourceOrigin`.
///                               Assign `1` to each dimension that’s not relevant to `sourceTexture`.
///                               If `sourceTexture` uses a compressed pixel format, set `sourceSize` to a
///                               multiple of the `sourceTexture's` ``MTLTexture/pixelFormat`` block size.
///                               If the block extends outside the bounds of the texture, clamp `sourceSize`
///                               to the edge of the texture.
///   - destinationBuffer:        An ``MTLBuffer`` instance the command copies data to.
///   - destinationOffset:        A byte offset within `destinationBuffer` the command copies to. The value
///                               you provide as this argument needs to be a multiple of `sourceTexture's` pixel size,
///                               in bytes.
///   - destinationBytesPerRow:   The number of bytes between adjacent rows of pixels in `destinationBuffer`.
///                               This value must be a multiple of `sourceTexture's` pixel size, in bytes,
///                               and less than or equal to the product of `sourceTexture's` pixel size,
///                               in bytes, and the largest pixel width `sourceTexture’s` type allows. If
///                               `sourceTexture` uses a compressed pixel format, set `destinationBytesPerRow`
///                               to the number of bytes between the starts of two row blocks.
///   - destinationBytesPerImage: The number of bytes between each 2D image of a 3D texture. This value must
///                               be a multiple of `sourceTexture's` pixel size, in bytes. Set this value to
///                               `0` if `sourceSize's` ``MTLSize/depth`` value is `1`.
///   - options:                  A ``MTLBlitOption`` value that applies to textures with applicable pixel
///                               formats, such as combined depth/stencil or PVRTC formats. If `sourceTexture's`
///                               ``MTLTexture/pixelFormat`` is a combined depth/stencil format, set `options`
///                               to either ``MTLBlitOption/MTLBlitOptionDepthFromDepthStencil`` or
///                               ``MTLBlitOption/MTLBlitOptionStencilFromDepthStencil``, but not both.
///                               If `sourceTexture's` ``MTLTexture/pixelFormat`` is a PVRTC format, set
///                               `options` to ``MTLBlitOption/MTLBlitOptionRowLinearPVRTC``.
- (void) copyFromTexture:(id<MTLTexture>)sourceTexture
             sourceSlice:(NSUInteger)sourceSlice
             sourceLevel:(NSUInteger)sourceLevel
            sourceOrigin:(MTLOrigin)sourceOrigin
              sourceSize:(MTLSize)sourceSize
                toBuffer:(id<MTLBuffer>)destinationBuffer
       destinationOffset:(NSUInteger)destinationOffset
  destinationBytesPerRow:(NSUInteger)destinationBytesPerRow
destinationBytesPerImage:(NSUInteger)destinationBytesPerImage
                 options:(MTLBlitOption)options;

/// Encodes a command that copies data from a buffer instance into another.
///
/// - Parameters:
///   - sourceBuffer:      An ``MTLBuffer`` instance the command copies data from.
///   - sourceOffset:      A byte offset within `sourceBuffer` the command copies from.
///   - destinationBuffer: An ``MTLBuffer`` instance the command copies data to.
///   - destinationOffset: A byte offset within `destinationBuffer` the command copies to.
///   - size:              The number of bytes the command copies from `sourceBuffer` to `destinationBuffer`.
- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer
          sourceOffset:(NSUInteger)sourceOffset
              toBuffer:(id<MTLBuffer>)destinationBuffer
     destinationOffset:(NSUInteger)destinationOffset
                  size:(NSUInteger)size;

/// Encodes a command to copy image data from a buffer instance into a texture.
///
/// - Parameters:
///   - sourceBuffer:        A ``MTLBuffer`` instance the command copies data from.
///   - sourceOffset:        A byte offset within `sourceBuffer` the command copies from. Set this value to
///                          a multiple of `destinationTexture's` pixel size, in bytes.
///   - sourceBytesPerRow:   The number of bytes between adjacent rows of pixels in `sourceBuffer`. Set this value to
///                          a multiple of `destinationTexture's` pixel size, in bytes, and less than or equal to
///                          the product of `destinationTexture's` pixel size, in bytes, and the largest pixel width
///                          `destinationTexture's` type allows. If `destinationTexture` uses a compressed pixel format,
///                          set `sourceBytesPerRow` to the number of bytes between the starts of two row blocks.
///   - sourceBytesPerImage: The number of bytes between each 2D image of a 3D texture. Set this value to a
///                          multiple of `destinationTexture's` pixel size, in bytes, or `0`
///                          if `sourceSize's` ``MTLSize/depth`` value is `1`.
///   - sourceSize:          A ``MTLSize`` instance that represents the size of the region in
///                          `destinationTexture`, in pixels, that the command copies data to, starting at
///                          `destinationOrigin`. Assign `1` to each dimension that’s not relevant to
///                          `destinationTexture`. If `destinationTexture` uses a compressed pixel format,
///                          set `sourceSize` to a multiple of `destinationTexture's` ``MTLTexture/pixelFormat``
///                          block size. If the block extends outside the bounds of the texture, clamp
///                          `sourceSize` to the edge of the texture.
///   - destinationTexture:  An ``MTLTexture`` instance the command copies data to. In order to copy the contents into
///                          the destination texture, set its ``MTLTexture/framebufferOnly`` property to
///                          <doc://com.apple.documentation/documentation/swift/false> and don't
///                          use a combined depth/stencil ``MTLTexture/pixelFormat``.
///   - destinationSlice:    A slice within `destinationTexture` the command uses as its starting point for
///                          copying data to. Set this to `0` if `destinationTexture` isn’t a texture array
///                          or a cube texture.
///   - destinationLevel:    A mipmap level within `destinationTexture` the command copies data to.
///   - destinationOrigin:   An ``MTLOrigin`` instance that represents a location within `destinationTexture`
///                          that the command begins copying data to. Assign `0` to each dimension that’s not
///                          relevant to `destinationTexture`.
- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer
          sourceOffset:(NSUInteger)sourceOffset
     sourceBytesPerRow:(NSUInteger)sourceBytesPerRow
   sourceBytesPerImage:(NSUInteger)sourceBytesPerImage
            sourceSize:(MTLSize)sourceSize
             toTexture:(id<MTLTexture>)destinationTexture
      destinationSlice:(NSUInteger)destinationSlice
      destinationLevel:(NSUInteger)destinationLevel
     destinationOrigin:(MTLOrigin)destinationOrigin;

/// Encodes a command to copy image data from a buffer into a texture with options for special texture formats.
///
/// - Parameters:
///   - sourceBuffer:        An ``MTLBuffer`` instance the command copies data from.
///   - sourceOffset:        A byte offset within `sourceBuffer` the command copies from. Set this value to
///                          a multiple of `destinationTexture's` pixel size, in bytes.
///   - sourceBytesPerRow:   The number of bytes between adjacent rows of pixels in `sourceBuffer`. Set this value to
///                          a multiple of `destinationTexture's` pixel size, in bytes, and less than or equal to
///                          the product of `destinationTexture's` pixel size, in bytes, and the largest pixel width
///                          `destinationTexture's` type allows. If `destinationTexture` uses a compressed pixel format,
///                          set `sourceBytesPerRow` to the number of bytes between the starts of two row blocks.
///   - sourceBytesPerImage: The number of bytes between each 2D image of a 3D texture. Set this value to a
///                          multiple of `destinationTexture's` pixel size, in bytes, or `0`
///                          if `sourceSize's` ``MTLSize/depth`` value is `1`.
///   - sourceSize:          An ``MTLSize`` instance that represents the size of the region in
///                          `destinationTexture`, in pixels, that the command copies data to, starting at
///                          `destinationOrigin`. Assign `1` to each dimension that’s not relevant to
///                          `destinationTexture`. If `destinationTexture` uses a compressed pixel format,
///                          set `sourceSize` to a multiple of `destinationTexture's` ``MTLTexture/pixelFormat``
///                          block size. If the block extends outside the bounds of the texture, clamp
///                          `sourceSize` to the edge of the texture.
///   - destinationTexture:  An ``MTLTexture`` instance the command copies data to. In order to copy the contents into
///                          the destination texture, set its ``MTLTexture/framebufferOnly`` property to
///                          <doc://com.apple.documentation/documentation/swift/false> and don't
///                          use a combined depth/stencil ``MTLTexture/pixelFormat``.
///   - destinationSlice:    A slice within `destinationTexture` the command uses as its starting point for
///                          copying data to. Set this to `0` if `destinationTexture` isn’t a texture array
///                          or a cube texture.
///   - destinationLevel:    A mipmap level within `destinationTexture` the command copies data to.
///   - destinationOrigin:   An ``MTLOrigin`` instance that represents a location within `destinationTexture`
///                          that the command begins copying data to. Assign `0` to each dimension that’s not
///                          relevant to `destinationTexture`.
///   - options:             An ``MTLBlitOption`` value that applies to textures with applicable pixel formats,
///                          such as combined depth/stencil or PVRTC formats. If `destinationTexture's`
///                          ``MTLTexture/pixelFormat`` is a combined depth/stencil format, set `options` to
///                          either ``MTLBlitOption/MTLBlitOptionDepthFromDepthStencil`` or
///                          ``MTLBlitOption/MTLBlitOptionStencilFromDepthStencil``, but not both.
///                          If `destinationTexture's` ``MTLTexture/pixelFormat`` is a PVRTC format, set
///                          `options` to ``MTLBlitOption/MTLBlitOptionRowLinearPVRTC``.
- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer
          sourceOffset:(NSUInteger)sourceOffset
     sourceBytesPerRow:(NSUInteger)sourceBytesPerRow
   sourceBytesPerImage:(NSUInteger)sourceBytesPerImage
            sourceSize:(MTLSize)sourceSize
             toTexture:(id<MTLTexture>)destinationTexture
      destinationSlice:(NSUInteger)destinationSlice
      destinationLevel:(NSUInteger)destinationLevel
     destinationOrigin:(MTLOrigin)destinationOrigin
               options:(MTLBlitOption)options;



/// Encodes a command to copy data from a slice of the data plane of a tensor into a slice of the data plane of
/// another tensor.
///
/// If `sourceTensor` and `destinationTensor` are not aliasable, this command applies a reshape operation.
///
/// Ensure the first dimension of `sourceOrigin`, `sourceDimensions`, `destinationOrigin`,
/// and `destinationDimensions` is byte aligned.
///
/// - Parameters:
///   - sourceTensor: A tensor instance the method copies data from.
///   - sourceOrigin: An array of per-dimension offsets that together locate the first element
///     to copy in `sourceTensor`. Each element in this array corresponds to the dimension at the
///     same index in `sourceDimensions`. Each offset value represents the number of elements from
///     the start of that dimension.
///   - sourceDimensions: An array of per-dimension sizes that together define the extent of the
///     slice to copy from `sourceTensor`. Each element in this array corresponds to the dimension
///     at the same index in `sourceOrigin`. Each size value represents the number of elements to
///     include along that dimension, starting from the corresponding offset in `sourceOrigin`.
///   - destinationTensor: A tensor instance the method copies data to.
///   - destinationOrigin: An array of per-dimension offsets that together locate the first element
///     to write in `destinationTensor`. Each element in this array corresponds to the dimension at
///     the same index in `destinationDimensions`. Each offset value represents the number of elements
///     from the start of that dimension.
///   - destinationDimensions: An array of per-dimension sizes that together define the extent of
///     the slice to write in `destinationTensor`. Each element in this array corresponds to the
///     dimension at the same index in `destinationOrigin`. Each size value represents the number of
///     elements to include along that dimension, starting from the corresponding offset in
///     `destinationOrigin`.
- (void)copyFromTensor:(id<MTLTensor>)sourceTensor
          sourceOrigin:(MTLTensorExtents *)sourceOrigin
      sourceDimensions:(MTLTensorExtents *)sourceDimensions
              toTensor:(id<MTLTensor>)destinationTensor
     destinationOrigin:(MTLTensorExtents *)destinationOrigin
 destinationDimensions:(MTLTensorExtents *)destinationDimensions;




/// Encodes a command to copy data from a slice of a plane of a tensor into a slice of a plane of
/// another tensor.
///
/// If `sourceTensor` and `destinationTensor` are not aliasable, this command applies a reshape operation.
/// For auxiliary planes, specify origin and dimensions in plane coordinates by applying the corresponding auxiliary plane's block
/// factors.
///
/// Ensure the first dimension of `sourceOrigin`, `sourceDimensions`, `destinationOrigin`,
/// and `destinationDimensions` is byte aligned.
///
/// - Parameters:
///   - sourceTensor: A tensor instance the method copies data from.
///   - sourceOrigin: An array of per-dimension offsets that together locate the first element
///     to copy in `sourceTensor`. Each element in this array corresponds to the dimension at the
///     same index in `sourceDimensions`. Each offset value represents the number of elements from
///     the start of that dimension.
///   - sourceDimensions: An array of per-dimension sizes that together define the extent of the
///     slice to copy from `sourceTensor`. Each element in this array corresponds to the dimension
///     at the same index in `sourceOrigin`. Each size value represents the number of elements to
///     include along that dimension, starting from the corresponding offset in `sourceOrigin`.
///   - sourcePlane: The plane the method copies data from.
///   - destinationTensor: A tensor instance the method copies data to.
///   - destinationOrigin: An array of per-dimension offsets that together locate the first element
///     to write in `destinationTensor`. Each element in this array corresponds to the dimension at
///     the same index in `destinationDimensions`. Each offset value represents the number of elements
///     from the start of that dimension.
///   - destinationDimensions: An array of per-dimension sizes that together define the extent of
///     the slice to write in `destinationTensor`. Each element in this array corresponds to the
///     dimension at the same index in `destinationOrigin`. Each size value represents the number of
///     elements to include along that dimension, starting from the corresponding offset in
///     `destinationOrigin`.
///   - destinationPlane: The plane the method copies data to.
- (void)copyFromTensor:(id<MTLTensor>)sourceTensor
          sourceOrigin:(MTLTensorExtents *)sourceOrigin
      sourceDimensions:(MTLTensorExtents *)sourceDimensions
           sourcePlane:(MTLTensorPlaneType)sourcePlane
              toTensor:(id<MTLTensor>)destinationTensor
     destinationOrigin:(MTLTensorExtents *)destinationOrigin
 destinationDimensions:(MTLTensorExtents *)destinationDimensions
      destinationPlane:(MTLTensorPlaneType)destinationPlane API_AVAILABLE(macos(27.0), ios(27.0));


/// Encodes a command that generates mipmaps for a texture instance from the base mipmap level up to the highest
/// mipmap level.
///
/// This method generates mipmaps for a mipmapped texture. The texture you provide needs to have a
/// ``MTLTexture/mipmapLevelCount`` greater than `1`, and a color-renderable or color-filterable
/// ``MTLTexture/pixelFormat``.
///
/// - Parameter texture: A mipmapped, color-renderable or color-filterable ``MTLTexture`` instance the command generates mipmaps for.
- (void)generateMipmapsForTexture:(id<MTLTexture>)texture;

/// Encodes a command that fills a buffer with a constant value for each byte.
///
/// - Parameters:
///   - buffer: A ``MTLBuffer`` instance for which this command assigns each byte in a range to a value.
///   - range:  A range of bytes within `buffer` the command assigns value to. When calling this method, pass in a
///             range with a length greater than `0`.
///   - value:  The value to write to each byte.
- (void)fillBuffer:(id<MTLBuffer>)buffer
             range:(NSRange)range
             value:(uint8_t)value;

/// Encodes a command that modifies the contents of a texture to improve the performance of GPU accesses
/// to its contents.
///
/// Optimizing a texture for GPU access may affect the performance of CPU accesses, however, the data the CPU
/// retrieves from the texture remains consistent.
///
/// You typically run this command for:
/// * Textures the GPU accesses for an extended period of time.
/// * Textures with a ``MTLTextureDescriptor/storageMode`` property that's ``MTLStorageMode/MTLStorageModeShared`` or
///   ``MTLStorageMode/MTLStorageModeManaged``.
///
/// - Parameter texture: A ``MTLTexture`` instance the command optimizes for GPU access.
- (void)optimizeContentsForGPUAccess:(id<MTLTexture>)texture;

/// Encodes a command that modifies the contents of a texture instance to improve the performance of GPU accesses
/// to its contents in a specific region.
///
/// Optimizing a texture for GPU access may affect the performance of CPU accesses, however, the data the CPU
/// retrieves from the texture remains consistent.
///
/// You typically run this command for:
/// * Textures the GPU accesses for an extended period of time.
/// * Textures with a ``MTLTextureDescriptor/storageMode`` property that's ``MTLStorageMode/MTLStorageModeShared`` or
///   ``MTLStorageMode/MTLStorageModeManaged``.
///
/// - Parameters:
///   - texture: A ``MTLTexture`` the command optimizes for GPU access.
///   - slice:   A slice within `texture`.
///   - level:   A mipmap level within `texture`.
- (void)optimizeContentsForGPUAccess:(id<MTLTexture>)texture
                               slice:(NSUInteger)slice
                               level:(NSUInteger)level;

/// Encodes a command that modifies the contents of a texture to improve the performance of CPU accesses to
/// its contents.
///
/// Optimizing a texture for CPU access may affect the performance of GPU accesses, however, the data the GPU
/// retrieves from the texture remains consistent.
///
/// You typically use this command for:
/// * Textures the CPU accesses for an extended period of time.
/// * Textures with a ``MTLTextureDescriptor/storageMode`` property that's ``MTLStorageMode/MTLStorageModeShared`` or
///   ``MTLStorageMode/MTLStorageModeManaged``.
///
/// - Parameter texture: A ``MTLTexture`` instance the command optimizes for CPU access.
- (void)optimizeContentsForCPUAccess:(id<MTLTexture>)texture;

/// Encodes a command that modifies the contents of a texture to improve the performance of CPU accesses
/// to its contents in a specific region.
///
/// Optimizing a texture for CPU access may affect the performance of GPU accesses, however, the data the GPU
/// retrieves from the texture remains consistent.
///
/// You typically use this command for:
/// * Textures the CPU accesses for an extended period of time.
/// * Textures with a ``MTLTextureDescriptor/storageMode`` property that's ``MTLStorageMode/MTLStorageModeShared`` or
///   ``MTLStorageMode/MTLStorageModeManaged``.
///
/// - Parameters:
///   - texture: A ``MTLTexture`` the command optimizes for CPU access.
///   - slice:   A slice within `texture`.
///   - level:   A mipmap level within `texture`.
- (void)optimizeContentsForCPUAccess:(id<MTLTexture>)texture
                               slice:(NSUInteger)slice
                               level:(NSUInteger)level;

/// Encodes a command that resets a range of commands in an indirect command buffer.
///
/// - Parameters:
///   - buffer: An ``MTLIndirectCommandBuffer`` the command resets.
///   - range: A range of commands within `buffer`.
- (void)resetCommandsInBuffer:(id<MTLIndirectCommandBuffer>)buffer
                    withRange:(NSRange)range;

/// Encodes a command that copies commands from an indirect command buffer into another.
///
/// - Parameters:
///   - source:           An ``MTLIndirectCommandBuffer`` instance from where the command copies.
///   - sourceRange:      The range of commands in `source` to copy.
///                       The copy operation requires that the source range starts at a valid execution point.
///   - destination:      Another ``MTLIndirectCommandBuffer`` instance into which the command copies.
///   - destinationIndex: An index in `destination` into where the command copies content to. The copy operation requires
///                       that the destination index is a valid execution point with enough space left in `destination`
///                       to accommodate `sourceRange.count` commands.
- (void)copyIndirectCommandBuffer:(id<MTLIndirectCommandBuffer>)source
                      sourceRange:(NSRange)sourceRange
                      destination:(id<MTLIndirectCommandBuffer>)destination
                 destinationIndex:(NSUInteger)destinationIndex;

/// Encode a command to attempt to improve the performance of a range of commands within an indirect command buffer.
///
/// - Parameters:
///   - indirectCommandBuffer: An ``MTLIndirectCommandBuffer`` instance that this command optimizes.
///   - range:                 A range of commands within `indirectCommandBuffer`.
- (void)optimizeIndirectCommandBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer
                            withRange:(NSRange)range;

/// Sets an argument table for the compute shader stage of this pipeline.
///
/// Metal takes a snapshot of the resources in the argument table when you make dispatch or execute calls on
/// this encoder instance. Metal makes the snapshot contents available to the compute shader function of the
/// current pipeline state.
///
/// - Parameters:
///   - argumentTable: A ``MTL4ArgumentTable`` to set on the command encoder.
- (void)setArgumentTable:(nullable id<MTL4ArgumentTable>)argumentTable;

/// Encodes an acceleration structure build into the command buffer.
///
/// Before you build an instance acceleration structure, you are responsible for ensuring the build operations for all
/// primitive acceleration structures is complete. The built acceleration structure doesn't retain any references to
/// the input buffers of the descriptor, such as the vertex buffer or instance buffer, among others.
///
/// The acceleration structure build process may continue as long as the command buffer is not completed. However,
/// you can safely encode ray tracing work against the acceleration structure if you schedule and synchronize the
/// command buffers that contain this ray tracing work such that the command buffer with the build command is complete
/// by the time ray tracing starts.
///
/// You are responsible for ensuring that the acceleration structure and scratch buffer are at least the size
/// that the query ``MTLDevice/accelerationStructureSizesWithDescriptor:`` returns.
///
/// Use an instance of ``MTLResidencySet`` to mark residency of the scratch buffer the `scratchBuffer` parameter references,
/// as well as for all the primitive acceleration structures you directly and indirectly reference.
///
/// - Parameters:
///   - accelerationStructure: Acceleration structure storage to build into.
///   - descriptor:            A descriptor for the acceleration structure Metal builds.
///   - scratchBuffer:         Scratch buffer Metal can use while building the acceleration structure.
///                            Metal may overwrite the contents of this buffer, and you should consider
///                            them "undefined" after the refit operation starts and completes.
- (void)buildAccelerationStructure:(id <MTLAccelerationStructure>)accelerationStructure
                        descriptor:(MTL4AccelerationStructureDescriptor *)descriptor
                     scratchBuffer:(MTL4BufferRange)scratchBuffer;

/// Encodes an acceleration structure refit into the command buffer.
///
/// You refit an acceleration structure to update it when the geometry it references changes. This operation is typically
/// much faster than rebuilding the acceleration structure from scratch. The trade off is that after you refit the
/// acceleration structure, its quality, as well as the performance of any subsequent ray tracing operation degrades,
/// depending on how much the geometry changes.
///
/// After certain operations, refitting an acceleration structure may not be possible, for example, after adding or
/// removing geometry.
///
/// When you refit an acceleration structure, you can do so in place, by specifying the same source and destination
/// acceleration structures, or by providing a `nil` destination acceleration structure. If the source and destination
/// acceleration structures aren't the same, then you are responsible for ensuring they don't overlap in memory.
///
/// Typically, the destination acceleration structure is at least as large as the source acceleration structure,
/// except in cases where you compact the source acceleration structure. In this case, you need to allocate the
/// destination acceleration to be at least as large as the compacted size of the source acceleration structure.
///
/// The scratch buffer you provide for the refit operation needs to be at least as large as the size that the query
/// ``MTLDevice/accelerationStructureSizesWithDescriptor:`` returns. If the size this query returns is zero, you
/// can omit providing a scratch buffer by passing `0` as the address to the `scratchBuffer` parameter.
///
/// Use an instance of ``MTLResidencySet`` to mark residency of the scratch buffer the `scratchBuffer` parameter references,
/// as well as for all the instance and primitive acceleration structures you directly and indirectly reference.
///
/// - Parameters:
///   - sourceAccelerationStructure:      Acceleration structure to refit.
///   - descriptor:                       A descriptor for the acceleration structure to refit.
///   - destinationAccelerationStructure: Acceleration structure to store the refit result into.
///                                       If `destinationAccelerationStructure` is `nil`, Metal performs an in-place
///                                       refit operation of the `sourceAccelerationStructure`.
///   - scratchBuffer:                    Scratch buffer Metal can use while refitting the acceleration structure.
///                                       Metal may overwrite the contents of this buffer, and you should consider
///                                       them "undefined" after the refit operation starts and completes.
- (void)refitAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                        descriptor:(MTL4AccelerationStructureDescriptor *)descriptor
                       destination:(nullable id <MTLAccelerationStructure>)destinationAccelerationStructure
                     scratchBuffer:(MTL4BufferRange)scratchBuffer;

/// Encodes an acceleration structure refit operation into the command buffer, providing additional options.
///
/// You refit an acceleration structure to update it when the geometry it references changes. This operation is typically
/// much faster than rebuilding the acceleration structure from scratch. The trade off is that after you refit the
/// acceleration structure, its quality, as well as the performance of any subsequent ray tracing operation degrades,
/// depending on how much the geometry changes.
///
/// After certain operations, refitting an acceleration structure may not be possible, for example, after adding or
/// removing geometry.
///
/// When you refit an acceleration structure, you can do so in place, by specifying the same source and destination
/// acceleration structures, or by providing a `nil` destination acceleration structure. If the source and destination
/// acceleration structures aren't the same, then you are responsible for ensuring they don't overlap in memory.
///
/// Typically, the destination acceleration structure is at least as large as the source acceleration structure,
/// except in cases where you compact the source acceleration structure. In this case, you need to allocate the
/// destination acceleration to be at least as large as the compacted size of the source acceleration structure.
///
/// The scratch buffer you provide for the refit operation needs to be at least as large as the size that the query
/// ``MTLDevice/accelerationStructureSizesWithDescriptor:`` returns. If the size this query returns is zero, you
/// can omit providing a scratch buffer by passing `0` as the address to the `scratchBuffer` parameter.
///
/// Use an instance of ``MTLResidencySet`` to mark residency of the scratch buffer the `scratchBuffer` parameter references,
/// as well as for all the instance and primitive acceleration structures you directly and indirectly reference.
///
/// - Parameters:
///   - sourceAccelerationStructure:      Acceleration structure to refit.
///   - descriptor:                       A descriptor for the acceleration structure to refit.
///   - destinationAccelerationStructure: Acceleration structure to store the refit result into.
///                                       If `destinationAccelerationStructure` is `nil`, Metal performs an in-place
///                                       refit operation of the `sourceAccelerationStructure`.
///   - scratchBuffer:                    Scratch buffer Metal can use while refitting the acceleration structure.
///                                       Metal may overwrite the contents of this buffer, and you should consider
///                                       them "undefined" after the refit operation starts and completes.
///   - options:                          Options specifying the elements of the acceleration structure to refit.
- (void)refitAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                        descriptor:(MTL4AccelerationStructureDescriptor *)descriptor
                       destination:(nullable id <MTLAccelerationStructure>)destinationAccelerationStructure
                     scratchBuffer:(MTL4BufferRange)scratchBuffer
                           options:(MTLAccelerationStructureRefitOptions)options;

/// Encodes an acceleration structure copy operation into the command buffer.
///
/// You are responsible for ensuring the source and destination acceleration structures don't overlap in memory.
/// If this is an instance acceleration structure, Metal preserves references to the primitive acceleration structures
/// it references.
///
/// Typically, the destination acceleration structure is at least as large as the source acceleration structure,
/// except in cases where you compact the source acceleration structure. In this case, you need to allocate the
/// destination acceleration to be at least as large as the compacted size of the source acceleration structure.
///
/// - Parameters:
///   - sourceAccelerationStructure:      Acceleration structure to copy from.
///   - destinationAccelerationStructure: Acceleration structure to copy to.
- (void)copyAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
          toAccelerationStructure:(id <MTLAccelerationStructure>)destinationAccelerationStructure;

/// Encodes a command to compute the size an acceleration structure can compact into, writing the result
/// into a buffer.
///
/// This size is potentially smaller than the acceleration structure. To perform compaction, you typically read
/// this size from the buffer once the command buffer completes. You then use it to allocate a new, potentially
/// smaller acceleration structure. Finally, you call the ``copyAndCompactAccelerationStructure:toAccelerationStructure:``
/// method to perform the copy.
///
/// - Parameters:
///   - accelerationStructure: Source acceleration structure.
///   - buffer:                Destination size buffer. Metal writes the compacted size as a 64-bit unsigned integer
///                            value, representing the compacted size in bytes.
- (void)writeCompactedAccelerationStructureSize:(id <MTLAccelerationStructure>)accelerationStructure
                                       toBuffer:(MTL4BufferRange)buffer;

/// Encodes a command to copy and compact an acceleration structure.
///
/// You are responsible for ensuring that the source and destination acceleration structures don't overlap in memory.
/// If this is an instance acceleration structure, Metal preserves references to primitive acceleration structures it
/// references.
///
/// This operation requires that the destination acceleration structure is at least as large as the compacted size of
/// the source acceleration structure. You can compute this size by calling the
/// ``writeCompactedAccelerationStructureSize:toBuffer:`` method.
///
/// - Parameters:
///   - sourceAccelerationStructure:      Acceleration structure to copy and compact.
///   - destinationAccelerationStructure: Acceleration structure to copy to.
- (void)copyAndCompactAccelerationStructure:(id <MTLAccelerationStructure>)sourceAccelerationStructure
                    toAccelerationStructure:(id <MTLAccelerationStructure>)destinationAccelerationStructure;

/// Writes a GPU timestamp into a heap.
///
/// The method ensures that any prior work finishes, but doesn't delay any subsequent work.
///
/// You can alter this command's behavior through the `granularity` parameter.
/// - Pass ``MTL4TimestampGranularity/MTL4TimestampGranularityRelaxed`` to allow Metal to provide timestamps with
/// minimal impact to runtime performance, but with less detail. For example, the command may group all timestamps for
/// a pass together.
/// - Pass ``MTL4TimestampGranularity/MTL4TimestampGranularityPrecise`` to request that Metal provides timestamps
/// with the most detail. This can affect runtime performance.
///
/// - Parameters:
///   - granularity: ``MTL4TimestampGranularity`` hint to Metal about acceptable the level of precision.
///   - counterHeap: ``MTL4CounterHeap`` to write timestamps into.
///   - index:       The index value into which Metal writes the timestamp.
- (void)writeTimestampWithGranularity:(MTL4TimestampGranularity)granularity
                             intoHeap:(id<MTL4CounterHeap>)counterHeap
                              atIndex:(NSUInteger)index;

@end

NS_ASSUME_NONNULL_END


#endif //MTL4ComputeCommandEncoder_h
