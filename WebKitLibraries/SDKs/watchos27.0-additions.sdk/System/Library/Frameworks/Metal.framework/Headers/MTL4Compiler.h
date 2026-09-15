//
//  MTL4Compiler.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4Compiler_h
#define MTL4Compiler_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTL4CompilerTask.h>
#import <Metal/MTLLibrary.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLDynamicLibrary;
@class MTL4LibraryDescriptor;
@class MTL4FunctionDescriptor;
@class MTL4RenderPipelineDescriptor;
@class MTL4ComputePipelineDescriptor;
@class MTL4PipelineDescriptor;
@protocol MTL4BinaryFunction;
@protocol MTLComputePipelineState;
@protocol MTLRenderPipelineState;

@class MTL4MachineLearningPipelineDescriptor;
@protocol MTL4MachineLearningPipelineState;

@protocol MTL4Archive;
@protocol MTL4PipelineDataSetSerializer;

@class MTL4BinaryFunctionDescriptor;


@class MTL4PipelineStageDynamicLinkingDescriptor;
@class MTL4RenderPipelineDynamicLinkingDescriptor;

/// Groups together properties for creating a compiler context.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4CompilerDescriptor : NSObject <NSCopying>

/// Assigns an optional descriptor label to the compiler for debugging purposes.
@property (copy, nonatomic, nullable) NSString *label;

/// Assigns a pipeline data set serializer into which this compiler stores data for all pipelines it creates.
@property (nullable, strong) id<MTL4PipelineDataSetSerializer> pipelineDataSetSerializer;


@end

/// The configuration options that control the behavior of a compilation task for a Metal 4 compiler instance.
///
/// You can configure task-specific settings that affect a compilation task by
/// creating an instance of this class, setting its properties,
/// and passing it to one of the applicable methods of an ``MTL4Compiler`` instance.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4CompilerTaskOptions : NSObject <NSCopying>

/// An array of archive instances that can potentially accelerate a compilation task.
///
/// The compiler can reduce the runtime of a compilation task if it finds an entry
/// that matches a function description within any of the archives in this array.
/// The compiler searches the archives in the order of the array's element.
///
/// Consider adding archives to the array in scenarios that can benefit from the runtime savings,
/// such as repeat builds or when your app can share compilation results across multiple contexts.
///
/// - Important: Only add ``MTL4Archive`` instances to the array that are compatible with the Metal device.
@property (nullable, copy, nonatomic) NSArray<id<MTL4Archive>>* lookupArchives;

@end

/// Provides a signature for a callback block that Metal calls when the compiler finishes a build task for a binary function.
typedef void (^ NS_SWIFT_SENDABLE MTL4NewBinaryFunctionCompletionHandler)(id<MTL4BinaryFunction> __nullable function, NSError* __nullable error)
    API_AVAILABLE(macos(26.0), ios(26.0));


/// Provides a signature for a callback block that Metal calls when the compiler finishes a build task for a machine learning pipeline state.
typedef void (^ NS_SWIFT_SENDABLE MTL4NewMachineLearningPipelineStateCompletionHandler)(id<MTL4MachineLearningPipelineState> __nullable mlPipelineState, NSError* __nullable error)
API_AVAILABLE(macos(26.0), ios(26.0));

/// A abstraction for a pipeline state and shader function compiler.
API_AVAILABLE(macos(26.0), ios(26.0)) NS_SWIFT_SENDABLE
@protocol MTL4Compiler <NSObject>
/// Returns the device that this compiler belongs to.
@property (readonly) id<MTLDevice> device;

/// Returns the optional label you specify at creation time.
@property (readonly, nullable) NSString* label;

/// Returns the pipeline data set serializer into which this compiler stores data for all pipelines it creates.
@property (readonly, strong, nullable) id<MTL4PipelineDataSetSerializer> pipelineDataSetSerializer;


/// Creates a new Metal library synchronously.
///
/// - Parameters:
///   - descriptor: A description of the library to create.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: a Metal library instance upon success, `nil` otherwise.
- (nullable id<MTLLibrary>)newLibraryWithDescriptor:(MTL4LibraryDescriptor*)descriptor error:(NSError**)error;

/// Creates a new dynamic library from a library containing Metal IR code synchronously.
///
/// - Parameters:
///   - library: A library from which this compiler creates the new a dynamic library
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: A new dynamic Metal library upon success, `nil` otherwise.
- (nullable id<MTLDynamicLibrary>)newDynamicLibrary:(id<MTLLibrary>)library error:(NSError**)error;

