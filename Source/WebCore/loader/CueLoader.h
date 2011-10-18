/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef CueLoader_h
#define CueLoader_h

#if ENABLE(VIDEO_TRACK)

#include "CachedCues.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "Document.h"
#include "Timer.h"
#include "WebVTTParser.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class CueLoader;
class ScriptExecutionContext;

class CueLoaderClient {
public:
    virtual ~CueLoaderClient() { }
    
    virtual bool shouldLoadCues(CueLoader*) = 0;
    virtual void newCuesAvailable(CueLoader*) = 0;
    virtual void cueLoadingStarted(CueLoader*) = 0;
    virtual void cueLoadingCompleted(CueLoader*, bool loadingFailed) = 0;
};

class CueLoader : public CachedResourceClient, private WebVTTParserClient {
    WTF_MAKE_NONCOPYABLE(CueLoader); 
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<CueLoader> create(CueLoaderClient* client, ScriptExecutionContext* context)
    {
        return adoptPtr(new CueLoader(client, context));
    }
    virtual ~CueLoader();
    
    bool load(const KURL&);
    void getNewCues(Vector<RefPtr<TextTrackCue> >& outputCues);
    
private:

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*);
    virtual void didReceiveData(CachedResource*);
    
    // WebVTTParserClient
    virtual void newCuesParsed();
    
    CueLoader(CueLoaderClient*, ScriptExecutionContext*);
    
    void processNewCueData(CachedResource*);
    void cueLoadTimerFired(Timer<CueLoader>*);

    enum State { Idle, Loading, Finished, Failed };
    
    CueLoaderClient* m_client;
    OwnPtr<WebVTTParser> m_cueParser;
    CachedResourceHandle<CachedCues> m_cachedCueData;
    ScriptExecutionContext* m_scriptExecutionContext;
    Timer<CueLoader> m_cueLoadTimer;
    State m_state;
    unsigned m_parseOffset;
    bool m_newCuesAvailable;
};

} // namespace WebCore

#endif
#endif
