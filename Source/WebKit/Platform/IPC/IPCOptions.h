/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "Timeout.h"
#include <initializer_list>
#include <wtf/OptionSet.h>

namespace IPC {

enum class SendOption : uint8_t {
    // Whether this message should be dispatched when waiting for a sync reply.
    // This is the default for synchronous messages.
    DispatchMessageEvenWhenWaitingForSyncReply = 1 << 0,
    DispatchMessageEvenWhenWaitingForUnboundedSyncReply = 1 << 1,
    IgnoreFullySynchronousMode = 1 << 2,
#if ENABLE(IPC_TESTING_API)
    IPCTestingMessage = 1 << 3,
#endif
};

enum class SendSyncOption : uint8_t {
    // Use this to inform that this sync call will suspend this process until the user responds with input.
    InformPlatformProcessWillSuspend = 1 << 0,
    UseFullySynchronousModeForTesting = 1 << 1,
    ForceDispatchWhenDestinationIsWaitingForUnboundedSyncReply = 1 << 2,
    MaintainOrderingWithAsyncMessages = 1 << 3,
};

enum class WaitForOption {
    // Use this to make waitForMessage be interrupted immediately by any incoming sync messages.
    InterruptWaitingIfSyncMessageArrives = 1 << 0,
    DispatchIncomingSyncMessagesWhileWaiting = 1 << 1,
};

class StreamSendOptions {
public:
    template<typename Type> static constexpr bool isOptionType =
        std::is_same_v<Type, Timeout>
        || std::is_same_v<Type, Seconds>;

    template<typename T>
    StreamSendOptions(std::initializer_list<T> options)
    {
        for (auto option : options)
            setOption(option);
    }

    template<typename T, typename U, typename... Rest, typename = std::enable_if_t<isOptionType<std::decay_t<T>>>>
    StreamSendOptions(T first, U second, Rest... rest)
    {
        setOption(first);
        setOption(second);
        (setOption(rest), ...);
    }

    template<typename... Overrides>
    StreamSendOptions(const StreamSendOptions& other, Overrides... overrides)
        : StreamSendOptions(other)
    {
        setOption(overrides...);
    }

    StreamSendOptions() = default;
    StreamSendOptions(const StreamSendOptions&) = default;
    StreamSendOptions& operator=(const StreamSendOptions&) = default;
    StreamSendOptions(StreamSendOptions&&) = default;
    StreamSendOptions& operator=(StreamSendOptions&&) = default;

    std::optional<Timeout> timeout() const { return m_timeout; }

private:
    void setOption(Timeout timeout) { m_timeout = timeout; }
    void setOption(Seconds timeout) { m_timeout = timeout; }

    std::optional<Timeout> m_timeout;
};

class StreamSendSyncOptions {
public:
    template<typename Type> static constexpr bool isOptionType =
        std::is_same_v<Type, Timeout>
        || std::is_same_v<Type, Seconds>;

    template<typename T>
    StreamSendSyncOptions(std::initializer_list<T> options)
    {
        for (auto option : options)
            setOption(option);
    }

    template<typename T, typename U, typename... Rest, typename = std::enable_if_t<isOptionType<std::decay_t<T>>>>
    StreamSendSyncOptions(T first, U second, Rest... rest)
    {
        setOption(first);
        setOption(second);
        (setOption(rest), ...);
    }

    template<typename... Overrides>
    StreamSendSyncOptions(const StreamSendSyncOptions& other, Overrides... overrides)
        : StreamSendSyncOptions(other)
    {
        (setOption(overrides), ...);
    }

    StreamSendSyncOptions() = default;
    StreamSendSyncOptions(const StreamSendSyncOptions&) = default;
    StreamSendSyncOptions& operator=(const StreamSendSyncOptions&) = default;
    StreamSendSyncOptions(StreamSendSyncOptions&&) = default;
    StreamSendSyncOptions& operator=(StreamSendSyncOptions&&) = default;

    std::optional<Timeout> timeout() const { return m_timeout; }

private:
    void setOption(Timeout timeout) { m_timeout = timeout; }
    void setOption(Seconds timeout) { m_timeout = timeout; }

    std::optional<Timeout> m_timeout;
};

class StreamWaitForOptions {
public:
    template<typename Type> static constexpr bool isOptionType =
        std::is_same_v<Type, WaitForOption>
        || std::is_same_v<Type, OptionSet<WaitForOption>>
        || std::is_same_v<Type, Timeout>
        || std::is_same_v<Type, Seconds>;


    template<typename T>
    StreamWaitForOptions(std::initializer_list<T> options)
    {
        for (auto option : options)
            setOption(option);
    }

    template<typename T, typename U, typename... Rest, typename = std::enable_if_t<isOptionType<std::decay_t<T>>>>
    StreamWaitForOptions(T first, U second, Rest... rest)
    {
        setOption(first);
        setOption(second);
        (setOption(rest), ...);
    }

    template<typename... Overrides>
    StreamWaitForOptions(const StreamWaitForOptions& other, Overrides... overrides)
        : StreamWaitForOptions(other)
    {
        (setOption(overrides), ...);
    }

    StreamWaitForOptions() = default;
    StreamWaitForOptions(const StreamWaitForOptions&) = default;
    StreamWaitForOptions& operator=(const StreamWaitForOptions&) = default;
    StreamWaitForOptions(StreamWaitForOptions&&) = default;
    StreamWaitForOptions& operator=(StreamWaitForOptions&&) = default;

    std::optional<Timeout> timeout() const { return m_timeout; }
    OptionSet<WaitForOption> flags() const { return m_flags; }

private:
    void setOption(Timeout timeout) { m_timeout = timeout; }
    void setOption(Seconds timeout) { m_timeout = timeout; }
    void setOption(OptionSet<WaitForOption> flags) { m_flags = flags; }
    void setOption(WaitForOption flag) { m_flags.add(flag); }

    std::optional<Timeout> m_timeout;
    OptionSet<WaitForOption> m_flags;
};

}
