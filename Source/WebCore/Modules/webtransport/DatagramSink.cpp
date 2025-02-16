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
#include "DatagramSink.h"

#include "Exception.h"
#include "WebTransport.h"
#include "WebTransportSession.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

DatagramSink::DatagramSink() = default;

DatagramSink::~DatagramSink() = default;

void DatagramSink::attachTo(WebTransport& transport)
{
    ASSERT(!m_transport.get());
    m_transport = transport;
}

void DatagramSink::write(ScriptExecutionContext& context, JSC::JSValue value, DOMPromiseDeferred<void>&& promise)
{
    if (!context.globalObject())
        return promise.settle(Exception { ExceptionCode::InvalidStateError });

    if (m_isClosed)
        return promise.settle(Exception { ExceptionCode::InvalidStateError });

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context.globalObject());
    auto scope = DECLARE_THROW_SCOPE(globalObject.vm());

    auto bufferSource = convert<IDLUnion<IDLArrayBuffer, IDLArrayBufferView>>(globalObject, value);
    if (UNLIKELY(bufferSource.hasException(scope)))
        return promise.settle(Exception { ExceptionCode::ExistingExceptionError });

    RefPtr transport = m_transport.get();
    if (!transport)
        return promise.settle(Exception { ExceptionCode::InvalidStateError });

    RefPtr session = transport->session();
    if (!session)
        return promise.settle(Exception { ExceptionCode::InvalidStateError });

    WTF::switchOn(bufferSource.releaseReturnValue(), [&](auto&& arrayBufferOrView) {
        context.enqueueTaskWhenSettled(session->sendDatagram(arrayBufferOrView->span()), WebCore::TaskSource::Networking, [promise = WTFMove(promise)] (auto&& exception) mutable {
            if (!exception)
                promise.settle(Exception { ExceptionCode::NetworkError });
            else if (*exception)
                promise.settle(WTFMove(**exception));
            else
                promise.resolve();
        });
    });
}

}
