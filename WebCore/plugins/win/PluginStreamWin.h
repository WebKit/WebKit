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

#ifndef PluginStreamWin_H
#define PluginStreamWin_H

#include <winsock2.h>
#include <windows.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include "CString.h"
#include "KURL.h"
#include "npfunctions.h"
#include "NetscapePlugInStreamLoader.h"
#include "PlatformString.h"
#include "Shared.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "StringHash.h"
#include "Timer.h"

namespace WebCore {
    class Frame;
    class PluginViewWin;

    enum PluginStreamState { StreamBeforeStarted, StreamStarted, StreamStopped };

    class PluginStreamWin : public Shared<PluginStreamWin>, private NetscapePlugInStreamLoaderClient {
    public:
        PluginStreamWin(PluginViewWin*, Frame*, const ResourceRequest&, bool sendNotification, void* notifyData);
        ~PluginStreamWin();
        
        void start();
        void stop();

        void startStream();

        void setLoadManually(bool loadManually) { m_loadManually = loadManually; }

        // NetscapePlugInStreamLoaderClient
        virtual void didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse&);
        virtual void didReceiveData(NetscapePlugInStreamLoader*, const char*, int);
        virtual void didFail(NetscapePlugInStreamLoader*, const ResourceError&);
        virtual void didFinishLoading(NetscapePlugInStreamLoader*);

        void sendJavaScriptStream(const KURL& requestURL, const CString& resultString);
        void cancelAndDestroyStream(NPReason);

        static NPP ownerForStream(NPStream*);
    private:
        void deliverData();
        void destroyStream(NPReason);
        void destroyStream();

        ResourceRequest m_resourceRequest;
        ResourceResponse m_resourceResponse;

        Frame* m_frame;
        RefPtr<NetscapePlugInStreamLoader> m_loader;
        PluginViewWin* m_pluginView;
        void* m_notifyData;
        bool m_sendNotification;
        PluginStreamState m_streamState;
        bool m_loadManually;

        Timer<PluginStreamWin> m_delayDeliveryTimer;
        void delayDeliveryTimerFired(Timer<PluginStreamWin>*);

        Vector<char>* m_deliveryData;

        HANDLE m_tempFileHandle;

        const NPPluginFuncs* m_pluginFuncs;
        NPP m_instance;
        uint16 m_transferMode;
        int32 m_offset;
        CString m_headers;
        CString m_path;
        NPReason m_reason;
        NPStream m_stream;
    };

} // namespace WebCore

#endif
