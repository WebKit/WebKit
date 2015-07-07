/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#ifndef PageScriptDebugServer_h
#define PageScriptDebugServer_h

#if ENABLE(INSPECTOR)

#include <inspector/ScriptDebugServer.h>
#include <wtf/Forward.h>

namespace WebCore {

class Frame;
class FrameView;
class Page;
class PageGroup;

class PageScriptDebugServer final : public Inspector::ScriptDebugServer {
    WTF_MAKE_NONCOPYABLE(PageScriptDebugServer);
public:
    PageScriptDebugServer(Page&);
    virtual ~PageScriptDebugServer() { }

    void addListener(Inspector::ScriptDebugListener*);
    void removeListener(Inspector::ScriptDebugListener*, bool isBeingDestroyed);

    virtual void recompileAllJSFunctions() override;

private:
    virtual ListenerSet* getListenersForGlobalObject(JSC::JSGlobalObject*) override { return &m_listeners; }
    virtual void didPause(JSC::JSGlobalObject*) override;
    virtual void didContinue(JSC::JSGlobalObject*) override;
    virtual void runEventLoopWhilePaused() override;
    virtual bool isContentScript(JSC::ExecState*) const override;
    virtual void reportException(JSC::ExecState*, JSC::JSValue) const override;

    void runEventLoopWhilePausedInternal();

    void setJavaScriptPaused(const PageGroup&, bool paused);
    void setJavaScriptPaused(Page*, bool paused);
    void setJavaScriptPaused(Frame*, bool paused);
    void setJavaScriptPaused(FrameView*, bool paused);

    ListenerSet m_listeners;
    Page& m_page;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // PageScriptDebugServer_h
