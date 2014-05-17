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
#include "WebUserContentControllerProxy.h"

#include "WebProcessProxy.h"
#include "WebUserContentControllerMessages.h"

namespace WebKit {

static uint64_t generateIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

PassRefPtr<WebUserContentControllerProxy> WebUserContentControllerProxy::create()
{
    return adoptRef(new WebUserContentControllerProxy);
}

WebUserContentControllerProxy::WebUserContentControllerProxy()
    : m_identifier(generateIdentifier())
{
}

WebUserContentControllerProxy::~WebUserContentControllerProxy()
{
}

void WebUserContentControllerProxy::addProcess(WebProcessProxy& webProcessProxy)
{
    ASSERT(webProcessProxy.state() == WebProcessProxy::State::Running);

    if (!m_processes.add(&webProcessProxy).isNewEntry)
        return;

    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserScripts(m_userScripts), m_identifier);
}

void WebUserContentControllerProxy::removeProcess(WebProcessProxy& webProcessProxy)
{
    ASSERT(m_processes.contains(&webProcessProxy));

    m_processes.remove(&webProcessProxy);
}

void WebUserContentControllerProxy::addUserScript(WebCore::UserScript userScript)
{
    m_userScripts.append(std::move(userScript));

    for (auto& processAndCount : m_processes)
        processAndCount.key->connection()->send(Messages::WebUserContentController::AddUserScripts({ m_userScripts.last() }), m_identifier);
}

void WebUserContentControllerProxy::removeAllUserScripts()
{
    m_userScripts.clear();

    for (auto& processAndCount : m_processes)
        processAndCount.key->connection()->send(Messages::WebUserContentController::RemoveAllUserScripts(), m_identifier);
}

} // namespace WebKit
