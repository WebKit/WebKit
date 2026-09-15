//
//  MTLBinaryArchive.h
//  Metal
//
//  Copyright (c) 2020 Apple Inc. All rights reserved.
//


#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>
@class MTLComputePipelineDescriptor;
@class MTLRenderPipelineDescriptor;
@class MTLTileRenderPipelineDescriptor;

NS_ASSUME_NONNULL_BEGIN

MTL_EXTERN NSErrorDomain const MTLBinaryArchiveDomain API_AVAILABLE(macos(11.0), ios(14.0));

typedef NS_ENUM(NSUInteger, MTLBinaryArchiveError)
{
    MTLBinaryArchiveErrorNone = 0,
    MTLBinaryArchiveErrorInvalidFile = 1,
    MTLBinaryArchiveErrorUnexpectedElement = 2,
    MTLBinaryArchiveErrorCompilationFailure = 3,
    MTLBinaryArchiveErrorInternalError API_AVAILABLE(macos(13.0), ios(16.0)) = 4,
} API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @class MTLBinaryArchiveDescriptor
 @abstract A class used to indicate how an archive should be created
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLBinaryArchiveDescriptor : NSObject <NSCopying>

/*!
 @property url
 @abstract The file URL from which to open a MTLBinaryArchive, or nil to create an empty MTLBinaryArchive.
 */
@property (readwrite, nullable, nonatomic, copy) NSURL *url;

@end

/*!
 @protocol MTLBinaryArchive
 @abstract A container of pipeline state descriptors and their associated compiled code.
 @discussion A MTLBinaryArchive allows to persist compiled pipeline state objects for a device, which can be used to skip recompilation on a subsequent run of the app.
 One or more archives may be supplied in the descriptor of a pipeline state, allowing the device to attempt to look up compiled code in them before performing compilation.
 If no archives are provided, or no archives contain the requested content, the pipeline state is created by compiling the code as usual.
 Note that software updates of the OS or device drivers may cause the archive to become outdated, causing the lookup to fail and the usual path performing on-demand compilation is taken.
 A MTLBinaryArchive is populated by adding functions from pipeline state descriptors to it, indicating which compiled code should be persisted in the archive.
 Once all desired pipeline state descriptors have been added, use serializeToURL:error: to write the contents for the current device to disk.
 MTLBinaryArchive files generated for multiple different devices can be combined using the "lipo" tool into a single archive, which can then be shipped with the application.
 It is possible to maintain different archive files for different contexts; for example each level in a game may use a different cache object.
 Note: Metal maintains a separate cache of pipeline states on behalf of each app that contains all compiled code; this cache is populated as compilation occurs.
 This cache will automatically accelerate pipeline state creation after a pipeline is created for the first time.
 Use MTLBinaryArchive to augment that cache by accelerating pipeline state creation even on the first run of an app.
 Updating a MTLBinaryArchive at runtime in a shipping app configuration is not recommended; such a scenario requires corruption resiliency, careful storage space management and may cache hard-to-reproduce errors.
 These kind of issues are handled transparently by the Metal maintained cache, therefore we recommend that MTLBinaryArchive is populated during development time and shipped as an asset.
 */
API_AVAILABLE(macos(11.0), ios(14.0))
@protocol MTLBinaryArchive <NSObject>

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, copy, atomic) NSString *label;

/*!
 @property device
 @abstract The device this resource was created against.  This resource can only be used with this device.
 */
@property (readonly) id<MTLDevice> device;

/*!
 @method addComputePipelineFunctionsWithDescriptor:error:
 @abstract Add the function(s) from a compute pipeline state to the archive.
 @param descriptor The descriptor from which function(s) will be added.
 @param error If the function fails, this will be set to describe the failure. This can be (but is not required to be) an error from the MTLBinaryArchiveDomain domain.
 @return Whether or not the addition succeeded. Functions referenced multiple times are silently accepted.
 */
