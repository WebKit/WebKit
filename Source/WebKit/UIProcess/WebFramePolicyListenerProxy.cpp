/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebFramePolicyListenerProxy.h"

#include "WebsitePolicies.h"
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/Function.h>

namespace WebKit {

Ref<WebFramePolicyListenerProxy> WebFramePolicyListenerProxy::create(Function<void(WebCore::PolicyAction, std::optional<WebsitePolicies>&&)>&& completionHandler)
{
    return adoptRef(*new WebFramePolicyListenerProxy(WTFMove(completionHandler)));
}

WebFramePolicyListenerProxy::WebFramePolicyListenerProxy(Function<void(WebCore::PolicyAction, std::optional<WebsitePolicies>&&)>&& completionHandler)
    : m_completionHandler(WTFMove(completionHandler))
{
}

void WebFramePolicyListenerProxy::use(std::optional<WebsitePolicies>&& websitePolicies)
{
    if (auto completionHandler = std::exchange(m_completionHandler, nullptr))
        completionHandler(WebCore::PolicyAction::Use, WTFMove(websitePolicies));
}

void WebFramePolicyListenerProxy::download()
{
    if (auto completionHandler = std::exchange(m_completionHandler, nullptr))
        completionHandler(WebCore::PolicyAction::Download, std::nullopt);
}

void WebFramePolicyListenerProxy::ignore()
{
    if (auto completionHandler = std::exchange(m_completionHandler, nullptr))
        completionHandler(WebCore::PolicyAction::Ignore, std::nullopt);
}

} // namespace WebKit
