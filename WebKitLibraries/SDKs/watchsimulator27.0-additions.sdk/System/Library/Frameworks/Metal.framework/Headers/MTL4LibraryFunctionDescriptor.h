//
//  MTL4LibraryFunctionDescriptor.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4LibraryFunctionDescriptor_h
#define MTL4LibraryFunctionDescriptor_h

#import <Metal/MTLDefines.h>

#import <Metal/MTL4FunctionDescriptor.h>

@protocol MTLLibrary;

NS_ASSUME_NONNULL_BEGIN

/// Describes a shader function from a Metal library.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4LibraryFunctionDescriptor : MTL4FunctionDescriptor

/// Assigns a name to the function.
@property (nullable, copy, atomic) NSString* name;

/// Returns a reference to the library containing the function.
@property (nullable, readwrite, nonatomic, retain) id<MTLLibrary> library;
@end

NS_ASSUME_NONNULL_END

#endif // MTL4LibraryFunctionDescriptor_h
