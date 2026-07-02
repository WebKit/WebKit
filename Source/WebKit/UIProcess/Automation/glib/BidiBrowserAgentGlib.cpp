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
#if ENABLE(2022_GLIB_API)
#include "WebKitNetworkSession.h"
#endif
#include "WebKitWebContextPrivate.h"
#include "WebKitWebsiteDataManagerPrivate.h"

namespace WebKit {

using namespace Inspector;

std::unique_ptr<BidiUserContext> BidiBrowserAgent::platformCreateUserContext(String& error)
{
#if ENABLE(2022_GLIB_API)
    GRefPtr<WebKitNetworkSession> networkSession = adoptGRef(webkit_network_session_new_ephemeral());
    GRefPtr<WebKitWebsiteDataManager> dataManager = webkit_network_session_get_website_data_manager(networkSession.get());
#else
    GRefPtr<WebKitWebsiteDataManager> dataManager = adoptGRef(webkit_website_data_manager_new_ephemeral());
#endif

    GRefPtr<WebKitWebContext> context = adoptGRef(WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT,
#if !ENABLE(2022_GLIB_API)
        "website-data-manager", dataManager.get(),
#endif
    // WPE has PSON enabled by default and doesn't have such parameter.
#if PLATFORM(GTK)
#if !ENABLE(2022_GLIB_API)
        "process-swap-on-cross-site-navigation-enabled", true,
#endif
#endif
        nullptr)));
    if (!context) {
        error = "Failed to create GLib ephemeral context"_s;
        return nullptr;
    }

    return makeUnique<BidiUserContext>(webkitWebsiteDataManagerGetDataStore(dataManager.get()), webkitWebContextGetProcessPool(context.get()), WTF::move(context));
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

