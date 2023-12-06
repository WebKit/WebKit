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
#import "ContextMenuContextData.h"
#import "Logging.h"
#import "SandboxUtilities.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextParameters.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionControllerMessages.h"
#import "WebExtensionControllerProxyMessages.h"
#import "WebExtensionEventListenerType.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import <WebCore/ContentRuleListResults.h>
#import <wtf/FileSystem.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/WTFString.h>

static constexpr Seconds purgeMatchedRulesInterval = 5_min;

namespace WebKit {

String WebExtensionController::storageDirectory(WebExtensionContext& extensionContext) const
{
    if (m_configuration->storageIsPersistent() && extensionContext.hasCustomUniqueIdentifier())
        return FileSystem::pathByAppendingComponent(m_configuration->storageDirectory(), extensionContext.uniqueIdentifier());
    return nullString();
}

bool WebExtensionController::load(WebExtensionContext& extensionContext, NSError **outError)
{
    if (outError)
        *outError = nil;

    if (!m_extensionContexts.add(extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded");
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::AlreadyLoaded);
        return false;
    }

    if (!m_extensionContextBaseURLMap.add(extensionContext.baseURL().protocolHostAndPort(), extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded with same base URL: %{private}@", (NSURL *)extensionContext.baseURL());
        m_extensionContexts.remove(extensionContext);
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::BaseURLAlreadyInUse);
        return false;
    }

    for (auto& processPool : m_processPools)
        processPool.addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.identifier(), extensionContext);

    auto scheme = extensionContext.baseURL().protocol().toString();
    m_registeredSchemeHandlers.ensure(scheme, [&]() {
        auto handler = WebExtensionURLSchemeHandler::create(*this);

        for (auto& page : m_pages)
            page.setURLSchemeHandlerForScheme(handler.copyRef(), scheme);

        return handler;
    });

    auto extensionDirectory = storageDirectory(extensionContext);
    if (!!extensionDirectory && !FileSystem::makeAllDirectories(extensionDirectory))
        RELEASE_LOG_ERROR(Extensions, "Failed to create directory: %{private}@", (NSString *)extensionDirectory);

    if (!extensionContext.load(*this, extensionDirectory, outError)) {
        m_extensionContexts.remove(extensionContext);
        m_extensionContextBaseURLMap.remove(extensionContext.baseURL().protocolHostAndPort());

        for (auto& processPool : m_processPools)
            processPool.removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.identifier());

        return false;
    }

    sendToAllProcesses(Messages::WebExtensionControllerProxy::Load(extensionContext.parameters()), m_identifier);

    return true;
}

bool WebExtensionController::unload(WebExtensionContext& extensionContext, NSError **outError)
{
    if (outError)
        *outError = nil;

    Ref protectedExtensionContext = extensionContext;

    if (!m_extensionContexts.remove(extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context not loaded");
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::NotLoaded);
        return false;
    }

    bool result = m_extensionContextBaseURLMap.remove(extensionContext.baseURL().protocolHostAndPort());
    UNUSED_VARIABLE(result);
    ASSERT(result);

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

    for (auto& entry : m_registeredSchemeHandlers)
        page.setURLSchemeHandlerForScheme(entry.value.copyRef(), entry.key);

    Ref pool = page.process().processPool();
    addProcessPool(pool);

    Ref controller = page.userContentController();
    addUserContentController(controller, page.websiteDataStore().isPersistent() ? ForPrivateBrowsing::No : ForPrivateBrowsing::Yes);
}

void WebExtensionController::removePage(WebPageProxy& page)
{
    ASSERT(m_pages.contains(page));
    m_pages.remove(page);

    Ref pool = page.process().processPool();
    removeProcessPool(pool);

    Ref controller = page.userContentController();
    removeUserContentController(controller);
}

void WebExtensionController::addProcessPool(WebProcessPool& processPool)
{
    if (!m_processPools.add(processPool))
        return;

    processPool.addMessageReceiver(Messages::WebExtensionController::messageReceiverName(), m_identifier, *this);

    for (auto& context : m_extensionContexts)
        processPool.addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context->identifier(), context);
}

void WebExtensionController::removeProcessPool(WebProcessPool& processPool)
{
    // Only remove the message receiver and process pool if no other pages use the same process pool.
    for (auto& knownPage : m_pages) {
        if (knownPage.process().processPool() == processPool)
            return;
    }

    processPool.removeMessageReceiver(Messages::WebExtensionController::messageReceiverName(), m_identifier);

    for (auto& context : m_extensionContexts)
        processPool.removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context->identifier());

    m_processPools.remove(processPool);
}

