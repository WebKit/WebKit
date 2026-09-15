//
//  MTLFunctionDescriptor.h
//  Metal
//
//  Copyright © 2020 Apple, Inc. All rights reserved.
//


#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLFunctionConstantValues.h>

@protocol MTLBinaryArchive;

typedef NS_OPTIONS(NSUInteger, MTLFunctionOptions) {
    /**
     * @brief Default usage
     */
    MTLFunctionOptionNone = 0,
    
    /**
     * @brief Compiles the found function. This enables dynamic linking of this `MTLFunction`.
     * Only supported for `visible` functions.
     */
    MTLFunctionOptionCompileToBinary API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 1 << 0,
    /**
     * @brief stores and tracks this function in a Metal Pipelines Script
     * This flag is optional and only supported in the context of binary archives.
     * @discussion This flag is required for inspecting and consuming binary archives with specialized MTLFunctions via the metal-source tool. It is not required for recompilation, nor for storing functions in binary archives. Set this flag only if you intend to use metal-source on a serialized binary archive.
     */
    MTLFunctionOptionStoreFunctionInMetalPipelinesScript API_AVAILABLE(macos(15.0), ios(18.0)) = 1 << 1,
    MTLFunctionOptionStoreFunctionInMetalScript API_DEPRECATED_WITH_REPLACEMENT("MTLFunctionOptionStoreFunctionInMetalPipelinesScript", macos(14.0, 15.0), ios(17.0, 18.0), tvos(17.0, 18.0)) = 1 << 1,
    
    /**
         * @brief Function creation fails (i.e nil is returned) if:
         * - A lookup binary archive has been specified
         * - The function has not been found in the archive
         */
         MTLFunctionOptionFailOnBinaryArchiveMiss API_AVAILABLE(macos(15.0), ios(18.0)) = 1 << 2,
    /**
     * @brief Compiles the function to have its function handles return a constant MTLResourceID across
     * all pipeline states. The function needs to be linked to the pipeline that will use this function.
     * This function option can only be used for functions that are compiled with `MTLFunctionOptionCompileToBinary`.
     */
    MTLFunctionOptionPipelineIndependent API_AVAILABLE(macos(26.0), ios(26.0)) = 1 << 3,
} API_AVAILABLE(macos(11.0), ios(14.0));

MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLFunctionDescriptor : NSObject <NSCopying>

/*!
 @method functionDescriptor
 @abstract Create an autoreleased function descriptor
 */
+ (nonnull MTLFunctionDescriptor *)functionDescriptor;

/*!
* @property name
* @abstract The name of the `visible` function to find.
*/
@property (nullable, copy, nonatomic) NSString *name;

/*!
* @property specializedName
* @abstract An optional new name for a `visible` function to allow reuse with different specializations.
*/
@property (nullable, copy, nonatomic) NSString *specializedName;

/*!
* @property constantValues
* @abstract The set of constant values assigned to the function constants. Compilation fails if you do not provide valid constant values for all required function constants.
*/
@property (nullable, nonatomic, copy) MTLFunctionConstantValues *constantValues;

/*!
* @property options
* @abstract The options to use for this new `MTLFunction`.
*/
@property (nonatomic) MTLFunctionOptions options;

/*!
@property binaryArchives
@abstract The array of archives to be searched.
@discussion Binary archives to be searched for precompiled functions during the compilation of this function.
*/
@property (readwrite, nullable, nonatomic, copy) NSArray<id<MTLBinaryArchive>> *binaryArchives API_AVAILABLE(macos(12.0), ios(15.0));

@end

MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@interface MTLIntersectionFunctionDescriptor : MTLFunctionDescriptor <NSCopying>

@end

