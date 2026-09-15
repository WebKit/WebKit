//
//  MTLComputePipeline.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLStageInputOutputDescriptor.h>
#import <Metal/MTLPipeline.h>
#import <Metal/MTLAllocation.h>

#import <Metal/MTLLinkedFunctions.h>
@protocol MTLFunctionHandle;

@protocol MTLDynamicLibrary;

NS_ASSUME_NONNULL_BEGIN

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@interface MTLComputePipelineReflection : NSObject

@property (nonnull, readonly) NSArray<id<MTLBinding>> *bindings API_AVAILABLE(macos(13.0), ios(16.0));
@property (readonly) NSArray <MTLArgument *> *arguments API_DEPRECATED_WITH_REPLACEMENT("bindings", macos(10.11, 13.0), ios(8.0, 16.0));

@end

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(9.0))
@interface MTLComputePipelineDescriptor : NSObject <NSCopying>

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, copy, nonatomic) NSString *label;

/*!
 @property computeFunction
 @abstract The function to use with the MTLComputePipelineState
 */
@property (nullable, readwrite, nonatomic, strong) id <MTLFunction> computeFunction;

/*!
 @property threadGroupSizeIsMultipleOfThreadExecutionWidth
 @abstract An optimization flag, set if the thread group size will always be a multiple of thread execution width
 */
@property (readwrite, nonatomic) BOOL threadGroupSizeIsMultipleOfThreadExecutionWidth;

/*!
 @property maxTotalThreadsPerThreadgroup
 @abstract Optional property. Set the maxTotalThreadsPerThreadgroup. If it is not set, returns zero.
 */
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerThreadgroup API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @property computeDataDescriptor
 @abstract An MTLStageInputOutputDescriptor to fetch data from buffers
 */
@property (nullable, copy, nonatomic) MTLStageInputOutputDescriptor *stageInputDescriptor API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @property buffers
 @abstract Optional properties for each buffer binding used by the compute function.
 */
@property (readonly) MTLPipelineBufferDescriptorArray *buffers API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @property supportIndirectCommandBuffers
 @abstract This flag makes this pipeline usable with indirect command buffers.
 */
@property (readwrite, nonatomic) BOOL supportIndirectCommandBuffers API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0));

/*!
 @property insertLibraries
 @abstract The set of MTLDynamicLibrary to use to resolve external symbols before considering symbols from dependent MTLDynamicLibrary.
 @discussion Typical workflows use the libraries property of MTLCompileOptions to record dependent libraries at compile time without having to use insertLibraries.
 This property can be used to override symbols from dependent libraries for experimentation or evaluating alternative implementations.
 It can also be used to provide dynamic libraries that are dynamically created (for example, from source) that have no stable installName that can be used to automatically load from the file system.
 @see MTLDynamicLibrary
 */
@property (readwrite, nullable, nonatomic, copy) NSArray<id<MTLDynamicLibrary>>* insertLibraries API_DEPRECATED_WITH_REPLACEMENT("preloadedLibraries", macos(11.0, 12.0), ios(14.0, 15.0));

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
 @property binaryArchives
 @abstract The set of MTLBinaryArchive to search for compiled code when creating the pipeline state.
 @discussion Accelerate pipeline state creation by providing archives of compiled code such that no compilation needs to happen on the fast path.
 @see MTLBinaryArchive
 */
@property (readwrite, nullable, nonatomic, copy) NSArray<id<MTLBinaryArchive>> *binaryArchives API_AVAILABLE(macos(11.0), ios(14.0));



/*!
 @method reset
 @abstract Restore all compute pipeline descriptor properties to their default values.
 */
- (void)reset;


/*!
 @property linkedFunctions
 @abstract The set of functions to be linked with the pipeline state and accessed from the compute function. 
 @see MTLLinkedFunctions
 */
@property (nullable, copy, nonatomic) MTLLinkedFunctions *linkedFunctions
    API_AVAILABLE(macos(11.0), ios(14.0));



/*!
 @property supportAddingBinaryFunctions
 @abstract This flag makes this pipeline support creating a new pipeline by adding binary functions.
 */
@property (readwrite, nonatomic) BOOL supportAddingBinaryFunctions
    API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));


/*!
 @property maxCallStackDepth
 @abstract The maximum depth of the call stack in stack frames from the kernel. Defaults to 1 additional stack frame.
 */
@property (readwrite, nonatomic) NSUInteger maxCallStackDepth
    API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));



/*!
 @property shaderValidation
 @abstract Toggle that determines whether Metal Shader Validation should be enabled or disabled for the pipeline.
 @discussion The value can be overridden using `MTL_SHADER_VALIDATION_ENABLE_PIPELINES` or `MTL_SHADER_VALIDATION_DISABLE_PIPELINES` Environment Variables.
 */
@property (readwrite, nonatomic) MTLShaderValidation shaderValidation API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @property requiredThreadsPerThreadgroup
 @abstract Sets the required threads-per-threadgroup during dispatches. The `threadsPerThreadgroup` argument of any dispatch must match this value if it is set.
           Setting this to a size of 0 in every dimension disables this property
*/
@property(readwrite, nonatomic) MTLSize requiredThreadsPerThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));


@end

