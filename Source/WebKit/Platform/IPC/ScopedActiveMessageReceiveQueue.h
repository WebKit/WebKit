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

#pragma once

#include <memory>
#include <utility>
#include <wtf/HashTraits.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace IPC {

// Scoped holder for objects that have active message receive queues. Enforces that
// once holder stops holding the object, the message queue should be removed.
// The convention is to call stopListeningForIPC() for the object.
// Optionally the object can obtain the owning reference that was used to hold the object
// in order to attempt to avoid needless destruction of the object in holder thread.
// This useful in the case the stop invokes a cleanup task in the message handler
// thread.
template <typename T, typename HolderType = RefPtr<T>>
class ScopedActiveMessageReceiveQueue {
public:
    ScopedActiveMessageReceiveQueue() = default;
    template <typename U>
    ScopedActiveMessageReceiveQueue(U&& object)
        : m_object(WTFMove(object))
    {
    }
    ScopedActiveMessageReceiveQueue(ScopedActiveMessageReceiveQueue&& other)
        : m_object(std::exchange(other.m_object, nullptr))
    {
    }
    ScopedActiveMessageReceiveQueue& operator=(ScopedActiveMessageReceiveQueue&& other)
    {
        if (this != &other) {
            reset();
            m_object = std::exchange(other.m_object, nullptr);
        }
        return *this;
    }
    ~ScopedActiveMessageReceiveQueue()
    {
        reset();
    }
    void reset()
    {
        if (!m_object)
            return;
        stopListeningForIPCAndRelease(m_object);
    }
    T* get() const { return m_object.get(); }
    T* operator->() const { return m_object.get(); }
private:
    template<typename U>
    static auto stopListeningForIPCAndRelease(U& object) -> decltype(object->stopListeningForIPC(object.releaseNonNull()), void())
    {
        object->stopListeningForIPC(object.releaseNonNull());
    }
    template<typename U>
    static auto stopListeningForIPCAndRelease(U& object) -> decltype(object->stopListeningForIPC(WTFMove(object)), void())
    {
        object->stopListeningForIPC(WTFMove(object));
    }
    template<typename U>
    static auto stopListeningForIPCAndRelease(U& object) -> decltype(object->stopListeningForIPC(), void())
    {
        object->stopListeningForIPC();
        object = nullptr;
    }
    HolderType m_object;
};

template<typename T>
ScopedActiveMessageReceiveQueue(std::unique_ptr<T>&&) -> ScopedActiveMessageReceiveQueue<T, std::unique_ptr<T>>;

template<typename T>
ScopedActiveMessageReceiveQueue(Ref<T>&&) -> ScopedActiveMessageReceiveQueue<T, RefPtr<T>>;

}

namespace WTF {

template<typename T, typename HolderType> struct HashTraits<IPC::ScopedActiveMessageReceiveQueue<T, HolderType>> : GenericHashTraits<IPC::ScopedActiveMessageReceiveQueue<T, HolderType>> {
    using PeekType = T*;
    static T* peek(const IPC::ScopedActiveMessageReceiveQueue<T, HolderType>& value) { return value.get(); }
};

}
