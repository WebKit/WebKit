//
//  MTL4SpecializedFunction.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4SpecializedFunctionDescriptor_h
#define MTL4SpecializedFunctionDescriptor_h

#import <Metal/MTLDefines.h>

#import <Metal/MTL4FunctionDescriptor.h>
#import <Metal/MTLFunctionConstantValues.h>

NS_ASSUME_NONNULL_BEGIN

/// Groups together properties to configure and create a specialized function by passing it to a factory method.
///
/// You can pass an instance of this class to any methods that accept a ``MTL4FunctionDescriptor`` parameter to
/// provide extra configuration, such as function constants or a name.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4SpecializedFunctionDescriptor : MTL4FunctionDescriptor

/// Provides a descriptor that corresponds to a base function that the specialization applies to.
@property (nullable, copy, readwrite, nonatomic) MTL4FunctionDescriptor* functionDescriptor;

/// Assigns an optional name to the specialized function.
@property (nullable, copy, atomic) NSString* specializedName;

/// Configures optional function constant values to associate with the function.
@property (nullable, copy, nonatomic) MTLFunctionConstantValues* constantValues;
@end

NS_ASSUME_NONNULL_END

#endif // MTL4SpecializedFunction_h
