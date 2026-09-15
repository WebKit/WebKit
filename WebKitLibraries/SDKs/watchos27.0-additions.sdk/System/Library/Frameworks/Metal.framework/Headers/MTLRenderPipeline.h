//
//  MTLRenderPipeline.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>
#import <Metal/MTLRenderCommandEncoder.h>
#import <Metal/MTLRenderPass.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLFunctionConstantValues.h>
#import <Metal/MTLPipeline.h>
#import <Metal/MTLAllocation.h>


#import <Metal/MTLLinkedFunctions.h>
#import <Metal/MTLFunctionHandle.h>

NS_ASSUME_NONNULL_BEGIN
@class MTLVertexDescriptor;

typedef NS_ENUM(NSUInteger, MTLBlendFactor) {
    MTLBlendFactorZero = 0,
    MTLBlendFactorOne = 1,
    MTLBlendFactorSourceColor = 2,
    MTLBlendFactorOneMinusSourceColor = 3,
    MTLBlendFactorSourceAlpha = 4,
    MTLBlendFactorOneMinusSourceAlpha = 5,
    MTLBlendFactorDestinationColor = 6,
    MTLBlendFactorOneMinusDestinationColor = 7,
    MTLBlendFactorDestinationAlpha = 8,
    MTLBlendFactorOneMinusDestinationAlpha = 9,
    MTLBlendFactorSourceAlphaSaturated = 10,
    MTLBlendFactorBlendColor = 11,
    MTLBlendFactorOneMinusBlendColor = 12,
    MTLBlendFactorBlendAlpha = 13,
    MTLBlendFactorOneMinusBlendAlpha = 14,
    MTLBlendFactorSource1Color              API_AVAILABLE(macos(10.12), ios(11.0)) = 15,
    MTLBlendFactorOneMinusSource1Color      API_AVAILABLE(macos(10.12), ios(11.0)) = 16,
    MTLBlendFactorSource1Alpha              API_AVAILABLE(macos(10.12), ios(11.0)) = 17,
    MTLBlendFactorOneMinusSource1Alpha      API_AVAILABLE(macos(10.12), ios(11.0)) = 18,
    /// Defers assigning the blend factor.
    ///
    /// Until you specialize this value in the pipeline state, it:
    /// * behaves as `MTLBlendFactorOne` for `sourceRGBBlendFactor` and `sourceAlphaBlendFactor`
    /// * behaves as `MTLBlendFactorZero` for `destinationRGBBlendFactor` and `destinationAlphaBlendFactor`
    MTLBlendFactorUnspecialized API_AVAILABLE(macos(26.0), ios(26.0)) = 19,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_ENUM(NSUInteger, MTLBlendOperation) {
    MTLBlendOperationAdd = 0,
    MTLBlendOperationSubtract = 1,
    MTLBlendOperationReverseSubtract = 2,
    MTLBlendOperationMin = 3,
    MTLBlendOperationMax = 4,
    /// Defers assigning the blend operation.
    ///
    // Until you specialize this value in the pipeline state, it behaves as `MTLBlendOperationAdd`.
    MTLBlendOperationUnspecialized API_AVAILABLE(macos(26.0), ios(26.0)) = 5,

} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_OPTIONS(NSUInteger, MTLColorWriteMask) {
    MTLColorWriteMaskNone  = 0,
    MTLColorWriteMaskRed   = 0x1 << 3,
    MTLColorWriteMaskGreen = 0x1 << 2,
    MTLColorWriteMaskBlue  = 0x1 << 1,
    MTLColorWriteMaskAlpha = 0x1 << 0,
    MTLColorWriteMaskAll   = 0xf,
    /// Defers assigning the color write mask.
    ///
    /// Until you specialize this value in the pipeline state, it behaves as `MTLColorWriteMaskAll`.
    MTLColorWriteMaskUnspecialized API_AVAILABLE(macos(26.0), ios(26.0)) = 0x10,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_ENUM(NSUInteger, MTLPrimitiveTopologyClass) {
    MTLPrimitiveTopologyClassUnspecified = 0,
    MTLPrimitiveTopologyClassPoint = 1,
    MTLPrimitiveTopologyClassLine = 2,
    MTLPrimitiveTopologyClassTriangle = 3,
} API_AVAILABLE(macos(10.11), ios(12.0));

typedef NS_ENUM(NSUInteger, MTLTessellationPartitionMode) {
    MTLTessellationPartitionModePow2 = 0,
    MTLTessellationPartitionModeInteger = 1,
    MTLTessellationPartitionModeFractionalOdd = 2,
    MTLTessellationPartitionModeFractionalEven = 3,
} API_AVAILABLE(macos(10.12), ios(10.0));

typedef NS_ENUM(NSUInteger, MTLTessellationFactorStepFunction) {
    MTLTessellationFactorStepFunctionConstant = 0,
    MTLTessellationFactorStepFunctionPerPatch = 1,
    MTLTessellationFactorStepFunctionPerInstance = 2,
    MTLTessellationFactorStepFunctionPerPatchAndPerInstance = 3,
} API_AVAILABLE(macos(10.12), ios(10.0));

typedef NS_ENUM(NSUInteger, MTLTessellationFactorFormat) {
    MTLTessellationFactorFormatHalf = 0,
} API_AVAILABLE(macos(10.12), ios(10.0));

typedef NS_ENUM(NSUInteger, MTLTessellationControlPointIndexType) {
    MTLTessellationControlPointIndexTypeNone = 0,
    MTLTessellationControlPointIndexTypeUInt16 = 1,
    MTLTessellationControlPointIndexTypeUInt32 = 2,
} API_AVAILABLE(macos(10.12), ios(10.0));

@class MTLRenderPipelineColorAttachmentDescriptorArray;

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPipelineColorAttachmentDescriptor : NSObject <NSCopying>

/*! Pixel format.  Defaults to MTLPixelFormatInvalid */
@property (nonatomic) MTLPixelFormat pixelFormat;

/*! Enable blending.  Defaults to NO. */
@property (nonatomic, getter = isBlendingEnabled) BOOL blendingEnabled;

/*! Defaults to MTLBlendFactorOne */
@property (nonatomic) MTLBlendFactor sourceRGBBlendFactor;

/*! Defaults to MTLBlendFactorZero */
@property (nonatomic) MTLBlendFactor destinationRGBBlendFactor;

/*! Defaults to MTLBlendOperationAdd */
@property (nonatomic) MTLBlendOperation rgbBlendOperation;

/*! Defaults to MTLBlendFactorOne */
@property (nonatomic) MTLBlendFactor sourceAlphaBlendFactor;

/*! Defaults to MTLBlendFactorZero */
@property (nonatomic) MTLBlendFactor destinationAlphaBlendFactor;

/*! Defaults to MTLBlendOperationAdd */
@property (nonatomic) MTLBlendOperation alphaBlendOperation;

/* Other Options */

/*! Defaults to MTLColorWriteMaskAll */
@property (nonatomic) MTLColorWriteMask writeMask;

@end


/// Allows you to easily specify color attachment remapping from logical to physical indices.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTLLogicalToPhysicalColorAttachmentMap : NSObject<NSCopying>

/// Maps a physical color attachment index to a logical index.
///
/// - Parameters:
///   - physicalIndex: index of the color attachment's physical mapping.
///   - logicalIndex: index of the color attachment's logical mapping.
- (void)setPhysicalIndex:(NSUInteger)physicalIndex forLogicalIndex:(NSUInteger)logicalIndex;

/// Queries the physical color attachment index corresponding to a logical index.
- (NSUInteger)getPhysicalIndexForLogicalIndex:(NSUInteger)logicalIndex;

- (void)reset;
@end


MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@interface MTLRenderPipelineReflection : NSObject

@property (nonnull, readonly) NSArray <id<MTLBinding>> *vertexBindings API_AVAILABLE(macos(13.0), ios(16.0));
@property (nonnull, readonly) NSArray <id<MTLBinding>> *fragmentBindings API_AVAILABLE(macos(13.0), ios(16.0));
@property (nonnull, readonly) NSArray <id<MTLBinding>> *tileBindings API_AVAILABLE(macos(13.0), ios(16.0));
@property (nonnull, readonly) NSArray <id<MTLBinding>> *objectBindings API_AVAILABLE(macos(13.0), ios(16.0));
@property (nonnull, readonly) NSArray <id<MTLBinding>> *meshBindings API_AVAILABLE(macos(13.0), ios(16.0));
@property (nullable, readonly) NSArray <MTLArgument *> *vertexArguments API_DEPRECATED_WITH_REPLACEMENT("vertexBindings", macos(10.11, 13.0), ios(8.0, 16.0));
@property (nullable, readonly) NSArray <MTLArgument *> *fragmentArguments API_DEPRECATED_WITH_REPLACEMENT("fragmentBindings", macos(10.11, 13.0), ios(8.0, 16.0));
@property (nullable, readonly) NSArray <MTLArgument *> *tileArguments API_DEPRECATED_WITH_REPLACEMENT("tileBindings", macos(11.0, 13.0), macCatalyst(14.0, 16.0), ios(11.0, 16.0), tvos(14.5, 16.0));
@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPipelineDescriptor : NSObject <NSCopying>

@property (nullable, copy, nonatomic) NSString *label;

@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> vertexFunction;
@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> fragmentFunction;

@property (nullable, copy, nonatomic) MTLVertexDescriptor *vertexDescriptor;

/* Rasterization and visibility state */
@property (readwrite, nonatomic) NSUInteger sampleCount API_DEPRECATED_WITH_REPLACEMENT("rasterSampleCount", macos(10.11, 13.0), ios(8.0, 16.0));
@property (readwrite, nonatomic) NSUInteger rasterSampleCount;
@property (readwrite, nonatomic, getter = isAlphaToCoverageEnabled) BOOL alphaToCoverageEnabled;
@property (readwrite, nonatomic, getter = isAlphaToOneEnabled) BOOL alphaToOneEnabled;
@property (readwrite, nonatomic, getter = isRasterizationEnabled) BOOL rasterizationEnabled;


@property (readwrite, nonatomic) NSUInteger maxVertexAmplificationCount API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0));

