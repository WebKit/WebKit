/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "CueParser.h"

#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ThreadableLoader.h"

namespace WebCore {

CueParser::CueParser()
{
    // FIXME(62893): Implement.
}

CueParser::~CueParser()
{
    if (m_loader)
        m_loader->cancel();
    m_loader = 0;
}

void CueParser::load(const String& url, ScriptExecutionContext* context, CueParserClient* client)
{
    ResourceRequest request(url);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbacks;
    options.crossOriginRequestPolicy = AllowCrossOriginRequests;

    m_client = client;

    m_loader = ThreadableLoader::create(context, this, request, options);
}

bool CueParser::supportsType(const String& url)
{
    // FIXME(62893): check against a list of supported types
    return false;
}

void CueParser::didReceiveResponse(unsigned long /*identifier*/, const ResourceResponse& response)
{
    if (response.mimeType() == "text/vtt")
        createWebVTTParser();
}

void CueParser::didReceiveData(const char* data, int length)
{
    // If mime type from didReceiveResponse was not informative, sniff file content.
    if (!m_private && WebVTTParser::hasRequiredFileIdentifier(data, length))
        createWebVTTParser();

    if (m_private)
        m_private->parseBytes(data, length);
}

void CueParser::didFinishLoading(unsigned long)
{
    m_client->trackLoadCompleted();
}

void CueParser::didFail(const ResourceError&)
{
    m_client->trackLoadError();
}

void CueParser::fetchParsedCues(Vector<RefPtr<TextTrackCue> >&)
{
    // FIXME(62893): Implement.
}

void CueParser::newCuesParsed()
{
    m_client->newCuesParsed();
}

void CueParser::createWebVTTParser()
{
    if (!m_private) {
        m_private = WebVTTParser::create(this);
        m_client->trackLoadStarted();
    }
}

} // namespace WebCore

#endif
