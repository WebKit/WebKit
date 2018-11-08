/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UserMediaPermissionCheckProxy.h"

#include "UserMediaPermissionRequestManagerProxy.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>

namespace WebKit {
using namespace WebCore;

UserMediaPermissionCheckProxy::UserMediaPermissionCheckProxy(uint64_t userMediaID, uint64_t frameID, CompletionHandler&& handler, Ref<WebCore::SecurityOrigin>&& userMediaDocumentOrigin, Ref<WebCore::SecurityOrigin>&& topLevelDocumentOrigin)
    : m_userMediaID(userMediaID)
    , m_frameID(frameID)
    , m_completionHandler(WTFMove(handler))
    , m_userMediaDocumentSecurityOrigin(WTFMove(userMediaDocumentOrigin))
    , m_topLevelDocumentSecurityOrigin(WTFMove(topLevelDocumentOrigin))
{
}

void UserMediaPermissionCheckProxy::setUserMediaAccessInfo(bool allowed)
{
    ASSERT(m_completionHandler);
    if (!m_completionHandler)
        return;

    m_completionHandler(m_userMediaID, allowed);
    m_completionHandler = nullptr;
}

void UserMediaPermissionCheckProxy::invalidate()
{
    m_completionHandler = nullptr;
}

} // namespace WebKit

