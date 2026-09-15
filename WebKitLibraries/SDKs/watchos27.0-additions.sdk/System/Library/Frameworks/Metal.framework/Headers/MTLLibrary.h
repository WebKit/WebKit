//
//  MTLLibrary.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLTypes.h>


#import <Metal/MTLFunctionDescriptor.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;
@protocol MTLFunction;
@protocol MTLLibrary;
@class MTLCompileOptions;
@class MTLFunctionConstantValues;
@class MTLIntersectionFunctionDescriptor;
@protocol MTLDynamicLibrary;

@class MTLRenderPipelineReflection;
@class MTLComputePipelineReflection;

@protocol MTLLibrary;
@protocol MTLRenderPipelineState;
@protocol MTLComputePipelineState;

/* Convenience typedefs that make it easy to declare storage for certain return types. */
typedef __autoreleasing MTLRenderPipelineReflection * __nullable MTLAutoreleasedRenderPipelineReflection;
typedef __autoreleasing MTLComputePipelineReflection * __nullable MTLAutoreleasedComputePipelineReflection;

typedef void (^MTLNewLibraryCompletionHandler)(id <MTLLibrary> __nullable library, NSError * __nullable error);

typedef void (^MTLNewRenderPipelineStateCompletionHandler)(id <MTLRenderPipelineState> __nullable renderPipelineState, NSError * __nullable error);

typedef void (^MTLNewRenderPipelineStateWithReflectionCompletionHandler)(id <MTLRenderPipelineState> __nullable renderPipelineState, MTLRenderPipelineReflection * _Nullable_result reflection, NSError * __nullable error);

typedef void (^MTLNewComputePipelineStateCompletionHandler)(id <MTLComputePipelineState> __nullable computePipelineState, NSError * __nullable error);

typedef void (^MTLNewComputePipelineStateWithReflectionCompletionHandler)(id <MTLComputePipelineState> __nullable computePipelineState, MTLComputePipelineReflection * _Nullable_result reflection, NSError * __nullable error);

typedef void (^MTLNewDynamicLibraryCompletionHandler)(id<MTLDynamicLibrary> __nullable library, NSError* __nullable error);


@protocol MTLArgumentEncoder;

typedef __autoreleasing MTLArgument *__nullable MTLAutoreleasedArgument API_DEPRECATED("Use MTLBinding and cast to specific Binding (MTLTextureBinding, MTLBufferBinding, .etc) instead", macos(10.11, 13.0), ios(8.0, 16.0));

typedef NS_ENUM(NSUInteger, MTLPatchType) {
    MTLPatchTypeNone = 0,
    MTLPatchTypeTriangle = 1,
    MTLPatchTypeQuad = 2,
} API_AVAILABLE(macos(10.12), ios(10.0));

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLVertexAttribute : NSObject

@property (readonly) NSString                    *name;
@property (readonly) NSUInteger                   attributeIndex;
@property (readonly) MTLDataType                  attributeType API_AVAILABLE(macos(10.11), ios(8.3));
@property (readonly, getter=isActive) BOOL        active;
@property (readonly, getter=isPatchData) BOOL              patchData API_AVAILABLE(macos(10.12), ios(10.0));
@property (readonly, getter=isPatchControlPointData) BOOL  patchControlPointData API_AVAILABLE(macos(10.12), ios(10.0));

@end

MTL_EXPORT API_AVAILABLE(macos(10.12), ios(10.0))
@interface MTLAttribute : NSObject

@property (readonly) NSString                    *name;
@property (readonly) NSUInteger                   attributeIndex;
@property (readonly) MTLDataType                  attributeType;
@property (readonly, getter=isActive) BOOL        active;
@property (readonly, getter=isPatchData) BOOL              patchData API_AVAILABLE(macos(10.12), ios(10.0));
@property (readonly, getter=isPatchControlPointData) BOOL  patchControlPointData API_AVAILABLE(macos(10.12), ios(10.0));

@end

/*!
 @enum MTLFunctionType
 @abstract An identifier for a top-level Metal function.
 @discussion Each location in the API where a program is used requires a function written for that specific usage.
 
 @constant MTLFunctionTypeVertex
 A vertex shader, usable for a MTLRenderPipelineState.
 
 @constant MTLFunctionTypeFragment
 A fragment shader, usable for a MTLRenderPipelineState.
 
 @constant MTLFunctionTypeKernel
 A compute kernel, usable to create a MTLComputePipelineState.
 */
