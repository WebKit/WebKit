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
#include "WebExtensionController.h"
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
{
    ASSERT(!get(identifier()));
    webExtensionContexts().add(identifier(), *this);
}

WebExtensionContextParameters WebExtensionContext::parameters() const
{
    return {
        identifier(),
        baseURL(),
        uniqueIdentifier(),
        unsupportedAPIs(),
        m_grantedPermissions,
        extension().serializeLocalization(),
        extension().serializeManifest(),
        extension().manifestVersion(),
        isSessionStorageAllowedInContentScripts(),
        backgroundPageIdentifier(),
#if ENABLE(INSPECTOR_EXTENSIONS)
        inspectorPageIdentifiers(),
        inspectorBackgroundPageIdentifiers(),
#endif
        popupPageIdentifiers(),
        tabPageIdentifiers()
    };
}

bool WebExtensionContext::inTestingMode() const
{
    return m_extensionController && m_extensionController->inTestingMode();
}

const WebExtensionContext::UserContentControllerProxySet& WebExtensionContext::userContentControllers() const
{
    ASSERT(isLoaded());

    if (hasAccessToPrivateData())
        return extensionController()->allUserContentControllers();
    return extensionController()->allNonPrivateUserContentControllers();
}

bool WebExtensionContext::pageListensForEvent(const WebPageProxy& page, WebExtensionEventListenerType type, WebExtensionContentWorldType contentWorldType) const
{
    if (!isLoaded())
        return false;

    if (!hasAccessToPrivateData() && page.sessionID().isEphemeral())
        return false;

    auto findAndCheckPage = [&](WebExtensionContentWorldType worldType) {
        auto entry = m_eventListenerPages.find({ type, worldType });
        return entry != m_eventListenerPages.end() && entry->value.contains(page);
    };

    bool found = findAndCheckPage(contentWorldType);

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (!found) {
        // Inspector content world is a special alias of Main. Check it when Main is requested (and vice versa).
        found = findAndCheckPage(contentWorldType == WebExtensionContentWorldType::Main ? WebExtensionContentWorldType::Inspector : WebExtensionContentWorldType::Main);
    }
#endif

    if (!found)
        return false;

    return page.legacyMainFrameProcess().canSendMessage();
}

WebExtensionContext::WebProcessProxySet WebExtensionContext::processes(EventListenerTypeSet&& typeSet, ContentWorldTypeSet&& contentWorldTypeSet) const
{
    WebProcessProxySet result;

    if (!isLoaded())
        return result;

#if ENABLE(INSPECTOR_EXTENSIONS)
    // Inspector content world is a special alias of Main. Include it when Main is requested (and vice versa).
    if (contentWorldTypeSet.contains(WebExtensionContentWorldType::Main))
        contentWorldTypeSet.add(WebExtensionContentWorldType::Inspector);
    else if (contentWorldTypeSet.contains(WebExtensionContentWorldType::Inspector))
        contentWorldTypeSet.add(WebExtensionContentWorldType::Main);
#endif

    for (auto type : typeSet) {
        for (auto contentWorldType : contentWorldTypeSet) {
            auto pagesEntry = m_eventListenerPages.find({ type, contentWorldType });
            if (pagesEntry == m_eventListenerPages.end())
                continue;

            for (auto entry : pagesEntry->value) {
                if (!hasAccessToPrivateData() && entry.key.sessionID().isEphemeral())
                    continue;

                Ref process = entry.key.legacyMainFrameProcess();
                if (process->canSendMessage())
                    result.add(WTFMove(process));
            }
        }
    }

    return result;
}

WebExtensionMessagePort::~WebExtensionMessagePort()
{
    remove();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
