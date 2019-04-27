/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include <mach/message.h>
#include <memory>
#include <wtf/text/CString.h>

namespace IPC {

class MachMessage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<MachMessage> create(CString&& messageReceiverName, CString&& messageName, size_t);
    ~MachMessage();

    static size_t messageSize(size_t bodySize, size_t portDescriptorCount, size_t memoryDescriptorCount);

    size_t size() const { return m_size; }
    mach_msg_header_t* header() { return m_messageHeader; }

    void leakDescriptors();

    const CString& messageReceiverName() const { return m_messageReceiverName; }
    const CString& messageName() const { return m_messageName; }

private:
    MachMessage(CString&& messageReceiverName, CString&& messageName, size_t);

    CString m_messageReceiverName;
    CString m_messageName;
    size_t m_size;
    bool m_shouldFreeDescriptors { true };
    mach_msg_header_t m_messageHeader[];
};

}

#endif // PLATFORM(COCOA)
