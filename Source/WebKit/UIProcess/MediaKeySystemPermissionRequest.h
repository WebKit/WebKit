/*
 * Copyright (C) 2021 Igalia S.L.
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

#include "APIObject.h"
#include <WebCore/ClientOrigin.h>
#include <wtf/CompletionHandler.h>

namespace WebKit {

class MediaKeySystemPermissionRequest : public RefCounted<MediaKeySystemPermissionRequest> {
public:
    static Ref<MediaKeySystemPermissionRequest> create(const String& keySystem, CompletionHandler<void(bool)>&& completionHandler)
    {
        return adoptRef(*new MediaKeySystemPermissionRequest(keySystem, WTFMove(completionHandler)));
    }

    void complete(bool success)
    {
        auto completionHandler = std::exchange(m_completionHandler, { });
        completionHandler(success);
    }

    const String& keySystem() const { return m_keySystem; }

private:
    MediaKeySystemPermissionRequest(const String& keySystem, CompletionHandler<void(bool)>&& completionHandler)
        : m_keySystem(keySystem)
        , m_completionHandler(WTFMove(completionHandler))
    { }

    String m_keySystem;
    CompletionHandler<void(bool)> m_completionHandler;
};

class MediaKeySystemPermissionCallback : public API::ObjectImpl<API::Object::Type::MediaKeySystemPermissionCallback> {
public:
    static Ref<MediaKeySystemPermissionCallback> create(CompletionHandler<void(bool)>&& completionHandler)
    {
        return adoptRef(*new MediaKeySystemPermissionCallback(WTFMove(completionHandler)));
    }

    void complete(bool granted) { m_completionHandler(granted); }

private:
    MediaKeySystemPermissionCallback(CompletionHandler<void(bool)>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    { }

    CompletionHandler<void(bool)> m_completionHandler;
};

} // namespace WebKit
