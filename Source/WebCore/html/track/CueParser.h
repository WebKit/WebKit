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

#ifndef CueParser_h
#define CueParser_h

#if ENABLE(VIDEO_TRACK)

#include "ThreadableLoader.h"
#include "ThreadableLoaderClient.h"
#include "WebVTTParser.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ScriptExecutionContext;
class TextTrackCue;

class CueParserClient {
public:
    virtual void newCuesParsed() = 0;
    virtual void trackLoadStarted() = 0;
    virtual void trackLoadError() = 0;
    virtual void trackLoadCompleted() = 0;
};

class CueParser : public ThreadableLoaderClient, CueParserPrivateClient {
public:
    CueParser();
    virtual ~CueParser();

    void didReceiveResponse(unsigned long, const ResourceResponse&);
    void didReceiveData(const char*, int);
    void didFinishLoading(unsigned long);
    void didFail(const ResourceError&);

    void load(const String&, ScriptExecutionContext*, CueParserClient*);
    bool supportsType(const String&);

    void fetchParsedCues(Vector<RefPtr<TextTrackCue> >&);
    void newCuesParsed();

protected:
    ScriptExecutionContext* m_scriptExecutionContext;

private:
    void createWebVTTParser();
 
    RefPtr<ThreadableLoader> m_loader;
    OwnPtr<CueParserPrivateInterface> m_private;
    CueParserClient* m_client;
};

} // namespace WebCore

#endif
#endif
