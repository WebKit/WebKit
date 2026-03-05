/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "MessageNames.h"
#include <WebKit/WKDeclarationSpecifiers.h>
#include <array>
#include <atomic>
#include <wtf/ExportMacros.h>
#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>

namespace IPC {

inline constexpr size_t messageLogCapacity = 256;

template<size_t Capacity>
class MessageLog {
    WTF_MAKE_NONCOPYABLE(MessageLog);
    static_assert(hasOneBitSet(Capacity), "Capacity must be a power of two to handle size_t overflow correctly");
public:
    constexpr MessageLog()
    {
        m_buffer.fill(MessageName::Invalid);
    }

    void add(MessageName messageName)
    {
        size_t index = m_index.fetch_add(1, std::memory_order_relaxed);
        m_buffer[index % Capacity] = messageName;
    }

    size_t indexForTesting() const { return m_index.load(std::memory_order_relaxed); }
    const std::array<MessageName, Capacity>& bufferForTesting() const { return m_buffer; }

private:
    std::atomic<size_t> m_index { 0 };
    std::array<MessageName, Capacity> m_buffer;
};

// Exported information to help a debugger read the data
struct MessageLogMetadata {
    size_t version;
    size_t capacity;
    size_t elementSize;
    size_t size;
    MessageName initialValue;
};

WK_EXPORT MessageLog<messageLogCapacity>& NODELETE messageLog();
WK_EXPORT const MessageLogMetadata& NODELETE messageLogMetadata();

} // namespace IPC
