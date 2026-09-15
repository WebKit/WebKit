//
//  MTLIOCompression.h
//  Metal
//
//  Copyright © 2022 Apple, Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>



NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, MTLIOCompressionStatus) {
    MTLIOCompressionStatusComplete = 0,
    MTLIOCompressionStatusError = 1
} API_AVAILABLE(macos(13.0), ios(16.0));

typedef void* MTLIOCompressionContext;

/*
 These methods are expected to be used by offline tools that
 process and generate assets.
 */

/*
 @function MTLIOCompressionContextDefaultChunkSize
 @abstract The default chunk size to use for a compression context.
 */
MTL_EXTERN size_t MTLIOCompressionContextDefaultChunkSize(void);

/*
 @function MTLIOCreateCompressionContext
 @abstract used to create a context that writes a stream of data to
 a path using a given codec and chunk size.
 @discussion An invalid type will cause a nil to be returned. If path is
 invalid or fails to open a nil will be returned and errno will be set.
 */
MTL_EXTERN __nullable MTLIOCompressionContext MTLIOCreateCompressionContext(const char* path, MTLIOCompressionMethod type, size_t chunkSize) API_AVAILABLE(macos(13.0), ios(16.0));

/*
 @function MTLIOCompressionContextAppendData
 @abstract append data to a compression context.
 */
MTL_EXTERN void MTLIOCompressionContextAppendData(MTLIOCompressionContext context, const void* data, size_t size) API_AVAILABLE(macos(13.0), ios(16.0));

/*
 @function MTLIOFlushAndDestroyCompressionContext
 @abstract close the compression context and write the pack file.
 */
MTL_EXTERN MTLIOCompressionStatus MTLIOFlushAndDestroyCompressionContext(MTLIOCompressionContext context) API_AVAILABLE(macos(13.0), ios(16.0));

NS_ASSUME_NONNULL_END


