/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include <wtf/URL.h>

namespace WebCore {

class BlobPart {
public:
    enum class Type : bool {
        Data,
        Blob
    };

    BlobPart()
        : m_type(Type::Data)
    {
    }

    BlobPart(Vector<uint8_t>&& data)
        : m_type(Type::Data)
        , m_data(WTFMove(data))
    {
    }

    BlobPart(const URL& url)
        : m_type(Type::Blob)
        , m_url(url)
    {
    }

    Type type() const { return m_type; }

    const Vector<uint8_t>& data() const
    {
        ASSERT(m_type == Type::Data);
        return m_data;
    }

    Vector<uint8_t> moveData()
    {
        ASSERT(m_type == Type::Data);
        return WTFMove(m_data);
    }

    const URL& url() const
    {
        ASSERT(m_type == Type::Blob);
        return m_url;
    }

    void detachFromCurrentThread()
    {
        m_url = m_url.isolatedCopy();
    }

private:
    Type m_type;
    Vector<uint8_t> m_data;
    URL m_url;
};

} // namespace WebCore