@property (readonly) MTLRenderPipelineColorAttachmentDescriptorArray *colorAttachments;

@property (nonatomic) MTLPixelFormat depthAttachmentPixelFormat;
@property (nonatomic) MTLPixelFormat stencilAttachmentPixelFormat;

@property (readwrite, nonatomic) MTLPrimitiveTopologyClass inputPrimitiveTopology API_AVAILABLE(macos(10.11), ios(12.0), tvos(14.5));

@property (readwrite, nonatomic) MTLTessellationPartitionMode tessellationPartitionMode API_AVAILABLE(macos(10.12), ios(10.0));
@property (readwrite, nonatomic) NSUInteger maxTessellationFactor API_AVAILABLE(macos(10.12), ios(10.0));
@property (readwrite, nonatomic, getter = isTessellationFactorScaleEnabled) BOOL tessellationFactorScaleEnabled API_AVAILABLE(macos(10.12), ios(10.0));
@property (readwrite, nonatomic) MTLTessellationFactorFormat tessellationFactorFormat API_AVAILABLE(macos(10.12), ios(10.0));
@property (readwrite, nonatomic) MTLTessellationControlPointIndexType tessellationControlPointIndexType API_AVAILABLE(macos(10.12), ios(10.0));
@property (readwrite, nonatomic) MTLTessellationFactorStepFunction tessellationFactorStepFunction API_AVAILABLE(macos(10.12), ios(10.0));
@property (readwrite, nonatomic) MTLWinding tessellationOutputWindingOrder API_AVAILABLE(macos(10.12), ios(10.0));

