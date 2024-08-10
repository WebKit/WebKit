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

#include "MediaKeySystemPermissionRequestProxy.h"
#include <WebCore/SecurityOrigin.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class MediaKeySystemPermissionRequestManagerProxy;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::MediaKeySystemPermissionRequestManagerProxy> : std::true_type { };
}

namespace WebCore {
class SecurityOrigin;
};

namespace WebKit {

class WebPageProxy;

class MediaKeySystemPermissionRequestManagerProxy : public CanMakeWeakPtr<MediaKeySystemPermissionRequestManagerProxy> {
    WTF_MAKE_TZONE_ALLOCATED(MediaKeySystemPermissionRequestManagerProxy);
public:
    explicit MediaKeySystemPermissionRequestManagerProxy(WebPageProxy&);
    ~MediaKeySystemPermissionRequestManagerProxy();

    WebPageProxy& page() const { return m_page; }

    void invalidatePendingRequests();

    Ref<MediaKeySystemPermissionRequestProxy> createRequestForFrame(WebCore::MediaKeySystemRequestIdentifier, WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, const String& keySystem);

    void grantRequest(MediaKeySystemPermissionRequestProxy&);
    void denyRequest(MediaKeySystemPermissionRequestProxy&, const String& message = { });

private:
#if !RELEASE_LOG_DISABLED
    const Logger& logger() const;
    const void* logIdentifier() const { return m_logIdentifier; }
#endif

    WebPageProxy& m_page;

    HashMap<WebCore::MediaKeySystemRequestIdentifier, RefPtr<MediaKeySystemPermissionRequestProxy>> m_pendingRequests;
    HashSet<String> m_validAuthorizationTokens;

#if !RELEASE_LOG_DISABLED
    const void* m_logIdentifier;
#endif
};

} // namespace WebKit
