//
//  MTL4FunctionDescriptor.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4FunctionDescriptor_h
#define MTL4FunctionDescriptor_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>

NS_ASSUME_NONNULL_BEGIN

/// Base interface for describing a Metal 4 shader function.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4FunctionDescriptor : NSObject <NSCopying>
@end

NS_ASSUME_NONNULL_END

#endif // MTL4FunctionDescriptor_h