@property (readonly) MTLPipelineBufferDescriptorArray *vertexBuffers API_AVAILABLE(macos(10.13), ios(11.0));
@property (readonly) MTLPipelineBufferDescriptorArray *fragmentBuffers API_AVAILABLE(macos(10.13), ios(11.0));

@property (readwrite, nonatomic) BOOL supportIndirectCommandBuffers API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @property binaryArchives
 @abstract The set of MTLBinaryArchive to search for compiled code when creating the pipeline state.
 @discussion Accelerate pipeline state creation by providing archives of compiled code such that no compilation needs to happen on the fast path.
 @see MTLBinaryArchive
 */
@property (readwrite, nullable, nonatomic, copy) NSArray<id<MTLBinaryArchive>> *binaryArchives API_AVAILABLE(macos(11.0), ios(14.0));


/*!
 @property vertexPreloadedLibraries
 @abstract The set of MTLDynamicLibrary to use to resolve external symbols for the vertexFunction before considering symbols from dependent MTLDynamicLibrary.
 @discussion Typical workflows use the libraries property of MTLCompileOptions to record dependent libraries at compile time without having to use vertexPreloadedLibraries.
 This property can be used to override symbols from dependent libraries for experimentation or evaluating alternative implementations.
 It can also be used to provide dynamic libraries that are dynamically created (for example, from source) that have no stable installName that can be used to automatically load from the file system.
 @see MTLDynamicLibrary
 */
@property (readwrite, nonnull, nonatomic, copy) NSArray<id<MTLDynamicLibrary>>* vertexPreloadedLibraries API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @property fragmentPreloadedLibraries
 @abstract The set of MTLDynamicLibrary to use to resolve external symbols for the fragmentFunction before considering symbols from dependent MTLDynamicLibrary.
 @discussion Typical workflows use the libraries property of MTLCompileOptions to record dependent libraries at compile time without having to use fragmentPreloadedLibraries.
 This property can be used to override symbols from dependent libraries for experimentation or evaluating alternative implementations.
 It can also be used to provide dynamic libraries that are dynamically created (for example, from source) that have no stable installName that can be used to automatically load from the file system.
 @see MTLDynamicLibrary
 */
@property (readwrite, nonnull, nonatomic, copy) NSArray<id<MTLDynamicLibrary>>* fragmentPreloadedLibraries API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @property vertexLinkedFunctions
 @abstract The set of functions to be linked with the pipeline state and accessed from the vertex function.
 @see MTLLinkedFunctions
 */
@property (null_resettable, copy, nonatomic) MTLLinkedFunctions *vertexLinkedFunctions
    API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @property fragmentLinkedFunctions
 @abstract The set of functions to be linked with the pipeline state and accessed from the fragment function.
 @see MTLLinkedFunctions
 */
@property (null_resettable, copy, nonatomic) MTLLinkedFunctions *fragmentLinkedFunctions
    API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @property supportAddingVertexBinaryFunctions
 @abstract This flag makes this pipeline support creating a new pipeline by adding binary functions.
 */
@property (readwrite, nonatomic) BOOL supportAddingVertexBinaryFunctions
API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @property supportFragmentAddingBinaryFunctions
 @abstract This flag makes this pipeline support creating a new pipeline by adding binary functions.
 */
@property (readwrite, nonatomic) BOOL supportAddingFragmentBinaryFunctions
API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @property maxVertexCallStackDepth
 @abstract The maximum depth of the call stack in stack frames from the shader. Defaults to 1 additional stack frame.
 */
@property (readwrite, nonatomic) NSUInteger maxVertexCallStackDepth
API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @property maxFragmentCallStackDepth
 @abstract The maximum depth of the call stack in stack frames from the shader. Defaults to 1 additional stack frame.
 */
@property (readwrite, nonatomic) NSUInteger maxFragmentCallStackDepth
API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));



/*!
 @method reset
 @abstract Restore all pipeline descriptor properties to their default values.
 */
- (void)reset;

/*!
 @property shaderValidation
 @abstract Toggle that determines whether Metal Shader Validation should be enabled or disabled for the pipeline.
 @discussion The value can be overridden using `MTL_SHADER_VALIDATION_ENABLE_PIPELINES` or `MTL_SHADER_VALIDATION_DISABLE_PIPELINES` Environment Variables.
 */
@property (readwrite, nonatomic) MTLShaderValidation shaderValidation API_AVAILABLE(macos(15.0), ios(18.0));

