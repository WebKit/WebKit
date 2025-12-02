/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WebTransportSendGroup.h"

#include "JSDOMPromiseDeferred.h"
#include "JSWebTransportSendStreamStats.h"
#include "WebTransportSendStreamStats.h"
#include "WebTransportSession.h"
#include "WebTransport.h"

namespace WebCore {

Ref<WebTransportSendGroup> WebTransportSendGroup::create(WebTransport& transport)
{
    return adoptRef(*new WebTransportSendGroup(transport));
}

WebTransportSendGroup::WebTransportSendGroup(WebTransport& transport)
    : m_identifier(WebTransportSendGroupIdentifier::generate())
    , m_transport(transport) { }

WebTransportSendGroup::~WebTransportSendGroup() = default;

void WebTransportSendGroup::getStats(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    RefPtr transport = m_transport.get();
    if (!transport)
        return promise->reject(ExceptionCode::InvalidStateError);
    RefPtr session = transport->session();
    if (!session)
        return promise->reject(ExceptionCode::InvalidStateError);
    context.enqueueTaskWhenSettled(session->getSendGroupStats(m_identifier), WebCore::TaskSource::Networking, [promise = WTFMove(promise)] (auto&& stats) mutable {
        if (!stats)
            return promise->reject(ExceptionCode::InvalidStateError);
        promise->resolve<IDLDictionary<WebTransportSendStreamStats>>(*stats);
    });
}

RefPtr<WebTransport> WebTransportSendGroup::transport()
{
    return m_transport.get();
}

}