typedef NS_ENUM(NSUInteger, MTLFunctionType) {
    MTLFunctionTypeVertex = 1,
    MTLFunctionTypeFragment = 2,
    MTLFunctionTypeKernel = 3,
    MTLFunctionTypeVisible API_AVAILABLE(macos(11.0), ios(14.0)) = 5,
    MTLFunctionTypeIntersection API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 6,
    MTLFunctionTypeMesh API_AVAILABLE(macos(13.0), ios(16.0)) = 7,
    MTLFunctionTypeObject API_AVAILABLE(macos(13.0), ios(16.0)) = 8,
} API_AVAILABLE(macos(10.11), ios(8.0));


/*!
 @interface MTLFunctionConstant
 @abstract describe an uberShader constant used by the function
 */
MTL_EXPORT API_AVAILABLE(macos(10.12), ios(10.0))
@interface MTLFunctionConstant : NSObject

@property (readonly) NSString *name;
@property (readonly) MTLDataType type;
@property (readonly) NSUInteger index;
@property (readonly) BOOL required;

@end

/*!
 @protocol MTLFunction
 @abstract A handle to intermediate code used as inputs for either a MTLComputePipelineState or a MTLRenderPipelineState.
 @discussion MTLFunction is a single vertex shader, fragment shader, or compute function.  A Function can only be used with the device that it was created against.
*/
API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@protocol MTLFunction <NSObject>

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, copy, atomic) NSString *label  API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @property device
 @abstract The device this resource was created against.  This resource can only be used with this device.
 */
@property (readonly) id <MTLDevice> device;

/*!
 @property functionType
 @abstract The overall kind of entry point: compute, vertex, or fragment.
 */
@property (readonly) MTLFunctionType functionType;

/*!
 @property patchType
 @abstract Returns the patch type. MTLPatchTypeNone if it is not a post tessellation vertex shader.
 */
@property (readonly) MTLPatchType patchType API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @property patchControlPointCount
 @abstract Returns the number of patch control points if it was specified in the shader. Returns -1 if it
 was not specified.
 */
@property (readonly) NSInteger patchControlPointCount API_AVAILABLE(macos(10.12), ios(10.0));

@property (nullable, readonly) NSArray <MTLVertexAttribute *> *vertexAttributes;

/*!
 @property stageInputAttributes
 @abstract Returns an array describing the attributes
 */
@property (nullable, readonly) NSArray <MTLAttribute *> *stageInputAttributes API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @property name
 @abstract The name of the function in the shading language.
 */
@property (readonly) NSString *name;

/*!
 @property functionConstantsDictionary
 @abstract A dictionary containing information about all function contents, keyed by the constant names.
 */
@property (readonly) NSDictionary<NSString *, MTLFunctionConstant *> *functionConstantsDictionary API_AVAILABLE(macos(10.12), ios(10.0));


/*!
 * @method newArgumentEncoderWithBufferIndex:
 * @abstract Creates an argument encoder which will encode arguments matching the layout of the argument buffer at the given bind point index.
 */
- (id <MTLArgumentEncoder>)newArgumentEncoderWithBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 * @method newArgumentEncoderWithBufferIndex:
 * @abstract Creates an argument encoder which will encode arguments matching the layout of the argument buffer at the given bind point index.
 */
- (id <MTLArgumentEncoder>)newArgumentEncoderWithBufferIndex:(NSUInteger)bufferIndex
                                                  reflection:(MTLAutoreleasedArgument * __nullable)reflection API_DEPRECATED("Use MTLDevice's newArgumentEncoderWithBufferBinding: instead", macos(10.13, 13.0), ios(11.0, 16.0));



/*!
 @property options
 @abstract The options this function was created with.
 */
@property (readonly) MTLFunctionOptions options API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));


@end