void WebExtensionController::addUserContentController(WebUserContentControllerProxy& userContentController, ForPrivateBrowsing forPrivateBrowsing)
{
    if (forPrivateBrowsing == ForPrivateBrowsing::No)
        m_allNonPrivateUserContentControllers.add(userContentController);
    else
        m_allPrivateUserContentControllers.add(userContentController);

    if (!m_allUserContentControllers.add(userContentController))
        return;

    for (auto& context : m_extensionContexts) {
        if (!context->hasAccessInPrivateBrowsing() && forPrivateBrowsing == ForPrivateBrowsing::Yes)
            continue;

        context->addInjectedContent(userContentController);
    }
}

void WebExtensionController::removeUserContentController(WebUserContentControllerProxy& userContentController)
{
    // Only remove the user content controller if no other pages use the same one.
    for (auto& knownPage : m_pages) {
        if (knownPage.userContentController() == userContentController)
            return;
    }

    for (auto& context : m_extensionContexts)
        context->removeInjectedContent(userContentController);

    m_allNonPrivateUserContentControllers.remove(userContentController);
    m_allPrivateUserContentControllers.remove(userContentController);
    m_allUserContentControllers.remove(userContentController);
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
    return m_extensionContextBaseURLMap.get(url.protocolHostAndPort());
}

WebExtensionController::WebExtensionSet WebExtensionController::extensions() const
{
    WebExtensionSet extensions;
    extensions.reserveInitialCapacity(m_extensionContexts.size());
    for (auto& extensionContext : m_extensionContexts)
        extensions.addVoid(extensionContext->extension());
    return extensions;
}

#if PLATFORM(MAC)
void WebExtensionController::addItemsToContextMenu(WebPageProxy& page, const ContextMenuContextData& contextData, NSMenu *menu)
{
    [menu addItem:NSMenuItem.separatorItem];

    for (auto& context : m_extensionContexts)
        context->addItemsToContextMenu(page, contextData, menu);
}
#endif

void WebExtensionController::didStartProvisionalLoadForFrame(WebPageProxyIdentifier pageID, WebExtensionFrameIdentifier frameID, WebExtensionFrameIdentifier parentFrameID, const URL& targetURL, WallTime timestamp)
{
    for (auto& context : m_extensionContexts)
        context->didStartProvisionalLoadForFrame(pageID, frameID, parentFrameID, targetURL, timestamp);
}

void WebExtensionController::didCommitLoadForFrame(WebPageProxyIdentifier pageID, WebExtensionFrameIdentifier frameID, WebExtensionFrameIdentifier parentFrameID, const URL& frameURL, WallTime timestamp)
{
    for (auto& context : m_extensionContexts)
        context->didCommitLoadForFrame(pageID, frameID, parentFrameID, frameURL, timestamp);
}

void WebExtensionController::didFinishLoadForFrame(WebPageProxyIdentifier pageID, WebExtensionFrameIdentifier frameID, WebExtensionFrameIdentifier parentFrameID, const URL& frameURL, WallTime timestamp)
{
    for (auto& context : m_extensionContexts)
        context->didFinishLoadForFrame(pageID, frameID, parentFrameID, frameURL, timestamp);
}

void WebExtensionController::didFailLoadForFrame(WebPageProxyIdentifier pageID, WebExtensionFrameIdentifier frameID, WebExtensionFrameIdentifier parentFrameID, const URL& frameURL, WallTime timestamp)
{
    for (auto& context : m_extensionContexts)
        context->didFailLoadForFrame(pageID, frameID, parentFrameID, frameURL, timestamp);
}

void WebExtensionController::handleContentRuleListNotification(WebPageProxyIdentifier pageID, URL& url, WebCore::ContentRuleListResults& results)
{
    bool savedMatchedRule = false;

    for (const auto& result : results.results) {
        auto contentRuleListIdentifier = result.first;
        for (auto& context : m_extensionContexts) {
            if (context->uniqueIdentifier() != contentRuleListIdentifier)
                continue;

            RefPtr tab = context->getTab(pageID);
            if (!tab)
                break;

            savedMatchedRule |= context->handleContentRuleListNotificationForTab(*tab, url, result.second);

            break;
        }
    }

    if (!savedMatchedRule || m_purgeOldMatchedRulesTimer)
        return;

    m_purgeOldMatchedRulesTimer = makeUnique<WebCore::Timer>(*this, &WebExtensionController::purgeOldMatchedRules);
    m_purgeOldMatchedRulesTimer->start(purgeMatchedRulesInterval, purgeMatchedRulesInterval);
}

void WebExtensionController::purgeOldMatchedRules()
{
    WallTime earliestDateToKeep = WallTime::now() - purgeMatchedRulesInterval;

    bool stillHaveRules = false;
    for (auto& extensionContext : m_extensionContexts)
        stillHaveRules |= extensionContext->purgeMatchedRulesFromBefore(earliestDateToKeep);

    if (!stillHaveRules)
        m_purgeOldMatchedRulesTimer = nullptr;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
