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
#include "WebTransportDatagramsWritable.h"

#include "DatagramSink.h"
#include "InternalWritableStream.h"
#include "WebTransport.h"
#include "WebTransportSendGroup.h"
#include "WebTransportSendOptions.h"

namespace WebCore {

ExceptionOr<Ref<WebTransportDatagramsWritable>> WebTransportDatagramsWritable::create(ScriptExecutionContext& context, RefPtr<WebTransport>&& transport, WebTransportSendOptions&& options)
{
    RefPtr sendGroup = options.sendGroup;
    if (sendGroup && sendGroup->transport().get() != transport.get())
        return Exception { ExceptionCode::InvalidStateError };

    auto* globalObject = context.globalObject();
    if (!globalObject) {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::InvalidStateError };
    }
    auto& domGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);

    Ref datagramSink = DatagramSink::create(transport ? transport->session().get() : nullptr);
    auto internal = createInternalWritableStream(domGlobalObject, datagramSink.copyRef());
    if (internal.hasException())
        return internal.releaseException();

    Ref result = adoptRef(*new WebTransportDatagramsWritable(internal.releaseReturnValue(), WTF::move(options)));
    datagramSink->attachTo(result);
    if (transport)
        transport->datagramsWritableCreated(result);
    return { WTF::move(result) };
}

WebTransportDatagramsWritable::WebTransportDatagramsWritable(Ref<InternalWritableStream>&& stream, WebTransportSendOptions&& options)
    : WritableStream(WTF::move(stream))
    , m_sendGroup(options.sendGroup)
    , m_sendOrder(options.sendOrder) { }

WebTransportDatagramsWritable::~WebTransportDatagramsWritable() = default;

const RefPtr<WebTransportSendGroup>& WebTransportDatagramsWritable::sendGroup()
{
    return m_sendGroup;
}

void WebTransportDatagramsWritable::setSendGroup(WebTransportSendGroup* group)
{
    m_sendGroup = group;
}

int64_t WebTransportDatagramsWritable::sendOrder()
{
    return m_sendOrder;
}

void WebTransportDatagramsWritable::setSendOrder(int64_t order)
{
    m_sendOrder = order;
}

}
