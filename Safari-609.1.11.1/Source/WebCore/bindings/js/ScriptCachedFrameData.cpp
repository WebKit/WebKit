/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "ScriptCachedFrameData.h"

#include "CommonVM.h"
#include "Document.h"
#include "Frame.h"
#include "GCController.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "ScriptController.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/WeakGCMapInlines.h>

namespace WebCore {
using namespace JSC;

ScriptCachedFrameData::ScriptCachedFrameData(Frame& frame)
{
    JSLockHolder lock(commonVM());

    for (auto windowProxy : frame.windowProxy().jsWindowProxiesAsVector()) {
        auto* window = jsCast<JSDOMWindow*>(windowProxy->window());
        m_windows.add(&windowProxy->world(), Strong<JSDOMWindow>(window->vm(), window));
        window->setConsoleClient(nullptr);
    }

    frame.windowProxy().attachDebugger(nullptr);
}

ScriptCachedFrameData::~ScriptCachedFrameData()
{
    clear();
}

void ScriptCachedFrameData::restore(Frame& frame)
{
    JSLockHolder lock(commonVM());

    Page* page = frame.page();

    for (auto windowProxy : frame.windowProxy().jsWindowProxiesAsVector()) {
        auto* world = &windowProxy->world();

        if (auto* window = m_windows.get(world).get())
            windowProxy->setWindow(window->vm(), *window);
        else {
            ASSERT(frame.document()->domWindow());
            auto& domWindow = *frame.document()->domWindow();
            if (&windowProxy->wrapped() == &domWindow)
                continue;

            windowProxy->setWindow(domWindow);

            if (page) {
                windowProxy->attachDebugger(page->debugger());
                windowProxy->window()->setProfileGroup(page->group().identifier());
            }
        }

        if (page)
            windowProxy->window()->setConsoleClient(&page->console());
    }
}

void ScriptCachedFrameData::clear()
{
    if (m_windows.isEmpty())
        return;

    JSLockHolder lock(commonVM());
    m_windows.clear();
    GCController::singleton().garbageCollectSoon();
}

} // namespace WebCore