/// Creates a new dynamic library from the contents of a file at an URL location synchronously.
///
/// - Parameters:
///   - url: An URL referencing a file whose contents this compiler uses to build a dynamic library.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: A new dynamic Metal library upon success, `nil` otherwise.
- (nullable id<MTLDynamicLibrary>)newDynamicLibraryWithURL:(NSURL*)url error:(NSError**)error;

/// Creates a new compute pipeline state object synchronously.
///
/// - Parameters:
///   - descriptor: A compute pipeline state descriptor describing the pipeline this compiler creates.
///   - compilerTaskOptions: A description of the compilation process itself, providing parameters that
///         influence execution of the compilation process.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: A new compute pipeline state object upon success, `nil` otherwise.
- (nullable id<MTLComputePipelineState>)newComputePipelineStateWithDescriptor:(MTL4ComputePipelineDescriptor*)descriptor
                                                          compilerTaskOptions:(nullable MTL4CompilerTaskOptions*)compilerTaskOptions
                                                                        error:(NSError**)error;

/// Creates a new compute pipeline state synchronously.
///
/// - Parameters:
///   - descriptor: A compute pipeline state descriptor describing the pipeline this compiler creates.
///   - dynamicLinkingDescriptor: An optional parameter that provides additional configuration for linking the pipeline state object.
///   - compilerTaskOptions: A description of the compilation process itself, providing parameters that
///         influence execution of the compilation process.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: A new compute pipeline state object upon success, `nil` otherwise.
- (nullable id<MTLComputePipelineState>)newComputePipelineStateWithDescriptor:(MTL4ComputePipelineDescriptor *)descriptor
                                                     dynamicLinkingDescriptor:(nullable MTL4PipelineStageDynamicLinkingDescriptor*)dynamicLinkingDescriptor
                                                          compilerTaskOptions:(nullable MTL4CompilerTaskOptions *)compilerTaskOptions
                                                                        error:(NSError **)error;

/// Creates a new render pipeline state synchronously.
///
/// Use this method to build any render pipeline type, including render, tile, and mesh render pipeline states.
/// The type of the descriptor you pass indicates the pipeline type this method builds.
///
/// Passing in a compute pipeline descriptor to the `descriptor` parameter produces an error.
///
/// - Parameters:
///   - descriptor: A render, tile, or mesh pipeline state descriptor that describes the pipeline to create.
///   - compilerTaskOptions: A description of the compilation process itself, providing parameters that
///         influence execution of the compilation process.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: A new render pipeline state object upon success, `nil` otherwise.
- (nullable id<MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTL4PipelineDescriptor*)descriptor
                                                        compilerTaskOptions:(nullable MTL4CompilerTaskOptions*)compilerTaskOptions
                                                                      error:(NSError**)error;

/// Creates a new render pipeline state synchronously.
///
/// Use this method to build any render pipeline type, including render, tile, and mesh render pipeline states.
/// The type of the descriptor you pass indicates the pipeline type this method builds.
///
/// Passing in a compute pipeline descriptor to the `descriptor` parameter produces an error.
///
/// - Parameters:
///   - descriptor: A render, tile, or mesh pipeline state descriptor that describes the pipeline to create.
///   - dynamicLinkingDescriptor: An optional parameter that provides additional configuration for linking the pipeline state object.
///   - compilerTaskOptions: A description of the compilation process itself, providing parameters that
///         influence execution of the compilation process.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: A new render pipeline state object upon success, `nil` otherwise.

- (nullable id<MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTL4PipelineDescriptor *)descriptor
                                                   dynamicLinkingDescriptor:(nullable MTL4RenderPipelineDynamicLinkingDescriptor*)dynamicLinkingDescriptor
                                                        compilerTaskOptions:(nullable MTL4CompilerTaskOptions *)compilerTaskOptions
                                                                      error:(NSError **)error;

/// Creates a new render pipeline state from another, previously unspecialized, pipeline state.
///
/// Metal specializes the pipeline state with new state values the descriptor provides, observing the following rules:
/// * The compiler only updates properties that were originally specified as *unspecialized*. It doesn't modify other
/// already-specialized properties
/// * The compiler sets to their default behavior any unspecialized properties that your passed-in descriptor doesn't specialize
///
/// Additionally, there are some cases where the Metal can't specialize a pipeline:
/// * If the original pipeline state object doesn't have any unspecialized properties
/// * You can't re-specialize a previously specialized pipeline state object
///
/// - Parameters:
///   - descriptor: A render pipeline state descriptor or any type: default, tile, or mesh render pipeline descriptor.
///   - pipeline: A render pipeline state containing unspecialized substate.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: a fully-specialized pipeline state object.
///
- (nullable id<MTLRenderPipelineState>)newRenderPipelineStateBySpecializationWithDescriptor:(MTL4PipelineDescriptor*)descriptor
                                                                                   pipeline:(id<MTLRenderPipelineState>)pipeline
                                                                                      error:(NSError**)error
    API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a new binary visible or intersection function synchronously.
