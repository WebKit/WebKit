/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContextProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "JSWebExtensionWrapper.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionControllerProxy.h"
#import "WebExtensionLocalization.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjectIdentifier.h>
#import <wtf/text/MakeString.h>

namespace WebKit {

static HashMap<WebExtensionContextIdentifier, WeakPtr<WebExtensionContextProxy>>& webExtensionContextProxies()
{
    static MainRunLoopNeverDestroyed<HashMap<WebExtensionContextIdentifier, WeakPtr<WebExtensionContextProxy>>> contexts;
    return contexts;
}

RefPtr<WebExtensionContextProxy> WebExtensionContextProxy::get(WebExtensionContextIdentifier identifier)
{
    return webExtensionContextProxies().get(identifier);
}

WebExtensionContextProxy::WebExtensionContextProxy(const WebExtensionContextParameters& parameters)
    : m_unprivilegedIdentifier(parameters.unprivilegedIdentifier)
{
    ASSERT(!get(m_unprivilegedIdentifier));
    webExtensionContextProxies().add(m_unprivilegedIdentifier, *this);

    WebProcess::singleton().addMessageReceiver(Messages::WebExtensionContextProxy::messageReceiverName(), m_unprivilegedIdentifier, *this);
}

WebExtensionContextProxy::~WebExtensionContextProxy()
{
    webExtensionContextProxies().remove(m_unprivilegedIdentifier);
    WebProcess::singleton().removeMessageReceiver(*this);
}

Ref<WebExtensionContextProxy> WebExtensionContextProxy::getOrCreate(const WebExtensionContextParameters& parameters, WebExtensionControllerProxy& extensionControllerProxy, WebPage* newPage)
{
    auto updateProperties = [&](WebExtensionContextProxy& context) {
        context.m_extensionControllerProxy = extensionControllerProxy;
        context.m_baseURL = parameters.baseURL;
        context.m_uniqueIdentifier = parameters.uniqueIdentifier;
        context.m_unsupportedAPIs = parameters.unsupportedAPIs;
        context.m_grantedPermissions = parameters.grantedPermissions;
        context.m_localization = parseLocalization(parameters.localizationJSON, parameters.baseURL);
        Ref manifestJSON = *parameters.manifestJSON;
        context.m_manifest = parseJSON(manifestJSON);
        context.m_manifestVersion = parameters.manifestVersion;
        context.m_isSessionStorageAllowedInContentScripts = parameters.isSessionStorageAllowedInContentScripts;

        if (parameters.privilegedIdentifier)
            context.m_privilegedIdentifier = parameters.privilegedIdentifier;

        if (parameters.backgroundPageIdentifier) {
            if (newPage && parameters.backgroundPageIdentifier.value() == newPage->identifier())
                context.setBackgroundPage(*newPage);
            else if (RefPtr page = WebProcess::singleton().webPage(parameters.backgroundPageIdentifier.value()))
                context.setBackgroundPage(*page);
        }

        auto processPageIdentifiers = [&context, &newPage](auto& identifiers, auto addPage) {
            for (auto& identifierTuple : identifiers) {
                auto& pageIdentifier = std::get<WebCore::PageIdentifier>(identifierTuple);
                auto& tabIdentifier = std::get<std::optional<WebExtensionTabIdentifier>>(identifierTuple);
                auto& windowIdentifier = std::get<std::optional<WebExtensionWindowIdentifier>>(identifierTuple);

                if (newPage && pageIdentifier == newPage->identifier())
                    addPage(context, *newPage, tabIdentifier, windowIdentifier);
                else if (RefPtr<WebPage> page = WebProcess::singleton().webPage(pageIdentifier))
                    addPage(context, *page, tabIdentifier, windowIdentifier);
            }
        };

#if ENABLE(INSPECTOR_EXTENSIONS)
        processPageIdentifiers(parameters.inspectorBackgroundPageIdentifiers, [](auto& context, auto& page, auto& tabIdentifier, auto& windowIdentifier) {
            context.addInspectorBackgroundPage(page, tabIdentifier, windowIdentifier);
        });
#endif

        processPageIdentifiers(parameters.popupPageIdentifiers, [](auto& context, auto& page, auto& tabIdentifier, auto& windowIdentifier) {
            context.addPopupPage(page, tabIdentifier, windowIdentifier);
        });

        processPageIdentifiers(parameters.tabPageIdentifiers, [](auto& context, auto& page, auto& tabIdentifier, auto& windowIdentifier) {
            context.addTabPage(page, tabIdentifier, windowIdentifier);
        });
    };

    if (RefPtr context = get(parameters.unprivilegedIdentifier)) {
        updateProperties(*context);
        return *context;
    }

    Ref result = adoptRef(*new WebExtensionContextProxy(parameters));
    updateProperties(result);
    return result;
}

bool WebExtensionContextProxy::isUnsupportedAPI(const String& propertyPath, const ASCIILiteral& propertyName) const
{
    auto fullPropertyPath = !propertyPath.isEmpty() ? makeString(propertyPath, '.', propertyName) : propertyName;
    return m_unsupportedAPIs.contains(fullPropertyPath);
}

bool WebExtensionContextProxy::hasPermission(const String& permission) const
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (m_nextGrantedPermissionsExpirationDate != WallTime::nan() && m_nextGrantedPermissionsExpirationDate > currentTime)
        goto finish;

    m_nextGrantedPermissionsExpirationDate = WallTime::infinity();

    m_grantedPermissions.removeIf([&](auto& entry) {
        if (entry.value <= currentTime)
            return true;

        if (entry.value < m_nextGrantedPermissionsExpirationDate)
            m_nextGrantedPermissionsExpirationDate = entry.value;

        return false;
    });

finish:
    return m_grantedPermissions.contains(permission);
}

void WebExtensionContextProxy::updateGrantedPermissions(PermissionsMap&& permissions)
{
    m_grantedPermissions = WTFMove(permissions);
    m_nextGrantedPermissionsExpirationDate = WallTime::nan();
}

RefPtr<WebExtensionLocalization> WebExtensionContextProxy::parseLocalization(RefPtr<API::Data> json, const URL& baseURL)
{
    if (!json)
        return nullptr;

    if (RefPtr value = JSON::Value::parseJSON(String::fromUTF8(json->span()))) {
        if (RefPtr object = value->asObject())
            return WebExtensionLocalization::create(object, baseURL.host().toString());
    }

    return nullptr;
}

Ref<WebCore::DOMWrapperWorld> WebExtensionContextProxy::toDOMWrapperWorld(WebExtensionContentWorldType contentWorldType) const
{
    switch (contentWorldType) {
    case WebExtensionContentWorldType::Main:
    case WebExtensionContentWorldType::WebPage:
#if ENABLE(INSPECTOR_EXTENSIONS)
    case WebExtensionContentWorldType::Inspector:
#endif
        return mainWorldSingleton();
    case WebExtensionContentWorldType::ContentScript:
        return contentScriptWorld();
    case WebExtensionContentWorldType::Native:
        ASSERT_NOT_REACHED();
        return mainWorldSingleton();
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
