/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmMemoryMode.h"
#include "WasmPageCount.h"

#include <wtf/CagedPtr.h>
#include <wtf/Expected.h>
#include <wtf/Function.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class PrintStream;
}

namespace JSC {

class LLIntOffsetsExtractor;

namespace Wasm {

class Instance;

class MemoryHandle final : public ThreadSafeRefCounted<MemoryHandle> {
    WTF_MAKE_NONCOPYABLE(MemoryHandle);
    WTF_MAKE_FAST_ALLOCATED;
    friend LLIntOffsetsExtractor;
public:
    MemoryHandle(void*, size_t size, size_t mappedCapacity, PageCount initial, PageCount maximum, MemorySharingMode, MemoryMode);
    JS_EXPORT_PRIVATE ~MemoryHandle();

    void* memory() const;
    size_t size() const { return m_size; }
    size_t mappedCapacity() const { return m_mappedCapacity; }
    size_t boundsCheckingSize() const { return m_mappedCapacity; }
    PageCount initial() const { return m_initial; }
    PageCount maximum() const { return m_maximum; }
    MemorySharingMode sharingMode() const { return m_sharingMode; }
    MemoryMode mode() const { return m_mode; }
    static ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(MemoryHandle, m_size); }
    Lock& lock() { return m_lock; }

    void growToSize(size_t size)
    {
        m_size = size;
    }

private:
    using CagedMemory = CagedPtr<Gigacage::Primitive, void, tagCagedPtr>;

    Lock m_lock;
    MemorySharingMode m_sharingMode { MemorySharingMode::Default };
    MemoryMode m_mode { MemoryMode::BoundsChecking };
    CagedMemory m_memory;
    size_t m_size { 0 };
    size_t m_mappedCapacity { 0 };
    PageCount m_initial;
    PageCount m_maximum;
};

class Memory final : public RefCounted<Memory> {
    WTF_MAKE_NONCOPYABLE(Memory);
    WTF_MAKE_FAST_ALLOCATED;
    friend LLIntOffsetsExtractor;
public:
    void dump(WTF::PrintStream&) const;

    explicit operator bool() const { return !!m_handle->memory(); }
    
    enum NotifyPressure { NotifyPressureTag };
    enum SyncTryToReclaim { SyncTryToReclaimTag };
    enum GrowSuccess { GrowSuccessTag };

    static Ref<Memory> create();
    JS_EXPORT_PRIVATE static Ref<Memory> create(Ref<MemoryHandle>&&, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);
    static RefPtr<Memory> tryCreate(PageCount initial, PageCount maximum, MemorySharingMode, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);

    JS_EXPORT_PRIVATE ~Memory();

    static size_t fastMappedRedzoneBytes();
    static size_t fastMappedBytes(); // Includes redzone.
    static bool addressIsInGrowableOrFastMemory(void*);

    void* memory() const { return m_handle->memory(); }
    size_t size() const { return m_handle->size(); }
    PageCount sizeInPages() const { return PageCount::fromBytes(size()); }
    size_t boundsCheckingSize() const { return m_handle->boundsCheckingSize(); }
    PageCount initial() const { return m_handle->initial(); }
    PageCount maximum() const { return m_handle->maximum(); }
    MemoryHandle& handle() { return m_handle.get(); }

    MemorySharingMode sharingMode() const { return m_handle->sharingMode(); }
    MemoryMode mode() const { return m_handle->mode(); }

    enum class GrowFailReason {
        InvalidDelta,
        InvalidGrowSize,
        WouldExceedMaximum,
        OutOfMemory,
    };
    Expected<PageCount, GrowFailReason> grow(PageCount);
    bool fill(uint32_t, uint8_t, uint32_t);
    bool copy(uint32_t, uint32_t, uint32_t);
    bool init(uint32_t, const uint8_t*, uint32_t);

    void registerInstance(Instance*);

    void check() {  ASSERT(!deletionHasBegun()); }

    static ptrdiff_t offsetOfHandle() { return OBJECT_OFFSETOF(Memory, m_handle); }

private:
    Memory();
    Memory(Ref<MemoryHandle>&&, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);
    Memory(PageCount initial, PageCount maximum, MemorySharingMode, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);

    Expected<PageCount, GrowFailReason> growShared(PageCount);

    Ref<MemoryHandle> m_handle;
    WTF::Function<void(NotifyPressure)> m_notifyMemoryPressure;
    WTF::Function<void(SyncTryToReclaim)> m_syncTryToReclaimMemory;
    WTF::Function<void(GrowSuccess, PageCount, PageCount)> m_growSuccessCallback;
    Vector<WeakPtr<Instance>> m_instances;
};

} } // namespace JSC::Wasm

#else

namespace JSC { namespace Wasm {

class Memory {
public:
    static size_t maxFastMemoryCount() { return 0; }
    static bool addressIsInGrowableOrFastMemory(void*) { return false; }
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
