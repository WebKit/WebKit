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
#include "WebExtensionContextProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPINamespace.h"
#include "JSWebExtensionWrapper.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

void WebExtensionContextProxy::addFrameWithExtensionContent(WebFrame& frame)
{
    m_extensionContentFrames.add(frame);
}

RefPtr<WebPage> WebExtensionContextProxy::backgroundPage() const
{
    return m_backgroundPage.get();
}

void WebExtensionContextProxy::setBackgroundPage(WebPage& page)
{
    m_backgroundPage = page;
}

Vector<Ref<WebPage>> WebExtensionContextProxy::popupPages(std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier) const
{
    Vector<Ref<WebPage>> result;

    for (auto entry : m_popupPageMap) {
        if (tabIdentifier && entry.value.first && entry.value.first.value() != tabIdentifier.value())
            continue;

        if (windowIdentifier && entry.value.second && entry.value.second.value() != windowIdentifier.value())
            continue;

        result.append(entry.key);
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

        result.append(entry.key);
    }

    return result;
}

void WebExtensionContextProxy::addTabPage(WebPage& page, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    m_tabPageMap.set(page, TabWindowIdentifierPair { tabIdentifier, windowIdentifier });
}

void WebExtensionContextProxy::setBackgroundPageIdentifier(WebCore::PageIdentifier pageIdentifier)
{
    if (auto* page = WebProcess::singleton().webPage(pageIdentifier))
        setBackgroundPage(*page);
}

void WebExtensionContextProxy::addPopupPageIdentifier(WebCore::PageIdentifier pageIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    if (auto* page = WebProcess::singleton().webPage(pageIdentifier))
        addPopupPage(*page, tabIdentifier, windowIdentifier);
}

void WebExtensionContextProxy::addTabPageIdentifier(WebCore::PageIdentifier pageIdentifier, WebExtensionTabIdentifier tabIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier)
{
    if (auto* page = WebProcess::singleton().webPage(pageIdentifier))
        addTabPage(*page, tabIdentifier, windowIdentifier);
}

void WebExtensionContextProxy::setStorageAccessLevel(bool allowedInContentScripts)
{
    m_isSessionStorageAllowedInContentScripts = allowedInContentScripts;
}

void WebExtensionContextProxy::enumerateFramesAndNamespaceObjects(const Function<void(WebFrame&, WebExtensionAPINamespace&)>& function, DOMWrapperWorld& world)
{
    for (auto& frame : m_extensionContentFrames) {
        auto* page = frame.page() ? frame.page()->corePage() : nullptr;
        if (!page)
            continue;

        auto context = page->isServiceWorkerPage() ? frame.jsContextForServiceWorkerWorld(world) : frame.jsContextForWorld(world);
        auto globalObject = JSContextGetGlobalObject(context);
        auto namespaceObject = JSObjectGetProperty(context, globalObject, toJSString("browser").get(), nullptr);
        if (!namespaceObject || !JSValueIsObject(context, namespaceObject))
            continue;

        auto* namespaceObjectImpl = toWebExtensionAPINamespace(context, namespaceObject);
        if (!namespaceObjectImpl)
            continue;

        function(frame, *namespaceObjectImpl);
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
