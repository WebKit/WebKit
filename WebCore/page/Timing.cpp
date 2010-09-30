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

#include "config.h"
#include "Timing.h"

#if ENABLE(WEB_TIMING)

#include "DocumentLoadTiming.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "ResourceLoadTiming.h"
#include "ResourceResponse.h"

namespace WebCore {

static unsigned long long toIntegerMilliseconds(double seconds)
{
    ASSERT(seconds >= 0);
    return static_cast<unsigned long long>(seconds * 1000.0);
}

static double getPossiblySkewedTimeInKnownRange(double skewedTime, double lowerBound, double upperBound)
{
#if PLATFORM(CHROMIUM)
    // The chromium port's currentTime() implementation only syncs with the
    // system clock every 60 seconds. So it is possible for timing marks
    // collected in different threads or processes to have a small skew.
    // FIXME: It may be possible to add a currentTimeFromSystemTime() method
    // that eliminates the skew.
    if (skewedTime <= lowerBound)
        return lowerBound;

    if (skewedTime >= upperBound)
        return upperBound;
#else
    ASSERT_UNUSED(lowerBound, skewedTime >= lowerBound);
    ASSERT_UNUSED(upperBound, skewedTime <= upperBound);
#endif

    return skewedTime;
}

Timing::Timing(Frame* frame)
    : m_frame(frame)
{
}

Frame* Timing::frame() const
{
    return m_frame;
}

void Timing::disconnectFrame()
{
    m_frame = 0;
}

unsigned long long Timing::navigationStart() const
{
    DocumentLoadTiming* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->navigationStart);
}

unsigned long long Timing::unloadEventEnd() const
{
    DocumentLoadTiming* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->unloadEventEnd);
}

unsigned long long Timing::redirectStart() const
{
    DocumentLoadTiming* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->redirectStart);
}

unsigned long long Timing::redirectEnd() const
{
    DocumentLoadTiming* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->redirectEnd);
}

unsigned long long Timing::fetchStart() const
{
    DocumentLoadTiming* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->fetchStart);
}

unsigned long long Timing::domainLookupStart() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    // This will be -1 when a DNS request is not performed.
    // Rather than exposing a special value that indicates no DNS, we "backfill" with fetchStart.
    int dnsStart = timing->dnsStart;
    if (dnsStart < 0)
        return fetchStart();

    return resourceLoadTimeRelativeToAbsolute(dnsStart);
}

unsigned long long Timing::domainLookupEnd() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    // This will be -1 when a DNS request is not performed.
    // Rather than exposing a special value that indicates no DNS, we "backfill" with domainLookupStart.
    int dnsEnd = timing->dnsEnd;
    if (dnsEnd < 0)
        return domainLookupStart();

    return resourceLoadTimeRelativeToAbsolute(dnsEnd);
}

unsigned long long Timing::connectStart() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return 0;

    ResourceLoadTiming* timing = loader->response().resourceLoadTiming();
    if (!timing)
        return 0;

    // connectStart will be -1 when a network request is not made.
    // Rather than exposing a special value that indicates no new connection, we "backfill" with domainLookupEnd.
    int connectStart = timing->connectStart;
    if (connectStart < 0 || loader->response().connectionReused())
        return domainLookupEnd();

    // ResourceLoadTiming's connect phase includes DNS and SSL, however Web Timing's
    // connect phase should not. So if there is DNS time, trim it from the start.
    if (timing->dnsEnd >= 0 && timing->dnsEnd > connectStart)
        connectStart = timing->dnsEnd;

    return resourceLoadTimeRelativeToAbsolute(connectStart);
}

unsigned long long Timing::connectEnd() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return 0;

    ResourceLoadTiming* timing = loader->response().resourceLoadTiming();
    if (!timing)
        return 0;

    // connectEnd will be -1 when a network request is not made.
    // Rather than exposing a special value that indicates no new connection, we "backfill" with connectStart.
    int connectEnd = timing->connectEnd;
    if (connectEnd < 0 || loader->response().connectionReused())
        return connectStart();

    // ResourceLoadTiming's connect phase includes DNS and SSL, however Web Timing's
    // connect phase should not. So if there is SSL time, trim it from the end.
    if (timing->sslStart >= 0 && timing->sslStart < connectEnd)
        connectEnd = timing->sslStart;

    return resourceLoadTimeRelativeToAbsolute(connectEnd);
}

unsigned long long Timing::requestStart() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    ASSERT(timing->sendStart >= 0);
    return resourceLoadTimeRelativeToAbsolute(timing->sendStart);
}

unsigned long long Timing::requestEnd() const
{
    return responseStart();
}

unsigned long long Timing::responseStart() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    // FIXME: Response start needs to be the time of the first received byte.
    // However, the ResourceLoadTiming API currently only supports the time
    // the last header byte was received. For many responses with reasonable
    // sized cookies, the HTTP headers fit into a single packet so this time
    // is basically equivalent. But for some responses, particularly those with
    // headers larger than a single packet, this time will be too late.
    ASSERT(timing->receiveHeadersEnd >= 0);
    return resourceLoadTimeRelativeToAbsolute(timing->receiveHeadersEnd);
}

unsigned long long Timing::responseEnd() const
{
    DocumentLoadTiming* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->responseEnd);
}

unsigned long long Timing::loadEventStart() const
{
    DocumentLoadTiming* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->loadEventStart);
}

unsigned long long Timing::loadEventEnd() const
{
    DocumentLoadTiming* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->loadEventEnd);
}

DocumentLoader* Timing::documentLoader() const
{
    if (!m_frame)
        return 0;

    return m_frame->loader()->documentLoader();
}

DocumentLoadTiming* Timing::documentLoadTiming() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return 0;

    return loader->timing();
}

ResourceLoadTiming* Timing::resourceLoadTiming() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return 0;

    return loader->response().resourceLoadTiming();
}

unsigned long long Timing::resourceLoadTimeRelativeToAbsolute(int relativeSeconds) const
{
    ASSERT(relativeSeconds >= 0);
    ResourceLoadTiming* resourceTiming = resourceLoadTiming();
    ASSERT(resourceTiming);
    DocumentLoadTiming* documentTiming = documentLoadTiming();
    ASSERT(documentTiming);

    // The ResourceLoadTiming API's requestTime is the base time to which all
    // other marks are relative. So to get an absolute time, we must add it to
    // the relative marks.
    //
    // Since ResourceLoadTimings came from the network platform layer, we must
    // check them for skew because they may be from another thread/process.
    double baseTime = getPossiblySkewedTimeInKnownRange(resourceTiming->requestTime, documentTiming->fetchStart, documentTiming->responseEnd - (resourceTiming->receiveHeadersEnd / 1000.0));
    return toIntegerMilliseconds(baseTime) + relativeSeconds;
}

} // namespace WebCore

#endif // ENABLE(WEB_TIMING)
