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
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class InspectorArray;
class InspectorController;
class InspectorFrontend;
class InspectorObject;
class InspectorValue;
class ResourceResponse;

class InspectorApplicationCacheAgent : public Noncopyable {
public:
    InspectorApplicationCacheAgent(InspectorController* inspectorController, InspectorFrontend* frontend);
    ~InspectorApplicationCacheAgent() { }

    // Backend to Frontend
    void didReceiveManifestResponse(unsigned long identifier, const ResourceResponse&);
    void updateApplicationCacheStatus(ApplicationCacheHost::Status);
    void updateNetworkState(bool isNowOnline);

    // From Frontend
    void getApplicationCaches(RefPtr<InspectorValue>* applicationCaches);

private:
    PassRefPtr<InspectorObject> buildObjectForApplicationCache(const ApplicationCacheHost::ResourceInfoList&, const ApplicationCacheHost::CacheInfo&);
    PassRefPtr<InspectorArray> buildArrayForApplicationCacheResources(const ApplicationCacheHost::ResourceInfoList&);
    PassRefPtr<InspectorObject> buildObjectForApplicationCacheResource(const ApplicationCacheHost::ResourceInfo&);

    InspectorController* m_inspectorController;
    InspectorFrontend* m_frontend;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)
#endif // InspectorApplicationCacheAgent_h
