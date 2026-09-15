//
//  MTL4RenderPipeline.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4RenderPipeline_h
#define MTL4RenderPipeline_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTL4PipelineState.h>
#import <Metal/MTLFunctionHandle.h>
#import <Metal/MTLRenderPipeline.h>
#import <Metal/MTLTypes.h>

NS_ASSUME_NONNULL_BEGIN

@class MTL4FunctionDescriptor;
@protocol MTL4BinaryFunction;
@class MTL4StaticLinkingDescriptor;

/// Enumerates possible behaviors of how a pipeline maps its logical outputs to its color attachments.
API_AVAILABLE(macos(26.0), ios(26.0))
typedef NS_ENUM(NSInteger, MTL4LogicalToPhysicalColorAttachmentMappingState) {
    
    /// Treats the logical color attachment descriptor array for render and tile render pipelines to match the physical one.
    ///
    /// This is the default behavior, which produces an identity mapping.
    MTL4LogicalToPhysicalColorAttachmentMappingStateIdentity = 0,
    
    /// Deduces the color attachment mapping by inheriting it from the color attachment map of the current encoder.
    ///
    /// Use this setting to indicate Metal should inherit the mapping from the ``colorAttachmentMap`` property of the current
    /// ``MTL4RenderCommandEncoder`` or ``MTLRenderCommandEncoder`` in use at draw time.
    MTL4LogicalToPhysicalColorAttachmentMappingStateInherited = 1,
};

// Groups together properties of a color attachment for a ``MTL4RenderPipeline``.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4RenderPipelineColorAttachmentDescriptor : NSObject <NSCopying>

/// Configures the pixel format.
///
/// This property defaults to ``MTLPixelFormatInvalid``.
@property (nonatomic) MTLPixelFormat pixelFormat;

/// Configure the blend state for color attachments the pipeline state uses.
///
/// This property's default value is ``MTL4BlendStateDisabled``.
@property (nonatomic) MTL4BlendState blendingState;

/// Configures the source RGB blend factor.
///
/// This property defaults to ``MTLBlendFactorOne``.
@property (nonatomic) MTLBlendFactor sourceRGBBlendFactor;

/// Configures the destination RGB blend factor.
///
/// This property defaults to ``MTLBlendFactorZero``.
@property (nonatomic) MTLBlendFactor destinationRGBBlendFactor;

/// Configures the RGB blend operation.
///
/// This property defaults to ``MTLBlendOperationAdd``.
@property (nonatomic) MTLBlendOperation rgbBlendOperation;

/// Configures the source-alpha blend factor.
///
/// This property defaults to ``MTLBlendFactorOne``.
@property (nonatomic) MTLBlendFactor sourceAlphaBlendFactor;

/// Configures the destination-alpha blend factor.
///
/// This property defaults to ``MTLBlendFactorZero``.
@property (nonatomic) MTLBlendFactor destinationAlphaBlendFactor;

/// Configures the alpha blending operation.
///
/// This property defaults to ``MTLBlendOperationAdd``.
@property (nonatomic) MTLBlendOperation alphaBlendOperation;

/// Configures the color write mask.
///
/// This property defaults to ``MTLColorWriteMaskAll``.
@property (nonatomic) MTLColorWriteMask writeMask;

/// Resets this descriptor to its default state.
- (void)reset;
@end

/// An array of color attachment descriptions for a render pipeline.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4RenderPipelineColorAttachmentDescriptorArray : NSObject <NSCopying>

