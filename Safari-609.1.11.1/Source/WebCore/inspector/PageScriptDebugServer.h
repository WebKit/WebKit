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

#pragma once

#include <JavaScriptCore/ScriptDebugServer.h>

namespace WebCore {

class Frame;
class Page;
class PageGroup;

class PageScriptDebugServer final : public Inspector::ScriptDebugServer {
    WTF_MAKE_NONCOPYABLE(PageScriptDebugServer);
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageScriptDebugServer(Page&);
    ~PageScriptDebugServer() override = default;

    void recompileAllJSFunctions() override;

private:
    void attachDebugger() override;
    void detachDebugger(bool isBeingDestroyed) override;

    void didPause(JSC::JSGlobalObject*) override;
    void didContinue(JSC::JSGlobalObject*) override;
    void runEventLoopWhilePaused() override;
    bool isContentScript(JSC::JSGlobalObject*) const override;
    void reportException(JSC::JSGlobalObject*, JSC::Exception*) const override;

    void runEventLoopWhilePausedInternal();

    void setJavaScriptPaused(const PageGroup&, bool paused);
    void setJavaScriptPaused(Frame&, bool paused);

    Page& m_page;
};

} // namespace WebCore
