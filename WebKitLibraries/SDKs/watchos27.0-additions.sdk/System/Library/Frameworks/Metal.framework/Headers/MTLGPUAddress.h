//
//  MTLGPUAddress.h
//  Metal
//
//  Copyright (c) 2025 Apple Inc. All rights reserved.
//

#ifdef __METAL_VERSION__

#import <metal_stdlib>

#else

#include <stdint.h>

#endif

/// A 64-bit unsigned integer type appropriate for storing GPU addresses.
typedef uint64_t MTLGPUAddress;
