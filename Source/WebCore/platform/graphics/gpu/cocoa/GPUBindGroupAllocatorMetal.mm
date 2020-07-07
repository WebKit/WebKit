/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "GPUBindGroupAllocator.h"

#if ENABLE(WEBGPU)

#import "GPUErrorScopes.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/CheckedArithmetic.h>

namespace WebCore {

Ref<GPUBindGroupAllocator> GPUBindGroupAllocator::create(GPUErrorScopes& errors)
{
    return adoptRef(*new GPUBindGroupAllocator(errors));
}

GPUBindGroupAllocator::GPUBindGroupAllocator(GPUErrorScopes& errors)
    : m_errorScopes(makeRef(errors))
{
}

#if USE(METAL)

Optional<GPUBindGroupAllocator::ArgumentBufferOffsets> GPUBindGroupAllocator::allocateAndSetEncoders(MTLArgumentEncoder *vertex, MTLArgumentEncoder *fragment, MTLArgumentEncoder *compute)
{
    id<MTLDevice> device = nil;
    auto checkedOffset = Checked<NSUInteger>(m_lastOffset);

    if (vertex) {
        device = vertex.device;
        checkedOffset += vertex.encodedLength;
    }
    if (fragment) {
        device = fragment.device;
        checkedOffset += fragment.encodedLength;
    }
    if (compute) {
        device = compute.device;
        checkedOffset += compute.encodedLength;
    }

    // No arguments; nothing to be done.
    if (!device)
        return { };

    if (checkedOffset.hasOverflowed()) {
        m_errorScopes->generateError("", GPUErrorFilter::OutOfMemory);
        return { };
    }

    auto newOffset = checkedOffset.unsafeGet();

    if (m_argumentBuffer && newOffset > m_argumentBuffer.get().length) {
        if (!reallocate(newOffset))
            return { };
    } else if (!m_argumentBuffer) {
        // Based off mimimum allocation for a shared-memory MTLBuffer on macOS.
        NSUInteger minimumSize = 4096;
        if (minimumSize < newOffset)
            minimumSize = newOffset;

        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_argumentBuffer = adoptNS([device newBufferWithLength:minimumSize options:0]);
        END_BLOCK_OBJC_EXCEPTIONS

        if (!m_argumentBuffer) {
            m_errorScopes->generateError("", GPUErrorFilter::OutOfMemory);
            return { };
        }
    }

    ArgumentBufferOffsets offsets;

    // Math in the following section is guarded against overflow by newOffset calculation.
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (vertex) {
        offsets.vertex = m_lastOffset;
        [vertex setArgumentBuffer:m_argumentBuffer.get() offset:*offsets.vertex];
    }
    if (fragment) {
        offsets.fragment = (vertex ? vertex.encodedLength : 0) + m_lastOffset;
        [fragment setArgumentBuffer:m_argumentBuffer.get() offset:*offsets.fragment];
    }
    if (compute) {
        offsets.compute = (vertex ? vertex.encodedLength : 0) + (fragment ? fragment.encodedLength : 0) + m_lastOffset;
        [compute setArgumentBuffer:m_argumentBuffer.get() offset:*offsets.compute];
    }

    END_BLOCK_OBJC_EXCEPTIONS

    m_lastOffset = newOffset;
    ++m_numBindGroups;

    return offsets;
}

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=200657, https://bugs.webkit.org/show_bug.cgi?id=200658 Optimize reallocation and reset behavior.
bool GPUBindGroupAllocator::reallocate(NSUInteger newOffset)
{
    MTLBuffer *newBuffer = nil;

    auto newLength = Checked<NSUInteger>(m_argumentBuffer.get().length);
    while (newLength < newOffset) {
        newLength *= 1.25;

        if (newLength.hasOverflowed()) {
            newLength = std::numeric_limits<NSUInteger>::max();
            break;
        }
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    newBuffer = [m_argumentBuffer.get().device newBufferWithLength:newLength.unsafeGet() options:0];
    memcpy(newBuffer.contents, m_argumentBuffer.get().contents, m_argumentBuffer.get().length);

    END_BLOCK_OBJC_EXCEPTIONS

    if (!newBuffer) {
        m_errorScopes->generateError("", GPUErrorFilter::OutOfMemory);
        return false;
    }

    m_argumentBuffer = adoptNS(newBuffer);
    return true;
}

void GPUBindGroupAllocator::tryReset()
{
    --m_numBindGroups;
    
    ASSERT(m_numBindGroups > -1);

    if (!m_numBindGroups) {
        m_argumentBuffer = nullptr;
        m_lastOffset = 0;
    }
}

#endif // USE(METAL)

} // namespace WebCore

#endif // ENABLE(WEBGPU)
