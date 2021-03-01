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
#include <WebCore/FrameIdentifier.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class MediaKeySystemPermissionRequestManagerProxy;

class MediaKeySystemPermissionRequestProxy : public RefCounted<MediaKeySystemPermissionRequestProxy> {
public:
    static Ref<MediaKeySystemPermissionRequestProxy> create(MediaKeySystemPermissionRequestManagerProxy& manager, uint64_t mediaKeySystemID, WebCore::FrameIdentifier mainFrameID, WebCore::FrameIdentifier frameID, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, const String& keySystem)
    {
        return adoptRef(*new MediaKeySystemPermissionRequestProxy(manager, mediaKeySystemID, mainFrameID, frameID, WTFMove(topLevelDocumentOrigin), keySystem));
    }

    void allow();
    void deny();

    void invalidate();

    uint64_t mediaKeySystemID() const { return m_mediaKeySystemID; }
    WebCore::FrameIdentifier mainFrameID() const { return m_mainFrameID; }
    WebCore::FrameIdentifier frameID() const { return m_frameID; }

    WebCore::SecurityOrigin& topLevelDocumentSecurityOrigin() { return m_topLevelDocumentSecurityOrigin.get(); }
    const WebCore::SecurityOrigin& topLevelDocumentSecurityOrigin() const { return m_topLevelDocumentSecurityOrigin.get(); }

    const String& keySystem() const { return m_keySystem; }

    void doDefaultAction();

private:
    MediaKeySystemPermissionRequestProxy(MediaKeySystemPermissionRequestManagerProxy&, uint64_t mediaKeySystemID, WebCore::FrameIdentifier mainFrameID, WebCore::FrameIdentifier, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, const String& keySystem);

    WeakPtr<MediaKeySystemPermissionRequestManagerProxy> m_manager;
    uint64_t m_mediaKeySystemID;
    WebCore::FrameIdentifier m_mainFrameID;
    WebCore::FrameIdentifier m_frameID;
    Ref<WebCore::SecurityOrigin> m_topLevelDocumentSecurityOrigin;
    String m_keySystem;
};

} // namespace WebKit
