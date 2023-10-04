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
#include "WebExtensionContextProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPINamespace.h"
#include "JSWebExtensionWrapper.h"
#include "WebFrame.h"
#include "WebPage.h"

namespace WebKit {

using namespace WebCore;

void WebExtensionContextProxy::addFrameWithExtensionContent(WebFrame& frame)
{
    m_extensionContentFrames.add(frame);
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
