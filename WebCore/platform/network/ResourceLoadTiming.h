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
    PassRefPtr<ResourceLoadTiming> create()
    {
        return adoptRef(new ResourceLoadTiming);
    }

    PassRefPtr<ResourceLoadTiming> deepCopy()
    {
        RefPtr<ResourceLoadTiming> timing = create();
        timing->redirectStart = redirectStart;
        timing->redirectEnd = redirectEnd;
        timing->redirectCount = redirectCount;
        timing->domainLookupStart = domainLookupStart;
        timing->domainLookupEnd = domainLookupEnd;
        timing->connectStart = connectStart;
        timing->connectEnd = connectEnd;
        timing->requestStart = requestStart;
        timing->requestEnd = requestEnd;
        timing->responseStart = responseStart;
        timing->responseEnd = responseEnd;
        return timing.release();
    }

    bool operator==(const ResourceLoadTiming& other) const
    {
        return redirectStart == other.redirectStart
            && redirectEnd == other.redirectEnd
            && redirectCount == other.redirectCount
            && domainLookupStart == other.domainLookupStart
            && domainLookupEnd == other.domainLookupEnd
            && connectStart == other.connectStart
            && connectEnd == other.connectEnd
            && requestStart == other.requestStart
            && requestEnd == other.requestEnd
            && responseStart == other.responseStart
            && responseEnd == other.responseEnd;
    }

    bool operator!=(const ResourceLoadTiming& other) const
    {
        return !(*this == other);
    }

    double redirectStart;
    double redirectEnd;
    unsigned short redirectCount;
    double domainLookupStart;
    double domainLookupEnd;
    double connectStart;
    double connectEnd;
    double requestStart;
    double requestEnd;
    double responseStart;
    double responseEnd;

private:
    ResourceLoadTiming()
        : redirectStart(0.0)
        , redirectEnd(0.0)
        , redirectCount(0)
        , domainLookupStart(0.0)
        , domainLookupEnd(0.0)
        , connectStart(0.0)
        , connectEnd(0.0)
        , requestStart(0.0)
        , requestEnd(0.0)
        , responseStart(0.0)
        , responseEnd(0.0)
    {
    }
};

}

#endif
