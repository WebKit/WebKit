/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#ifndef WebResourceLoadStatisticsStore_h
#define WebResourceLoadStatisticsStore_h

#include "APIObject.h"
#include "Connection.h"
#include "WebsiteDataRecord.h"
#include <WebCore/ResourceLoadStatisticsStore.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class KeyedDecoder;
class KeyedEncoder;
struct ResourceLoadStatistics;
}

namespace WebKit {

class WebProcessPool;
class WebProcessProxy;

class WebResourceLoadStatisticsStore : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<WebResourceLoadStatisticsStore> create(const String&);
    static void setNotifyPagesWhenDataRecordsWereScanned(bool);
    static void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    static void setMinimumTimeBetweeenDataRecordsRemoval(double);
    virtual ~WebResourceLoadStatisticsStore();
    
    void setResourceLoadStatisticsEnabled(bool);
    bool resourceLoadStatisticsEnabled() const;
    void registerSharedResourceLoadObserver();
    
    void resourceLoadStatisticsUpdated(const Vector<WebCore::ResourceLoadStatistics>& origins);

    void processWillOpenConnection(WebProcessProxy&, IPC::Connection&);
    void processDidCloseConnection(WebProcessProxy&, IPC::Connection&);
    void applicationWillTerminate();

    void readDataFromDiskIfNeeded();

    WebCore::ResourceLoadStatisticsStore& coreStore() { return m_resourceLoadStatisticsStore.get(); }
    const WebCore::ResourceLoadStatisticsStore& coreStore() const { return m_resourceLoadStatisticsStore.get(); }

private:
    explicit WebResourceLoadStatisticsStore(const String&);

    void processStatisticsAndDataRecords();

    bool hasPrevalentResourceCharacteristics(const WebCore::ResourceLoadStatistics&);
    void classifyResource(WebCore::ResourceLoadStatistics&);
    void removeDataRecords();

    String persistentStoragePath(const String& label) const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void writeEncoderToDisk(WebCore::KeyedEncoder&, const String& label) const;
    std::unique_ptr<WebCore::KeyedDecoder> createDecoderFromDisk(const String& label) const;

    Ref<WebCore::ResourceLoadStatisticsStore> m_resourceLoadStatisticsStore;
    Ref<WTF::WorkQueue> m_statisticsQueue;
    String m_storagePath;
    bool m_resourceLoadStatisticsEnabled { false };

    double m_lastTimeDataRecordsWereRemoved { 0 };
    bool m_dataRecordsRemovalPending { false };
};

} // namespace WebKit

#endif // WebResourceLoadStatisticsStore_h
