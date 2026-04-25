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
#include <JavaScriptCore/APICast.h>
#include <WebCore/JSDOMGlobalObject.h>

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

    JSRetainPtr browserString = toJSString("browser"_s);
    if (!browserString)
        return;

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG auto namespaceObject = JSObjectGetProperty(context, globalObject, browserString.get(), nullptr);
    if (namespaceObject && JSValueIsObject(context, namespaceObject))
        return;

    extension->addFrameWithExtensionContent(frame);

    if (!isMainWorld)
        extension->setContentScriptWorld(world);

    auto contentWorldType = isMainWorld ? WebExtensionContentWorldType::Main : WebExtensionContentWorldType::ContentScript;

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (page.isInspectorPage() || extension->isInspectorBackgroundPage(page)) {
        // Inspector pages have a limited set of APIs (like content scripts).
        contentWorldType = WebExtensionContentWorldType::Inspector;
    }
#endif

    namespaceObject = toJS(context, WebExtensionAPINamespace::create(contentWorldType, *extension).ptr());

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, globalObject, browserString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);

    if (JSRetainPtr chromeString = toJSString("chrome"_s)) {
        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, globalObject, chromeString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    }

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

    JSRetainPtr browserString = toJSString("browser"_s);
    if (!browserString)
        return;

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG auto namespaceObject = JSObjectGetProperty(context, globalObject, browserString.get(), nullptr);
    if (namespaceObject && JSValueIsObject(context, namespaceObject))
        return;

    extension->addFrameWithExtensionContent(frame);

    namespaceObject = toJS(context, WebExtensionAPINamespace::create(WebExtensionContentWorldType::Main, *extension).ptr());

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, globalObject, browserString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    if (JSRetainPtr chromeString = toJSString("chrome"_s)) {
        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, globalObject, chromeString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    }

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

    JSRetainPtr browserString = toJSString("browser"_s);
    if (!browserString)
        return;

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG auto namespaceObject = JSObjectGetProperty(context, globalObject, browserString.get(), nullptr);
    bool browserAlreadySet = namespaceObject && JSValueIsObject(context, namespaceObject);

    auto* domGlobalObject = dynamicDowncast<JSDOMGlobalObject>(toJS(context));
    bool callbacksAlreadyRegistered = !domGlobalObject || domGlobalObject->hasScriptErrorCallbacks();

    // If both browser and callbacks are already set up, nothing to do.
    if (browserAlreadySet && callbacksAlreadyRegistered)
        return;

    if (!browserAlreadySet) {
        namespaceObject = toJS(context, WebExtensionAPIWebPageNamespace::create(WebExtensionContentWorldType::WebPage).ptr());

        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, globalObject, browserString.get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
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
