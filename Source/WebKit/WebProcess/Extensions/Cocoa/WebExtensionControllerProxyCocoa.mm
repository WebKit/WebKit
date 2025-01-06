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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#include "config.h"
#include "WebExtensionControllerProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPINamespace.h"
#include "JSWebExtensionAPIWebPageNamespace.h"
#include "JSWebExtensionWrapper.h"
#include "MessageSenderInlines.h"
#include "WebExtensionAPINamespace.h"
#include "WebExtensionAPIWebPageNamespace.h"
#include "WebExtensionContextProxy.h"
#include "WebExtensionControllerMessages.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

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

    auto namespaceObject = JSObjectGetProperty(context, globalObject, toJSString("browser").get(), nullptr);
    if (namespaceObject && JSValueIsObject(context, namespaceObject))
        return;

    extension->addFrameWithExtensionContent(frame);

    if (!isMainWorld)
        extension->setContentScriptWorld(&world);

    auto contentWorldType = isMainWorld ? WebExtensionContentWorldType::Main : WebExtensionContentWorldType::ContentScript;

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (page.isInspectorPage() || extension->isInspectorBackgroundPage(page)) {
        // Inspector pages have a limited set of APIs (like content scripts).
        contentWorldType = WebExtensionContentWorldType::Inspector;
    }
#endif

    namespaceObject = toJS(context, WebExtensionAPINamespace::create(contentWorldType, *extension).ptr());

    JSObjectSetProperty(context, globalObject, toJSString("browser").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(context, globalObject, toJSString("chrome").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
}

void WebExtensionControllerProxy::serviceWorkerGlobalObjectIsAvailableForFrame(WebPage& page, WebFrame& frame, DOMWrapperWorld& world)
{
    RELEASE_ASSERT(world.isNormal());

    RefPtr extension = extensionContext(frame, world);
    if (!extension)
        return;

    auto context = frame.jsContextForServiceWorkerWorld(world);
    auto globalObject = JSContextGetGlobalObject(context);

    auto namespaceObject = JSObjectGetProperty(context, globalObject, toJSString("browser").get(), nullptr);
    if (namespaceObject && JSValueIsObject(context, namespaceObject))
        return;

    extension->addFrameWithExtensionContent(frame);

    namespaceObject = toJS(context, WebExtensionAPINamespace::create(WebExtensionContentWorldType::Main, *extension).ptr());

    JSObjectSetProperty(context, globalObject, toJSString("browser").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(context, globalObject, toJSString("chrome").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
}

void WebExtensionControllerProxy::addBindingsToWebPageFrameIfNecessary(WebFrame& frame, DOMWrapperWorld& world)
{
    auto context = frame.jsContextForWorld(world);
    auto globalObject = JSContextGetGlobalObject(context);

    auto namespaceObject = JSObjectGetProperty(context, globalObject, toJSString("browser").get(), nullptr);
    if (namespaceObject && JSValueIsObject(context, namespaceObject))
        return;

    namespaceObject = toJS(context, WebExtensionAPIWebPageNamespace::create(WebExtensionContentWorldType::WebPage).ptr());

    JSObjectSetProperty(context, globalObject, toJSString("browser").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
}

static WebExtensionFrameParameters toFrameParameters(WebFrame& frame, const URL& url, bool includeDocumentIdentifier = true)
{
    return {
        .url = url,
        .parentFrameIdentifier = frame.isMainFrame() ? WebExtensionFrameConstants::NoneIdentifier : toWebExtensionFrameIdentifier(*frame.parentFrame()),
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
