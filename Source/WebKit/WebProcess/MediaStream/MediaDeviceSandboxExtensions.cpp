/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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


namespace WebKit {

MediaDeviceSandboxExtensions::MediaDeviceSandboxExtensions(Vector<String> ids, Vector<SandboxExtension::Handle>&& handles, SandboxExtension::Handle&& machBootstrapHandle)
    : m_ids(ids)
    , m_handles(WTF::move(handles))
    , m_machBootstrapHandle(WTF::move(machBootstrapHandle))
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_ids.size() == m_handles.size());
}

std::pair<String, Ref<SandboxExtension>> MediaDeviceSandboxExtensions::operator[](size_t i)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_ids.size() == m_handles.size());
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(i < m_ids.size());
    return { m_ids[i], SandboxExtension::create(WTF::move(m_handles[i])).releaseNonNull() };
}

size_t MediaDeviceSandboxExtensions::size() const
{
    return m_ids.size();
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
