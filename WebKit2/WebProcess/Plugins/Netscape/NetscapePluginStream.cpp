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

using namespace WebCore;

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

    // FIXME: Send the JavaScript string as well.
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
    
void NetscapePluginStream::stop(NPReason reason)
{
    if (m_isStarted) {
        m_plugin->NPP_DestroyStream(&m_npStream, reason);
        m_isStarted = false;
    }

    if (m_sendNotification)
        m_plugin->NPP_URLNotify(m_responseURL.data(), reason, m_notificationData);

    m_plugin->removePluginStream(this);
}

} // namespace WebKit

