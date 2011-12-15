/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#ifndef PageScriptDebugServer_h
#define PageScriptDebugServer_h

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "ScriptDebugServer.h"
#include <wtf/Forward.h>

namespace WebCore {

class Page;

class PageScriptDebugServer : public ScriptDebugServer {
    WTF_MAKE_NONCOPYABLE(PageScriptDebugServer);
public:
    static PageScriptDebugServer& shared();

    void addListener(ScriptDebugListener*, Page*);
    void removeListener(ScriptDebugListener*, Page*);

    class ClientMessageLoop {
    public:
        virtual ~ClientMessageLoop() { }
        virtual void run(Page*) = 0;
        virtual void quitNow() = 0;
    };
    void setClientMessageLoop(PassOwnPtr<ClientMessageLoop>);

private:
    PageScriptDebugServer();
    virtual ~PageScriptDebugServer() { }
    
    virtual ScriptDebugListener* getDebugListenerForContext(v8::Handle<v8::Context>);
    virtual void runMessageLoopOnPause(v8::Handle<v8::Context>);
    virtual void quitMessageLoopOnPause();

    typedef HashMap<Page*, ScriptDebugListener*> ListenersMap;
    ListenersMap m_listenersMap;
    OwnPtr<ClientMessageLoop> m_clientMessageLoop;
    Page* m_pausedPage;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#endif // PageScriptDebugServer_h
