//
//  MTL4MeshRenderPipeline.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4MeshRenderPipeline_h
#define MTL4MeshRenderPipeline_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTL4RenderPipeline.h>
#import <Metal/MTLFunctionHandle.h>
#import <Metal/MTLRenderPipeline.h>
#import <Metal/MTLTypes.h>

@class MTL4FunctionDescriptor;

NS_ASSUME_NONNULL_BEGIN

/// Groups together properties you use to create a mesh render pipeline state object.
///
/// Compared to ``MTLMeshRenderPipelineDescriptor``, this interface doesn't offer a mechanism to hint to Metal mutability
/// of object, mesh, or fragment buffers. Additionally, when you use this descriptor, you don't specify binary archives.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4MeshRenderPipelineDescriptor : MTL4PipelineDescriptor

/// Assigns a function descriptor representing the function this pipeline executes for each *object* in the object shader stage.
@property (nullable, readwrite, nonatomic, copy) MTL4FunctionDescriptor* objectFunctionDescriptor;

/// Assigns a function descriptor representing the function this pipeline executes for each primitive in the mesh shader stage.
@property (nullable, readwrite, nonatomic, copy) MTL4FunctionDescriptor* meshFunctionDescriptor;

/// Assigns a function descriptor representing the function this pipeline executes for each fragment.
@property (nullable, readwrite, nonatomic, copy) MTL4FunctionDescriptor* fragmentFunctionDescriptor;

/// Controls the largest number of threads the pipeline state can execute in a single object shader threadgroup dispatch.
///
/// This number represents the maximum size of the product of the components of parameter `threadsPerObjectThreadgroup`
/// that Metal can use when drawing with this pipeline in mesh shader dispatch methods, such as
/// ``MTL4RenderCommandEncoder/drawMeshThreadgroups:threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:``.
///
/// The compiler's optimizer can use the value of this property to generate more efficient code, specifically when
/// the value doesn't exceed the thread execution width of the underlying GPU.
///
/// The default value of this property is `0`, which indicates that the number you pass to attribute
/// `[[max_total_threads_per_threadgroup(N)]]` of the pipeline's object function determines the maximum
/// total threads per threadgroup.
///
/// When you specify both the `[[max_total_threads_per_threadgroup(N)]]` attribute and this property, you are responsible
/// for making sure these values match.
///
/// Additionally, you are responsible for ensuring this value doesn't exceed the "maximum threads per threadgroup"
/// device limit documented in the "Metal Feature Set Tables" PDF:
/// <https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf>.
///
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerObjectThreadgroup;

/// Controls the largest number of threads the pipeline state can execute in a single mesh shader threadgroup dispatch.
///
/// This number represents the maximum size of the product of the components of parameter `threadsPerMeshThreadgroup`
/// that Metal can use when drawing with this pipeline in mesh shader dispatch methods, such as
/// ``MTL4RenderCommandEncoder/drawMeshThreadgroups:threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:``.
///
/// The compiler's optimizer can use the value of this property to generate more efficient code, specifically when
/// the value doesn't exceed the thread execution width of the underlying GPU.
///
/// The default value of this property is `0`, thish indicates that the Metal Shader Language attribute
/// `[[max_total_threads_per_threadgroup]]` you attache to the pipeline's mesh shader function determines
/// the value of this property.
///
///
/// When you specify both the `[[max_total_threads_per_threadgroup(N)]]` attribute and this property, you are responsible
/// for making sure these values match.
///
/// Additionally, you are responsible for ensuring this value doesn't exceed the "maximum threads per threadgroup"
/// device limit documented in the "Metal Feature Set Tables" PDF:
/// <https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf>.
///
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerMeshThreadgroup;

/// Controls the required number of object threads-per-threadgroup when drawing with a mesh shader pipeline you create
/// from this descriptor.
///
/// This argument is optional, unless this pipeline uses `CooperativeTensors`, in which case you are responsible for providing it.
///
/// When this value is set to non-zero, you are responsible for ensuring the parameter `threadsPerObjectThreadgroup`
/// in any mesh dispatch draw calls that use this mesh render pipeline, such as
/// ``MTL4RenderCommandEncoder/drawMeshThreadgroups:threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:``,
/// match it.
///
/// Setting this value to a size of 0 in every dimension disables this property.
///
@property(readwrite, nonatomic) MTLSize requiredThreadsPerObjectThreadgroup;

/// Controls the required number of mesh threads-per-threadgroup when drawing with a mesh shader pipeline you create
/// from this descriptor.
///
/// This argument is optional, unless this pipeline uses `CooperativeTensors`, in which case you are responsible for providing it.
///
/// When this value is set to non-zero, you are responsible for ensuring the parameter `threadsPerMeshThreadgroup`
/// in any mesh dispatch draw calls that use this mesh render pipeline, such as
/// ``MTL4RenderCommandEncoder/drawMeshThreadgroups:threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:``,
/// match it.
///
/// Setting this value to a size of 0 in every dimension disables this property.
///
@property(readwrite, nonatomic) MTLSize requiredThreadsPerMeshThreadgroup;

