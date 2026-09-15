//
//  MTL4BufferRange.h
//  Metal
//
//  Copyright (c) 2025 Apple Inc. All rights reserved.
//

#ifdef __METAL_VERSION__

#import <metal_stdlib>
#import <Metal/MTLGPUAddress.h>

#else

#include <stdint.h>

#ifndef _WIN32
#import <Metal/MTLDefines.h>
#import <Metal/MTLGPUAddress.h>
#else
#include <Metal/MTLDefines.h>
#include <Metal/MTLGPUAddress.h>
#endif // _WIN32

#endif

/**
 * @brief A struct representing a range of a Metal buffer. The offset into the buffer is included in the address.
 * The length is generally optional, which a value of (uint64_t)-1 representing the range from the given address to
 * the end of the buffer. However, providing the length can enable more accurate API validation, especially when
 * sub-allocating ranges of a buffer.
 */
typedef struct MTL4BufferRange {
    /**
     * @brief Buffer address returned by the gpuAddress property of an MTLBuffer plus any offset into the buffer
     */
    MTLGPUAddress bufferAddress;

    /**
     * @brief Length of the region which begins at the given address. If the length is not known, a value of
     * (uint64_t)-1 represents the range from the given address to the end of the buffer.
     */
    uint64_t length;

#ifdef __cplusplus
    MTL4BufferRange()
        : bufferAddress(0),
          length(0)
    {
    }

    MTL4BufferRange(MTLGPUAddress bufferAddress)
        : bufferAddress(bufferAddress),
          length((uint64_t)-1)
    {
    }

    MTL4BufferRange(MTLGPUAddress bufferAddress, uint64_t length)
        : bufferAddress(bufferAddress),
          length(length)
    {
    }
#endif
} MTL4BufferRange;

#ifndef __METAL_VERSION__
/**
 * @brief Create a buffer range from a buffer's GPU address (given by the gpuAddress property) and length. A length of
 * (uint64_t)-1 represents the the range from the given address to the end of the buffer.
 */
MTL_INLINE MTL4BufferRange MTL4BufferRangeMake(MTLGPUAddress bufferAddress, uint64_t length) {
    MTL4BufferRange range;

    range.bufferAddress = bufferAddress;
    range.length = length;
    
    return range;
}
#endif
