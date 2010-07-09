/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#ifndef ResourceLoadTiming_h
#define ResourceLoadTiming_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ResourceLoadTiming : public RefCounted<ResourceLoadTiming> {
public:
    static PassRefPtr<ResourceLoadTiming> create()
    {
        return adoptRef(new ResourceLoadTiming);
    }

    PassRefPtr<ResourceLoadTiming> deepCopy()
    {
        RefPtr<ResourceLoadTiming> timing = create();
        timing->requestTime = requestTime;
        timing->proxyDuration = proxyDuration;
        timing->dnsDuration = dnsDuration;
        timing->connectDuration = connectDuration;
        timing->sendDuration = sendDuration;
        timing->receiveHeadersDuration = receiveHeadersDuration;
        timing->sslDuration = sslDuration;
        return timing.release();
    }

    bool operator==(const ResourceLoadTiming& other) const
    {
        return requestTime == other.requestTime
            && proxyDuration == other.proxyDuration
            && dnsDuration == other.dnsDuration
            && connectDuration == other.connectDuration
            && sendDuration == other.sendDuration
            && receiveHeadersDuration == other.receiveHeadersDuration
            && sslDuration == other.sslDuration;
    }

    bool operator!=(const ResourceLoadTiming& other) const
    {
        return !(*this == other);
    }

    double requestTime;
    double proxyDuration;
    double dnsDuration;
    double connectDuration;
    double sendDuration;
    double receiveHeadersDuration;
    double sslDuration;

private:
    ResourceLoadTiming()
        : requestTime(0.0)
        , proxyDuration(-1.0)
        , dnsDuration(-1.0)
        , connectDuration(-1.0)
        , sendDuration(0.0)
        , receiveHeadersDuration(0.0)
        , sslDuration(-1.0)
    {
    }
};

}

#endif
