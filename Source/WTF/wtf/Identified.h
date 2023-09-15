/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <atomic>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadAssertions.h>
#include <wtf/UUID.h>

namespace WTF {

template <typename IdentifierType, typename ClassType>
class IdentifiedBase {
public:
    IdentifierType identifier() const
    {
        return m_identifier;
    }

protected:
    IdentifiedBase(const IdentifiedBase&) = default;

    explicit IdentifiedBase(IdentifierType identifier)
        : m_identifier(identifier)
    {
    }

    IdentifiedBase& operator=(const IdentifiedBase&) = default;

private:
    IdentifierType m_identifier;
};

template <typename T>
class Identified : public IdentifiedBase<uint64_t, T> {
protected:
    Identified()
        : IdentifiedBase<uint64_t, T>(generateIdentifier())
    {
    }

    Identified(const Identified&) = default;
    Identified& operator=(const Identified&) = default;

    explicit Identified(uint64_t identifier)
        : IdentifiedBase<uint64_t, T>(identifier)
    {
    }

private:
    static uint64_t generateIdentifier()
    {
        static NeverDestroyed<ThreadLikeAssertion> initializationThread;
        assertIsCurrent(initializationThread); // You should be using ThreadSafeIdentified if you hit this assertion.

        static uint64_t currentIdentifier;
        return ++currentIdentifier;
    }
};

template <typename T>
class ThreadSafeIdentified : public IdentifiedBase<uint64_t, T> {
protected:
    ThreadSafeIdentified()
        : IdentifiedBase<uint64_t, T>(generateIdentifier())
    {
    }

    ThreadSafeIdentified(const ThreadSafeIdentified&) = default;
    ThreadSafeIdentified& operator=(const ThreadSafeIdentified&) = default;

    explicit ThreadSafeIdentified(uint64_t identifier)
        : IdentifiedBase<uint64_t, T>(identifier)
    {
    }

private:
    static uint64_t generateIdentifier()
    {
        static LazyNeverDestroyed<std::atomic<uint64_t>> currentIdentifier;
        static std::once_flag initializeCurrentIdentifier;
        std::call_once(initializeCurrentIdentifier, [] {
            currentIdentifier.construct(0);
        });
        return ++currentIdentifier.get();
    }
};

template <typename T>
class UUIDIdentified : public IdentifiedBase<UUID, T> {
protected:
    UUIDIdentified()
        : IdentifiedBase<UUID, T>(UUID::createVersion4())
    {
    }

    UUIDIdentified(const UUIDIdentified&) = default;
};

} // namespace WTF

using WTF::Identified;
using WTF::ThreadSafeIdentified;
using WTF::UUIDIdentified;