///
/// - Parameters:
///   - descriptor: A binary function descriptor to use for creating the binary function.
///   - compilerTaskOptions: A descriptor of the compilation itself, providing parameters that
///         influence execution of the compilation process.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: a new binary function upon success, `nil` otherwise.
- (nullable id<MTL4BinaryFunction>) newBinaryFunctionWithDescriptor:(MTL4BinaryFunctionDescriptor *)descriptor
                                                compilerTaskOptions:(nullable MTL4CompilerTaskOptions*)compilerTaskOptions
                                                              error:(NSError**)error;

/// Creates a new Metal library instance asynchronously.
///
/// - Parameters:
///   - descriptor: A description of the library to create.
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: a compiler task representing the asynchronous compilation task.
- (id<MTL4CompilerTask>)newLibraryWithDescriptor:(MTL4LibraryDescriptor*)descriptor
                               completionHandler:(MTLNewLibraryCompletionHandler)completionHandler;

/// Creates a new dynamic Metal library instance asynchronously.
///
/// - Parameters:
///   - library: A library from which this compiler creates the new a dynamic library
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: A compiler task representing the asynchronous compilation task.
- (id<MTL4CompilerTask>)newDynamicLibrary:(id<MTLLibrary>)library
                        completionHandler:(MTLNewDynamicLibraryCompletionHandler)completionHandler;

/// Creates a new dynamic library from the contents of a file at an URL location synchronously.
///
/// - Parameters:
///   - url: An URL referencing a file whose contents this compiler uses to build a dynamic library.
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: a compiler task representing the asynchronous compilation task.
- (id<MTL4CompilerTask>)newDynamicLibraryWithURL:(NSURL*)url
                               completionHandler:(MTLNewDynamicLibraryCompletionHandler)completionHandler;

/// Creates a new compute pipeline state asynchronously.
///
/// - Parameters:
///   - descriptor: A compute pipeline state descriptor, describing the compute pipeline to create.
///   - compilerTaskOptions: A descriptor of the compilation itself, providing parameters that
///         influence execution of the compilation process.
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: a compiler task representing the asynchronous compilation task.
- (id<MTL4CompilerTask>)newComputePipelineStateWithDescriptor:(MTL4ComputePipelineDescriptor*)descriptor
                                          compilerTaskOptions:(nullable MTL4CompilerTaskOptions*)compilerTaskOptions
                                            completionHandler:(MTLNewComputePipelineStateCompletionHandler)completionHandler;

/// Creates a new compute pipeline state asynchronously.
///
/// - Parameters:
///   - descriptor: A compute pipeline state descriptor, describing the compute pipeline to create.
///   - dynamicLinkingDescriptor: An optional parameter that provides additional configuration for linking the pipeline state object.
///   - compilerTaskOptions: A description of the compilation process itself, providing parameters that
///         influence execution of the compilation process.
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: a compiler task representing the asynchronous compilation task.
- (id<MTL4CompilerTask>)newComputePipelineStateWithDescriptor:(MTL4ComputePipelineDescriptor *)descriptor
                                     dynamicLinkingDescriptor:(nullable MTL4PipelineStageDynamicLinkingDescriptor*)dynamicLinkingDescriptor
                                          compilerTaskOptions:(nullable MTL4CompilerTaskOptions *)compilerTaskOptions
                                            completionHandler:(MTLNewComputePipelineStateCompletionHandler)completionHandler;

/// Creates a new render pipeline state asynchronously.
///
/// Use this method to build any render pipeline type, including render, tile, and mesh render pipeline states.
/// The type of the descriptor you pass indicates the pipeline type this method builds.
///
/// Passing in a compute pipeline descriptor to the `descriptor` parameter produces an error.
///
/// - Parameters:
///   - descriptor: A render, tile, or mesh pipeline state descriptor that describes the pipeline to create.
///   - compilerTaskOptions: A description of the compilation process itself, providing parameters that
///         influence execution of the compilation process.
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: a compiler task representing the asynchronous compilation task.
- (id<MTL4CompilerTask>)newRenderPipelineStateWithDescriptor:(MTL4PipelineDescriptor*)descriptor
                                         compilerTaskOptions:(nullable MTL4CompilerTaskOptions*)compilerTaskOptions
                                           completionHandler:(MTLNewRenderPipelineStateCompletionHandler)completionHandler;

