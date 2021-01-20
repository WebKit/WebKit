/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "APIInspectorExtension.h"

#if ENABLE(INSPECTOR_EXTENSIONS)

#include "InspectorExtensionTypes.h"
#include "WebInspectorUIExtensionControllerProxy.h"

namespace API {

InspectorExtension::InspectorExtension(const WTF::String& identifier, WebKit::WebInspectorUIExtensionControllerProxy& extensionControllerProxy)
    : m_identifier(identifier)
    , m_extensionControllerProxy(makeWeakPtr(extensionControllerProxy))
{
}

Ref<InspectorExtension> InspectorExtension::create(const WTF::String& identifier, WebKit::WebInspectorUIExtensionControllerProxy& extensionControllerProxy)
{
    return adoptRef(*new InspectorExtension(identifier, extensionControllerProxy));
}

void InspectorExtension::createTab(const WTF::String& tabName, const WTF::URL& tabIconURL, const WTF::URL& sourceURL, WTF::CompletionHandler<void(Expected<WebKit::InspectorExtensionTabID, WebKit::InspectorExtensionError>)>&& completionHandler)
{
    if (!m_extensionControllerProxy) {
        completionHandler(makeUnexpected(WebKit::InspectorExtensionError::ContextDestroyed));
        return;
    }

    m_extensionControllerProxy->createTabForExtension(m_identifier, tabName, tabIconURL, sourceURL, WTFMove(completionHandler));
}

} // namespace API

#endif // ENABLE(INSPECTOR_EXTENSIONS)
