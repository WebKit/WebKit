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

#include "Logging.h"

#include "WebExtensionContextParameters.h"
#include "WebExtensionContextProxyMessages.h"
#include "WebExtensionController.h"
#include "WebPageProxy.h"
#include <WebCore/LocalizedStrings.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

int WebExtensionContext::toAPIError(WebExtensionContext::Error error)
{
    switch (error) {
    case WebExtensionContext::Error::Unknown:
        return static_cast<int>(WebExtensionContext::APIError::Unknown);
    case WebExtensionContext::Error::AlreadyLoaded:
        return static_cast<int>(WebExtensionContext::APIError::AlreadyLoaded);
    case WebExtensionContext::Error::NotLoaded:
        return static_cast<int>(WebExtensionContext::APIError::NotLoaded);
    case WebExtensionContext::Error::BaseURLAlreadyInUse:
        return static_cast<int>(WebExtensionContext::APIError::BaseURLAlreadyInUse);
    case WebExtensionContext::Error::NoBackgroundContent:
        return static_cast<int>(WebExtensionContext::APIError::NoBackgroundContent);
    case WebExtensionContext::Error::BackgroundContentFailedToLoad:
        return static_cast<int>(WebExtensionContext::APIError::BackgroundContentFailedToLoad);
    }

    ASSERT_NOT_REACHED();
    return static_cast<int>(WebExtensionContext::APIError::Unknown);
}

Ref<API::Error> WebExtensionContext::createError(Error error, const String& customLocalizedDescription, RefPtr<API::Error> underlyingError)
{
    auto errorCode = toAPIError(error);
    String localizedDescription;

    switch (error) {
    case Error::Unknown:
        localizedDescription = WEB_UI_STRING_KEY("An unknown error has occurred.", "An unknown error has occurred. (WKWebExtensionContext)", "WKWebExtensionContextErrorUnknown description");
        break;

    case Error::AlreadyLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is already loaded.", "WKWebExtensionContextErrorAlreadyLoaded description");
        break;

    case Error::NotLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is not loaded.", "WKWebExtensionContextErrorNotLoaded description");
        break;

    case Error::BaseURLAlreadyInUse:
        localizedDescription = WEB_UI_STRING("Another extension context is loaded with the same base URL.", "WKWebExtensionContextErrorBaseURLAlreadyInUse description");
        break;

    case Error::NoBackgroundContent:
        localizedDescription = WEB_UI_STRING("No background content is available to load.", "WKWebExtensionContextErrorNoBackgroundContent description");
        break;

    case Error::BackgroundContentFailedToLoad:
        localizedDescription = WEB_UI_STRING("The background content failed to load due to an error.", "WKWebExtensionContextErrorBackgroundContentFailedToLoad description");
        break;
    }

    if (!customLocalizedDescription.isEmpty())
        localizedDescription = customLocalizedDescription;

    return API::Error::create({ "WKWebExtensionContextErrorDomain"_s, errorCode, { }, localizedDescription }, underlyingError);
}

Vector<Ref<API::Error>> WebExtensionContext::errors()
{
    auto array = protectedExtension()->errors();
    array.appendVector(m_errors);
    return array;
}

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

WebExtensionContextIdentifier WebExtensionContext::privilegedIdentifier() const
{
    if (!m_privilegedIdentifier)
        m_privilegedIdentifier = WebExtensionContextIdentifier::generate();
    return *m_privilegedIdentifier;
}

bool WebExtensionContext::isPrivilegedMessage(IPC::Decoder& message) const
{
    if (!m_privilegedIdentifier)
        return false;
    return m_privilegedIdentifier.value().toRawValue() == message.destinationID();
}

WebExtensionContextParameters WebExtensionContext::parameters(IncludePrivilegedIdentifier includePrivilegedIdentifier) const
{
    RefPtr extension = m_extension;

    return {
        identifier(),
        includePrivilegedIdentifier == IncludePrivilegedIdentifier::Yes ? std::optional(privilegedIdentifier()) : std::nullopt,
        baseURL(),
        uniqueIdentifier(),
        unsupportedAPIs(),
        m_grantedPermissions,
        extension->serializeLocalization(),
        extension->serializeManifest(),
        extension->manifestVersion(),
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

WebExtensionContext::WebProcessProxySet WebExtensionContext::processes(EventListenerTypeSet&& typeSet, ContentWorldTypeSet&& contentWorldTypeSet, Function<bool(WebProcessProxy&, WebPageProxy&, WebFrameProxy&)>&& predicate) const
{
    if (!isLoaded())
        return { };

#if ENABLE(INSPECTOR_EXTENSIONS)
    // Inspector content world is a special alias of Main. Include it when Main is requested (and vice versa).
    if (contentWorldTypeSet.contains(WebExtensionContentWorldType::Main))
        contentWorldTypeSet.add(WebExtensionContentWorldType::Inspector);
    else if (contentWorldTypeSet.contains(WebExtensionContentWorldType::Inspector))
        contentWorldTypeSet.add(WebExtensionContentWorldType::Main);
#endif

    WebProcessProxySet result;

    for (auto type : typeSet) {
        for (auto contentWorldType : contentWorldTypeSet) {
            auto pagesEntry = m_eventListenerFrames.find({ type, contentWorldType });
            if (pagesEntry == m_eventListenerFrames.end())
                continue;

            for (auto entry : pagesEntry->value) {
                Ref frame = entry.key;
                RefPtr page = frame->page();
                if (!page)
                    continue;

                if (!hasAccessToPrivateData() && page->sessionID().isEphemeral())
                    continue;

                Ref webProcess = frame->process();
                if (predicate && !predicate(webProcess, *page, frame))
                    continue;

                if (webProcess->canSendMessage())
                    result.add(webProcess);
            }
        }
    }

    return result;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
