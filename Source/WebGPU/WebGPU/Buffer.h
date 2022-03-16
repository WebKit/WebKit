/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import <utility>
#import <wtf/CompletionHandler.h>
#import <wtf/FastMalloc.h>
#import <wtf/Range.h>
#import <wtf/RangeSet.h>
#import <wtf/Ref.h>
#import <wtf/ThreadSafeRefCounted.h>

namespace WebGPU {

class Device;

class Buffer : public ThreadSafeRefCounted<Buffer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class State : uint8_t;
    struct MappingRange {
        size_t beginOffset; // Inclusive
        size_t endOffset; // Exclusive
    };

    static Ref<Buffer> create(id<MTLBuffer> buffer, uint64_t size, WGPUBufferUsageFlags usage, State initialState, MappingRange initialMappingRange, Device& device)
    {
        return adoptRef(*new Buffer(buffer, size, usage, initialState, initialMappingRange, device));
    }

    ~Buffer();

    void destroy();
    const void* getConstMappedRange(size_t offset, size_t);
    void* getMappedRange(size_t offset, size_t);
    void mapAsync(WGPUMapModeFlags, size_t offset, size_t, CompletionHandler<void(WGPUBufferMapAsyncStatus)>&& callback);
    void unmap();
    void setLabel(const char*);

    // https://gpuweb.github.io/gpuweb/#buffer-state
    // Each GPUBuffer has a current buffer state on the Content timeline which is one of the following:
    enum class State : uint8_t {
        Mapped, // "where the GPUBuffer is available for CPU operations on its content."
        MappedAtCreation, // "where the GPUBuffer was just created and is available for CPU operations on its content."
        MappingPending, // "where the GPUBuffer is being made available for CPU operations on its content."
        Unmapped, // "where the GPUBuffer is available for GPU operations."
        Destroyed, // "where the GPUBuffer is no longer available for any operations except destroy."
    };

    id<MTLBuffer> buffer() const { return m_buffer; }
    size_t size() const { return m_size; }
    WGPUBufferUsageFlags usage() const { return m_usage; }

private:
    Buffer(id<MTLBuffer>, uint64_t size, WGPUBufferUsageFlags, State initialState, MappingRange initialMappingRange, Device&);

    bool validateGetMappedRange(size_t offset, size_t rangeSize) const;
    bool validateMapAsync(WGPUMapModeFlags, size_t offset, size_t rangeSize) const;
    bool validateUnmap() const;

    id<MTLBuffer> m_buffer { nil };

    // https://gpuweb.github.io/gpuweb/#buffer-interface
    // "GPUBuffer has the following internal slots:"
    const size_t m_size { 0 }; // "The length of the GPUBuffer allocation in bytes."
    const WGPUBufferUsageFlags m_usage { 0 }; // "The allowed usages for this GPUBuffer."
    State m_state { State::Unmapped }; // "The current state of the GPUBuffer."
    // "[[mapping]] of type ArrayBuffer or Promise or null." This is unnecessary; we can just use m_device.contents.
    MappingRange m_mappingRange { 0, 0 }; // "[[mapping_range]] of type list<unsigned long long> or null."
    using MappedRanges = RangeSet<Range<size_t>>;
    MappedRanges m_mappedRanges; // "[[mapped_ranges]] of type list<ArrayBuffer> or null."
    WGPUMapModeFlags m_mapMode { WGPUMapMode_None }; // "The GPUMapModeFlags of the last call to mapAsync() (if any)."

    const Ref<Device> m_device;
};

} // namespace WebGPU

struct WGPUBufferImpl {
    Ref<WebGPU::Buffer> buffer;
};
