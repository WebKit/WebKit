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
#import "RemoteNetworkingContext.h"

#import "WebCore/ResourceError.h"
#import "WebErrors.h"

using namespace WebCore;

namespace WebKit {

RemoteNetworkingContext::RemoteNetworkingContext(bool needsSiteSpecificQuirks, bool localFileContentSniffingEnabled)
    : m_needsSiteSpecificQuirks(needsSiteSpecificQuirks)
    , m_localFileContentSniffingEnabled(localFileContentSniffingEnabled)
{
}

RemoteNetworkingContext::~RemoteNetworkingContext()
{
}

bool RemoteNetworkingContext::isValid() const
{
    return true;
}

bool RemoteNetworkingContext::needsSiteSpecificQuirks() const
{
    return m_needsSiteSpecificQuirks;
}

bool RemoteNetworkingContext::localFileContentSniffingEnabled() const
{
    return m_localFileContentSniffingEnabled;
}

NSOperationQueue *RemoteNetworkingContext::scheduledOperationQueue() const
{
    static NSOperationQueue *queue;
    if (!queue) {
        queue = [[NSOperationQueue alloc] init];
        // Default concurrent operation count depends on current system workload, but delegate methods are mostly idling in IPC, so we can run as many as needed.
        [queue setMaxConcurrentOperationCount:NSIntegerMax];
    }
    return queue;
}

ResourceError RemoteNetworkingContext::blockedError(const ResourceRequest& request) const
{
    return WebKit::blockedError(request);
}

}
