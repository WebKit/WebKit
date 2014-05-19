/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebUserContentController.h"

#include "WebProcess.h"
#include "WebUserContentControllerMessages.h"
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/ScriptController.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

static HashMap<uint64_t, WebUserContentController*>& userContentControllers()
{
    static NeverDestroyed<HashMap<uint64_t, WebUserContentController*>> userContentControllers;

    return userContentControllers;
}

PassRefPtr<WebUserContentController> WebUserContentController::getOrCreate(uint64_t identifier)
{
    auto& userContentControllerPtr = userContentControllers().add(identifier, nullptr).iterator->value;
    if (userContentControllerPtr)
        return userContentControllerPtr;

    RefPtr<WebUserContentController> userContentController = adoptRef(new WebUserContentController(identifier));
    userContentControllerPtr = userContentController.get();

    return userContentController.release();
}

WebUserContentController::WebUserContentController(uint64_t identifier)
    : m_identifier(identifier)
    , m_userContentController(*UserContentController::create())
{
    WebProcess::shared().addMessageReceiver(Messages::WebUserContentController::messageReceiverName(), m_identifier, *this);
}

WebUserContentController::~WebUserContentController()
{
    ASSERT(userContentControllers().contains(m_identifier));

    WebProcess::shared().removeMessageReceiver(Messages::WebUserContentController::messageReceiverName(), m_identifier);

    userContentControllers().remove(m_identifier);
}

void WebUserContentController::addUserScripts(const Vector<WebCore::UserScript>& userScripts)
{
    for (const auto& userScript : userScripts)
        m_userContentController->addUserScript(mainThreadNormalWorld(), std::make_unique<WebCore::UserScript>(userScript));
}

void WebUserContentController::removeAllUserScripts()
{
    m_userContentController->removeUserScripts(mainThreadNormalWorld());
}

} // namespace WebKit
