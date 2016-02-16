/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceLoadObserver_h
#define ResourceLoadObserver_h

#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class KeyedDecoder;
class KeyedEncoder;
class URL;

struct ResourceLoadStatistics;

class ResourceLoadObserver {
    friend class NeverDestroyed<ResourceLoadObserver>;
public:
    WEBCORE_EXPORT static ResourceLoadObserver& sharedObserver();
    
    void logFrameNavigation(bool isRedirect, const URL& sourceURL, const URL& targetURL, bool isMainFrame, const URL& mainFrameURL);
    void logSubresourceLoading(bool isRedirect, const URL& sourceURL, const URL& targetURL, const URL& mainFrameURL);
    void logUserInteraction(const Document&);

    WEBCORE_EXPORT void writeDataToDisk();
    WEBCORE_EXPORT void readDataFromDiskIfNeeded();
    WEBCORE_EXPORT void setStatisticsStorageDirectory(const String&);

    WEBCORE_EXPORT String statisticsForOrigin(const String&);

private:
    ResourceLoadStatistics& resourceStatisticsForPrimaryDomain(const String&);
    
    static String primaryDomain(const URL&);

    bool isPrevalentResource(const String&) const;

    String persistentStoragePath(const String& label) const;

    void writeDataToDisk(const String& origin, const ResourceLoadStatistics&) const;

    std::unique_ptr<KeyedDecoder> createDecoderFromDisk(const String& label) const;
    void writeEncoderToDisk(KeyedEncoder&, const String& label) const;

    HashMap<String, size_t> m_originsVisitedMap;
    HashMap<String, ResourceLoadStatistics> m_resourceStatisticsMap;
    String m_storagePath;
};
    
} // namespace WebCore

#endif /* ResourceLoadObserver_h */