@end

MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0))
@interface MTLRenderPipelineFunctionsDescriptor : NSObject <NSCopying>

/*!
 @property vertexAdditionalBinaryFunctions
 @abstract The set of additional binary functions to be accessed from the vertex function in an incrementally created pipeline state.
 */
@property (nullable, nonatomic, copy) NSArray<id<MTLFunction>> *vertexAdditionalBinaryFunctions;

/*!
 @property fragmentAdditionalBinaryFunctions
 @abstract The set of additional binary functions to be accessed from the fragment function in an incrementally created pipeline state.
 */
@property (nullable, nonatomic, copy) NSArray<id<MTLFunction>> *fragmentAdditionalBinaryFunctions;

/*!
 @property tileAdditionalBinaryFunctions
 @abstract The set of additional binary functions to be accessed from the tile function in an incrementally created pipeline state.
 */
@property (nullable, nonatomic, copy) NSArray<id<MTLFunction>> *tileAdditionalBinaryFunctions;
@end

@class MTL4RenderPipelineBinaryFunctionsDescriptor;
@class MTL4PipelineDescriptor;

/*!
 @protocol MTLRenderPipelineState
 @abstract MTLRenderPipelineState represents a compiled render pipeline
 
 @discussion MTLRenderPipelineState is a compiled render pipeline and can be set on a MTLRenderCommandEncoder.
 */
API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@protocol MTLRenderPipelineState <MTLAllocation, NSObject>

@property (nullable, readonly) NSString *label;
@property (readonly) id <MTLDevice> device;

/// The render pipeline's reflection information, if available.
///
/// The property is `nil` by default to help reduce your app's memory footprint,
/// but you can create reflection information when your app needs it.
///
/// Create reflection information by building a pipeline from an
/// ``MTL4Compiler`` instance with the following steps:
///
/// 1. Configure the ``MTL4PipelineOptions/shaderReflection`` property of an ``MTL4PipelineOptions`` instance.
/// 2. Assign that instance to the ``MTL4PipelineDescriptor/options`` property of an ``MTL4PipelineDescriptor`` instance.
/// 3. Create a compute pipeline state by passing that pipeline descriptor to one of the ``MTL4Compiler`` instance's methods.
///
/// The property is `nil` when you create a pipeline state from an``MTLDevice`` instance,
/// such as with its ``MTLDevice/newRenderPipelineStateWithDescriptor:error:`` method.
@property (nullable, readonly) MTLRenderPipelineReflection* reflection API_AVAILABLE(macos(26.0), ios(26.0));

/// Obtains a function handle for the a specific function this pipeline links at the Metal IR level.
///
/// - Parameters:
///   - name: A string containing the name of the function.
///   - stage: The shader stage that uses the function.
///
/// - Returns: a function handle representing the function if present, otherwise `nil`.
- (nullable id<MTLFunctionHandle>)functionHandleWithName:(NSString*)name
                                                   stage:(MTLRenderStages)stage API_AVAILABLE(macos(26.0), ios(26.0));

/// Obtains the function handle for a specific function this pipeline state links at the binary level.
///
/// - Parameters:
///   - function: a binary function to retrieve the handle.
///   - stage: The shader stage that uses the function.
///
/// - Returns: a function handle representing the function if present, otherwise `nil`.
- (nullable id<MTLFunctionHandle>)functionHandleWithBinaryFunction:(id<MTL4BinaryFunction>)function
                                                             stage:(MTLRenderStages)stage API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a new render pipeline state by adding binary functions to each stage of this pipeline
/// state.
///
/// - Parameters:
///   - binaryFunctionsDescriptor: A non-`nil` dynamic linking descriptor.
///   - error: An optional pointer that Metal populates with information in case of an error.
///
/// - Returns: A new render pipeline state upon success, otherwise `nil`.
///
- (nullable id<MTLRenderPipelineState>)newRenderPipelineStateWithBinaryFunctions:(MTL4RenderPipelineBinaryFunctionsDescriptor*)binaryFunctionsDescriptor
                                                                           error:(NSError**)error API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a render pipeline descriptor from this pipeline that you can use for pipeline specialization.
///
/// Use this method to obtain a new ``MTL4PipelineDescriptor`` instance that you can use to specialize any unspecialized
/// properties in this pipeline state object.
///
/// The returned descriptor contains every unspecialized field in the current pipeline state object, set to unspecialized.
/// It may, however, not contain valid or accurate properties in any other field.
///
/// This descriptor is only valid for the purpose of calling specialization functions on the ``MTL4Compiler`` to
/// specialize this pipeline, for example: ``MTL4Compiler/newRenderPipelineStateBySpecializationWithDescriptor:pipeline:error:``.
///
/// Although this method returns the ``MTL4PipelineDescriptor`` base class, the concrete instance this method returns
/// corresponds to the specific descriptor type for the creation of this pipeline state, for example if a ``MTL4Compiler``
/// instance creates this current pipeline form a ``MTLTileRenderPipelineDescriptor``, this method returns a concrete
/// ``MTLTileRenderPipelineDescriptor`` instance.
///
/// - Returns: a new pipeline descriptor that you use for pipeline state specialization.
///
- (MTL4PipelineDescriptor*)newRenderPipelineDescriptorForSpecialization
    API_AVAILABLE(macos(26.0), ios(26.0));


