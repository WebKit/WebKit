/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/TrackingRef.h>

namespace WTF {

template<typename T, typename TrackingTester = DefaultTrackingTester<T>>
class TrackingRefPtr : public TrackingReferenceHolder<T, TrackingTester> {
    using Base = TrackingReferenceHolder<T, TrackingTester>;
public:
    // Constructors must contain information about this holder.
    TrackingRefPtr(HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_refPtr(nullptr) {}
    TrackingRefPtr(std::nullptr_t, HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_refPtr(nullptr) {}
    TrackingRefPtr(T* ob, HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_refPtr(ob) { haveRefd(); }

    TrackingRefPtr(const TrackingRefPtr&) = delete;
    TrackingRefPtr& operator=(const TrackingRefPtr&) = delete;

    TrackingRefPtr(const RefPtr<T>& ob, HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_refPtr(ob) { haveRefd(); }
    TrackingRefPtr(RefPtr<T>&& ob, HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_refPtr(WTFMove(ob)) { haveRefd(); }

    ~TrackingRefPtr() { willDeref(); }

    TrackingRefPtr& operator=(const RefPtr<T>& r)
    {
        if (LIKELY(r.get() != m_refPtr.get())) {
            willDeref();
            m_refPtr = r;
            haveRefd();
        } else
            m_refPtr = r; // m_refPtr.refCount() won't actually change.
        return *this;
    }
    TrackingRefPtr& operator=(RefPtr<T>&& r)
    {
        if (LIKELY(r.get() != m_refPtr.get())) {
            willDeref();
            m_refPtr = WTFMove(r);
            haveRefd();
        } else
            m_refPtr = WTFMove(r); // m_refPtr.refCount() won't actually change.
        return *this;
    }

    void swap(RefPtr<T>& r) { willDeref(); m_refPtr.swap(r); haveRefd(); }
    friend void swap(RefPtr<T>& r, TrackingRefPtr& self) { self.willDeref(); self.m_refPtr.swap(r); self.haveRefd(); }

    TrackingRefPtr& operator=(T* ptr) { willDeref(); m_refPtr = ptr; haveRefd(); return *this; }
    TrackingRefPtr& operator=(std::nullptr_t) { willDeref(); m_refPtr = nullptr; return *this; }

    RefPtr<T> extractRefPtr() { willDeref(); return WTFMove(m_refPtr); }

    Ref<T> releaseNonNull() { ASSERT(m_refPtr); willDeref(); Ref<T> tmp(adoptRef(*m_refPtr)); m_refPtr = nullptr; return tmp; }

    T* leakRef() { willDeref(); return m_refPtr.leakRef(); }

    // Pass-through operations, which shouldn't affect the refCount.
    const RefPtr<T>& getRefPtr() const { return m_refPtr; }
    explicit operator bool() const { return bool(m_refPtr); }
    T* get() const { return m_refPtr.get(); }
    T& operator*() const { ASSERT(m_refPtr); return *m_refPtr.get(); }
    ALWAYS_INLINE T* operator->() const { return m_refPtr.get(); }

private:
    static constexpr bool shouldTrack(T& ob) { return TrackingTester::shouldTrack(ob); }

    void record(TrackingReferenceOperation op) { if (m_refPtr) { Base::record(*m_refPtr, op); } }
    void haveRefd() { record(TrackingReferenceOperation::ref); }
    void willDeref() { record(TrackingReferenceOperation::deref); }

    RefPtr<T> m_refPtr;
};

} // namespace WTF
