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

namespace WebKit {

class WebApplicationCacheHostClient;
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
    virtual void willStartMainResourceRequest(WebURLRequest&) = 0;
    virtual void willStartSubResourceRequest(WebURLRequest&) = 0;

    // One or the other selectCache methods is called after having parsed the <html> tag.
    // The latter returns false if the current document has been identified as a "foreign"
    // entry, in which case the frame navigation will be restarted by webkit.
    virtual void selectCacheWithoutManifest() = 0;
    virtual bool selectCacheWithManifest(const WebURL& manifestURL) = 0;

    // Called as the main resource is retrieved.
    virtual void didReceiveResponseForMainResource(const WebURLResponse&) = 0;
    virtual void didReceiveDataForMainResource(const char* data, int len) = 0;
    virtual void didFinishLoadingMainResource(bool success) = 0;

    // Called on behalf of the scriptable interface.
    virtual Status status() = 0;
    virtual bool startUpdate() = 0;
    virtual bool swapCache() = 0;
};

}  // namespace WebKit

#endif  // WebApplicationCacheHost_h

