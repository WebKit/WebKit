/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#include "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionContextParameters.h"
#include "WebExtensionContextProxyMessages.h"
#include "WebPageProxy.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

static HashMap<WebExtensionContextIdentifier, WeakRef<WebExtensionContext>>& webExtensionContexts()
{
    static NeverDestroyed<HashMap<WebExtensionContextIdentifier, WeakRef<WebExtensionContext>>> contexts;
    return contexts;
}

WebExtensionContext* WebExtensionContext::get(WebExtensionContextIdentifier identifier)
{
    return webExtensionContexts().get(identifier);
}

WebExtensionContext::WebExtensionContext()
    : m_identifier(WebExtensionContextIdentifier::generate())
{
    ASSERT(!get(m_identifier));
    webExtensionContexts().add(m_identifier, *this);
}

WebExtensionContextParameters WebExtensionContext::parameters() const
{
    return {
        identifier(),
        baseURL(),
        uniqueIdentifier(),
        extension().serializeLocalization(),
        extension().serializeManifest(),
        extension().manifestVersion(),
        inTestingMode(),
        isSessionStorageAllowedInContentScripts(),
        backgroundPageIdentifier(),
        popupPageIdentifiers(),
        tabPageIdentifiers()
    };
}

const WebExtensionContext::UserContentControllerProxySet& WebExtensionContext::userContentControllers() const
{
    ASSERT(isLoaded());

    if (hasAccessInPrivateBrowsing())
        return extensionController()->allUserContentControllers();
    return extensionController()->allNonPrivateUserContentControllers();
}

bool WebExtensionContext::pageListensForEvent(const WebPageProxy& page, WebExtensionEventListenerType type, WebExtensionContentWorldType contentWorldType) const
{
    if (!hasAccessInPrivateBrowsing() && page.sessionID().isEphemeral())
        return false;

    auto pagesEntry = m_eventListenerPages.find({ type, contentWorldType });
    if (pagesEntry == m_eventListenerPages.end())
        return false;

    if (!pagesEntry->value.contains(page))
        return false;

    return page.process().canSendMessage();
}

WebExtensionContext::WebProcessProxySet WebExtensionContext::processes(WebExtensionEventListenerType type, WebExtensionContentWorldType contentWorldType) const
{
    return processes(EventListenerTypeSet { type }, contentWorldType);
}

WebExtensionContext::WebProcessProxySet WebExtensionContext::processes(EventListenerTypeSet typeSet, WebExtensionContentWorldType contentWorldType) const
{
    WebProcessProxySet result;

    for (auto type : typeSet) {
        auto pagesEntry = m_eventListenerPages.find({ type, contentWorldType });
        if (pagesEntry == m_eventListenerPages.end())
            continue;

        for (auto entry : pagesEntry->value) {
            if (!hasAccessInPrivateBrowsing() && entry.key.sessionID().isEphemeral())
                continue;

            Ref process = entry.key.process();
            if (process->canSendMessage())
                result.add(WTFMove(process));
        }
    }

    return result;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
