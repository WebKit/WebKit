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

#if ENABLE(MEDIA_STREAM)

#include "SandboxExtension.h"
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class MediaDeviceSandboxExtensions {
    WTF_MAKE_NONCOPYABLE(MediaDeviceSandboxExtensions);
public:
    MediaDeviceSandboxExtensions()
    {
    }
    MediaDeviceSandboxExtensions(MediaDeviceSandboxExtensions&&) = default;
    MediaDeviceSandboxExtensions& operator=(MediaDeviceSandboxExtensions&&) = default;

    MediaDeviceSandboxExtensions(Vector<String> ids, Vector<SandboxExtension::Handle>&& handles);

    std::pair<String, RefPtr<SandboxExtension>> operator[](size_t i);
    size_t size() const;

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, MediaDeviceSandboxExtensions&);

private:
    Vector<String> m_ids;
    Vector<SandboxExtension::Handle> m_handles;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
