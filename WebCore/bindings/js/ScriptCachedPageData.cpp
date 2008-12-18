/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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
#include "ScriptCachedPageData.h"

#include "Frame.h"
#include "GCController.h"
#include "Page.h"
#include "PageGroup.h"
#include <runtime/JSLock.h>
#include "ScriptController.h"

using namespace JSC;

namespace WebCore {

ScriptCachedPageData::ScriptCachedPageData(Page* page)
{
    JSLock lock(false);

    ScriptController* scriptController = page->mainFrame()->script();
    if (scriptController->haveWindowShell()) {
        m_window = scriptController->windowShell()->window();
    }
}

DOMWindow* ScriptCachedPageData::domWindow() const {
    return m_window ? m_window->impl() : 0;
}

ScriptCachedPageData::~ScriptCachedPageData()
{
    clear();
}

void ScriptCachedPageData::restore(Page* page)
{
    Frame* mainFrame = page->mainFrame();

    JSLock lock(false);

    ScriptController* scriptController = mainFrame->script();
    if (scriptController->haveWindowShell()) {
        JSDOMWindowShell* windowShell = scriptController->windowShell();
        if (m_window) {
            windowShell->setWindow(m_window.get());
        } else {
            windowShell->setWindow(mainFrame->domWindow());
            scriptController->attachDebugger(page->debugger());
            windowShell->window()->setProfileGroup(page->group().identifier());
        }
    }
}

void ScriptCachedPageData::clear()
{
    JSLock lock(false);

    if (!m_window) {
        m_window = 0;
        gcController().garbageCollectSoon();
    }
}

} // namespace WebCore
