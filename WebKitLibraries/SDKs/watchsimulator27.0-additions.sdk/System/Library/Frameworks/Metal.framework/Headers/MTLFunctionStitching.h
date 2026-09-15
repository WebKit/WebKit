//
//  MTLFunctionStitching.h
//  Framework
//
//  Copyright © 2021 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLArgument.h>


#import <Metal/MTLFunctionDescriptor.h>

@protocol MTLFunction;

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract
 A bitfield of options to create a stitched library
 */
typedef NS_OPTIONS(NSUInteger, MTLStitchedLibraryOptions)
{
    MTLStitchedLibraryOptionNone                       = 0,
    
    /**
     * @brief Library creation fails (i.e nil is returned) if:
     * - A lookup binary archive has been specified
     * - The library has not been found in the archive
     */
    MTLStitchedLibraryOptionFailOnBinaryArchiveMiss    = 1 << 0,
    /**
     * @brief stores and tracks this library in a Metal Pipelines Script
     * This flag is optional and only supported in the context of binary archives.
     * @discussion This flag is required for inspecting and consuming binary archives with stitched libraries via the metal-source tool. It is not required for recompilation, nor for storing stitched libraries in binary archives. Set this flag only if you intend to use metal-source on a serialized binary archive.
     */
    MTLStitchedLibraryOptionStoreLibraryInMetalPipelinesScript  = 1 << 1,
} API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @protocol MTLFunctionStitchingAttribute
 @abstract An attribute to be applied to the produced stitched function.
*/
API_AVAILABLE(macos(12.0), ios(15.0))
@protocol MTLFunctionStitchingAttribute <NSObject>
@end

/*!
 @interface MTLFunctionStitchingAttributeAlwaysInline
 @abstract Applies the `__attribute__((always_inline))` attribute to the produced stitched function.
 */
MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0))
@interface MTLFunctionStitchingAttributeAlwaysInline : NSObject<MTLFunctionStitchingAttribute>
@end

/*!
 @protocol MTLFunctionStitchingNode
 @abstract A node used in a graph for stitching.
*/
API_AVAILABLE(macos(12.0), ios(15.0))
@protocol MTLFunctionStitchingNode <NSObject, NSCopying>
@end

/*!
 @interface MTLFunctionStitchingInputNode
 @abstract An indexed input node of the produced stitched function.
 */
MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0))
@interface MTLFunctionStitchingInputNode : NSObject<MTLFunctionStitchingNode>
@property (readwrite, nonatomic) NSUInteger argumentIndex;
- (instancetype)initWithArgumentIndex:(NSUInteger)argument;
@end

/*!
 @interface MTLFunctionStitchingFunctionNode
 @abstract A function node that calls the specified function with arguments and ordering determined by data and control dependencies.
 */
MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0))
@interface MTLFunctionStitchingFunctionNode : NSObject<MTLFunctionStitchingNode>
@property (readwrite, copy, nonnull, nonatomic) NSString* name;
@property (readwrite, copy, nonnull, nonatomic) NSArray<id<MTLFunctionStitchingNode>>* arguments;
@property (readwrite, copy, nonnull, nonatomic) NSArray<MTLFunctionStitchingFunctionNode *>* controlDependencies;
- (instancetype)initWithName:(nonnull NSString*)name
                   arguments:(nonnull NSArray<id<MTLFunctionStitchingNode>>*)arguments
         controlDependencies:(nonnull NSArray<MTLFunctionStitchingFunctionNode *>*)controlDependencies;
@end

/*!
 @interface MTLFunctionStitchingGraph
 @abstract A function graph that describes a directed acyclic graph.
 @discussion The return value of the output node will be used as the return value for the final stitched graph.
 */
MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0))
@interface MTLFunctionStitchingGraph : NSObject<NSCopying>
@property (readwrite, copy, nonnull, nonatomic) NSString* functionName;
@property (readwrite, copy, nonnull, nonatomic) NSArray<MTLFunctionStitchingFunctionNode *>* nodes;
@property (readwrite, retain, nullable, nonatomic) MTLFunctionStitchingFunctionNode* outputNode;
@property (readwrite, copy, nonnull, nonatomic) NSArray<id<MTLFunctionStitchingAttribute>>* attributes;
- (instancetype)initWithFunctionName:(nonnull NSString*)functionName
                               nodes:(nonnull NSArray<MTLFunctionStitchingFunctionNode *>*)nodes
                          outputNode:(nullable MTLFunctionStitchingFunctionNode*)outputNode
                          attributes:(nonnull NSArray<id<MTLFunctionStitchingAttribute>>*)attributes;
@end

/*!
 @interface MTLStitchedLibraryDescriptor
 @abstract A container for the graphs and functions needed to create the stitched functions described by the graphs.
 */
MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0))
@interface MTLStitchedLibraryDescriptor : NSObject<NSCopying>
@property (readwrite, copy, nonnull, nonatomic) NSArray<MTLFunctionStitchingGraph *>* functionGraphs;
@property (readwrite, copy, nonnull, nonatomic) NSArray<id<MTLFunction>>* functions;

/*!
@property binaryArchives
@abstract The array of archives to be searched.
@discussion Binary archives to be searched for precompiled stitched libraries during the compilation of this library.
*/
@property (readwrite, copy, nonnull, nonatomic) NSArray<id<MTLBinaryArchive>>* binaryArchives API_AVAILABLE(macos(15.0), ios(18.0));

/*!
* @property options
* @abstract The options to use for this new MTLLibrary.
*/
@property (readwrite, nonatomic) MTLStitchedLibraryOptions options API_AVAILABLE(macos(15.0), ios(18.0));
@end


NS_ASSUME_NONNULL_END
