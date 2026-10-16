/*
 * Copyright (C) 2025 Microsoft Corporation. All rights reserved.
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
#include "BidiBrowserAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "BidiUserContext.h"
#include "WebAutomationSession.h"
#if ENABLE(2022_GLIB_API)
#include "WebKitNetworkSession.h"
#endif
#include "WebKitWebsiteDataManager.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include "WebProcessPool.h"

namespace WebKit {

using namespace Inspector;

std::unique_ptr<BidiUserContext> BidiBrowserAgent::platformCreateUserContext(String& error)
{
    // Pages of a user context live in the session's own web context and process pool, with their own
    // ephemeral storage, so that the session sees them like any other automation page.
    RefPtr session = m_session.get();
    RefPtr processPool = session ? session->processPool() : nullptr;
    if (!processPool) {
        error = "The automation session has no process pool"_s;
        return nullptr;
    }

#if ENABLE(2022_GLIB_API)
    GRefPtr<WebKitNetworkSession> networkSession = adoptGRef(webkit_network_session_new_ephemeral());
    auto& dataStore = webkitWebsiteDataManagerGetDataStore(webkit_network_session_get_website_data_manager(networkSession.get()));
    return makeUnique<BidiUserContext>(dataStore, *processPool, WTF::move(networkSession));
#else
    GRefPtr<WebKitWebsiteDataManager> dataManager = adoptGRef(webkit_website_data_manager_new_ephemeral());
    auto& dataStore = webkitWebsiteDataManagerGetDataStore(dataManager.get());
    return makeUnique<BidiUserContext>(dataStore, *processPool, WTF::move(dataManager));
#endif
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

