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

#include "config.h"
#include "WebExtensionControllerProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPINamespace.h"
#include "JSWebExtensionWrapper.h"
#include "WebExtensionAPINamespace.h"
#include "WebExtensionContextProxy.h"
#include "WebExtensionControllerMessages.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

void WebExtensionControllerProxy::globalObjectIsAvailableForFrame(WebPage& page, WebFrame& frame, DOMWrapperWorld& world)
{
    auto extension = extensionContext(frame, world);
    bool isMainWorld = world.isNormal();

    if (!extension && isMainWorld) {
        // FIXME: <https://webkit.org/b/246491> Add bindings for externally connectable.
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

    auto forMainWorld = isMainWorld ? WebExtensionAPINamespace::ForMainWorld::Yes : WebExtensionAPINamespace::ForMainWorld::No;
    // FIXME: <https://webkit.org/b/246485> Handle inspector pages by setting forMainWorld to No.

    namespaceObject = toJS(context, WebExtensionAPINamespace::create(forMainWorld, *extension).ptr());

    JSObjectSetProperty(context, globalObject, toJSString("browser").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(context, globalObject, toJSString("chrome").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
}

void WebExtensionControllerProxy::serviceWorkerGlobalObjectIsAvailableForFrame(WebPage& page, WebFrame& frame, DOMWrapperWorld& world)
{
    RELEASE_ASSERT(world.isNormal());

    auto extension = extensionContext(frame, world);
    if (!extension)
        return;

    auto context = frame.jsContextForServiceWorkerWorld(world);
    auto globalObject = JSContextGetGlobalObject(context);

    auto namespaceObject = JSObjectGetProperty(context, globalObject, toJSString("browser").get(), nullptr);
    if (namespaceObject && JSValueIsObject(context, namespaceObject))
        return;

    extension->addFrameWithExtensionContent(frame);

    namespaceObject = toJS(context, WebExtensionAPINamespace::create(WebExtensionAPINamespace::ForMainWorld::Yes, *extension).ptr());

    JSObjectSetProperty(context, globalObject, toJSString("browser").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(context, globalObject, toJSString("chrome").get(), namespaceObject, kJSPropertyAttributeNone, nullptr);
}

void WebExtensionControllerProxy::didStartProvisionalLoadForFrame(WebPage& page, WebFrame& frame, const URL& url)
{
    WebProcess::singleton().send(Messages::WebExtensionController::DidStartProvisionalLoadForFrame(page.webPageProxyIdentifier(), frame.frameID(), url), identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
