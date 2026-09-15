//
//  MTL4LibraryDescriptor.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4LibraryDescriptor_h
#define MTL4LibraryDescriptor_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>

NS_ASSUME_NONNULL_BEGIN

@class MTLCompileOptions;

/// Serves as the base descriptor for creating a Metal library.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4LibraryDescriptor : NSObject <NSCopying>
/// Assigns an optional string containing the source code of the shader language program to compile into a
/// Metal library.
@property(nullable, copy, atomic) NSString *source;

/// Provides compile-time options for the Metal library.
@property(nullable, copy, atomic) MTLCompileOptions *options;

/// Assigns an optional name to the Metal library.
@property(nullable, copy, atomic) NSString *name;

@end

NS_ASSUME_NONNULL_END

#endif // MTL4LibraryDescriptor_h
