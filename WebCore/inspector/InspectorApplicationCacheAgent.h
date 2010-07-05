/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorApplicationCacheAgent_h
#define InspectorApplicationCacheAgent_h

#if ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCacheHost.h"
#include "KURL.h"
#include "PlatformString.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class ApplicationCache;
class InspectorController;
class InspectorFrontend;
class ResourceResponse;
class ScriptArray;
class ScriptObject;

class InspectorApplicationCacheAgent : public Noncopyable {
public:
    struct ApplicationCacheInfo {
        ApplicationCacheInfo(const KURL& manifest, const String& creationTime, const String& updateTime, long long size)
            : m_manifest(manifest)
            , m_creationTime(creationTime)
            , m_updateTime(updateTime)
            , m_size(size)
        {
        }

        KURL m_manifest;
        String m_creationTime;
        String m_updateTime;
        long long m_size;
    };

    struct ResourceInfo {
        ResourceInfo(const KURL& resource, bool isMaster, bool isManifest, bool isFallback, bool isForeign, bool isExplicit, long long size)
            : m_resource(resource)
            , m_isMaster(isMaster)
            , m_isManifest(isManifest)
            , m_isFallback(isFallback)
            , m_isForeign(isForeign)
            , m_isExplicit(isExplicit)
            , m_size(size)
        {
        }

        KURL m_resource;
        bool m_isMaster;
        bool m_isManifest;
        bool m_isFallback;
        bool m_isForeign;
        bool m_isExplicit;
        long long m_size;
    };

    typedef Vector<ResourceInfo> ResourceInfoList;

    InspectorApplicationCacheAgent(InspectorController* inspectorController, InspectorFrontend* frontend);
    ~InspectorApplicationCacheAgent() { }

    // Backend to Frontend
    void didReceiveManifestResponse(unsigned long identifier, const ResourceResponse&);
    void updateApplicationCacheStatus(ApplicationCacheHost::Status);
    void updateNetworkState(bool isNowOnline);

    // From Frontend
    void getApplicationCaches(long callId);

private:
    ScriptObject buildObjectForApplicationCache(const ResourceInfoList&, const ApplicationCacheInfo&);
    ScriptArray buildArrayForApplicationCacheResources(const ResourceInfoList&);
    ScriptObject buildObjectForApplicationCacheResource(const ResourceInfo&);

    void fillResourceList(ApplicationCache*, ResourceInfoList*);

    InspectorController* m_inspectorController;
    InspectorFrontend* m_frontend;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)
#endif // InspectorApplicationCacheAgent_h
