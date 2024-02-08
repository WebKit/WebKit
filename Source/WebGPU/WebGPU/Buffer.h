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
#import <wtf/RefCounted.h>
#import <wtf/WeakPtr.h>

struct WGPUBufferImpl {
};

namespace WebGPU {

class CommandEncoder;
class Device;

// https://gpuweb.github.io/gpuweb/#gpubuffer
class Buffer : public WGPUBufferImpl, public RefCounted<Buffer>, public CanMakeWeakPtr<Buffer> {
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
    static Ref<Buffer> createInvalid(Device& device)
    {
        return adoptRef(*new Buffer(device));
    }

    ~Buffer();

    void destroy();
    const void* getConstMappedRange(size_t offset, size_t);
    void* getMappedRange(size_t offset, size_t);
    void mapAsync(WGPUMapModeFlags, size_t offset, size_t, CompletionHandler<void(WGPUBufferMapAsyncStatus)>&& callback);
    void unmap();
    void setLabel(String&&);

    bool isValid() const;

    // https://gpuweb.github.io/gpuweb/#buffer-state
    enum class State : uint8_t {
        Mapped,
        MappedAtCreation,
        MappingPending,
        Unmapped,
        Destroyed,
    };

    id<MTLBuffer> buffer() const { return m_buffer; }
    uint64_t size() const;
    WGPUBufferUsageFlags usage() const { return m_usage; }
    State state() const { return m_state; }

    Device& device() const { return m_device; }
    bool isDestroyed() const;
    void setCommandEncoder(CommandEncoder&, bool mayModifyBuffer = false) const;
    uint32_t maxIndex(MTLIndexType) const;

private:
    Buffer(id<MTLBuffer>, uint64_t size, WGPUBufferUsageFlags, State initialState, MappingRange initialMappingRange, Device&);
    Buffer(Device&);
    void recomputeMaxIndexValues() const;

    bool validateGetMappedRange(size_t offset, size_t rangeSize) const;
    NSString* errorValidatingMapAsync(WGPUMapModeFlags, size_t offset, size_t rangeSize) const;
    bool validateUnmap() const;

    id<MTLBuffer> m_buffer { nil };

    // https://gpuweb.github.io/gpuweb/#buffer-interface

    const uint64_t m_size { 0 };
    const WGPUBufferUsageFlags m_usage { 0 };
    State m_state { State::Unmapped };
    // [[mapping]] is unnecessary; we can just use m_device.contents.
    MappingRange m_mappingRange { 0, 0 };
    using MappedRanges = RangeSet<Range<size_t>>;
    MappedRanges m_mappedRanges;
    WGPUMapModeFlags m_mapMode { WGPUMapMode_None };
    Vector<uint8_t> m_emptyBuffer;

    const Ref<Device> m_device;
    mutable WeakPtr<CommandEncoder> m_commandEncoder;
    mutable uint16_t m_max16BitIndex { 0 };
    mutable uint32_t m_max32BitIndex { 0 };
};

} // namespace WebGPU