/*!
 @property maxTotalThreadsPerThreadgroup
 @abstract The maximum total number of threads that can be in a single tile shader threadgroup.
 */
@property (readonly) NSUInteger maxTotalThreadsPerThreadgroup API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @property threadgroupSizeMatchesTileSize
 @abstract Returns true when the pipeline state requires a tile shader threadgroup size equal to the tile size
 */
@property (readonly) BOOL threadgroupSizeMatchesTileSize API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));



/*!
 @property imageblockSampleLength
 @brief Returns imageblock memory length used by a single sample when rendered using this pipeline.
 */
@property (readonly) NSUInteger imageblockSampleLength API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method imageblockMemoryLengthForDimensions:sampleCount:
 @brief Returns imageblock memory length for given image block dimensions. Dimensions must be valid tile dimensions.
 */
- (NSUInteger)imageblockMemoryLengthForDimensions:(MTLSize)imageblockDimensions API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));


@property (readonly) BOOL supportIndirectCommandBuffers API_AVAILABLE(macos(10.14), ios(12.0));


/*!
 @property maxTotalThreadsPerObjectThreadgroup
 @abstract The maximum total number of threads that can be in a single object shader threadgroup.
 @discussion This value is set in MTLMeshRenderPipelineDescriptor.
 */
@property (readonly) NSUInteger maxTotalThreadsPerObjectThreadgroup API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @property maxTotalThreadsPerMeshThreadgroup
 @abstract The maximum total number of threads that can be in a single mesh shader threadgroup.
 @discussion This value is set in MTLMeshRenderPipelineDescriptor.
 */
@property (readonly) NSUInteger maxTotalThreadsPerMeshThreadgroup API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @property objectThreadExecutionWidth
 @abstract The number of threads in a SIMD group of the object shader.
 @discussion This value is also available in the shader with the [[threads_per_simdgroup]] attribute.
 */
@property (readonly) NSUInteger objectThreadExecutionWidth API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @property meshThreadExecutionWidth
 @abstract The number of threads in a SIMD group of the mesh shader.
 @discussion This value is also available in the shader with the [[threads_per_simdgroup]] attribute.
 */
@property (readonly) NSUInteger meshThreadExecutionWidth API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @property maxTotalThreadgroupsPerMeshGrid
 @abstract The maximum total number of threadgroups that can be in a single mesh shader grid.
 @discussion This value is set in MTLMeshRenderPipelineDescriptor.
 */
@property (readonly) NSUInteger maxTotalThreadgroupsPerMeshGrid API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method functionHandleWithFunction:stage:
 @brief Gets the function handle for the specified function on the specified stage of the pipeline.
 */
- (nullable id<MTLFunctionHandle>)functionHandleWithFunction:(id<MTLFunction>)function stage:(MTLRenderStages)stage API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method newVisibleFunctionTableWithDescriptor:stage:
 @brief Allocate a visible function table for the specified stage of the pipeline with the provided descriptor.
 */
- (nullable id<MTLVisibleFunctionTable>)newVisibleFunctionTableWithDescriptor:(MTLVisibleFunctionTableDescriptor * __nonnull)descriptor stage:(MTLRenderStages)stage API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method newIntersectionFunctionTableWithDescriptor:stage:
 @brief Allocate an intersection function table for the specified stage of the pipeline with the provided descriptor.
 */
- (nullable id <MTLIntersectionFunctionTable>)newIntersectionFunctionTableWithDescriptor:(MTLIntersectionFunctionTableDescriptor * _Nonnull)descriptor stage:(MTLRenderStages)stage API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method newRenderPipelineStateWithAdditionalBinaryFunctions:error:
 @brief Allocate a new render pipeline state by adding binary functions for each stage of this pipeline state.
 */
- (nullable id <MTLRenderPipelineState>)newRenderPipelineStateWithAdditionalBinaryFunctions:(nonnull MTLRenderPipelineFunctionsDescriptor *)additionalBinaryFunctions error:(__autoreleasing NSError **)error API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @property shaderValidation
 @abstract Current state of Shader Validation for the pipeline.
 */
@property (readonly, nonatomic) MTLShaderValidation shaderValidation API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @property requiredThreadsPerTileThreadgroup
 @abstract The required size of every tile shader threadgroup.
*/
@property (readonly) MTLSize requiredThreadsPerTileThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));

/*!
 @property requiredThreadsPerObjectThreadgroup
 @abstract The required size of every object shader threadgroup.
 @discussion This value is set in MTLMeshRenderPipelineDescriptor.
*/
@property (readonly) MTLSize requiredThreadsPerObjectThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));

/*!
 @property requiredThreadsPerMeshThreadgroup
 @abstract The required size of every mesh shader threadgroup.
 @discussion This value is set in MTLMeshRenderPipelineDescriptor.
*/
@property (readonly) MTLSize requiredThreadsPerMeshThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));

@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLRenderPipelineColorAttachmentDescriptorArray : NSObject

