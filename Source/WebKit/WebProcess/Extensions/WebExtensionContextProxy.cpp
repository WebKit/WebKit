/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "WebExtensionContextProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPINamespace.h"
#include "JSWebExtensionAPIWebPageNamespace.h"
#include "JSWebExtensionWrapper.h"
#include "MessageSenderInlines.h"
#include "WebExtensionAPINamespace.h"
#include "WebExtensionAPIWebPageNamespace.h"
#include "WebExtensionContextMessages.h"
#include "WebExtensionContextProxyMessages.h"
#include "WebExtensionControllerProxy.h"
#include "WebExtensionLocalization.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Page.h>
#include <wtf/Borrow.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionContextProxy);

static HashMap<WebExtensionContextIdentifier, WeakPtr<WebExtensionContextProxy>>& LEGACY_NODELETE webExtensionContextProxies()
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
        context.m_manifest = JSON::Value::parseJSON(String::fromUTF8(manifestJSON->span()));
        context.m_manifestVersion = parameters.manifestVersion;
        context.m_storageAccessLevels = parameters.storageAccessLevels;
        context.m_defaultSessionID = parameters.defaultSessionID;

        if (parameters.privilegedIdentifier)
            context.m_privilegedIdentifier = parameters.privilegedIdentifier;

        if (parameters.backgroundPageIdentifier) {
            if (newPage && parameters.backgroundPageIdentifier.value() == newPage->identifier())
                context.setBackgroundPage(*newPage);
            else if (RefPtr page = WebProcess::singleton().webPage(parameters.backgroundPageIdentifier.value()))
                context.setBackgroundPage(*page);
        }

        auto processPageIdentifiers = [&context, &newPage](auto& identifiers, auto addPage) {
            for (auto& identifierTuple : borrow(identifiers)) {
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

WebExtensionControllerProxy* WebExtensionContextProxy::extensionControllerProxy() const
{
    return m_extensionControllerProxy.get();
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
    m_grantedPermissions = WTF::move(permissions);
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

void WebExtensionContextProxy::addFrameWithExtensionContent(WebFrame& frame)
{
    m_extensionContentFrames.add(frame);
}

bool WebExtensionContextProxy::isURLForThisExtension(const URL& url) const
{
    return url.isValid() && url.string().startsWith(m_baseURL.string());
}

std::optional<WebExtensionTabIdentifier> WebExtensionContextProxy::tabIdentifier(WebPage& page) const
{
    if (m_popupPageMap.contains(page))
        return std::get<std::optional<WebExtensionTabIdentifier>>(m_popupPageMap.get(page));

    if (m_tabPageMap.contains(page))
        return std::get<std::optional<WebExtensionTabIdentifier>>(m_tabPageMap.get(page));

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (m_inspectorPageMap.contains(page))
        return std::get<std::optional<WebExtensionTabIdentifier>>(m_inspectorPageMap.get(page));

    if (m_inspectorBackgroundPageMap.contains(page))
        return std::get<std::optional<WebExtensionTabIdentifier>>(m_inspectorBackgroundPageMap.get(page));
#endif

    return std::nullopt;
}

bool WebExtensionContextProxy::inTestingMode() const
{
    return m_extensionControllerProxy && m_extensionControllerProxy->inTestingMode();
}

RefPtr<WebPage> WebExtensionContextProxy::backgroundPage() const
{
    return m_backgroundPage.get();
}

void WebExtensionContextProxy::setBackgroundPage(WebPage& page)
{
    m_backgroundPage = page;
}

#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
void WebExtensionContextProxy::setOffscreenPage(WebPage& page)
{
    m_offscreenPage = page;
}

bool WebExtensionContextProxy::isOffscreenPage(WebPage& page) const
{
    return m_offscreenPage.get() == &page;
}
#endif

#if ENABLE(INSPECTOR_EXTENSIONS)
void WebExtensionContextProxy::addInspectorPage(WebPage& page, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    m_inspectorPageMap.set(page, TabWindowIdentifierPair { tabIdentifier, windowIdentifier });
}

void WebExtensionContextProxy::addInspectorPageIdentifier(WebCore::PageIdentifier pageIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    if (RefPtr page = WebProcess::singleton().webPage(pageIdentifier))
        addInspectorPage(*page, tabIdentifier, windowIdentifier);
}

void WebExtensionContextProxy::addInspectorBackgroundPageIdentifier(WebCore::PageIdentifier pageIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    if (RefPtr page = WebProcess::singleton().webPage(pageIdentifier))
        addInspectorBackgroundPage(*page, tabIdentifier, windowIdentifier);
}

void WebExtensionContextProxy::addInspectorBackgroundPage(WebPage& page, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    m_inspectorBackgroundPageMap.set(page, TabWindowIdentifierPair { tabIdentifier, windowIdentifier });
}

bool WebExtensionContextProxy::isInspectorBackgroundPage(WebPage& page) const
{
    return m_inspectorBackgroundPageMap.contains(page);
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

Vector<Ref<WebPage>> WebExtensionContextProxy::popupPages(std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier) const
{
    Vector<Ref<WebPage>> result;

    for (auto entry : m_popupPageMap) {
        if (tabIdentifier && entry.value.first && entry.value.first.value() != tabIdentifier.value())
            continue;

        if (windowIdentifier && entry.value.second && entry.value.second.value() != windowIdentifier.value())
            continue;

        result.append(Ref { entry.key });
    }

    return result;
}

void WebExtensionContextProxy::addPopupPage(WebPage& page, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    m_popupPageMap.set(page, TabWindowIdentifierPair { tabIdentifier, windowIdentifier });
}

Vector<Ref<WebPage>> WebExtensionContextProxy::tabPages(std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier) const
{
    Vector<Ref<WebPage>> result;

    for (auto entry : m_tabPageMap) {
        if (tabIdentifier && entry.value.first && entry.value.first.value() != tabIdentifier.value())
            continue;

        if (windowIdentifier && entry.value.second && entry.value.second.value() != windowIdentifier.value())
            continue;

        result.append(Ref { entry.key });
    }

    return result;
}

void WebExtensionContextProxy::addTabPage(WebPage& page, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    m_tabPageMap.set(page, TabWindowIdentifierPair { tabIdentifier, windowIdentifier });
}

void WebExtensionContextProxy::setBackgroundPageIdentifier(WebCore::PageIdentifier pageIdentifier)
{
    if (RefPtr page = WebProcess::singleton().webPage(pageIdentifier))
        setBackgroundPage(*page);
}

#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
void WebExtensionContextProxy::setOffscreenPageIdentifier(WebCore::PageIdentifier pageIdentifier)
{
    if (RefPtr page = WebProcess::singleton().webPage(pageIdentifier))
        setOffscreenPage(*page);
}
#endif

void WebExtensionContextProxy::addPopupPageIdentifier(WebCore::PageIdentifier pageIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    if (RefPtr page = WebProcess::singleton().webPage(pageIdentifier))
        addPopupPage(*page, tabIdentifier, windowIdentifier);
}

void WebExtensionContextProxy::addTabPageIdentifier(WebCore::PageIdentifier pageIdentifier, WebExtensionTabIdentifier tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    if (RefPtr page = WebProcess::singleton().webPage(pageIdentifier))
        addTabPage(*page, tabIdentifier, windowIdentifier);
}

void WebExtensionContextProxy::setStorageAccessLevel(WebExtensionDataType dataType, WebExtensionStorageAccessLevel accessLevel)
{
    m_storageAccessLevels.set(dataType, accessLevel);
}

void WebExtensionContextProxy::setContentScriptWorld(WebCore::DOMWrapperWorld& world)
{
    m_contentScriptWorld = world;
    world.setClosedShadowRootIsExposedForExtensions();
}

void WebExtensionContextProxy::enumerateFramesAndNamespaceObjects(NOESCAPE const Function<void(WebFrame&, WebExtensionAPINamespace&)>& function, Ref<DOMWrapperWorld>&& world)
{
    m_extensionContentFrames.forEach([&](auto& frame) {
        RefPtr page = frame.page() ? frame.page()->corePage() : nullptr;
        if (!page)
            return;

        // We only support calling jsContextForServiceWorkerWorld() for the normal world.
        if (page->isServiceWorkerPage() && !world->isNormal())
            return;

        auto context = page->isServiceWorkerPage() ? frame.jsContextForServiceWorkerWorld(world) : frame.jsContextForWorld(world);
        auto globalObject = JSContextGetGlobalObject(context);

        RefPtr<WebExtensionAPINamespace> namespaceObjectImpl;
        auto browserNamespaceObject = JSObjectGetProperty(context, globalObject, toJSString("browser"_s).get(), nullptr);
        if (browserNamespaceObject && JSValueIsObject(context, browserNamespaceObject))
            namespaceObjectImpl = toWebExtensionAPINamespace(context, browserNamespaceObject);

        if (!namespaceObjectImpl) {
            auto chromeNamespaceObject = JSObjectGetProperty(context, globalObject, toJSString("chrome"_s).get(), nullptr);
            if (chromeNamespaceObject && JSValueIsObject(context, chromeNamespaceObject))
                namespaceObjectImpl = toWebExtensionAPINamespace(context, chromeNamespaceObject);
        }

        if (!namespaceObjectImpl)
            return;

        function(frame, *namespaceObjectImpl);
    });
}

void WebExtensionContextProxy::enumerateFramesAndWebPageNamespaceObjects(NOESCAPE const Function<void(WebFrame&, WebExtensionAPIWebPageNamespace&)>& function)
{
    m_extensionContentFrames.forEach([&](auto& frame) {
        auto context = frame.jsContextForWorld(mainWorldSingleton());
        auto globalObject = JSContextGetGlobalObject(context);

        RefPtr<WebExtensionAPIWebPageNamespace> namespaceObjectImpl;
        auto browserNamespaceObject = JSObjectGetProperty(context, globalObject, toJSString("browser"_s).get(), nullptr);
        if (browserNamespaceObject && JSValueIsObject(context, browserNamespaceObject))
            namespaceObjectImpl = toWebExtensionAPIWebPageNamespace(context, browserNamespaceObject);

        if (!namespaceObjectImpl)
            return;

        function(frame, *namespaceObjectImpl);
    });
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

    RELEASE_ASSERT_NOT_REACHED();
}

void WebExtensionContextProxy::didEncounterScriptError(const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, WebExtensionContentWorldType worldType)
{
    WebProcess::singleton().send(Messages::WebExtensionContext::DidEncounterScriptError(message, sourceURL, lineNumber, columnNumber, worldType), identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
