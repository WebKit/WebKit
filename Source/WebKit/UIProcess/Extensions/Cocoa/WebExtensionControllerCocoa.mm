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
#import "WebPageProxy.h"
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

    return true;
}

bool WebExtensionController::unload(WebExtensionContext& extensionContext, NSError **outError)
{
    if (outError)
        *outError = nil;

    if (!m_extensionContexts.remove(extensionContext)) {
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::NotLoaded);
        return false;
    }

    ASSERT(m_extensionContextBaseURLMap.contains(extensionContext.baseURL()));
    m_extensionContextBaseURLMap.remove(extensionContext.baseURL());

    if (!extensionContext.unload(outError))
        return false;

    return true;
}

void WebExtensionController::addPage(WebPageProxy& page)
{
    ASSERT(!m_pages.contains(page));

    m_pages.add(page);

    for (auto& entry : m_registeredSchemeHandlers)
        page.setURLSchemeHandlerForScheme(entry.value.copyRef(), entry.key);
}

void WebExtensionController::removePage(WebPageProxy& page)
{
    ASSERT(m_pages.contains(page));

    m_pages.remove(page);
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
