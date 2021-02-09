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

#include "config.h"
#include "MediaKeySystemPermissionRequestProxy.h"

#include "MediaKeySystemPermissionRequestManagerProxy.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

MediaKeySystemPermissionRequestProxy::MediaKeySystemPermissionRequestProxy(MediaKeySystemPermissionRequestManagerProxy& manager, uint64_t mediaKeySystemID, FrameIdentifier mainFrameID, FrameIdentifier frameID, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin, const String& keySystem)
    : m_manager(makeWeakPtr(manager))
    , m_mediaKeySystemID(mediaKeySystemID)
    , m_mainFrameID(mainFrameID)
    , m_frameID(frameID)
    , m_topLevelDocumentSecurityOrigin(WTFMove(topLevelDocumentOrigin))
    , m_keySystem(keySystem)
{
}

void MediaKeySystemPermissionRequestProxy::allow()
{
    ASSERT(m_manager);
    if (!m_manager)
        return;

    m_manager->grantRequest(*this);
    invalidate();
}

void MediaKeySystemPermissionRequestProxy::deny()
{
    if (!m_manager)
        return;

    m_manager->denyRequest(*this);
    invalidate();
}

void MediaKeySystemPermissionRequestProxy::invalidate()
{
    m_manager = nullptr;
}

void MediaKeySystemPermissionRequestProxy::doDefaultAction()
{
#if PLATFORM(COCOA)
    // Backward compatibility for platforms not yet supporting the MediaKeySystem permission request
    // in their APIUIClient.
    allow();
#else
    deny();
#endif
}

} // namespace WebKit