/// Accesses a color attachment at a specific index.
///
/// - Parameter attachmentIndex: Index of the attachment to access.
- (MTL4RenderPipelineColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;

/// Sets an attachment at an index.
///
/// This function offers 'copy' semantics.
///
/// You can safely set the color attachment at any legal index to nil. This has the effect of resetting that attachment
/// descriptor's state to its default values.
///
/// - Parameters:
///   - attachment: the descriptor of the attachment to set.
///   - attachmentIndex: the index of the attachment within the array.
- (void)setObject:(nullable MTL4RenderPipelineColorAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

/// Resets the elements of the descriptor array
- (void)reset;
@end


@protocol MTL4BinaryFunction;
@class MTLVertexDescriptor;

/// Allows you to specify additional binary functions to link to each stage of a render pipeline.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4RenderPipelineBinaryFunctionsDescriptor : NSObject<NSCopying>

/// Provides an array of binary functions representing additional binary vertex shader functions.
@property (nullable, nonatomic, copy) NSArray<id<MTL4BinaryFunction>>* vertexAdditionalBinaryFunctions;

/// Provides an array of binary functions representing additional binary fragment shader functions.
@property (nullable, nonatomic, copy) NSArray<id<MTL4BinaryFunction>>* fragmentAdditionalBinaryFunctions;

/// Provides an array of binary functions representing additional binary tile shader functions.
@property (nullable, nonatomic, copy) NSArray<id<MTL4BinaryFunction>>* tileAdditionalBinaryFunctions;

/// Provides an array of binary functions representing additional binary object shader functions.
@property (nullable, nonatomic, copy) NSArray<id<MTL4BinaryFunction>>* objectAdditionalBinaryFunctions;

/// Provides an array of binary functions representing additional binary mesh shader functions.
@property (nullable, nonatomic, copy) NSArray<id<MTL4BinaryFunction>>* meshAdditionalBinaryFunctions;

/// Resets this descriptor to its default state.
- (void)reset;

@end

/// Groups together properties to create a render pipeline state object.
///
/// Compared to ``MTLRenderPipelineDescriptor``, this interface doesn't offer a mechanism to hint to Metal mutability of
/// vertex and fragment buffers. Additionally, using this descriptor, you don't specify binary archives.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4RenderPipelineDescriptor : MTL4PipelineDescriptor

/// Assigns the shader function that this pipeline executes for each vertex.
@property (nullable, readwrite, nonatomic, copy) MTL4FunctionDescriptor* vertexFunctionDescriptor;

/// Assigns the shader function that this pipeline executes for each fragment.
///
/// When you don't specify a fragment function, you need to disable rasterization by setting property
/// ``rasterizationEnabled`` to false.
@property (nullable, readwrite, nonatomic, copy) MTL4FunctionDescriptor* fragmentFunctionDescriptor;

/// Configures an optional vertex descriptor for the vertex input.
///
/// A vertex descriptor specifies the layout of your vertex data, allowing your vertex shaders to access the content
/// in your vertex arrays via the `[[stage_in]]` attribute in Metal Shading Language.
@property (nullable, nonatomic, copy) MTLVertexDescriptor* vertexDescriptor;

/// Controls the number of samples this pipeline applies for each fragment.
@property (readwrite, nonatomic) NSUInteger rasterSampleCount;

/// Indicates whether to read and use the alpha channel fragment output of color attachments to compute a sample coverage mask.
@property (readwrite, nonatomic) MTL4AlphaToCoverageState alphaToCoverageState;

/// Indicates whether the pipeline forces alpha channel values of color attachments to the largest representable value.
@property (readwrite, nonatomic) MTL4AlphaToOneState alphaToOneState;

/// Determines whether the pipeline rasterizes primitives.
///
/// By default, this value is <doc://com.apple.documentation/documentation/swift/true>, specifying that this pipeline
/// rasterizes primitives. Set this property to <doc://com.apple.documentation/documentation/swift/false> when you
/// don't provide a fragment shader function via function ``fragmentFunctionDescriptor``.
@property (readwrite, nonatomic, getter=isRasterizationEnabled) BOOL rasterizationEnabled;

/// Determines the maximum value that can you can pass as the pipeline's amplification count.
///
/// This property controls the maximum count you pass to ``MTL4RenderCommandEncoder/setVertexAmplificationCount:viewMappings:``
/// when using vertex amplification with this pipeline.
@property (readwrite, nonatomic) NSUInteger maxVertexAmplificationCount;

/// Accesses an array containing descriptions of the color attachments this pipeline writes to.
@property (readonly) MTL4RenderPipelineColorAttachmentDescriptorArray* colorAttachments;

/// Assigns type of primitive topology this pipeline renders.
@property (readwrite, nonatomic) MTLPrimitiveTopologyClass inputPrimitiveTopology;

/// Provides static linking information for the vertex stage of the render pipeline.
///
/// Use this property to link extra shader functions to the vertex stage of the render pipeline.
@property (null_resettable, copy, nonatomic) MTL4StaticLinkingDescriptor *vertexStaticLinkingDescriptor;

/// Provides static linking information for the fragment stage of the render pipeline.
///
/// Use this property to link extra shader functions to the fragment stage of the render pipeline.
@property (null_resettable, copy, nonatomic) MTL4StaticLinkingDescriptor *fragmentStaticLinkingDescriptor;

/// Indicates whether you can use the render pipeline to create new pipelines by
/// adding binary functions to the vertex shader function’s callable functions list.
@property (readwrite, nonatomic) BOOL supportVertexBinaryLinking;

/// Indicates whether you can use the pipeline to create new pipelines by
/// adding binary functions to the fragment shader function’s callable functions list.
@property (readwrite, nonatomic) BOOL supportFragmentBinaryLinking;

/// Configures a logical-to-physical rendering remap state.
///
/// Use this property to assign how a ``MTL4RenderCommandEncoder`` instance maps the output of your fragment shader to
/// physical color attachments.
@property (readwrite, nonatomic) MTL4LogicalToPhysicalColorAttachmentMappingState colorAttachmentMappingState;

/// Indicates whether the pipeline supports indirect command buffers.
@property (readwrite, nonatomic) MTL4IndirectCommandBufferSupportState supportIndirectCommandBuffers;

/// Resets this descriptor to its default state.
- (void)reset;

@end

NS_ASSUME_NONNULL_END
#endif // MTL4RenderPipeline_h