- (BOOL) addComputePipelineFunctionsWithDescriptor:(MTLComputePipelineDescriptor*)descriptor error:(NSError**)error;

/*!
 @method addRenderPipelineFunctionsWithDescriptor:error:
 @abstract Add the function(s) from a render pipeline state to the archive.
 @param descriptor The descriptor from which function(s) will be added.
 @param error If the function fails, this will be set to describe the failure. This can be (but is not required to be) an error from the MTLBinaryArchiveDomain domain.
 @return Whether or not the addition succeeded. Functions referenced multiple times are silently accepted.
 */
- (BOOL) addRenderPipelineFunctionsWithDescriptor:(MTLRenderPipelineDescriptor*)descriptor error:(NSError**)error;

/*!
 @method addTileRenderPipelineFunctionsWithDescriptor:error:
 @abstract Add the function(s) from a tile render pipeline state to the archive.
 @param descriptor The descriptor from which function(s) will be added.
 @param error If the function fails, this will be set to describe the failure. This can be (but is not required to be) an error from the MTLBinaryArchiveDomain domain.
 @return Whether or not the addition succeeded. Functions referenced multiple times are silently accepted.
 */
- (BOOL) addTileRenderPipelineFunctionsWithDescriptor:(MTLTileRenderPipelineDescriptor*)descriptor error:(NSError**)error API_AVAILABLE(tvos(14.5));

/*!
 @method addMeshRenderPipelineFunctionsWithDescriptor:error:
 @abstract Add the function(s) from a mesh render pipeline state to the archive.
 @param descriptor The descriptor from which function(s) will be added.
 @param error If the function fails, this will be set to describe the failure. This can be (but is not required to be) an error from the MTLBinaryArchiveDomain domain.
 @return Whether or not the addition succeeded. Functions referenced multiple times are silently accepted.
 */
- (BOOL) addMeshRenderPipelineFunctionsWithDescriptor:(MTLMeshRenderPipelineDescriptor*)descriptor error:(NSError**)error API_AVAILABLE(macos(15.0), ios(18.0));


/*!
 @method addLibraryWithDescriptor:error:
 @abstract Add the function(s) from a stitched library to the archive.
 @param descriptor The stitched library descriptor from which function(s) will be added.
 @param error If the function fails, this will be set to describe the failure. This can be (but is not required to be) an error from the MTLBinaryArchiveDomain domain.
 @return Whether or not the addition succeeded. Functions referenced multiple times are silently accepted.
 */
- (BOOL) addLibraryWithDescriptor:(MTLStitchedLibraryDescriptor *)descriptor error:(NSError **)error API_AVAILABLE(macos(15.0), ios(18.0));
/*!
 @method serializeToURL:error:
 @abstract Write the contents of a MTLBinaryArchive to a file.
 @discussion Persisting the archive to a file allows opening the archive on a subsequent instance of the app, making available the contents without recompiling.
 @param url The file URL to which to write the file
 @param error If the function fails, this will be set to describe the failure. This can be (but is not required to be) an error from the MTLBinaryArchiveDomain domain. Other possible errors can be file access or I/O related.
 @return Whether or not the writing the file succeeded.
 */
- (BOOL) serializeToURL:(NSURL*)url error:(NSError**)error;

/*!
 @method addFunctionWithDescriptor:library:error:
 @abstract Add a `visible` or `intersection` function to the archive.
 @param descriptor The descriptor from which the function will be added.
 @param library Library of functions to add the function from.
 @param error If the function fails, this will be set to describe the failure. This can be (but is not required to be) an error from the MTLBinaryArchiveDomain domain. Other possible errors can be file access or I/O related.
 @return Whether or not the addition succeeded. Functions referenced multiple times are silently accepted.
 */
- (BOOL) addFunctionWithDescriptor:(MTLFunctionDescriptor *)descriptor library:(id<MTLLibrary>)library error:(NSError **)error API_AVAILABLE(macos(12.0), ios(15.0));

@end

NS_ASSUME_NONNULL_END
