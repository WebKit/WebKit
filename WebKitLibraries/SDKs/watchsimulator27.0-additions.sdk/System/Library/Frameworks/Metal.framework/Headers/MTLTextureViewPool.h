//
//  MTLTextureViewPool.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTLTextureViewPool_h
#define MTLTextureViewPool_h

#import <Metal/MTLResourceViewPool.h>

NS_ASSUME_NONNULL_BEGIN


/// A pool of lightweight texture views.
///
/// Use texture view pools to create lightweight texture view objects of ``MTLTexture``
/// and ``MTLBuffer`` instances.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTLTextureViewPool <MTLResourceViewPool>

/// Copies a default texture view to a slot in this texture view pool at an index provided.
///
/// - Parameters:
///   - texture: An ``MTLTexture`` instance for which to copy its texture view.
///   - index: An index of a slot in this texture pool into which this method copies the texture view.
/// - Returns: The ``MTLResourceID`` of a newly created texture view in this pool.
- (MTLResourceID)setTextureView:(id<MTLTexture>)texture
                        atIndex:(NSUInteger)index;

/// Creates a new lightweight texture view.
///
/// This method creates a lightweight texture view over a texture according to
/// a descriptor you provide. It then associates the texture view with a slot
/// in this texture view pool at the index you specify.
///
/// - Parameters:
///   - texture: An ``MTLTexture`` instance for which to create a new lightweight texture view.
///   - descriptor: A descriptor specifying properties of the texture view to create.
///   - index: An index of a slot in the texture pool into which this method writes the new texture view.
/// - Returns: The ``MTLResourceID`` of a newly created texture view in this pool.
- (MTLResourceID)setTextureView:(id<MTLTexture>)texture
                     descriptor:(MTLTextureViewDescriptor *)descriptor
                        atIndex:(NSUInteger)index;

/// Creates a new lightweight texture view of a buffer.
///
/// This method creates a lightweight texture view over a buffer, according to
/// a descriptor you provide. It then associates the texture view with a slot
/// in this texture view pool at the index you specify.
///
/// - Parameters:
///   - buffer: An ``MTLBuffer`` instance for which to create a new texture view.
///   - descriptor: A descriptor specifying properties of the texture view to create.
///   - offset: A byte offset, within the `buffer` parameter, at which the data for the texture view starts.
///   - bytesPerRow: The number of bytes between adjacent rows of pixels in the source buffer’s memory.
///   - index: An index of a slot in the table into which this method writes the new texture view.
/// - Returns: The ``MTLResourceID`` of a new buffer view in this pool.
- (MTLResourceID)setTextureViewFromBuffer:(id<MTLBuffer>)buffer
                               descriptor:(MTLTextureDescriptor *)descriptor
                                   offset:(NSUInteger)offset
                              bytesPerRow:(NSUInteger)bytesPerRow
                                  atIndex:(NSUInteger)index;


@end


NS_ASSUME_NONNULL_END

#endif //MTLTextureViewPool_h