typedef NS_ENUM(NSUInteger, MTLLanguageVersion) {
    MTLLanguageVersion1_0 API_DEPRECATED("Use a newer language standard", ios(9.0, 16.0)) API_UNAVAILABLE(macos, macCatalyst) = (1 << 16),
    MTLLanguageVersion1_1 API_AVAILABLE(macos(10.11), ios(9.0)) = (1 << 16) + 1,
    MTLLanguageVersion1_2 API_AVAILABLE(macos(10.12), ios(10.0)) = (1 << 16) + 2,
    MTLLanguageVersion2_0 API_AVAILABLE(macos(10.13), ios(11.0)) = (2 << 16),
    MTLLanguageVersion2_1 API_AVAILABLE(macos(10.14), ios(12.0)) = (2 << 16) + 1,
    MTLLanguageVersion2_2 API_AVAILABLE(macos(10.15), ios(13.0)) = (2 << 16) + 2,
    MTLLanguageVersion2_3 API_AVAILABLE(macos(11.0), ios(14.0)) = (2 << 16) + 3,
    MTLLanguageVersion2_4 API_AVAILABLE(macos(12.0), ios(15.0)) = (2 << 16) + 4,
    MTLLanguageVersion3_0 API_AVAILABLE(macos(13.0), ios(16.0)) =
    (3 << 16) + 0,
    MTLLanguageVersion3_1 API_AVAILABLE(macos(14.0), ios(17.0)) = 
    (3 << 16) + 1,
    MTLLanguageVersion3_2 API_AVAILABLE(macos(15.0), ios(18.0)) =
    (3 << 16) + 2,
    MTLLanguageVersion4_0 API_AVAILABLE(macos(26.0), ios(26.0)) =
    (4 << 16) + 0,
    MTLLanguageVersion4_1 API_AVAILABLE(macos(27.0), ios(27.0)) =
    (4 << 16) + 1,
} API_AVAILABLE(macos(10.11), ios(9.0));

typedef NS_ENUM(NSInteger, MTLLibraryType) {
    MTLLibraryTypeExecutable = 0,
    MTLLibraryTypeDynamic = 1,
} API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @enum MTLLibraryOptimizationLevel
 @abstract Optimization level for the Metal compiler.
 
 @constant MTLLibraryOptimizationLevelDefault
 Optimize for program performance.
 
 @constant MTLLibraryOptimizationLevelSize
 Like default, with extra optimizations to reduce code size.
 */
typedef NS_ENUM(NSInteger, MTLLibraryOptimizationLevel)
{
    MTLLibraryOptimizationLevelDefault = 0,
    MTLLibraryOptimizationLevelSize = 1,
} API_AVAILABLE(macos(13.0), ios(16.0));


typedef NS_ENUM(NSInteger, MTLCompileSymbolVisibility)
{
    MTLCompileSymbolVisibilityDefault = 0,
    MTLCompileSymbolVisibilityHidden = 1,
} API_AVAILABLE(macos(13.3), ios(16.4));

/*!
 @enum MTLMathMode
 @abstract An enum to indicate if the compiler can perform optimizations for floating-point arithmetic that may violate the IEEE 754 standard
 
 @constant MTLMathModeSafe 
 Disables unsafe floating-point optimizations

 @constant MTLMathModeRelaxed
 Allows aggressive, unsafe floating-point optimizations but preserves infs and nans

 @constant MTLMathModeFast
 Allows aggressive, unsafe floating-point optimizations
 */
typedef NS_ENUM(NSInteger, MTLMathMode)
{
    MTLMathModeSafe = 0,
    MTLMathModeRelaxed = 1,
    MTLMathModeFast = 2,
};

/*!
 @enum MTLMathFloatingPointFunctions
 @abstract An enum to indicate the default math functions for single precision floating-point
 
 @constant MTLMathFloatingPointFunctionsFast
 Sets the default math functions for single precision floating-point to the corresponding functions in `metal::fast` namespace

 @constant MTLMathFloatingPointFunctionsPrecise
 Sets the default math functions for single precision floating-point to the corresponding functions in 'metal::precise' namespace
 */
typedef NS_ENUM(NSInteger, MTLMathFloatingPointFunctions)
{
    MTLMathFloatingPointFunctionsFast = 0,
    MTLMathFloatingPointFunctionsPrecise = 1,
};

/*!
 @enum MTLFloatingPointConversionRoundingMode
 @abstract The rounding mode for narrowing floating-point conversions.

 @constant MTLFloatingPointConversionRoundingModeToNearestEven
 Round-to-nearest-even (default). Standard IEEE 754 rounding mode.

 @constant MTLFloatingPointConversionRoundingModeTowardZero
 Round-toward-zero.
 */
