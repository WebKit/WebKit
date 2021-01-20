/*
 * Copyright (C) 2008 Collin Jackson  <collinj@webkit.org>
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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

#include "config.h"
#include "DNSResolveQueueCFNet.h"

#include "NotImplemented.h"
#include "Timer.h"
#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(WIN)
#include "LoaderRunLoopCF.h"
#endif

#if PLATFORM(WIN) || PLATFORM(IOS_FAMILY)
#include <CFNetwork/CFNetwork.h>
#endif

namespace WebCore {

void DNSResolveQueueCFNet::updateIsUsingProxy()
{
    RetainPtr<CFDictionaryRef> proxySettings = adoptCF(CFNetworkCopySystemProxySettings());
    if (!proxySettings) {
        m_isUsingProxy = false;
        return;
    }

    RetainPtr<CFURLRef> httpCFURL = URL({ }, "http://example.com/").createCFURL();
    RetainPtr<CFURLRef> httpsCFURL = URL({ }, "https://example.com/").createCFURL();

    RetainPtr<CFArrayRef> httpProxyArray = adoptCF(CFNetworkCopyProxiesForURL(httpCFURL.get(), proxySettings.get()));
    RetainPtr<CFArrayRef> httpsProxyArray = adoptCF(CFNetworkCopyProxiesForURL(httpsCFURL.get(), proxySettings.get()));

    CFIndex httpProxyCount = CFArrayGetCount(httpProxyArray.get());
    CFIndex httpsProxyCount = CFArrayGetCount(httpsProxyArray.get());
    if (httpProxyCount == 1 && CFEqual(CFDictionaryGetValue(static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(httpProxyArray.get(), 0)), kCFProxyTypeKey), kCFProxyTypeNone))
        httpProxyCount = 0;
    if (httpsProxyCount == 1 && CFEqual(CFDictionaryGetValue(static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(httpsProxyArray.get(), 0)), kCFProxyTypeKey), kCFProxyTypeNone))
        httpsProxyCount = 0;

    m_isUsingProxy = httpProxyCount || httpsProxyCount;
}

static void clientCallback(CFHostRef theHost, CFHostInfoType, const CFStreamError*, void*)
{
    DNSResolveQueue::singleton().decrementRequestCount(); // It's ok to call singleton() from a secondary thread, the static variable has already been initialized by now.
    CFRelease(theHost);
}

void DNSResolveQueueCFNet::platformResolve(const String& hostname)
{
    ASSERT(isMainThread());

    RetainPtr<CFHostRef> host = adoptCF(CFHostCreateWithName(0, hostname.createCFString().get()));
    if (!host) {
        decrementRequestCount();
        return;
    }

    CFHostClientContext context = { 0, 0, 0, 0, 0 };
    CFHostRef leakedHost = host.leakRef(); // The host will be released from clientCallback().
    Boolean result = CFHostSetClient(leakedHost, clientCallback, &context);
    ASSERT_UNUSED(result, result);
#if !PLATFORM(WIN)
    CFHostScheduleWithRunLoop(leakedHost, CFRunLoopGetMain(), kCFRunLoopCommonModes);
#else
    // On Windows, we run a separate thread with CFRunLoop, which is where clientCallback will be called.
    CFHostScheduleWithRunLoop(leakedHost, loaderRunLoop(), kCFRunLoopDefaultMode);
#endif
    CFHostStartInfoResolution(leakedHost, kCFHostAddresses, 0);
}

void DNSResolveQueueCFNet::resolve(const String& /* hostname */, uint64_t /* identifier */, DNSCompletionHandler&& /* completionHandler */)
{
    notImplemented();
}

void DNSResolveQueueCFNet::stopResolve(uint64_t /* identifier */)
{
    notImplemented();
}

}
