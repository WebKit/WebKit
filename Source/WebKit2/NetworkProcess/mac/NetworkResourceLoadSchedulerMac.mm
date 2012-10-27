/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkResourceLoadScheduler.h"

#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceRequestCFNet.h>
#import <WebCore/WebCoreSystemInterface.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

unsigned NetworkResourceLoadScheduler::platformInitializeMaximumHTTPConnectionCountPerHost()
{
    wkInitializeMaximumHTTPConnectionCountPerHost = WKInitializeMaximumHTTPConnectionCountPerHost;
    wkSetHTTPPipeliningMaximumPriority = WKSetHTTPPipeliningMaximumPriority;
    wkSetHTTPPipeliningMinimumFastLanePriority = WKSetHTTPPipeliningMinimumFastLanePriority;

    static const unsigned preferredConnectionCount = 6;
    static const unsigned unlimitedConnectionCount = 10000;

    // Always set the connection count per host, even when pipelining.
    unsigned maximumHTTPConnectionCountPerHost = wkInitializeMaximumHTTPConnectionCountPerHost(preferredConnectionCount);

    Boolean keyExistsAndHasValidFormat = false;
    Boolean prefValue = CFPreferencesGetAppBooleanValue(CFSTR("WebKitEnableHTTPPipelining"), kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat);
    
    if (keyExistsAndHasValidFormat)
        ResourceRequest::setHTTPPipeliningEnabled(prefValue);

    if (ResourceRequest::httpPipeliningEnabled()) {
        wkSetHTTPPipeliningMaximumPriority(toHTTPPipeliningPriority(ResourceLoadPriorityHighest));
        wkSetHTTPPipeliningMinimumFastLanePriority(toHTTPPipeliningPriority(ResourceLoadPriorityMedium));

        // When pipelining do not rate-limit requests sent from WebCore since CFNetwork handles that.
        return unlimitedConnectionCount;
    }

    return maximumHTTPConnectionCountPerHost;
}

} // namespace WebKit