typedef NS_ENUM(NSInteger, MTLFloatingPointConversionRoundingMode)
{
    MTLFloatingPointConversionRoundingModeToNearestEven = 0,
    MTLFloatingPointConversionRoundingModeTowardZero    = 1,
};

MTL_EXPORT API_AVAILABLE(macos(10.11), ios(8.0))
@interface MTLCompileOptions : NSObject <NSCopying>

// Pre-processor options

/*!
 @property preprocessorNames
 @abstract List of preprocessor macros to consider to when compiling this program. Specified as key value pairs, using a NSDictionary. The keys must be NSString objects and values can be either NSString or NSNumber objects.
 @discussion The default value is nil.
 */
@property (nullable, readwrite, copy, nonatomic) NSDictionary <NSString *, NSObject *> *preprocessorMacros;

// Math intrinsics options

/*!
 @property fastMathEnabled
 @abstract If YES, enables the compiler to perform optimizations for floating-point arithmetic that may violate the IEEE 754 standard. It also enables the high precision variant of math functions for single precision floating-point scalar and vector types. fastMathEnabled defaults to YES.
 */
@property (readwrite, nonatomic) BOOL fastMathEnabled API_DEPRECATED("Use mathMode instead", macos(10.11, 15.0), ios(8.0, 18.0));

 /*!
 @property mathMode
 @abstract Sets the floating-point arithmetic optimizations. Default depends on the language standard version.
 */
@property (readwrite, nonatomic) MTLMathMode mathMode API_AVAILABLE(macos(15.0), ios(18.0));

 /*!
 @property mathFloatingPointFunctions
 @abstract Sets the default math functions for single precision floating-point. Default is `MTLMathFloatingPointFunctionsFast`.
 */
@property (nonatomic) MTLMathFloatingPointFunctions mathFloatingPointFunctions API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @property languageVersion
 @abstract set the metal language version used to interpret the source.
 */
@property (readwrite, nonatomic) MTLLanguageVersion languageVersion API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @property type
 @abstract Which type the library should be compiled as. The default value is MTLLibraryTypeExecutable.
 @discussion MTLLibraryTypeExecutable is suitable to build a library of "kernel", "vertex" and "fragment" qualified functions.
 MTLLibraryType is suitable when the compilation result will instead be used to instantiate a MTLDynamicLibrary.
 MTLDynamicLibrary contains no qualified functions, but it's unqualified functions and variables can be used as an external dependency for compiling other libraries.
*/
@property (readwrite, nonatomic) MTLLibraryType libraryType API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @property installName
 @abstract The install name of this dynamic library.
 @discussion The install name is used when a pipeline state is created that depends, directly or indirectly, on a dynamic library.
 The installName is embedded into any other MTLLibrary that links against the compilation result.
 This property should be set such that the dynamic library can be found in the file system at the time a pipeline state is created.
 Specify one of:
 - an absolute path to a file from which the dynamic library can be loaded, or
 - a path relative to \@executable_path, where \@executable_path is substituted with the directory name from which the MTLLibrary containing the MTLFunction entrypoint used to create the pipeline state is loaded, or
 - a path relative to \@loader_path, where \@loader_path is substituted with the directory name from which the MTLLibrary with the reference to this installName embedded is loaded.
 The first is appropriate for MTLDynamicLibrary written to the file-system using its serializeToURL:error: method on the current device.
 The others are appropriate when the MTLDynamicLibrary is installed as part of a bundle or app, where the absolute path is not known.
 This property is ignored when the type property is not set to MTLLibraryTypeDynamic.
 This propery should not be null if the property type is set to MTLLibraryTypeDynamic: the compilation will fail in that scenario.
 */
@property (readwrite, nullable, copy, nonatomic) NSString *installName API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @property libraries
 @abstract A set of MTLDynamicLibrary instances to link against.
 The installName of the provided MTLDynamicLibrary is embedded into the compilation result.
 When a function from the resulting MTLLibrary is used (either as an MTLFunction, or as an to create a pipeline state, the embedded install names are used to automatically load the MTLDynamicLibrary instances.
 This property can be null if no libraries should be automatically loaded, either because the MTLLibrary has no external dependencies, or because you will use preloadedLibraries to specify the libraries to use at pipeline creation time.
*/
@property (readwrite, nullable, copy, nonatomic) NSArray<id<MTLDynamicLibrary>> *libraries API_AVAILABLE(macos(11.0), ios(14.0));


/*!
 @property preserveInvariance
 @abstract If YES,  set the compiler to compile shaders to preserve invariance.  The default is false.
 */
@property (readwrite, nonatomic) BOOL preserveInvariance API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(14.0));

