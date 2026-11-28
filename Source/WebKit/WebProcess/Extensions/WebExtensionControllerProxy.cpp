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

#include "config.h"
#include "WebExtensionControllerProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPINamespace.h"
#include "JSWebExtensionAPIWebPageNamespace.h"
#include "JSWebExtensionWrapper.h"
#include "MessageSenderInlines.h"
#include "WebExtensionAPINamespace.h"
#include "WebExtensionAPITest.h"
#include "WebExtensionAPIWebPageNamespace.h"
#include "WebExtensionContextProxy.h"
#include "WebExtensionControllerMessages.h"
#include "WebExtensionControllerProxyMessages.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebExtensionUtilities.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/JSDOMGlobalObject.h>
#include <WebCore/LocalFrame.h>
#include <wtf/Borrow.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

NODELETE static HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionControllerProxy>>& webExtensionControllerProxies()
{
    static MainRunLoopNeverDestroyed<HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionControllerProxy>>> controllers;
    return controllers;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionControllerProxy);

RefPtr<WebExtensionControllerProxy> WebExtensionControllerProxy::get(WebExtensionControllerIdentifier identifier)
{
    return webExtensionControllerProxies().get(identifier);
}

Ref<WebExtensionControllerProxy> WebExtensionControllerProxy::getOrCreate(const WebExtensionControllerParameters& parameters, WebPage* newPage)
{
    auto updateProperties = [&](WebExtensionControllerProxy& controller) {
        WebExtensionContextProxySet contexts;
        WebExtensionContextProxyBaseURLMap baseURLMap;

        for (auto& contextParameters : borrow(parameters.contextParameters)) {
            Ref context = WebExtensionContextProxy::getOrCreate(contextParameters, controller, newPage);
            baseURLMap.add(contextParameters.baseURL.protocolHostAndPort(), context);
            contexts.add(context);
        }

        controller.m_testingMode = parameters.testingMode;
        controller.m_extensionContexts = WTF::move(contexts);
        controller.m_extensionContextBaseURLMap = WTF::move(baseURLMap);
    };

    if (RefPtr controller = get(parameters.identifier)) {
        updateProperties(*controller);
        return *controller;
    }

    Ref result = adoptRef(*new WebExtensionControllerProxy(parameters));
    updateProperties(result);
    return result;
}

WebExtensionControllerProxy::WebExtensionControllerProxy(const WebExtensionControllerParameters& parameters)
    : m_identifier(parameters.identifier)
{
    ASSERT(!get(m_identifier));
    webExtensionControllerProxies().add(m_identifier, *this);

    WebProcess::singleton().addMessageReceiver(Messages::WebExtensionControllerProxy::messageReceiverName(), m_identifier, *this);
}

WebExtensionControllerProxy::~WebExtensionControllerProxy()
{
    webExtensionControllerProxies().remove(m_identifier);
    WebProcess::singleton().removeMessageReceiver(*this);
}

void WebExtensionControllerProxy::load(const WebExtensionContextParameters& contextParameters)
{
    auto context = WebExtensionContextProxy::getOrCreate(contextParameters, *this);
    m_extensionContextBaseURLMap.add(contextParameters.baseURL.protocolHostAndPort(), context);
    m_extensionContexts.add(context);

#if PLATFORM(COCOA)
    addTestListenersToContext(context);
#endif
}

