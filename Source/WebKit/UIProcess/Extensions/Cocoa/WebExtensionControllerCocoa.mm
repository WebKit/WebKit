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
#import "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionControllerMessages.h"
#import "WebExtensionControllerProxyMessages.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

bool WebExtensionController::load(WebExtensionContext& extensionContext, NSError **outError)
{
    if (outError)
        *outError = nil;

    if (!m_extensionContexts.add(extensionContext)) {
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::AlreadyLoaded);
        return false;
    }

    if (!m_extensionContextBaseURLMap.add(extensionContext.baseURL(), extensionContext)) {
        m_extensionContexts.remove(extensionContext);
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::BaseURLTaken);
        return false;
    }

    if (!extensionContext.load(*this, outError)) {
        m_extensionContexts.remove(extensionContext);
        m_extensionContextBaseURLMap.remove(extensionContext.baseURL());
        return false;
    }

    auto scheme = extensionContext.baseURL().protocol().toString();
    m_registeredSchemeHandlers.ensure(scheme, [&]() {
        auto handler = WebExtensionURLSchemeHandler::create(*this);

        for (auto& page : m_pages)
            page.setURLSchemeHandlerForScheme(handler.copyRef(), scheme);

        return handler;
    });

    for (auto& processPool : m_processPools)
        processPool.addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.identifier(), extensionContext);

    sendToAllProcesses(Messages::WebExtensionControllerProxy::Load(extensionContext.parameters()), m_identifier);

    return true;
}

bool WebExtensionController::unload(WebExtensionContext& extensionContext, NSError **outError)
{
    if (outError)
        *outError = nil;

    Ref protectedExtensionContext = extensionContext;

    if (!m_extensionContexts.remove(extensionContext)) {
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::NotLoaded);
        return false;
    }

    ASSERT(m_extensionContextBaseURLMap.contains(extensionContext.baseURL()));
    m_extensionContextBaseURLMap.remove(extensionContext.baseURL());

    sendToAllProcesses(Messages::WebExtensionControllerProxy::Unload(extensionContext.identifier()), m_identifier);

    for (auto& processPool : m_processPools)
        processPool.removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.identifier());

    if (!extensionContext.unload(outError))
        return false;

    return true;
}

void WebExtensionController::unloadAll()
{
    auto contextsCopy = m_extensionContexts;
    for (auto& context : contextsCopy)
        unload(context, nullptr);
}

void WebExtensionController::addPage(WebPageProxy& page)
{
    ASSERT(!m_pages.contains(page));
    m_pages.add(page);

    auto& processPool = page.process().processPool();
    if (m_processPools.add(processPool))
        processPool.addMessageReceiver(Messages::WebExtensionController::messageReceiverName(), m_identifier, *this);

    for (auto& entry : m_registeredSchemeHandlers)
        page.setURLSchemeHandlerForScheme(entry.value.copyRef(), entry.key);
}

void WebExtensionController::removePage(WebPageProxy& page)
{
    ASSERT(m_pages.contains(page));
    m_pages.remove(page);

    // The process pool might have already been deallocated and removed from the weak set.
    auto& processPool = page.process().processPool();
    if (!m_processPools.contains(processPool))
        return;

    // Only remove the message receiver and process pool if no other pages use the same process pool.
    for (auto& knownPage : m_pages) {
        if (knownPage.process().processPool() == processPool)
            return;
    }

    processPool.removeMessageReceiver(Messages::WebExtensionController::messageReceiverName(), m_identifier);
    m_processPools.remove(processPool);
}

RefPtr<WebExtensionContext> WebExtensionController::extensionContext(const WebExtension& extension) const
{
    for (auto& extensionContext : m_extensionContexts) {
        if (extensionContext->extension() == extension)
            return extensionContext.ptr();
    }

    return nullptr;
}

RefPtr<WebExtensionContext> WebExtensionController::extensionContext(const URL& url) const
{
    return m_extensionContextBaseURLMap.get(url.truncatedForUseAsBase());
}

WebExtensionController::WebExtensionSet WebExtensionController::extensions() const
{
    WebExtensionSet extensions;
    extensions.reserveInitialCapacity(m_extensionContexts.size());
    for (auto& extensionContext : m_extensionContexts)
        extensions.addVoid(extensionContext->extension());
    return extensions;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
