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

static HashMap<WebExtensionControllerIdentifier, WebExtensionController*>& webExtensionControllers()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<WebExtensionControllerIdentifier, WebExtensionController*>> controllers;
    return controllers;
}

WebExtensionController* WebExtensionController::get(WebExtensionControllerIdentifier identifier)
{
    return webExtensionControllers().get(identifier);
}

WebExtensionController::WebExtensionController()
    : m_identifier(WebExtensionControllerIdentifier::generate())
{
    webExtensionControllers().add(m_identifier, this);
}

WebExtensionController::~WebExtensionController()
{
    ASSERT(webExtensionControllers().contains(m_identifier));
    webExtensionControllers().remove(m_identifier);
}

WebExtensionControllerParameters WebExtensionController::parameters() const
{
    WebExtensionControllerParameters parameters;

    parameters.identifier = identifier();

    return parameters;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
