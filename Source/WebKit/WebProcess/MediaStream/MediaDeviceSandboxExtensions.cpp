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

#include "config.h"
#include "MediaDeviceSandboxExtensions.h"

#if ENABLE(MEDIA_STREAM)

#include "WebCoreArgumentCoders.h"

namespace WebKit {

MediaDeviceSandboxExtensions::MediaDeviceSandboxExtensions(Vector<String> ids, SandboxExtension::HandleArray&& handles)
    : m_ids(ids)
    , m_handles(WTFMove(handles))
{
    ASSERT_WITH_SECURITY_IMPLICATION(m_ids.size() == m_handles.size());
}

void MediaDeviceSandboxExtensions::encode(IPC::Encoder& encoder) const
{
    encoder << m_ids;
    m_handles.encode(encoder);
}

bool MediaDeviceSandboxExtensions::decode(IPC::Decoder& decoder, MediaDeviceSandboxExtensions& result)
{
    if (!decoder.decode(result.m_ids))
        return false;

    std::optional<SandboxExtension::HandleArray> handles;
    decoder >> handles;
    if (!handles)
        return false;
    result.m_handles = WTFMove(*handles);

    return true;
}

std::pair<String, RefPtr<SandboxExtension>> MediaDeviceSandboxExtensions::operator[](size_t i)
{
    ASSERT_WITH_SECURITY_IMPLICATION(m_ids.size() == m_handles.size());
    ASSERT_WITH_SECURITY_IMPLICATION(i < m_ids.size());
    return { m_ids[i], SandboxExtension::create(WTFMove(m_handles[i])) };
}

size_t MediaDeviceSandboxExtensions::size() const
{
    return m_ids.size();
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