/* Individual attachment state access */
- (MTLRenderPipelineColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;

/* This always uses 'copy' semantics.  It is safe to set the attachment state at any legal index to nil, which resets that attachment descriptor state to default values. */
- (void)setObject:(nullable MTLRenderPipelineColorAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

@end


MTL_EXPORT API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
@interface MTLTileRenderPipelineColorAttachmentDescriptor : NSObject <NSCopying>

/*! Pixel format.  Defaults to MTLPixelFormatInvalid */
@property (nonatomic) MTLPixelFormat pixelFormat;

@end

MTL_EXPORT API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
@interface MTLTileRenderPipelineColorAttachmentDescriptorArray : NSObject

/* Individual tile attachment state access */
- (MTLTileRenderPipelineColorAttachmentDescriptor*)objectAtIndexedSubscript:(NSUInteger)attachmentIndex;

/* This always uses 'copy' semantics.  It is safe to set the attachment state at any legal index to nil, which resets that attachment descriptor state to default values. */
- (void)setObject:(MTLTileRenderPipelineColorAttachmentDescriptor*)attachment atIndexedSubscript:(NSUInteger)attachmentIndex;

@end

MTL_EXPORT API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5))
@interface MTLTileRenderPipelineDescriptor : NSObject <NSCopying>

/*!
 @property label:
 @abstract The descriptor label.
 */
@property (copy, nonatomic, nullable) NSString *label;

/*!
 @property tileFunction:
 @abstract  The kernel or fragment function that serves as the tile shader for this pipeline.
 @discussion Both kernel-based and fragment-based tile pipelines dispatches will barrier against previous
 draws and other dispatches. Kernel-based pipelines will wait until all prior access to the tile completes.
 Fragment-based pipelines will only wait until all prior access to the fragment's location completes.
 */
@property (readwrite, nonatomic, strong) id <MTLFunction> tileFunction;

/* Rasterization and visibility state */
@property (readwrite, nonatomic) NSUInteger rasterSampleCount;
@property (readonly) MTLTileRenderPipelineColorAttachmentDescriptorArray *colorAttachments;

/*!
 @property threadgroupSizeMatchesTileSize:
 @abstract Whether all threadgroups associated with this pipeline will cover tiles entirely.
 @discussion Metal can optimize code generation for this case.
 */
@property (readwrite, nonatomic) BOOL threadgroupSizeMatchesTileSize;

@property (readonly) MTLPipelineBufferDescriptorArray *tileBuffers API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0));

/*!
 @property maxTotalThreadsPerThreadgroup
 @abstract Optional property. Set the maxTotalThreadsPerThreadgroup. If it is not set, returns zero.
 */
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerThreadgroup API_AVAILABLE(macos(10.15), ios(12.0));

/*!
 @property binaryArchives
 @abstract The set of MTLBinaryArchive to search for compiled code when creating the pipeline state.
 @discussion Accelerate pipeline state creation by providing archives of compiled code such that no compilation needs to happen on the fast path.
 @see MTLBinaryArchive
 */
@property (readwrite, nullable, nonatomic, copy) NSArray<id<MTLBinaryArchive>> *binaryArchives API_AVAILABLE(macos(11.0), ios(14.0));


/*!
 @property preloadedLibraries
 @abstract The set of MTLDynamicLibrary to use to resolve external symbols before considering symbols from dependent MTLDynamicLibrary.
 @discussion Typical workflows use the libraries property of MTLCompileOptions to record dependent libraries at compile time without having to use preloadedLibraries.
 This property can be used to override symbols from dependent libraries for experimentation or evaluating alternative implementations.
 It can also be used to provide dynamic libraries that are dynamically created (for example, from source) that have no stable installName that can be used to automatically load from the file system.
 @see MTLDynamicLibrary
 */
@property (readwrite, nonnull, nonatomic, copy) NSArray<id<MTLDynamicLibrary>>* preloadedLibraries API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @property linkedFunctions
 @abstract The set of functions to be linked with the pipeline state and accessed from the tile function.
 @see MTLLinkedFunctions
 */
@property (null_resettable, copy, nonatomic) MTLLinkedFunctions *linkedFunctions
    API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @property supportAddingBinaryFunctions
 @abstract This flag makes this pipeline support creating a new pipeline by adding binary functions.
 */
@property (readwrite, nonatomic) BOOL supportAddingBinaryFunctions
API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @property maxCallStackDepth
 @abstract The maximum depth of the call stack in stack frames from the tile function. Defaults to 1 additional stack frame.
 */
@property (readwrite, nonatomic) NSUInteger maxCallStackDepth
API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));


- (void)reset;

/*!
 @property shaderValidation
 @abstract Toggle that determines whether Metal Shader Validation should be enabled or disabled for the pipeline.
 @discussion The value can be overridden using `MTL_SHADER_VALIDATION_ENABLE_PIPELINES` or `MTL_SHADER_VALIDATION_DISABLE_PIPELINES` Environment Variables.
 */
@property (readwrite, nonatomic) MTLShaderValidation shaderValidation API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @property requiredThreadsPerThreadgroup
 @abstract Sets the required threads-per-threadgroup during tile dispatches. The `threadsPerTile` argument of any tile dispatch must match to this value if it is set.
           Setting this to a size of 0 in every dimension disables this property
*/
@property(readwrite, nonatomic) MTLSize requiredThreadsPerThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));

@end


/*!
 @class MTLMeshRenderPipelineDescriptor
 @discussion As an alternative to a vertex + fragment shader render pipeline, this render pipeline uses a (object +) mesh + fragment shader for rendering geometry.
 */
MTL_EXPORT API_AVAILABLE(macos(13.0), ios(16.0))
@interface MTLMeshRenderPipelineDescriptor : NSObject <NSCopying>

/*!
 @property label
 @discussion A name or description provided by the application that will be displayed in debugging tools.
 The default value is nil.
 */
