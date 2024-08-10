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

#if ENABLE(ENCRYPTED_MEDIA)

#include "SandboxExtension.h"
#include <WebCore/MediaCanStartListener.h>
#include <WebCore/MediaKeySystemClient.h>
#include <WebCore/MediaKeySystemRequest.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class WebPage;

class MediaKeySystemPermissionRequestManager : private WebCore::MediaCanStartListener {
    WTF_MAKE_TZONE_ALLOCATED(MediaKeySystemPermissionRequestManager);
public:
    explicit MediaKeySystemPermissionRequestManager(WebPage&);
    ~MediaKeySystemPermissionRequestManager() = default;

    void startMediaKeySystemRequest(WebCore::MediaKeySystemRequest&);
    void cancelMediaKeySystemRequest(WebCore::MediaKeySystemRequest&);
    void mediaKeySystemWasGranted(WebCore::MediaKeySystemRequestIdentifier);
    void mediaKeySystemWasDenied(WebCore::MediaKeySystemRequestIdentifier, String&&);

private:
    void sendMediaKeySystemRequest(WebCore::MediaKeySystemRequest&);

    // WebCore::MediaCanStartListener
    void mediaCanStart(WebCore::Document&) final;

    WebPage& m_page;

    HashMap<WebCore::MediaKeySystemRequestIdentifier, Ref<WebCore::MediaKeySystemRequest>> m_ongoingMediaKeySystemRequests;
    HashMap<RefPtr<WebCore::Document>, Vector<Ref<WebCore::MediaKeySystemRequest>>> m_pendingMediaKeySystemRequests;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