void WebExtensionControllerProxy::unload(WebExtensionContextIdentifier contextIdentifier)
{
    m_extensionContextBaseURLMap.removeIf([&](auto& entry) {
        return entry.value->unprivilegedIdentifier() == contextIdentifier;
    });

    m_extensionContexts.removeIf([&](auto& entry) {
        return entry->unprivilegedIdentifier() == contextIdentifier;
    });
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(const String& uniqueIdentifier) const
{
    for (auto& extensionContext : m_extensionContexts) {
        if (extensionContext->uniqueIdentifier() == uniqueIdentifier)
            return extensionContext.ptr();
    }

    return nullptr;
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(const URL& url) const
{
    return m_extensionContextBaseURLMap.get(url.protocolHostAndPort());
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(WebFrame& frame, DOMWrapperWorld& world) const
{
    if (!world.isNormal()) {
        auto prefix = "WebExtension-"_s;
        if (!world.name().startsWith(prefix))
            return nullptr;

        auto prefixLength = prefix.length();
        auto uniqueIdentifier = world.name().substring(prefixLength);
        return extensionContext(uniqueIdentifier);
    }

    return extensionContext(frame.url());
}


void WebExtensionControllerProxy::globalObjectIsAvailableForFrame(WebPage& page, WebFrame& frame, DOMWrapperWorld& world)
{
    RefPtr extension = extensionContext(frame, world);
    bool isMainWorld = world.isNormal();

    if (!extension && isMainWorld) {
        addBindingsToWebPageFrameIfNecessary(frame, world);
        return;
    }

    if (!extension)
        return;

    auto context = frame.jsContextForWorld(world);
    auto globalObject = JSContextGetGlobalObject(context);

    RefPtr browserString = toJSString("browser"_s);
    if (!browserString)
        return;

    auto namespaceObject = JSObjectGetProperty(context, globalObject, browserString.get(), nullptr);
    if (namespaceObject && JSValueIsObject(context, namespaceObject))
        return;

    extension->addFrameWithExtensionContent(frame);

    if (!isMainWorld)
        extension->setContentScriptWorld(world);
    else if (RefPtr coreFrame = frame.coreLocalFrame()) {
        if (RefPtr documentLoader = coreFrame->loader().loaderForWebsitePolicies())
            documentLoader->setAllowsJSHandleCreationInPageWorld(true);
    }

    auto contentWorldType = isMainWorld ? WebExtensionContentWorldType::Main : WebExtensionContentWorldType::ContentScript;

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (page.isInspectorPage() || extension->isInspectorBackgroundPage(page)) {
        // Inspector pages have a limited set of APIs (like content scripts).
        contentWorldType = WebExtensionContentWorldType::Inspector;
    }
#endif

    namespaceObject = toJS(context, WebExtensionAPINamespace::create(contentWorldType, *extension).ptr());

    JSObjectSetProperty(context, globalObject, browserString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);

    if (RefPtr chromeString = toJSString("chrome"_s))
        JSObjectSetProperty(context, globalObject, chromeString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);

    if (auto* domGlobalObject = dynamicDowncast<JSDOMGlobalObject>(toJS(context))) {
        domGlobalObject->addScriptErrorCallback([extension = Ref { *extension }, contentWorldType](const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber) {
            extension->didEncounterScriptError(message, sourceURL, lineNumber, columnNumber, contentWorldType);
        });
    }
}

void WebExtensionControllerProxy::serviceWorkerGlobalObjectIsAvailableForFrame(WebPage& page, WebFrame& frame, DOMWrapperWorld& world)
{
    RELEASE_ASSERT(world.isNormal());

    RefPtr extension = extensionContext(frame, world);
    if (!extension)
        return;

    auto context = frame.jsContextForServiceWorkerWorld(world);
    auto globalObject = JSContextGetGlobalObject(context);

    RefPtr browserString = toJSString("browser"_s);
    if (!browserString)
        return;

    auto namespaceObject = JSObjectGetProperty(context, globalObject, browserString.get(), nullptr);
    if (namespaceObject && JSValueIsObject(context, namespaceObject))
        return;

    extension->addFrameWithExtensionContent(frame);

    namespaceObject = toJS(context, WebExtensionAPINamespace::create(WebExtensionContentWorldType::Main, *extension).ptr());

    JSObjectSetProperty(context, globalObject, browserString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    if (RefPtr chromeString = toJSString("chrome"_s))
        JSObjectSetProperty(context, globalObject, chromeString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);

    if (auto* domGlobalObject = dynamicDowncast<JSDOMGlobalObject>(toJS(context))) {
        domGlobalObject->addScriptErrorCallback([extension = Ref { *extension }](const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber) {
            extension->didEncounterScriptError(message, sourceURL, lineNumber, columnNumber, WebExtensionContentWorldType::Main);
        });
    }
}

void WebExtensionControllerProxy::addBindingsToWebPageFrameIfNecessary(WebFrame& frame, DOMWrapperWorld& world)
{
    auto context = frame.jsContextForWorld(world);
    auto globalObject = JSContextGetGlobalObject(context);

    RefPtr browserString = toJSString("browser"_s);
    if (!browserString)
        return;

    auto namespaceObject = JSObjectGetProperty(context, globalObject, browserString.get(), nullptr);
    bool browserAlreadySet = namespaceObject && JSValueIsObject(context, namespaceObject);

    auto* domGlobalObject = dynamicDowncast<JSDOMGlobalObject>(toJS(context));
    bool callbacksAlreadyRegistered = !domGlobalObject || domGlobalObject->hasScriptErrorCallbacks();

    // If both browser and callbacks are already set up, nothing to do.
    if (browserAlreadySet && callbacksAlreadyRegistered)
        return;

    if (!browserAlreadySet) {
        namespaceObject = toJS(context, WebExtensionAPIWebPageNamespace::create(WebExtensionContentWorldType::WebPage).ptr());

        JSObjectSetProperty(context, globalObject, browserString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    }

    // For each loaded extension, install a callback that fires only when an error's sourceURL
    // originates from that extension (main-world content script case). Guard against registering
    // duplicate callbacks: if no callbacks are registered yet (e.g. extension contexts were not
    // yet loaded when the global object was first created), register them now.
    if (domGlobalObject && !domGlobalObject->hasScriptErrorCallbacks()) {
        for (Ref extensionContext : m_extensionContexts) {
            domGlobalObject->addScriptErrorCallback([extensionContext = extensionContext.copyRef()](const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber) {
                if (!extensionContext->isURLForThisExtension(URL(URL(), sourceURL)))
                    return;

                extensionContext->didEncounterScriptError(message, sourceURL, lineNumber, columnNumber, WebExtensionContentWorldType::Main);
            });
        }
    }
}

void WebExtensionControllerProxy::addFrameWithTestListeners(WebFrame& frame)
{
    m_framesWithTestListeners.add(frame);
}

void WebExtensionControllerProxy::addTestListenersToContext(WebExtensionContextProxy& context)
{
    if (!m_testingMode)
        return;

    m_framesWithTestListeners.forEach([&](auto& frame) {
        auto jsContext = frame.jsContextForWorld(WebExtensionContextProxy::mainWorldSingleton());
        auto globalObject = JSContextGetGlobalObject(jsContext);

        auto browserObject = JSObjectGetProperty(jsContext, globalObject, toJSString("browser"_s).get(), nullptr);
        if (!browserObject || !JSValueIsObject(jsContext, browserObject))
            return;

        RefPtr namespaceObject = toWebExtensionAPIWebPageNamespace(jsContext, browserObject);
        if (!namespaceObject)
            return;

        context.addFrameWithExtensionContent(frame);

        Ref testObject = namespaceObject->test();
        protect(testObject->onMessage())->addListenersToContext(context);
        protect(testObject->onTestStarted())->addListenersToContext(context);
        protect(testObject->onTestFinished())->addListenersToContext(context);
    });
}

static WebExtensionFrameParameters toFrameParameters(WebFrame& frame, const URL& url, bool includeDocumentIdentifier = true)
{
    auto parentFrameIdentifier = WebExtensionFrameConstants::NoneIdentifier;
    if (RefPtr parentFrame = frame.parentFrame())
        parentFrameIdentifier = toWebExtensionFrameIdentifier(*parentFrame);

    return {
        .url = url,
        .parentFrameIdentifier = parentFrameIdentifier,
        .frameIdentifier = toWebExtensionFrameIdentifier(frame),
        .documentIdentifier = includeDocumentIdentifier ? toDocumentIdentifier(frame) : std::nullopt
    };
}

void WebExtensionControllerProxy::didStartProvisionalLoadForFrame(WebPage& page, WebFrame& frame, const URL& url)
{
    if (!hasLoadedContexts())
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::DidStartProvisionalLoadForFrame(page.webPageProxyIdentifier(), toFrameParameters(frame, url, false), WallTime::now()), identifier());
}

void WebExtensionControllerProxy::didCommitLoadForFrame(WebPage& page, WebFrame& frame, const URL& url)
{
    if (!hasLoadedContexts())
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::DidCommitLoadForFrame(page.webPageProxyIdentifier(), toFrameParameters(frame, url), WallTime::now()), identifier());
}

void WebExtensionControllerProxy::didFinishLoadForFrame(WebPage& page, WebFrame& frame, const URL& url)
{
    if (!hasLoadedContexts())
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::DidFinishLoadForFrame(page.webPageProxyIdentifier(), toFrameParameters(frame, url), WallTime::now()), identifier());
}

void WebExtensionControllerProxy::didFailLoadForFrame(WebPage& page, WebFrame& frame, const URL& url)
{
    if (!hasLoadedContexts())
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::DidFailLoadForFrame(page.webPageProxyIdentifier(), toFrameParameters(frame, url), WallTime::now()), identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
