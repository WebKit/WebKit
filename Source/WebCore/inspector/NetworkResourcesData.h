/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NetworkResourcesData_h
#define NetworkResourcesData_h

#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INSPECTOR)

namespace WebCore {

class NetworkResourcesData {
public:
    class ResourceData {
    public:
        ResourceData(unsigned long identifier, const String& loaderId);

        unsigned long identifier() const { return m_identifier; }
        String loaderId() const { return m_loaderId; }

        String frameId() const { return m_frameId; }
        void setFrameId(String frameId) { m_frameId = frameId; }

        String url() const { return m_url; }
        void setUrl(String url) { m_url = url; }

        bool isXHR() const { return m_isXHR; }
        void setIsXHR(bool isXHR) { m_isXHR = isXHR; }

        bool hasContent() const { return m_hasContent; }
        String content();
        void appendContent(const String&);

        bool isContentPurged() const { return m_isContentPurged; }
        void setIsContentPurged(bool isContentPurged) { m_isContentPurged = isContentPurged; }
        unsigned purgeContent();

    private:
        unsigned long m_identifier;
        String m_loaderId;
        String m_frameId;
        String m_url;
        bool m_hasContent;
        StringBuilder m_contentBuilder;
        bool m_isXHR;
        bool m_isContentPurged;
    };

    NetworkResourcesData();

    ~NetworkResourcesData();

    void resourceCreated(unsigned long identifier, const String& loaderId);
    void responseReceived(unsigned long identifier, const String& frameId, const String& url);
    void didReceiveXHRResponse(unsigned long identifier);
    void addResourceContent(unsigned long identifier, const String& content);

    bool isXHR(unsigned long identifier);
    ResourceData* data(unsigned long identifier);
    void clear(const String& preservedLoaderId = String());

private:
    bool ensureFreeSpace(int size);

    Deque<ResourceData*> m_resourceDataDeque;

    typedef HashMap<unsigned long, ResourceData*> ResourceDataMap;
    ResourceDataMap m_identifierToResourceDataMap;
    int m_contentSize;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(NetworkResourcesData_h)