/*!
 @property optimizationLevel
 @abstract Sets the compiler optimization level.
 */
@property (readwrite, nonatomic) MTLLibraryOptimizationLevel optimizationLevel API_AVAILABLE(macos(13.0), ios(16.0));

/*!
@property
@abstract Adds a compiler command to force the default visibility of symbols to be hidden
*/
@property (readwrite, nonatomic) MTLCompileSymbolVisibility compileSymbolVisibility API_AVAILABLE(macos(13.3), ios(16.4));

/*!
@property allowReferencingUndefinedSymbols
@abstract Adds a compiler command to allow the reference of undefined symbols
*/
@property (readwrite, nonatomic) BOOL allowReferencingUndefinedSymbols API_AVAILABLE(macos(13.3), ios(16.4));

/*!
@property maxTotalThreadsPerThreadgroup
@abstract Adds a compiler command to specify the total threads per threadgroup
*/
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerThreadgroup API_AVAILABLE(macos(13.3), ios(16.4));

/*!
 @property requiredThreadsPerThreadgroup
 @abstract Sets the required threads-per-threadgroup during dispatches. The `threadsPerThreadgroup` argument of any dispatch must match this value if it is set.
           Setting this to a size of 0 in every dimension disables this property
*/
@property(readwrite, nonatomic) MTLSize requiredThreadsPerThreadgroup API_AVAILABLE(macos(26.0), ios(26.0));

/*!
 @property enableLogging
 @abstract If YES,  set the compiler to enable any logging in the shader. The default is false.
 */
@property (readwrite, nonatomic) BOOL enableLogging API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @property floatingPointConversionRoundingMode
 @abstract Sets the rounding mode for narrowing floating-point conversions. Default is MTLFloatingPointConversionRoundingModeToNearestEven.
 */
@property (readwrite, nonatomic) MTLFloatingPointConversionRoundingMode floatingPointConversionRoundingMode API_AVAILABLE(macos(27.0), ios(27.0));
@end

/// Represents a reflection object containing information about a function in a Metal library.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0)) NS_SWIFT_SENDABLE
@interface MTLFunctionReflection : NSObject

/// Provides a list of inputs and outputs of the function.
@property (nonnull, readonly) NSArray<id<MTLBinding>> *bindings;

/// The string passed to the user annotation attribute for this function. Null if no user annotation is present for this function.
@property (nullable, readonly) NSString *userAnnotation API_AVAILABLE(macos(26.0), ios(26.0));

@end

/*!
 @constant MTLLibraryErrorDomain
 @abstract NSErrors raised when creating a library.
 */
API_AVAILABLE(macos(10.11), ios(8.0))
MTL_EXTERN NSErrorDomain const MTLLibraryErrorDomain;

/*!
 @enum MTLLibraryError
 @abstract NSErrors raised when creating a library.
 */
typedef NS_ENUM(NSUInteger, MTLLibraryError) {
    MTLLibraryErrorUnsupported      = 1,
    MTLLibraryErrorInternal         = 2,
    MTLLibraryErrorCompileFailure   = 3,
    MTLLibraryErrorCompileWarning   = 4,
    MTLLibraryErrorFunctionNotFound API_AVAILABLE(macos(10.12), ios(10.0)) = 5,
    MTLLibraryErrorFileNotFound API_AVAILABLE(macos(10.12), ios(10.0)) = 6,
} API_AVAILABLE(macos(10.11), ios(8.0));

API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@protocol MTLLibrary <NSObject>

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, copy, atomic) NSString *label;

/*!
 @property device
 @abstract The device this resource was created against.  This resource can only be used with this device.
 */
@property (readonly) id <MTLDevice> device;

/*!
 @method newFunctionWithName
 @abstract Returns a pointer to a function object, return nil if the function is not found in the library.
 */
- (nullable id <MTLFunction>) newFunctionWithName:(NSString *)functionName;

