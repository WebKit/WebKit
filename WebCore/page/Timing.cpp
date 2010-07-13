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

#include "DocumentLoader.h"
#include "Frame.h"
#include "ResourceLoadTiming.h"
#include "ResourceResponse.h"

namespace WebCore {

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
    if (!m_frame)
        return 0;

    return toIntegerMilliseconds(m_frame->loader()->frameLoadTimeline()->navigationStart);
}

unsigned long long Timing::unloadEventEnd() const
{
    if (!m_frame)
        return 0;

    return toIntegerMilliseconds(m_frame->loader()->frameLoadTimeline()->unloadEventEnd);
}

unsigned long long Timing::fetchStart() const
{
    if (!m_frame)
        return 0;

    return toIntegerMilliseconds(m_frame->loader()->frameLoadTimeline()->fetchStart);
}

unsigned long long Timing::domainLookupStart() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->dnsStart);
}

unsigned long long Timing::domainLookupEnd() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->dnsEnd);
}

unsigned long long Timing::connectStart() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->connectStart);
}

unsigned long long Timing::connectEnd() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->connectEnd);
}

unsigned long long Timing::requestStart() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->sendStart);
}

unsigned long long Timing::requestEnd() const
{
    ResourceLoadTiming* timing = resourceLoadTiming();
    if (!timing)
        return 0;

    return toIntegerMilliseconds(timing->sendEnd);
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
    return toIntegerMilliseconds(timing->receiveHeadersEnd);
}

unsigned long long Timing::responseEnd() const
{
    if (!m_frame)
        return 0;

    return toIntegerMilliseconds(m_frame->loader()->frameLoadTimeline()->responseEnd);
}

unsigned long long Timing::loadEventStart() const
{
    if (!m_frame)
        return 0;

    return toIntegerMilliseconds(m_frame->loader()->frameLoadTimeline()->loadEventStart);
}

unsigned long long Timing::loadEventEnd() const
{
    if (!m_frame)
        return 0;

    return toIntegerMilliseconds(m_frame->loader()->frameLoadTimeline()->loadEventEnd);
}

ResourceLoadTiming* Timing::resourceLoadTiming() const
{
    DocumentLoader* documentLoader = m_frame->loader()->documentLoader();
    if (!documentLoader)
        return 0;

    return documentLoader->response().resourceLoadTiming();
}

unsigned long long Timing::toIntegerMilliseconds(double milliseconds)
{
    if (milliseconds <= 0)
        return 0;
    return static_cast<unsigned long long>(milliseconds * 1000.0);
}

} // namespace WebCore

#endif // ENABLE(WEB_TIMING)
