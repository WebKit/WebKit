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
#include "DatagramSource.h"

#include "Exception.h"
#include "WebTransport.h"
#include "WebTransportDatagramDuplexStream.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

DatagramDefaultSource::DatagramDefaultSource() = default;

DatagramDefaultSource::~DatagramDefaultSource() = default;

void DatagramDefaultSource::setTransport(WebTransport& transport)
{
    m_transport = transport;
}

uint32_t DatagramDefaultSource::incomingMaxBufferedDatagrams() const
{
    if (RefPtr transport = m_transport.get())
        return transport->datagrams().incomingMaxBufferedDatagrams();
    return 1;
}

bool DatagramDefaultSource::deliverDatagram()
{
    ASSERT(!m_incomingQueue.isEmpty());
    if (controller().enqueue(m_incomingQueue.takeFirst())) {
        pullFinished();
        return true;
    }
    doCancel();
    pullFinished();
    return false;
}

void DatagramDefaultSource::doStart()
{
    startFinished();
}

void DatagramDefaultSource::doPull()
{
    if (m_isCancelled || m_isClosed)
        return;

    // FIXME: Check if datagrams are too old according to incomingMaxAge.
    if (!m_incomingQueue.isEmpty()) {
        if (!deliverDatagram())
            return;
        if (m_finReceived && m_incomingQueue.isEmpty() && !m_isClosed) {
            m_isClosed = true;
            controller().close();
        }
        return;
    }

    m_pullPending = true;
}

void DatagramDefaultSource::receiveDatagram(std::span<const uint8_t> datagram, bool withFin, std::optional<Exception>&& exception)
{
    if (m_isCancelled || m_isClosed)
        return;
    if (exception) {
        controller().error(*exception);
        clean();
        return;
    }
    if (datagram.size()) {
        if (auto arrayBuffer = ArrayBuffer::tryCreateUninitialized(datagram.size(), 1)) {
            memcpySpan(arrayBuffer->mutableSpan(), datagram);
            while (m_incomingQueue.size() >= incomingMaxBufferedDatagrams())
                m_incomingQueue.removeFirst();
            // FIXME: We should remove datagrams that are too old based on IncomingDatagramsExpirationDuration.
            m_incomingQueue.append(WTF::move(arrayBuffer));
            if (std::exchange(m_pullPending, false))
                deliverDatagram();
        }
    }
    if (withFin) {
        m_finReceived = true;
        if (m_incomingQueue.isEmpty() && !m_isClosed) {
            m_isClosed = true;
            controller().close();
            clean();
        }
    }
}

void DatagramDefaultSource::doCancel()
{
    m_isCancelled = true;
    m_incomingQueue.clear();
    cancelFinished();
}

void DatagramDefaultSource::error(JSC::JSGlobalObject& globalObject, JSC::JSValue value)
{
    ReadableStreamSource::error(globalObject, value);
}

}
