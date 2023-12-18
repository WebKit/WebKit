/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "BrowsingContextGroup.h"

#include "WebProcessProxy.h"

namespace WebKit {

BrowsingContextGroup::BrowsingContextGroup() = default;

WebProcessProxy* BrowsingContextGroup::processForDomain(const WebCore::RegistrableDomain& domain)
{
    auto process = m_processMap.get(domain);
    if (!process)
        return nullptr;
    if (process->state() == WebProcessProxy::State::Terminated)
        return nullptr;
    return process.get();
}

// FIXME: This needs a corresponding remove call when a process terminates. <rdar://116202371>
void BrowsingContextGroup::addProcessForDomain(const WebCore::RegistrableDomain& domain, WebProcessProxy& process)
{
    ASSERT(!m_processMap.get(domain) || m_processMap.get(domain)->state() == WebProcessProxy::State::Terminated || m_processMap.get(domain) == &process);
    m_processMap.set(domain, process);
}

} // namespace WebKit
