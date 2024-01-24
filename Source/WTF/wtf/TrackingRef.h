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

#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace WTF {

template <typename T>
struct DefaultTrackingTester { static constexpr bool shouldTrack(T&) { return true; } };

template <typename PointeeType, typename DowncastTypeToTrack>
struct DowncastTrackingTester { static constexpr bool shouldTrack(PointeeType& ob) { return is<DowncastTypeToTrack>(ob); } };

enum class TrackingReferenceOperation { ref, deref };

struct HolderContainer {
    HolderContainer(uintptr_t value) : m_value(value) {}
    HolderContainer(const void* value) : m_value(reinterpret_cast<uintptr_t>(value)) {}
    constexpr bool operator==(const HolderContainer& other) const { return other.m_value == m_value; }
    uintptr_t m_value;
};

template<typename T, typename TrackingTester>
class TrackingReferenceHolder {
protected:
    TrackingReferenceHolder(HolderContainer holderContainer, String holderName) : m_holderContainer(holderContainer), m_holderName(WTFMove(holderName)) {}

    static constexpr bool shouldTrack(T& ob) { return TrackingTester::shouldTrack(ob); }

    void record(T& ob, TrackingReferenceOperation op) { if (shouldTrack(ob)) { ob.recordRefOperation(ob, op, m_holderContainer, m_holderName); } }

private:
    HolderContainer m_holderContainer;
    String m_holderName;
};

template<typename T, typename TrackingTester = DefaultTrackingTester<T>>
class TrackingRef : public TrackingReferenceHolder<T, TrackingTester> {
    using Base = TrackingReferenceHolder<T, TrackingTester>;
public:
    // Constructors must contain information about this holder.
    TrackingRef(HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_ref(nullptr) {}
    TrackingRef(std::nullptr_t, HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_ref(nullptr) {}
    TrackingRef(T& ob, HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_ref(ob) { haveRefd(); }

    TrackingRef(const TrackingRef&) = delete;
    TrackingRef& operator=(const TrackingRef&) = delete;

    TrackingRef(const Ref<T>& ob, HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_ref(ob) { haveRefd(); }
    TrackingRef(Ref<T>&& ob, HolderContainer holderContainer, String holderName) : Base(holderContainer, WTFMove(holderName)), m_ref(WTFMove(ob)) { haveRefd(); }

    ~TrackingRef() { willDeref(); }

    TrackingRef& operator=(const Ref<T>& r)
    {
        if (LIKELY(r.ptr() != m_ref.ptr())) {
            willDeref();
            m_ref = r;
            haveRefd();
        } else
            m_ref = r; // m_ref.refCount() won't actually change.
        return *this;
    }
    TrackingRef& operator=(Ref<T>&& r)
    {
        if (LIKELY(r.ptr() != m_ref.ptr())) {
            willDeref();
            m_ref = WTFMove(r);
            haveRefd();
        } else
            m_ref = WTFMove(r); // m_ref.refCount() won't actually change.
        return *this;
    }

    void swap(Ref<T>& r) { willDeref(); m_ref.swap(r); haveRefd(); }
    friend void swap(Ref<T>& r, TrackingRef& self) { self.willDeref(); self.m_ref.swap(r); self.haveRefd(); }

    TrackingRef& operator=(T* ptr) { willDeref(); m_ref = ptr; haveRefd(); return *this; }
    TrackingRef& operator=(std::nullptr_t) { willDeref(); m_ref = nullptr; return *this; }

    Ref<T> extractRef() { willDeref(); return WTFMove(m_ref); }

    Ref<T> releaseNonNull() { ASSERT(m_ref); willDeref(); Ref<T> tmp(adoptRef(*m_ref)); m_ref = nullptr; return tmp; }

    T* leakRef() { willDeref(); return m_ref.leakRef(); }

    // Pass-through operations, which shouldn't affect the refCount.
    const Ref<T>& getRef() const { return m_ref; }
    T* ptr() const { return m_ref.ptr(); }
    T& get() const { return m_ref.get(); }
    ALWAYS_INLINE T* operator->() const { return m_ref.ptr(); }

private:
    static constexpr bool shouldTrack(T& ob) { return TrackingTester::shouldTrack(ob); }

    void record(TrackingReferenceOperation op) { Base::record(m_ref.get(), op); }
    void haveRefd() { record(TrackingReferenceOperation::ref); }
    void willDeref() { record(TrackingReferenceOperation::deref); }

    Ref<T> m_ref;
};

} // namespace WTF
