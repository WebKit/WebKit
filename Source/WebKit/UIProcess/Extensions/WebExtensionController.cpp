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
#include "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionControllerParameters.h"
#include "WebExtensionControllerProxyMessages.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

static HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionController>>& webExtensionControllers()
{
    static MainThreadNeverDestroyed<HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionController>>> controllers;
    return controllers;
}

WebExtensionController* WebExtensionController::get(WebExtensionControllerIdentifier identifier)
{
    return webExtensionControllers().get(identifier).get();
}

WebExtensionController::WebExtensionController()
    : m_identifier(WebExtensionControllerIdentifier::generate())
{
    ASSERT(!webExtensionControllers().contains(m_identifier));
    webExtensionControllers().add(m_identifier, this);
}

WebExtensionController::~WebExtensionController()
{
    unloadAll();
}

WebExtensionControllerParameters WebExtensionController::parameters() const
{
    WebExtensionControllerParameters parameters;

    parameters.identifier = identifier();

    Vector<WebExtensionContextParameters> contextParameters;
    contextParameters.reserveInitialCapacity(extensionContexts().size());

    for (auto& context : extensionContexts())
        contextParameters.append(context->parameters());

    parameters.contextParameters = contextParameters;

    return parameters;
}

WebExtensionController::WebProcessProxySet WebExtensionController::allProcesses() const
{
    WebProcessProxySet processes;
    for (auto& page : m_pages)
        processes.add(page.process());
    return processes;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