/// Creates a new render pipeline state asynchronously.
///
/// Use this method to build any render pipeline type, including render, tile, and mesh render pipeline states.
/// The type of the descriptor you pass indicates the pipeline type this method builds.
///
/// Passing in a compute pipeline descriptor to the `descriptor` parameter produces an error.
///
/// - Parameters:
///   - descriptor: A render, tile, or mesh pipeline state descriptor that describes the pipeline to create.
///   - dynamicLinkingDescriptor: An optional parameter that provides additional configuration for linking the pipeline state object.
///   - compilerTaskOptions: A description of the compilation process itself, providing parameters that
///         influence execution of the compilation process.
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: a compiler task representing the asynchronous compilation task.
- (id<MTL4CompilerTask>)newRenderPipelineStateWithDescriptor:(MTL4PipelineDescriptor*)descriptor
                                    dynamicLinkingDescriptor:(nullable MTL4RenderPipelineDynamicLinkingDescriptor*)dynamicLinkingDescriptor
                                         compilerTaskOptions:(nullable MTL4CompilerTaskOptions*)compilerTaskOptions
                                           completionHandler:(MTLNewRenderPipelineStateCompletionHandler)completionHandler;

/// Creates a new render pipeline state from another, previously unspecialized, pipeline state
///
/// Metal specializes the pipeline state with new state values the descriptor provides, observing the following rules:
/// * The compiler only updates properties that were originally specified as *unspecialized*. It doesn't modify other
/// already-specialized properties
/// * The compiler sets to their default behavior any unspecialized properties that your passed-in descriptor doesn't specialize
///
/// Additionally, there are some cases where the Metal can't specialize a pipeline:
/// * If the original pipeline state object doesn't have any unspecialized properties
/// * You can't re-specialize a previously specialized pipeline state object
///
/// - Parameters:
///   - descriptor: A render pipeline state descriptor or any type: default, tile, or mesh render pipeline descriptor.
///   - pipeline: A render pipeline state containing unspecialized substate.
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: a compiler task representing the asynchronous compilation task.
///
- (id<MTL4CompilerTask>)newRenderPipelineStateBySpecializationWithDescriptor:(MTL4PipelineDescriptor*)descriptor
                                                                    pipeline:(id<MTLRenderPipelineState>)pipeline
                                                           completionHandler:(MTLNewRenderPipelineStateCompletionHandler)completionHandler
    API_AVAILABLE(macos(26.0), ios(26.0));

/// Returns a new compiler task that asyncrhonously creates a binary version
/// of a GPU visible function or GPU intersection function.
///
/// - Parameters:
///   - descriptor: A configuration that tells the method which GPU function to
///   make into a binary function and which options to apply when compiling it.
///   - compilerTaskOptions: A configuration for the compiler task.
///   - completionHandler: A completetion handler that you provide, which the task calls
///   when it finishes compiling the binary function.
- (id<MTL4CompilerTask>)newBinaryFunctionWithDescriptor:(MTL4BinaryFunctionDescriptor *)descriptor
                                    compilerTaskOptions:(nullable MTL4CompilerTaskOptions*)compilerTaskOptions
                                      completionHandler:(MTL4NewBinaryFunctionCompletionHandler)completionHandler;

/// Creates a new ML pipeline state with descriptor.
///
/// - Parameters:
///   - descriptor: A machine learning pipeline state descriptor to use for creating the new pipeline state.
///   - error: An optional parameter into which Metal stores information in case of an error.
///
/// - Returns: A machine learning pipeline state if operation is successful, otherwise `nil`.
- (nullable id<MTL4MachineLearningPipelineState>)newMachineLearningPipelineStateWithDescriptor:(MTL4MachineLearningPipelineDescriptor *)descriptor
                                                                                         error:(NSError**)error;

/// Creates a new machine learning pipeline state asynchronously.
///
/// - Parameters:
///   - descriptor: A machine learning pipeline state descriptor to use for creating the new pipeline state.
///   - completionHandler: A block Metal calls when it finishes the build task.
///
/// - Returns: a compiler task representing the asynchronous compilation task.
- (id<MTL4CompilerTask>)newMachineLearningPipelineStateWithDescriptor:(MTL4MachineLearningPipelineDescriptor *)descriptor
                                                    completionHandler:(MTL4NewMachineLearningPipelineStateCompletionHandler)completionHandler;


@end

NS_ASSUME_NONNULL_END

#endif // MTL4Compiler_h
