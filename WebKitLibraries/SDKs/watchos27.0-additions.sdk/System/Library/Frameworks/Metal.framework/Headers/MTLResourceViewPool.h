//
//  MTLResourceViewPool.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTLResourceViewPool_h
#define MTLResourceViewPool_h

#import <Metal/MTLDefines.h>
#import <Metal/MTLAllocation.h>
#import <Metal/MTLDevice.h>
#import <Metal/MTLTypes.h>

NS_ASSUME_NONNULL_BEGIN


/// Provides parameters for creating a resource view pool.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTLResourceViewPoolDescriptor : NSObject <NSCopying>

/// Configures the number of resource views with which Metal creates the resource view pool.
@property (readwrite, nonatomic) NSUInteger resourceViewCount;

/// Assigns an optional label you to the resource view pool for debugging purposes.
@property (nullable, copy, nonatomic) NSString *label;

@end

/// Contains views over resources of a specific type, and allows you to manage those views.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTLResourceViewPool <NSObject>

/// Obtains the resource ID corresponding to the resource view at index 0 in this resource view pool.
@property (readonly, nonatomic) MTLResourceID baseResourceID;

/// Queries the number of resource views that this pool contains.
@property (readonly, nonatomic) NSUInteger resourceViewCount;

/// Obtains a reference to the GPU device this pool belongs to.
@property (readonly) id<MTLDevice> device;

/// Queries the optional debug label of this resource view pool.
@property (nullable, readonly, nonatomic) NSString *label;

/// Copies a range of resource views from a source view pool to a destination location in this view pool.
///
/// - Parameters:
///   - sourcePool: resource view pool from which to copy resource views.
///   - sourceRange: The range in the source resource view pool to copy.
///   - destinationIndex: The starting index in this destination view pool into which to copy the source range of resource views.
///
/// - Returns: The ``MTLResourceID`` of the resource view corresponding to `destinationIndex` of the copy in this resource view pool.
- (MTLResourceID)copyResourceViewsFromPool:(id<MTLResourceViewPool>)sourcePool
                               sourceRange:(NSRange)sourceRange
                          destinationIndex:(NSUInteger)destinationIndex;

@end


NS_ASSUME_NONNULL_END

#endif //MTLResourceViewPool_h

