/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkCaptureRecorder.h"

#if ENABLE(NETWORK_CAPTURE)

#include "NetworkCaptureLogging.h"
#include "NetworkCaptureManager.h"
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>

#define DEBUG_CLASS Recorder

namespace WebKit {
namespace NetworkCapture {

void Recorder::recordRequestSent(const WebCore::ResourceRequest& request)
{
    // This class records NetworkLoad's process of loading a network resource.
    // NetworkLoad does this by creating a NetworkDataTask and calling resume
    // on it. This call to resume can be called immediately after the task has
    // been created, or -- if the loading is marked as deferred -- it can be
    // called later in NetworkLoad::setDeferred(true). In fact, the latter can
    // be called multiple times if the loading is suspended and resumed
    // multiple times.
    //
    // This method is called in both places where resume is called. Our task is
    // to catch the call to NetworkDataTask::resume that starts the network
    // loading process. We want to ignore the other calls to resume. Our
    // approach to knowing which one is the first is to check our collection of
    // recorded events. If it is empty, then this is the first call into the
    // recorder, and we want to record the event. Otherwise, ignore it.

    if (m_events.size())
        return;

    DEBUG_LOG("Sent request for URL = " STRING_SPECIFIER, DEBUG_STR(request.url().string()));

    m_initialRequest = request;
    recordEvent(RequestSentEvent(request));
}

void Recorder::recordResponseReceived(const WebCore::ResourceResponse& response)
{
    // Called when receiving a response other than a redirect or error.

    DEBUG_LOG("Received response from URL = " STRING_SPECIFIER, DEBUG_STR(response.url().string()));
    ASSERT(m_events.size());

    // TODO: Is there a better response to receiving a multi-part resource?
    // Learn more about multi-part resources. Why don't we record these? (Note,
    // this decision is based on some NetworkCache code.)

    if (!response.isMultipart())
        recordEvent(ResponseReceivedEvent(response));
    else
        m_events.clear();
}

void Recorder::recordRedirectReceived(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response)
{
    DEBUG_LOG("Received redirect to URL = " STRING_SPECIFIER, DEBUG_STR(request.url().string()));
    ASSERT(m_events.size());

    recordEvent(RedirectReceivedEvent(request, response));
}

void Recorder::recordRedirectSent(const WebCore::ResourceRequest& request)
{
    DEBUG_LOG("Sent redirect for URL = " STRING_SPECIFIER, DEBUG_STR(request.url().string()));
    ASSERT(m_events.size());

    recordEvent(RedirectSentEvent(request));
}

void Recorder::recordDataReceived(WebCore::SharedBuffer& buffer)
{
    DEBUG_LOG("Received %u bytes of data", buffer.size());

    if (!m_events.size())
        return;

    // Prevent memory growth in case of streaming data. TODO: Is there a better
    // response to this? If we encounter this condition, all of our recording
    // silently goes out the window. Replay will not work, and the user doesn't
    // know that.

    constexpr size_t kMaximumCacheBufferSize = 10 * 1024 * 1024;
    m_dataLength += buffer.size();
    if (m_dataLength <= kMaximumCacheBufferSize)
        recordEvent(DataReceivedEvent(buffer));
    else
        m_events.clear();
}

void Recorder::recordFinish(const WebCore::ResourceError& error)
{
    DEBUG_LOG("Finished");

    if (!m_events.size())
        return;

    recordEvent(FinishedEvent(error));
    writeEvents();
}

void Recorder::writeEvents()
{
    auto path = Manager::singleton().requestToPath(m_initialRequest);
    auto handle = Manager::singleton().openCacheFile(path, WebCore::OpenForWrite);
    if (!handle)
        return;

    for (auto const& event : m_events) {
        auto asString = eventToString(event);
        // Write out the JSON string with the terminating NUL. This allows us
        // to better find the separate JSON objects that we write to a single
        // file. It also works better with JSON parsers that expect to find a
        // NUL at the end of their input.
        if (handle.write(asString.c_str(), asString.size() + 1) == -1) {
            DEBUG_LOG_ERROR("Error trying to write to file for URL = " STRING_SPECIFIER, DEBUG_STR(m_initialRequest.url().string()));
            return;
        }
    }

    Manager::singleton().logRecordedResource(m_initialRequest);
}

} // namespace NetworkCapture
} // namespace WebKit

#endif // ENABLE(NETWORK_CAPTURE)
