//
//  MTL4BinaryFunctionDescriptor.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4BinaryFunctionDescriptor_h
#define MTL4BinaryFunctionDescriptor_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTL4FunctionDescriptor.h>

NS_ASSUME_NONNULL_BEGIN

/// Options for configuring the creation of binary functions.
typedef NS_OPTIONS(NSUInteger, MTL4BinaryFunctionOptions) {
    /// Represents the default value: no options.
    MTL4BinaryFunctionOptionNone = 0,
    /// Compiles the function to have its function handles return a constant MTLResourceID across
    /// all pipeline states. The function needs to be linked to the pipeline that will use this function.
    MTL4BinaryFunctionOptionPipelineIndependent = 1 << 1,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// Base interface for other function-derived interfaces.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4BinaryFunctionDescriptor : NSObject<NSCopying>

/// Associates a string that uniquely identifies a binary function.
///
/// You can use this property to look up a corresponding binary function by name in a ``MTL4Archive`` instance.
@property (nonnull, copy, readwrite) NSString *name;

/// Provides the function descriptor corresponding to the function to compile into a binary function.
@property(nonatomic, readwrite, copy) MTL4FunctionDescriptor* functionDescriptor;

/// Configure the options to use at binary function creation time.
@property (nonatomic) MTL4BinaryFunctionOptions options;
@end

NS_ASSUME_NONNULL_END


#endif // MTLBinary4FunctionDescriptor_h
