/*
 * Copyright (C) 2012 Igalia S.L.
 * Copyright (C) 2013 Company 100 Inc.
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
#include "WebFrameNetworkingContext.h"

#include "NetworkSession.h"
#include "NetworkSessionCreationParameters.h"
#include "SessionTracker.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/FrameLoader.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Settings.h>
#include <WebCore/SoupNetworkSession.h>
#include <pal/SessionID.h>

namespace WebKit {
using namespace WebCore;

void WebFrameNetworkingContext::ensureWebsiteDataStoreSession(WebsiteDataStoreParameters&& parameters)
{
    auto sessionID = parameters.networkSessionParameters.sessionID;
    ASSERT(RunLoop::isMain());

    if (NetworkStorageSession::storageSession(sessionID))
        return;

    NetworkStorageSession::ensureSession(sessionID, String::number(sessionID.sessionID()));
}

WebFrameNetworkingContext::WebFrameNetworkingContext(WebFrame* frame)
    : FrameNetworkingContext(frame->coreFrame())
{
}

NetworkStorageSession& WebFrameNetworkingContext::storageSession() const
{
    if (frame()) {
        if (auto* storageSession = NetworkStorageSession::storageSession(frame()->page()->sessionID()))
            return *storageSession;
    }
    return NetworkStorageSession::defaultStorageSession();
}

WebFrameLoaderClient* WebFrameNetworkingContext::webFrameLoaderClient() const
{
    if (!frame())
        return nullptr;

    return toWebFrameLoaderClient(frame()->loader().client());
}

}