@property (nullable, copy, nonatomic) NSString *label;

/*!
 @property objectFunction
 @discussion Optional shader function responsible for determining how many threadgroups of the mesh shader to run, can optionally provide payload data for the mesh stage.
 If this is nil, no payload data is available to the mesh function, and the draw command determines how many threadgroups of the mesh stage to run.
 The default value is nil.
 */
@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> objectFunction;

/*!
 @property meshFunction
 @discussion Shader function responsible for exporting a chunk of geometry per threadgroup for the rasterizer.
 The default value is nil.
 */
@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> meshFunction;

/*!
 @property fragmentFunction
 @discussion Like a classical render pipeline, this fragments covered by the rasterized geometry are shaded with this function.
 The default value is nil. To create a pipeline, you must either set fragmentFunction to non-nil, or set rasterizationEnabled to NO.
 */
@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> fragmentFunction;

/*!
 @property maxTotalThreadsPerObjectThreadgroup
 @discussion The maximum size of the product of threadsPerObjectThreadgroup that can be used for draws with this pipeline.
 This information can be used by the optimizer to generate more efficient code, specifically when the specified value does not exceed the thread execution width of the underlying GPU.
 The default value is 0, which means that the value specified with the [[max_total_threads_per_threadgroup(N)]] specified on objectFunction will be used.
 When both the [[max_total_threads_per_threadgroup(N)]] attribute and a non-zero value are specified, both values must match.
 Any value specified cannot exceed the device limit as documented in the "Metal Feature Set Tables" for "Maximum threads per threadgroup".
 */
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerObjectThreadgroup;

/*!
 @property maxTotalThreadsPerMeshThreadgroup
 @discussion The maximum size of the product of threadsPerMeshThreadgroup that can be used for draws with this pipeline.
 This information can be used by the optimizer to generate more efficient code, specifically when the specified value does not exceed the thread execution width of the underlying GPU.
 The default value is 0, which means that the value specified with the [[max_total_threads_per_threadgroup(N)]] specified on meshFunction will be used.
 When both the [[max_total_threads_per_threadgroup(N)]] attribute and a non-zero value are specified, both values must match.
 Any value specified cannot exceed the device limit as documented in the "Metal Feature Set Tables" for "Maximum threads per threadgroup".
 */
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerMeshThreadgroup;

/*!
 @property objectThreadgroupSizeIsMultipleOfThreadExecutionWidth
 @discussion Set this value to YES when you will only use draws with the product of threadsPerObjectThreadgroup set to a multiple of the objectThreadExecutionWidth of the returned pipeline state.
 This information can be used by the optimizer to generate more efficient code.
 The default value is NO.
 */
@property (readwrite, nonatomic) BOOL objectThreadgroupSizeIsMultipleOfThreadExecutionWidth;

/*!
 @property meshThreadgroupSizeIsMultipleOfThreadExecutionWidth
 @discussion Set this value to YES when you will only use draws with the product of threadsPerMeshThreadgroup set to a multiple of the meshThreadExecutionWidth of the returned pipeline state.
 This information can be used by the optimizer to generate more efficient code.
 The default value is NO.
 */
@property (readwrite, nonatomic) BOOL meshThreadgroupSizeIsMultipleOfThreadExecutionWidth;

/*!
 @property payloadMemoryLength
 @discussion The size, in bytes, of the buffer indicated by [[payload]] in the object and mesh shader.
 If this value is 0, the size of the dereferenced type declared in the object shader for the buffer is used (space for a single element is assumed for pointers).
 The default value is 0.
 */
@property (readwrite, nonatomic) NSUInteger payloadMemoryLength;

/*!
 @property maxTotalThreadgroupsPerMeshGrid
 @discussion The maximum value of the product of vector elements that the object shader may pass to the mesh_grid_properties::set_threadgroups_per_grid built-in function.
 The default value is 0, which means that the value specified with the [[max_total_threadgroups_per_mesh_grid(N)]] specified on objectFunction will be used.
 When both the [[max_total_threadgroups_per_mesh_grid(N)]] attribute and a non-zero value are specified, both values must match.
 Any value specified cannot exceed the device limit as documented in the "Metal Feature Set Tables" for "Maximum threadgroups per mesh grid".
 Specifying this value is optional; it may be used to improve scheduling of the workload. If neither this value nor the shader attribute are used, the device's maximum supported value is used instead.
 */
@property (readwrite, nonatomic) NSUInteger maxTotalThreadgroupsPerMeshGrid;

/*!
 @property objectBuffers
 @abstract Provide mutability information on the buffers used by objectFunction.
 @discussion Specifying these values is optional; it may be used to optimize the shader code.
 */
@property (readonly) MTLPipelineBufferDescriptorArray *objectBuffers;

/*!
 @property meshBuffers
 @abstract Provide mutability information on the buffers used by meshFunction.
 @discussion Specifying these values is optional; it may be used to optimize the shader code.
 */
@property (readonly) MTLPipelineBufferDescriptorArray *meshBuffers;

/*!
 @property fragmentBuffers
 @abstract Provide mutability information on the buffers used by fragmentFunction.
 @discussion Specifying these values is optional; it may be used to optimize the shader code.
 */
@property (readonly) MTLPipelineBufferDescriptorArray *fragmentBuffers;

/*!
 @property rasterSampleCount
 @discussion The number of samples per fragment of the render pass in which this pipeline will be used.
 */
