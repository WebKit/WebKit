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

#include "NetscapePluginStream.h"

#include "NetscapePlugin.h"
#include <utility>

using namespace WebCore;
using namespace std;

namespace WebKit {

NetscapePluginStream::NetscapePluginStream(PassRefPtr<NetscapePlugin> plugin, uint64_t streamID, bool sendNotification, void* notificationData)
    : m_plugin(plugin)
    , m_streamID(streamID)
    , m_sendNotification(sendNotification)
    , m_notificationData(notificationData)
    , m_npStream()
    , m_transferMode(NP_NORMAL)
    , m_offset(0)
    , m_isStarted(false)
    , m_deliveryDataTimer(RunLoop::main(), this, &NetscapePluginStream::deliverDataToPlugin)
    , m_stopStreamWhenDoneDelivering(false)
{
}

NetscapePluginStream::~NetscapePluginStream()
{
    ASSERT(!m_isStarted);
}

void NetscapePluginStream::sendJavaScriptStream(const String& requestURLString, const String& result)
{
    CString resultCString = requestURLString.utf8();
    if (resultCString.isNull()) {
        // There was an error evaluating the JavaScript, just call stop.
        stop(NPRES_NETWORK_ERR);
        return;
    }

    if (!start(requestURLString, resultCString.length(), 0, "text/plain", "")) {
        stop(NPRES_NETWORK_ERR);
        return;
    }

    deliverData(resultCString.data(), resultCString.length());
    stop(NPRES_DONE);
}

bool NetscapePluginStream::start(const WebCore::String& responseURLString, uint32_t expectedContentLength, 
                                 uint32_t lastModifiedTime, const WebCore::String& mimeType, const WebCore::String& headers)
{
    m_responseURL = responseURLString.utf8();
    m_mimeType = mimeType.utf8();
    m_headers = headers.utf8();

    m_npStream.ndata = this;
    m_npStream.url = m_responseURL.data();
    m_npStream.end = expectedContentLength;
    m_npStream.lastmodified = lastModifiedTime;
    m_npStream.notifyData = 0;
    m_npStream.headers = m_headers.length() == 0 ? 0 : m_headers.data();

    NPError error = m_plugin->NPP_NewStream(const_cast<char*>(m_mimeType.data()), &m_npStream, false, &m_transferMode);
    if (error != NPERR_NO_ERROR)
        return false;

    // We successfully started the stream.
    m_isStarted = true;

    switch (m_transferMode) {
        case NP_NORMAL:
            break;
        // FIXME: We don't support streaming to files.
        case NP_ASFILEONLY:
        case NP_ASFILE:
            return false;
        // FIXME: We don't support seekable streams.
        case NP_SEEK:
            return false;
    }

    return true;
}

void NetscapePluginStream::deliverData(const char* bytes, int length)
{
    ASSERT(m_isStarted);

    if (m_transferMode != NP_ASFILEONLY) {
        if (!m_deliveryData)
            m_deliveryData.set(new Vector<char>);

        m_deliveryData->reserveCapacity(m_deliveryData->size() + length);
        m_deliveryData->append(bytes, length);
        
        deliverDataToPlugin();
    }

    // FIXME: Deliver the data to a file as well if needed.
}

void NetscapePluginStream::deliverDataToPlugin()
{
    ASSERT(m_isStarted);

    int32_t numBytesToDeliver = m_deliveryData->size();
    int32_t numBytesDelivered = 0;

    while (numBytesDelivered < numBytesToDeliver) {
        int32_t numBytesPluginCanHandle = m_plugin->NPP_WriteReady(&m_npStream);
        
        if (numBytesPluginCanHandle <= 0) {
            // The plug-in can't handle more data, we'll send the rest later
            m_deliveryDataTimer.startOneShot(0);
            break;
        }

        // Figure out how much data to send to the plug-in.
        int32_t dataLength = min(numBytesPluginCanHandle, numBytesToDeliver - numBytesDelivered);
        char* data = m_deliveryData->data() + numBytesDelivered;

        int32_t numBytesWritten = m_plugin->NPP_Write(&m_npStream, m_offset, dataLength, data);
        if (numBytesWritten < 0) {
            // FIXME: Destroy the stream!
            ASSERT_NOT_REACHED();
        }

        numBytesWritten = min(numBytesWritten, dataLength);
        m_offset += numBytesWritten;
        numBytesDelivered += numBytesWritten;
    }

    // We didn't write anything.
    if (!numBytesDelivered)
        return;

    if (numBytesDelivered < numBytesToDeliver) {
        // Remove the bytes that we actually delivered.
        m_deliveryData->remove(0, numBytesDelivered);
    } else {
        m_deliveryData->clear();

        if (m_stopStreamWhenDoneDelivering)
            stop(NPRES_DONE);
    }
}

void NetscapePluginStream::stop(NPReason reason)
{
    if (m_isStarted) {
        if (reason == NPRES_DONE && m_deliveryData && !m_deliveryData->isEmpty()) {
            // There is still data left that the plug-in hasn't been able to consume yet.
            ASSERT(m_deliveryDataTimer.isActive());
            
            // Set m_stopStreamWhenDoneDelivering to true so that the next time the delivery timer fires
            // and calls deliverDataToPlugin the stream will be closed if all the remaining data was
            // successfully delivered.
            m_stopStreamWhenDoneDelivering = true;
            return;
        }

        m_deliveryData = 0;
        m_deliveryDataTimer.stop();

        m_plugin->NPP_DestroyStream(&m_npStream, reason);
        m_isStarted = false;
    }

    ASSERT(!m_deliveryDataTimer.isActive());

    if (m_sendNotification)
        m_plugin->NPP_URLNotify(m_responseURL.data(), reason, m_notificationData);

    m_plugin->removePluginStream(this);
}

} // namespace WebKit