/// Provides a guarantee to Metal regarding the number of threadgroup threads for the object stage of a pipeline you
/// create from this descriptor.
///
/// If you set this property to <doc://com.apple.documentation/documentation/swift/true>, you state to Metal that when
/// you use a mesh render pipeline you create from this descriptor, the number of threadgroup threads you dispatch for
/// the object stage is a multiple of its ``MTLRenderPipelineState/objectThreadExecutionWidth``. The compiler's optimizer can use this guarantee to generate
///  more efficient code.
///
/// This property's default value is <doc://com.apple.documentation/documentation/swift/false>.
@property (readwrite, nonatomic) BOOL objectThreadgroupSizeIsMultipleOfThreadExecutionWidth;

/// Provides a guarantee to Metal regarding the number of threadgroup threads for the mesh stage of a pipeline you
/// create from this descriptor.
///
/// If you set this property to <doc://com.apple.documentation/documentation/swift/true>, you state to Metal that when
/// you use a mesh render pipeline you create from this descriptor, the number of threadgroup threads you dispatch for
/// the mesh stage is a multiple of its ``MTLRenderPipelineState/meshThreadExecutionWidth``. The compiler's optimizer
/// can use this guarantee to generate more efficient code.
///
/// This property's default value is <doc://com.apple.documentation/documentation/swift/false>.
@property (readwrite, nonatomic) BOOL meshThreadgroupSizeIsMultipleOfThreadExecutionWidth;

/// Reserves storage for the object-to-mesh stage payload.
///
/// This property determines the size, in bytes, of the buffer you indicate via the Metal Shading Language `[[payload]]`
/// attribute in the object and mesh shader functions of the mesh render pipeline.
///
/// If this value is `0`, Metal derives the size from the (dereferenced) type you declare for the payload in the object
/// shader function. If the type is a pointer, Metal reserves space for a single element.
///
/// The default value is `0`.
@property (readwrite, nonatomic) NSUInteger payloadMemoryLength;


/// Controls the largest number of threads the pipeline state can execute when the object stage of a mesh
/// render pipeline you create from this descriptor dispatches its mesh stage.
///
/// This number represents the maximum size of the product of the components of the parameter you pass to Metal
/// Shading Language's built-in function `mesh_grid_properties::set_threadgroups_per_grid`.
///
/// The default value of this property is `0`, which indicates that the Metal Shading Language attribute
/// `[[max_total_threadgroups_per_mesh_grid(N)]]` you attach to the pipeline's mesh shader function determines
/// the value of this property.
///
/// When you specify both the `[[max_total_threadgroups_per_mesh_grid(N)]]` attribute and this property, you are
/// responsible for making sure these values match.
///
/// Additionally, you are responsible for ensuring this value doesn't exceed the "maximum threads per mesh grid"
/// device limit documented in the "Metal Feature Set Tables" PDF:
/// <https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf>.
///
@property (readwrite, nonatomic) NSUInteger maxTotalThreadgroupsPerMeshGrid;

/// Sets number of samples this pipeline applies for each fragment.
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

/// Provides static linking information for the object stage of the render pipeline.
///
/// Use this property to link extra shader functions to the object stage of the render pipeline.
@property (null_resettable, copy, nonatomic) MTL4StaticLinkingDescriptor* objectStaticLinkingDescriptor;

/// Provides static linking information for the mesh stage of the render pipeline.
///
/// Use this property to link extra shader functions to the mesh stage of the render pipeline.
@property (null_resettable, copy, nonatomic) MTL4StaticLinkingDescriptor* meshStaticLinkingDescriptor;

/// Provides static linking information for the fragment stage of the render pipeline.
///
/// Use this property to link extra shader functions to the fragment stage of the render pipeline.
@property (null_resettable, copy, nonatomic) MTL4StaticLinkingDescriptor* fragmentStaticLinkingDescriptor;

/// Indicates whether you can use the render pipeline to create new pipelines by adding binary functions to the object
/// shader function’s callable functions list.
@property (readwrite, nonatomic) BOOL supportObjectBinaryLinking;

/// Indicates whether you can use the render pipeline to create new pipelines by adding binary functions to the mesh
/// shader function’s callable functions list.
@property (readwrite, nonatomic) BOOL supportMeshBinaryLinking;

/// Indicates whether you can use the render pipeline to create new pipelines by adding binary functions to the fragment
/// shader function’s callable functions list.
@property (readwrite, nonatomic) BOOL supportFragmentBinaryLinking;

/// Sets the logical-to-physical rendering remap state.
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
#endif // MTL4MeshRenderPipeline_h