@property (readwrite, nonatomic) NSUInteger rasterSampleCount;

/*!
 @property alphaToCoverageEnabled
 @abstract Whether the alpha value exported by the fragment shader for the first color attachment is converted to a sample mask, which is subsequently AND-ed with the fragments' sample mask
 @discussion The default value is NO.
 */
@property (readwrite, nonatomic, getter = isAlphaToCoverageEnabled) BOOL alphaToCoverageEnabled;

/*!
 @property alphaToOneEnabled
 @abstract Whether the alpha value exported by the fragment shader for all color attachments is modified to 1 (after evaluating alphaToCoverage).
 @discussion The default value is NO.
 */
@property (readwrite, nonatomic, getter = isAlphaToOneEnabled) BOOL alphaToOneEnabled;

/*!
 @property rasterizationEnabled
 @abstract Whether rasterization is disabled, all primitives are dropped prior to rasterization.
 @discussion The default value is YES.
 */
@property (readwrite, nonatomic, getter = isRasterizationEnabled) BOOL rasterizationEnabled;

/*!
 @property maxVertexAmplificationCount
 @abstract The maximum value that can be passed to setVertexAmplificationCount when using this pipeline.
 @discussion The default value is 1. The value must be supported by the device, which can be checked with supportsVertexAmplificationCount.
 */
@property (readwrite, nonatomic) NSUInteger maxVertexAmplificationCount;

/*!
 @property colorAttachments
 @abstract Describes the color attachments of the render pass in which this pipeline will be used.
 */
@property (readonly) MTLRenderPipelineColorAttachmentDescriptorArray *colorAttachments;

/*!
 @property depthAttachmentPixelFormat
 @abstract The pixel format of the depth attachment of the render pass in which this pipeline will be used.
 @discussion The default value is MTLPixelFormatInvalid; indicating no depth attachment will be used.
 */
@property (nonatomic) MTLPixelFormat depthAttachmentPixelFormat;

/*!
 @property stencilAttachmentPixelFormat
 @abstract The pixel format of the stencil attachment of the render pass in which this pipeline will be used.
 @discussion The default value is MTLPixelFormatInvalid; indicating no stencil attachment will be used.
 */
@property (nonatomic) MTLPixelFormat stencilAttachmentPixelFormat;

/*!
 @property supportIndirectCommandBuffers
 @abstract Whether this pipeline will support being used by commands in an indirect command buffer.
 @discussion The default value is NO.
 */
@property (readwrite, nonatomic) BOOL supportIndirectCommandBuffers API_AVAILABLE(macos(14.0), ios(17.0), tvos(18.1), visionos(2.1));

/*!
 @property binaryArchives
 @abstract The set of MTLBinaryArchive to search for compiled code when creating the pipeline state.
 @discussion Accelerate pipeline state creation by providing archives of compiled code such that no compilation needs to happen on the fast path.
 @see MTLBinaryArchive
 */
@property (readwrite, nullable, nonatomic, copy) NSArray<id<MTLBinaryArchive>> *binaryArchives API_AVAILABLE(macos(15.0), ios(18.0));


/*!
 @property objectLinkedFunctions
 @abstract The set of functions to be linked with the pipeline state and accessed from the object function.
 @see MTLLinkedFunctions
 */
@property (null_resettable, copy, nonatomic) MTLLinkedFunctions *objectLinkedFunctions API_AVAILABLE(macos(14.0), ios(17.0));

/*!
 @property meshLinkedFunctions
 @abstract The set of functions to be linked with the pipeline state and accessed from the mesh function.
 @see MTLLinkedFunctions
 */
@property (null_resettable, copy, nonatomic) MTLLinkedFunctions *meshLinkedFunctions API_AVAILABLE(macos(14.0), ios(17.0));

/*!
 @property fragmentLinkedFunctions
 @abstract The set of functions to be linked with the pipeline state and accessed from the fragment function.
 @see MTLLinkedFunctions
 */
@property (null_resettable, copy, nonatomic) MTLLinkedFunctions *fragmentLinkedFunctions API_AVAILABLE(macos(14.0), ios(17.0));

/*!
 @method reset
 @abstract Restore all mesh pipeline descriptor properties to their default values.
 */
- (void)reset;

/*!
 @property shaderValidation
 @abstract Toggle that determines whether Metal Shader Validation should be enabled or disabled for the pipeline.
 @discussion The value can be overridden using `MTL_SHADER_VALIDATION_ENABLE_PIPELINES` or `MTL_SHADER_VALIDATION_DISABLE_PIPELINES` Environment Variables.
 */
@property (readwrite, nonatomic) MTLShaderValidation shaderValidation API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @property requiredThreadsPerObjectThreadgroup
 @abstract Sets the required object threads-per-threadgroup during mesh draws. The `threadsPerObjectThreadgroup` argument of any draw must match to this value if it is set.
           Setting this to a size of 0 in every dimension disables this property
*/
@property (readwrite, nonatomic) MTLSize requiredThreadsPerObjectThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));

/*!
 @property requiredThreadsPerMeshThreadgroup
 @abstract Sets the required mesh threads-per-threadgroup during mesh draws. The `threadsPerMeshThreadgroup` argument of any draw must match to this value if it is set.
           Setting this to a size of 0 in every dimension disables this property
*/
@property (readwrite, nonatomic) MTLSize requiredThreadsPerMeshThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));

@end


NS_ASSUME_NONNULL_END
