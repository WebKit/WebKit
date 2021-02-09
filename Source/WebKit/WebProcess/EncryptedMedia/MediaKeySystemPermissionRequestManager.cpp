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
#include "MediaKeySystemPermissionRequestManager.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "Logging.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>

namespace WebKit {
using namespace WebCore;

MediaKeySystemPermissionRequestManager::MediaKeySystemPermissionRequestManager(WebPage& page)
    : m_page(page)
{
}

void MediaKeySystemPermissionRequestManager::startMediaKeySystemRequest(MediaKeySystemRequest& request)
{
    Document* document = request.document();
    Frame* frame = document ? document->frame() : nullptr;

    if (!frame || !document->page()) {
        request.deny(emptyString());
        return;
    }

    if (document->page()->canStartMedia()) {
        sendMediaKeySystemRequest(request);
        return;
    }

    auto& pendingRequests = m_pendingMediaKeySystemRequests.add(document, Vector<Ref<MediaKeySystemRequest>>()).iterator->value;
    pendingRequests.append(request);
}

void MediaKeySystemPermissionRequestManager::sendMediaKeySystemRequest(MediaKeySystemRequest& userRequest)
{
    auto* frame = userRequest.document() ? userRequest.document()->frame() : nullptr;
    if (!frame) {
        userRequest.deny(emptyString());
        return;
    }

    m_ongoingMediaKeySystemRequests.add(userRequest.identifier(), makeRef(userRequest));

    WebFrame* webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);

    auto* topLevelDocumentOrigin = userRequest.topLevelDocumentOrigin();
    m_page.send(Messages::WebPageProxy::RequestMediaKeySystemPermissionForFrame(userRequest.identifier().toUInt64(), webFrame->frameID(), topLevelDocumentOrigin->data(), userRequest.keySystem()));
}

void MediaKeySystemPermissionRequestManager::cancelMediaKeySystemRequest(MediaKeySystemRequest& request)
{
    if (auto removedRequest = m_ongoingMediaKeySystemRequests.take(request.identifier()))
        return;

    auto* document = request.document();
    if (!document)
        return;

    auto iterator = m_pendingMediaKeySystemRequests.find(document);
    if (iterator == m_pendingMediaKeySystemRequests.end())
        return;

    auto& pendingRequests = iterator->value;
    pendingRequests.removeFirstMatching([&request](auto& item) {
        return &request == item.ptr();
    });

    if (!pendingRequests.isEmpty())
        return;

    m_pendingMediaKeySystemRequests.remove(iterator);
}

void MediaKeySystemPermissionRequestManager::mediaKeySystemWasGranted(uint64_t requestID, CompletionHandler<void()>&& completionHandler)
{
    auto request = m_ongoingMediaKeySystemRequests.take(makeObjectIdentifier<MediaKeySystemRequestIdentifierType>(requestID));
    if (!request) {
        completionHandler();
        return;
    }

    request->allow(WTFMove(completionHandler));
}

void MediaKeySystemPermissionRequestManager::mediaKeySystemWasDenied(uint64_t requestID, String&& message)
{
    auto request = m_ongoingMediaKeySystemRequests.take(makeObjectIdentifier<MediaKeySystemRequestIdentifierType>(requestID));
    if (!request)
        return;

    request->deny(WTFMove(message));
}

} // namespace WebKit

#endif // ENABLE(ENCRYPTED_MEDIA)
