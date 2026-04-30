/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/MemoryMode.h>
#include <JavaScriptCore/PageCount.h>
#include <JavaScriptCore/WasmAddressType.h>
#include <JavaScriptCore/WeakGCSet.h>

#include <wtf/CagedPtr.h>
#include <wtf/Expected.h>
#include <wtf/Function.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>

namespace WTF {
class PrintStream;
}

namespace JSC {

class LLIntOffsetsExtractor;

namespace Wasm {
class Memory final : public RefCountedAndCanMakeWeakPtr<Memory> {
    WTF_MAKE_NONCOPYABLE(Memory);
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(Memory, JS_EXPORT_PRIVATE);
    friend LLIntOffsetsExtractor;
public:
    void dump(WTF::PrintStream&) const;

    enum NotifyPressure { NotifyPressureTag };
    enum SyncTryToReclaim { SyncTryToReclaimTag };
    enum GrowSuccess { GrowSuccessTag };

    static Ref<Memory> create();
    JS_EXPORT_PRIVATE static Ref<Memory> create(Ref<BufferMemoryHandle>&&, AddressType, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);
    JS_EXPORT_PRIVATE static Ref<Memory> create(Ref<SharedArrayBufferContents>&&, AddressType, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);
    JS_EXPORT_PRIVATE static Ref<Memory> createZeroSized(MemorySharingMode, AddressType, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);
    static RefPtr<Memory> tryCreate(VM&, PageCount initial, PageCount maximum, MemorySharingMode, AddressType, std::optional<MemoryMode> desiredMemoryMode, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);

    JS_EXPORT_PRIVATE ~Memory();

    static size_t fastMappedRedzoneBytes() { return BufferMemoryHandle::fastMappedRedzoneBytes(); }
    static size_t fastMappedBytes() { return BufferMemoryHandle::fastMappedBytes(); } // Includes redzone.

    static bool addressIsInGrowableOrFastMemory(void*);

    void* basePointer() const { return m_handle->memory(); }
    size_t size() const { return m_handle->size(); }
    size_t mappedCapacity() const { return m_handle->mappedCapacity(); }
    PageCount initial() const { return m_handle->initial(); }
    PageCount maximum() const { return m_handle->maximum(); }
    AddressType addressType() const { return m_addressType; }
    BufferMemoryHandle& handle() { return m_handle.get(); }

    MemorySharingMode sharingMode() const { return m_handle->sharingMode(); }
    MemoryMode mode() const { return m_handle->mode(); }

    Expected<PageCount, GrowFailReason> grow(VM&, PageCount);
    bool fill(uint32_t, uint8_t, uint32_t);
    bool copy(uint32_t, uint32_t, uint32_t);
    bool init(uint64_t, const uint8_t*, uint32_t);

    void registerInstance(JSWebAssemblyInstance&);

    void checkLifetime() { ASSERT(!refCountDebugger().deletionHasBegun()); }

    static constexpr ptrdiff_t offsetOfHandle() { return OBJECT_OFFSETOF(Memory, m_handle); }

    SharedArrayBufferContents* shared() const { return m_shared.get(); }

private:
    Memory();
    Memory(Ref<BufferMemoryHandle>&&, AddressType, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);
    Memory(Ref<BufferMemoryHandle>&&, Ref<SharedArrayBufferContents>&&, AddressType, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);
    Memory(PageCount initial, PageCount maximum, MemorySharingMode, AddressType, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);

    Expected<PageCount, GrowFailReason> growShared(VM&, PageCount);

    Ref<BufferMemoryHandle> m_handle;
    RefPtr<SharedArrayBufferContents> m_shared;
    WTF::Function<void(GrowSuccess, PageCount, PageCount)> m_growSuccessCallback;
    AddressType m_addressType;
    // FIXME: If/When merging this into JSWebAssemblyMemory we should just use an unconditionalFinalizer.
};

} } // namespace JSC::Wasm

#else

namespace JSC { namespace Wasm {

class Memory {
public:
    static bool addressIsInGrowableOrFastMemory(void*) { return false; }
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
