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

#pragma once

#if ENABLE(NETSCAPE_PLUGIN_API)

#include <WebCore/npruntime_internal.h>
#include <memory>
#include <wtf/FileSystem.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>

namespace WebKit {

class NetscapePlugin;

class NetscapePluginStream : public RefCounted<NetscapePluginStream> {
public:
    static Ref<NetscapePluginStream> create(Ref<NetscapePlugin>&& plugin, uint64_t streamID, const String& requestURLString, bool sendNotification, void* notificationData)
    {
        return adoptRef(*new NetscapePluginStream(WTFMove(plugin), streamID, requestURLString, sendNotification, notificationData));
    }
    ~NetscapePluginStream();

    uint64_t streamID() const { return m_streamID; }
    const NPStream* npStream() const { return &m_npStream; }

    void willSendRequest(const URL& requestURL, const URL& redirectResponseURL, int redirectResponseStatus);
    void didReceiveResponse(const URL& responseURL, uint32_t streamLength,
                            uint32_t lastModifiedTime, const String& mimeType, const String& headers);
    void didReceiveData(const char* bytes, int length);
    void didFinishLoading();
    void didFail(bool wasCancelled);

    void sendJavaScriptStream(const String& result);

    void stop(NPReason);
    NPError destroy(NPReason);
    void setURL(const String& newURLString);

private:
    NetscapePluginStream(Ref<NetscapePlugin>&&, uint64_t streamID, const String& requestURLString, bool sendNotification, void* notificationData);

    bool start(const String& responseURLString, uint32_t streamLength, 
               uint32_t lastModifiedTime, const String& mimeType, const String& headers);

    void cancel();
    void notifyAndDestroyStream(NPReason);

    void deliverData(const char* bytes, int length);
    void deliverDataToPlugin();
    void deliverDataToFile(const char* bytes, int length);

    Ref<NetscapePlugin> m_plugin;
    uint64_t m_streamID;

    String m_requestURLString;
    bool m_sendNotification;
    void* m_notificationData;

    NPStream m_npStream;
    uint16_t m_transferMode;
    int32_t m_offset;

    String m_filePath;
    FileSystem::PlatformFileHandle m_fileHandle;
    
    // Whether NPP_NewStream has successfully been called.
    bool m_isStarted;

#if ASSERT_ENABLED
    bool m_urlNotifyHasBeenCalled;
#endif

    CString m_responseURL;
    CString m_mimeType;
    CString m_headers;

    RunLoop::Timer<NetscapePluginStream> m_deliveryDataTimer;
    std::unique_ptr<Vector<uint8_t>> m_deliveryData;
    bool m_stopStreamWhenDoneDelivering;
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
