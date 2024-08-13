/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011,2017 Igalia S.L.
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

#include "Attachment.h"
#include "Encoder.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace IPC {

class MessageInfo {
public:
    MessageInfo()
    {
        // The entire MessageInfo is passed to write(), so we have to zero our
        // padding bytes to avoid writing uninitialized memory.
        memset(static_cast<void*>(this), 0, sizeof(*this));
    }

    MessageInfo(size_t bodySize, size_t initialAttachmentCount)
    {
        memset(static_cast<void*>(this), 0, sizeof(*this));
        m_bodySize = bodySize;
        m_attachmentCount = initialAttachmentCount;
    }

    MessageInfo(const MessageInfo& info)
    {
        memset(static_cast<void*>(this), 0, sizeof(*this));
        *this = info;
    }

    MessageInfo& operator=(const MessageInfo&) = default;

    void setBodyOutOfLine()
    {
        ASSERT(!isBodyOutOfLine());

        m_isBodyOutOfLine = true;
        m_attachmentCount++;
    }

    bool isBodyOutOfLine() const { return m_isBodyOutOfLine; }
    size_t bodySize() const { return m_bodySize; }
    size_t attachmentCount() const { return m_attachmentCount; }

private:
    // The MessageInfo will be copied using memcpy, so all members must be trivially copyable.
    size_t m_bodySize;
    size_t m_attachmentCount;
    bool m_isBodyOutOfLine;
};

class UnixMessage {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(UnixMessage);
public:
    UnixMessage(Encoder& encoder)
        : m_attachments(encoder.releaseAttachments())
        , m_messageInfo(encoder.span().size(), m_attachments.size())
        , m_body(const_cast<uint8_t*>(encoder.span().data()), encoder.span().size())
    {
    }

    UnixMessage(UnixMessage&& other)
        : m_attachments(WTFMove(other.m_attachments))
        , m_messageInfo(WTFMove(other.m_messageInfo))
    {
        if (other.m_bodyOwned) {
            std::swap(m_body, other.m_body);
            std::swap(m_bodyOwned, other.m_bodyOwned);
        } else if (!m_messageInfo.isBodyOutOfLine()) {
            m_body = std::span { static_cast<uint8_t*>(fastMalloc(m_messageInfo.bodySize())), m_messageInfo.bodySize() };
            memcpySpan(m_body, other.m_body);
            m_bodyOwned = true;
            other.m_body = { };
            other.m_bodyOwned = false;
        }
    }

    ~UnixMessage()
    {
        if (m_bodyOwned)
            fastFree(m_body.data());
    }

    const Vector<Attachment>& attachments() const { return m_attachments; }
    MessageInfo& messageInfo() { return m_messageInfo; }

    std::span<uint8_t> body() const { return m_body; }
    size_t bodySize() const  { return m_messageInfo.bodySize(); }

    void appendAttachment(Attachment&& attachment)
    {
        m_attachments.append(WTFMove(attachment));
    }

private:
    Vector<Attachment> m_attachments;
    MessageInfo m_messageInfo;
    std::span<uint8_t> m_body;
    bool m_bodyOwned { false };
};

} // namespace IPC