/*!
 @method newFunctionWithName:constantValues:error:
 @abstract Returns a pointer to a function object obtained by applying the constant values to the named function.
 @discussion This method will call the compiler. Use newFunctionWithName:constantValues:completionHandler: to
 avoid waiting on the compiler.
 */
- (nullable id <MTLFunction>) newFunctionWithName:(NSString *)name constantValues:(MTLFunctionConstantValues *)constantValues
					error:(__autoreleasing NSError **)error API_AVAILABLE(macos(10.12), ios(10.0));


/*!
 @method newFunctionWithName:constantValues:completionHandler:
 @abstract Returns a pointer to a function object obtained by applying the constant values to the named function.
 @discussion This method is asynchronous since it is will call the compiler.
 */
- (void) newFunctionWithName:(NSString *)name constantValues:(MTLFunctionConstantValues *)constantValues
			completionHandler:(void (^)(id<MTLFunction> __nullable function, NSError* __nullable error))completionHandler API_AVAILABLE(macos(10.12), ios(10.0));


/// Retrieves reflection information for a function in the library.
///
/// - Parameters:
///   - functionName: The name of a GPU function in the library.
///   The name needs to match one of the elements in the string array of library's ``functionNames`` property.
///
/// - Returns: An `MTLFunctionReflection` instance when the method succeeds; otherwise `nil`.
///
/// The reflection instance contains metadata information about a specific GPU function,
/// which can include:
/// * Function parameters
/// * Return types
/// * Bindings
/// * Annotations from a developer, if available
///
/// - Note: The Metal compiler generates the function's reflection information
/// when you or Xcode build the library.
///
/// The method only returns reflection information if all of the following conditions apply:
/// * The library has a function with a name that matches `functionName`.
/// * The deployment target is macOS 13.0 or later, or iOS 16.0 or later, or visionOS 2.0 or later.
- (nullable MTLFunctionReflection *)reflectionForFunctionWithName:(NSString *)functionName API_AVAILABLE(macos(26.0), ios(26.0));


/*!
 @method newFunctionWithDescriptor:completionHandler:
 @abstract Create a new MTLFunction object asynchronously.
 */
- (void)newFunctionWithDescriptor:(nonnull MTLFunctionDescriptor *)descriptor
                completionHandler:(void (^)(id<MTLFunction> __nullable function, NSError* __nullable error))completionHandler API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @method newFunctionWithDescriptor:error:
 @abstract Create  a new MTLFunction object synchronously.
 */
- (nullable id <MTLFunction>)newFunctionWithDescriptor:(nonnull MTLFunctionDescriptor *)descriptor
                                                 error:(__autoreleasing NSError **)error API_AVAILABLE(macos(11.0), ios(14.0));



/*!
 @method newIntersectionFunctionWithDescriptor:completionHandler:
 @abstract Create a new MTLFunction object asynchronously.
 */
- (void)newIntersectionFunctionWithDescriptor:(nonnull MTLIntersectionFunctionDescriptor *)descriptor
                            completionHandler:(void (^)(id<MTLFunction> __nullable function, NSError* __nullable error))completionHandler
    API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 @method newIntersectionFunctionWithDescriptor:error:
 @abstract Create  a new MTLFunction object synchronously.
 */
- (nullable id <MTLFunction>)newIntersectionFunctionWithDescriptor:(nonnull MTLIntersectionFunctionDescriptor *)descriptor
                                                             error:(__autoreleasing NSError **)error
    API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));



/*!
 @property functionNames
 @abstract The array contains NSString objects, with the name of each function in library.
 */
@property (readonly) NSArray <NSString *> *functionNames;

/*!
 @property type
 @abstract The library type provided when this MTLLibrary was created.
 Libraries with MTLLibraryTypeExecutable can be used to obtain MTLFunction from.
 Libraries with MTLLibraryTypeDynamic can be used to resolve external references in other MTLLibrary from.
 @see MTLCompileOptions
 */
@property (readonly) MTLLibraryType type API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @property installName
 @abstract The installName provided when this MTLLibrary was created.
 @discussion Always nil if the type of the library is not MTLLibraryTypeDynamic.
 @see MTLCompileOptions
 */
@property (readonly, nullable) NSString* installName API_AVAILABLE(macos(11.0), ios(14.0));


@end

NS_ASSUME_NONNULL_END