/*!
 @protocol MTLComputePipelineState
 @abstract A handle to compiled code for a compute function.
 @discussion MTLComputePipelineState is a single compute function.  It can only be used with the device that it was created against.
*/
API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@protocol MTLComputePipelineState <MTLAllocation, NSObject>

@property (nullable, readonly) NSString *label API_AVAILABLE(macos(10.13), ios(11.0));

/// The compute pipeline's reflection information, if available.
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
/// such as with its ``MTLDevice/newComputePipelineStateWithDescriptor:options:completionHandler:`` method.
@property (nullable, readonly) MTLComputePipelineReflection* reflection API_AVAILABLE(macos(26.0), ios(26.0));

/// Gets the function handle for a function this pipeline links at the Metal IR level by name.
///
/// - Parameters:
///   - name: A string representing the name of the function.
///
/// - Returns: A function handle corresponding to the function if the name matches a function in this pipeline state,
/// otherwise `nil`.
- (nullable id<MTLFunctionHandle>)functionHandleWithName:(NSString*)name API_AVAILABLE(macos(26.0), ios(26.0));

/// Gets the function handle for a function this pipeline links at the binary level.
///
/// - Parameters:
///   - function: A binary function object representing the function binary to find.
///
/// - Returns: A function handle corresponding to the function if the binary function matches a function in this
///            pipeline state, otherwise `nil`.
- (nullable id<MTLFunctionHandle>)functionHandleWithBinaryFunction:(id<MTL4BinaryFunction>)function API_AVAILABLE(macos(26.0), ios(26.0));

/// Allocates a new compute pipeline state by adding binary functions to this pipeline state.
///
/// - Parameters:
///   - additionalBinaryFunctions: A non-`nil` array containing binary functions to add to this pipeline.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: A new compute pipeline state upon success, otherwise `nil`.
- (nullable id<MTLComputePipelineState>)newComputePipelineStateWithBinaryFunctions:(NSArray<id<MTL4BinaryFunction>>*)additionalBinaryFunctions
                                                                             error:(NSError**)error API_AVAILABLE(macos(26.0), ios(26.0));

/*!
 @property device
 @abstract The device this resource was created against.  This resource can only be used with this device.
 */
@property (readonly) id <MTLDevice> device;

/*!
 @property maxTotalThreadsPerThreadgroup
 @abstract The maximum total number of threads that can be in a single compute threadgroup.
 */
@property (readonly) NSUInteger maxTotalThreadsPerThreadgroup;

/*!
 @property threadExecutionWidth
 @abstract For most efficient execution, the threadgroup size should be a multiple of this when executing the kernel.
 */
@property (readonly) NSUInteger threadExecutionWidth;


/*!
 @property staticThreadgroupMemoryLength
 @abstract The length in bytes of threadgroup memory that is statically allocated.
 */
@property (readonly) NSUInteger staticThreadgroupMemoryLength API_AVAILABLE(macos(10.13), ios(11.0));


/*!
 @method imageblockMemoryLengthForDimensions:
 @brief Returns imageblock memory length for given image block dimensions.
 */
- (NSUInteger)imageblockMemoryLengthForDimensions:(MTLSize)imageblockDimensions API_AVAILABLE(ios(11.0), macos(11.0), macCatalyst(14.0), tvos(14.5));


/*!
 @property supportIndirectCommandBuffers
 @abstract Tells whether this pipeline state is usable through an Indirect Command Buffer.
 */
@property (readonly) BOOL supportIndirectCommandBuffers API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0));

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID API_AVAILABLE(macos(13.0), ios(16.0));


/*!
 @method functionHandleWithFunction:
 @brief Get the function handle for the specified function from the pipeline state.
 */
- (nullable id<MTLFunctionHandle>)functionHandleWithFunction:(id<MTLFunction>)function API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));


/*!
 @method newRenderPipelineStateWithAdditionalBinaryFunctions:stage:
 @brief Allocate a new compute pipeline state by adding binary functions to this pipeline state.
 */
- (nullable id <MTLComputePipelineState>)newComputePipelineStateWithAdditionalBinaryFunctions:(nonnull NSArray<id<MTLFunction>> *)functions error:(__autoreleasing NSError **)error API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 @method newVisibleFunctionTableWithDescriptor:
 @brief Allocate a visible function table for the pipeline with the provided descriptor.
 */
- (nullable id<MTLVisibleFunctionTable>)newVisibleFunctionTableWithDescriptor:(MTLVisibleFunctionTableDescriptor * __nonnull)descriptor API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 @method newIntersectionFunctionTableWithDescriptor:
 @brief Allocate an intersection function table for the pipeline with the provided descriptor.
 */
- (nullable id <MTLIntersectionFunctionTable>)newIntersectionFunctionTableWithDescriptor:(MTLIntersectionFunctionTableDescriptor * _Nonnull)descriptor API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));


/*!
 @property shaderValidation
 @abstract Current state of Shader Validation for the pipeline.
 */
@property (readonly, nonatomic) MTLShaderValidation shaderValidation API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @property requiredThreadsPerThreadgroup
 @abstract The required size of every compute threadgroup.
*/
@property (readonly) MTLSize requiredThreadsPerThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));


@end

NS_ASSUME_NONNULL_END
