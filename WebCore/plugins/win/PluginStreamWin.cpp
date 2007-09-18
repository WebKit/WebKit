/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginStreamWin.h"

#include "CString.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "PluginDebug.h"
#include "PluginPackageWin.h"
#include "PluginViewWin.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"

// We use -2 here because some plugins like to return -1 to indicate error
// and this way we won't clash with them.
static const int WebReasonNone = -2;

using std::max;
using std::min;

namespace WebCore {

typedef HashMap<NPStream*, NPP> StreamMap;
static StreamMap& streams()
{
    static StreamMap staticStreams;
    return staticStreams;
}

PluginStreamWin::PluginStreamWin(PluginViewWin* pluginView, Frame* frame, const ResourceRequest& resourceRequest, bool sendNotification, void* notifyData)
    : m_resourceRequest(resourceRequest)
    , m_frame(frame)
    , m_pluginView(pluginView)
    , m_notifyData(notifyData)
    , m_sendNotification(sendNotification)
    , m_loadManually(false)
    , m_streamState(StreamBeforeStarted)
    , m_delayDeliveryTimer(this, &PluginStreamWin::delayDeliveryTimerFired)
    , m_deliveryData(0)
    , m_tempFileHandle(INVALID_HANDLE_VALUE)
    , m_pluginFuncs(pluginView->plugin()->pluginFuncs())
    , m_instance(pluginView->instance())
{
    ASSERT(m_pluginView);

    m_stream.url = 0;
    m_stream.ndata = 0;
    m_stream.pdata = 0;
    m_stream.end = 0;
    m_stream.notifyData = 0;
    m_stream.lastmodified = 0;

    streams().add(&m_stream, m_instance);
}

PluginStreamWin::~PluginStreamWin()
{
    ASSERT(m_streamState != StreamStarted);
    ASSERT(!m_loader);

    delete m_deliveryData;

    free((char*)m_stream.url);

    streams().remove(&m_stream);
}

void PluginStreamWin::start()
{
    ASSERT(!m_loadManually);

    m_loader = NetscapePlugInStreamLoader::create(m_frame, this);

    m_loader->setShouldBufferData(false);
    m_loader->documentLoader()->addPlugInStreamLoader(m_loader.get());
    m_loader->load(m_resourceRequest);
}

void PluginStreamWin::stop()
{
    m_streamState = StreamStopped;

    if (m_loadManually) {
        ASSERT(!m_loader);

        DocumentLoader* documentLoader = m_frame->loader()->activeDocumentLoader();
        ASSERT(documentLoader);

        if (documentLoader->isLoadingMainResource())
            documentLoader->cancelMainResourceLoad(m_frame->loader()->cancelledError(m_resourceRequest));

        return;
    }

    if (m_loader) {
        m_loader->cancel();
        m_loader = 0;
    }
}

void PluginStreamWin::startStream()
{
    ASSERT(m_streamState == StreamBeforeStarted);

    const KURL& responseURL = m_resourceResponse.url();

    // Some plugins (Flash) expect that javascript URLs are passed back decoded as this is the
    // format used when requesting the URL.
    if (responseURL.url().startsWith("javascript:", false))
        m_stream.url = _strdup(responseURL.decode_string(responseURL.url()).utf8());
    else
        m_stream.url = _strdup(responseURL.url().utf8());
    
    CString mimeTypeStr = m_resourceResponse.mimeType().utf8();
    
    long long expectedContentLength = m_resourceResponse.expectedContentLength();

    if (m_resourceResponse.isHTTP()) {
        Vector<UChar> stringBuilder;
        String separator(": ");

        HTTPHeaderMap::const_iterator end = m_resourceResponse.httpHeaderFields().end();
        for (HTTPHeaderMap::const_iterator it = m_resourceResponse.httpHeaderFields().begin(); it != end; ++it) {
            stringBuilder.append(it->first.characters(), it->first.length());
            stringBuilder.append(separator.characters(), separator.length());
            stringBuilder.append(it->second.characters(), it->second.length());
            stringBuilder.append((UChar)'\n');
        }

        m_headers = String::adopt(stringBuilder).utf8();

        // If the content is encoded (most likely compressed), then don't send its length to the plugin,
        // which is only interested in the decoded length, not yet known at the moment.
        // <rdar://problem/4470599> tracks a request for -[NSURLResponse expectedContentLength] to incorporate this logic.
        String contentEncoding = m_resourceResponse.httpHeaderField("Content-Encoding");
        if (!contentEncoding.isNull() && contentEncoding != "identity")
            expectedContentLength = -1;
    }

    m_stream.headers = m_headers.data();
    m_stream.pdata = 0;
    m_stream.ndata = this;
    m_stream.end = max(expectedContentLength, 0LL);
    m_stream.lastmodified = m_resourceResponse.lastModifiedDate();
    m_stream.notifyData = m_notifyData;

    m_transferMode = NP_NORMAL;
    m_offset = 0;
    m_reason = WebReasonNone;

    // Protect the stream if destroystream is called from within the newstream handler
    RefPtr<PluginStreamWin> protect(this);

    NPError npErr = m_pluginFuncs->newstream(m_instance, (NPMIMEType)mimeTypeStr.data(), &m_stream, false, &m_transferMode);
    
    // If the stream was destroyed in the call to newstream we return
    if (m_reason != WebReasonNone)
        return;
        
    m_streamState = StreamStarted;

    if (npErr != NPERR_NO_ERROR)
        cancelAndDestroyStream(npErr);

    if (m_transferMode == NP_NORMAL)
        return;
    
    char tempPath[MAX_PATH];

    int result = ::GetTempPathA(_countof(tempPath), tempPath);
    if (result > 0 && result <= _countof(tempPath)) {
        char tempFile[MAX_PATH];

        if (::GetTempFileNameA(tempPath, "WKP", 0, tempFile) > 0) {
            m_tempFileHandle = ::CreateFileA(tempFile, GENERIC_READ | GENERIC_WRITE, 0, 0, 
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

            if (m_tempFileHandle != INVALID_HANDLE_VALUE) {
                m_path = tempFile;
                return;
            }
        }
    }

    // Something went wrong, cancel loading the stream
    cancelAndDestroyStream(NPRES_NETWORK_ERR);
}

NPP PluginStreamWin::ownerForStream(NPStream* stream)
{
    return streams().get(stream);
}

void PluginStreamWin::cancelAndDestroyStream(NPReason reason)
{
    RefPtr<PluginStreamWin> protect(this);

    destroyStream(reason);
    stop();
}

void PluginStreamWin::destroyStream(NPReason reason)
{
    m_reason = reason;
    if (m_reason != NPRES_DONE) {
        // Stop any pending data from being streamed
        if (m_deliveryData)
            m_deliveryData->resize(0);
    } else if (m_deliveryData && m_deliveryData->size() > 0) {
        // There is more data to be streamed, don't destroy the stream now.
        return;
    }
    destroyStream();
}

void PluginStreamWin::destroyStream()
{
    if (m_streamState == StreamStopped)
        return;

    ASSERT (m_reason != WebReasonNone);
    ASSERT (!m_deliveryData || m_deliveryData->size() == 0);

    if (m_tempFileHandle != INVALID_HANDLE_VALUE)
        ::CloseHandle(m_tempFileHandle);

    if (m_stream.ndata != 0) {
        if (m_reason == NPRES_DONE && (m_transferMode == NP_ASFILE || m_transferMode == NP_ASFILEONLY)) {
            ASSERT(!m_path.isNull());

            m_pluginFuncs->asfile(m_instance, &m_stream, m_path.data());
        }

        NPError npErr;
        npErr = m_pluginFuncs->destroystream(m_instance, &m_stream, m_reason);
        LOG_NPERROR(npErr);

        m_stream.ndata = 0;
    }

    if (m_sendNotification)
        m_pluginFuncs->urlnotify(m_instance, m_resourceRequest.url().url().utf8(), m_reason, m_notifyData);

    m_streamState = StreamStopped;

    // disconnectStream can cause us to be deleted.
    RefPtr<PluginStreamWin> protect(this);
    if (!m_loadManually)
        m_pluginView->disconnectStream(this);

    m_pluginView = 0;

    if (!m_path.isNull())
        ::DeleteFileA(m_path.data());
}

void PluginStreamWin::delayDeliveryTimerFired(Timer<PluginStreamWin>* timer)
{
    ASSERT(timer == &m_delayDeliveryTimer);

    deliverData();
}

void PluginStreamWin::deliverData()
{
    ASSERT(m_deliveryData);
    
    if (m_streamState == StreamStopped)
        // FIXME: We should cancel our job in the SubresourceLoader on error so we don't reach this case
        return;

    ASSERT(m_streamState != StreamBeforeStarted);

    if (!m_stream.ndata || m_deliveryData->size() == 0)
        return;

    int32 totalBytes = m_deliveryData->size();
    int32 totalBytesDelivered = 0;

    while (totalBytesDelivered < totalBytes) {
        int32 deliveryBytes = m_pluginFuncs->writeready(m_instance, &m_stream);

        if (deliveryBytes <= 0) {
            m_delayDeliveryTimer.startOneShot(0);
            break;
        } else {
            deliveryBytes = min(deliveryBytes, totalBytes - totalBytesDelivered);
            int32 dataLength = deliveryBytes;
            char* data = m_deliveryData->data() + totalBytesDelivered;

            // Write the data
            deliveryBytes = m_pluginFuncs->write(m_instance, &m_stream, m_offset, dataLength, (void*)data);
            if (deliveryBytes < 0) {
                LOG_PLUGIN_NET_ERROR();
                cancelAndDestroyStream(NPRES_NETWORK_ERR);
                return;
            }
            deliveryBytes = min(deliveryBytes, dataLength);
            m_offset += deliveryBytes;
            totalBytesDelivered += deliveryBytes;
        }
    }

    if (totalBytesDelivered > 0) {
        if (totalBytesDelivered < totalBytes) {
            int remainingBytes = totalBytes - totalBytesDelivered;
            memmove(m_deliveryData, m_deliveryData + totalBytesDelivered, remainingBytes);
            m_deliveryData->resize(remainingBytes);
        } else {
            m_deliveryData->resize(0);
            if (m_reason != WebReasonNone)
                destroyStream();
        }
    } 
}

void PluginStreamWin::sendJavaScriptStream(const KURL& requestURL, const CString& resultString)
{
    didReceiveResponse(0, ResourceResponse(requestURL, "text/plain", resultString.length(), "", ""));

    if (m_streamState == StreamStopped)
        return;

    didReceiveData(0, resultString.data(), resultString.length());
    if (m_streamState == StreamStopped)
        return;

    didFinishLoading(0);
}

void PluginStreamWin::didReceiveResponse(NetscapePlugInStreamLoader* loader, const ResourceResponse& response)
{
    ASSERT(loader == m_loader);
    ASSERT(m_streamState == StreamBeforeStarted);

    m_resourceResponse = response;

    startStream();
}

void PluginStreamWin::didReceiveData(NetscapePlugInStreamLoader* loader, const char* data, int length)
{
    ASSERT(loader == m_loader);
    ASSERT(length > 0);
    ASSERT(m_streamState == StreamStarted);
    
    if (!m_deliveryData)
        m_deliveryData = new Vector<char>;

    int oldSize = m_deliveryData->size();
    m_deliveryData->resize(oldSize + length);
    memcpy(m_deliveryData->data() + oldSize, data, length);

    // If the plug-in cancels the stream in deliverData it could be deleted, 
    // so protect it here.
    RefPtr<PluginStreamWin> protect(this);

    if (m_transferMode != NP_ASFILEONLY)
        deliverData();

    if (m_streamState != StreamStopped && m_tempFileHandle != INVALID_HANDLE_VALUE) {
        DWORD written;
        bool retval = true;

        retval = WriteFile(m_tempFileHandle, data, length, &written, 0);

        if (!retval || (int)written != length)
            cancelAndDestroyStream(NPRES_NETWORK_ERR);
    }

}

void PluginStreamWin::didFail(NetscapePlugInStreamLoader* loader, const ResourceError&)
{
    ASSERT(loader == m_loader);

    m_loader = 0;

    LOG_PLUGIN_NET_ERROR();
    destroyStream(NPRES_NETWORK_ERR);
}

void PluginStreamWin::didFinishLoading(NetscapePlugInStreamLoader* loader)
{
    ASSERT(loader == m_loader);
    ASSERT(m_streamState == StreamStarted);

    m_loader = 0;

    destroyStream(NPRES_DONE);
}

}
