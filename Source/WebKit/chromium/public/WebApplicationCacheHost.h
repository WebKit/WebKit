/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebApplicationCacheHost_h
#define WebApplicationCacheHost_h

#include "WebCommon.h"
#include "WebURL.h"
#include "WebVector.h"

namespace WebKit {

class WebApplicationCacheHostClient;
class WebFrame;
class WebURL;
class WebURLRequest;
class WebURLResponse;
struct WebURLError;

// This interface is used by webkit to call out to the embedder. Webkit uses
// the WebFrameClient::createApplicationCacheHost method to create instances,
// and calls delete when the instance is no longer needed.
class WebApplicationCacheHost {
public:
    // These values must match WebCore::ApplicationCacheHost::Status values
    enum Status {
        Uncached,
        Idle,
        Checking,
        Downloading,
        UpdateReady,
        Obsolete
    };

    // These values must match WebCore::ApplicationCacheHost::EventID values
    enum EventID {
        CheckingEvent,
        ErrorEvent,
        NoUpdateEvent,
        DownloadingEvent,
        ProgressEvent,
        UpdateReadyEvent,
        CachedEvent,
        ObsoleteEvent
    };

    virtual ~WebApplicationCacheHost() { }

    // Called for every request made within the context.
    virtual void willStartMainResourceRequest(WebURLRequest& r, const WebFrame*) { willStartMainResourceRequest(r); }
    virtual void willStartSubResourceRequest(WebURLRequest&) { }

    virtual void willStartMainResourceRequest(WebURLRequest&) { } // DEPRECATED, remove after derived classes have caught up.

    // One or the other selectCache methods is called after having parsed the <html> tag.
    // The latter returns false if the current document has been identified as a "foreign"
    // entry, in which case the frame navigation will be restarted by webkit.
    virtual void selectCacheWithoutManifest() { }
    virtual bool selectCacheWithManifest(const WebURL& manifestURL) { return true; }

    // Called as the main resource is retrieved.
    virtual void didReceiveResponseForMainResource(const WebURLResponse&) { }
    virtual void didReceiveDataForMainResource(const char* data, int len) { }
    virtual void didFinishLoadingMainResource(bool success) { }

    // Called on behalf of the scriptable interface.
    virtual Status status() { return Uncached; }
    virtual bool startUpdate() { return false; }
    virtual bool swapCache() { return false; }

    // Structures and methods to support inspecting Application Caches.
    struct CacheInfo {
        WebURL manifestURL; // Empty if there is no associated cache.
        double creationTime;
        double updateTime;
        long long totalSize;
        CacheInfo() : creationTime(0), updateTime(0), totalSize(0) { }
    };
    struct ResourceInfo {
        WebURL url;
        long long size;
        bool isMaster;
        bool isManifest;
        bool isExplicit;
        bool isForeign;
        bool isFallback;
        ResourceInfo() : size(0), isMaster(false), isManifest(false), isExplicit(false), isForeign(false), isFallback(false) { }
    };
    virtual void getAssociatedCacheInfo(CacheInfo*) { }
    virtual void getResourceList(WebVector<ResourceInfo>*) { }
    virtual void deleteAssociatedCacheGroup() { }
};

}  // namespace WebKit

#endif  // WebApplicationCacheHost_h

